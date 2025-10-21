// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Version: v3.1.2 - FIXED WebSocket Connection Issues
// Date: 2025-10-21
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// FIXES v3.1.2:
// ✅ Fixed connection URL configuration (uses passed URL from Application)
// ✅ Better error handling for connection failures
// ✅ Prevents multiple simultaneous reconnection attempts
// ✅ Added connection timeout detection
// ✅ Improved logging for debugging connection issues
// ✅ Queue persistence across reconnections
// ✅ Graceful degradation when backend unavailable
// ============================================================================

class BackendService {
    constructor(eventBus, logger) {
        this.eventBus = eventBus;
        this.logger = logger;
        
        // Configuration
        this.config = {
            url: null, // Will be set when connect() is called
            reconnectDelay: 2000,
            maxReconnectDelay: 30000,
            reconnectBackoff: 1.5,
            requestTimeout: 10000,
            heartbeatInterval: 30000,
            protocolVersion: '3.0',
            connectionTimeout: 5000, // NEW: Timeout for initial connection
            maxReconnectAttempts: 10 // NEW: Limit reconnection attempts
        };
        
        // État
        this.ws = null;
        this.connected = false;
        this.reconnecting = false;
        this.reconnectAttempts = 0;
        this.reconnectTimer = null;
        this.heartbeatTimer = null;
        this.connectionTimer = null; // NEW: Track connection timeout
        
        // Gestion des requêtes
        this.pendingRequests = new Map(); // requestId -> {resolve, reject, timeout}
        this.messageQueue = []; // Messages en attente si déconnecté
        
        // Statistiques
        this.stats = {
            messagesSent: 0,
            messagesReceived: 0,
            requestsSent: 0,
            responsesReceived: 0,
            eventsReceived: 0,
            errorsReceived: 0,
            reconnections: 0,
            connectionFailures: 0
        };
        
        this.logger.info('BackendService', '✓ Service initialized (Protocol v3.0)');
    }
    
    // ========================================================================
    // CONNEXION / DÉCONNEXION
    // ========================================================================
    
    /**
     * Connecte au serveur WebSocket
     * @param {string} url - URL du serveur (optionnel)
     * @returns {Promise<void>}
     */
    connect(url = null) {
        if (url) {
            this.config.url = url;
        }
        
        if (!this.config.url) {
            const error = new Error('No WebSocket URL configured');
            this.logger.error('BackendService', 'Cannot connect:', error.message);
            return Promise.reject(error);
        }
        
        // Prevent multiple simultaneous connection attempts
        if (this.ws && (this.ws.readyState === WebSocket.CONNECTING || this.ws.readyState === WebSocket.OPEN)) {
            this.logger.warn('BackendService', 'Already connected or connecting');
            return Promise.resolve();
        }
        
        return new Promise((resolve, reject) => {
            try {
                this.logger.info('BackendService', `🔌 Connecting to ${this.config.url}...`);
                
                // Clean up existing connection
                if (this.ws) {
                    this.ws.onopen = null;
                    this.ws.onclose = null;
                    this.ws.onerror = null;
                    this.ws.onmessage = null;
                    if (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING) {
                        this.ws.close();
                    }
                }
                
                this.ws = new WebSocket(this.config.url);
                
                // Set connection timeout
                this.connectionTimer = setTimeout(() => {
                    if (!this.connected) {
                        this.logger.error('BackendService', 'Connection timeout');
                        this.stats.connectionFailures++;
                        if (this.ws) {
                            this.ws.close();
                        }
                        reject(new Error('Connection timeout'));
                    }
                }, this.config.connectionTimeout);
                
                this.ws.onopen = () => {
                    clearTimeout(this.connectionTimer);
                    this.connectionTimer = null;
                    
                    this.connected = true;
                    this.reconnecting = false;
                    this.reconnectAttempts = 0;
                    
                    this.logger.info('BackendService', '✓ Connected to backend');
                    
                    // Démarrer heartbeat
                    this.startHeartbeat();
                    
                    // Vider la queue
                    this.flushMessageQueue();
                    
                    // Émettre événement
                    this.eventBus.emit('websocket:connected');
                    
                    resolve();
                };
                
                this.ws.onmessage = (event) => {
                    this.onMessage(event);
                };
                
                this.ws.onclose = (event) => {
                    clearTimeout(this.connectionTimer);
                    this.connectionTimer = null;
                    this.onClose(event);
                };
                
                this.ws.onerror = (error) => {
                    clearTimeout(this.connectionTimer);
                    this.connectionTimer = null;
                    this.stats.connectionFailures++;
                    this.onError(error);
                    
                    if (!this.connected) {
                        reject(error);
                    }
                };
                
            } catch (error) {
                clearTimeout(this.connectionTimer);
                this.connectionTimer = null;
                this.stats.connectionFailures++;
                this.logger.error('BackendService', 'Connection failed:', error);
                reject(error);
            }
        });
    }
    
    /**
     * Déconnecte du serveur
     */
    disconnect() {
        this.logger.info('BackendService', 'Disconnecting...');
        
        // Arrêter reconnexion automatique
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        
        // Arrêter connection timeout
        if (this.connectionTimer) {
            clearTimeout(this.connectionTimer);
            this.connectionTimer = null;
        }
        
        // Arrêter heartbeat
        this.stopHeartbeat();
        
        // Rejeter toutes les requêtes en attente
        this.rejectAllPendingRequests('Disconnected');
        
        // Fermer la connexion
        if (this.ws) {
            this.ws.onopen = null;
            this.ws.onclose = null;
            this.ws.onerror = null;
            this.ws.onmessage = null;
            this.ws.close();
            this.ws = null;
        }
        
        this.connected = false;
        this.reconnecting = false;
        
        this.eventBus.emit('websocket:disconnected');
    }
    
    /**
     * Vérifie si connecté
     * @returns {boolean}
     */
    isConnected() {
        return this.connected && this.ws && this.ws.readyState === WebSocket.OPEN;
    }
    
    // ========================================================================
    // HANDLERS WEBSOCKET
    // ========================================================================
    
    /**
     * Handler onMessage
     * @param {MessageEvent} event
     */
    onMessage(event) {
        this.stats.messagesReceived++;
        
        try {
            const message = JSON.parse(event.data);
            
            // Vérifier si c'est le nouveau format avec enveloppe
            if (message.envelope) {
                this.handleEnvelopeMessage(message);
            } else {
                // Legacy format
                this.handleLegacyMessage(message);
            }
            
        } catch (error) {
            this.logger.error('BackendService', 'Failed to parse message:', error);
        }
    }
    
    /**
     * Handler onClose
     * @param {CloseEvent} event
     */
    onClose(event) {
        const wasConnected = this.connected;
        this.connected = false;
        
        // Log with more detail
        if (event.code === 1006) {
            this.logger.warn('BackendService', `Connection closed abnormally (1006) - Server may not be running`);
        } else {
            this.logger.warn('BackendService', `Connection closed (code: ${event.code}, reason: ${event.reason || 'none'})`);
        }
        
        // Arrêter heartbeat
        this.stopHeartbeat();
        
        // Émettre événement seulement si on était connecté
        if (wasConnected) {
            this.eventBus.emit('websocket:disconnected');
        }
        
        // Planifier reconnexion si on était connecté et qu'on n'a pas dépassé le max
        if (wasConnected && !this.reconnecting && this.reconnectAttempts < this.config.maxReconnectAttempts) {
            this.scheduleReconnect();
        } else if (this.reconnectAttempts >= this.config.maxReconnectAttempts) {
            this.logger.error('BackendService', `Max reconnection attempts (${this.config.maxReconnectAttempts}) reached. Giving up.`);
            this.eventBus.emit('websocket:reconnect-failed');
        }
    }
    
    /**
     * Handler onError
     * @param {Event} error
     */
    onError(error) {
        this.logger.error('BackendService', 'WebSocket error:', error);
        this.eventBus.emit('websocket:error', error);
    }
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    /**
     * Envoie un message brut au serveur
     * @param {Object} message - Message à envoyer
     */
    sendMessage(message) {
        if (!this.isConnected()) {
            this.logger.warn('BackendService', 'Not connected, message queued');
            this.messageQueue.push(message);
            return;
        }
        
        try {
            const json = JSON.stringify(message);
            this.ws.send(json);
            this.stats.messagesSent++;
        } catch (error) {
            this.logger.error('BackendService', 'Failed to send message:', error);
        }
    }
    
    /**
     * Envoie une commande au serveur (format enveloppe)
     * @param {string} command - Commande à envoyer
     * @param {Object} params - Paramètres de la commande
     * @returns {Promise<Object>}
     */
    sendCommand(command, params = {}) {
        const requestId = this.generateRequestId();
        
        const message = {
            envelope: {
                version: this.config.protocolVersion,
                type: 'request',
                id: requestId,
                timestamp: Date.now()
            },
            request: {
                command: command,
                params: params
            }
        };
        
        this.logger.debug('BackendService', `→ Command: ${command}`, params);
        
        return new Promise((resolve, reject) => {
            // Créer timeout
            const timeout = setTimeout(() => {
                this.pendingRequests.delete(requestId);
                reject(new Error(`Request timeout: ${command}`));
            }, this.config.requestTimeout);
            
            // Stocker la promesse
            this.pendingRequests.set(requestId, { resolve, reject, timeout });
            
            // Envoyer
            this.sendMessage(message);
            this.stats.requestsSent++;
        });
    }
    
    // ========================================================================
    // GESTION PROTOCOLE v3.0 (ENVELOPPES)
    // ========================================================================
    
    /**
     * Gère un message avec enveloppe (nouveau format)
     * @param {Object} message
     */
    handleEnvelopeMessage(message) {
        const { envelope } = message;
        
        switch (envelope.type) {
            case 'response':
                this.handleResponse(message);
                break;
            case 'event':
                this.handleEvent(message);
                break;
            case 'error':
                this.handleErrorMessage(message);
                break;
            default:
                this.logger.warn('BackendService', `Unknown message type: ${envelope.type}`);
        }
    }
    
    /**
     * Gère un message legacy (ancien format)
     * @param {Object} message
     */
    handleLegacyMessage(message) {
        // Convertir en format enveloppe
        if (message.type === 'event') {
            this.handleEvent({
                envelope: { type: 'event' },
                event: message
            });
        } else if (message.error) {
            this.handleErrorMessage({
                envelope: { type: 'error' },
                error: { message: message.error }
            });
        } else {
            this.handleResponse({
                envelope: { type: 'response', id: message.requestId },
                response: message
            });
        }
    }
    
    /**
     * Gère une réponse
     * @param {Object} message
     */
    handleResponse(message) {
        this.stats.responsesReceived++;
        
        const requestId = message.envelope.id;
        const pending = this.pendingRequests.get(requestId);
        
        if (pending) {
            clearTimeout(pending.timeout);
            this.pendingRequests.delete(requestId);
            
            if (message.response.success) {
                pending.resolve(message.response.data);
            } else {
                pending.reject(new Error(message.response.error || 'Unknown error'));
            }
        }
    }
    
    /**
     * Gère un événement
     * @param {Object} message
     */
    handleEvent(message) {
        this.stats.eventsReceived++;
        
        const event = message.event;
        
        // Émettre sur EventBus
        this.eventBus.emit(`backend:${event.type}`, event.data);
    }
    
    /**
     * Gère un message d'erreur
     * @param {Object} message
     */
    handleErrorMessage(message) {
        this.stats.errorsReceived++;
        
        const requestId = message.envelope?.id;
        
        if (requestId) {
            const pending = this.pendingRequests.get(requestId);
            if (pending) {
                clearTimeout(pending.timeout);
                this.pendingRequests.delete(requestId);
                pending.reject(new Error(message.error.message));
            }
        }
        
        this.logger.error('BackendService', 'Server error:', message.error);
    }
    
    // ========================================================================
    // RECONNEXION & HEARTBEAT
    // ========================================================================
    
    /**
     * Planifie une reconnexion avec backoff exponentiel
     */
    scheduleReconnect() {
        if (this.reconnectTimer) {
            return;
        }
        
        if (this.reconnectAttempts >= this.config.maxReconnectAttempts) {
            this.logger.error('BackendService', 'Max reconnection attempts reached');
            return;
        }
        
        this.reconnecting = true;
        
        const delay = Math.min(
            this.config.reconnectDelay * Math.pow(
                this.config.reconnectBackoff,
                this.reconnectAttempts
            ),
            this.config.maxReconnectDelay
        );
        
        this.logger.info('BackendService', `Reconnecting in ${Math.round(delay/1000)}s (attempt ${this.reconnectAttempts + 1}/${this.config.maxReconnectAttempts})`);
        
        this.reconnectTimer = setTimeout(async () => {
            this.reconnectTimer = null;
            this.reconnectAttempts++;
            this.stats.reconnections++;
            
            try {
                await this.connect();
            } catch (error) {
                this.logger.error('BackendService', 'Reconnection failed:', error.message);
                this.scheduleReconnect();
            }
        }, delay);
    }
    
    /**
     * Démarre le heartbeat (ping toutes les 30s)
     */
    startHeartbeat() {
        if (this.heartbeatTimer) {
            return;
        }
        
        this.heartbeatTimer = setInterval(() => {
            if (this.isConnected()) {
                const ping = {
                    envelope: {
                        version: this.config.protocolVersion,
                        type: 'request',
                        id: 'ping-' + Date.now(),
                        timestamp: Date.now()
                    },
                    request: {
                        command: 'system.ping',
                        params: {}
                    }
                };
                this.sendMessage(ping);
            }
        }, this.config.heartbeatInterval);
    }
    
    /**
     * Arrête le heartbeat
     */
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
        }
    }
    
    // ========================================================================
    // FILE D'ATTENTE
    // ========================================================================
    
    /**
     * Vide la file d'attente des messages
     */
    flushMessageQueue() {
        if (this.messageQueue.length === 0) {
            return;
        }
        
        this.logger.info('BackendService', `Sending ${this.messageQueue.length} queued messages...`);
        
        while (this.messageQueue.length > 0) {
            const message = this.messageQueue.shift();
            this.sendMessage(message);
        }
    }
    
    /**
     * Rejette toutes les requêtes en attente
     * @param {string} reason - Raison du rejet
     */
    rejectAllPendingRequests(reason) {
        for (const [requestId, pending] of this.pendingRequests.entries()) {
            if (pending.timeout) {
                clearTimeout(pending.timeout);
            }
            pending.reject(new Error(reason));
        }
        this.pendingRequests.clear();
    }
    
    // ========================================================================
    // API COMMANDES (unchanged from original)
    // ========================================================================
    
    // System
    async getSystemInfo() {
        return this.sendCommand('system.get-info');
    }
    
    async systemPing() {
        return this.sendCommand('system.ping');
    }
    
    // Playback
    async play() {
        return this.sendCommand('playback.play');
    }
    
    async pause() {
        return this.sendCommand('playback.pause');
    }
    
    async stop() {
        return this.sendCommand('playback.stop');
    }
    
    async seek(position) {
        return this.sendCommand('playback.seek', { position });
    }
    
    async getPlaybackState() {
        return this.sendCommand('playback.get-state');
    }
    
    // Playlist
    async loadPlaylist(filePath) {
        return this.sendCommand('playlist.load', { file_path: filePath });
    }
    
    async getPlaylist() {
        return this.sendCommand('playlist.get');
    }
    
    async setTrack(index) {
        return this.sendCommand('playlist.set-track', { index });
    }
    
    // Files
    async listFiles(path = '/') {
        return this.sendCommand('files.list', { path });
    }
    
    async uploadFile(filename, base64Data) {
        return this.sendCommand('files.upload', { filename, data: base64Data });
    }
    
    async getFile(fileId) {
        return this.sendCommand('files.get', { file_id: fileId });
    }
    
    async deleteFile(filePath) {
        return this.sendCommand('files.delete', { file_path: filePath });
    }
    
    // Devices
    async listDevices() {
        return this.sendCommand('devices.list');
    }
    
    async connectDevice(deviceId) {
        return this.sendCommand('devices.connect', { device_id: deviceId });
    }
    
    async disconnectDevice(deviceId) {
        return this.sendCommand('devices.disconnect', { device_id: deviceId });
    }
    
    // Routing
    async addRoute(route) {
        return this.sendCommand('routing.addRoute', route);
    }
    
    async removeRoute(routeId) {
        return this.sendCommand('routing.removeRoute', { route_id: routeId });
    }
    
    async listRoutes() {
        return this.sendCommand('routing.listRoutes');
    }
    
    async updateRoute(routeId, changes) {
        return this.sendCommand('routing.updateRoute', { route_id: routeId, ...changes });
    }
    
    // Editor
    async editorLoad(filePath) {
        return this.sendCommand('editor.load', { file_path: filePath });
    }
    
    async editorSave(filePath, jsonMidi) {
        return this.sendCommand('editor.save', { file_path: filePath, jsonmidi: jsonMidi });
    }
    
    async editorAddNote(note) {
        return this.sendCommand('editor.addNote', note);
    }
    
    async editorDeleteNote(noteId) {
        return this.sendCommand('editor.deleteNote', { note_id: noteId });
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Génère un ID de requête unique
     * @returns {string}
     */
    generateRequestId() {
        return `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    }
    
    /**
     * Obtient les statistiques
     * @returns {Object}
     */
    getStats() {
        return {
            ...this.stats,
            connected: this.connected,
            reconnecting: this.reconnecting,
            reconnectAttempts: this.reconnectAttempts,
            pendingRequests: this.pendingRequests.size,
            queuedMessages: this.messageQueue.length,
            configuredUrl: this.config.url
        };
    }
    
    /**
     * Réinitialise les statistiques
     */
    resetStats() {
        this.stats = {
            messagesSent: 0,
            messagesReceived: 0,
            requestsSent: 0,
            responsesReceived: 0,
            eventsReceived: 0,
            errorsReceived: 0,
            reconnections: 0,
            connectionFailures: 0
        };
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackendService;
}
// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Version: v3.2.0 - CONFORME DOCUMENTATION WEBSOCKET
// Date: 2025-10-21
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// CONFORMITÉ v3.2.0:
// ✅ Structure EXACTE selon documentation WebSocket
// ✅ Format Envelope: {id, type, timestamp, version, payload}
// ✅ REQUEST payload: {id, command, params, timeout}
// ✅ RESPONSE payload: {request_id, success, data, latency}
// ✅ EVENT payload: {name, data, priority}
// ✅ ERROR payload: {code, message, details, retryable}
// ✅ UUID v4 pour les IDs
// ✅ Timestamp ISO 8601 format
// ✅ URL correcte: ws://localhost:8080
// ✅ Gestion erreurs selon codes documentés
// ============================================================================

class BackendService {
    constructor(eventBus, logger) {
        this.eventBus = eventBus;
        this.logger = logger;
        
        // Configuration
        this.config = {
            url: 'ws://localhost:8080', // URL selon documentation
            reconnectDelay: 2000,
            maxReconnectDelay: 30000,
            reconnectBackoff: 1.5,
            requestTimeout: 10000,
            heartbeatInterval: 30000,
            protocolVersion: '1.0', // Version selon documentation
            connectionTimeout: 5000,
            maxReconnectAttempts: 10
        };
        
        // État
        this.ws = null;
        this.connected = false;
        this.reconnecting = false;
        this.reconnectAttempts = 0;
        this.reconnectTimer = null;
        this.heartbeatTimer = null;
        this.connectionTimer = null;
        
        // Gestion des requêtes
        this.pendingRequests = new Map(); // requestId -> {resolve, reject, timeout, startTime}
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
        
        this.logger.info('BackendService', '✓ Service initialized (Protocol v1.0 - WebSocket Envelope)');
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
     * Handler onMessage - Parse selon documentation
     * @param {MessageEvent} event
     */
    onMessage(event) {
        this.stats.messagesReceived++;
        
        try {
            const envelope = JSON.parse(event.data);
            
            // Vérifier structure Envelope selon documentation
            if (!envelope.id || !envelope.type || !envelope.timestamp || !envelope.version || !envelope.payload) {
                this.logger.warn('BackendService', 'Invalid message format (missing Envelope fields)');
                return;
            }
            
            this.logger.debug('BackendService', `← ${envelope.type.toUpperCase()}:`, envelope.id);
            
            // Router selon type
            switch (envelope.type) {
                case 'response':
                    this.handleResponse(envelope);
                    break;
                case 'event':
                    this.handleEvent(envelope);
                    break;
                case 'error':
                    this.handleError(envelope);
                    break;
                default:
                    this.logger.warn('BackendService', `Unknown message type: ${envelope.type}`);
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
        
        // Log avec détails selon code
        if (event.code === 1006) {
            this.logger.warn('BackendService', `Connection closed abnormally (1006) - Server may not be running on ${this.config.url}`);
        } else {
            this.logger.warn('BackendService', `Connection closed (code: ${event.code}, reason: ${event.reason || 'none'})`);
        }
        
        // Arrêter heartbeat
        this.stopHeartbeat();
        
        // Émettre événement seulement si on était connecté
        if (wasConnected) {
            this.eventBus.emit('websocket:disconnected');
        }
        
        // Planifier reconnexion
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
    // GESTION PROTOCOLE - FORMAT DOCUMENTATION
    // ========================================================================
    
    /**
     * Gère une RESPONSE selon documentation
     * Payload: {request_id, success, data, error_message, error_code, latency}
     * @param {Object} envelope
     */
    handleResponse(envelope) {
        this.stats.responsesReceived++;
        
        const payload = envelope.payload;
        const requestId = payload.request_id;
        
        const pending = this.pendingRequests.get(requestId);
        
        if (pending) {
            clearTimeout(pending.timeout);
            this.pendingRequests.delete(requestId);
            
            // Calculer latence réelle
            const actualLatency = Date.now() - pending.startTime;
            
            this.logger.debug('BackendService', 
                `Response for ${requestId}: ${payload.success ? 'SUCCESS' : 'FAILED'} (${actualLatency}ms)`);
            
            if (payload.success) {
                pending.resolve(payload.data);
            } else {
                const error = new Error(payload.error_message || 'Unknown error');
                error.code = payload.error_code;
                pending.reject(error);
            }
        } else {
            this.logger.warn('BackendService', `No pending request for ${requestId}`);
        }
    }
    
    /**
     * Gère un EVENT selon documentation
     * Payload: {name, data, priority}
     * @param {Object} envelope
     */
    handleEvent(envelope) {
        this.stats.eventsReceived++;
        
        const payload = envelope.payload;
        const eventName = payload.name;
        
        this.logger.debug('BackendService', `Event: ${eventName} [${payload.priority || 'normal'}]`);
        
        // Émettre sur EventBus avec préfixe
        this.eventBus.emit(`backend:${eventName}`, payload.data);
        
        // Émettre aussi l'événement générique
        this.eventBus.emit('backend:event', {
            name: eventName,
            data: payload.data,
            priority: payload.priority || 'normal',
            timestamp: envelope.timestamp
        });
    }
    
    /**
     * Gère une ERROR selon documentation
     * Payload: {code, message, details, retryable}
     * @param {Object} envelope
     */
    handleError(envelope) {
        this.stats.errorsReceived++;
        
        const payload = envelope.payload;
        
        this.logger.error('BackendService', `Server error [${payload.code}]: ${payload.message}`, payload.details);
        
        // Émettre événement d'erreur
        this.eventBus.emit('backend:error', {
            code: payload.code,
            message: payload.message,
            details: payload.details,
            retryable: payload.retryable || false,
            timestamp: envelope.timestamp
        });
    }
    
    // ========================================================================
    // ENVOI DE MESSAGES - FORMAT DOCUMENTATION
    // ========================================================================
    
    /**
     * Génère un UUID v4 conforme
     * @returns {string}
     */
    generateUUID() {
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
            const r = Math.random() * 16 | 0;
            const v = c === 'x' ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
    }
    
    /**
     * Génère un timestamp ISO 8601 avec millisecondes
     * @returns {string}
     */
    generateTimestamp() {
        return new Date().toISOString();
    }
    
    /**
     * Envoie un message brut au serveur
     * @param {Object} envelope - Envelope complet
     */
    sendMessage(envelope) {
        if (!this.isConnected()) {
            this.logger.warn('BackendService', 'Not connected, message queued');
            this.messageQueue.push(envelope);
            return;
        }
        
        try {
            const json = JSON.stringify(envelope);
            this.ws.send(json);
            this.stats.messagesSent++;
        } catch (error) {
            this.logger.error('BackendService', 'Failed to send message:', error);
        }
    }
    
    /**
     * Envoie une commande selon format documentation
     * @param {string} command - Commande (ex: "devices:list")
     * @param {Object} params - Paramètres
     * @param {number} timeout - Timeout optionnel (ms)
     * @returns {Promise<Object>}
     */
    sendCommand(command, params = {}, timeout = null) {
        const envelopeId = this.generateUUID();
        const requestId = this.generateUUID();
        const timestamp = this.generateTimestamp();
        
        // Construction selon documentation exacte
        const envelope = {
            id: envelopeId,
            type: 'request',
            timestamp: timestamp,
            version: this.config.protocolVersion,
            payload: {
                id: requestId,
                command: command,
                params: params,
                timeout: timeout || this.config.requestTimeout
            }
        };
        
        this.logger.debug('BackendService', `→ REQUEST: ${command}`, params);
        
        return new Promise((resolve, reject) => {
            const startTime = Date.now();
            
            // Créer timeout
            const timeoutHandle = setTimeout(() => {
                this.pendingRequests.delete(requestId);
                this.logger.warn('BackendService', `Request timeout: ${command} (${timeout || this.config.requestTimeout}ms)`);
                reject(new Error(`Request timeout: ${command}`));
            }, timeout || this.config.requestTimeout);
            
            // Stocker la promesse avec startTime pour calcul latence
            this.pendingRequests.set(requestId, { 
                resolve, 
                reject, 
                timeout: timeoutHandle,
                startTime,
                command
            });
            
            // Envoyer
            this.sendMessage(envelope);
            this.stats.requestsSent++;
        });
    }
    
    // ========================================================================
    // API COMMANDES - SELON DOCUMENTATION
    // ========================================================================
    
    // --- System ---
    
    async getSystemInfo() {
        return this.sendCommand('session:info');
    }
    
    async systemPing() {
        return this.sendCommand('system:ping');
    }
    
    // --- Devices MIDI ---
    
    async listDevices() {
        return this.sendCommand('devices:list');
    }
    
    async getDeviceInfo(deviceId) {
        return this.sendCommand('devices:info', { deviceId });
    }
    
    async connectDevice(deviceId) {
        return this.sendCommand('devices:connect', { deviceId });
    }
    
    async disconnectDevice(deviceId) {
        return this.sendCommand('devices:disconnect', { deviceId });
    }
    
    // --- MIDI Messages ---
    
    async sendMidiMessage(device, message) {
        return this.sendCommand('midi:send', { device, message });
    }
    
    // --- Player ---
    
    async playerPlay(fileId) {
        return this.sendCommand('player:play', { fileId });
    }
    
    async playerStop() {
        return this.sendCommand('player:stop');
    }
    
    async playerPause() {
        return this.sendCommand('player:pause');
    }
    
    async playerSeek(position) {
        return this.sendCommand('player:seek', { position });
    }
    
    async getPlayerState() {
        return this.sendCommand('player:state');
    }
    
    // --- Presets ---
    
    async loadPreset(presetId) {
        return this.sendCommand('preset:load', { presetId });
    }
    
    async savePreset(presetId, data) {
        return this.sendCommand('preset:save', { presetId, data });
    }
    
    async listPresets() {
        return this.sendCommand('preset:list');
    }
    
    // --- Files ---
    
    async listFiles(path = '/') {
        return this.sendCommand('files:list', { path });
    }
    
    async uploadFile(filename, data) {
        return this.sendCommand('files:upload', { filename, data });
    }
    
    async deleteFile(filePath) {
        return this.sendCommand('files:delete', { filePath });
    }
    
    // --- Routing (si supporté par backend) ---
    
    async addRoute(route) {
        return this.sendCommand('routing:add', route);
    }
    
    async removeRoute(routeId) {
        return this.sendCommand('routing:remove', { routeId });
    }
    
    async listRoutes() {
        return this.sendCommand('routing:list');
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
     * Démarre le heartbeat (ping périodique)
     */
    startHeartbeat() {
        if (this.heartbeatTimer) {
            return;
        }
        
        this.heartbeatTimer = setInterval(() => {
            if (this.isConnected()) {
                // Envoyer ping selon format documentation
                this.sendCommand('system:ping')
                    .catch(err => {
                        this.logger.warn('BackendService', 'Heartbeat ping failed:', err.message);
                    });
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
            const envelope = this.messageQueue.shift();
            this.sendMessage(envelope);
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
    // UTILITAIRES
    // ========================================================================
    
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
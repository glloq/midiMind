// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Version: v5.0.0 - PROTOCOLE WEBSOCKET CONFORME
// Date: 2025-11-02
// ============================================================================
// MODIFICATIONS v5.0.0:
// ✅ Format de message conforme: { id, type, timestamp, version, payload }
// ✅ Types: request, response, event, error
// ✅ Heartbeat avec system.ping (30s interval, 5s timeout)
// ✅ UUID v4 pour les IDs
// ✅ Gestion complète des erreurs selon codes protocole
// ✅ Reconnexion automatique avec backoff exponentiel
// ============================================================================

class BackendService {
    constructor(url, eventBus, logger) {
        this.eventBus = eventBus;
        this.logger = logger || console;
        
        // État de la connexion
        this.ws = null;
        this.connected = false;
        this.connecting = false;
        this.offlineMode = false;
        this.reconnectionStopped = false;
        
        // Configuration selon documentation
        this.config = {
            url: url || 'ws://localhost:8080',
            reconnectInterval: 3000,
            maxReconnectInterval: 30000,
            reconnectDecay: 1.5,
            timeoutInterval: 5000,
            heartbeatInterval: 30000,      // 30 secondes selon doc
            heartbeatTimeout: 5000,        // 5 secondes selon doc
            maxReconnectAttempts: 5,
            defaultCommandTimeout: 5000,
            protocolVersion: '1.0'
        };
        
        // Gestion de la reconnexion
        this.reconnectAttempts = 0;
        this.reconnectTimer = null;
        this.heartbeatTimer = null;
        this.heartbeatTimeoutTimer = null;
        this.connectionTimeout = null;
        
        // Suivi de l'activité backend
        this.lastActivityTime = Date.now();
        this.heartbeatPending = false;
        this.heartbeatFailures = 0;
        
        // File d'attente des messages
        this.messageQueue = [];
        this.maxQueueSize = 100;
        
        // Requêtes en attente
        this.pendingRequests = new Map();
        
        this.logger.info('BackendService', 'Service initialized (v5.0.0 - WebSocket Protocol)');
    }
    
    /**
     * Génère un UUID v4
     */
    generateUUID() {
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
            const r = Math.random() * 16 | 0;
            const v = c === 'x' ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
    }
    
    /**
     * Obtient timestamp ISO 8601 avec millisecondes
     */
    getISO8601Timestamp() {
        return new Date().toISOString();
    }
    
    /**
     * Se connecte au backend
     */
    async connect(url = null) {
        if (this.connected) {
            this.logger.warn('BackendService', 'Already connected');
            return true;
        }
        
        if (this.connecting) {
            this.logger.warn('BackendService', 'Connection already in progress');
            return false;
        }
        
        const wsUrl = url || this.config.url;
        this.connecting = true;
        this.reconnectionStopped = false;
        this.logger.info('BackendService', `Connecting to ${wsUrl}...`);
        
        return new Promise((resolve) => {
            try {
                this.ws = new WebSocket(wsUrl);
                
                // Timeout de connexion
                this.connectionTimeout = setTimeout(() => {
                    if (!this.connected) {
                        this.logger.error('BackendService', 'Connection timeout');
                        this.handleConnectionError('Connection timeout');
                        resolve(false);
                    }
                }, this.config.timeoutInterval);
                
                // Événements WebSocket
                this.ws.onopen = () => {
                    clearTimeout(this.connectionTimeout);
                    this.handleOpen();
                    resolve(true);
                };
                
                this.ws.onclose = (event) => {
                    this.handleClose(event);
                };
                
                this.ws.onerror = (error) => {
                    this.handleError(error);
                    resolve(false);
                };
                
                this.ws.onmessage = (event) => {
                    this.handleMessage(event);
                };
                
            } catch (error) {
                this.logger.error('BackendService', 'Connection error:', error);
                this.connecting = false;
                this.handleConnectionError(error.message);
                resolve(false);
            }
        });
    }
    
    /**
     * Gère l'ouverture de la connexion
     */
    handleOpen() {
        this.connected = true;
        this.connecting = false;
        this.reconnectAttempts = 0;
        this.offlineMode = false;
        this.reconnectionStopped = false;
        this.lastActivityTime = Date.now();
        this.heartbeatFailures = 0;
        
        this.logger.info('BackendService', '✓ Connected to backend');
        
        this.eventBus.emit('backend:connected', {
            url: this.config.url,
            timestamp: Date.now()
        });
        
        // Démarrer le heartbeat
        this.startHeartbeat();
        
        // Envoyer les messages en attente
        this.flushMessageQueue();
    }
    
    /**
     * Gère la fermeture de la connexion
     */
    handleClose(event) {
        const wasConnected = this.connected;
        
        this.connected = false;
        this.connecting = false;
        this.stopHeartbeat();
        
        const reason = event.reason || 'Unknown reason';
        const code = event.code;
        
        this.logger.warn('BackendService', `Disconnected (code: ${code}, reason: ${reason})`);
        
        this.eventBus.emit('backend:disconnected', { 
            code, 
            reason,
            wasConnected,
            offlineMode: this.offlineMode
        });
        
        // Tenter une reconnexion automatique
        if (!event.wasClean && !this.reconnectionStopped) {
            this.scheduleReconnect();
        } else if (this.reconnectionStopped) {
            this.enterOfflineMode();
        }
    }
    
    /**
     * Gère les erreurs
     */
    handleError(error) {
        this.logger.error('BackendService', 'WebSocket error:', error);
        this.eventBus.emit('backend:error', { error });
    }
    
    /**
     * Gère une erreur de connexion
     */
    handleConnectionError(message) {
        this.connected = false;
        this.connecting = false;
        
        this.eventBus.emit('backend:connection-failed', { 
            message,
            attempt: this.reconnectAttempts + 1,
            maxAttempts: this.config.maxReconnectAttempts
        });
        
        this.scheduleReconnect();
    }
    
    /**
     * Planifie une tentative de reconnexion avec backoff exponentiel
     */
    scheduleReconnect() {
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
        }
        
        if (this.reconnectAttempts >= this.config.maxReconnectAttempts) {
            this.logger.error('BackendService', 
                `✗ Max reconnection attempts (${this.config.maxReconnectAttempts}) reached`);
            
            this.reconnectionStopped = true;
            
            this.eventBus.emit('backend:max-reconnect-attempts', {
                attempts: this.reconnectAttempts,
                maxAttempts: this.config.maxReconnectAttempts
            });
            
            this.enterOfflineMode();
            return;
        }
        
        // Calculer le délai avec backoff exponentiel
        const delay = Math.min(
            this.config.reconnectInterval * Math.pow(this.config.reconnectDecay, this.reconnectAttempts),
            this.config.maxReconnectInterval
        );
        
        this.reconnectAttempts++;
        
        this.logger.info('BackendService', 
            `↻ Reconnection attempt ${this.reconnectAttempts}/${this.config.maxReconnectAttempts} in ${Math.round(delay/1000)}s`);
        
        this.eventBus.emit('backend:reconnecting', {
            attempt: this.reconnectAttempts,
            maxAttempts: this.config.maxReconnectAttempts,
            delay: delay
        });
        
        this.reconnectTimer = setTimeout(() => {
            this.logger.info('BackendService', 'Attempting reconnection...');
            this.connect();
        }, delay);
    }
    
    /**
     * Entre en mode offline
     */
    enterOfflineMode() {
        this.offlineMode = true;
        this.logger.warn('BackendService', '⚠️ Entering offline mode');
        
        this.eventBus.emit('backend:offline-mode', {
            timestamp: Date.now()
        });
    }
    
    /**
     * Démarre le heartbeat selon documentation
     */
    startHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
        }
        
        // Envoyer ping toutes les 30 secondes
        this.heartbeatTimer = setInterval(() => {
            this.sendPing();
        }, this.config.heartbeatInterval);
        
        this.logger.debug('BackendService', '💗 Heartbeat started (30s interval)');
    }
    
    /**
     * Envoie un ping selon documentation
     */
    sendPing() {
        if (this.heartbeatPending) {
            this.logger.debug('BackendService', 'Ping already pending, skipping');
            return;
        }
        
        const heartbeatId = `heartbeat-${Date.now()}`;
        
        const ping = {
            id: heartbeatId,
            type: 'request',
            timestamp: this.getISO8601Timestamp(),
            version: this.config.protocolVersion,
            payload: {
                id: heartbeatId,
                command: 'system.ping',
                params: {},
                timeout: this.config.heartbeatTimeout
            }
        };
        
        this.heartbeatPending = true;
        
        // Créer une promesse pour gérer le timeout
        const timeoutPromise = new Promise((resolve) => {
            this.heartbeatTimeoutTimer = setTimeout(() => {
                if (this.heartbeatPending) {
                    this.logger.error('BackendService', '💀 Heartbeat timeout - reconnecting');
                    this.heartbeatPending = false;
                    this.heartbeatFailures++;
                    
                    if (this.heartbeatFailures >= 2) {
                        this.forceReconnect('Heartbeat timeout');
                    }
                }
                resolve();
            }, this.config.heartbeatTimeout);
        });
        
        // Enregistrer la requête pour recevoir la réponse
        this.pendingRequests.set(heartbeatId, {
            resolve: (data) => {
                clearTimeout(this.heartbeatTimeoutTimer);
                this.heartbeatPending = false;
                this.heartbeatFailures = 0;
                this.logger.debug('BackendService', '✓ Heartbeat OK');
            },
            reject: (error) => {
                clearTimeout(this.heartbeatTimeoutTimer);
                this.heartbeatPending = false;
                this.heartbeatFailures++;
                this.logger.warn('BackendService', `⚠️ Heartbeat failed: ${error.message}`);
                
                if (this.heartbeatFailures >= 2) {
                    this.forceReconnect('Multiple heartbeat failures');
                }
            }
        });
        
        this.send(ping);
        this.logger.debug('BackendService', '💗 Sending ping (system.ping)');
    }
    
    /**
     * Arrête le heartbeat
     */
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
        }
        if (this.heartbeatTimeoutTimer) {
            clearTimeout(this.heartbeatTimeoutTimer);
            this.heartbeatTimeoutTimer = null;
        }
        this.heartbeatPending = false;
    }
    
    /**
     * Force une reconnexion immédiate
     */
    forceReconnect(reason) {
        this.logger.warn('BackendService', `Forcing reconnect: ${reason}`);
        
        this.stopHeartbeat();
        
        if (this.ws) {
            this.ws.close(1000, reason);
            this.ws = null;
        }
        
        this.connected = false;
        this.connecting = false;
        
        // Reconnexion immédiate (sans backoff)
        setTimeout(() => {
            this.connect();
        }, 1000);
    }
    
    /**
     * Gère les messages reçus selon le protocole
     */
    handleMessage(event) {
        try {
            const message = JSON.parse(event.data);
            
            // Toute réponse/event = activité backend
            this.lastActivityTime = Date.now();
            
            this.logger.debug('BackendService', 'Message received:', message);
            
            // Vérifier le format du protocole
            if (!message.id || !message.type) {
                this.logger.warn('BackendService', 'Invalid message format (missing id or type)');
                return;
            }
            
            switch (message.type) {
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
                    this.logger.warn('BackendService', `Unknown message type: ${message.type}`);
            }
            
        } catch (error) {
            this.logger.error('BackendService', 'Error parsing message:', error);
        }
    }
    
    /**
     * Gère les réponses
     */
    handleResponse(message) {
        const { payload } = message;
        
        if (!payload || !payload.request_id) {
            this.logger.warn('BackendService', 'Response missing request_id');
            return;
        }
        
        const requestId = payload.request_id;
        
        if (!this.pendingRequests.has(requestId)) {
            this.logger.debug('BackendService', `No pending request for id: ${requestId}`);
            return;
        }
        
        const pending = this.pendingRequests.get(requestId);
        this.pendingRequests.delete(requestId);
        
        if (payload.success === true) {
            pending.resolve(payload.data || payload);
        } else {
            const error = new Error(payload.error_message || 'Command failed');
            error.code = payload.error_code;
            pending.reject(error);
        }
    }
    
    /**
     * Gère les événements
     */
    handleEvent(message) {
        const { payload } = message;
        
        if (!payload || !payload.name) {
            this.logger.warn('BackendService', 'Event missing name');
            return;
        }
        
        const eventName = payload.name;
        const eventData = payload.data || {};
        
        // Émettre événement spécifique
        this.eventBus.emit(`backend:event:${eventName}`, {
            ...eventData,
            priority: payload.priority,
            source: payload.source,
            timestamp: message.timestamp
        });
        
        this.logger.debug('BackendService', `Event received: ${eventName}`, eventData);
    }
    
    /**
     * Gère les messages d'erreur
     */
    handleErrorMessage(message) {
        const { payload } = message;
        
        this.logger.error('BackendService', 'Error message:', payload);
        
        this.eventBus.emit('backend:error-message', {
            code: payload.code,
            message: payload.message,
            details: payload.details,
            retryable: payload.retryable,
            timestamp: message.timestamp
        });
    }
    
    /**
     * Envoie un message au backend
     */
    send(data) {
        if (!this.connected || !this.ws || this.ws.readyState !== WebSocket.OPEN) {
            // Ajouter à la file d'attente
            if (this.messageQueue.length < this.maxQueueSize) {
                this.messageQueue.push(data);
                this.logger.debug('BackendService', 'Message queued (not connected)');
            } else {
                this.logger.warn('BackendService', 'Message queue full, dropping message');
            }
            return false;
        }
        
        try {
            const message = typeof data === 'string' ? data : JSON.stringify(data);
            this.ws.send(message);
            return true;
        } catch (error) {
            this.logger.error('BackendService', 'Error sending message:', error);
            return false;
        }
    }
    
    /**
     * Envoie une commande au backend selon le protocole
     */
    async sendCommand(command, params = {}, timeout = null) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected()) {
                reject(new Error('Not connected to backend'));
                return;
            }
            
            const timeoutMs = timeout || this.config.defaultCommandTimeout;
            const requestId = this.generateUUID();
            
            // Timer de timeout
            const timeoutTimer = setTimeout(() => {
                if (this.pendingRequests.has(requestId)) {
                    this.pendingRequests.delete(requestId);
                    reject(new Error(`Command timeout after ${timeoutMs}ms: ${command}`));
                }
            }, timeoutMs);
            
            // Enregistrer avec cleanup du timeout
            this.pendingRequests.set(requestId, {
                resolve: (data) => {
                    clearTimeout(timeoutTimer);
                    resolve(data);
                },
                reject: (error) => {
                    clearTimeout(timeoutTimer);
                    reject(error);
                }
            });
            
            // Format selon protocole
            const message = {
                id: requestId,
                type: 'request',
                timestamp: this.getISO8601Timestamp(),
                version: this.config.protocolVersion,
                payload: {
                    id: requestId,
                    command: command,
                    params: params,
                    timeout: timeoutMs
                }
            };
            
            this.send(message);
            
            this.logger.debug('BackendService', `Sent command: ${command}`, message);
        });
    }
    
    /**
     * Vide la file d'attente des messages
     */
    flushMessageQueue() {
        if (this.messageQueue.length === 0) return;
        
        this.logger.info('BackendService', `Sending ${this.messageQueue.length} queued messages`);
        
        while (this.messageQueue.length > 0) {
            const message = this.messageQueue.shift();
            this.send(message);
        }
    }
    
    /**
     * Déconnecte du backend
     */
    disconnect() {
        this.stopHeartbeat();
        this.reconnectionStopped = true;
        
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        
        // Rejeter toutes les requêtes en attente
        for (const [requestId, { reject }] of this.pendingRequests.entries()) {
            reject(new Error('Disconnected'));
        }
        this.pendingRequests.clear();
        
        if (this.ws) {
            this.ws.close(1000, 'Client disconnect');
            this.ws = null;
        }
        
        this.connected = false;
        this.connecting = false;
        this.reconnectAttempts = 0;
        
        this.logger.info('BackendService', 'Disconnected');
    }
    
    /**
     * Réactive la reconnexion automatique
     */
    enableReconnection() {
        this.reconnectionStopped = false;
        this.offlineMode = false;
        this.reconnectAttempts = 0;
        
        if (!this.connected && !this.connecting) {
            this.logger.info('BackendService', 'Reconnection enabled, attempting to connect');
            this.connect();
        }
    }
    
    /**
     * Vérifie si connecté
     */
    isConnected() {
        return this.connected && this.ws && this.ws.readyState === WebSocket.OPEN;
    }
    
    /**
     * Vérifie si en mode offline
     */
    isOffline() {
        return this.offlineMode;
    }
    
    /**
     * Obtient l'état de la connexion
     */
    getConnectionState() {
        if (this.offlineMode) return 'offline';
        if (!this.ws) return 'disconnected';
        
        switch (this.ws.readyState) {
            case WebSocket.CONNECTING: return 'connecting';
            case WebSocket.OPEN: return 'connected';
            case WebSocket.CLOSING: return 'closing';
            case WebSocket.CLOSED: return 'disconnected';
            default: return 'unknown';
        }
    }
    
    /**
     * Obtient le statut complet
     */
    getStatus() {
        return {
            connected: this.connected,
            connecting: this.connecting,
            offlineMode: this.offlineMode,
            reconnectAttempts: this.reconnectAttempts,
            maxReconnectAttempts: this.config.maxReconnectAttempts,
            reconnectionStopped: this.reconnectionStopped,
            state: this.getConnectionState(),
            queuedMessages: this.messageQueue.length,
            url: this.config.url,
            lastActivityTime: this.lastActivityTime,
            timeSinceActivity: Date.now() - this.lastActivityTime,
            heartbeatFailures: this.heartbeatFailures,
            heartbeatPending: this.heartbeatPending
        };
    }
    
    // ========================================================================
    // MÉTHODES API - Wrapper pour les commandes backend
    // ========================================================================
    
    // --- DEVICES ---
    async listDevices() {
        return this.sendCommand('devices.list');
    }
    
    async getDevice(deviceId) {
        return this.sendCommand('devices.get', { device_id: deviceId });
    }
    
    async connectDevice(deviceId) {
        return this.sendCommand('devices.connect', { device_id: deviceId });
    }
    
    async disconnectDevice(deviceId) {
        return this.sendCommand('devices.disconnect', { device_id: deviceId });
    }
    
    async refreshDevices() {
        return this.sendCommand('devices.refresh');
    }
    
    // --- PLAYBACK ---
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
    
    async setTempo(tempo) {
        return this.sendCommand('playback.setTempo', { tempo });
    }
    
    async getStatus() {
        return this.sendCommand('playback.status');
    }
    
    // --- FILES ---
    async listFiles(path = '/') {
        return this.sendCommand('files.list', { path });
    }
    
    async loadFile(path) {
        return this.sendCommand('files.load', { path });
    }
    
    async saveFile(path, data) {
        return this.sendCommand('files.save', { path, data });
    }
    
    async deleteFile(path) {
        return this.sendCommand('files.delete', { path });
    }
    
    async getFileInfo(path) {
        return this.sendCommand('files.info', { path });
    }
    
    // --- SYSTEM ---
    async getSystemInfo() {
        return this.sendCommand('system.info');
    }
    
    async ping() {
        return this.sendCommand('system.ping');
    }
    
    async shutdown() {
        return this.sendCommand('system.shutdown');
    }
    
    async restart() {
        return this.sendCommand('system.restart');
    }
    
    // --- ROUTING ---
    async addRoute(sourceId, destId, channel = null) {
        const params = { source_id: sourceId, dest_id: destId };
        if (channel !== null) params.channel = channel;
        return this.sendCommand('routing.add', params);
    }
    
    async removeRoute(routeId) {
        return this.sendCommand('routing.remove', { route_id: routeId });
    }
    
    async listRoutes() {
        return this.sendCommand('routing.list');
    }
    
    async clearRoutes() {
        return this.sendCommand('routing.clear');
    }
    
    // --- EDITOR ---
    async getEditorState() {
        return this.sendCommand('editor.getState');
    }
    
    async setEditorState(state) {
        return this.sendCommand('editor.setState', state);
    }
    
    async addNote(note) {
        return this.sendCommand('editor.addNote', note);
    }
    
    async removeNote(noteId) {
        return this.sendCommand('editor.removeNote', { note_id: noteId });
    }
    
    async updateNote(noteId, note) {
        return this.sendCommand('editor.updateNote', { note_id: noteId, ...note });
    }
    
    // --- INSTRUMENTS ---
    async listInstruments() {
        return this.sendCommand('instruments.list');
    }
    
    async getInstrument(instrumentId) {
        return this.sendCommand('instruments.get', { instrument_id: instrumentId });
    }
    
    async setInstrumentConfig(instrumentId, config) {
        return this.sendCommand('instruments.setConfig', { instrument_id: instrumentId, config });
    }
    
    // --- PLAYLISTS ---
    async createPlaylist(name, items = []) {
        return this.sendCommand('playlist.create', { name, items });
    }
    
    async deletePlaylist(playlistId) {
        return this.sendCommand('playlist.delete', { playlist_id: playlistId });
    }
    
    async updatePlaylist(playlistId, config) {
        return this.sendCommand('playlist.update', { playlist_id: playlistId, config });
    }
    
    async listPlaylists() {
        return this.sendCommand('playlist.list');
    }
    
    async getPlaylist(playlistId) {
        return this.sendCommand('playlist.get', { playlist_id: playlistId });
    }
    
    async addPlaylistItem(playlistId, item) {
        return this.sendCommand('playlist.addItem', { playlist_id: playlistId, item });
    }
    
    async removePlaylistItem(playlistId, itemId) {
        return this.sendCommand('playlist.removeItem', { 
            playlist_id: playlistId, 
            item_id: itemId 
        });
    }
}

// Export pour utilisation
if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackendService;
}
window.BackendService = BackendService;
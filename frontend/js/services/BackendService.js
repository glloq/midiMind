// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Version: v3.3.0 - COMPLETE WITH ALL MISSING METHODS
// Date: 2025-10-23
// ============================================================================
// CORRECTIONS v3.3.0:
// ✓ Ajout de sendCommand() pour tous les contrôleurs
// ✓ Ajout de uploadFile() pour FileService
// ✓ Ajout des méthodes de routing (listDevices, setChannelRouting, etc.)
// ✓ Reconnexion automatique améliorée avec backoff exponentiel
// ✓ Heartbeat pour détecter les connexions mortes
// ✓ File d'attente de messages en cas de déconnexion
// ============================================================================

class BackendService {
    constructor(eventBus, logger) {
        this.eventBus = eventBus;
        this.logger = logger || console;
        
        // État de la connexion
        this.ws = null;
        this.connected = false;
        this.connecting = false;
        
        // Configuration
        this.config = {
            url: 'ws://localhost:8080',
            reconnectInterval: 3000,
            maxReconnectInterval: 30000,
            reconnectDecay: 1.5,
            timeoutInterval: 5000,
            heartbeatInterval: 30000,
            maxReconnectAttempts: 10
        };
        
        // Gestion de la reconnexion
        this.reconnectAttempts = 0;
        this.reconnectTimer = null;
        this.heartbeatTimer = null;
        this.connectionTimeout = null;
        
        // File d'attente des messages
        this.messageQueue = [];
        this.maxQueueSize = 100;
        
        // Compteur de requêtes pour les IDs
        this.requestId = 0;
        this.pendingRequests = new Map();
        
        this.logger.info('BackendService', 'Service initialized');
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
        
        this.logger.info('BackendService', '✓ Connected to backend');
        this.eventBus.emit('backend:connected');
        
        // Démarrer le heartbeat
        this.startHeartbeat();
        
        // Envoyer les messages en attente
        this.flushMessageQueue();
    }
    
    /**
     * Gère la fermeture de la connexion
     */
    handleClose(event) {
        this.connected = false;
        this.connecting = false;
        this.stopHeartbeat();
        
        const reason = event.reason || 'Unknown reason';
        const code = event.code;
        
        this.logger.warn('BackendService', `Disconnected (code: ${code}, reason: ${reason})`);
        this.eventBus.emit('websocket:disconnected', { code, reason });
        
        // Tenter une reconnexion automatique
        if (!event.wasClean) {
            this.scheduleReconnect();
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
            attempt: this.reconnectAttempts + 1
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
            this.logger.error('BackendService', 'Max reconnection attempts reached');
            this.eventBus.emit('backend:max-reconnect-attempts');
            return;
        }
        
        // Calculer le délai avec backoff exponentiel
        const delay = Math.min(
            this.config.reconnectInterval * Math.pow(this.config.reconnectDecay, this.reconnectAttempts),
            this.config.maxReconnectInterval
        );
        
        this.reconnectAttempts++;
        this.logger.info('BackendService', `Scheduling reconnect in ${Math.round(delay/1000)}s (attempt ${this.reconnectAttempts}/${this.config.maxReconnectAttempts})`);
        
        this.reconnectTimer = setTimeout(() => {
            this.logger.info('BackendService', 'Attempting reconnection...');
            this.connect();
        }, delay);
    }
    
    /**
     * Démarre le heartbeat
     */
    startHeartbeat() {
        this.stopHeartbeat();
        
        this.heartbeatTimer = setInterval(() => {
            if (this.connected) {
                this.send({ type: 'ping' });
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
    
    /**
     * Gère les messages reçus
     */
    handleMessage(event) {
        try {
            const data = JSON.parse(event.data);
            
            // Répondre aux pings
            if (data.type === 'ping') {
                this.send({ type: 'pong' });
                return;
            }
            
            // Gérer les réponses aux requêtes
            if (data.requestId && this.pendingRequests.has(data.requestId)) {
                const { resolve, reject } = this.pendingRequests.get(data.requestId);
                this.pendingRequests.delete(data.requestId);
                
                if (data.error) {
                    reject(new Error(data.error));
                } else {
                    resolve(data);
                }
                return;
            }
            
            // Émettre le message via l'eventBus
            this.eventBus.emit('backend:message', data);
            
            // Émettre également un événement spécifique au type
            if (data.type) {
                this.eventBus.emit(`backend:${data.type}`, data);
            }
            
        } catch (error) {
            this.logger.error('BackendService', 'Error parsing message:', error);
        }
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
     * Envoie une commande au backend et attend la réponse
     * @param {string} command - Nom de la commande
     * @param {Object} params - Paramètres de la commande
     * @returns {Promise<Object>} Réponse du backend
     */
    async sendCommand(command, params = {}) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected()) {
                reject(new Error('Not connected to backend'));
                return;
            }
            
            const requestId = ++this.requestId;
            
            // Enregistrer la requête en attente
            this.pendingRequests.set(requestId, { resolve, reject });
            
            // Timeout après 30 secondes
            setTimeout(() => {
                if (this.pendingRequests.has(requestId)) {
                    this.pendingRequests.delete(requestId);
                    reject(new Error('Command timeout'));
                }
            }, 30000);
            
            // Envoyer la commande
            this.send({
                type: 'command',
                command,
                params,
                requestId
            });
        });
    }
    
    /**
     * Upload un fichier au backend
     * @param {File} file - Fichier à uploader
     * @param {Function} progressCallback - Callback pour la progression
     * @returns {Promise<Object>} Réponse du backend
     */
    async uploadFile(file, progressCallback = null) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected()) {
                reject(new Error('Not connected to backend'));
                return;
            }
            
            const reader = new FileReader();
            
            reader.onload = async (event) => {
                try {
                    const data = event.target.result;
                    const base64Data = btoa(
                        new Uint8Array(data).reduce((data, byte) => data + String.fromCharCode(byte), '')
                    );
                    
                    const response = await this.sendCommand('upload_file', {
                        filename: file.name,
                        data: base64Data,
                        size: file.size,
                        type: file.type
                    });
                    
                    resolve(response);
                } catch (error) {
                    reject(error);
                }
            };
            
            reader.onerror = () => {
                reject(new Error('Error reading file'));
            };
            
            reader.readAsArrayBuffer(file);
        });
    }
    
    // ========================================================================
    // MÉTHODES DE ROUTING
    // ========================================================================
    
    /**
     * Liste les devices MIDI disponibles
     */
    async listDevices() {
        return this.sendCommand('list_devices');
    }
    
    /**
     * Obtient la configuration de routing actuelle
     */
    async getRouting() {
        return this.sendCommand('get_routing');
    }
    
    /**
     * Configure le routing d'un canal
     */
    async setChannelRouting(channelId, deviceId) {
        return this.sendCommand('set_channel_routing', { channelId, deviceId });
    }
    
    /**
     * Définit le volume d'un canal
     */
    async setChannelVolume(channelId, volume) {
        return this.sendCommand('set_channel_volume', { channelId, volume });
    }
    
    /**
     * Définit le panoramique d'un canal
     */
    async setChannelPan(channelId, pan) {
        return this.sendCommand('set_channel_pan', { channelId, pan });
    }
    
    /**
     * Active/désactive le mute d'un canal
     */
    async muteChannel(channelId, muted) {
        return this.sendCommand('mute_channel', { channelId, muted });
    }
    
    /**
     * Active/désactive le solo d'un canal
     */
    async soloChannel(channelId, soloed) {
        return this.sendCommand('solo_channel', { channelId, soloed });
    }
    
    /**
     * Définit la transposition d'un canal
     */
    async setChannelTranspose(channelId, semitones) {
        return this.sendCommand('set_channel_transpose', { channelId, semitones });
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
     * Vérifie si connecté
     */
    isConnected() {
        return this.connected && this.ws && this.ws.readyState === WebSocket.OPEN;
    }
    
    /**
     * Obtient l'état de la connexion
     */
    getConnectionState() {
        if (!this.ws) return 'disconnected';
        
        switch (this.ws.readyState) {
            case WebSocket.CONNECTING: return 'connecting';
            case WebSocket.OPEN: return 'connected';
            case WebSocket.CLOSING: return 'closing';
            case WebSocket.CLOSED: return 'disconnected';
            default: return 'unknown';
        }
    }
}

// Export pour utilisation
if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackendService;
}
window.BackendService = BackendService;
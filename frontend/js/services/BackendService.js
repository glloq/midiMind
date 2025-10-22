// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Version: v3.2.0 - IMPROVED RECONNECTION
// Date: 2025-10-22
// ============================================================================
// AMÉLIORATIONS v3.2.0:
// ✓ Reconnexion automatique améliorée avec backoff exponentiel
// ✓ Meilleure gestion des erreurs de connexion
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
// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Version: v3.2.0 - CORRECTED (Retry Logic + Better Error Handling)
// Date: 2025-10-21
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// CORRECTIONS v3.2.0:
// ✅ Ajout retry automatique (5 tentatives avec backoff)
// ✅ Timeout configurables sur connexion
// ✅ Mode graceful degradation si backend indisponible
// ✅ Meilleure gestion des erreurs de connexion
// ✅ Événement 'backend:connected' pour init différée
// ✅ Conservation de toutes les fonctionnalités v3.1.1
// ============================================================================

class BackendService {
    constructor(eventBus, logger) {
        this.eventBus = eventBus;
        this.logger = logger;
        
        // Configuration
        this.config = {
            url: 'ws://localhost:8080',
            reconnectDelay: 1000,
            maxReconnectDelay: 30000,
            reconnectBackoff: 1.5,
            requestTimeout: 10000,
            heartbeatInterval: 30000,
            protocolVersion: '3.0',
            // ← NOUVEAU: Configuration retry
            maxRetries: 5,
            retryBaseDelay: 1000,
            connectionTimeout: 5000
        };
        
        // État
        this.ws = null;
        this.connected = false;
        this.reconnecting = false;
        this.reconnectAttempts = 0;
        this.reconnectTimer = null;
        this.heartbeatTimer = null;
        
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
            connectionAttempts: 0,
            connectionFailures: 0
        };
        
        this.logger.info('BackendService', '✓ Service initialized (Protocol v3.0 with retry)');
    }
    
    // ========================================================================
    // CONNEXION / DÉCONNEXION - AVEC RETRY
    // ========================================================================
    
    /**
     * Connecte au serveur WebSocket avec retry automatique
     * @param {string} url - URL du serveur (optionnel)
     * @returns {Promise<void>}
     */
    async connect(url = null) {
        if (url) {
            this.config.url = url;
        }
        
        const maxRetries = this.config.maxRetries;
        const baseDelay = this.config.retryBaseDelay;
        
        for (let attempt = 1; attempt <= maxRetries; attempt++) {
            try {
                this.stats.connectionAttempts++;
                
                this.logger.info('BackendService', 
                    `🔌 Connection attempt ${attempt}/${maxRetries} to ${this.config.url}`);
                
                // Tenter connexion avec timeout
                await Promise.race([
                    this._connectOnce(),
                    this._timeout(this.config.connectionTimeout, `Connection timeout (${this.config.connectionTimeout}ms)`)
                ]);
                
                this.logger.info('BackendService', '✅ Connected to backend');
                
                // Émettre événement de connexion réussie
                this.eventBus.emit('backend:connected');
                
                return; // Succès !
                
            } catch (error) {
                this.stats.connectionFailures++;
                
                this.logger.warn('BackendService', 
                    `Attempt ${attempt}/${maxRetries} failed: ${error.message}`);
                
                if (attempt < maxRetries) {
                    // Calcul du délai avec backoff exponentiel
                    const delay = baseDelay * Math.pow(2, attempt - 1);
                    this.logger.info('BackendService', `Retrying in ${delay}ms...`);
                    await this._sleep(delay);
                } else {
                    // Échec après toutes les tentatives
                    const errorMsg = `Failed to connect after ${maxRetries} attempts`;
                    this.logger.error('BackendService', errorMsg);
                    
                    // Émettre événement d'échec
                    this.eventBus.emit('backend:connection-failed', { 
                        error: errorMsg,
                        attempts: maxRetries 
                    });
                    
                    throw new Error(errorMsg);
                }
            }
        }
    }
    
    /**
     * Tentative de connexion unique (interne)
     * @private
     * @returns {Promise<void>}
     */
    _connectOnce() {
        return new Promise((resolve, reject) => {
            try {
                this.ws = new WebSocket(this.config.url);
                
                // Handler onopen
                this.ws.onopen = () => {
                    this.connected = true;
                    this.reconnecting = false;
                    this.reconnectAttempts = 0;
                    
                    // Démarrer heartbeat
                    this.startHeartbeat();
                    
                    // Vider la queue
                    this.flushMessageQueue();
                    
                    // Émettre événement
                    this.eventBus.emit('websocket:connected');
                    
                    resolve();
                };
                
                // Handler onmessage
                this.ws.onmessage = (event) => {
                    this.onMessage(event);
                };
                
                // Handler onclose
                this.ws.onclose = (event) => {
                    this.onClose(event);
                };
                
                // Handler onerror
                this.ws.onerror = (error) => {
                    this.onError(error);
                    
                    // Rejeter uniquement si pas encore connecté
                    if (!this.connected) {
                        reject(new Error('WebSocket connection error'));
                    }
                };
                
            } catch (error) {
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
        
        // Arrêter heartbeat
        this.stopHeartbeat();
        
        // Rejeter toutes les requêtes en attente
        this.rejectAllPendingRequests('Disconnected');
        
        // Fermer la connexion
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
        
        this.connected = false;
        this.reconnecting = false;
        
        this.logger.info('BackendService', '✓ Disconnected');
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
        try {
            this.stats.messagesReceived++;
            
            const message = JSON.parse(event.data);
            
            // Détecter format du message
            if (message.envelope) {
                // Format v3.0 avec enveloppe
                this.handleEnvelopeMessage(message);
            } else {
                // Format legacy
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
        this.logger.warn('BackendService', `Connection closed (code: ${event.code})`);
        
        this.connected = false;
        
        // Arrêter heartbeat
        this.stopHeartbeat();
        
        // Émettre événement
        this.eventBus.emit('websocket:disconnected', { code: event.code });
        
        // Planifier reconnexion
        if (!this.reconnecting) {
            this.scheduleReconnect();
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
                error: message
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
     * @param {Object} message - Message réponse
     */
    handleResponse(message) {
        this.stats.responsesReceived++;
        
        const { envelope, response } = message;
        const requestId = envelope.id;
        
        if (!requestId) {
            this.logger.warn('BackendService', 'Response without requestId');
            return;
        }
        
        const pending = this.pendingRequests.get(requestId);
        
        if (!pending) {
            this.logger.warn('BackendService', `No pending request for ${requestId}`);
            return;
        }
        
        // Nettoyer timeout
        if (pending.timeout) {
            clearTimeout(pending.timeout);
        }
        
        // Résoudre ou rejeter
        if (response.success !== false) {
            pending.resolve(response);
        } else {
            pending.reject(new Error(response.error || 'Command failed'));
        }
        
        // Supprimer de la map
        this.pendingRequests.delete(requestId);
    }
    
    /**
     * Gère un événement
     * @param {Object} message - Message événement
     */
    handleEvent(message) {
        this.stats.eventsReceived++;
        
        const { event } = message;
        
        this.logger.debug('BackendService', `← EVENT: ${event.name || event.type}`);
        
        // Émettre sur EventBus
        const eventName = `backend:${event.name || event.type}`;
        this.eventBus.emit(eventName, event.data);
    }
    
    /**
     * Gère un message d'erreur
     * @param {Object} message - Message erreur
     */
    handleErrorMessage(message) {
        this.stats.errorsReceived++;
        
        const { error } = message;
        
        this.logger.error('BackendService', '← ERROR:', error.message);
        
        // Émettre sur EventBus
        this.eventBus.emit('backend:error', error);
    }
    
    // ========================================================================
    // ENVOI DE COMMANDES
    // ========================================================================
    
    /**
     * Envoie une commande au backend
     * @param {string} command - Nom de la commande
     * @param {Object} params - Paramètres de la commande
     * @param {number} timeout - Timeout en ms (optionnel)
     * @returns {Promise<Object>} - Réponse du backend
     */
    sendCommand(command, params = {}, timeout = null) {
        return new Promise((resolve, reject) => {
            // Générer requestId unique
            const requestId = this.generateRequestId();
            
            // Créer enveloppe
            const envelope = {
                type: 'request',
                id: requestId,
                timestamp: Date.now()
            };
            
            // Créer message complet
            const message = {
                envelope,
                request: {
                    command,
                    params
                }
            };
            
            // Si déconnecté, mettre en queue
            if (!this.isConnected()) {
                this.logger.warn('BackendService', 'Not connected, message queued');
                this.messageQueue.push({ message, resolve, reject });
                return;
            }
            
            // Stocker requête en attente
            const timeoutMs = timeout || this.config.requestTimeout;
            const timeoutHandle = setTimeout(() => {
                this.pendingRequests.delete(requestId);
                reject(new Error(`Request timeout (${timeoutMs}ms): ${command}`));
            }, timeoutMs);
            
            this.pendingRequests.set(requestId, {
                resolve,
                reject,
                timeout: timeoutHandle,
                command,
                timestamp: Date.now()
            });
            
            // Envoyer
            try {
                this.ws.send(JSON.stringify(message));
                this.stats.messagesSent++;
                this.stats.requestsSent++;
                
                this.logger.debug('BackendService', `→ REQUEST: ${command}`, params);
                
            } catch (error) {
                this.pendingRequests.delete(requestId);
                clearTimeout(timeoutHandle);
                reject(error);
            }
        });
    }
    
    /**
     * Vide la queue de messages
     */
    flushMessageQueue() {
        if (this.messageQueue.length === 0) return;
        
        this.logger.info('BackendService', `Flushing ${this.messageQueue.length} queued messages`);
        
        const queue = [...this.messageQueue];
        this.messageQueue = [];
        
        queue.forEach(({ message, resolve, reject }) => {
            try {
                this.ws.send(JSON.stringify(message));
                
                // Remettre en pending
                const requestId = message.envelope.id;
                const timeoutHandle = setTimeout(() => {
                    this.pendingRequests.delete(requestId);
                    reject(new Error('Request timeout'));
                }, this.config.requestTimeout);
                
                this.pendingRequests.set(requestId, {
                    resolve,
                    reject,
                    timeout: timeoutHandle,
                    timestamp: Date.now()
                });
                
            } catch (error) {
                reject(error);
            }
        });
    }
    
    /**
     * Rejette toutes les requêtes en attente
     * @param {string} reason - Raison du rejet
     */
    rejectAllPendingRequests(reason) {
        this.pendingRequests.forEach((pending, requestId) => {
            if (pending.timeout) {
                clearTimeout(pending.timeout);
            }
            pending.reject(new Error(reason));
        });
        
        this.pendingRequests.clear();
    }
    
    // ========================================================================
    // RECONNEXION AUTOMATIQUE
    // ========================================================================
    
    /**
     * Planifie une reconnexion
     */
    scheduleReconnect() {
        if (this.reconnecting || this.connected) return;
        
        this.reconnecting = true;
        this.reconnectAttempts++;
        this.stats.reconnections++;
        
        // Calcul du délai avec backoff
        const delay = Math.min(
            this.config.reconnectDelay * Math.pow(this.config.reconnectBackoff, this.reconnectAttempts - 1),
            this.config.maxReconnectDelay
        );
        
        this.logger.info('BackendService', 
            `Scheduling reconnection attempt ${this.reconnectAttempts} in ${delay}ms`);
        
        this.reconnectTimer = setTimeout(async () => {
            try {
                await this.connect();
            } catch (error) {
                // La reconnexion sera replanifiée par onClose
                this.logger.warn('BackendService', 'Reconnection failed:', error.message);
            }
        }, delay);
    }
    
    // ========================================================================
    // HEARTBEAT
    // ========================================================================
    
    /**
     * Démarre le heartbeat
     */
    startHeartbeat() {
        this.stopHeartbeat(); // Au cas où
        
        this.heartbeatTimer = setInterval(() => {
            if (this.isConnected()) {
                this.sendCommand('system.ping').catch(error => {
                    this.logger.warn('BackendService', 'Heartbeat failed:', error.message);
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
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Génère un requestId unique
     * @returns {string}
     */
    generateRequestId() {
        return `req_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    }
    
    /**
     * Promise timeout
     * @private
     * @param {number} ms - Délai en millisecondes
     * @param {string} message - Message d'erreur
     * @returns {Promise}
     */
    _timeout(ms, message = 'Timeout') {
        return new Promise((_, reject) => {
            setTimeout(() => reject(new Error(message)), ms);
        });
    }
    
    /**
     * Sleep async
     * @private
     * @param {number} ms - Délai en millisecondes
     * @returns {Promise}
     */
    _sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
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
            queuedMessages: this.messageQueue.length
        };
    }
}

// ============================================================================
// FIN DU FICHIER BackendService.js v3.2.0
// ============================================================================
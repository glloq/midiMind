// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Version: v3.1.1 - CORRIGÉ COMPLET (100% conformité documentation)
// Date: 2025-10-13
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// CORRECTIONS v3.1.1:
// ✅ Ajout uploadFile(filename, base64Data) manquant
// ✅ Ajout getFile(fileId) manquant
// ✅ Conservation de toutes les fonctionnalités existantes (pas de downgrade)
// ✅ 100% de conformité avec la documentation v3.1.0
// ✅ Protocole unifié avec enveloppes (Protocol.h)
// ✅ Support requestId pour suivi des requêtes
// ✅ Gestion des réponses, événements et erreurs
// ✅ Compatibilité legacy pour transition en douceur
// ✅ Timeout et retry améliorés
// ✅ Queue de messages si déconnecté
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
            protocolVersion: '3.0'
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
            reconnections: 0
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
        
        return new Promise((resolve, reject) => {
            try {
                this.logger.info('BackendService', `🔌 Connecting to ${this.config.url}...`);
                
                this.ws = new WebSocket(this.config.url);
                
                this.ws.onopen = () => {
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
                    this.onClose(event);
                };
                
                this.ws.onerror = (error) => {
                    this.onError(error);
                    
                    if (!this.connected) {
                        reject(error);
                    }
                };
                
            } catch (error) {
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
        this.logger.warn('BackendService', 'Connection closed', event.code);
        
        this.connected = false;
        
        // Arrêter heartbeat
        this.stopHeartbeat();
        
        // Émettre événement
        this.eventBus.emit('websocket:disconnected');
        
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
        
        // Supprimer
        this.pendingRequests.delete(requestId);
    }
    
    /**
     * Gère un événement
     * @param {Object} message - Message événement
     */
    handleEvent(message) {
        this.stats.eventsReceived++;
        
        const { event } = message;
        
        if (!event || !event.name) {
            this.logger.warn('BackendService', 'Invalid event format');
            return;
        }
        
        // Émettre sur l'EventBus avec préfixe backend:
        this.eventBus.emit(`backend:${event.name}`, event.data || {});
        
        // Émettre aussi un événement générique
        this.eventBus.emit('backend:event', event);
    }
    
    /**
     * Gère un message d'erreur
     * @param {Object} message - Message erreur
     */
    handleErrorMessage(message) {
        this.stats.errorsReceived++;
        
        const { envelope, error } = message;
        
        this.logger.error('BackendService', `Backend error: ${error.message}`);
        
        // Si c'est une erreur liée à une requête
        if (envelope.id) {
            const pending = this.pendingRequests.get(envelope.id);
            if (pending) {
                if (pending.timeout) {
                    clearTimeout(pending.timeout);
                }
                pending.reject(new Error(error.message));
                this.pendingRequests.delete(envelope.id);
            }
        }
        
        // Émettre événement
        this.eventBus.emit('backend:error', error);
    }
    
    // ========================================================================
    // ENVOI DE COMMANDES (NOUVEAU FORMAT)
    // ========================================================================
    
    /**
     * Envoie une commande au backend
     * @param {string} command - Nom de la commande
     * @param {Object} params - Paramètres
     * @param {Object} options - Options (timeout, priority)
     * @returns {Promise<Object>} - Réponse
     */
    sendCommand(command, params = {}, options = {}) {
        return new Promise((resolve, reject) => {
            // Générer ID unique
            const requestId = this.generateRequestId();
            
            // Construire le message avec enveloppe
            const message = {
                envelope: {
                    version: this.config.protocolVersion,
                    id: requestId,
                    type: 'request',
                    timestamp: Date.now()
                },
                request: {
                    command: command,
                    params: params
                }
            };
            
            // Timeout
            const timeout = options.timeout || this.config.requestTimeout;
            const timeoutHandle = setTimeout(() => {
                this.pendingRequests.delete(requestId);
                reject(new Error(`Request timeout: ${command}`));
            }, timeout);
            
            // Enregistrer la requête
            this.pendingRequests.set(requestId, {
                command,
                resolve,
                reject,
                timeout: timeoutHandle,
                timestamp: Date.now()
            });
            
            // Envoyer
            this.sendMessage(message);
            
            this.stats.requestsSent++;
            this.logger.debug('BackendService', `→ Command: ${command}`, params);
        });
    }
    
    /**
     * Envoie un message brut
     * @param {Object} message - Message à envoyer
     */
    sendMessage(message) {
        if (!this.isConnected()) {
            // Mettre en queue si déconnecté
            this.messageQueue.push(message);
            this.logger.warn('BackendService', 'Not connected, message queued');
            return;
        }
        
        try {
            this.ws.send(JSON.stringify(message));
            this.stats.messagesSent++;
        } catch (error) {
            this.logger.error('BackendService', 'Failed to send message:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // RACCOURCIS POUR COMMANDES FRÉQUENTES
    // ========================================================================
    
    // --- Playback ---
    
    /**
     * Démarre la lecture
     * @returns {Promise<Object>}
     */
    async play() {
        return this.sendCommand('playback.play');
    }
    
    /**
     * Met en pause
     * @returns {Promise<Object>}
     */
    async pause() {
        return this.sendCommand('playback.pause');
    }
    
    /**
     * Arrête la lecture
     * @returns {Promise<Object>}
     */
    async stop() {
        return this.sendCommand('playback.stop');
    }
    
    /**
     * Seek à une position
     * @param {number} position - Position en ms
     * @returns {Promise<Object>}
     */
    async seek(position) {
        return this.sendCommand('playback.seek', { position });
    }
    
    /**
     * Charge un fichier
     * @param {string} filePath - Chemin du fichier
     * @returns {Promise<Object>}
     */
    async loadFile(filePath) {
        return this.sendCommand('playback.load', { file_path: filePath });
    }
    
    /**
     * Obtient le statut de lecture
     * @returns {Promise<Object>}
     */
    async getPlaybackStatus() {
        return this.sendCommand('playback.status');
    }
    
    /**
     * Définit le tempo
     * @param {number} tempo - Tempo en BPM
     * @returns {Promise<Object>}
     */
    async setTempo(tempo) {
        return this.sendCommand('playback.setTempo', { tempo });
    }
    
    /**
     * Active/désactive la boucle
     * @param {boolean} enabled - Activer la boucle
     * @param {number} start - Point de départ (optionnel)
     * @param {number} end - Point de fin (optionnel)
     * @returns {Promise<Object>}
     */
    async setLoop(enabled, start = null, end = null) {
        return this.sendCommand('playback.setLoop', { enabled, start, end });
    }
    
    // --- Files ---
    
    /**
     * Liste les fichiers
     * @param {string} directory - Répertoire (optionnel)
     * @returns {Promise<Object>}
     */
    async listFiles(directory = null) {
        return this.sendCommand('files.list', { directory });
    }
    
    /**
     * Scanne les fichiers
     * @returns {Promise<Object>}
     */
    async scanFiles() {
        return this.sendCommand('files.scan');
    }
    
    /**
     * Supprime un fichier
     * @param {string} filePath - Chemin du fichier
     * @returns {Promise<Object>}
     */
    async deleteFile(filePath) {
        return this.sendCommand('files.delete', { file_path: filePath });
    }
    
    /**
     * Obtient les métadonnées d'un fichier
     * @param {string} filePath - Chemin du fichier
     * @returns {Promise<Object>}
     */
    async getFileMetadata(filePath) {
        return this.sendCommand('files.getMetadata', { file_path: filePath });
    }
    
    /**
     * ✅ NOUVEAU v3.1.1: Obtient un fichier par son ID
     * @param {string} fileId - ID du fichier
     * @returns {Promise<Object>}
     */
    async getFile(fileId) {
        return this.sendCommand('files.get', { file_id: fileId });
    }
    
    /**
     * ✅ NOUVEAU v3.1.1: Upload un fichier en base64
     * @param {string} filename - Nom du fichier
     * @param {string} base64Data - Données en base64
     * @returns {Promise<Object>}
     */
    async uploadFile(filename, base64Data) {
        return this.sendCommand('files.upload', { 
            filename: filename,
            data: base64Data 
        });
    }
    
    // --- Devices ---
    
    /**
     * Scanne les périphériques
     * @returns {Promise<Object>}
     */
    async scanDevices() {
        return this.sendCommand('devices.scan');
    }
    
    /**
     * Liste les périphériques
     * @returns {Promise<Object>}
     */
    async listDevices() {
        return this.sendCommand('devices.list');
    }
    
    /**
     * Connecte un périphérique
     * @param {string} deviceId - ID du périphérique
     * @returns {Promise<Object>}
     */
    async connectDevice(deviceId) {
        return this.sendCommand('devices.connect', { device_id: deviceId });
    }
    
    /**
     * Déconnecte un périphérique
     * @param {string} deviceId - ID du périphérique
     * @returns {Promise<Object>}
     */
    async disconnectDevice(deviceId) {
        return this.sendCommand('devices.disconnect', { device_id: deviceId });
    }
    
    // --- Routing ---
    
    /**
     * Ajoute une route
     * @param {Object} route - Configuration de la route
     * @returns {Promise<Object>}
     */
    async addRoute(route) {
        return this.sendCommand('routing.addRoute', route);
    }
    
    /**
     * Supprime une route
     * @param {string} routeId - ID de la route
     * @returns {Promise<Object>}
     */
    async removeRoute(routeId) {
        return this.sendCommand('routing.removeRoute', { route_id: routeId });
    }
    
    /**
     * Liste les routes
     * @returns {Promise<Object>}
     */
    async listRoutes() {
        return this.sendCommand('routing.listRoutes');
    }
    
    /**
     * Met à jour une route
     * @param {string} routeId - ID de la route
     * @param {Object} changes - Modifications
     * @returns {Promise<Object>}
     */
    async updateRoute(routeId, changes) {
        return this.sendCommand('routing.updateRoute', { route_id: routeId, ...changes });
    }
    
    // --- Editor ---
    
    /**
     * Charge un fichier dans l'éditeur
     * @param {string} filePath - Chemin du fichier
     * @returns {Promise<Object>}
     */
    async editorLoad(filePath) {
        return this.sendCommand('editor.load', { file_path: filePath });
    }
    
    /**
     * Sauvegarde un fichier depuis l'éditeur
     * @param {string} filePath - Chemin du fichier
     * @param {Object} jsonMidi - Données JsonMidi
     * @returns {Promise<Object>}
     */
    async editorSave(filePath, jsonMidi) {
        return this.sendCommand('editor.save', { file_path: filePath, jsonmidi: jsonMidi });
    }
    
    /**
     * Ajoute une note
     * @param {Object} note - Note à ajouter
     * @returns {Promise<Object>}
     */
    async editorAddNote(note) {
        return this.sendCommand('editor.addNote', note);
    }
    
    /**
     * Supprime une note
     * @param {string} noteId - ID de la note
     * @returns {Promise<Object>}
     */
    async editorDeleteNote(noteId) {
        return this.sendCommand('editor.deleteNote', { note_id: noteId });
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
        
        this.reconnecting = true;
        
        const delay = Math.min(
            this.config.reconnectDelay * Math.pow(
                this.config.reconnectBackoff,
                this.reconnectAttempts
            ),
            this.config.maxReconnectDelay
        );
        
        this.logger.info('BackendService', `Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts + 1})`);
        
        this.reconnectTimer = setTimeout(async () => {
            this.reconnectTimer = null;
            this.reconnectAttempts++;
            this.stats.reconnections++;
            
            try {
                await this.connect();
            } catch (error) {
                this.logger.error('BackendService', 'Reconnection failed:', error);
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
            queuedMessages: this.messageQueue.length
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
            reconnections: 0
        };
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackendService;
}
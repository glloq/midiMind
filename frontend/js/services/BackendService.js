// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Version: v3.8.0 - FORMAT API SIMPLIFIÉ CONFORME DOC
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.8.0:
// ✅ FORMAT SIMPLIFIÉ selon API_DOCUMENTATION_FRONTEND.md
// ✅ Requête: { id, command, ...params } (paramètres aplatis)
// ✅ Réponse: { id, status, data, message } (format simple)
// ✅ Support événements: { event, device_id, data }
// ✅ IDs numériques simples (incrémentaux)
// ✅ Compatibilité totale avec documentation officielle
//
// CONSERVÉ DE v3.7.0:
// ✅ Heartbeat avec system.status ou list_devices
// ✅ Timeout configurable par commande
// ✅ Gestion reconnexion robuste
// ✅ Watchdog activité backend
// ✅ Logs détaillés debugging
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
        
        // Configuration
        this.config = {
            url: url || 'ws://localhost:8080',
            reconnectInterval: 3000,
            maxReconnectInterval: 30000,
            reconnectDecay: 1.5,
            timeoutInterval: 5000,
            heartbeatInterval: 20000,      // Vérifier toutes les 20s
            heartbeatTimeout: 45000,       // Considérer mort si pas de réponse depuis 45s
            maxReconnectAttempts: 5,
            defaultCommandTimeout: 5000    // Timeout par défaut pour les commandes
        };
        
        // Gestion de la reconnexion
        this.reconnectAttempts = 0;
        this.reconnectTimer = null;
        this.heartbeatTimer = null;
        this.connectionTimeout = null;
        
        // Suivi de l'activité backend
        this.lastActivityTime = Date.now();  // Toute réponse = activité
        this.heartbeatPending = false;       // Éviter ping en double
        this.heartbeatFailures = 0;          // Compteur d'échecs consécutifs
        
        // File d'attente des messages
        this.messageQueue = [];
        this.maxQueueSize = 100;
        
        // Compteur de requêtes pour les IDs (simple, numérique)
        this.requestId = 0;
        this.pendingRequests = new Map();
        
        this.logger.info('BackendService', 'Service initialized (v3.8.0 - Format API Simplifié)');
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
                `✌ Max reconnection attempts (${this.config.maxReconnectAttempts}) reached`);
            
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
            `⏱ Reconnection attempt ${this.reconnectAttempts}/${this.config.maxReconnectAttempts} in ${Math.round(delay/1000)}s`);
        
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
     * Démarre le heartbeat
     */
    startHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
        }
        
        this.heartbeatTimer = setInterval(() => {
            this.checkHeartbeat();
        }, this.config.heartbeatInterval);
        
        this.logger.debug('BackendService', '💓 Heartbeat started');
    }
    
    /**
     * Vérifie le heartbeat
     */
    async checkHeartbeat() {
        const timeSinceActivity = Date.now() - this.lastActivityTime;
        
        // Si pas d'activité depuis heartbeatTimeout, considérer connexion morte
        if (timeSinceActivity > this.config.heartbeatTimeout) {
            this.heartbeatFailures++;
            
            this.logger.warn('BackendService', 
                `✌ Heartbeat timeout! No activity since ${Math.round(timeSinceActivity/1000)}s (failure #${this.heartbeatFailures})`);
            
            if (this.heartbeatFailures >= 2) {
                // Après 2 échecs consécutifs, forcer reconnexion
                this.logger.error('BackendService', '💀 Connection dead, forcing reconnect');
                this.forceReconnect('Heartbeat timeout - connection dead');
                return;
            }
        }
        
        // Si déjà un heartbeat en cours, attendre
        if (this.heartbeatPending) {
            this.logger.debug('BackendService', 'Heartbeat pending, skipping');
            return;
        }
        
        // Envoyer un list_devices comme heartbeat (commande simple et rapide)
        try {
            this.heartbeatPending = true;
            this.logger.debug('BackendService', '💓 Sending heartbeat (list_devices)');
            
            const startTime = Date.now();
            await this.sendCommand('list_devices');
            const latency = Date.now() - startTime;
            
            this.heartbeatPending = false;
            this.heartbeatFailures = 0;  // Reset sur succès
            
            this.logger.debug('BackendService', `✓ Heartbeat OK (latency: ${latency}ms)`);
            
        } catch (error) {
            this.heartbeatPending = false;
            this.heartbeatFailures++;
            
            this.logger.warn('BackendService', 
                `⚠️ Heartbeat failed: ${error.message} (failure #${this.heartbeatFailures})`);
            
            if (this.heartbeatFailures >= 3) {
                this.logger.error('BackendService', '💀 Multiple heartbeat failures, forcing reconnect');
                this.forceReconnect('Multiple heartbeat failures');
            }
        }
    }
    
    /**
     * Arrête le heartbeat
     */
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
            this.heartbeatPending = false;
        }
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
     * ✅ Gère les messages reçus (FORMAT API SIMPLIFIÉ)
     */
    handleMessage(event) {
        try {
            const data = JSON.parse(event.data);
            
            // ✅ Toute réponse/event = activité backend
            this.lastActivityTime = Date.now();
            
            // ✅ FORMAT SIMPLE: Si message a un 'id', c'est une réponse à une commande
            if (data.id !== undefined && this.pendingRequests.has(data.id)) {
                const pending = this.pendingRequests.get(data.id);
                this.pendingRequests.delete(data.id);
                
                if (data.status === 'success') {
                    // ✅ Résoudre avec data (ou réponse entière si pas de data)
                    pending.resolve(data.data || data);
                } else {
                    // ✅ Rejeter avec message d'erreur
                    const error = new Error(data.message || 'Command failed');
                    error.code = data.code;
                    pending.reject(error);
                }
                return;
            }
            
            // ✅ Si message a un 'event', c'est un événement backend
            if (data.event) {
                // Émettre événement spécifique avec préfixe
                this.eventBus.emit(`backend:event:${data.event}`, data);
                
                // Si device_id présent, émettre aussi événement par device
                if (data.device_id !== undefined) {
                    this.eventBus.emit(`${data.event}:${data.device_id}`, data);
                }
                
                this.logger.debug('BackendService', `Event received: ${data.event}`, data);
                return;
            }
            
            // Émettre événement générique pour autres messages
            this.eventBus.emit('backend:message', data);
            
            this.logger.debug('BackendService', 'Message received:', data);
            
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
     * ✅ Envoie une commande au backend et attend la réponse (FORMAT API SIMPLIFIÉ)
     */
    async sendCommand(command, params = {}, timeout = null) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected()) {
                reject(new Error('Not connected to backend'));
                return;
            }
            
            // ✅ Timeout configurable (défaut: 5000ms)
            const timeoutMs = timeout || this.config.defaultCommandTimeout || 5000;
            
            // ✅ ID simple et numérique (incrémental)
            const requestId = ++this.requestId;
            
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
            
            // ✅ FORMAT API SIMPLIFIÉ selon documentation
            // Requête: { id, command, ...params }
            // Les paramètres sont aplatis au premier niveau
            const message = {
                id: requestId,
                command: command,
                ...params  // ✅ Aplatir les paramètres (pas de sous-objet "params")
            };
            
            this.send(message);
            
            this.logger.debug('BackendService', `Sent command: ${command} (id: ${requestId})`, message);
        });
    }
    
    /**
     * Upload un fichier au backend
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
                        file_name: file.name,
                        file_data: base64Data,
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
    // MÉTHODES DE ROUTING (Helper methods)
    // ========================================================================
    
    async listDevices() {
        return this.sendCommand('list_devices');
    }
    
    async connectDevice(deviceId) {
        return this.sendCommand('connect_device', { device_id: deviceId });
    }
    
    async disconnectDevice(deviceId) {
        return this.sendCommand('disconnect_device', { device_id: deviceId });
    }
    
    async listFiles() {
        return this.sendCommand('list_files');
    }
    
    async loadFile(filePath) {
        return this.sendCommand('load_file', { file_path: filePath });
    }
    
    async play(filePath) {
        return this.sendCommand('play', { file_path: filePath });
    }
    
    async pause() {
        return this.sendCommand('pause');
    }
    
    async stop() {
        return this.sendCommand('stop');
    }
    
    async seek(position) {
        return this.sendCommand('seek', { position: position });
    }
    
    async createRoute(sourceId, destId, channel = null) {
        const params = {
            source_id: sourceId,
            destination_id: destId
        };
        if (channel !== null) {
            params.channel = channel;
        }
        return this.sendCommand('create_route', params);
    }
    
    async deleteRoute(routeId) {
        return this.sendCommand('delete_route', { route_id: routeId });
    }
    
    async listRoutes() {
        return this.sendCommand('list_routes');
    }
    
    async sendMidi(deviceId, data) {
        return this.sendCommand('send_midi', { 
            device_id: deviceId,
            data: data
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
}

// Export pour utilisation
if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackendService;
}
window.BackendService = BackendService;
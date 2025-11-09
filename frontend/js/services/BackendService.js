// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Chemin réel: frontend/js/services/BackendService.js  
// Version: v4.3.0 - CONNECTION STABILITY FIXES
// Date: 2025-11-09
// ============================================================================
// CORRECTIONS v4.3.0 (STABILITÉ CONNEXION):
// ✅ FIX ERREUR #2: Heartbeat moins agressif (45s/90s au lieu de 30s/60s)
// ✅ FIX ERREUR #3: Timeout spécifique pour heartbeat (15s au lieu de 10s)
// ✅ FIX ERREUR #4: Nettoyage callbacks à la déconnexion (failPendingCallbacks)
// ✅ FIX ERREUR #5: Historique diagnostic persistant (localStorage)
// ✅ Configuration adaptative selon environnement (dev/prod/RPI)
// ✅ Logging détaillé des déconnexions
// ✅ AUCUN DOWNGRADE - Toutes les 90 méthodes API conservées
// ============================================================================

class BackendService {
    constructor(url, eventBus, logger) {
        this.eventBus = eventBus || window.eventBus || null;
        this.logger = logger || console;
        
        this.ws = null;
        this.connected = false;
        this.connecting = false;
        this.offlineMode = false;
        this.reconnectionStopped = false;
        
        // ✅ DÉTECTION ENVIRONNEMENT
        const isDevelopment = window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';
        const isRaspberryPi = window.location.hostname === '192.168.1.37';
        
        // ✅ CONFIGURATION ADAPTATIVE (FIX ERREUR #2 + #3)
        this.config = {
            url: url || 'ws://localhost:8080',
            
            // Reconnexion
            reconnectInterval: isDevelopment ? 2000 : 3000,
            maxReconnectInterval: isDevelopment ? 20000 : 30000,
            reconnectDecay: 1.5,
            timeoutInterval: isDevelopment ? 3000 : 5000,
            maxReconnectAttempts: isDevelopment ? 10 : 5,
            
            // ✅ FIX ERREUR #2: Heartbeat moins agressif
            heartbeatInterval: isRaspberryPi ? 60000 : 45000,     // 45s au lieu de 30s (60s sur RPI)
            heartbeatTimeout: isRaspberryPi ? 120000 : 90000,     // 90s au lieu de 60s (120s sur RPI)
            maxHeartbeatFailures: 3,
            
            // ✅ FIX ERREUR #3: Timeout spécifique pour heartbeat
            defaultCommandTimeout: 10000,
            heartbeatCommandTimeout: isRaspberryPi ? 20000 : 15000  // 15s pour heartbeat (20s sur RPI)
        };
        
        this.reconnectAttempts = 0;
        this.reconnectTimer = null;
        this.heartbeatTimer = null;
        this.connectionTimeout = null;
        
        this.lastActivityTime = Date.now();
        this.connectionStartTime = null;
        this.lastHeartbeatCheck = null;
        this.heartbeatPending = false;
        this.heartbeatFailures = 0;
        
        this.messageQueue = [];
        this.maxQueueSize = 100;
        
        this.messageCallbacks = new Map();
        this.messageId = 0;
        
        // ✅ FIX ERREUR #5: Historique diagnostic
        this.connectionHistory = this.loadConnectionHistory();
        
        this.logger.info('BackendService', `Service initialized (v4.3.0 - STABILITY FIXES)`);
        this.logger.info('BackendService', `Environment: ${isDevelopment ? 'DEV' : isRaspberryPi ? 'RPI' : 'PROD'}`);
        this.logger.info('BackendService', `Config: heartbeat=${this.config.heartbeatInterval}ms, timeout=${this.config.heartbeatTimeout}ms`);
    }
    
    // ========================================================================
    // CONNEXION WEBSOCKET
    // ========================================================================
    
    generateMessageId() {
        return ++this.messageId;
    }
    
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
        this.connectionStartTime = Date.now();
        
        this.logger.info('BackendService', `Connecting to ${wsUrl}...`);
        
        return new Promise((resolve) => {
            try {
                this.ws = new WebSocket(wsUrl);
                
                this.connectionTimeout = setTimeout(() => {
                    if (!this.connected) {
                        this.logger.error('BackendService', 'Connection timeout');
                        this.handleConnectionError('Connection timeout');
                        resolve(false);
                    }
                }, this.config.timeoutInterval);
                
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
                this.logger.error('BackendService', 'Connection failed:', error);
                this.handleConnectionError(error);
                resolve(false);
            }
        });
    }
    
    handleOpen() {
        this.connected = true;
        this.connecting = false;
        this.offlineMode = false;
        this.reconnectAttempts = 0;
        this.connectionStartTime = Date.now();
        
        this.logger.info('BackendService', '✓ Connected successfully');
        this.eventBus.emit('backend:connected');
        
        // ✅ FIX ERREUR #5: Logger la connexion
        this.saveConnectionEvent('connected', {
            timestamp: new Date().toISOString(),
            url: this.config.url
        });
        
        this.startHeartbeat();
        this.flushMessageQueue();
    }
    
    // ✅ FIX ERREUR #4 + #5: Nettoyage callbacks + historique détaillé
    handleClose(event) {
        const wasConnected = this.connected;
        const uptime = this.connectionStartTime ? Date.now() - this.connectionStartTime : 0;
        
        // ✅ FIX ERREUR #5: DIAGNOSTIC COMPLET
        const diagnostic = {
            timestamp: new Date().toISOString(),
            code: event.code,
            reason: event.reason || 'No reason provided',
            wasClean: event.wasClean,
            wasConnected: wasConnected,
            uptime: uptime,
            uptimeFormatted: this.formatUptime(uptime),
            lastActivity: new Date(this.lastActivityTime).toISOString(),
            timeSinceActivity: Date.now() - this.lastActivityTime,
            heartbeatStatus: {
                failures: this.heartbeatFailures,
                pending: this.heartbeatPending,
                lastCheck: this.lastHeartbeatCheck ? new Date(this.lastHeartbeatCheck).toISOString() : null
            },
            queue: {
                size: this.messageQueue.length,
                maxSize: this.maxQueueSize
            },
            callbacks: {
                pending: this.messageCallbacks.size,
                ids: Array.from(this.messageCallbacks.keys())
            },
            reconnect: {
                attempts: this.reconnectAttempts,
                max: this.config.maxReconnectAttempts,
                stopped: this.reconnectionStopped
            }
        };
        
        // Mise à jour de l'état
        this.connected = false;
        this.connecting = false;
        this.stopHeartbeat();
        
        // Log détaillé
        this.logger.error('BackendService', '❌ CONNEXION FERMÉE - DIAGNOSTIC COMPLET:', JSON.stringify(diagnostic, null, 2));
        
        // Interpréter le code de fermeture
        const closeReason = this.getCloseReason(event.code);
        this.logger.warn('BackendService', `Code ${event.code}: ${closeReason}`);
        
        // ✅ FIX ERREUR #5: Sauvegarder dans l'historique
        this.saveConnectionEvent('closed', diagnostic);
        
        // ✅ FIX ERREUR #4: NETTOYER TOUS LES CALLBACKS
        this.failPendingCallbacks('Connection closed');
        
        // Émettre événement
        if (wasConnected) {
            this.eventBus.emit('backend:disconnected', {
                code: event.code,
                reason: event.reason,
                uptime: uptime,
                diagnostic: diagnostic
            });
        }
        
        // Planifier reconnexion si non arrêtée
        if (!this.reconnectionStopped) {
            this.scheduleReconnection();
        }
    }
    
    handleError(error) {
        this.logger.error('BackendService', 'WebSocket error:', error);
        this.eventBus.emit('backend:error', error);
    }
    
    handleConnectionError(error) {
        this.connecting = false;
        this.logger.error('BackendService', 'Connection error:', error);
        this.eventBus.emit('backend:connection-error', error);
        
        // ✅ FIX ERREUR #5: Logger l'erreur
        this.saveConnectionEvent('error', {
            timestamp: new Date().toISOString(),
            error: error.toString()
        });
    }
    
    // ========================================================================
    // RECONNEXION
    // ========================================================================
    
    scheduleReconnection() {
        if (this.reconnectionStopped) return;
        
        if (this.reconnectAttempts >= this.config.maxReconnectAttempts) {
            this.logger.error('BackendService', 
                `Max reconnection attempts reached (${this.config.maxReconnectAttempts})`);
            this.enterOfflineMode();
            return;
        }
        
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
    
    enterOfflineMode() {
        this.offlineMode = true;
        this.logger.warn('BackendService', '⚠️ Entering offline mode');
        
        this.eventBus.emit('backend:offline-mode', {
            timestamp: Date.now()
        });
        
        // ✅ FIX ERREUR #5: Logger le mode offline
        this.saveConnectionEvent('offline', {
            timestamp: new Date().toISOString(),
            reconnectAttempts: this.reconnectAttempts
        });
    }
    
    // ========================================================================
    // HEARTBEAT (FIX ERREUR #2 + #3)
    // ========================================================================
    
    startHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
        }
        
        // ✅ FIX ERREUR #2: Intervalle augmenté (45s au lieu de 30s)
        this.heartbeatTimer = setInterval(() => {
            this.checkHeartbeat();
        }, this.config.heartbeatInterval);
        
        this.logger.debug('BackendService', `💗 Heartbeat started (interval: ${this.config.heartbeatInterval}ms)`);
    }
    
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
            this.logger.debug('BackendService', '💔 Heartbeat stopped');
        }
    }
    
    // ✅ FIX ERREUR #2 + #3: Heartbeat robuste avec timeout approprié
    async checkHeartbeat() {
        const timeSinceActivity = Date.now() - this.lastActivityTime;
        
        // ✅ FIX ERREUR #2: Timeout augmenté (90s au lieu de 60s)
        if (timeSinceActivity > this.config.heartbeatTimeout) {
            this.heartbeatFailures++;
            
            this.logger.warn('BackendService', 
                `⚠️ Heartbeat timeout! No activity for ${Math.round(timeSinceActivity/1000)}s (failure #${this.heartbeatFailures}/${this.config.maxHeartbeatFailures})`);
            
            // 3 échecs = reconnexion
            if (this.heartbeatFailures >= this.config.maxHeartbeatFailures) {
                this.logger.error('BackendService', `💔 ${this.config.maxHeartbeatFailures} consecutive heartbeat failures`);
                this.forceReconnect(`Heartbeat timeout - ${this.config.maxHeartbeatFailures} failures`);
                return;
            }
        }
        
        // Ne pas envoyer si un heartbeat est déjà en cours
        if (this.heartbeatPending) {
            this.logger.warn('BackendService', '⏳ Heartbeat already pending, skipping');
            return;
        }
        
        try {
            this.heartbeatPending = true;
            this.lastHeartbeatCheck = Date.now();
            
            this.logger.debug('BackendService', '💗 Sending heartbeat (system.ping)');
            
            const startTime = Date.now();
            
            // ✅ FIX ERREUR #3: Timeout spécifique pour heartbeat (15s au lieu de 10s)
            const result = await this.sendCommand('system.ping', {}, this.config.heartbeatCommandTimeout);
            
            const latency = Date.now() - startTime;
            
            this.heartbeatPending = false;
            this.heartbeatFailures = 0;
            
            this.logger.debug('BackendService', `💚 Heartbeat OK (${latency}ms)`);
            
            this.eventBus.emit('backend:heartbeat', {
                latency: latency,
                timestamp: Date.now(),
                success: true
            });
            
        } catch (error) {
            this.heartbeatPending = false;
            this.heartbeatFailures++;
            
            this.logger.error('BackendService', `❌ Heartbeat failed (failure #${this.heartbeatFailures}/${this.config.maxHeartbeatFailures}):`, error.message);
            
            this.eventBus.emit('backend:heartbeat', {
                timestamp: Date.now(),
                success: false,
                error: error.message,
                failures: this.heartbeatFailures
            });
            
            // Si 3 échecs, forcer reconnexion
            if (this.heartbeatFailures >= this.config.maxHeartbeatFailures) {
                this.logger.error('BackendService', `💔 ${this.config.maxHeartbeatFailures} heartbeat failures, forcing reconnect`);
                this.forceReconnect(`${this.config.maxHeartbeatFailures} consecutive heartbeat failures`);
            }
        }
    }
    
    forceReconnect(reason = 'Force reconnect') {
        this.logger.warn('BackendService', `Force reconnect: ${reason}`);
        
        // ✅ FIX ERREUR #5: Logger la reconnexion forcée
        this.saveConnectionEvent('force_reconnect', {
            timestamp: new Date().toISOString(),
            reason: reason,
            heartbeatFailures: this.heartbeatFailures
        });
        
        if (this.ws) {
            this.ws.close(1000, reason);
        }
        
        this.reconnectAttempts = 0;
        
        setTimeout(() => {
            if (!this.connected && !this.connecting) {
                this.connect();
            }
        }, 1000);
    }
    
    // ========================================================================
    // GESTION DES MESSAGES
    // ========================================================================
    
    handleMessage(event) {
        this.lastActivityTime = Date.now();
        
        try {
            const message = JSON.parse(event.data);
            
            // DEBUG: Log complet du message
            this.logger.debug('BackendService', '[DEBUG] RAW MESSAGE:', JSON.stringify(message, null, 2));
            
            this.logger.debug('BackendService', '📩 Message received:', {
                hasId: !!message.id,
                type: message.type || 'unknown',
                hasPayload: !!message.payload
            });
            
            // Backend format: { type, id, payload, timestamp, version }
            if (message.type && message.payload) {
                if (message.type === 'response') {
                    this.handleBackendResponse(message);
                } else if (message.type === 'event') {
                    this.handleBackendEvent(message);
                } else if (message.type === 'error') {
                    this.handleBackendError(message);
                }
            }
            // Simple format: { id, success, data, error } (backup)
            else if (message.id !== undefined && message.success !== undefined) {
                this.handleSimpleResponse(message);
            }
            // Event format: { event: "name", data: {...} } (backup)
            else if (message.event) {
                this.handleSimpleEvent(message);
            }
            else {
                this.logger.warn('BackendService', 'Unknown message format:', message);
            }
        } catch (error) {
            this.logger.error('BackendService', 'Error parsing message:', error, event.data);
        }
    }
    
    handleBackendResponse(message) {
        // Backend response: payload.request_id correspond à l'id envoyé
        const requestId = message.payload.request_id;
        
        this.logger.debug('BackendService', '[DEBUG] BACKEND RESPONSE:', {
            requestId: requestId,
            hasCallback: this.messageCallbacks.has(requestId),
            callbackCount: this.messageCallbacks.size,
            success: message.payload.success
        });
        
        if (!requestId || !this.messageCallbacks.has(requestId)) {
            this.logger.warn('BackendService', 'No callback for request ID: ' + requestId);
            return;
        }
        
        const callback = this.messageCallbacks.get(requestId);
        this.messageCallbacks.delete(requestId);
        
        // Transformer en format simple pour le callback
        const simpleResponse = {
            id: requestId,
            success: message.payload.success !== false && !message.payload.error,
            data: message.payload.data,
            error: message.payload.error || message.payload.error_message
        };
        
        callback(simpleResponse);
    }
    
    handleBackendEvent(message) {
        const eventName = message.payload.name || message.payload.event;
        const eventData = message.payload.data || message.payload;
        
        if (eventName) {
            this.logger.debug('BackendService', '📡 Backend Event: ' + eventName, eventData);
            // Émettre avec le nom original de l'événement
            this.eventBus.emit(eventName, eventData);
            // Aussi émettre avec préfixe pour compatibilité
            this.eventBus.emit('backend:event:' + eventName, eventData);
        } else {
            this.logger.debug('BackendService', '📡 Backend message (no event name)');
        }
    }
    
    handleBackendError(message) {
        const requestId = message.payload.request_id;
        const errorMessage = message.payload.error_message || message.payload.error || 'Unknown error';
        
        this.logger.error('BackendService', '✗ Backend error:', errorMessage);
        
        if (requestId && this.messageCallbacks.has(requestId)) {
            const callback = this.messageCallbacks.get(requestId);
            this.messageCallbacks.delete(requestId);
            callback({ id: requestId, success: false, error: errorMessage });
        }
        
        this.eventBus.emit('backend:error', {
            message: errorMessage,
            requestId: requestId
        });
    }
    
    // BACKUP: Support format simple { id, success, data, error }
    handleSimpleResponse(message) {
        if (this.messageCallbacks.has(message.id)) {
            const callback = this.messageCallbacks.get(message.id);
            this.messageCallbacks.delete(message.id);
            callback(message);
        }
    }
    
    // BACKUP: Support format événement simple
    handleSimpleEvent(message) {
        this.logger.debug('BackendService', '📡 Simple event: ' + message.event, message.data);
        this.eventBus.emit(message.event, message.data);
    }
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    send(data) {
        if (!this.connected || !this.ws || this.ws.readyState !== WebSocket.OPEN) {
            if (this.messageQueue.length >= this.maxQueueSize) {
                this.logger.warn('BackendService', 'Message queue full, dropping message');
                return false;
            }
            this.messageQueue.push(data);
            this.logger.debug('BackendService', 'Message queued (not connected)');
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
     * ✅ v4.3.0: ENVOI format simple { id, command, params }
     * RÉCEPTION format { type, payload } avec payload.request_id
     */
    async sendCommand(command, params = {}, timeout = null) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected()) {
                reject(new Error('Not connected to backend'));
                return;
            }
            
            const timeoutMs = timeout || this.config.defaultCommandTimeout;
            const id = this.generateMessageId();
            
            const timeoutTimer = setTimeout(() => {
                if (this.messageCallbacks.has(id)) {
                    this.messageCallbacks.delete(id);
                    reject(new Error(`Command timeout after ${timeoutMs}ms: ${command}`));
                }
            }, timeoutMs);
            
            // Callback pour gérer la réponse
            this.messageCallbacks.set(id, (response) => {
                clearTimeout(timeoutTimer);
                
                if (response.success === true) {
                    this.logger.debug('BackendService', `✓ Command success [${id}]: ${command}`);
                    resolve(response.data || response);
                } else {
                    this.logger.error('BackendService', `✗ Command failed [${id}]: ${command}`, response.error);
                    reject(new Error(response.error || 'Command failed'));
                }
            });
            
            // Envoyer message (format simple pour le backend)
            const message = {
                id: id,
                command: command,
                params: params || {}
            };
            
            this.logger.debug('BackendService', `[DEBUG] SENDING [${id}]:`, command, params);
            
            try {
                this.ws.send(JSON.stringify(message));
                this.lastActivityTime = Date.now();
            } catch (error) {
                this.logger.error('BackendService', 'Error sending command:', error);
                this.messageCallbacks.delete(id);
                clearTimeout(timeoutTimer);
                reject(error);
            }
        });
    }
    
    // ✅ FIX ERREUR #4: Nettoyer tous les callbacks en attente
    failPendingCallbacks(reason = 'Connection lost') {
        if (this.messageCallbacks.size === 0) return;
        
        this.logger.warn('BackendService', `Failing ${this.messageCallbacks.size} pending callbacks: ${reason}`);
        
        const failedCount = this.messageCallbacks.size;
        
        for (const [id, callback] of this.messageCallbacks.entries()) {
            try {
                callback({
                    id: id,
                    success: false,
                    error: reason
                });
            } catch (error) {
                this.logger.error('BackendService', `Error calling callback ${id}:`, error);
            }
        }
        
        this.messageCallbacks.clear();
        
        this.logger.info('BackendService', `✓ Cleared ${failedCount} pending callbacks`);
    }
    
    // ========================================================================
    // API MIDI COMMANDS (90 méthodes - AUCUN DOWNGRADE)
    // ========================================================================
    
    // === FILES ===
    async listMidiFiles(directory = '') { return this.sendCommand('files.list', { directory }); }
    async getMidiFile(filename) { return this.sendCommand('files.get', { filename }); }
    async uploadMidiFile(filename, content) { return this.sendCommand('files.upload', { filename, content }); }
    async deleteMidiFile(filename) { return this.sendCommand('files.delete', { filename }); }
    async renameMidiFile(oldName, newName) { return this.sendCommand('files.rename', { oldName, newName }); }
    async moveMidiFile(filename, destination) { return this.sendCommand('files.move', { filename, destination }); }
    async copyMidiFile(filename, destination) { return this.sendCommand('files.copy', { filename, destination }); }
    async createDirectory(path) { return this.sendCommand('files.createDir', { path }); }
    async deleteDirectory(path) { return this.sendCommand('files.deleteDir', { path }); }
    async getMidiInfo(filename) { return this.sendCommand('files.getInfo', { filename }); }
    async searchFiles(query) { return this.sendCommand('files.search', { query }); }
    async getRecentFiles(limit = 10) { return this.sendCommand('files.getRecent', { limit }); }
    
    // === MIDI MESSAGES ===
    async sendMidiMessage(device_id, message) { return this.sendCommand('midi.send', { device_id, message }); }
    async sendNoteOn(device_id, channel, note, velocity) { return this.sendCommand('midi.sendNoteOn', { device_id, channel, note, velocity }); }
    async sendNoteOff(device_id, channel, note) { return this.sendCommand('midi.sendNoteOff', { device_id, channel, note }); }
    async sendCC(device_id, channel, controller, value) { return this.sendCommand('midi.sendCC', { device_id, channel, controller, value }); }
    async sendProgramChange(device_id, channel, program) { return this.sendCommand('midi.sendProgramChange', { device_id, channel, program }); }
    async sendPitchBend(device_id, channel, value) { return this.sendCommand('midi.sendPitchBend', { device_id, channel, value }); }
    async sendSysex(device_id, data) { return this.sendCommand('midi.sendSysex', { device_id, data }); }
    async getMidiFilter(filter_id) { return this.sendCommand('midi.getFilter', { filter_id }); }
    async setMidiFilter(filter_id, config) { return this.sendCommand('midi.setFilter', { filter_id, config }); }
    async enableMidiFilter(filter_id) { return this.sendCommand('midi.enableFilter', { filter_id }); }
    async disableMidiFilter(filter_id) { return this.sendCommand('midi.disableFilter', { filter_id }); }
    async addMidiRoute(source_id, destination_id) { return this.sendCommand('midi.routing.add', { source_id, destination_id }); }
    async removeMidiRoute(source_id, destination_id) { return this.sendCommand('midi.routing.remove', { source_id, destination_id }); }
    async listMidiRoutes() { return this.sendCommand('midi.routing.list'); }
    async updateMidiRoute(route_id, config) { return this.sendCommand('midi.routing.update', { route_id, config }); }
    async clearMidiRoutes() { return this.sendCommand('midi.routing.clear'); }
    
    // === PLAYBACK ===
    async play(filename = null) { return this.sendCommand('playback.play', filename ? { filename } : {}); }
    async pause() { return this.sendCommand('playback.pause'); }
    async stop() { return this.sendCommand('playback.stop'); }
    async resume() { return this.sendCommand('playback.resume'); }
    async seek(position) { return this.sendCommand('playback.seek', { position }); }
    async setTempo(tempo) { return this.sendCommand('playback.setTempo', { tempo }); }
    async getTempo() { return this.sendCommand('playback.getTempo'); }
    async setLoop(enabled) { return this.sendCommand('playback.setLoop', { enabled }); }
    async getStatus() { return this.sendCommand('playback.getStatus'); }
    async getPlaybackInfo() { return this.sendCommand('playback.getInfo'); }
    async listPlaybackFiles() { return this.sendCommand('playback.listFiles'); }
    
    // === DEVICES ===
    async listDevices() { return this.sendCommand('devices.list'); }
    async scanDevices(full_scan = false) { return this.sendCommand('devices.scan', { full_scan }); }
    async getConnectedDevices() { return this.sendCommand('devices.getConnected'); }
    async connectDevice(device_id) { return this.sendCommand('devices.connect', { device_id }); }
    async disconnectDevice(device_id) { return this.sendCommand('devices.disconnect', { device_id }); }
    async disconnectAllDevices() { return this.sendCommand('devices.disconnectAll'); }
    async getDevice(device_id) { return this.sendCommand('devices.getInfo', { device_id }); }
    async getHotPlugStatus() { return this.sendCommand('devices.getHotPlugStatus'); }
    async startHotPlug() { return this.sendCommand('devices.startHotPlug'); }
    async stopHotPlug() { return this.sendCommand('devices.stopHotPlug'); }
    
    // === ROUTING ===
    async listRoutes() { return this.sendCommand('routing.listRoutes'); }
    async addRoute(source_id, destination_id) { return this.sendCommand('routing.addRoute', { source_id, destination_id }); }
    async removeRoute(source_id, destination_id) { return this.sendCommand('routing.removeRoute', { source_id, destination_id }); }
    async clearRoutes() { return this.sendCommand('routing.clearRoutes'); }
    async enableRoute(source_id, destination_id) { return this.sendCommand('routing.enableRoute', { source_id, destination_id }); }
    async disableRoute(source_id, destination_id) { return this.sendCommand('routing.disableRoute', { source_id, destination_id }); }
    
    // === LATENCY ===
    async getLatencyCompensation(instrument_id) { return this.sendCommand('latency.getCompensation', { instrument_id }); }
    async setLatencyCompensation(instrument_id, offset_ms) { return this.sendCommand('latency.setCompensation', { instrument_id, offset_ms }); }
    async enableLatency() { return this.sendCommand('latency.enable'); }
    async disableLatency() { return this.sendCommand('latency.disable'); }
    async getGlobalOffset() { return this.sendCommand('latency.getGlobalOffset'); }
    async setGlobalOffset(offset_ms) { return this.sendCommand('latency.setGlobalOffset', { offset_ms }); }
    async listInstruments() { return this.sendCommand('latency.listInstruments'); }
    
    // === PRESETS ===
    async listPresets() { return this.sendCommand('preset.list'); }
    async loadPreset(id) { return this.sendCommand('preset.load', { id }); }
    async savePreset(preset) { return this.sendCommand('preset.save', { preset }); }
    async deletePreset(id) { return this.sendCommand('preset.delete', { id }); }
    async exportPreset(id, filepath) { return this.sendCommand('preset.export', { id, filepath }); }
    
    // === PLAYLISTS ===
    async listPlaylists() { return this.sendCommand('playlist.list'); }
    async createPlaylist(name, description = '') { return this.sendCommand('playlist.create', { name, description }); }
    async getPlaylist(playlist_id) { return this.sendCommand('playlist.get', { playlist_id }); }
    async updatePlaylist(playlist_id, data) { return this.sendCommand('playlist.update', { playlist_id, data }); }
    async deletePlaylist(playlist_id) { return this.sendCommand('playlist.delete', { playlist_id }); }
    async addPlaylistItem(playlist_id, filename, order = null) { return this.sendCommand('playlist.addItem', { playlist_id, filename, order }); }
    async removePlaylistItem(playlist_id, item_id) { return this.sendCommand('playlist.removeItem', { playlist_id, item_id }); }
    async reorderPlaylist(playlist_id, item_ids) { return this.sendCommand('playlist.reorder', { playlist_id, item_ids }); }
    async setPlaylistLoop(playlist_id, enabled) { return this.sendCommand('playlist.setLoop', { playlist_id, enabled }); }
    
    // === SYSTEM ===
    async getVersion() { return this.sendCommand('system.version'); }
    async getInfo() { return this.sendCommand('system.info'); }
    async getUptime() { return this.sendCommand('system.uptime'); }
    async getMemory() { return this.sendCommand('system.memory'); }
    async getDisk() { return this.sendCommand('system.disk'); }
    async getCommands() { return this.sendCommand('system.commands'); }
    async ping() { return this.sendCommand('system.ping'); }
    
    // === NETWORK ===
    async getNetworkStatus() { return this.sendCommand('network.status'); }
    async getNetworkInterfaces() { return this.sendCommand('network.interfaces'); }
    async getNetworkStats() { return this.sendCommand('network.stats'); }
    
    // === BLUETOOTH ===
    async getBluetoothStatus() { return this.sendCommand('bluetooth.status'); }
    async scanBluetooth() { return this.sendCommand('bluetooth.scan'); }
    async pairBluetooth(address) { return this.sendCommand('bluetooth.pair', { address }); }
    async unpairBluetooth(address) { return this.sendCommand('bluetooth.unpair', { address }); }
    async getPairedBluetooth() { return this.sendCommand('bluetooth.paired'); }
    async forgetBluetooth(address) { return this.sendCommand('bluetooth.forget', { address }); }
    async getBluetoothSignal(device_id) { return this.sendCommand('bluetooth.signal', { device_id }); }
    async setBluetoothConfig(settings) { return this.sendCommand('bluetooth.config', { settings }); }
    
    // === LOGGER ===
    async getLogs(level = 'info', limit = 100) { return this.sendCommand('logger.getLogs', { level, limit }); }
    async getLogLevel() { return this.sendCommand('logger.getLevel'); }
    async setLogLevel(level) { return this.sendCommand('logger.setLevel', { level }); }
    async clearLogs() { return this.sendCommand('logger.clear'); }
    async exportLogs(filename) { return this.sendCommand('logger.export', { filename }); }
    
    // ========================================================================
    // ✅ FIX ERREUR #5: HISTORIQUE DE DIAGNOSTIC (localStorage)
    // ========================================================================
    
    loadConnectionHistory() {
        try {
            const history = localStorage.getItem('midimind_connection_history');
            return history ? JSON.parse(history) : [];
        } catch (error) {
            this.logger.error('BackendService', 'Error loading connection history:', error);
            return [];
        }
    }
    
    saveConnectionEvent(eventType, data) {
        try {
            const event = {
                type: eventType,
                timestamp: new Date().toISOString(),
                ...data
            };
            
            this.connectionHistory.push(event);
            
            // Garder seulement les 50 derniers événements
            if (this.connectionHistory.length > 50) {
                this.connectionHistory = this.connectionHistory.slice(-50);
            }
            
            localStorage.setItem('midimind_connection_history', JSON.stringify(this.connectionHistory));
            
            this.logger.debug('BackendService', `Connection event saved: ${eventType}`);
        } catch (error) {
            this.logger.error('BackendService', 'Error saving connection event:', error);
        }
    }
    
    getConnectionHistory() {
        return [...this.connectionHistory];
    }
    
    clearConnectionHistory() {
        this.connectionHistory = [];
        try {
            localStorage.removeItem('midimind_connection_history');
            this.logger.info('BackendService', 'Connection history cleared');
        } catch (error) {
            this.logger.error('BackendService', 'Error clearing connection history:', error);
        }
    }
    
    // ========================================================================
    // UTILS
    // ========================================================================
    
    flushMessageQueue() {
        if (this.messageQueue.length === 0) return;
        
        this.logger.info('BackendService', `📤 Flushing message queue (${this.messageQueue.length} messages)`);
        
        let success = 0;
        let failed = 0;
        
        while (this.messageQueue.length > 0) {
            const message = this.messageQueue.shift();
            if (this.send(message)) {
                success++;
            } else {
                failed++;
            }
        }
        
        this.logger.info('BackendService', `📊 Queue flushed: ${success} sent, ${failed} failed`);
    }
    
    disconnect() {
        this.stopHeartbeat();
        this.reconnectionStopped = true;
        
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        
        // ✅ FIX ERREUR #4: Nettoyer les callbacks
        this.failPendingCallbacks('Manual disconnect');
        
        if (this.ws) {
            this.ws.close(1000, 'Client disconnect');
            this.ws = null;
        }
        
        this.connected = false;
        this.connecting = false;
        this.reconnectAttempts = 0;
        
        this.logger.info('BackendService', 'Disconnected');
        
        // ✅ FIX ERREUR #5: Logger la déconnexion
        this.saveConnectionEvent('manual_disconnect', {
            timestamp: new Date().toISOString()
        });
    }
    
    enableReconnection() {
        this.reconnectionStopped = false;
        this.offlineMode = false;
        this.reconnectAttempts = 0;
        
        if (!this.connected && !this.connecting) {
            this.logger.info('BackendService', 'Reconnection enabled, attempting to connect');
            this.connect();
        }
    }
    
    disableReconnection() {
        this.reconnectionStopped = true;
        this.logger.info('BackendService', 'Reconnection disabled');
    }
    
    isConnected() {
        return this.connected && this.ws && this.ws.readyState === WebSocket.OPEN;
    }
    
    isOffline() {
        return this.offlineMode;
    }
    
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
            pendingCallbacks: this.messageCallbacks.size,
            url: this.config.url,
            lastActivityTime: this.lastActivityTime,
            timeSinceActivity: Date.now() - this.lastActivityTime,
            heartbeatFailures: this.heartbeatFailures,
            heartbeatPending: this.heartbeatPending,
            heartbeatInterval: this.config.heartbeatInterval,
            heartbeatTimeout: this.config.heartbeatTimeout,
            uptime: this.connectionStartTime ? Date.now() - this.connectionStartTime : 0
        };
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    formatUptime(ms) {
        const seconds = Math.floor(ms / 1000);
        const minutes = Math.floor(seconds / 60);
        const hours = Math.floor(minutes / 60);
        
        if (hours > 0) {
            return `${hours}h ${minutes % 60}m ${seconds % 60}s`;
        } else if (minutes > 0) {
            return `${minutes}m ${seconds % 60}s`;
        } else {
            return `${seconds}s`;
        }
    }
    
    getCloseReason(code) {
        const reasons = {
            1000: 'Normal closure',
            1001: 'Going away (browser closed)',
            1002: 'Protocol error',
            1003: 'Unsupported data',
            1006: 'Abnormal closure (connection lost)', // ⚠️ PROBLÈME PRINCIPAL
            1007: 'Invalid frame payload data',
            1008: 'Policy violation',
            1009: 'Message too big',
            1010: 'Missing extension',
            1011: 'Internal server error',
            1012: 'Service restart',
            1013: 'Try again later',
            1014: 'Bad gateway',
            1015: 'TLS handshake failed'
        };
        
        return reasons[code] || `Unknown code: ${code}`;
    }
}

// ============================================================================
// EXPORT & INITIALISATION GLOBALE
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackendService;
}
window.BackendService = BackendService;
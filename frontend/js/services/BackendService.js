// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Chemin rÃƒÂ©el: frontend/js/services/BackendService.js  
// Version: v4.4.1 - API v4.2.2 ENVELOPE + VALIDATION
// Date: 2025-11-10
// ============================================================================
// CORRECTIONS v4.4.1 (VALIDATION ENVELOPE):
// Ã¢Å“â€¦ validateEnvelopeFormat() - Validation messages avant envoi
// Ã¢Å“â€¦ Validation dans send() et flushMessageQueue()
// Ã¢Å“â€¦ Rejet messages invalides sans mise en queue
// Ã¢Å“â€¦ FIXES SYNTAXE: 3 erreurs corrigÃƒÂ©es (backticks, parenthÃƒÂ¨ses)
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
        
        const isDevelopment = window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';
        const isRaspberryPi = window.location.hostname === '192.168.1.37';
        
        this.config = {
            url: url || 'ws://localhost:8080',
            reconnectInterval: isDevelopment ? 2000 : 3000,
            maxReconnectInterval: isDevelopment ? 20000 : 30000,
            reconnectDecay: 1.5,
            timeoutInterval: isDevelopment ? 3000 : 5000,
            maxReconnectAttempts: isDevelopment ? 10 : 5,
            heartbeatInterval: isRaspberryPi ? 60000 : 45000,
            heartbeatTimeout: isRaspberryPi ? 120000 : 90000,
            maxHeartbeatFailures: 3,
            defaultCommandTimeout: 10000,
            heartbeatCommandTimeout: isRaspberryPi ? 20000 : 15000
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
        this.connectionHistory = this.loadConnectionHistory();
        
        this.logger.info('BackendService', `Service initialized (v4.4.1 - VALIDATION)`);
        this.logger.info('BackendService', `Environment: ${isDevelopment ? 'DEV' : isRaspberryPi ? 'RPI' : 'PROD'}`);
    }
    
    // ========================================================================
    // UUID & TIMESTAMP
    // ========================================================================
    
    generateUUID() {
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
            const r = Math.random() * 16 | 0;
            const v = c === 'x' ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
    }
    
    generateTimestamp() {
        return new Date().toISOString();
    }
    
    /**
     * Ã¢Å“â€¦ v4.4.1: Valide format envelope avant envoi
     */
    validateEnvelopeFormat(message) {
        if (typeof message !== 'object' || message === null) {
            this.logger.error('BackendService', 'Ã¢ÂÅ’ Not an object');
            return false;
        }
        
        const required = ['id', 'type', 'timestamp', 'version', 'payload'];
        for (const field of required) {
            if (!message.hasOwnProperty(field)) {
                this.logger.error('BackendService', `Ã¢ÂÅ’ Missing field: ${field}`);
                return false;
            }
        }
        
        if (message.type === 'request' && message.payload) {
            const payloadRequired = ['id', 'command', 'params'];
            for (const field of payloadRequired) {
                if (!message.payload.hasOwnProperty(field)) {
                    this.logger.error('BackendService', `Ã¢ÂÅ’ Missing payload field: ${field}`);
                    return false;
                }
            }
        }
        
        return true;
    }
    
    // ========================================================================
    // CONNEXION WEBSOCKET
    // ========================================================================
    
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
        
        this.logger.info('BackendService', 'Ã¢Å“â€¦ Connected successfully');
        this.eventBus.emit('backend:connected');
        
        this.saveConnectionEvent('connected', {
            timestamp: this.generateTimestamp(),
            url: this.config.url
        });
        
        this.startHeartbeat();
        this.flushMessageQueue();
    }
    
    handleClose(event) {
        const wasConnected = this.connected;
        const uptime = this.connectionStartTime ? Date.now() - this.connectionStartTime : 0;
        
        const diagnostic = {
            timestamp: this.generateTimestamp(),
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
        
        this.connected = false;
        this.connecting = false;
        this.stopHeartbeat();
        
        this.logger.error('BackendService', 'Ã¢ÂÅ’ CONNEXION FERMÃƒâ€°E:', JSON.stringify(diagnostic, null, 2));
        
        const closeReason = this.getCloseReason(event.code);
        this.logger.warn('BackendService', `Code ${event.code}: ${closeReason}`);
        
        this.saveConnectionEvent('closed', diagnostic);
        this.failPendingCallbacks('Connection closed');
        
        if (wasConnected) {
            this.eventBus.emit('backend:disconnected', {
                code: event.code,
                reason: event.reason,
                uptime: uptime,
                diagnostic: diagnostic
            });
        }
        
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
        
        this.saveConnectionEvent('error', {
            timestamp: this.generateTimestamp(),
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
            `Ã¢â€ Â» Reconnection attempt ${this.reconnectAttempts}/${this.config.maxReconnectAttempts} in ${Math.round(delay/1000)}s`);
        
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
        this.logger.warn('BackendService', 'Ã¢Å¡Â Ã¯Â¸Â Entering offline mode');
        
        this.eventBus.emit('backend:offline-mode', {
            timestamp: Date.now()
        });
        
        this.saveConnectionEvent('offline', {
            timestamp: this.generateTimestamp(),
            reconnectAttempts: this.reconnectAttempts
        });
    }
    
    // ========================================================================
    // HEARTBEAT
    // ========================================================================
    
    startHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
        }
        
        this.heartbeatTimer = setInterval(() => {
            this.checkHeartbeat();
        }, this.config.heartbeatInterval);
        
        this.logger.debug('BackendService', `Ã°Å¸â€™â€” Heartbeat started (interval: ${this.config.heartbeatInterval}ms)`);
    }
    
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
            this.logger.debug('BackendService', 'Ã°Å¸â€™â€ Heartbeat stopped');
        }
    }
    
    async checkHeartbeat() {
        const timeSinceActivity = Date.now() - this.lastActivityTime;
        
        if (timeSinceActivity > this.config.heartbeatTimeout) {
            this.heartbeatFailures++;
            
            this.logger.warn('BackendService', 
                `Ã¢Å¡Â Ã¯Â¸Â Heartbeat timeout! No activity for ${Math.round(timeSinceActivity/1000)}s (failure #${this.heartbeatFailures}/${this.config.maxHeartbeatFailures})`);
            
            if (this.heartbeatFailures >= this.config.maxHeartbeatFailures) {
                this.logger.error('BackendService', `Ã°Å¸â€™â€ ${this.config.maxHeartbeatFailures} consecutive heartbeat failures`);
                this.forceReconnect(`Heartbeat timeout - ${this.config.maxHeartbeatFailures} failures`);
                return;
            }
        }
        
        if (this.heartbeatPending) {
            this.logger.warn('BackendService', 'Ã¢ÂÂ³ Heartbeat already pending, skipping');
            return;
        }
        
        try {
            this.heartbeatPending = true;
            this.lastHeartbeatCheck = Date.now();
            
            this.logger.debug('BackendService', 'Ã°Å¸â€™â€” Sending heartbeat (system.ping)');
            
            const startTime = Date.now();
            const result = await this.sendCommand('system.ping', {}, this.config.heartbeatCommandTimeout);
            const latency = Date.now() - startTime;
            
            this.heartbeatPending = false;
            this.heartbeatFailures = 0;
            
            this.logger.debug('BackendService', `Ã°Å¸â€™Å¡ Heartbeat OK (${latency}ms)`);
            
            this.eventBus.emit('backend:heartbeat', {
                latency: latency,
                timestamp: Date.now(),
                success: true
            });
            
        } catch (error) {
            this.heartbeatPending = false;
            this.heartbeatFailures++;
            
            this.logger.error('BackendService', `Ã¢ÂÅ’ Heartbeat failed (failure #${this.heartbeatFailures}/${this.config.maxHeartbeatFailures}):`, error.message);
            
            this.eventBus.emit('backend:heartbeat', {
                timestamp: Date.now(),
                success: false,
                error: error.message,
                failures: this.heartbeatFailures
            });
            
            if (this.heartbeatFailures >= this.config.maxHeartbeatFailures) {
                this.logger.error('BackendService', `Ã°Å¸â€™â€ ${this.config.maxHeartbeatFailures} heartbeat failures, forcing reconnect`);
                this.forceReconnect(`${this.config.maxHeartbeatFailures} consecutive heartbeat failures`);
            }
        }
    }
    
    forceReconnect(reason = 'Force reconnect') {
        this.logger.warn('BackendService', `Force reconnect: ${reason}`);
        
        this.saveConnectionEvent('force_reconnect', {
            timestamp: this.generateTimestamp(),
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
            
            this.logger.debug('BackendService', '[DEBUG] RAW MESSAGE:', JSON.stringify(message, null, 2));
            
            if (!message.id || !message.type || !message.timestamp || !message.version || !message.payload) {
                this.logger.error('BackendService', 'Ã¢ÂÅ’ INVALID MESSAGE FORMAT (missing envelope fields):', message);
                return;
            }
            
            this.logger.debug('BackendService', 'Ã°Å¸â€œÂ© Message received:', {
                id: message.id,
                type: message.type,
                timestamp: message.timestamp
            });
            
            switch (message.type) {
                case 'response':
                    this.handleBackendResponse(message);
                    break;
                
                case 'event':
                    this.handleBackendEvent(message);
                    break;
                
                case 'error':
                    this.handleBackendError(message);
                    break;
                
                default:
                    this.logger.warn('BackendService', `Unknown message type: ${message.type}`);
            }
            
        } catch (error) {
            this.logger.error('BackendService', 'Error parsing message:', error, event.data);
        }
    }
    
    handleBackendResponse(message) {
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
        
        const simpleResponse = {
            id: requestId,
            success: message.payload.success === true,
            data: message.payload.data,
            error: message.payload.error_message || message.payload.error,
            latency: message.payload.latency
        };
        
        callback(simpleResponse);
    }
    
    handleBackendEvent(message) {
        const eventName = message.payload.name || message.payload.event;
        const eventData = message.payload.data || message.payload;
        
        if (eventName) {
            this.logger.debug('BackendService', 'Ã°Å¸â€œÂ¡ Backend Event: ' + eventName, eventData);
            this.eventBus.emit(eventName, eventData);
            this.eventBus.emit('backend:event:' + eventName, eventData);
        } else {
            this.logger.debug('BackendService', 'Ã°Å¸â€œÂ¡ Backend message (no event name)');
        }
    }
    
    handleBackendError(message) {
        const requestId = message.payload.request_id;
        const errorMessage = message.payload.message || message.payload.error || 'Unknown error';
        const errorCode = message.payload.code;
        
        this.logger.error('BackendService', 'Ã¢Å“â€” Backend error:', errorCode, errorMessage);
        
        if (requestId && this.messageCallbacks.has(requestId)) {
            const callback = this.messageCallbacks.get(requestId);
            this.messageCallbacks.delete(requestId);
            callback({ 
                id: requestId, 
                success: false, 
                error: errorMessage,
                error_code: errorCode
            });
        }
        
        this.eventBus.emit('backend:error', {
            message: errorMessage,
            code: errorCode,
            requestId: requestId,
            retryable: message.payload.retryable
        });
    }
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    send(data) {
        if (!this.connected || !this.ws || this.ws.readyState !== WebSocket.OPEN) {
            if (typeof data === 'object' && !this.validateEnvelopeFormat(data)) {
                this.logger.error('BackendService', 'Ã¢ÂÅ’ REJECT invalid - not queued');
                return false;
            }
            if (this.messageQueue.length >= this.maxQueueSize) {
                this.logger.warn('BackendService', 'Queue full');
                return false;
            }
            this.messageQueue.push(data);
            return false;
        }
        
        try {
            let message;
            if (typeof data === 'string') {
                const parsed = JSON.parse(data);
                if (!this.validateEnvelopeFormat(parsed)) return false;
                message = data;
            } else {
                if (!this.validateEnvelopeFormat(data)) return false;
                message = JSON.stringify(data);
            }
            this.ws.send(message);
            return true;
        } catch (error) {
            this.logger.error('BackendService', 'Send error:', error);
            return false;
        }
    }
    
    async sendCommand(command, params = {}, timeout = null) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected()) {
                reject(new Error('Not connected to backend'));
                return;
            }
            
            const timeoutMs = timeout || this.config.defaultCommandTimeout;
            const id = this.generateUUID();
            
            const timeoutTimer = setTimeout(() => {
                if (this.messageCallbacks.has(id)) {
                    this.messageCallbacks.delete(id);
                    reject(new Error(`Command timeout after ${timeoutMs}ms: ${command}`));
                }
            }, timeoutMs);
            
            this.messageCallbacks.set(id, (response) => {
                clearTimeout(timeoutTimer);
                
                if (response.success === true) {
                    this.logger.debug('BackendService', `Ã¢Å“â€œ Command success [${id}]: ${command}`);
                    resolve(response.data || response);
                } else {
                    this.logger.error('BackendService', `Ã¢Å“â€” Command failed [${id}]: ${command}`, response.error);
                    reject(new Error(response.error || 'Command failed'));
                }
            });
            
            const message = {
                id: id,
                type: "request",
                timestamp: this.generateTimestamp(),
                version: "1.0",
                payload: {
                    id: id,
                    command: command,
                    params: params || {},
                    timeout: timeoutMs
                }
            };
            
            this.logger.debug('BackendService', `[DEBUG] SENDING ENVELOPE [${id}]:`, command, params);
            
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
        
        this.logger.info('BackendService', `Ã¢Å“â€œ Cleared ${failedCount} pending callbacks`);
    }
    
    // ========================================================================
    // API MIDI COMMANDS (90+ mÃƒÂ©thodes)
    // ========================================================================
    
    // DEVICES
    async listDevices() { return this.sendCommand('devices.list'); }
    async scanDevices(full_scan = false) { return this.sendCommand('devices.scan', { full_scan }); }
    async connectDevice(device_id) { return this.sendCommand('devices.connect', { device_id }); }
    async disconnectDevice(device_id) { return this.sendCommand('devices.disconnect', { device_id }); }
    async disconnectAllDevices() { return this.sendCommand('devices.disconnectAll'); }
    async getDeviceInfo(device_id) { return this.sendCommand('devices.getInfo', { device_id }); }
    async getConnectedDevices() { return this.sendCommand('devices.getConnected'); }
    async startHotPlug(interval_ms = 2000) { return this.sendCommand('devices.startHotPlug', { interval_ms }); }
    async stopHotPlug() { return this.sendCommand('devices.stopHotPlug'); }
    async getHotPlugStatus() { return this.sendCommand('devices.getHotPlugStatus'); }
    
    // BLUETOOTH
    async bluetoothConfig(enabled, scan_timeout = 5) { return this.sendCommand('bluetooth.config', { enabled, scan_timeout }); }
    async bluetoothStatus() { return this.sendCommand('bluetooth.status'); }
    async bluetoothScan(duration = 5, filter = '') { return this.sendCommand('bluetooth.scan', { duration, filter }); }
    async bluetoothPair(address, pin = '') { return this.sendCommand('bluetooth.pair', { address, pin }); }
    async bluetoothUnpair(address) { return this.sendCommand('bluetooth.unpair', { address }); }
    async bluetoothPaired() { return this.sendCommand('bluetooth.paired'); }
    async bluetoothForget(address) { return this.sendCommand('bluetooth.forget', { address }); }
    async bluetoothSignal(device_id) { return this.sendCommand('bluetooth.signal', { device_id }); }
    
    // ROUTING
    async addRoute(source_id, destination_id) { return this.sendCommand('routing.addRoute', { source_id, destination_id }); }
    async removeRoute(source_id, destination_id) { return this.sendCommand('routing.removeRoute', { source_id, destination_id }); }
    async clearRoutes() { return this.sendCommand('routing.clearRoutes'); }
    async listRoutes() { return this.sendCommand('routing.listRoutes'); }
    async enableRoute(source_id, destination_id) { return this.sendCommand('routing.enableRoute', { source_id, destination_id }); }
    async disableRoute(source_id, destination_id) { return this.sendCommand('routing.disableRoute', { source_id, destination_id }); }
    
    // PLAYBACK
    async loadPlayback(filename) { return this.sendCommand('playback.load', { filename }); }
    async play(filename = null) { return this.sendCommand('playback.play', filename ? { filename } : {}); }
    async pause() { return this.sendCommand('playback.pause'); }
    async stop() { return this.sendCommand('playback.stop'); }
    async getPlaybackStatus() { return this.sendCommand('playback.getStatus'); }
    async seek(position) { return this.sendCommand('playback.seek', { position }); }
    async setTempo(tempo) { return this.sendCommand('playback.setTempo', { tempo }); }
    async setLoop(enabled) { return this.sendCommand('playback.setLoop', { enabled }); }
    async getPlaybackInfo() { return this.sendCommand('playback.getInfo'); }
    async listPlaybackFiles() { return this.sendCommand('playback.listFiles'); }
    async resume() { return this.sendCommand('playback.resume'); }
    
    // FILES
    async listFiles(path = '/midi') { return this.sendCommand('files.list', { path }); }
    async readFile(filename) { return this.sendCommand('files.read', { filename }); }
    async writeFile(filename, content, base64 = true) { return this.sendCommand('files.write', { filename, content, base64 }); }
    async deleteFile(filename) { return this.sendCommand('files.delete', { filename }); }
    async fileExists(filename) { return this.sendCommand('files.exists', { filename }); }
    async getFileInfo(filename) { return this.sendCommand('files.getInfo', { filename }); }
    
    // LATENCY
    async setLatencyCompensation(instrument_id, offset_ms) { return this.sendCommand('latency.setCompensation', { instrument_id, offset_ms }); }
    async getLatencyCompensation(instrument_id) { return this.sendCommand('latency.getCompensation', { instrument_id }); }
    async enableLatency() { return this.sendCommand('latency.enable'); }
    async disableLatency() { return this.sendCommand('latency.disable'); }
    async setGlobalLatencyOffset(offset_ms) { return this.sendCommand('latency.setGlobalOffset', { offset_ms }); }
    async getGlobalLatencyOffset() { return this.sendCommand('latency.getGlobalOffset'); }
    async listLatencyInstruments() { return this.sendCommand('latency.listInstruments'); }
    
    // PRESETS
    async listPresets() { return this.sendCommand('preset.list'); }
    async loadPreset(id) { return this.sendCommand('preset.load', { id }); }
    async savePreset(preset) { return this.sendCommand('preset.save', { preset }); }
    async deletePreset(id) { return this.sendCommand('preset.delete', { id }); }
    async exportPreset(id, filepath) { return this.sendCommand('preset.export', { id, filepath }); }
    
    // MIDI
    async convertMidi(filename) { return this.sendCommand('midi.convert', { filename }); }
    async loadMidi(filepath) { return this.sendCommand('midi.load', { filepath }); }
    async saveMidi(filepath, data) { return this.sendCommand('midi.save', { filepath, data }); }
    async importMidi(filepath) { return this.sendCommand('midi.import', { filepath }); }
    
    /**
     * âœ… v4.4.2: Upload File object (wrapper pour importMidi)
     * UtilisÃ© comme fallback dans FileController
     */
    async uploadFile(file) {
        if (!file || !file.name) {
            throw new Error('Invalid file object');
        }
        
        this.logger.info('BackendService', `Uploading file: ${file.name}`);
        
        // Lire le fichier en ArrayBuffer puis convertir en base64
        const arrayBuffer = await this._readFileAsArrayBuffer(file);
        const base64Data = btoa(
            new Uint8Array(arrayBuffer).reduce((data, byte) => data + String.fromCharCode(byte), '')
        );
        
        // Appeler importMidi avec le contenu base64
        return await this.importMidi(file.name, base64Data, true);
    }
    
    /**
     * Utilitaire: Lire un File en ArrayBuffer
     */
    _readFileAsArrayBuffer(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = (e) => resolve(e.target.result);
            reader.onerror = (e) => reject(new Error('Failed to read file'));
            reader.readAsArrayBuffer(file);
        });
    }
    
    async addMidiRouting(midi_file_id, track_id, device_id, instrument_name = '', channel = 0, enabled = true) { 
        return this.sendCommand('midi.routing.add', { midi_file_id, track_id, device_id, instrument_name, channel, enabled }); 
    }
    async listMidiRoutings(midi_file_id) { return this.sendCommand('midi.routing.list', { midi_file_id }); }
    async updateMidiRouting(routing_id, enabled) { return this.sendCommand('midi.routing.update', { routing_id, enabled }); }
    async removeMidiRouting(routing_id) { return this.sendCommand('midi.routing.remove', { routing_id }); }
    async clearMidiRoutings(midi_file_id) { return this.sendCommand('midi.routing.clear', { midi_file_id }); }
    async sendNoteOn(device_id, note, velocity, channel = 0) { return this.sendCommand('midi.sendNoteOn', { device_id, note, velocity, channel }); }
    async sendNoteOff(device_id, note, channel = 0) { return this.sendCommand('midi.sendNoteOff', { device_id, note, channel }); }
    
    // PLAYLISTS
    async createPlaylist(name, description = '') { return this.sendCommand('playlist.create', { name, description }); }
    async deletePlaylist(playlist_id) { return this.sendCommand('playlist.delete', { playlist_id }); }
    async updatePlaylist(playlist_id, name, description = '') { return this.sendCommand('playlist.update', { playlist_id, name, description }); }
    async listPlaylists() { return this.sendCommand('playlist.list'); }
    async getPlaylist(playlist_id) { return this.sendCommand('playlist.get', { playlist_id }); }
    async addPlaylistItem(playlist_id, midi_file_id) { return this.sendCommand('playlist.addItem', { playlist_id, midi_file_id }); }
    async removePlaylistItem(playlist_id, item_id) { return this.sendCommand('playlist.removeItem', { playlist_id, item_id }); }
    async reorderPlaylist(playlist_id, item_ids) { return this.sendCommand('playlist.reorder', { playlist_id, item_ids }); }
    async setPlaylistLoop(playlist_id, enabled) { return this.sendCommand('playlist.setLoop', { playlist_id, enabled }); }
    
    // SYSTEM
    async systemPing() { return this.sendCommand('system.ping'); }
    async getVersion() { return this.sendCommand('system.version'); }
    async getSystemInfo() { return this.sendCommand('system.info'); }
    async getUptime() { return this.sendCommand('system.uptime'); }
    async getMemory() { return this.sendCommand('system.memory'); }
    async getDisk() { return this.sendCommand('system.disk'); }
    async getCommands() { return this.sendCommand('system.commands'); }
    
    // NETWORK
    async getNetworkStatus() { return this.sendCommand('network.status'); }
    async getNetworkInterfaces() { return this.sendCommand('network.interfaces'); }
    async getNetworkStats() { return this.sendCommand('network.stats'); }
    
    // LOGGER
    async setLogLevel(level) { return this.sendCommand('logger.setLevel', { level }); }
    async getLogLevel() { return this.sendCommand('logger.getLevel'); }
    async getLogs(count = 100) { return this.sendCommand('logger.getLogs', { count }); }
    async clearLogs() { return this.sendCommand('logger.clear'); }
    async exportLogs(filename) { return this.sendCommand('logger.export', { filename }); }
    
    // MIDI ALIASES
    async listMidiFiles(directory = '') { return this.sendCommand('files.list', { path: directory }); }
    async getMidiFile(filename) { return this.sendCommand('files.read', { filename }); }
    async uploadMidiFile(filename, content) { return this.sendCommand('files.write', { filename, content }); }
    async deleteMidiFile(filename) { return this.sendCommand('files.delete', { filename }); }
    async renameMidiFile(oldName, newName) { return this.sendCommand('files.rename', { oldName, newName }); }
    async moveMidiFile(filename, destination) { return this.sendCommand('files.move', { filename, destination }); }
    async copyMidiFile(filename, destination) { return this.sendCommand('files.copy', { filename, destination }); }
    async createDirectory(path) { return this.sendCommand('files.createDir', { path }); }
    async deleteDirectory(path) { return this.sendCommand('files.deleteDir', { path }); }
    async getMidiInfo(filename) { return this.sendCommand('files.getInfo', { filename }); }
    async searchFiles(query) { return this.sendCommand('files.search', { query }); }
    async getRecentFiles(limit = 10) { return this.sendCommand('files.getRecent', { limit }); }
    
    // MIDI MESSAGES
    async sendMidiMessage(device_id, message) { return this.sendCommand('midi.send', { device_id, message }); }
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
    
    // ========================================================================
    // DIAGNOSTIC
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
                timestamp: this.generateTimestamp(),
                ...data
            };
            
            this.connectionHistory.push(event);
            
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
        
        let success = 0, failed = 0, invalid = 0;
        
        while (this.messageQueue.length > 0) {
            const msg = this.messageQueue.shift();
            if (typeof msg === 'object' && !this.validateEnvelopeFormat(msg)) {
                invalid++;
                continue;
            }
            this.send(msg) ? success++ : failed++;
        }
        
        this.logger.info('BackendService', `Flushed: ${success} sent, ${failed} fail, ${invalid} invalid`);
    }
    
    disconnect() {
        this.stopHeartbeat();
        this.reconnectionStopped = true;
        
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        
        this.failPendingCallbacks('Manual disconnect');
        
        if (this.ws) {
            this.ws.close(1000, 'Client disconnect');
            this.ws = null;
        }
        
        this.connected = false;
        this.connecting = false;
        this.reconnectAttempts = 0;
        
        this.logger.info('BackendService', 'Disconnected');
        
        this.saveConnectionEvent('manual_disconnect', {
            timestamp: this.generateTimestamp()
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
            1006: 'Abnormal closure (connection lost)',
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
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackendService;
}
window.BackendService = BackendService;
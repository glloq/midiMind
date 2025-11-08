// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Chemin réel: frontend/js/services/BackendService.js
// Version: v4.2.3 - API FORMAT PLAT CORRIGÉ (NO DOWNGRADING)
// Date: 2025-11-08
// ============================================================================
// CORRECTIONS v4.2.3:
// ✅ FORMAT MESSAGE PLAT: { id, type, timestamp, version, payload }
// ✅ Suppression de la structure envelope/payload imbriquée dans sendCommand
// ✅ Conforme à API_WEBSOCKET_PROTOCOL.md v4.2.2
// ✅ GARDE la compatibilité avec handleResponse/handleEvent (legacy frontend)
// ✅ AJOUTE handleBackendError pour messages d'erreur type='error'
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
        
        this.config = {
            url: url || 'ws://localhost:8080',
            reconnectInterval: 3000,
            maxReconnectInterval: 30000,
            reconnectDecay: 1.5,
            timeoutInterval: 5000,
            heartbeatInterval: 30000,
            heartbeatTimeout: 60000,
            maxReconnectAttempts: 5,
            defaultCommandTimeout: 10000
        };
        
        this.reconnectAttempts = 0;
        this.reconnectTimer = null;
        this.heartbeatTimer = null;
        this.connectionTimeout = null;
        
        this.lastActivityTime = Date.now();
        this.heartbeatPending = false;
        this.heartbeatFailures = 0;
        
        this.messageQueue = [];
        this.maxQueueSize = 100;
        
        this.pendingRequests = new Map();
        
        this.logger.info('BackendService', 'Service initialized (v4.2.3 - FLAT FORMAT, NO DOWNGRADING)');
    }
    
    generateUUID() {
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
            const r = Math.random() * 16 | 0;
            const v = c === 'x' ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
    }
    
    generateTimestamp() {
        return new Date().toISOString();
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
        
        this.logger.info('BackendService', '✓ Connected successfully');
        this.eventBus.emit('backend:connected');
        
        this.startHeartbeat();
        this.flushMessageQueue();
    }
    
    handleClose(event) {
        const wasConnected = this.connected;
        
        this.connected = false;
        this.connecting = false;
        this.stopHeartbeat();
        
        this.logger.warn('BackendService', `Connection closed: code=${event.code}, reason=${event.reason}`);
        
        if (wasConnected) {
            this.eventBus.emit('backend:disconnected', {
                code: event.code,
                reason: event.reason
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
    }
    
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
    }
    
    startHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
        }
        
        this.heartbeatTimer = setInterval(() => {
            this.checkHeartbeat();
        }, this.config.heartbeatInterval);
        
        this.logger.debug('BackendService', '💗 Heartbeat started');
    }
    
    async checkHeartbeat() {
        const timeSinceActivity = Date.now() - this.lastActivityTime;
        
        if (timeSinceActivity > this.config.heartbeatTimeout) {
            this.heartbeatFailures++;
            
            this.logger.warn('BackendService', 
                `⚠ Heartbeat timeout! No activity since ${Math.round(timeSinceActivity/1000)}s (failure #${this.heartbeatFailures})`);
            
            if (this.heartbeatFailures >= 3) {
                this.logger.error('BackendService', '💔 Connection dead, forcing reconnect');
                this.forceReconnect('Heartbeat timeout - connection dead');
                return;
            }
        }
        
        if (this.heartbeatPending) {
            this.logger.debug('BackendService', 'Heartbeat pending, skipping');
            return;
        }
        
        try {
            this.heartbeatPending = true;
            this.logger.debug('BackendService', '💗 Sending heartbeat (system.ping)');
            
            const startTime = Date.now();
            await this.sendCommand('system.ping');
            const latency = Date.now() - startTime;
            
            this.heartbeatPending = false;
            this.heartbeatFailures = 0;
            
            this.logger.debug('BackendService', `💗 Heartbeat OK (${latency}ms)`);
            
            this.eventBus.emit('backend:heartbeat', {
                latency: latency,
                timestamp: Date.now()
            });
        } catch (error) {
            this.heartbeatPending = false;
            this.heartbeatFailures++;
            
            this.logger.warn('BackendService', `💔 Heartbeat failed (failure #${this.heartbeatFailures}):`, error.message);
            
            if (this.heartbeatFailures >= 3) {
                this.logger.error('BackendService', '💔 Too many heartbeat failures, forcing reconnect');
                this.forceReconnect('Multiple heartbeat failures');
            }
        }
    }
    
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
            this.logger.debug('BackendService', '💔 Heartbeat stopped');
        }
    }
    
    forceReconnect(reason = 'Force reconnect') {
        this.logger.warn('BackendService', `Force reconnect: ${reason}`);
        
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
    
    handleMessage(event) {
        this.lastActivityTime = Date.now();
        
        try {
            const message = JSON.parse(event.data);
            
            // DEBUG: Log complet du message
            console.log('[DEBUG] RAW MESSAGE:', JSON.stringify(message, null, 2));
            
            this.logger.debug('BackendService', '📩 Message received:', {
                hasId: !!message.id,
                hasEnvelope: !!message.envelope,
                type: message.type || message.envelope?.type || message.event || 'unknown'
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
            // Frontend format: { envelope: { type }, payload } - COMPATIBILITÉ LEGACY
            else if (message.envelope && message.envelope.type === 'response') {
                this.handleResponse(message);
            } else if (message.event) {
                this.handleEvent(message);
            } else {
                this.logger.warn('BackendService', 'Unknown message format:', message);
            }
        } catch (error) {
            this.logger.error('BackendService', 'Error parsing message:', error, event.data);
        }
    }
    
    handleBackendResponse(message) {
        // Backend response: payload.request_id correspond à l'id de la requête
        const requestId = message.payload.request_id;
        
        console.log('[DEBUG] RESPONSE:', {
            requestId: requestId,
            hasPending: this.pendingRequests.has(requestId),
            pendingCount: this.pendingRequests.size,
            payload: message.payload
        });
        
        if (!requestId || !this.pendingRequests.has(requestId)) {
            this.logger.warn('BackendService', 'No pending request for ID: ' + requestId);
            return;
        }
        
        const pending = this.pendingRequests.get(requestId);
        clearTimeout(pending.timeoutTimer);
        this.pendingRequests.delete(requestId);
        
        if (message.payload.success !== false && !message.payload.error) {
            const data = message.payload.data || message.payload;
            this.logger.debug('BackendService', '✓ Command success [' + requestId + ']', data);
            pending.resolve(data);
        } else {
            const error = message.payload.error || message.payload.error_message || 'Command failed';
            this.logger.error('BackendService', '✘ Command failed [' + requestId + ']:', error);
            pending.reject(new Error(error));
        }
    }
    
    handleBackendEvent(message) {
        const eventName = message.payload.name || message.payload.event;
        const eventData = message.payload.data || message.payload;
        
        if (eventName) {
            this.logger.debug('BackendService', '📡 Backend Event: ' + eventName, eventData);
            this.eventBus.emit('backend:event:' + eventName, eventData);
        } else {
            this.logger.debug('BackendService', '📡 Backend heartbeat (no event name)');
        }
    }
    
    handleBackendError(message) {
        const requestId = message.payload.request_id;
        const errorMessage = message.payload.error_message || message.payload.error || 'Unknown error';
        
        this.logger.error('BackendService', '❌ Backend error:', errorMessage);
        
        if (requestId && this.pendingRequests.has(requestId)) {
            const pending = this.pendingRequests.get(requestId);
            clearTimeout(pending.timeoutTimer);
            this.pendingRequests.delete(requestId);
            pending.reject(new Error(errorMessage));
        }
        
        this.eventBus.emit('backend:error', {
            message: errorMessage,
            requestId: requestId
        });
    }
    
    // COMPATIBILITÉ LEGACY - Format Frontend { envelope: {...}, payload: {...} }
    handleResponse(message) {
        const requestId = message.envelope.id;
        
        if (!this.pendingRequests.has(requestId)) {
            this.logger.warn('BackendService', `No pending request for ID: ${requestId}`);
            return;
        }
        
        const pending = this.pendingRequests.get(requestId);
        clearTimeout(pending.timeoutTimer);
        this.pendingRequests.delete(requestId);
        
        if (message.payload && message.payload.success) {
            const data = message.payload.data || message.payload;
            this.logger.debug('BackendService', `✓ Command success [${requestId}]`, data);
            pending.resolve(data);
        } else {
            const error = message.payload?.error || 'Command failed';
            this.logger.error('BackendService', `✘ Command failed [${requestId}]:`, error);
            pending.reject(new Error(error));
        }
    }
    
    // COMPATIBILITÉ LEGACY - Events Frontend
    handleEvent(message) {
        const eventName = message.event;
        const eventData = message.data || message;
        
        this.logger.debug('BackendService', `📡 Event: ${eventName}`, eventData);
        
        this.eventBus.emit(eventName, eventData);
    }
    
    send(data) {
        if (!this.isConnected()) {
            this.logger.warn('BackendService', 'Cannot send - not connected');
            
            if (this.messageQueue.length < this.maxQueueSize) {
                this.messageQueue.push(data);
                this.logger.info('BackendService', `Message queued (${this.messageQueue.length}/${this.maxQueueSize})`);
            } else {
                this.logger.error('BackendService', 'Message queue full - message dropped');
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
     * ✅ v4.2.3: Format PLAT conforme API v4.2.2
     * { id, type, timestamp, version, payload }
     */
    async sendCommand(command, params = {}, timeout = null) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected()) {
                reject(new Error('Not connected to backend'));
                return;
            }
            
            const timeoutMs = timeout || this.config.defaultCommandTimeout || 10000;
            const requestId = this.generateUUID();
            
            const timeoutTimer = setTimeout(() => {
                if (this.pendingRequests.has(requestId)) {
                    this.pendingRequests.delete(requestId);
                    reject(new Error(`Command timeout after ${timeoutMs}ms: ${command}`));
                }
            }, timeoutMs);
            
            this.pendingRequests.set(requestId, {
                resolve: (data) => {
                    resolve(data);
                },
                reject: (error) => {
                    reject(error);
                },
                timeoutTimer: timeoutTimer
            });
            
            // ✅ FORMAT PLAT selon API v4.2.2
            const message = {
                id: requestId,
                type: 'request',
                timestamp: this.generateTimestamp(),
                version: '1.0',
                payload: {
                    id: requestId,
                    command: command,
                    params: params,
                    timeout: timeoutMs
                }
            };
            
            console.log('[DEBUG] SENDING:', JSON.stringify(message, null, 2));
            this.logger.debug('BackendService', `📤 Sending command: ${command}`, params);
            
            if (!this.send(message)) {
                clearTimeout(timeoutTimer);
                this.pendingRequests.delete(requestId);
                reject(new Error(`Failed to send command: ${command}`));
            }
        });
    }
    
    async uploadFile(file, filename = null) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            
            reader.onload = async (e) => {
                try {
                    const arrayBuffer = e.target.result;
                    const uint8Array = new Uint8Array(arrayBuffer);
                    const base64 = btoa(String.fromCharCode.apply(null, uint8Array));
                    
                    const result = await this.sendCommand('midi.import', {
                        filename: filename || file.name,
                        content: base64,
                        base64: true
                    });
                    
                    resolve(result);
                } catch (error) {
                    reject(error);
                }
            };
            
            reader.onerror = (error) => {
                reject(error);
            };
            
            reader.readAsArrayBuffer(file);
        });
    }
    
    // ============================================================================
    // API COMMANDS (v4.2.2 - snake_case)
    // ============================================================================
    
    // === FILES ===
    async listFiles(path = '/midi') { 
        return this.sendCommand('files.list', { path }); 
    }
    
    async readFile(filename) { 
        return this.sendCommand('files.read', { filename }); 
    }
    
    async writeFile(filename, content) { 
        return this.sendCommand('files.write', { filename, content }); 
    }
    
    async deleteFile(filename) { 
        return this.sendCommand('files.delete', { filename }); 
    }
    
    async renameFile(old_filename, new_filename) { 
        return this.sendCommand('files.rename', { old_filename, new_filename }); 
    }
    
    async copyFile(source, destination) { 
        return this.sendCommand('files.copy', { source, destination }); 
    }
    
    async createDirectory(path) { 
        return this.sendCommand('files.createDir', { path }); 
    }
    
    async getFileInfo(filename) { 
        return this.sendCommand('files.info', { filename }); 
    }
    
    // === MIDI ===
    async importMidi(filename, content, base64 = true) { 
        return this.sendCommand('midi.import', { filename, content, base64 }); 
    }
    
    async exportMidi(midi_file_id, output_path = null) { 
        return this.sendCommand('midi.export', { midi_file_id, output_path }); 
    }
    
    async parseMidi(filename) { 
        return this.sendCommand('midi.parse', { filename }); 
    }
    
    async getMidiInfo(midi_file_id) { 
        return this.sendCommand('midi.info', { midi_file_id }); 
    }
    
    async loadMidiFile(filename) { 
        return this.sendCommand('midi.load', { filename }); 
    }
    
    async unloadMidiFile(midi_file_id) { 
        return this.sendCommand('midi.unload', { midi_file_id }); 
    }
    
    async getLoadedFiles() { 
        return this.sendCommand('midi.loaded'); 
    }
    
    // === PLAYBACK ===
    async playFile(filename, loop = false) { 
        return this.sendCommand('playback.play', { filename, loop }); 
    }
    
    async pausePlayback() { 
        return this.sendCommand('playback.pause'); 
    }
    
    async resumePlayback() { 
        return this.sendCommand('playback.resume'); 
    }
    
    async stopPlayback() { 
        return this.sendCommand('playback.stop'); 
    }
    
    async seekPlayback(position) { 
        return this.sendCommand('playback.seek', { position }); 
    }
    
    async setPlaybackLoop(enabled) { 
        return this.sendCommand('playback.setLoop', { enabled }); 
    }
    
    async setPlaybackTempo(tempo) { 
        return this.sendCommand('playback.setTempo', { tempo }); 
    }
    
    async getPlaybackState() { 
        return this.sendCommand('playback.getState'); 
    }
    
    async getPlaybackPosition() { 
        return this.sendCommand('playback.getPosition'); 
    }
    
    // === DEVICES ===
    async listDevices() { 
        return this.sendCommand('devices.list'); 
    }
    
    async scanDevices(full_scan = false) { 
        return this.sendCommand('devices.scan', { full_scan }); 
    }
    
    async getConnectedDevices() { 
        return this.sendCommand('devices.getConnected'); 
    }
    
    async connectDevice(device_id) { 
        return this.sendCommand('devices.connect', { device_id }); 
    }
    
    async disconnectDevice(device_id) { 
        return this.sendCommand('devices.disconnect', { device_id }); 
    }
    
    async getDeviceInfo(device_id) { 
        return this.sendCommand('devices.info', { device_id }); 
    }
    
    async setDeviceEnabled(device_id, enabled) { 
        return this.sendCommand('devices.setEnabled', { device_id, enabled }); 
    }
    
    async getHotPlugStatus() { 
        return this.sendCommand('devices.getHotPlugStatus'); 
    }
    
    async setHotPlug(enabled) { 
        return this.sendCommand('devices.setHotPlug', { enabled }); 
    }
    
    // === ROUTING ===
    async listRoutes() { 
        return this.sendCommand('routing.listRoutes'); 
    }
    
    async addRoute(source_id, destination_id, filter = null) { 
        return this.sendCommand('routing.addRoute', { source_id, destination_id, filter }); 
    }
    
    async removeRoute(routing_id) { 
        return this.sendCommand('routing.removeRoute', { routing_id }); 
    }
    
    async updateRoute(routing_id, filter) { 
        return this.sendCommand('routing.updateRoute', { routing_id, filter }); 
    }
    
    async clearRoutes() { 
        return this.sendCommand('routing.clearRoutes'); 
    }
    
    async enableRoute(routing_id) { 
        return this.sendCommand('routing.enableRoute', { routing_id }); 
    }
    
    async disableRoute(routing_id) { 
        return this.sendCommand('routing.disableRoute', { routing_id }); 
    }
    
    async getRouteInfo(routing_id) { 
        return this.sendCommand('routing.getInfo', { routing_id }); 
    }
    
    // === LATENCY ===
    async getLatency(instrument_id) { 
        return this.sendCommand('latency.get', { instrument_id }); 
    }
    
    async setLatency(instrument_id, offset_ms) { 
        return this.sendCommand('latency.set', { instrument_id, offset_ms }); 
    }
    
    async calibrateLatency(instrument_id) { 
        return this.sendCommand('latency.calibrate', { instrument_id }); 
    }
    
    async getAllLatencies() { 
        return this.sendCommand('latency.getAll'); 
    }
    
    async resetLatency(instrument_id) { 
        return this.sendCommand('latency.reset', { instrument_id }); 
    }
    
    // === PRESETS ===
    async listPresets() { 
        return this.sendCommand('preset.list'); 
    }
    
    async loadPreset(id) { 
        return this.sendCommand('preset.load', { id }); 
    }
    
    async savePreset(preset) { 
        return this.sendCommand('preset.save', { preset }); 
    }
    
    async deletePreset(id) { 
        return this.sendCommand('preset.delete', { id }); 
    }
    
    async exportPreset(id, filepath) { 
        return this.sendCommand('preset.export', { id, filepath }); 
    }
    
    // === PLAYLISTS ===
    async listPlaylists() { 
        return this.sendCommand('playlist.list'); 
    }
    
    async createPlaylist(name, description = '') { 
        return this.sendCommand('playlist.create', { name, description }); 
    }
    
    async getPlaylist(playlist_id) { 
        return this.sendCommand('playlist.get', { playlist_id }); 
    }
    
    async updatePlaylist(playlist_id, data) { 
        return this.sendCommand('playlist.update', { playlist_id, data }); 
    }
    
    async deletePlaylist(playlist_id) { 
        return this.sendCommand('playlist.delete', { playlist_id }); 
    }
    
    async addPlaylistItem(playlist_id, filename, order = null) { 
        return this.sendCommand('playlist.addItem', { playlist_id, filename, order }); 
    }
    
    async removePlaylistItem(playlist_id, item_id) { 
        return this.sendCommand('playlist.removeItem', { playlist_id, item_id }); 
    }
    
    async reorderPlaylist(playlist_id, item_id, new_order) { 
        return this.sendCommand('playlist.reorder', { playlist_id, item_id, new_order }); 
    }
    
    async setPlaylistLoop(playlist_id, enabled) { 
        return this.sendCommand('playlist.setLoop', { playlist_id, enabled }); 
    }
    
    // === SYSTEM ===
    async getVersion() { 
        return this.sendCommand('system.version'); 
    }
    
    async getInfo() { 
        return this.sendCommand('system.info'); 
    }
    
    async getUptime() { 
        return this.sendCommand('system.uptime'); 
    }
    
    async getMemory() { 
        return this.sendCommand('system.memory'); 
    }
    
    async getDisk() { 
        return this.sendCommand('system.disk'); 
    }
    
    async getCommands() { 
        return this.sendCommand('system.commands'); 
    }
    
    // === NETWORK ===
    async getNetworkStatus() { 
        return this.sendCommand('network.status'); 
    }
    
    async getNetworkInterfaces() { 
        return this.sendCommand('network.interfaces'); 
    }
    
    async getNetworkStats() { 
        return this.sendCommand('network.stats'); 
    }
    
    // === BLUETOOTH ===
    async getBluetoothStatus() { 
        return this.sendCommand('bluetooth.status'); 
    }
    
    async scanBluetooth() { 
        return this.sendCommand('bluetooth.scan'); 
    }
    
    async pairBluetooth(address) { 
        return this.sendCommand('bluetooth.pair', { address }); 
    }
    
    async unpairBluetooth(address) { 
        return this.sendCommand('bluetooth.unpair', { address }); 
    }
    
    async getPairedBluetooth() { 
        return this.sendCommand('bluetooth.paired'); 
    }
    
    async forgetBluetooth(address) { 
        return this.sendCommand('bluetooth.forget', { address }); 
    }
    
    async getBluetoothSignal(address) { 
        return this.sendCommand('bluetooth.signal', { address }); 
    }
    
    async setBluetoothConfig(settings) { 
        return this.sendCommand('bluetooth.config', { settings }); 
    }
    
    // === LOGGER ===
    async getLogs(level = 'info', limit = 100) { 
        return this.sendCommand('logger.getLogs', { level, limit }); 
    }
    
    async getLogLevel() { 
        return this.sendCommand('logger.getLevel'); 
    }
    
    async setLogLevel(level) { 
        return this.sendCommand('logger.setLevel', { level }); 
    }
    
    async clearLogs() { 
        return this.sendCommand('logger.clear'); 
    }
    
    async exportLogs(filename) { 
        return this.sendCommand('logger.export', { filename }); 
    }
    
    // ============================================================================
    // UTILS
    // ============================================================================
    
    flushMessageQueue() {
        if (this.messageQueue.length === 0) return;
        this.logger.info('BackendService', `Sending ${this.messageQueue.length} queued messages`);
        while (this.messageQueue.length > 0) {
            const message = this.messageQueue.shift();
            this.send(message);
        }
    }
    
    disconnect() {
        this.stopHeartbeat();
        this.reconnectionStopped = true;
        
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        
        for (const [requestId, pending] of this.pendingRequests.entries()) {
            clearTimeout(pending.timeoutTimer);
            pending.reject(new Error('Disconnected'));
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
    
    enableReconnection() {
        this.reconnectionStopped = false;
        this.offlineMode = false;
        this.reconnectAttempts = 0;
        
        if (!this.connected && !this.connecting) {
            this.logger.info('BackendService', 'Reconnection enabled, attempting to connect');
            this.connect();
        }
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
            pendingRequests: this.pendingRequests.size,
            url: this.config.url,
            lastActivityTime: this.lastActivityTime,
            timeSinceActivity: Date.now() - this.lastActivityTime,
            heartbeatFailures: this.heartbeatFailures,
            heartbeatPending: this.heartbeatPending
        };
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackendService;
}
window.BackendService = BackendService;
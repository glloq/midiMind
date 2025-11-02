// Version v4.0.2 - FORMAT REQUÊTE/RÉPONSE BACKEND COMPLET

class BackendService {
    constructor(url, eventBus, logger) {
        this.eventBus = eventBus;
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
            heartbeatInterval: 20000,
            heartbeatTimeout: 45000,
            maxReconnectAttempts: 5,
            defaultCommandTimeout: 5000
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
        
        this.requestId = 0;
        this.pendingRequests = new Map();
        
        this.logger.info('BackendService', 'Service initialized (v4.0.2 - FORMAT COMPLET)');
    }
    
    // Génère un UUID v4
    generateUUID() {
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
            const r = Math.random() * 16 | 0;
            const v = c === 'x' ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
    }
    
    // Génère timestamp ISO
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
                this.logger.error('BackendService', 'Connection error:', error);
                this.connecting = false;
                this.handleConnectionError(error.message);
                resolve(false);
            }
        });
    }
    
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
        
        this.startHeartbeat();
        this.flushMessageQueue();
    }
    
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
        
        if (!event.wasClean && !this.reconnectionStopped) {
            this.scheduleReconnect();
        } else if (this.reconnectionStopped) {
            this.enterOfflineMode();
        }
    }
    
    handleError(error) {
        this.logger.error('BackendService', 'WebSocket error:', error);
        this.eventBus.emit('backend:error', { error });
    }
    
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
                `✗ Heartbeat timeout! No activity since ${Math.round(timeSinceActivity/1000)}s (failure #${this.heartbeatFailures})`);
            
            if (this.heartbeatFailures >= 2) {
                this.logger.error('BackendService', '💀 Connection dead, forcing reconnect');
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
            this.logger.debug('BackendService', '💗 Sending heartbeat (devices.list)');
            
            const startTime = Date.now();
            await this.sendCommand('devices.list');
            const latency = Date.now() - startTime;
            
            this.heartbeatPending = false;
            this.heartbeatFailures = 0;
            
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
    
    stopHeartbeat() {
        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
            this.heartbeatPending = false;
        }
    }
    
    forceReconnect(reason) {
        this.logger.warn('BackendService', `Forcing reconnect: ${reason}`);
        
        this.stopHeartbeat();
        
        if (this.ws) {
            this.ws.close(1000, reason);
            this.ws = null;
        }
        
        this.connected = false;
        this.connecting = false;
        
        setTimeout(() => {
            this.connect();
        }, 1000);
    }
    
    /**
     * ✅ CORRIGÉ v4.0.2: Gère format backend complet
     */
    handleMessage(event) {
        try {
            const data = JSON.parse(event.data);
            
            this.logger.debug('BackendService', 'Message received:', data);
            
            this.lastActivityTime = Date.now();
            
            const messageType = data.type || 'unknown';
            const messageId = data.id;
            
            // Si type="response" => réponse à commande
            if (messageType === 'response' && messageId !== undefined) {
                const pending = this.pendingRequests.get(messageId);
                
                if (pending) {
                    this.pendingRequests.delete(messageId);
                    
                    const payload = data.payload || {};
                    
                    if (payload.error || payload.status === 'error') {
                        const error = new Error(payload.error || payload.message || 'Command failed');
                        error.code = payload.error_code || payload.code;
                        pending.reject(error);
                    } else {
                        pending.resolve(payload);
                    }
                    return;
                }
            }
            
            // Si type="event" => événement backend
            if (messageType === 'event' || data.event) {
                const eventName = data.event || data.payload?.event;
                if (eventName) {
                    this.eventBus.emit(`backend:event:${eventName}`, data);
                    
                    const deviceId = data.device_id || data.payload?.device_id;
                    if (deviceId !== undefined) {
                        this.eventBus.emit(`${eventName}:${deviceId}`, data);
                    }
                    
                    this.logger.debug('BackendService', `Event received: ${eventName}`, data);
                    return;
                }
            }
            
            this.eventBus.emit('backend:message', data);
            
        } catch (error) {
            this.logger.error('BackendService', 'Error parsing message:', error);
        }
    }
    
    send(data) {
        if (!this.connected || !this.ws || this.ws.readyState !== WebSocket.OPEN) {
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
     * ✅ CORRIGÉ v4.0.2: Format complet requête backend
     */
    async sendCommand(command, params = {}, timeout = null) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected()) {
                reject(new Error('Not connected to backend'));
                return;
            }
            
            const timeoutMs = timeout || this.config.defaultCommandTimeout || 5000;
            
            // ✅ Générer UUID pour ID
            const requestId = this.generateUUID();
            
            const timeoutTimer = setTimeout(() => {
                if (this.pendingRequests.has(requestId)) {
                    this.pendingRequests.delete(requestId);
                    reject(new Error(`Command timeout after ${timeoutMs}ms: ${command}`));
                }
            }, timeoutMs);
            
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
            
            // ✅ FORMAT COMPLET BACKEND
            const message = {
                type: "request",
                id: requestId,
                timestamp: this.generateTimestamp(),
                version: "1.0",
                payload: {
                    command: command,
                    params: params
                }
            };
            
            this.send(message);
            
            this.logger.debug('BackendService', `Sent command: ${command}`, message);
        });
    }
    
    async uploadFile(file, progressCallback = null) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            
            reader.onload = async () => {
                try {
                    const base64Data = btoa(
                        new Uint8Array(reader.result)
                            .reduce((data, byte) => data + String.fromCharCode(byte), '')
                    );
                    
                    const response = await this.sendCommand('files.upload', {
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
            
            reader.onerror = () => reject(new Error('File read error'));
            reader.readAsArrayBuffer(file);
        });
    }
    
    // === COMMANDES API (identique) ===
    async listDevices() { return this.sendCommand('devices.list'); }
    async getDevice(deviceId) { return this.sendCommand('devices.info', { device_id: deviceId }); }
    async enableDevice(deviceId) { return this.sendCommand('devices.enable', { device_id: deviceId }); }
    async disableDevice(deviceId) { return this.sendCommand('devices.disable', { device_id: deviceId }); }
    async connectDevice(deviceId) { return this.sendCommand('devices.connect', { device_id: deviceId }); }
    async disconnectDevice(deviceId) { return this.sendCommand('devices.disconnect', { device_id: deviceId }); }
    async setDeviceConfig(deviceId, config) { return this.sendCommand('devices.config.set', { device_id: deviceId, config }); }
    async getDeviceConfig(deviceId) { return this.sendCommand('devices.config.get', { device_id: deviceId }); }
    
    async addRoute(sourceId, destId, channel = null) {
        const params = { source_id: sourceId, dest_id: destId };
        if (channel !== null) params.channel = channel;
        return this.sendCommand('routing.add', params);
    }
    async removeRoute(routeId) { return this.sendCommand('routing.remove', { route_id: routeId }); }
    async listRoutes() { return this.sendCommand('routing.list'); }
    async updateRoute(routeId, config) { return this.sendCommand('routing.update', { route_id: routeId, config }); }
    async clearRoutes() { return this.sendCommand('routing.clear'); }
    async setRoutingMatrix(matrix) { return this.sendCommand('routing.setMatrix', { matrix }); }
    async getRoutingMatrix() { return this.sendCommand('routing.getMatrix'); }
    async enableChannel(routeId, channel) { return this.sendCommand('routing.channel.enable', { route_id: routeId, channel }); }
    async disableChannel(routeId, channel) { return this.sendCommand('routing.channel.disable', { route_id: routeId, channel }); }
    async setTranspose(routeId, semitones) { return this.sendCommand('routing.transpose', { route_id: routeId, semitones }); }
    async setVelocityCurve(routeId, curve) { return this.sendCommand('routing.velocityCurve', { route_id: routeId, curve }); }
    
    async play(fileId = null) { return this.sendCommand('playback.play', fileId ? { file_id: fileId } : {}); }
    async pause() { return this.sendCommand('playback.pause'); }
    async stop() { return this.sendCommand('playback.stop'); }
    async seek(position) { return this.sendCommand('playback.seek', { position }); }
    async setTempo(tempo) { return this.sendCommand('playback.setTempo', { tempo }); }
    async getStatus() { return this.sendCommand('playback.getStatus'); }
    async setLoop(enabled, startTick = null, endTick = null) {
        const params = { enabled };
        if (startTick !== null) params.start_tick = startTick;
        if (endTick !== null) params.end_tick = endTick;
        return this.sendCommand('playback.setLoop', params);
    }
    async setMetronome(enabled, volume = null) {
        const params = { enabled };
        if (volume !== null) params.volume = volume;
        return this.sendCommand('playback.setMetronome', params);
    }
    
    async listFiles(path = '/midi') { return this.sendCommand('files.list', { path }); }
    async loadFile(path) { return this.sendCommand('files.load', { path }); }
    async saveFile(path, data) { return this.sendCommand('files.save', { path, data }); }
    async deleteFile(path) { return this.sendCommand('files.delete', { path }); }
    async renameFile(oldPath, newPath) { return this.sendCommand('files.rename', { old_path: oldPath, new_path: newPath }); }
    async createDirectory(path) { return this.sendCommand('files.mkdir', { path }); }
    async getFileInfo(path) { return this.sendCommand('files.info', { path }); }
    async scanDirectory(path) { return this.sendCommand('files.scan', { path }); }
    
    async getEditorData(fileId) { return this.sendCommand('editor.getData', { file_id: fileId }); }
    async setEditorData(fileId, data) { return this.sendCommand('editor.setData', { file_id: fileId, data }); }
    async addNote(fileId, track, note) { return this.sendCommand('editor.addNote', { file_id: fileId, track, note }); }
    async removeNote(fileId, track, noteId) { return this.sendCommand('editor.removeNote', { file_id: fileId, track, note_id: noteId }); }
    async updateNote(fileId, track, noteId, changes) { return this.sendCommand('editor.updateNote', { file_id: fileId, track, note_id: noteId, changes }); }
    async addCC(fileId, track, cc) { return this.sendCommand('editor.addCC', { file_id: fileId, track, cc }); }
    async removeCC(fileId, track, ccId) { return this.sendCommand('editor.removeCC', { file_id: fileId, track, cc_id: ccId }); }
    async updateCC(fileId, track, ccId, changes) { return this.sendCommand('editor.updateCC', { file_id: fileId, track, cc_id: ccId, changes }); }
    async quantize(fileId, track, grid) { return this.sendCommand('editor.quantize', { file_id: fileId, track, grid }); }
    async transpose(fileId, track, semitones) { return this.sendCommand('editor.transpose', { file_id: fileId, track, semitones }); }
    
    async getVersion() { return this.sendCommand('system.version'); }
    async getInfo() { return this.sendCommand('system.info'); }
    async getUptime() { return this.sendCommand('system.uptime'); }
    async getMemory() { return this.sendCommand('system.memory'); }
    async getDisk() { return this.sendCommand('system.disk'); }
    async shutdown() { return this.sendCommand('system.shutdown'); }
    async restart() { return this.sendCommand('system.restart'); }
    async getLogs(lines = 100) { return this.sendCommand('system.logs', { lines }); }
    
    async measureLatency(deviceId) { return this.sendCommand('latency.measure', { device_id: deviceId }); }
    async setOffset(deviceId, offset) { return this.sendCommand('latency.setOffset', { device_id: deviceId, offset }); }
    async getOffset(deviceId) { return this.sendCommand('latency.getOffset', { device_id: deviceId }); }
    async enableLatencyCompensation() { return this.sendCommand('latency.enable'); }
    async disableLatencyCompensation() { return this.sendCommand('latency.disable'); }
    async setGlobalOffset(offset) { return this.sendCommand('latency.setGlobalOffset', { offset }); }
    async getGlobalOffset() { return this.sendCommand('latency.getGlobalOffset'); }
    async listInstruments() { return this.sendCommand('latency.listInstruments'); }
    
    async listPresets() { return this.sendCommand('preset.list'); }
    async loadPreset(name) { return this.sendCommand('preset.load', { name }); }
    async savePreset(name, config) { return this.sendCommand('preset.save', { name, config }); }
    async deletePreset(name) { return this.sendCommand('preset.delete', { name }); }
    async exportPreset(name) { return this.sendCommand('preset.export', { name }); }
    
    async convertMidi(data, format) { return this.sendCommand('midi.convert', { data, format }); }
    async loadMidi(path) { return this.sendCommand('midi.load', { path }); }
    async saveMidi(path, data) { return this.sendCommand('midi.save', { path, data }); }
    async importMidi(data) { return this.sendCommand('midi.import', { data }); }
    async addMidiRoute(sourceId, destId, channel = null) {
        const params = { source_id: sourceId, dest_id: destId };
        if (channel !== null) params.channel = channel;
        return this.sendCommand('midi.routing.add', params);
    }
    async listMidiRoutes() { return this.sendCommand('midi.routing.list'); }
    async updateMidiRoute(routeId, config) { return this.sendCommand('midi.routing.update', { route_id: routeId, config }); }
    async removeMidiRoute(routeId) { return this.sendCommand('midi.routing.remove', { route_id: routeId }); }
    async clearMidiRoutes() { return this.sendCommand('midi.routing.clear'); }
    async sendNoteOn(channel, note, velocity) { return this.sendCommand('midi.sendNoteOn', { channel, note, velocity }); }
    async sendNoteOff(channel, note) { return this.sendCommand('midi.sendNoteOff', { channel, note }); }
    
    async createPlaylist(name, items = []) { return this.sendCommand('playlist.create', { name, items }); }
    async deletePlaylist(playlistId) { return this.sendCommand('playlist.delete', { playlist_id: playlistId }); }
    async updatePlaylist(playlistId, config) { return this.sendCommand('playlist.update', { playlist_id: playlistId, config }); }
    async listPlaylists() { return this.sendCommand('playlist.list'); }
    async getPlaylist(playlistId) { return this.sendCommand('playlist.get', { playlist_id: playlistId }); }
    async addPlaylistItem(playlistId, item) { return this.sendCommand('playlist.addItem', { playlist_id: playlistId, item }); }
    async removePlaylistItem(playlistId, itemId) { return this.sendCommand('playlist.removeItem', { playlist_id: playlistId, item_id: itemId }); }
    async reorderPlaylist(playlistId, order) { return this.sendCommand('playlist.reorder', { playlist_id: playlistId, order }); }
    async setPlaylistLoop(playlistId, enabled) { return this.sendCommand('playlist.setLoop', { playlist_id: playlistId, enabled }); }
    
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
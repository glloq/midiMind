// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Chemin réel: frontend/js/services/BackendService.js
// Version: v4.2.2 - API COMPATIBLE (FULLY CORRECTED)
// Date: 2025-11-06
// ============================================================================
// CORRECTIONS v4.2.2 (COMPLÈTES):
// ✅ Tous les paramètres en snake_case
// ✅ midi.import: filename + content + base64
// ✅ Réponse: payload.data extraite correctement
// ✅ routing_id, device_id, midi_file_id, playlist_id
// ✅ routing.* : source_id + destination_id (pas route_id)
// ✅ latency.* : instrument_id + offset_ms (pas device_id/latency_ms)
// ✅ preset.save : { preset } (objet complet, pas name+data)
// ✅ preset.export : filepath (pas filename)
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
        
        this.logger.info('BackendService', 'Service initialized (v4.2.2 - FULLY CORRECTED)');
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
        
        this.logger.info('BackendService', '✔ Connected successfully');
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
                }
            }
            // Frontend format: { envelope: { type }, payload }
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
            this.logger.debug('BackendService', '✔ Command success [' + requestId + ']', data);
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
            this.logger.debug('BackendService', `✔ Command success [${requestId}]`, data);
            pending.resolve(data);
        } else {
            const error = message.payload?.error || 'Command failed';
            this.logger.error('BackendService', `✘ Command failed [${requestId}]:`, error);
            pending.reject(new Error(error));
        }
    }
    
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
     * ✔ v4.2.2: Format conforme API v4.2.2
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
            
            // FORMAT ENVELOPE/PAYLOAD selon doc
            const message = {
                envelope: {
                    id: requestId,
                    type: 'request',
                    timestamp: this.generateTimestamp(),
                    version: '1.0'
                },
                payload: {
                    id: requestId,  // ← CRITIQUE: id aussi dans payload
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
    // SHORTCUTS API v4.2.2 - ✅ TOUS CORRIGÉS ET CONFORMES
    // ============================================================================
    
    // === DEVICES ===
    async listDevices() { 
        return this.sendCommand('devices.list'); 
    }
    
    async scanDevices(full_scan = false) { 
        return this.sendCommand('devices.scan', { full_scan }); 
    }
    
    async getDevice(device_id) { 
        return this.sendCommand('devices.getInfo', { device_id }); 
    }
    
    async connectDevice(device_id) { 
        return this.sendCommand('devices.connect', { device_id }); 
    }
    
    async disconnectDevice(device_id) { 
        return this.sendCommand('devices.disconnect', { device_id }); 
    }
    
    async disconnectAllDevices() { 
        return this.sendCommand('devices.disconnectAll'); 
    }
    
    async getConnectedDevices() { 
        return this.sendCommand('devices.getConnected'); 
    }
    
    async startHotPlug() { 
        return this.sendCommand('devices.startHotPlug'); 
    }
    
    async stopHotPlug() { 
        return this.sendCommand('devices.stopHotPlug'); 
    }
    
    async getHotPlugStatus() { 
        return this.sendCommand('devices.getHotPlugStatus'); 
    }
    
    // === ROUTING - ✅ CORRIGÉ: source_id + destination_id (pas route_id)
    async listRoutes() { 
        return this.sendCommand('routing.listRoutes'); 
    }
    
    async addRoute(source_id, destination_id) { 
        return this.sendCommand('routing.addRoute', { source_id, destination_id }); 
    }
    
    async removeRoute(source_id, destination_id) { 
        return this.sendCommand('routing.removeRoute', { source_id, destination_id }); 
    }
    
    async enableRoute(source_id, destination_id) { 
        return this.sendCommand('routing.enableRoute', { source_id, destination_id }); 
    }
    
    async disableRoute(source_id, destination_id) { 
        return this.sendCommand('routing.disableRoute', { source_id, destination_id }); 
    }
    
    async clearRoutes() { 
        return this.sendCommand('routing.clearRoutes'); 
    }
    
    // === LATENCY - ✅ CORRIGÉ: instrument_id + offset_ms
    async enableLatency() { 
        return this.sendCommand('latency.enable'); 
    }
    
    async disableLatency() { 
        return this.sendCommand('latency.disable'); 
    }
    
    async setLatencyCompensation(instrument_id, offset_ms) { 
        return this.sendCommand('latency.setCompensation', { instrument_id, offset_ms }); 
    }
    
    async getLatencyCompensation(instrument_id) { 
        return this.sendCommand('latency.getCompensation', { instrument_id }); 
    }
    
    async setGlobalOffset(offset_ms) { 
        return this.sendCommand('latency.setGlobalOffset', { offset_ms }); 
    }
    
    async getGlobalOffset() { 
        return this.sendCommand('latency.getGlobalOffset'); 
    }
    
    async listInstruments() { 
        return this.sendCommand('latency.listInstruments'); 
    }
    
    // === PLAYBACK ===
    async listPlaybackFiles() { 
        return this.sendCommand('playback.listFiles'); 
    }
    
    async loadPlaybackFile(filename) { 
        return this.sendCommand('playback.load', { filename }); 
    }
    
    async play(file_id = null) { 
        return this.sendCommand('playback.play', file_id ? { file_id } : {}); 
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
    
    async setLoop(enabled) { 
        return this.sendCommand('playback.setLoop', { enabled }); 
    }
    
    async getStatus() { 
        return this.sendCommand('playback.getStatus'); 
    }
    
    async getPlaybackInfo() { 
        return this.sendCommand('playback.getInfo'); 
    }
    
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
    
    async fileExists(filename) { 
        return this.sendCommand('files.exists', { filename }); 
    }
    
    async getFileInfo(filename) { 
        return this.sendCommand('files.getInfo', { filename }); 
    }
    
    // === MIDI ===
    async loadMidi(id) { 
        return this.sendCommand('midi.load', { id }); 
    }
    
    async saveMidi(filename, midi_json) { 
        return this.sendCommand('midi.save', { filename, midi_json }); 
    }
    
    // ✔ CORRECTION MAJEURE: midi.import avec content + base64
    async importMidi(filename, content, base64 = true) { 
        return this.sendCommand('midi.import', { filename, content, base64 }); 
    }
    
    async convertMidi(json_data) { 
        return this.sendCommand('midi.convert', { json_data }); 
    }
    
    async sendNoteOn(device_id, note, velocity, channel = 0) { 
        return this.sendCommand('midi.sendNoteOn', { device_id, note, velocity, channel }); 
    }
    
    async sendNoteOff(device_id, note, channel = 0) { 
        return this.sendCommand('midi.sendNoteOff', { device_id, note, channel }); 
    }
    
    // ✔ MIDI ROUTING avec snake_case
    async addMidiRouting(midi_file_id, track_id, device_id, instrument_name = null, channel = 0, enabled = true) {
        return this.sendCommand('midi.routing.add', { 
            midi_file_id, 
            track_id, 
            device_id, 
            instrument_name, 
            channel, 
            enabled 
        });
    }
    
    async listMidiRouting(midi_file_id) {
        return this.sendCommand('midi.routing.list', { midi_file_id });
    }
    
    async updateMidiRouting(routing_id, updates) {
        return this.sendCommand('midi.routing.update', { routing_id, ...updates });
    }
    
    async removeMidiRouting(routing_id) {
        return this.sendCommand('midi.routing.remove', { routing_id });
    }
    
    async clearMidiRouting(midi_file_id) {
        return this.sendCommand('midi.routing.clear', { midi_file_id });
    }
    
    // === PRESETS - ✅ CORRIGÉ: preset (objet complet) + filepath
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
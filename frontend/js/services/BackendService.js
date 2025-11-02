// ============================================================================
// Fichier: frontend/js/services/BackendService.js
// Version: v4.0.0 - API CONFORME v4.2.2
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v4.0.0:
// ✅ Toutes les commandes API conformes à API_DOCUMENTATION_FRONTEND_CORRECTED.md
// ✅ Format: category.action (devices.list, playback.play, etc.)
// ✅ Paramètres: { params: {...} } encapsulés
// ✅ Réponse: { success: true/false, data: {...}, timestamp: ... }
// ✅ Événements: { event: "category:event", ... }
//
// CONSERVÉ DE v3.8.0:
// ✅ Heartbeat avec devices.list
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
        this.lastActivityTime = Date.now();
        this.heartbeatPending = false;
        this.heartbeatFailures = 0;
        
        // File d'attente des messages
        this.messageQueue = [];
        this.maxQueueSize = 100;
        
        // Compteur de requêtes pour les IDs
        this.requestId = 0;
        this.pendingRequests = new Map();
        
        this.logger.info('BackendService', 'Service initialized (v4.0.0 - API v4.2.2)');
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
                `✗ Max reconnection attempts (${this.config.maxReconnectAttempts}) reached`);
            
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
        
        this.logger.debug('BackendService', '💗 Heartbeat started');
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
                `✗ Heartbeat timeout! No activity since ${Math.round(timeSinceActivity/1000)}s (failure #${this.heartbeatFailures})`);
            
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
        
        // Envoyer un devices.list comme heartbeat (commande simple et rapide)
        try {
            this.heartbeatPending = true;
            this.logger.debug('BackendService', '💗 Sending heartbeat (devices.list)');
            
            const startTime = Date.now();
            await this.sendCommand('devices.list');
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
     * ✅ Gère les messages reçus (FORMAT API v4.2.2)
     */
    handleMessage(event) {
        try {
            const data = JSON.parse(event.data);
            
            // ✅ Toute réponse/event = activité backend
            this.lastActivityTime = Date.now();
            
            // ✅ Si message a un 'id', c'est une réponse à une commande
            if (data.id !== undefined && this.pendingRequests.has(data.id)) {
                const pending = this.pendingRequests.get(data.id);
                this.pendingRequests.delete(data.id);
                
                // Format API v4.2.2: { success: true/false, data: {...}, ... }
                if (data.success === true) {
                    // ✅ Résoudre avec data
                    pending.resolve(data.data || data);
                } else {
                    // ✅ Rejeter avec message d'erreur
                    const error = new Error(data.error || 'Command failed');
                    error.code = data.error_code;
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
     * ✅ Envoie une commande au backend et attend la réponse (FORMAT API v4.2.2)
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
            
            // ✅ FORMAT API v4.2.2 selon documentation
            // Requête: { id, command, params: { ... } }
            const message = {
                id: requestId,
                command: command,
                params: params  // ✅ Paramètres dans sous-objet "params"
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
                    
                    // ✅ Utiliser files.write pour l'upload
                    const response = await this.sendCommand('files.write', {
                        path: `/midi/${file.name}`,
                        content: base64Data,
                        encoding: 'base64'
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
    // MÉTHODES HELPER - API v4.2.2 CONFORME
    // ========================================================================
    
    // --- DEVICES (18 commandes) ---
    
    async listDevices() {
        return this.sendCommand('devices.list');
    }
    
    async scanDevices(fullScan = false) {
        return this.sendCommand('devices.scan', { full_scan: fullScan });
    }
    
    async connectDevice(deviceId) {
        return this.sendCommand('devices.connect', { device_id: deviceId });
    }
    
    async disconnectDevice(deviceId) {
        return this.sendCommand('devices.disconnect', { device_id: deviceId });
    }
    
    async disconnectAllDevices() {
        return this.sendCommand('devices.disconnectAll');
    }
    
    async getDeviceInfo(deviceId) {
        return this.sendCommand('devices.getInfo', { device_id: deviceId });
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
    
    // --- BLUETOOTH (8 commandes) ---
    
    async bluetoothConfig(config) {
        return this.sendCommand('bluetooth.config', config);
    }
    
    async bluetoothStatus() {
        return this.sendCommand('bluetooth.status');
    }
    
    async bluetoothScan(duration = 10) {
        return this.sendCommand('bluetooth.scan', { duration });
    }
    
    async bluetoothPair(deviceId) {
        return this.sendCommand('bluetooth.pair', { device_id: deviceId });
    }
    
    async bluetoothUnpair(deviceId) {
        return this.sendCommand('bluetooth.unpair', { device_id: deviceId });
    }
    
    async bluetoothPairedDevices() {
        return this.sendCommand('bluetooth.paired');
    }
    
    async bluetoothForget(deviceId) {
        return this.sendCommand('bluetooth.forget', { device_id: deviceId });
    }
    
    async bluetoothSignal(deviceId) {
        return this.sendCommand('bluetooth.signal', { device_id: deviceId });
    }
    
    // --- ROUTING (6 commandes) ---
    
    async addRoute(sourceId, destId, channel = null) {
        const params = {
            source_id: sourceId,
            dest_id: destId
        };
        if (channel !== null) {
            params.channel = channel;
        }
        return this.sendCommand('routing.addRoute', params);
    }
    
    async removeRoute(routeId) {
        return this.sendCommand('routing.removeRoute', { route_id: routeId });
    }
    
    async clearRoutes() {
        return this.sendCommand('routing.clearRoutes');
    }
    
    async listRoutes() {
        return this.sendCommand('routing.listRoutes');
    }
    
    async enableRoute(routeId) {
        return this.sendCommand('routing.enableRoute', { route_id: routeId });
    }
    
    async disableRoute(routeId) {
        return this.sendCommand('routing.disableRoute', { route_id: routeId });
    }
    
    // --- PLAYBACK (10 commandes) ---
    
    async loadFile(filePath) {
        return this.sendCommand('playback.load', { file_path: filePath });
    }
    
    async play() {
        return this.sendCommand('playback.play');
    }
    
    async pause() {
        return this.sendCommand('playback.pause');
    }
    
    async stop() {
        return this.sendCommand('playback.stop');
    }
    
    async getPlaybackStatus() {
        return this.sendCommand('playback.getStatus');
    }
    
    async seek(position) {
        return this.sendCommand('playback.seek', { position });
    }
    
    async setTempo(tempo) {
        return this.sendCommand('playback.setTempo', { tempo });
    }
    
    async setLoop(enabled, start = null, end = null) {
        const params = { enabled };
        if (start !== null) params.start = start;
        if (end !== null) params.end = end;
        return this.sendCommand('playback.setLoop', params);
    }
    
    async getPlaybackInfo() {
        return this.sendCommand('playback.getInfo');
    }
    
    async listPlaybackFiles() {
        return this.sendCommand('playback.listFiles');
    }
    
    // --- FILES (6 commandes) ---
    
    async listFiles(path = '/midi') {
        return this.sendCommand('files.list', { path });
    }
    
    async readFile(path) {
        return this.sendCommand('files.read', { path });
    }
    
    async writeFile(path, content) {
        return this.sendCommand('files.write', { path, content });
    }
    
    async deleteFile(path) {
        return this.sendCommand('files.delete', { path });
    }
    
    async fileExists(path) {
        return this.sendCommand('files.exists', { path });
    }
    
    async getFileInfo(path) {
        return this.sendCommand('files.getInfo', { path });
    }
    
    // --- SYSTEM (7 commandes) ---
    
    async ping() {
        return this.sendCommand('system.ping');
    }
    
    async getVersion() {
        return this.sendCommand('system.version');
    }
    
    async getSystemInfo() {
        return this.sendCommand('system.info');
    }
    
    async getUptime() {
        return this.sendCommand('system.uptime');
    }
    
    async getMemoryInfo() {
        return this.sendCommand('system.memory');
    }
    
    async getDiskInfo() {
        return this.sendCommand('system.disk');
    }
    
    async listCommands() {
        return this.sendCommand('system.commands');
    }
    
    // --- NETWORK (3 commandes) ---
    
    async getNetworkStatus() {
        return this.sendCommand('network.status');
    }
    
    async getNetworkInterfaces() {
        return this.sendCommand('network.interfaces');
    }
    
    async getNetworkStats() {
        return this.sendCommand('network.stats');
    }
    
    // --- LOGGER (5 commandes) ---
    
    async setLogLevel(level) {
        return this.sendCommand('logger.setLevel', { level });
    }
    
    async getLogLevel() {
        return this.sendCommand('logger.getLevel');
    }
    
    async getLogs(count = 100) {
        return this.sendCommand('logger.getLogs', { count });
    }
    
    async clearLogs() {
        return this.sendCommand('logger.clear');
    }
    
    async exportLogs() {
        return this.sendCommand('logger.export');
    }
    
    // --- LATENCY (7 commandes) ---
    
    async setLatencyCompensation(instrumentId, latency) {
        return this.sendCommand('latency.setCompensation', { 
            instrument_id: instrumentId, 
            latency 
        });
    }
    
    async getLatencyCompensation(instrumentId) {
        return this.sendCommand('latency.getCompensation', { 
            instrument_id: instrumentId 
        });
    }
    
    async enableLatencyCompensation() {
        return this.sendCommand('latency.enable');
    }
    
    async disableLatencyCompensation() {
        return this.sendCommand('latency.disable');
    }
    
    async setGlobalOffset(offset) {
        return this.sendCommand('latency.setGlobalOffset', { offset });
    }
    
    async getGlobalOffset() {
        return this.sendCommand('latency.getGlobalOffset');
    }
    
    async listInstruments() {
        return this.sendCommand('latency.listInstruments');
    }
    
    // --- PRESETS (5 commandes) ---
    
    async listPresets() {
        return this.sendCommand('preset.list');
    }
    
    async loadPreset(name) {
        return this.sendCommand('preset.load', { name });
    }
    
    async savePreset(name, config) {
        return this.sendCommand('preset.save', { name, config });
    }
    
    async deletePreset(name) {
        return this.sendCommand('preset.delete', { name });
    }
    
    async exportPreset(name) {
        return this.sendCommand('preset.export', { name });
    }
    
    // --- MIDI (11 commandes) ---
    
    async convertMidi(data, format) {
        return this.sendCommand('midi.convert', { data, format });
    }
    
    async loadMidi(path) {
        return this.sendCommand('midi.load', { path });
    }
    
    async saveMidi(path, data) {
        return this.sendCommand('midi.save', { path, data });
    }
    
    async importMidi(data) {
        return this.sendCommand('midi.import', { data });
    }
    
    async addMidiRoute(sourceId, destId, channel = null) {
        const params = { source_id: sourceId, dest_id: destId };
        if (channel !== null) params.channel = channel;
        return this.sendCommand('midi.routing.add', params);
    }
    
    async listMidiRoutes() {
        return this.sendCommand('midi.routing.list');
    }
    
    async updateMidiRoute(routeId, config) {
        return this.sendCommand('midi.routing.update', { route_id: routeId, config });
    }
    
    async removeMidiRoute(routeId) {
        return this.sendCommand('midi.routing.remove', { route_id: routeId });
    }
    
    async clearMidiRoutes() {
        return this.sendCommand('midi.routing.clear');
    }
    
    async sendNoteOn(channel, note, velocity) {
        return this.sendCommand('midi.sendNoteOn', { channel, note, velocity });
    }
    
    async sendNoteOff(channel, note) {
        return this.sendCommand('midi.sendNoteOff', { channel, note });
    }
    
    // --- PLAYLISTS (9 commandes) ---
    
    async createPlaylist(name, items = []) {
        return this.sendCommand('playlist.create', { name, items });
    }
    
    async deletePlaylist(playlistId) {
        return this.sendCommand('playlist.delete', { playlist_id: playlistId });
    }
    
    async updatePlaylist(playlistId, config) {
        return this.sendCommand('playlist.update', { playlist_id: playlistId, config });
    }
    
    async listPlaylists() {
        return this.sendCommand('playlist.list');
    }
    
    async getPlaylist(playlistId) {
        return this.sendCommand('playlist.get', { playlist_id: playlistId });
    }
    
    async addPlaylistItem(playlistId, item) {
        return this.sendCommand('playlist.addItem', { playlist_id: playlistId, item });
    }
    
    async removePlaylistItem(playlistId, itemId) {
        return this.sendCommand('playlist.removeItem', { 
            playlist_id: playlistId, 
            item_id: itemId 
        });
    }
    
    async reorderPlaylist(playlistId, order) {
        return this.sendCommand('playlist.reorder', { playlist_id: playlistId, order });
    }
    
    async setPlaylistLoop(playlistId, enabled) {
        return this.sendCommand('playlist.setLoop', { 
            playlist_id: playlistId, 
            enabled 
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
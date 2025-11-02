// ============================================================================
// Fichier: frontend/js/controllers/InstrumentController.js
// Version: 4.3.0 - API CONFORMITÉ DOCUMENTATION_FRONTEND
// Date: 2025-11-01
// ============================================================================
// Modifications:
//   - Support devices.getInfo, devices.getConnected, devices.disconnectAll
//   - Gestion hot-plug (startHotPlug, stopHotPlug, getHotPlugStatus)
//   - Gestion format API v4.2.1 (request/response standardisé)
//   - Détection automatique périphériques (hot-plug monitoring)
// ✔ CORRECTIONS v4.0.0: Compatibilité API v4.0.0
//   - Informations détaillées par périphérique
//   - Gestion codes erreur API
//   - Filtrage périphériques connectés/disponibles
// ============================================================================

class InstrumentController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.logger = window.logger || console;
        this.model = models.instrument;
        this.view = views.instrument;
        this.backend = window.app?.services?.backend || window.backendService;
        
        // État des périphériques
        this.devices = new Map(); // device_id -> device info
        this.connectedDevices = new Set();
        
        // Hot-plug monitoring
        this.hotPlugEnabled = false;
        this.hotPlugInterval = 2000; // ms
        this.hotPlugTimer = null;
        
        // Cache des infos périphériques
        this.deviceInfoCache = new Map();
        this.deviceInfoCacheTTL = 30000; // 30 secondes
        
        // État de scan
        this.isScanning = false;
        this.lastScanTime = null;
        
        this._fullyInitialized = true;
        this.bindEvents();
        // ✔ REMOVED: this.initialize() - BaseController calls it via autoInitialize
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    bindEvents() {
        // Backend
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        
        // Périphériques (événements du backend)
        this.eventBus.on('backend:device:connected', (data) => this.handleDeviceConnected(data));
        this.eventBus.on('backend:device:disconnected', (data) => this.handleDeviceDisconnected(data));
        this.eventBus.on('backend:device:discovered', (data) => this.handleDeviceDiscovered(data));
        this.eventBus.on('backend:device:error', (data) => this.handleDeviceError(data));
        
        // Navigation
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'instruments') {
                this.onInstrumentsPageActive();
            } else {
                this.onInstrumentsPageInactive();
            }
        });
        
        // Demandes de refresh
        this.eventBus.on('instruments:request_refresh', () => this.refreshDeviceList());
        
        if (this.logger?.info) {
            this.logger.info('InstrumentController', '✔️ Events bound');
        }
    }

    async initialize() {
        if (this.logger?.info) {
            this.logger.info('InstrumentController', 'Initializing...');
        }

        // Si backend connecté, charger périphériques
        if (this.backend?.isConnected()) {
            await this.onBackendConnected();
        }
    }


    async onBackendConnected() {
        if (this.logger?.info) {
            this.logger.info('InstrumentController', '✅ Backend connected');
        }

        // Non-bloquant - continuer même si timeout
        this.scanDevices().catch(err => {
            this.log('warn', 'InstrumentController', 'Initial scan failed:', err.message);
        });
        
        this.loadConnectedDevices().catch(err => {
            this.log('warn', 'InstrumentController', 'Load devices failed:', err.message);
        });
        
        this.getHotPlugStatus().then(status => {
            if (status?.enabled) {
                this.hotPlugEnabled = true;
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', '✅ Hot-plug enabled');
                }
            }
        }).catch(err => {
            this.log('warn', 'InstrumentController', 'Hot-plug status failed:', err.message);
        });
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'Initialization failed:', error);
            }
        }
    }

    onBackendDisconnected() {
        if (this.logger?.warn) {
            this.logger.warn('InstrumentController', '⚠️ Backend disconnected');
        }
        
        // Arrêter hot-plug local (backend gérera le sien)
        this.stopHotPlugMonitoring();
        
        // Marquer tous comme déconnectés
        this.connectedDevices.clear();
        
        this.refreshView();
    }

    // ========================================================================
    // SCAN ET LISTE PÉRIPHÉRIQUES
    // ========================================================================

    /**
     * Scan tous les périphériques MIDI disponibles
     * @param {boolean} fullScan - Scan complet ou rapide
     * @returns {Promise<Array>}
     */
    async scanDevices(fullScan = false) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        if (this.isScanning) {
            if (this.logger?.debug) {
                this.logger.debug('InstrumentController', 'Scan already in progress');
            }
            return [];
        }

        this.isScanning = true;
        
        try {
            const response = await this.backend.sendCommand('devices.list', {
                full_scan: fullScan
            });
            
            if (response.success) {
                const devices = response.data?.devices || [];
                
                // Mettre à jour le cache
                devices.forEach(device => {
                    this.devices.set(device.id, device);
                });
                
                this.lastScanTime = Date.now();
                
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', 
                        `✔️ Scan complete: ${devices.length} devices found`);
                }
                
                // Émettre événement
                this.eventBus.emit('instruments:scan_complete', { 
                    devices, 
                    count: devices.length 
                });
                
                // Rafraîchir la vue
                this.refreshView();
                
                return devices;
            } else {
                throw new Error(response.error_message || 'Scan failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'scanDevices failed:', error);
            }
            throw error;
        } finally {
            this.isScanning = false;
        }
    }

    /**
     * Liste tous les périphériques disponibles
     * @returns {Promise<Array>}
     */
    async listDevices() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('devices.list', {});
            
            if (response.success) {
                const devices = response.data?.devices || [];
                
                // Mettre à jour le cache
                devices.forEach(device => {
                    this.devices.set(device.id, device);
                });
                
                if (this.logger?.debug) {
                    this.logger.debug('InstrumentController', 
                        `✔️ Listed ${devices.length} devices`);
                }
                
                return devices;
            } else {
                throw new Error(response.error_message || 'List devices failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'listDevices failed:', error);
            }
            throw error;
        }
    }

    /**
     * Obtient uniquement les périphériques connectés
     * @returns {Promise<Array>}
     */
    async getConnectedDevices() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('devices.list', {});
            
            if (response.success) {
                const devices = response.data?.devices || [];
                
                // Mettre à jour la liste des connectés
                this.connectedDevices.clear();
                devices.forEach(device => {
                    this.connectedDevices.add(device.id);
                    this.devices.set(device.id, device);
                });
                
                if (this.logger?.debug) {
                    this.logger.debug('InstrumentController', 
                        `✔️ ${devices.length} devices connected`);
                }
                
                return devices;
            } else {
                throw new Error(response.error_message || 'Get connected devices failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'getConnectedDevices failed:', error);
            }
            throw error;
        }
    }

    /**
     * Obtient les informations détaillées d'un périphérique
     * @param {string} deviceId
     * @param {boolean} useCache - Utiliser cache si disponible
     * @returns {Promise<Object>}
     */
    async getDeviceInfo(deviceId, useCache = true) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        // Vérifier cache
        if (useCache) {
            const cached = this.deviceInfoCache.get(deviceId);
            if (cached && (Date.now() - cached.timestamp < this.deviceInfoCacheTTL)) {
                return cached.info;
            }
        }

        try {
            const response = await this.backend.sendCommand('devices.getInfo', {
                device_id: deviceId
            });
            
            if (response.success) {
                const info = response.data;
                
                // Mettre en cache
                this.deviceInfoCache.set(deviceId, {
                    info: info,
                    timestamp: Date.now()
                });
                
                // Mettre à jour le device dans la map
                if (this.devices.has(deviceId)) {
                    this.devices.set(deviceId, { ...this.devices.get(deviceId), ...info });
                }
                
                return info;
            } else {
                throw new Error(response.error_message || 'Get device info failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 
                    `getDeviceInfo failed for ${deviceId}:`, error);
            }
            throw error;
        }
    }

    // ========================================================================
    // CONNEXION / DÉCONNEXION
    // ========================================================================

    /**
     * Connecte un périphérique MIDI
     * @param {string} deviceId
     * @returns {Promise<boolean>}
     */
    async connectDevice(deviceId) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('devices.connect', {
                device_id: deviceId
            });
            
            if (response.success) {
                this.connectedDevices.add(deviceId);
                
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', `✔️ Device connected: ${deviceId}`);
                }
                
                // Émettre événement
                this.eventBus.emit('instrument:connected', { deviceId });
                
                // Rafraîchir la vue
                this.refreshView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Connection failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 
                    `connectDevice failed for ${deviceId}:`, error);
            }
            
            // Notification d'erreur
            this.eventBus.emit('notification:show', {
                message: `Failed to connect device: ${error.message}`,
                type: 'error',
                duration: 5000
            });
            
            throw error;
        }
    }

    /**
     * Déconnecte un périphérique MIDI
     * @param {string} deviceId
     * @returns {Promise<boolean>}
     */
    async disconnectDevice(deviceId) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('devices.disconnect', {
                device_id: deviceId
            });
            
            if (response.success) {
                this.connectedDevices.delete(deviceId);
                
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', 
                        `✔️ Device disconnected: ${deviceId}`);
                }
                
                // Émettre événement
                this.eventBus.emit('instrument:disconnected', { deviceId });
                
                // Rafraîchir la vue
                this.refreshView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Disconnection failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 
                    `disconnectDevice failed for ${deviceId}:`, error);
            }
            
            // Notification d'erreur
            this.eventBus.emit('notification:show', {
                message: `Failed to disconnect device: ${error.message}`,
                type: 'error',
                duration: 5000
            });
            
            throw error;
        }
    }

    /**
     * Déconnecte tous les périphériques
     * @returns {Promise<boolean>}
     */
    async disconnectAllDevices() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('devices.disconnectAll', {});
            
            if (response.success) {
                this.connectedDevices.clear();
                
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', '✔️ All devices disconnected');
                }
                
                // Émettre événement
                this.eventBus.emit('instruments:all_disconnected');
                
                // Notification
                this.eventBus.emit('notification:show', {
                    message: 'All devices disconnected',
                    type: 'success',
                    duration: 3000
                });
                
                // Rafraîchir la vue
                this.refreshView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Disconnect all failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'disconnectAllDevices failed:', error);
            }
            
            // Notification d'erreur
            this.eventBus.emit('notification:show', {
                message: `Failed to disconnect all devices: ${error.message}`,
                type: 'error',
                duration: 5000
            });
            
            throw error;
        }
    }

    // ========================================================================
    // HOT-PLUG (DÉTECTION AUTOMATIQUE)
    // ========================================================================

    /**
     * Démarre la surveillance hot-plug
     * @param {number} intervalMs - Intervalle de scan en ms (défaut: 2000)
     * @returns {Promise<boolean>}
     */
    async startHotPlug(intervalMs = 2000) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('devices.startHotPlug', {
                interval_ms: intervalMs
            });
            
            if (response.success) {
                this.hotPlugEnabled = true;
                this.hotPlugInterval = intervalMs;
                
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', 
                        `✔️ Hot-plug started (interval: ${intervalMs}ms)`);
                }
                
                // Émettre événement
                this.eventBus.emit('instruments:hotplug_started', { intervalMs });
                
                // Notification
                this.eventBus.emit('notification:show', {
                    message: 'Auto-detection of devices enabled',
                    type: 'success',
                    duration: 3000
                });
                
                // Rafraîchir la vue
                this.refreshView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Start hot-plug failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'startHotPlug failed:', error);
            }
            throw error;
        }
    }

    /**
     * Arrête la surveillance hot-plug
     * @returns {Promise<boolean>}
     */
    async stopHotPlug() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('devices.stopHotPlug', {});
            
            if (response.success) {
                this.hotPlugEnabled = false;
                
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', '✔️ Hot-plug stopped');
                }
                
                // Émettre événement
                this.eventBus.emit('instruments:hotplug_stopped');
                
                // Notification
                this.eventBus.emit('notification:show', {
                    message: 'Auto-detection of devices disabled',
                    type: 'info',
                    duration: 3000
                });
                
                // Rafraîchir la vue
                this.refreshView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Stop hot-plug failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'stopHotPlug failed:', error);
            }
            throw error;
        }
    }

    /**
     * Obtient le statut du hot-plug
     * @returns {Promise<Object>}
     */
    async getHotPlugStatus() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('devices.getHotPlugStatus', {});
            
            if (response.success) {
                const status = response.data;
                this.hotPlugEnabled = status.enabled || false;
                this.hotPlugInterval = status.interval_ms || 2000;
                
                return status;
            } else {
                throw new Error(response.error_message || 'Get hot-plug status failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'getHotPlugStatus failed:', error);
            }
            throw error;
        }
    }

    /**
     * Toggle hot-plug on/off
     * @returns {Promise<boolean>}
     */
    async toggleHotPlug() {
        if (this.hotPlugEnabled) {
            return await this.stopHotPlug();
        } else {
            return await this.startHotPlug(this.hotPlugInterval);
        }
    }

    /**
     * Monitoring hot-plug local (fallback si backend ne supporte pas)
     */
    startHotPlugMonitoring() {
        this.stopHotPlugMonitoring();
        
        this.hotPlugTimer = setInterval(async () => {
            try {
                await this.scanDevices(false);
            } catch (error) {
                if (this.logger?.debug) {
                    this.logger.debug('InstrumentController', 
                        'Hot-plug scan failed:', error);
                }
            }
        }, this.hotPlugInterval);
        
        if (this.logger?.debug) {
            this.logger.debug('InstrumentController', '✔️ Local hot-plug monitoring started');
        }
    }

    stopHotPlugMonitoring() {
        if (this.hotPlugTimer) {
            clearInterval(this.hotPlugTimer);
            this.hotPlugTimer = null;
        }
    }

    // ========================================================================
    // ÉVÉNEMENTS BACKEND
    // ========================================================================

    handleDeviceConnected(data) {
        const deviceId = data.device_id || data.deviceId;
        
        if (deviceId) {
            this.connectedDevices.add(deviceId);
            
            if (this.logger?.info) {
                this.logger.info('InstrumentController', 
                    `🔌✓ Device connected: ${deviceId}`);
            }
            
            // Invalider cache info
            this.deviceInfoCache.delete(deviceId);
            
            // Rafraîchir la vue
            this.refreshView();
            
            // Notification
            this.eventBus.emit('notification:show', {
                message: `Device connected: ${data.name || deviceId}`,
                type: 'success',
                duration: 3000
            });
        }
    }

    handleDeviceDisconnected(data) {
        const deviceId = data.device_id || data.deviceId;
        
        if (deviceId) {
            this.connectedDevices.delete(deviceId);
            
            if (this.logger?.info) {
                this.logger.info('InstrumentController', 
                    `🔌✗ Device disconnected: ${deviceId}`);
            }
            
            // Rafraîchir la vue
            this.refreshView();
            
            // Notification
            this.eventBus.emit('notification:show', {
                message: `Device disconnected: ${data.name || deviceId}`,
                type: 'warning',
                duration: 3000
            });
        }
    }

    handleDeviceDiscovered(data) {
        const device = data.device || data;
        
        if (device.id) {
            this.devices.set(device.id, device);
            
            if (this.logger?.debug) {
                this.logger.debug('InstrumentController', 
                    `🔍 Device discovered: ${device.id}`);
            }
            
            // Rafraîchir la vue
            this.refreshView();
        }
    }

    handleDeviceError(data) {
        if (this.logger?.error) {
            this.logger.error('InstrumentController', 
                'Device error:', data.error || data.message);
        }
        
        // Notification
        this.eventBus.emit('notification:show', {
            message: `Device error: ${data.error || data.message}`,
            type: 'error',
            duration: 5000
        });
    }

    // ========================================================================
    // GESTION VUE
    // ========================================================================

    refreshView() {
        if (!this.view || typeof this.view.render !== 'function') {
            return;
        }

        const data = {
            devices: Array.from(this.devices.values()),
            connectedDevices: Array.from(this.connectedDevices),
            hotPlugEnabled: this.hotPlugEnabled,
            hotPlugInterval: this.hotPlugInterval,
            isScanning: this.isScanning,
            lastScanTime: this.lastScanTime,
            backendConnected: this.backend?.isConnected() || false
        };

        this.view.render(data);
    }

    async refreshDeviceList() {
        try {
            await this.scanDevices(false);
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'Refresh failed:', error);
            }
        }
    }

    // ========================================================================
    // HELPERS / UTILITAIRES
    // ========================================================================

    /**
     * Charge la liste des périphériques connectés
     */
    async loadConnectedDevices() {
        try {
            await this.getConnectedDevices();
        } catch (error) {
            if (this.logger?.debug) {
                this.logger.debug('InstrumentController', 
                    'loadConnectedDevices failed:', error);
            }
        }
    }

    /**
     * Vérifie si un périphérique est connecté
     * @param {string} deviceId
     * @returns {boolean}
     */
    isDeviceConnected(deviceId) {
        return this.connectedDevices.has(deviceId);
    }

    /**
     * Obtient un périphérique du cache
     * @param {string} deviceId
     * @returns {Object|null}
     */
    getDevice(deviceId) {
        return this.devices.get(deviceId) || null;
    }

    /**
     * Obtient tous les périphériques disponibles
     * @returns {Array}
     */
    getAllDevices() {
        return Array.from(this.devices.values());
    }

    /**
     * Obtient tous les périphériques connectés (du cache local)
     * @returns {Array}
     */
    getConnectedDevicesFromCache() {
        return Array.from(this.devices.values()).filter(device => 
            this.connectedDevices.has(device.id)
        );
    }

    /**
     * Obtient tous les périphériques disponibles mais non connectés
     * @returns {Array}
     */
    getAvailableDevices() {
        return Array.from(this.devices.values()).filter(device => 
            !this.connectedDevices.has(device.id) && device.available !== false
        );
    }

    /**
     * Filtre les périphériques par type
     * @param {number} type - Type de périphérique
     * @returns {Array}
     */
    getDevicesByType(type) {
        return Array.from(this.devices.values()).filter(device => 
            device.type === type
        );
    }

    /**
     * Recherche un périphérique par nom
     * @param {string} query - Terme de recherche
     * @returns {Array}
     */
    searchDevices(query) {
        const lowerQuery = query.toLowerCase();
        return Array.from(this.devices.values()).filter(device => 
            device.name?.toLowerCase().includes(lowerQuery) ||
            device.id?.toLowerCase().includes(lowerQuery)
        );
    }

    /**
     * Nettoie le cache des infos périphériques
     */
    clearDeviceInfoCache() {
        this.deviceInfoCache.clear();
        
        if (this.logger?.debug) {
            this.logger.debug('InstrumentController', '✔️ Device info cache cleared');
        }
    }

    /**
     * Nettoie les périphériques obsolètes du cache
     * @param {number} maxAge - Age max en ms (défaut: 5 minutes)
     */
    cleanupDeviceCache(maxAge = 300000) {
        const now = Date.now();
        let cleaned = 0;

        for (const [deviceId, cachedInfo] of this.deviceInfoCache.entries()) {
            if (now - cachedInfo.timestamp > maxAge) {
                this.deviceInfoCache.delete(deviceId);
                cleaned++;
            }
        }

        if (cleaned > 0 && this.logger?.debug) {
            this.logger.debug('InstrumentController', 
                `✔️ Cleaned ${cleaned} expired cache entries`);
        }
    }

    // ========================================================================
    // ÉVÉNEMENTS PAGE
    // ========================================================================

    onInstrumentsPageActive() {
        if (this.logger?.debug) {
            this.logger.debug('InstrumentController', 'Instruments page active');
        }

        // Rafraîchir la liste
        this.refreshDeviceList();
        
        // Vérifier statut hot-plug
        if (this.backend?.isConnected()) {
            this.getHotPlugStatus().catch(() => {});
        }
    }

    onInstrumentsPageInactive() {
        if (this.logger?.debug) {
            this.logger.debug('InstrumentController', 'Instruments page inactive');
        }
    }

    // ========================================================================
    // LEGACY / COMPATIBILITÉ
    // ========================================================================

    /**
     * Scan périphériques (alias pour compatibilité)
     */
    async scan() {
        return await this.scanDevices(false);
    }

    /**
     * Connecte un instrument (alias pour compatibilité)
     */
    async connect(deviceId) {
        return await this.connectDevice(deviceId);
    }

    /**
     * Déconnecte un instrument (alias pour compatibilité)
     */
    async disconnect(deviceId) {
        return await this.disconnectDevice(deviceId);
    }

    // ========================================================================
    // STATISTIQUES
    // ========================================================================

    /**
     * Obtient les statistiques des périphériques
     * @returns {Object}
     */
    getStats() {
        return {
            totalDevices: this.devices.size,
            connectedDevices: this.connectedDevices.size,
            availableDevices: this.getAvailableDevices().length,
            hotPlugEnabled: this.hotPlugEnabled,
            hotPlugInterval: this.hotPlugInterval,
            lastScanTime: this.lastScanTime,
            cacheSize: this.deviceInfoCache.size
        };
    }

    // ========================================================================
    // DESTRUCTION
    // ========================================================================

    destroy() {
        // Arrêter hot-plug
        this.stopHotPlugMonitoring();
        
        // Nettoyer caches
        this.devices.clear();
        this.connectedDevices.clear();
        this.deviceInfoCache.clear();
        
        if (this.logger?.info) {
            this.logger.info('InstrumentController', 'Destroyed');
        }
    }
    
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        }
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = InstrumentController;
}

if (typeof window !== 'undefined') {
    window.InstrumentController = InstrumentController;
}
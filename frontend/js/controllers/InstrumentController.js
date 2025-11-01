// ============================================================================
// Fichier: frontend/js/controllers/InstrumentController.js
// Version: 4.3.0 - API CONFORMITÉ DOCUMENTATION_FRONTEND
// Date: 2025-11-01
// ============================================================================
// Modifications:
//   - Support devices.getInfo, devices.getConnected, devices.disconnectAll
//   - Gestion hot-plug (startHotPlug, stopHotPlug, getHotPlugStatus)
//   - Gestion format API v4.2.1 (request/response standardisÃƒÂ©)
//   - DÃƒÂ©tection automatique pÃƒÂ©riphÃƒÂ©riques (hot-plug monitoring)
//   - Informations dÃƒÂ©taillÃƒÂ©es par pÃƒÂ©riphÃƒÂ©rique
//   - Gestion codes erreur API
//   - Filtrage pÃƒÂ©riphÃƒÂ©riques connectÃƒÂ©s/disponibles
// ============================================================================

class InstrumentController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.logger = window.logger || console;
        this.model = models.instrument;
        this.view = views.instrument;
        this.backend = window.app?.services?.backend || window.backendService;
        
        // Ãƒâ€°tat des pÃƒÂ©riphÃƒÂ©riques
        this.devices = new Map(); // device_id -> device info
        this.connectedDevices = new Set();
        
        // Hot-plug monitoring
        this.hotPlugEnabled = false;
        this.hotPlugInterval = 2000; // ms
        this.hotPlugTimer = null;
        
        // Cache des infos pÃƒÂ©riphÃƒÂ©riques
        this.deviceInfoCache = new Map();
        this.deviceInfoCacheTTL = 30000; // 30 secondes
        
        // Ãƒâ€°tat de scan
        this.isScanning = false;
        this.lastScanTime = null;
        
        this._fullyInitialized = true;
        this.bindEvents();
        // âœ… REMOVED: this.initialize() - BaseController calls it via autoInitialize
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    bindEvents() {
        // Backend
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        
        // PÃƒÂ©riphÃƒÂ©riques (ÃƒÂ©vÃƒÂ©nements du backend)
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
            this.logger.info('InstrumentController', 'Ã¢Å“â€œ Events bound');
        }
    }

    async initialize() {
        if (this.logger?.info) {
            this.logger.info('InstrumentController', 'Initializing...');
        }

        // Si backend connectÃƒÂ©, charger pÃƒÂ©riphÃƒÂ©riques
        if (this.backend?.isConnected()) {
            await this.onBackendConnected();
        }
    }

    async onBackendConnected() {
        if (this.logger?.info) {
            this.logger.info('InstrumentController', 'Ã¢Å“â€¦ Backend connected');
        }

        try {
            // Charger liste pÃƒÂ©riphÃƒÂ©riques
            await this.scanDevices();
            
            // Charger pÃƒÂ©riphÃƒÂ©riques connectÃƒÂ©s
            await this.loadConnectedDevices();
            
            // DÃƒÂ©marrer hot-plug si configurÃƒÂ©
            const hotPlugStatus = await this.getHotPlugStatus();
            if (hotPlugStatus?.enabled) {
                this.hotPlugEnabled = true;
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', 'Ã¢Å“â€œ Hot-plug already enabled');
                }
            }
            
            // RafraÃƒÂ®chir la vue
            this.refreshView();
            
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('InstrumentController', 'Initialization failed:', error);
            }
        }
    }

    onBackendDisconnected() {
        if (this.logger?.warn) {
            this.logger.warn('InstrumentController', 'Ã°Å¸â€Â´ Backend disconnected');
        }
        
        // ArrÃƒÂªter hot-plug local (backend gÃƒÂ©rera le sien)
        this.stopHotPlugMonitoring();
        
        // Marquer tous comme dÃƒÂ©connectÃƒÂ©s
        this.connectedDevices.clear();
        
        this.refreshView();
    }

    // ========================================================================
    // SCAN ET LISTE PÃƒâ€°RIPHÃƒâ€°RIQUES
    // ========================================================================

    /**
     * Scan tous les pÃƒÂ©riphÃƒÂ©riques MIDI disponibles
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
            const response = await this.backend.sendCommand('list_devices', {
                full_scan: fullScan
            });
            
            if (response.success) {
                const devices = response.data?.devices || [];
                
                // Mettre ÃƒÂ  jour le cache
                devices.forEach(device => {
                    this.devices.set(device.id, device);
                });
                
                this.lastScanTime = Date.now();
                
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', 
                        `Ã¢Å“â€œ Scan complete: ${devices.length} devices found`);
                }
                
                // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
                this.eventBus.emit('instruments:scan_complete', { 
                    devices, 
                    count: devices.length 
                });
                
                // RafraÃƒÂ®chir la vue
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
     * Liste tous les pÃƒÂ©riphÃƒÂ©riques disponibles
     * @returns {Promise<Array>}
     */
    async listDevices() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('list_devices', {});
            
            if (response.success) {
                const devices = response.data?.devices || [];
                
                // Mettre ÃƒÂ  jour le cache
                devices.forEach(device => {
                    this.devices.set(device.id, device);
                });
                
                if (this.logger?.debug) {
                    this.logger.debug('InstrumentController', 
                        `Ã¢Å“â€œ Listed ${devices.length} devices`);
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
     * Obtient uniquement les pÃƒÂ©riphÃƒÂ©riques connectÃƒÂ©s
     * @returns {Promise<Array>}
     */
    async getConnectedDevices() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('list_devices', {});
            
            if (response.success) {
                const devices = response.data?.devices || [];
                
                // Mettre ÃƒÂ  jour la liste des connectÃƒÂ©s
                this.connectedDevices.clear();
                devices.forEach(device => {
                    this.connectedDevices.add(device.id);
                    this.devices.set(device.id, device);
                });
                
                if (this.logger?.debug) {
                    this.logger.debug('InstrumentController', 
                        `Ã¢Å“â€œ ${devices.length} devices connected`);
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
     * Obtient les informations dÃƒÂ©taillÃƒÂ©es d'un pÃƒÂ©riphÃƒÂ©rique
     * @param {string} deviceId
     * @param {boolean} useCache - Utiliser cache si disponible
     * @returns {Promise<Object>}
     */
    async getDeviceInfo(deviceId, useCache = true) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        // VÃƒÂ©rifier cache
        if (useCache) {
            const cached = this.deviceInfoCache.get(deviceId);
            if (cached && (Date.now() - cached.timestamp < this.deviceInfoCacheTTL)) {
                return cached.info;
            }
        }

        try {
            const response = await this.backend.sendCommand('get_device_info', {
                device_id: deviceId
            });
            
            if (response.success) {
                const info = response.data;
                
                // Mettre en cache
                this.deviceInfoCache.set(deviceId, {
                    info: info,
                    timestamp: Date.now()
                });
                
                // Mettre ÃƒÂ  jour le device dans la map
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
    // CONNEXION / DÃƒâ€°CONNEXION
    // ========================================================================

    /**
     * Connecte un pÃƒÂ©riphÃƒÂ©rique MIDI
     * @param {string} deviceId
     * @returns {Promise<boolean>}
     */
    async connectDevice(deviceId) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('connect_device', {
                device_id: deviceId
            });
            
            if (response.success) {
                this.connectedDevices.add(deviceId);
                
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', `Ã¢Å“â€œ Device connected: ${deviceId}`);
                }
                
                // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
                this.eventBus.emit('instrument:connected', { deviceId });
                
                // RafraÃƒÂ®chir la vue
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
     * DÃƒÂ©connecte un pÃƒÂ©riphÃƒÂ©rique MIDI
     * @param {string} deviceId
     * @returns {Promise<boolean>}
     */
    async disconnectDevice(deviceId) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('disconnect_device', {
                device_id: deviceId
            });
            
            if (response.success) {
                this.connectedDevices.delete(deviceId);
                
                if (this.logger?.info) {
                    this.logger.info('InstrumentController', 
                        `Ã¢Å“â€œ Device disconnected: ${deviceId}`);
                }
                
                // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
                this.eventBus.emit('instrument:disconnected', { deviceId });
                
                // RafraÃƒÂ®chir la vue
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
     * DÃƒÂ©connecte tous les pÃƒÂ©riphÃƒÂ©riques
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
                    this.logger.info('InstrumentController', 'Ã¢Å“â€œ All devices disconnected');
                }
                
                // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
                this.eventBus.emit('instruments:all_disconnected');
                
                // Notification
                this.eventBus.emit('notification:show', {
                    message: 'All devices disconnected',
                    type: 'success',
                    duration: 3000
                });
                
                // RafraÃƒÂ®chir la vue
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
    // HOT-PLUG (DÃƒâ€°TECTION AUTOMATIQUE)
    // ========================================================================

    /**
     * DÃƒÂ©marre la surveillance hot-plug
     * @param {number} intervalMs - Intervalle de scan en ms (dÃƒÂ©faut: 2000)
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
                        `Ã¢Å“â€œ Hot-plug started (interval: ${intervalMs}ms)`);
                }
                
                // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
                this.eventBus.emit('instruments:hotplug_started', { intervalMs });
                
                // Notification
                this.eventBus.emit('notification:show', {
                    message: 'Auto-detection of devices enabled',
                    type: 'success',
                    duration: 3000
                });
                
                // RafraÃƒÂ®chir la vue
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
     * ArrÃƒÂªte la surveillance hot-plug
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
                    this.logger.info('InstrumentController', 'Ã¢Å“â€œ Hot-plug stopped');
                }
                
                // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
                this.eventBus.emit('instruments:hotplug_stopped');
                
                // Notification
                this.eventBus.emit('notification:show', {
                    message: 'Auto-detection of devices disabled',
                    type: 'info',
                    duration: 3000
                });
                
                // RafraÃƒÂ®chir la vue
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
            this.logger.debug('InstrumentController', 'Ã¢Å“â€œ Local hot-plug monitoring started');
        }
    }

    stopHotPlugMonitoring() {
        if (this.hotPlugTimer) {
            clearInterval(this.hotPlugTimer);
            this.hotPlugTimer = null;
        }
    }

    // ========================================================================
    // Ãƒâ€°VÃƒâ€°NEMENTS BACKEND
    // ========================================================================

    handleDeviceConnected(data) {
        const deviceId = data.device_id || data.deviceId;
        
        if (deviceId) {
            this.connectedDevices.add(deviceId);
            
            if (this.logger?.info) {
                this.logger.info('InstrumentController', 
                    `Ã°Å¸â€œÂ¥ Device connected: ${deviceId}`);
            }
            
            // Invalider cache info
            this.deviceInfoCache.delete(deviceId);
            
            // RafraÃƒÂ®chir la vue
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
                    `Ã°Å¸â€œÂ¤ Device disconnected: ${deviceId}`);
            }
            
            // RafraÃƒÂ®chir la vue
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
                    `Ã°Å¸â€Â Device discovered: ${device.id}`);
            }
            
            // RafraÃƒÂ®chir la vue
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
     * Charge la liste des pÃƒÂ©riphÃƒÂ©riques connectÃƒÂ©s
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
     * VÃƒÂ©rifie si un pÃƒÂ©riphÃƒÂ©rique est connectÃƒÂ©
     * @param {string} deviceId
     * @returns {boolean}
     */
    isDeviceConnected(deviceId) {
        return this.connectedDevices.has(deviceId);
    }

    /**
     * Obtient un pÃƒÂ©riphÃƒÂ©rique du cache
     * @param {string} deviceId
     * @returns {Object|null}
     */
    getDevice(deviceId) {
        return this.devices.get(deviceId) || null;
    }

    /**
     * Obtient tous les pÃƒÂ©riphÃƒÂ©riques disponibles
     * @returns {Array}
     */
    getAllDevices() {
        return Array.from(this.devices.values());
    }

    /**
     * Obtient tous les pÃƒÂ©riphÃƒÂ©riques connectÃƒÂ©s (du cache local)
     * @returns {Array}
     */
    getConnectedDevicesFromCache() {
        return Array.from(this.devices.values()).filter(device => 
            this.connectedDevices.has(device.id)
        );
    }

    /**
     * Obtient tous les pÃƒÂ©riphÃƒÂ©riques disponibles mais non connectÃƒÂ©s
     * @returns {Array}
     */
    getAvailableDevices() {
        return Array.from(this.devices.values()).filter(device => 
            !this.connectedDevices.has(device.id) && device.available !== false
        );
    }

    /**
     * Filtre les pÃƒÂ©riphÃƒÂ©riques par type
     * @param {number} type - Type de pÃƒÂ©riphÃƒÂ©rique
     * @returns {Array}
     */
    getDevicesByType(type) {
        return Array.from(this.devices.values()).filter(device => 
            device.type === type
        );
    }

    /**
     * Recherche un pÃƒÂ©riphÃƒÂ©rique par nom
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
     * Nettoie le cache des infos pÃƒÂ©riphÃƒÂ©riques
     */
    clearDeviceInfoCache() {
        this.deviceInfoCache.clear();
        
        if (this.logger?.debug) {
            this.logger.debug('InstrumentController', 'Ã¢Å“â€œ Device info cache cleared');
        }
    }

    /**
     * Nettoie les pÃƒÂ©riphÃƒÂ©riques obsolÃƒÂ¨tes du cache
     * @param {number} maxAge - Age max en ms (dÃƒÂ©faut: 5 minutes)
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
                `Ã¢Å“â€œ Cleaned ${cleaned} expired cache entries`);
        }
    }

    // ========================================================================
    // Ãƒâ€°VÃƒâ€°NEMENTS PAGE
    // ========================================================================

    onInstrumentsPageActive() {
        if (this.logger?.debug) {
            this.logger.debug('InstrumentController', 'Instruments page active');
        }

        // RafraÃƒÂ®chir la liste
        this.refreshDeviceList();
        
        // VÃƒÂ©rifier statut hot-plug
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
    // LEGACY / COMPATIBILITÃƒâ€°
    // ========================================================================

    /**
     * Scan pÃƒÂ©riphÃƒÂ©riques (alias pour compatibilitÃƒÂ©)
     */
    async scan() {
        return await this.scanDevices(false);
    }

    /**
     * Connecte un instrument (alias pour compatibilitÃƒÂ©)
     */
    async connect(deviceId) {
        return await this.connectDevice(deviceId);
    }

    /**
     * DÃƒÂ©connecte un instrument (alias pour compatibilitÃƒÂ©)
     */
    async disconnect(deviceId) {
        return await this.disconnectDevice(deviceId);
    }

    // ========================================================================
    // STATISTIQUES
    // ========================================================================

    /**
     * Obtient les statistiques des pÃƒÂ©riphÃƒÂ©riques
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
        // ArrÃƒÂªter hot-plug
        this.stopHotPlugMonitoring();
        
        // Nettoyer caches
        this.devices.clear();
        this.connectedDevices.clear();
        this.deviceInfoCache.clear();
        
        if (this.logger?.info) {
            this.logger.info('InstrumentController', 'Destroyed');
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
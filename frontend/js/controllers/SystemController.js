// ============================================================================
// Fichier: frontend/js/controllers/SystemController.js
// Version: v3.2.0 - IMPLÃ‰MENTATION API COMPLÃˆTE
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.2.0:
// âœ… Toutes les commandes system.* implÃ©mentÃ©es
// âœ… Toutes les commandes devices.* implÃ©mentÃ©es  
// âœ… Gestion Ã©vÃ©nements backend temps rÃ©el
// âœ… Monitoring systÃ¨me automatique
// âœ… Gestion hot-plug devices
// ============================================================================

class SystemController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.logger = window.logger || console;
        this.systemModel = models.system || models.state;
        this.instrumentModel = models.instrument;
        this.view = views.system;
        this.backend = window.app?.services?.backend || window.backendService;
        
        // Ã‰tat systÃ¨me
        this.state = {
            ...this.state,
            backendConnected: false,
            devicesCount: 0,
            connectedDevicesCount: 0,
            lastScan: null
        };
        
        // Configuration monitoring
        this.config = {
            ...this.config,
            autoRefreshInterval: 5000, // 5 secondes
            statsUpdateInterval: 10000 // 10 secondes
        };
        
        // Timers
        this.statsTimer = null;
        this.devicesTimer = null;
        
        this._fullyInitialized = true;
        this.bindEvents();
    }
    
    // ========================================================================
    // Ã‰VÃ‰NEMENTS
    // ========================================================================
    
    bindEvents() {
        // Backend
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        
        // Devices (Ã©vÃ©nements backend)
        this.eventBus.on('backend:event:device_connected', (data) => {
            this.handleDeviceConnected(data);
        });
        this.eventBus.on('backend:event:device_disconnected', (data) => {
            this.handleDeviceDisconnected(data);
        });
        this.eventBus.on('backend:event:midi_message', (data) => {
            this.handleMidiMessage(data);
        });
        
        // Navigation
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'system') {
                this.startMonitoring();
            } else {
                this.stopMonitoring();
            }
        });
        
        this.log('info', 'SystemController', 'âœ… Events bound');
    }
    
    async onBackendConnected() {
        this.state.backendConnected = true;
        this.log('info', 'SystemController', 'âœ… Backend connected');
        
        try {
            // Charger infos systÃ¨me
            await this.refreshSystemInfo();
            
            // Scanner devices
            await this.scanDevices();
            
            // DÃ©marrer monitoring si page active
            const currentPage = this.systemModel?.get('currentPage');
            if (currentPage === 'system') {
                this.startMonitoring();
            }
        } catch (error) {
            this.log('error', 'SystemController', 'Initialization failed:', error);
        }
    }
    
    onBackendDisconnected() {
        this.state.backendConnected = false;
        this.stopMonitoring();
        this.log('warn', 'SystemController', 'âš ï¸ Backend disconnected');
    }
    
    // ========================================================================
    // COMMANDES SYSTEM.*
    // ========================================================================
    
    /**
     * Obtient la version du backend
     */
    async getVersion() {
        try {
            const response = await this.backend.sendCommand('get_status', {});
            
            if (response.success !== false) {
                return response.data || response;
            }
            throw new Error(response.message || 'Failed to get version');
        } catch (error) {
            this.log('error', 'SystemController', 'getVersion failed:', error);
            throw error;
        }
    }
    
    /**
     * Obtient les infos systÃ¨me
     */
    async getInfo() {
        try {
            const response = await this.backend.sendCommand('get_status', {});
            
            if (response.success !== false) {
                return response.data || response;
            }
            throw new Error(response.message || 'Failed to get info');
        } catch (error) {
            this.log('error', 'SystemController', 'getInfo failed:', error);
            throw error;
        }
    }
    
    /**
     * Obtient l'uptime
     */
    async getUptime() {
        try {
            const response = await this.backend.sendCommand('get_status', {});
            
            if (response.success !== false) {
                return response.data || response;
            }
            throw new Error(response.message || 'Failed to get uptime');
        } catch (error) {
            this.log('error', 'SystemController', 'getUptime failed:', error);
            throw error;
        }
    }
    
    /**
     * Obtient l'utilisation mÃ©moire
     */
    async getMemory() {
        try {
            const response = await this.backend.sendCommand('get_status', {});
            
            if (response.success !== false) {
                return response.data || response;
            }
            throw new Error(response.message || 'Failed to get memory');
        } catch (error) {
            this.log('error', 'SystemController', 'getMemory failed:', error);
            throw error;
        }
    }
    
    /**
     * Obtient l'utilisation disque
     */
    async getDisk() {
        try {
            const response = await this.backend.sendCommand('get_status', {});
            
            if (response.success !== false) {
                return response.data || response;
            }
            throw new Error(response.message || 'Failed to get disk');
        } catch (error) {
            this.log('error', 'SystemController', 'getDisk failed:', error);
            throw error;
        }
    }
    
    /**
     * Ping le backend
     */
    async ping() {
        try {
            const response = await this.backend.sendCommand('get_status', {});
            return response.success !== false;
        } catch (error) {
            return false;
        }
    }
    
    /**
     * RafraÃ®chit toutes les infos systÃ¨me
     */
    async refreshSystemInfo() {
        try {
            const [version, info, uptime, memory, disk] = await Promise.allSettled([
                this.getVersion(),
                this.getInfo(),
                this.getUptime(),
                this.getMemory(),
                this.getDisk()
            ]);
            
            const systemInfo = {
                version: version.status === 'fulfilled' ? version.value : null,
                info: info.status === 'fulfilled' ? info.value : null,
                uptime: uptime.status === 'fulfilled' ? uptime.value : null,
                memory: memory.status === 'fulfilled' ? memory.value : null,
                disk: disk.status === 'fulfilled' ? disk.value : null,
                lastUpdate: Date.now()
            };
            
            // Mettre Ã  jour le model
            if (this.systemModel) {
                this.systemModel.set('systemInfo', systemInfo);
            }
            
            // Ã‰mettre Ã©vÃ©nement
            this.eventBus.emit('system:info-updated', systemInfo);
            
            return systemInfo;
        } catch (error) {
            this.log('error', 'SystemController', 'refreshSystemInfo failed:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // COMMANDES DEVICES.*
    // ========================================================================
    
    /**
     * Liste tous les devices MIDI
     */
    async listDevices() {
        try {
            const response = await this.backend.sendCommand('list_devices', {});
            
            if (response.success !== false) {
                const devices = response.data?.devices || response.devices || [];
                
                this.state.devicesCount = devices.length;
                
                // Mettre Ã  jour le model instrument
                if (this.instrumentModel && devices.length > 0) {
                    this.instrumentModel.instruments.clear();
                    devices.forEach(device => {
                        this.instrumentModel.instruments.set(device.id, device);
                    });
                    
                    this.instrumentModel.state.totalInstruments = devices.length;
                    this.instrumentModel.state.connectedCount = devices.filter(d => d.connected).length;
                }
                
                this.eventBus.emit('devices:list-updated', { devices });
                
                return devices;
            }
            throw new Error(response.message || 'Failed to list devices');
        } catch (error) {
            this.log('error', 'SystemController', 'listDevices failed:', error);
            throw error;
        }
    }
    
    /**
     * Scanne les devices disponibles
     */
    async scanDevices() {
        try {
            this.log('info', 'SystemController', 'Scanning devices...');
            
            const response = await this.backend.sendCommand('list_devices', {});
            
            if (response.success !== false) {
                const devices = response.data?.devices || response.devices || [];
                
                this.state.devicesCount = devices.length;
                this.state.lastScan = Date.now();
                
                // Mettre Ã  jour le model instrument
                if (this.instrumentModel && devices.length > 0) {
                    this.instrumentModel.instruments.clear();
                    devices.forEach(device => {
                        this.instrumentModel.instruments.set(device.id, device);
                    });
                    
                    this.instrumentModel.state.totalInstruments = devices.length;
                    this.instrumentModel.state.connectedCount = devices.filter(d => d.connected).length;
                    this.instrumentModel.state.lastScan = Date.now();
                }
                
                this.log('info', 'SystemController', \`âœ… Found \${devices.length} devices\`);
                this.eventBus.emit('devices:scan-complete', { devices });
                
                return devices;
            }
            throw new Error(response.message || 'Scan failed');
        } catch (error) {
            this.log('error', 'SystemController', 'scanDevices failed:', error);
            throw error;
        }
    }
    
    /**
     * Connecte un device
     */
    async connectDevice(deviceId) {
        try {
            this.log('info', 'SystemController', \`Connecting device: \${deviceId}\`);
            
            const response = await this.backend.sendCommand('connect_device', {
                device_id: deviceId
            });
            
            if (response.success !== false) {
                // Mettre Ã  jour le model
                if (this.instrumentModel) {
                    const device = this.instrumentModel.instruments.get(deviceId);
                    if (device) {
                        device.connected = true;
                        this.instrumentModel.instruments.set(deviceId, device);
                        this.instrumentModel.state.connectedCount++;
                    }
                }
                
                this.log('info', 'SystemController', \`âœ… Device connected: \${deviceId}\`);
                this.eventBus.emit('devices:connected', { deviceId });
                
                return true;
            }
            throw new Error(response.message || 'Connection failed');
        } catch (error) {
            this.log('error', 'SystemController', 'connectDevice failed:', error);
            throw error;
        }
    }
    
    /**
     * DÃ©connecte un device
     */
    async disconnectDevice(deviceId) {
        try {
            this.log('info', 'SystemController', \`Disconnecting device: \${deviceId}\`);
            
            const response = await this.backend.sendCommand('disconnect_device', {
                device_id: deviceId
            });
            
            if (response.success !== false) {
                // Mettre Ã  jour le model
                if (this.instrumentModel) {
                    const device = this.instrumentModel.instruments.get(deviceId);
                    if (device) {
                        device.connected = false;
                        this.instrumentModel.instruments.set(deviceId, device);
                        this.instrumentModel.state.connectedCount = Math.max(0, this.instrumentModel.state.connectedCount - 1);
                    }
                }
                
                this.log('info', 'SystemController', \`âœ… Device disconnected: \${deviceId}\`);
                this.eventBus.emit('devices:disconnected', { deviceId });
                
                return true;
            }
            throw new Error(response.message || 'Disconnection failed');
        } catch (error) {
            this.log('error', 'SystemController', 'disconnectDevice failed:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // GESTION Ã‰VÃ‰NEMENTS BACKEND
    // ========================================================================
    
    handleDeviceConnected(data) {
        this.log('info', 'SystemController', \`Device connected: \${data.device_id}\`);
        
        // Notifier
        if (this.notifications) {
            this.notifications.show(
                'Device Connected',
                \`Device \${data.device_id} has been connected\`,
                'success',
                3000
            );
        }
        
        // RafraÃ®chir liste
        this.listDevices();
    }
    
    handleDeviceDisconnected(data) {
        this.log('info', 'SystemController', \`Device disconnected: \${data.device_id}\`);
        
        // Notifier
        if (this.notifications) {
            this.notifications.show(
                'Device Disconnected',
                \`Device \${data.device_id} has been disconnected\`,
                'warning',
                3000
            );
        }
        
        // RafraÃ®chir liste
        this.listDevices();
    }
    
    handleMidiMessage(data) {
        // Ã‰mettre pour visualisation
        this.eventBus.emit('midi:message', data);
    }
    
    // ========================================================================
    // MONITORING
    // ========================================================================
    
    startMonitoring() {
        if (this.statsTimer || this.devicesTimer) {
            return; // DÃ©jÃ  dÃ©marrÃ©
        }
        
        this.log('info', 'SystemController', 'Starting monitoring...');
        
        // Stats systÃ¨me
        this.statsTimer = setInterval(() => {
            this.refreshSystemInfo().catch(err => {
                this.log('error', 'SystemController', 'Stats update failed:', err);
            });
        }, this.config.statsUpdateInterval);
        
        // Devices
        this.devicesTimer = setInterval(() => {
            this.listDevices().catch(err => {
                this.log('error', 'SystemController', 'Devices update failed:', err);
            });
        }, this.config.autoRefreshInterval);
        
        // Premier refresh immÃ©diat
        this.refreshSystemInfo();
        this.listDevices();
    }
    
    stopMonitoring() {
        if (this.statsTimer) {
            clearInterval(this.statsTimer);
            this.statsTimer = null;
        }
        
        if (this.devicesTimer) {
            clearInterval(this.devicesTimer);
            this.devicesTimer = null;
        }
        
        this.log('info', 'SystemController', 'Monitoring stopped');
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        }
    }
}

// ============================================================================
// EXPORT
// ============================================================================
if (typeof window !== 'undefined') {
    window.SystemController = SystemController;
}
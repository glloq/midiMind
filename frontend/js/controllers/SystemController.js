// ============================================================================
// Fichier: frontend/js/controllers/SystemController.js
// Chemin réel: frontend/js/controllers/SystemController.js
// Version: v3.3.0 - GESTION ROBUSTE DES ERREURS BACKEND
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v3.3.0:
// ✅ Utilisation des bonnes commandes API (system.version, system.uptime, etc.)
// ✅ Gestion gracieuse des erreurs backend
// ✅ Fallbacks pour commandes non disponibles
// ✅ Pas de crash si backend ne répond pas
// ✅ Monitoring optionnel et résilient
// ============================================================================

class SystemController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.logger = window.logger || console;
        this.systemModel = models.system || models.state;
        this.instrumentModel = models.instrument;
        this.view = views.system;
        this.backend = window.app?.services?.backend || window.backendService;
        
        // État système
        this.state = {
            ...this.state,
            backendConnected: false,
            devicesCount: 0,
            connectedDevicesCount: 0,
            lastScan: null,
            systemInfoAvailable: false
        };
        
        // Configuration monitoring
        this.config = {
            ...this.config,
            autoRefreshInterval: 5000, // 5 secondes
            statsUpdateInterval: 10000, // 10 secondes
            enableSystemMonitoring: false // Désactivé par défaut si backend incomplet
        };
        
        // Timers
        this.statsTimer = null;
        this.devicesTimer = null;
        
        this._fullyInitialized = true;
        this.bindEvents();
    }
    
    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================
    
    bindEvents() {
        // Backend
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        
        // Devices (événements backend)
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
        
        this.log('info', 'SystemController', '✓ Events bound');
    }
    
    async onBackendConnected() {
        this.state.backendConnected = true;
        this.log('info', 'SystemController', '✓ Backend connected');
        
        try {
            // Tester disponibilité des commandes système
            await this.testSystemCommands();
            
            // Scanner devices (priorité plus haute)
            await this.scanDevices();
            
            // Charger infos système si disponibles
            if (this.state.systemInfoAvailable && this.config.enableSystemMonitoring) {
                await this.refreshSystemInfo();
            }
            
            // Démarrer monitoring si page active
            const currentPage = this.systemModel?.get('currentPage');
            if (currentPage === 'system') {
                this.startMonitoring();
            }
        } catch (error) {
            this.log('warn', 'SystemController', 'Initialization completed with warnings:', error.message);
        }
    }
    
    onBackendDisconnected() {
        this.state.backendConnected = false;
        this.stopMonitoring();
        this.log('warn', 'SystemController', '⚠️ Backend disconnected');
    }
    
    // ========================================================================
    // TEST DE DISPONIBILITÉ DES COMMANDES
    // ========================================================================
    
    /**
     * Teste si les commandes système sont disponibles
     */
    async testSystemCommands() {
        try {
            // Essayer system.version comme test
            const result = await this.backend.sendCommand('system.version', {});
            this.state.systemInfoAvailable = true;
            this.config.enableSystemMonitoring = true;
            this.log('info', 'SystemController', '✓ System commands available');
            return true;
        } catch (error) {
            this.state.systemInfoAvailable = false;
            this.config.enableSystemMonitoring = false;
            this.log('info', 'SystemController', 'ℹ️ System commands not available, continuing without monitoring');
            return false;
        }
    }
    
    // ========================================================================
    // COMMANDES SYSTEM.* - AVEC GESTION D'ERREUR ROBUSTE
    // ========================================================================
    
    /**
     * Obtient la version du backend
     */
    async getVersion() {
        try {
            const response = await this.backend.sendCommand('system.version', {});
            
            if (response && response.success !== false) {
                return response.data || response;
            }
            throw new Error(response?.message || 'Failed to get version');
        } catch (error) {
            this.log('debug', 'SystemController', 'getVersion not available:', error.message);
            return { version: 'unknown', available: false };
        }
    }
    
    /**
     * Obtient les infos système
     */
    async getInfo() {
        try {
            const response = await this.backend.sendCommand('system.info', {});
            
            if (response && response.success !== false) {
                return response.data || response;
            }
            throw new Error(response?.message || 'Failed to get info');
        } catch (error) {
            this.log('debug', 'SystemController', 'getInfo not available:', error.message);
            return { info: 'unknown', available: false };
        }
    }
    
    /**
     * Obtient l'uptime
     */
    async getUptime() {
        try {
            const response = await this.backend.sendCommand('system.uptime', {});
            
            if (response && response.success !== false) {
                return response.data || response;
            }
            throw new Error(response?.message || 'Failed to get uptime');
        } catch (error) {
            this.log('debug', 'SystemController', 'getUptime not available:', error.message);
            return { uptime: 0, available: false };
        }
    }
    
    /**
     * Obtient l'utilisation mémoire
     */
    async getMemory() {
        try {
            const response = await this.backend.sendCommand('system.memory', {});
            
            if (response && response.success !== false) {
                return response.data || response;
            }
            throw new Error(response?.message || 'Failed to get memory');
        } catch (error) {
            this.log('debug', 'SystemController', 'getMemory not available:', error.message);
            return { used: 0, total: 0, available: false };
        }
    }
    
    /**
     * Obtient l'utilisation disque
     */
    async getDisk() {
        try {
            const response = await this.backend.sendCommand('system.disk', {});
            
            if (response && response.success !== false) {
                return response.data || response;
            }
            throw new Error(response?.message || 'Failed to get disk');
        } catch (error) {
            this.log('debug', 'SystemController', 'getDisk not available:', error.message);
            return { used: 0, total: 0, available: false };
        }
    }
    
    /**
     * Ping le backend
     */
    async ping() {
        try {
            const response = await this.backend.sendCommand('system.ping', {});
            return response && response.success !== false;
        } catch (error) {
            // Utiliser devices.list comme alternative si ping n'existe pas
            try {
                await this.backend.sendCommand('devices.list', {});
                return true;
            } catch (e) {
                return false;
            }
        }
    }
    
    /**
     * Rafraîchit toutes les infos système (avec gestion d'erreur)
     */
    async refreshSystemInfo() {
        // Ne rien faire si les commandes système ne sont pas disponibles
        if (!this.config.enableSystemMonitoring) {
            return null;
        }
        
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
                timestamp: Date.now()
            };
            
            // Mettre à jour le model
            if (this.systemModel) {
                this.systemModel.set('systemInfo', systemInfo);
            }
            
            // Émettre événement
            this.eventBus.emit('system:info-updated', systemInfo);
            
            return systemInfo;
        } catch (error) {
            this.log('debug', 'SystemController', 'System info refresh failed:', error.message);
            return null;
        }
    }
    
    // ========================================================================
    // COMMANDES DEVICES.* - PRIORITAIRES
    // ========================================================================
    
    /**
     * Liste tous les devices (commande principale)
     */
    async listDevices() {
        try {
            const response = await this.backend.sendCommand('devices.list', {});
            
            if (response && response.success !== false) {
                const devices = response.data?.devices || response.devices || [];
                
                this.state.devicesCount = devices.length;
                this.state.connectedDevicesCount = devices.filter(d => d.connected).length;
                
                // Mettre à jour le model
                if (this.systemModel) {
                    this.systemModel.set('devices', devices);
                    this.systemModel.set('devicesCount', devices.length);
                }
                
                this.eventBus.emit('devices:list-updated', { devices });
                
                return devices;
            }
            throw new Error(response?.message || 'Failed to list devices');
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
            
            const response = await this.backend.sendCommand('devices.list', {});
            
            if (response && response.success !== false) {
                const devices = response.data?.devices || response.devices || [];
                
                this.state.devicesCount = devices.length;
                this.state.lastScan = Date.now();
                
                // Mettre à jour le model instrument
                if (this.instrumentModel && devices.length > 0) {
                    this.instrumentModel.instruments.clear();
                    devices.forEach(device => {
                        this.instrumentModel.instruments.set(device.id, device);
                    });
                    
                    this.instrumentModel.state.totalInstruments = devices.length;
                    this.instrumentModel.state.connectedCount = devices.filter(d => d.connected).length;
                    this.instrumentModel.state.lastScan = Date.now();
                }
                
                this.log('info', 'SystemController', `✓ Found ${devices.length} devices`);
                this.eventBus.emit('devices:scan-complete', { devices });
                
                return devices;
            }
            throw new Error(response?.message || 'Scan failed');
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
            this.log('info', 'SystemController', `Connecting device: ${deviceId}`);
            
            const response = await this.backend.sendCommand('devices.connect', {
                device_id: deviceId
            });
            
            if (response && response.success !== false) {
                // Mettre à jour le model
                if (this.instrumentModel) {
                    const device = this.instrumentModel.instruments.get(deviceId);
                    if (device) {
                        device.connected = true;
                        this.instrumentModel.instruments.set(deviceId, device);
                        this.instrumentModel.state.connectedCount++;
                    }
                }
                
                this.log('info', 'SystemController', `✓ Device connected: ${deviceId}`);
                this.eventBus.emit('devices:connected', { deviceId });
                
                return true;
            }
            throw new Error(response?.message || 'Connection failed');
        } catch (error) {
            this.log('error', 'SystemController', 'connectDevice failed:', error);
            throw error;
        }
    }
    
    /**
     * Déconnecte un device
     */
    async disconnectDevice(deviceId) {
        try {
            this.log('info', 'SystemController', `Disconnecting device: ${deviceId}`);
            
            const response = await this.backend.sendCommand('devices.disconnect', {
                device_id: deviceId
            });
            
            if (response && response.success !== false) {
                // Mettre à jour le model
                if (this.instrumentModel) {
                    const device = this.instrumentModel.instruments.get(deviceId);
                    if (device) {
                        device.connected = false;
                        this.instrumentModel.instruments.set(deviceId, device);
                        this.instrumentModel.state.connectedCount = Math.max(0, this.instrumentModel.state.connectedCount - 1);
                    }
                }
                
                this.log('info', 'SystemController', `✓ Device disconnected: ${deviceId}`);
                this.eventBus.emit('devices:disconnected', { deviceId });
                
                return true;
            }
            throw new Error(response?.message || 'Disconnection failed');
        } catch (error) {
            this.log('error', 'SystemController', 'disconnectDevice failed:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // GESTION ÉVÉNEMENTS BACKEND
    // ========================================================================
    
    handleDeviceConnected(data) {
        this.log('info', 'SystemController', `Device connected: ${data.device_id}`);
        
        // Notifier
        if (this.notifications) {
            this.notifications.show(
                'Device Connected',
                `Device ${data.device_id} has been connected`,
                'success',
                3000
            );
        }
        
        // Rafraîchir liste
        this.listDevices().catch(err => {
            this.log('error', 'SystemController', 'Failed to refresh devices:', err);
        });
    }
    
    handleDeviceDisconnected(data) {
        this.log('info', 'SystemController', `Device disconnected: ${data.device_id}`);
        
        // Notifier
        if (this.notifications) {
            this.notifications.show(
                'Device Disconnected',
                `Device ${data.device_id} has been disconnected`,
                'warning',
                3000
            );
        }
        
        // Rafraîchir liste
        this.listDevices().catch(err => {
            this.log('error', 'SystemController', 'Failed to refresh devices:', err);
        });
    }
    
    handleMidiMessage(data) {
        // Émettre pour visualisation
        this.eventBus.emit('midi:message', data);
    }
    
    // ========================================================================
    // MONITORING - OPTIONNEL ET RÉSILIENT
    // ========================================================================
    
    startMonitoring() {
        if (this.statsTimer || this.devicesTimer) {
            return; // Déjà démarré
        }
        
        this.log('info', 'SystemController', 'Starting monitoring...');
        
        // Stats système (seulement si disponibles)
        if (this.config.enableSystemMonitoring) {
            this.statsTimer = setInterval(() => {
                this.refreshSystemInfo().catch(err => {
                    this.log('debug', 'SystemController', 'Stats update skipped:', err.message);
                });
            }, this.config.statsUpdateInterval);
        }
        
        // Devices (toujours actif)
        this.devicesTimer = setInterval(() => {
            this.listDevices().catch(err => {
                this.log('error', 'SystemController', 'Devices update failed:', err);
            });
        }, this.config.autoRefreshInterval);
        
        // Premier refresh immédiat (devices seulement)
        this.listDevices().catch(err => {
            this.log('error', 'SystemController', 'Initial device scan failed:', err);
        });
        
        // System info optionnel
        if (this.config.enableSystemMonitoring) {
            this.refreshSystemInfo().catch(err => {
                this.log('debug', 'SystemController', 'Initial system info skipped:', err.message);
            });
        }
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
    
    /**
     * Obtient le statut complet du système
     */
    getStatus() {
        return {
            backendConnected: this.state.backendConnected,
            systemInfoAvailable: this.state.systemInfoAvailable,
            devicesCount: this.state.devicesCount,
            connectedDevicesCount: this.state.connectedDevicesCount,
            lastScan: this.state.lastScan,
            monitoringEnabled: !!(this.statsTimer || this.devicesTimer)
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================
if (typeof window !== 'undefined') {
    window.SystemController = SystemController;
}
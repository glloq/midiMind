// ============================================================================
// Fichier: frontend/js/controllers/InstrumentController.js
// Chemin rÃƒÂ©el: frontend/js/controllers/InstrumentController.js
// Version: v4.2.3 - FIXED BACKEND SIGNATURE - API CORRECTED
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v4.2.3:
// âœ… CRITIQUE: Ajout paramÃ¨tre backend au constructeur (6Ã¨me paramÃ¨tre)
// âœ… Fix: super() appelle BaseController avec backend
// âœ… this.backend initialisÃ© automatiquement via BaseController
// ============================================================================
// ============================================================================
// CORRECTIONS v4.2.2:
// Ã¢Å“â€¦ devices.scan pour count (pas devices.list)
// Ã¢Å“â€¦ response.data.devices extraction
// Ã¢Å“â€¦ device_id en snake_case
// ============================================================================

class InstrumentController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        this.logger = window.logger || console;
        this.model = models.instrument;
        this.view = views.instrument;
        // âœ… this.backend initialisÃ© automatiquement par BaseController
        
        this.devices = new Map();
        this.connectedDevices = new Set();
        
        this.hotPlugEnabled = false;
        this.hotPlugInterval = 2000;
        this.hotPlugTimer = null;
        
        this.deviceInfoCache = new Map();
        this.deviceInfoCacheTTL = 30000;
        
        this.isScanning = false;
        this.lastScanTime = null;
        
        this._fullyInitialized = true;
        this.bindEvents();
    }

    bindEvents() {
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        this.eventBus.on('backend:device:connected', (data) => this.handleDeviceConnected(data));
        this.eventBus.on('backend:device:disconnected', (data) => this.handleDeviceDisconnected(data));
        this.eventBus.on('backend:device:discovered', (data) => this.handleDeviceDiscovered(data));
        this.eventBus.on('backend:device:error', (data) => this.handleDeviceError(data));
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'instruments') {
                this.onInstrumentsPageActive();
            } else {
                this.onInstrumentsPageInactive();
            }
        });
        this.eventBus.on('instruments:request_refresh', () => this.refreshDeviceList());
        
        this.logger?.info?.('InstrumentController', 'Ã¢Å“â€œ Events bound');
    }

    async initialize() {
        this.logger?.info?.('InstrumentController', 'Initializing...');

        if (this.backend?.isConnected()) {
            await this.onBackendConnected();
        }
    }

    async onBackendConnected() {
        this.logger?.info?.('InstrumentController', 'Ã¢Å“â€¦ Backend connected');

        this.scanDevices().catch(err => {
            this.log('warn', 'InstrumentController', 'Initial scan failed:', err.message);
        });
        
        this.loadConnectedDevices().catch(err => {
            this.log('warn', 'InstrumentController', 'Load devices failed:', err.message);
        });
        
        this.getHotPlugStatus().then(status => {
            if (status?.enabled) {
                this.hotPlugEnabled = true;
                this.logger?.info?.('InstrumentController', 'Ã¢Å“â€¦ Hot-plug enabled');
            }
        }).catch(err => {
            this.log('warn', 'InstrumentController', 'Hot-plug status failed:', err.message);
        });
    }

    onBackendDisconnected() {
        this.logger?.warn?.('InstrumentController', 'Ã¢Å¡Â Ã¯Â¸Â Backend disconnected');
        this.stopHotPlugMonitoring();
        this.connectedDevices.clear();
        this.refreshView();
    }

    /**
     * Ã¢Å“â€¦ CORRECTION: Utiliser devices.scan pour obtenir count
     */
    async scanDevices(full_scan = false) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        if (this.isScanning) {
            this.logger?.debug?.('InstrumentController', 'Scan already in progress');
            return [];
        }

        this.isScanning = true;
        
        try {
            // Ã¢Å“â€¦ CORRECTION: devices.scan retourne count
            const response = await this.backend.scanDevices(full_scan);
            
            // Ã¢Å“â€¦ Extraction via response (dÃƒÂ©jÃƒÂ  data dans BackendService)
            const devices = response.devices || [];
            const count = response.count || devices.length;
            
            devices.forEach(device => {
                this.devices.set(device.id, device);
            });
            
            this.lastScanTime = Date.now();
            
            this.logger?.info?.('InstrumentController', 
                `Ã¢Å“â€œ Scan complete: ${count} devices found`);
            
            this.eventBus.emit('instruments:scan_complete', { 
                devices, 
                count 
            });
            
            this.refreshView();
            
            return devices;
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'scanDevices failed:', error);
            throw error;
        } finally {
            this.isScanning = false;
        }
    }

    async refreshDeviceList() {
        try {
            // Ã¢Å“â€¦ devices.list n'a pas de count
            const response = await this.backend.listDevices();
            const devices = response.devices || [];
            
            devices.forEach(device => {
                this.devices.set(device.id, device);
            });
            
            this.refreshView();
            return devices;
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'refreshDeviceList failed:', error);
            throw error;
        }
    }

    async loadConnectedDevices() {
        if (!this.backend) return [];
        
        try {
            const response = await this.backend.getConnectedDevices();
            const devices = response.devices || [];
            
            this.connectedDevices.clear();
            devices.forEach(device => {
                this.connectedDevices.add(device.id);
            });
            
            this.refreshView();
            return devices;
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'loadConnectedDevices failed:', error);
            return [];
        }
    }

    /**
     * Ã¢Å“â€¦ CORRECTION: device_id en snake_case
     */
    async connectDevice(device_id) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            this.logger?.info?.('InstrumentController', `Connecting device: ${device_id}`);
            
            await this.backend.connectDevice(device_id);
            
            this.connectedDevices.add(device_id);
            
            this.eventBus.emit('instruments:device_connected', { device_id });
            
            this.notifications?.success('Device connected', `Device ${device_id} connected successfully`);
            
            this.refreshView();
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', `Failed to connect device ${device_id}:`, error);
            this.notifications?.error('Connection failed', error.message);
            throw error;
        }
    }

    async disconnectDevice(device_id) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            this.logger?.info?.('InstrumentController', `Disconnecting device: ${device_id}`);
            
            await this.backend.disconnectDevice(device_id);
            
            this.connectedDevices.delete(device_id);
            
            this.eventBus.emit('instruments:device_disconnected', { device_id });
            
            this.notifications?.success('Device disconnected', `Device ${device_id} disconnected`);
            
            this.refreshView();
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', `Failed to disconnect device ${device_id}:`, error);
            this.notifications?.error('Disconnection failed', error.message);
            throw error;
        }
    }

    async disconnectAllDevices() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            this.logger?.info?.('InstrumentController', 'Disconnecting all devices');
            
            await this.backend.disconnectAllDevices();
            
            this.connectedDevices.clear();
            
            this.eventBus.emit('instruments:all_devices_disconnected');
            
            this.notifications?.success('All disconnected', 'All devices disconnected');
            
            this.refreshView();
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'Failed to disconnect all devices:', error);
            this.notifications?.error('Disconnection failed', error.message);
            throw error;
        }
    }

    async getDeviceInfo(device_id) {
        const cached = this.deviceInfoCache.get(device_id);
        if (cached && (Date.now() - cached.timestamp) < this.deviceInfoCacheTTL) {
            return cached.info;
        }
        
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            const info = await this.backend.getDevice(device_id);
            
            this.deviceInfoCache.set(device_id, {
                info,
                timestamp: Date.now()
            });
            
            return info;
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', `getDeviceInfo(${device_id}) failed:`, error);
            throw error;
        }
    }

    async startHotPlugMonitoring() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            await this.backend.startHotPlug();
            this.hotPlugEnabled = true;
            this.logger?.info?.('InstrumentController', 'Hot-plug monitoring started');
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'Failed to start hot-plug:', error);
            throw error;
        }
    }

    async stopHotPlugMonitoring() {
        if (!this.backend) return;
        
        try {
            await this.backend.stopHotPlug();
            this.hotPlugEnabled = false;
            this.logger?.info?.('InstrumentController', 'Hot-plug monitoring stopped');
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'Failed to stop hot-plug:', error);
        }
    }

    async getHotPlugStatus() {
        if (!this.backend) return null;
        
        try {
            return await this.backend.getHotPlugStatus();
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'getHotPlugStatus failed:', error);
            return null;
        }
    }

    handleDeviceConnected(data) {
        const device_id = data.device_id || data.id;
        this.logger?.info?.('InstrumentController', `Device connected: ${device_id}`);
        this.connectedDevices.add(device_id);
        this.refreshView();
    }

    handleDeviceDisconnected(data) {
        const device_id = data.device_id || data.id;
        this.logger?.info?.('InstrumentController', `Device disconnected: ${device_id}`);
        this.connectedDevices.delete(device_id);
        this.refreshView();
    }

    handleDeviceDiscovered(data) {
        const device = data.device;
        if (device) {
            this.devices.set(device.id, device);
            this.refreshView();
        }
    }

    handleDeviceError(data) {
        const device_id = data.device_id;
        const error = data.error;
        this.logger?.error?.('InstrumentController', `Device error (${device_id}):`, error);
        this.notifications?.error('Device error', `Device ${device_id}: ${error}`);
    }

    onInstrumentsPageActive() {
        this.refreshDeviceList();
    }

    onInstrumentsPageInactive() {
        // Rien pour l'instant
    }

    refreshView() {
        if (!this.view) return;
        
        const devicesArray = Array.from(this.devices.values());
        
        this.view.render({
            devices: devicesArray,
            connectedDevices: Array.from(this.connectedDevices),
            hotPlugEnabled: this.hotPlugEnabled
        });
    }

    getDevices() {
        return Array.from(this.devices.values());
    }

    getConnectedDevices() {
        return Array.from(this.connectedDevices);
    }

    isDeviceConnected(device_id) {
        return this.connectedDevices.has(device_id);
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = InstrumentController;
}

if (typeof window !== 'undefined') {
    window.InstrumentController = InstrumentController;
}
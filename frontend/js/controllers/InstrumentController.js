// ============================================================================
// Fichier: frontend/js/controllers/InstrumentController.js
// Version: 3.0.1-FIXED
// Date: 2025-10-20
// ============================================================================
// CORRECTIONS v3.0.1:
// ✅ Fixed constructor signature to match BaseController
// ✅ Proper initialization order with _fullyInitialized
// ✅ Logger initialized first
// ✅ Compatible with Application.js instantiation
// ============================================================================

class InstrumentController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Initialize logger FIRST
        this.logger = window.Logger || console;
        
        // Get specific model and view
        this.model = models.instrument;
        this.view = views.instrument;
        
        // Backend service
        this.backendService = window.app?.services?.backend || window.backendService;
        
        // État local
        this.devices = new Map();
        this.selectedDevice = null;
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        // Now setup
        this.setupEventListeners();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    setupEventListeners() {
        if (!this.eventBus || typeof this.eventBus.on !== 'function') {
            console.error('[InstrumentController] EventBus not available or invalid');
            return;
        }
        
        // Événements UI
        this.eventBus.on('instrument:select', (data) => this.selectDevice(data.deviceId));
        this.eventBus.on('instrument:connect', (data) => this.connectDevice(data.deviceId));
        this.eventBus.on('instrument:disconnect', (data) => this.disconnectDevice(data.deviceId));
        this.eventBus.on('instrument:scan', () => this.scanDevices());
        this.eventBus.on('instrument:refresh', () => this.refreshDeviceList());
        this.eventBus.on('instrument:getProfile', (data) => this.getDeviceProfile(data.deviceId));
        
        // Événements backend (NOUVEAU PROTOCOLE)
        this.eventBus.on('device:connected', (data) => this.handleDeviceConnected(data));
        this.eventBus.on('device:disconnected', (data) => this.handleDeviceDisconnected(data));
        this.eventBus.on('device:discovered', (data) => this.handleDeviceDiscovered(data));
        
        // Événements SysEx
        this.eventBus.on('sysex:identity', (data) => this.handleSysExIdentity(data));
        this.eventBus.on('sysex:notemap', (data) => this.handleSysExNoteMap(data));
        this.eventBus.on('sysex:cc_capabilities', (data) => this.handleSysExCC(data));
        this.eventBus.on('sysex:air_capabilities', (data) => this.handleSysExAir(data));
        this.eventBus.on('sysex:light_capabilities', (data) => this.handleSysExLight(data));
        this.eventBus.on('sysex:sensors', (data) => this.handleSysExSensors(data));
        this.eventBus.on('sysex:sync_clock', (data) => this.handleSysExSync(data));
        
        // Charger la liste initiale
        this.refreshDeviceList();
        
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', '✓ Event listeners setup');
        }
    }
    
    // ========================================================================
    // GESTION DES DEVICES
    // ========================================================================
    
    async refreshDeviceList() {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', 'Refreshing device list...');
        }
        
        if (!this.backendService) {
            if (this.logger && this.logger.warn) {
                this.logger.warn('InstrumentController', 'Backend service not available');
            }
            return;
        }
        
        try {
            const result = await this.backendService.sendCommand('devices.list');
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to load devices');
            }
            
            const devices = result.data?.devices || [];
            
            // Update devices map
            this.devices.clear();
            devices.forEach(device => {
                this.devices.set(device.id, device);
            });
            
            // Update model if available
            if (this.model && typeof this.model.loadInstruments === 'function') {
                await this.model.loadInstruments(devices);
            }
            
            // Update view if available
            if (this.view && typeof this.view.render === 'function') {
                this.view.render({ instruments: devices });
            }
            
            if (this.logger && this.logger.info) {
                this.logger.info('InstrumentController', `✓ ${devices.length} devices loaded`);
            }
            
            // Emit event
            this.eventBus.emit('instruments:loaded', { devices, count: devices.length });
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('InstrumentController', 'Failed to refresh devices:', error);
            }
            this.showError(`Failed to load devices: ${error.message}`);
        }
    }
    
    async selectDevice(deviceId) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', `Selecting device: ${deviceId}`);
        }
        
        this.selectedDevice = deviceId;
        const device = this.devices.get(deviceId);
        
        if (!device) {
            if (this.logger && this.logger.warn) {
                this.logger.warn('InstrumentController', `Device not found: ${deviceId}`);
            }
            return;
        }
        
        // Emit event
        this.eventBus.emit('instrument:selected', { deviceId, device });
        
        // Update view
        if (this.view && typeof this.view.updateSelection === 'function') {
            this.view.updateSelection(deviceId);
        }
    }
    
    async connectDevice(deviceId) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', `Connecting device: ${deviceId}`);
        }
        
        if (!this.backendService) {
            this.showError('Backend service not available');
            return;
        }
        
        try {
            const result = await this.backendService.sendCommand('devices.connect', {
                device_id: deviceId
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Connection failed');
            }
            
            this.showSuccess(`Device connected: ${deviceId}`);
            await this.refreshDeviceList();
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('InstrumentController', 'Connection failed:', error);
            }
            this.showError(`Connection failed: ${error.message}`);
        }
    }
    
    async disconnectDevice(deviceId) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', `Disconnecting device: ${deviceId}`);
        }
        
        if (!this.backendService) {
            this.showError('Backend service not available');
            return;
        }
        
        try {
            const result = await this.backendService.sendCommand('devices.disconnect', {
                device_id: deviceId
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Disconnection failed');
            }
            
            this.showSuccess(`Device disconnected: ${deviceId}`);
            await this.refreshDeviceList();
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('InstrumentController', 'Disconnection failed:', error);
            }
            this.showError(`Disconnection failed: ${error.message}`);
        }
    }
    
    async scanDevices() {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', 'Scanning for devices...');
        }
        
        if (!this.backendService) {
            this.showError('Backend service not available');
            return;
        }
        
        try {
            const result = await this.backendService.sendCommand('devices.scan');
            
            if (result.success === false) {
                throw new Error(result.error || 'Scan failed');
            }
            
            this.showSuccess('Device scan completed');
            await this.refreshDeviceList();
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('InstrumentController', 'Scan failed:', error);
            }
            this.showError(`Scan failed: ${error.message}`);
        }
    }
    
    async getDeviceProfile(deviceId) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', `Getting profile for: ${deviceId}`);
        }
        
        if (!this.backendService) {
            this.showError('Backend service not available');
            return null;
        }
        
        try {
            const result = await this.backendService.sendCommand('devices.getProfile', {
                device_id: deviceId
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to get profile');
            }
            
            return result.data;
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('InstrumentController', 'Failed to get profile:', error);
            }
            return null;
        }
    }
    
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================
    
    handleDeviceConnected(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', `Device connected: ${data.device_id}`);
        }
        
        this.refreshDeviceList();
    }
    
    handleDeviceDisconnected(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', `Device disconnected: ${data.device_id}`);
        }
        
        this.refreshDeviceList();
    }
    
    handleDeviceDiscovered(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', `Device discovered: ${data.device_id}`);
        }
        
        this.refreshDeviceList();
    }
    
    handleSysExIdentity(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', 'SysEx Identity received:', data.device_id);
        }
        
        const deviceId = data.device_id;
        const device = this.devices.get(deviceId);
        
        if (device) {
            device.sysex = device.sysex || {};
            device.sysex.identity = {
                manufacturer: data.manufacturer,
                family: data.family,
                model: data.model,
                version: data.version
            };
        }
    }
    
    handleSysExNoteMap(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', 'SysEx Note Map received:', data.device_id);
        }
    }
    
    handleSysExCC(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', 'SysEx CC Capabilities received:', data.device_id);
        }
    }
    
    handleSysExAir(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', 'SysEx Air Capabilities received:', data.device_id);
        }
    }
    
    handleSysExLight(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', 'SysEx Light Capabilities received:', data.device_id);
        }
    }
    
    handleSysExSensors(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', 'SysEx Sensors received:', data.device_id);
        }
    }
    
    handleSysExSync(data) {
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentController', 'SysEx Sync Clock received:', data.device_id);
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    showError(message) {
        if (this.notifications && typeof this.notifications.show === 'function') {
            this.notifications.show(message, 'error', { duration: 5000 });
        } else {
            this.eventBus.emit('notification:show', {
                type: 'error',
                message: message,
                duration: 5000
            });
        }
    }
    
    showSuccess(message) {
        if (this.notifications && typeof this.notifications.show === 'function') {
            this.notifications.show(message, 'success', { duration: 3000 });
        } else {
            this.eventBus.emit('notification:show', {
                type: 'success',
                message: message,
                duration: 3000
            });
        }
    }
    
    // ========================================================================
    // API PUBLIQUE
    // ========================================================================
    
    getDevices() {
        return Array.from(this.devices.values());
    }
    
    getDevice(deviceId) {
        return this.devices.get(deviceId);
    }
    
    getSelectedDevice() {
        return this.selectedDevice ? this.devices.get(this.selectedDevice) : null;
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = InstrumentController;
}

if (typeof window !== 'undefined') {
    window.InstrumentController = InstrumentController;
}
window.InstrumentController = InstrumentController;
// ============================================================================
// FIN DU FICHIER InstrumentController.js v3.0.1-FIXED
// ============================================================================
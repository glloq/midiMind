// ============================================================================
// Fichier: frontend/js/controllers/InstrumentController.js
// Version: 3.0.0-refonte
// Date: 2025-10-09
// ============================================================================
// Description:
//   Contrôleur de gestion des instruments MIDI - MIGRÉ vers protocole v3.0
//   Gestion devices, profils SysEx, connexion/déconnexion.
// ============================================================================

class InstrumentController extends BaseController {
    constructor(model, view, eventBus, backendService, logger) {
        super('InstrumentController', model, view, eventBus, logger);
        
        this.backendService = backendService;
        
        // État local
        this.devices = new Map();
        this.selectedDevice = null;
        
        this.setupEventListeners();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    setupEventListeners() {
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
    }
    
    // ========================================================================
    // GESTION DES DEVICES
    // ========================================================================
    
    async refreshDeviceList() {
        this.logger.info(this.name, 'Refreshing device list...');
        
        try {
            const result = await this.backendService.sendCommand('devices.list');
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to load devices');
            }
            
            const devices = result.data ? result.data.devices : result.devices || [];
            
            this.logger.info(this.name, `Loaded ${devices.length} devices`);
            
            // Mettre à jour le cache local
            this.devices.clear();
            devices.forEach(device => {
                this.devices.set(device.id, device);
            });
            
            // Mettre à jour le modèle
            this.model.setDevices(devices);
            
            // Mettre à jour la vue
            this.view.updateDeviceList(devices);
            
            // Notifier
            this.eventBus.emit('devices:loaded', { count: devices.length });
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to refresh device list:', error);
            this.showError('Failed to load devices: ' + error.message);
        }
    }
    
    async scanDevices() {
        this.logger.info(this.name, 'Scanning for devices...');
        
        try {
            this.view.showLoading('Scanning devices...');
            
            const result = await this.backendService.sendCommand('devices.scan');
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to scan devices');
            }
            
            this.logger.info(this.name, 'Scan initiated');
            
            // Attendre un peu puis rafraîchir
            setTimeout(() => {
                this.view.hideLoading();
                this.refreshDeviceList();
            }, 2000);
            
            this.showSuccess('Scanning for devices...');
            
        } catch (error) {
            this.view.hideLoading();
            this.logger.error(this.name, 'Failed to scan devices:', error);
            this.showError('Failed to scan: ' + error.message);
        }
    }
    
    selectDevice(deviceId) {
        this.logger.info(this.name, `Selecting device: ${deviceId}`);
        
        this.selectedDevice = deviceId;
        
        const device = this.devices.get(deviceId);
        
        if (device) {
            // Mettre à jour le modèle
            this.model.setSelectedDevice(device);
            
            // Mettre à jour la vue
            this.view.updateSelectedDevice(device);
            
            // Charger le profil si connecté
            if (device.connected) {
                this.getDeviceProfile(deviceId);
            }
        }
    }
    
    async connectDevice(deviceId) {
        this.logger.info(this.name, `Connecting device: ${deviceId}`);
        
        try {
            this.view.showLoading('Connecting...');
            
            const result = await this.backendService.sendCommand('devices.connect', {
                device_id: deviceId
            });
            
            this.view.hideLoading();
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to connect device');
            }
            
            this.logger.info(this.name, 'Device connected');
            
            // Mettre à jour localement
            const device = this.devices.get(deviceId);
            if (device) {
                device.connected = true;
                this.view.updateDeviceStatus(deviceId, 'connected');
            }
            
            this.showSuccess('Device connected');
            
            // Charger le profil
            this.getDeviceProfile(deviceId);
            
        } catch (error) {
            this.view.hideLoading();
            this.logger.error(this.name, 'Failed to connect device:', error);
            this.showError('Failed to connect: ' + error.message);
        }
    }
    
    async disconnectDevice(deviceId) {
        this.logger.info(this.name, `Disconnecting device: ${deviceId}`);
        
        try {
            this.view.showLoading('Disconnecting...');
            
            const result = await this.backendService.sendCommand('devices.disconnect', {
                device_id: deviceId
            });
            
            this.view.hideLoading();
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to disconnect device');
            }
            
            this.logger.info(this.name, 'Device disconnected');
            
            // Mettre à jour localement
            const device = this.devices.get(deviceId);
            if (device) {
                device.connected = false;
                this.view.updateDeviceStatus(deviceId, 'disconnected');
            }
            
            this.showSuccess('Device disconnected');
            
        } catch (error) {
            this.view.hideLoading();
            this.logger.error(this.name, 'Failed to disconnect device:', error);
            this.showError('Failed to disconnect: ' + error.message);
        }
    }
    
    // ========================================================================
    // PROFIL SYSEX
    // ========================================================================
    
    async getDeviceProfile(deviceId) {
        this.logger.info(this.name, `Getting profile for device: ${deviceId}`);
        
        try {
            const result = await this.backendService.sendCommand('instruments.getProfile', {
                device_id: deviceId
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to get profile');
            }
            
            const profile = result.data || result;
            
            this.logger.debug(this.name, 'Device profile:', profile);
            
            // Mettre à jour le modèle
            this.model.setDeviceProfile(deviceId, profile);
            
            // Mettre à jour la vue
            this.view.updateDeviceProfile(deviceId, profile);
            
        } catch (error) {
            this.logger.error(this.name, 'Failed to get device profile:', error);
        }
    }
    
    // ========================================================================
    // ÉVÉNEMENTS BACKEND (NOUVEAU)
    // ========================================================================
    
    handleDeviceConnected(data) {
        this.logger.info(this.name, 'Device connected:', data.device_id);
        
        const deviceId = data.device_id;
        
        // Mettre à jour localement
        const device = this.devices.get(deviceId);
        if (device) {
            device.connected = true;
        } else {
            // Ajouter le nouveau device
            this.devices.set(deviceId, {
                id: deviceId,
                name: data.name || deviceId,
                connected: true
            });
        }
        
        // Rafraîchir la liste
        this.refreshDeviceList();
        
        // Notification
        this.showSuccess(`Device ${data.name || deviceId} connected`);
    }
    
    handleDeviceDisconnected(data) {
        this.logger.info(this.name, 'Device disconnected:', data.device_id);
        
        const deviceId = data.device_id;
        
        // Mettre à jour localement
        const device = this.devices.get(deviceId);
        if (device) {
            device.connected = false;
        }
        
        // Rafraîchir la liste
        this.refreshDeviceList();
        
        // Notification
        this.showError(`Device ${data.name || deviceId} disconnected`);
    }
    
    handleDeviceDiscovered(data) {
        this.logger.info(this.name, 'Device discovered:', data.device_id);
        
        // Rafraîchir la liste
        this.refreshDeviceList();
    }
    
    // ========================================================================
    // ÉVÉNEMENTS SYSEX (NOUVEAU)
    // ========================================================================
    
    handleSysExIdentity(data) {
        this.logger.info(this.name, 'SysEx Identity received:', data.device_id);
        
        const deviceId = data.device_id;
        
        // Mettre à jour le profil
        const profile = this.model.getDeviceProfile(deviceId) || {};
        profile.identity = {
            manufacturer: data.manufacturer,
            model: data.model,
            version: data.version
        };
        
        this.model.setDeviceProfile(deviceId, profile);
        this.view.updateDeviceProfile(deviceId, profile);
    }
    
    handleSysExNoteMap(data) {
        this.logger.info(this.name, 'SysEx NoteMap received:', data.device_id);
        
        const deviceId = data.device_id;
        
        const profile = this.model.getDeviceProfile(deviceId) || {};
        profile.noteMap = {
            playableNotes: data.playable_notes,
            octaveRange: data.octave_range
        };
        
        this.model.setDeviceProfile(deviceId, profile);
        this.view.updateDeviceProfile(deviceId, profile);
    }
    
    handleSysExCC(data) {
        this.logger.info(this.name, 'SysEx CC Capabilities received:', data.device_id);
        
        const deviceId = data.device_id;
        
        const profile = this.model.getDeviceProfile(deviceId) || {};
        profile.ccCapabilities = {
            supportedCCs: data.supported_ccs
        };
        
        this.model.setDeviceProfile(deviceId, profile);
        this.view.updateDeviceProfile(deviceId, profile);
    }
    
    handleSysExAir(data) {
        this.logger.info(this.name, 'SysEx Air Capabilities received:', data.device_id);
        
        const deviceId = data.device_id;
        
        const profile = this.model.getDeviceProfile(deviceId) || {};
        profile.airCapabilities = {
            breathControl: data.breath_control,
            aftertouch: data.aftertouch
        };
        
        this.model.setDeviceProfile(deviceId, profile);
        this.view.updateDeviceProfile(deviceId, profile);
    }
    
    handleSysExLight(data) {
        this.logger.info(this.name, 'SysEx Light Capabilities received:', data.device_id);
        
        const deviceId = data.device_id;
        
        const profile = this.model.getDeviceProfile(deviceId) || {};
        profile.lightCapabilities = {
            rgbSupport: data.rgb_support,
            brightnessLevels: data.brightness_levels
        };
        
        this.model.setDeviceProfile(deviceId, profile);
        this.view.updateDeviceProfile(deviceId, profile);
    }
    
    handleSysExSensors(data) {
        this.logger.info(this.name, 'SysEx Sensors received:', data.device_id);
        
        const deviceId = data.device_id;
        
        const profile = this.model.getDeviceProfile(deviceId) || {};
        profile.sensors = {
            gyroscope: data.gyroscope,
            accelerometer: data.accelerometer
        };
        
        this.model.setDeviceProfile(deviceId, profile);
        this.view.updateDeviceProfile(deviceId, profile);
    }
    
    handleSysExSync(data) {
        this.logger.info(this.name, 'SysEx Sync Clock received:', data.device_id);
        
        const deviceId = data.device_id;
        
        const profile = this.model.getDeviceProfile(deviceId) || {};
        profile.syncClock = {
            midiClock: data.midi_clock,
            mtc: data.mtc,
            internalBPM: data.internal_bpm
        };
        
        this.model.setDeviceProfile(deviceId, profile);
        this.view.updateDeviceProfile(deviceId, profile);
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    showError(message) {
        this.eventBus.emit('notification:show', {
            type: 'error',
            message: message,
            duration: 5000
        });
    }
    
    showSuccess(message) {
        this.eventBus.emit('notification:show', {
            type: 'success',
            message: message,
            duration: 3000
        });
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

// ============================================================================
// FIN DU FICHIER InstrumentController.js
// ============================================================================

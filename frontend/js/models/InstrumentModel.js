// ============================================================================
// Fichier: frontend/js/models/InstrumentModel.js
// Chemin réel: frontend/js/models/InstrumentModel.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE
// Date: 2025-10-31
// ============================================================================

class InstrumentModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, backend, logger, {}, {
            persistKey: 'instrumentmodel',
            eventPrefix: 'instrument',
            autoPersist: false
        });
        
        this.data.devices = [];
        this.data.connected = [];
        this.data.currentDevice = null;
        
        this.log('debug', 'InstrumentModel', 'Initialized');
    }
    
    async listDevices() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'InstrumentModel.listDevices', 'Backend not connected');
            return [];
        }
        
        try {
            const response = await this.backend.send('devices.list', {});
            
            if (response.success && response.data.devices) {
                this.data.devices = response.data.devices;
                this.emit('devices:updated', { devices: this.data.devices });
                return this.data.devices;
            }
        } catch (error) {
            this.log('error', 'InstrumentModel.listDevices', error);
        }
        
        return [];
    }
    
    async scanDevices(fullScan = false) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'InstrumentModel.scanDevices', 'Backend not connected');
            return false;
        }
        
        try {
            const response = await this.backend.send('devices.scan', { full_scan: fullScan });
            
            if (response.success) {
                await this.listDevices();
                return true;
            }
        } catch (error) {
            this.log('error', 'InstrumentModel.scanDevices', error);
        }
        
        return false;
    }
    
    async connectDevice(deviceId) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'InstrumentModel.connectDevice', 'Backend not connected');
            return false;
        }
        
        try {
            const response = await this.backend.send('devices.connect', { device_id: deviceId });
            
            if (response.success) {
                this.emit('device:connected', { deviceId });
                await this.listConnected();
                return true;
            }
        } catch (error) {
            this.log('error', 'InstrumentModel.connectDevice', error);
        }
        
        return false;
    }
    
    async disconnectDevice(deviceId) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'InstrumentModel.disconnectDevice', 'Backend not connected');
            return false;
        }
        
        try {
            const response = await this.backend.send('devices.disconnect', { device_id: deviceId });
            
            if (response.success) {
                this.emit('device:disconnected', { deviceId });
                await this.listConnected();
                return true;
            }
        } catch (error) {
            this.log('error', 'InstrumentModel.disconnectDevice', error);
        }
        
        return false;
    }
    
    async listConnected() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'InstrumentModel.listConnected', 'Backend not connected');
            return [];
        }
        
        try {
            const response = await this.backend.send('devices.getConnected', {});
            
            if (response.success && response.data.devices) {
                this.data.connected = response.data.devices;
                this.emit('connected:updated', { connected: this.data.connected });
                return this.data.connected;
            }
        } catch (error) {
            this.log('error', 'InstrumentModel.listConnected', error);
        }
        
        return [];
    }
    
    getDevices() {
        return this.data.devices;
    }
    
    getConnectedDevices() {
        return this.data.connected;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = InstrumentModel;
}

if (typeof window !== 'undefined') {
    window.InstrumentModel = InstrumentModel;
}
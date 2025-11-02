// ============================================================================
// Fichier: frontend/js/models/SystemModel.js
// Chemin rÃ©el: frontend/js/models/SystemModel.js
// Version: v4.0.0 - API COMPATIBLE v4.2.2
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v4.0.0:
// âœ… RemplacÃ© backend.send() par backend.sendCommand()
// âœ… Compatible avec BackendService v4.0.0
// ============================================================================

class SystemModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, backend, logger, {}, {
            persistKey: 'systemmodel',
            eventPrefix: 'system',
            autoPersist: false
        });
        
        this.data.info = null;
        this.data.version = null;
        this.data.uptime = 0;
        this.data.memory = null;
        this.data.disk = null;
        
        this.log('debug', 'SystemModel', 'Initialized (v4.0.0)');
    }
    
    async ping() {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            // âœ… Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.ping', {});
            return response.pong || false;
        } catch (error) {
            return false;
        }
    }
    
    async getVersion() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            // âœ… Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.version', {});
            this.data.version = response;
            return response;
        } catch (error) {
            this.log('error', 'SystemModel.getVersion', error);
        }
        
        return null;
    }
    
    async getInfo() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            // âœ… Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.info', {});
            this.data.info = response;
            return response;
        } catch (error) {
            this.log('error', 'SystemModel.getInfo', error);
        }
        
        return null;
    }
    
    async getUptime() {
        if (!this.backend || !this.backend.isConnected()) return 0;
        
        try {
            // âœ… Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.uptime', {});
            this.data.uptime = response.uptime || response || 0;
            return this.data.uptime;
        } catch (error) {
            this.log('error', 'SystemModel.getUptime', error);
        }
        
        return 0;
    }
    
    async getMemory() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            // âœ… Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.memory', {});
            this.data.memory = response;
            return response;
        } catch (error) {
            this.log('error', 'SystemModel.getMemory', error);
        }
        
        return null;
    }
    
    async getDisk() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            // âœ… Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.disk', {});
            this.data.disk = response;
            return response;
        } catch (error) {
            this.log('error', 'SystemModel.getDisk', error);
        }
        
        return null;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = SystemModel;
}

if (typeof window !== 'undefined') {
    window.SystemModel = SystemModel;
}
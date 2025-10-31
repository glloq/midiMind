// ============================================================================
// Fichier: frontend/js/models/SystemModel.js
// Chemin réel: frontend/js/models/SystemModel.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE
// Date: 2025-10-31
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
        
        this.log('debug', 'SystemModel', 'Initialized');
    }
    
    async ping() {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            const response = await this.backend.send('system.ping', {});
            return response.success && response.data.pong;
        } catch (error) {
            return false;
        }
    }
    
    async getVersion() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            const response = await this.backend.send('system.version', {});
            if (response.success) {
                this.data.version = response.data;
                return response.data;
            }
        } catch (error) {
            this.log('error', 'SystemModel.getVersion', error);
        }
        
        return null;
    }
    
    async getInfo() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            const response = await this.backend.send('system.info', {});
            if (response.success) {
                this.data.info = response.data;
                return response.data;
            }
        } catch (error) {
            this.log('error', 'SystemModel.getInfo', error);
        }
        
        return null;
    }
    
    async getUptime() {
        if (!this.backend || !this.backend.isConnected()) return 0;
        
        try {
            const response = await this.backend.send('system.uptime', {});
            if (response.success) {
                this.data.uptime = response.data.uptime || 0;
                return this.data.uptime;
            }
        } catch (error) {
            this.log('error', 'SystemModel.getUptime', error);
        }
        
        return 0;
    }
    
    async getMemory() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            const response = await this.backend.send('system.memory', {});
            if (response.success) {
                this.data.memory = response.data;
                return response.data;
            }
        } catch (error) {
            this.log('error', 'SystemModel.getMemory', error);
        }
        
        return null;
    }
    
    async getDisk() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            const response = await this.backend.send('system.disk', {});
            if (response.success) {
                this.data.disk = response.data;
                return response.data;
            }
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
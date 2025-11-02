// ============================================================================
// Fichier: frontend/js/models/SystemModel.js
// Chemin réel: frontend/js/models/SystemModel.js
// Version: v4.2.2 - API COMPATIBLE v4.2.2
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v4.2.2:
// ✅ Remplacé backend.send() par backend.sendCommand()
// ✅ Compatible avec BackendService v4.0.0
// ✅ Extraction response.data corrigée
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
        
        this.log('debug', 'SystemModel', 'Initialized (v4.2.2)');
    }
    
    async ping() {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            // ✅ Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.ping', {});
            const data = response.data || response;
            return data.pong || false;
        } catch (error) {
            return false;
        }
    }
    
    async getVersion() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            // ✅ Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.version', {});
            const data = response.data || response;
            this.data.version = data;
            return data;
        } catch (error) {
            this.log('error', 'SystemModel.getVersion', error);
        }
        
        return null;
    }
    
    async getInfo() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            // ✅ Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.info', {});
            const data = response.data || response;
            this.data.info = data;
            return data;
        } catch (error) {
            this.log('error', 'SystemModel.getInfo', error);
        }
        
        return null;
    }
    
    async getUptime() {
        if (!this.backend || !this.backend.isConnected()) return 0;
        
        try {
            // ✅ Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.uptime', {});
            const data = response.data || response;
            this.data.uptime = data.uptime || data || 0;
            return this.data.uptime;
        } catch (error) {
            this.log('error', 'SystemModel.getUptime', error);
        }
        
        return 0;
    }
    
    async getMemory() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            // ✅ Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.memory', {});
            const data = response.data || response;
            this.data.memory = data;
            return data;
        } catch (error) {
            this.log('error', 'SystemModel.getMemory', error);
        }
        
        return null;
    }
    
    async getDisk() {
        if (!this.backend || !this.backend.isConnected()) return null;
        
        try {
            // ✅ Utilise sendCommand au lieu de send
            const response = await this.backend.sendCommand('system.disk', {});
            const data = response.data || response;
            this.data.disk = data;
            return data;
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
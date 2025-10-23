// ============================================================================
// Fichier: frontend/js/models/SystemModel.js
// Version: v3.0.7 - MINIMAL + OFFLINE MODE FIX
// Date: 2025-10-22
// ============================================================================
// CORRECTIONS v3.0.7:
// âœ… Ajout de _isBackendAvailable() pour vÃ©rifier le backend
// âœ… Gestion gracieuse du mode hors ligne (pas d'erreurs rÃ©pÃ©titives)
// âœ… Toutes les mÃ©thodes vÃ©rifient le backend avant utilisation
// ============================================================================


class SystemModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // âœ… FIX: Correct super() call
        super({}, {
            persistKey: 'systemmodel',
            eventPrefix: 'system',
            autoPersist: true
        });
        
        // âœ… FIX: Assign immediately
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // âœ… FIX: Initialize data directly
        this.data = {
            status: 'unknown',
            cpu: 0,
            memory: 0,
            temperature: 0,
            uptime: 0,
            
            // Configuration
            audioLatency: 10,
            bufferSize: 256,
            sampleRate: 44100,
            midiLatency: 5
        };
        
        this.logger.info('SystemModel', 'âœ“ Model initialized (minimal version)');
        
        // DÃ©marrer monitoring simple
        this.startMonitoring();
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * VÃ©rifie si le backend est disponible
     * @private
     */
    _isBackendAvailable() {
        return this.backend && typeof this.backend.sendCommand === 'function';
    }
    
    // ========================================================================
    // MONITORING SIMPLE
    // ========================================================================
    
    startMonitoring() {
        // RafraÃ®chir toutes les 5 secondes
        this.monitoringTimer = setInterval(() => {
            this.refreshSystemInfo();
        }, 5000);
        
        // Premier refresh immÃ©diat
        this.refreshSystemInfo();
        
        this.logger.debug('SystemModel', 'Monitoring started');
    }
    
    stopMonitoring() {
        if (this.monitoringTimer) {
            clearInterval(this.monitoringTimer);
            this.monitoringTimer = null;
        }
        
        this.logger.debug('SystemModel', 'Monitoring stopped');
    }
    
    /**
     * RafraÃ®chit les informations systÃ¨me
     */
    async refreshSystemInfo() {
        // VÃ©rifier si backend est disponible
        if (!this._isBackendAvailable()) {
            // Mode hors ligne - retourner des donnÃ©es par dÃ©faut sans erreur
            return {
                status: 'offline',
                cpu: 0,
                memory: 0,
                temperature: 0,
                uptime: 0
            };
        }
        
        try {
            const response = await this.backend.sendCommand('system.get-info', {});
            
            if (response.success && response.data) {
                const info = response.data;
                
                // Mettre Ã  jour silencieusement
                this.update({
                    status: info.status || 'ready',
                    cpu: info.cpu || 0,
                    memory: info.memory || 0,
                    temperature: info.temperature || 0,
                    uptime: info.uptime || 0
                }, { silent: true });
                
                // Ã‰mettre Ã©vÃ©nement
                this.eventBus.emit('system:info-updated', { info });
                
                return info;
            }
            
        } catch (error) {
            this.logger.error('SystemModel', `Refresh failed: ${error.message}`);
        }
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * Met Ã  jour la configuration audio
     */
    async updateAudioConfig(config) {
        if (!this._isBackendAvailable()) {
            this.logger.warn('SystemModel', 'Cannot update audio config: backend not available');
            return false;
        }
        
        try {
            this.logger.info('SystemModel', 'Updating audio config');
            
            const response = await this.backend.sendCommand('system.set-audio-config', {
                config: config
            });
            
            if (response.success) {
                this.update({
                    audioLatency: config.latency || this.get('audioLatency'),
                    bufferSize: config.bufferSize || this.get('bufferSize'),
                    sampleRate: config.sampleRate || this.get('sampleRate')
                });
                
                this.logger.info('SystemModel', 'Audio config updated');
                return true;
            }
            
        } catch (error) {
            this.logger.error('SystemModel', `Audio config update failed: ${error.message}`);
        }
        
        return false;
    }
    
    /**
     * Met Ã  jour la configuration MIDI
     */
    async updateMidiConfig(config) {
        if (!this._isBackendAvailable()) {
            this.logger.warn('SystemModel', 'Cannot update MIDI config: backend not available');
            return false;
        }
        
        try {
            this.logger.info('SystemModel', 'Updating MIDI config');
            
            const response = await this.backend.sendCommand('system.set-midi-config', {
                config: config
            });
            
            if (response.success) {
                this.update({
                    midiLatency: config.latency || this.get('midiLatency')
                });
                
                this.logger.info('SystemModel', 'MIDI config updated');
                return true;
            }
            
        } catch (error) {
            this.logger.error('SystemModel', `MIDI config update failed: ${error.message}`);
        }
        
        return false;
    }
    
    // ========================================================================
    // ACTIONS SYSTÃˆME
    // ========================================================================
    
    /**
     * RedÃ©marre le systÃ¨me
     */
    async restart() {
        if (!this._isBackendAvailable()) {
            this.logger.warn('SystemModel', 'Cannot restart: backend not available');
            return false;
        }
        
        try {
            this.logger.warn('SystemModel', 'System restart requested');
            
            const response = await this.backend.sendCommand('system.restart', {});
            
            if (response.success) {
                this.logger.info('SystemModel', 'System restarting...');
                return true;
            }
            
        } catch (error) {
            this.logger.error('SystemModel', `Restart failed: ${error.message}`);
        }
        
        return false;
    }
    
    /**
     * Ã‰teint le systÃ¨me
     */
    async shutdown() {
        if (!this._isBackendAvailable()) {
            this.logger.warn('SystemModel', 'Cannot shutdown: backend not available');
            return false;
        }
        
        try {
            this.logger.warn('SystemModel', 'System shutdown requested');
            
            const response = await this.backend.sendCommand('system.shutdown', {});
            
            if (response.success) {
                this.logger.info('SystemModel', 'System shutting down...');
                return true;
            }
            
        } catch (error) {
            this.logger.error('SystemModel', `Shutdown failed: ${error.message}`);
        }
        
        return false;
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    getSystemInfo() {
        return {
            status: this.get('status'),
            cpu: this.get('cpu'),
            memory: this.get('memory'),
            temperature: this.get('temperature'),
            uptime: this.get('uptime')
        };
    }
    
    getAudioConfig() {
        return {
            latency: this.get('audioLatency'),
            bufferSize: this.get('bufferSize'),
            sampleRate: this.get('sampleRate')
        };
    }
    
    getMidiConfig() {
        return {
            latency: this.get('midiLatency')
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = SystemModel;
}

if (typeof window !== 'undefined') {
    window.SystemModel = SystemModel;
}

// Export par défaut
window.SystemModel = SystemModel;
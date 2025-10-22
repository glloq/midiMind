// ============================================================================
// Fichier: frontend/js/models/SystemModel.js
// Version: v3.0.6 - MINIMAL (Constructor fixed + basic functions only)
// Date: 2025-10-19
// ============================================================================
// SIMPLIFICATION: Seulement les fonctions de base
// - Infos systÃ¨me (CPU, RAM, temp)
// - Configuration basique
// - Pas de monitoring avancÃ©
// - Pas de logs complexes
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
    
    /**
     * Vérifie si le backend est disponible
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
     * Rafraîchit les informations système
     */
    async refreshSystemInfo() {
        // Vérifier si backend est disponible
        if (!this._isBackendAvailable()) {
            // Mode hors ligne - retourner des données par défaut sans erreur
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
        try {
        if (!this._isBackendAvailable()) {
            this.logger.warn('SystemModel', 'Cannot update audio config: backend not available');
            return false;
        }

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
                
                this.eventBus.emit('system:audio-config-updated', { config });
                
                return true;
            }
            
            throw new Error(response.error || 'Failed to update audio config');
            
        } catch (error) {
            this.logger.error('SystemModel', `Update audio config failed: ${error.message}`);
            throw error;
        }
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
                
                this.eventBus.emit('system:midi-config-updated', { config });
                
                return true;
            }
            
            throw new Error(response.error || 'Failed to update MIDI config');
            
        } catch (error) {
            this.logger.error('SystemModel', `Update MIDI config failed: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // ACTIONS SYSTÃˆME
    // ========================================================================
    
    /**
     * RedÃ©marre le systÃ¨me
     */
        if (!this._isBackendAvailable()) {
            this.logger.warn('SystemModel', 'Cannot restart: backend not available');
            return false;
        }

    async restart() {
        try {
            this.logger.warn('SystemModel', 'System restart requested');
            
            const response = await this.backend.sendCommand('system.restart', {});
            
            if (response.success) {
                this.eventBus.emit('system:restarting');
                return true;
            }
            
            throw new Error(response.error || 'Failed to restart system');
            
        } catch (error) {
            this.logger.error('SystemModel', `Restart failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * ArrÃªte le systÃ¨me
     */
        if (!this._isBackendAvailable()) {
            this.logger.warn('SystemModel', 'Cannot shutdown: backend not available');
            return false;
        }

    async shutdown() {
        try {
            this.logger.warn('SystemModel', 'System shutdown requested');
            
            const response = await this.backend.sendCommand('system.shutdown', {});
            
            if (response.success) {
                this.eventBus.emit('system:shutting-down');
                return true;
            }
            
            throw new Error(response.error || 'Failed to shutdown system');
            
        } catch (error) {
            this.logger.error('SystemModel', `Shutdown failed: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    getStatus() {
        return this.get('status');
    }
    
    isReady() {
        return this.get('status') === 'ready';
    }
    
    getUptime() {
        return this.get('uptime');
    }
    
    getSystemInfo() {
        return {
            status: this.get('status'),
            cpu: this.get('cpu'),
            memory: this.get('memory'),
            temperature: this.get('temperature'),
            uptime: this.get('uptime')
        };
    }
    
    getConfig() {
        return {
            audioLatency: this.get('audioLatency'),
            bufferSize: this.get('bufferSize'),
            sampleRate: this.get('sampleRate'),
            midiLatency: this.get('midiLatency')
        };
    }
    
    /**
     * Nettoyage
     */
    destroy() {
        this.stopMonitoring();
        super.destroy();
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
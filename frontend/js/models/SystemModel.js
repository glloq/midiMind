// ============================================================================
// Fichier: frontend/js/models/SystemModel.js
// Version: v3.0.6 - MINIMAL (Constructor fixed + basic functions only)
// Date: 2025-10-19
// ============================================================================
// SIMPLIFICATION: Seulement les fonctions de base
// - Infos système (CPU, RAM, temp)
// - Configuration basique
// - Pas de monitoring avancé
// - Pas de logs complexes
// ============================================================================

class SystemModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // ✅ FIX: Correct super() call
        super({}, {
            persistKey: 'systemmodel',
            eventPrefix: 'system',
            autoPersist: true
        });
        
        // ✅ FIX: Assign immediately
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // ✅ FIX: Initialize data directly
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
        
        this.logger.info('SystemModel', '✓ Model initialized (minimal version)');
        
        // Démarrer monitoring simple
        this.startMonitoring();
    }
    
    // ========================================================================
    // MONITORING SIMPLE
    // ========================================================================
    
    startMonitoring() {
        // Rafraîchir toutes les 5 secondes
        this.monitoringTimer = setInterval(() => {
            this.refreshSystemInfo();
        }, 5000);
        
        // Premier refresh immédiat
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
        try {
            const response = await this.backend.sendCommand('system.get-info', {});
            
            if (response.success && response.data) {
                const info = response.data;
                
                // Mettre à jour silencieusement
                this.update({
                    status: info.status || 'ready',
                    cpu: info.cpu || 0,
                    memory: info.memory || 0,
                    temperature: info.temperature || 0,
                    uptime: info.uptime || 0
                }, { silent: true });
                
                // Émettre événement
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
     * Met à jour la configuration audio
     */
    async updateAudioConfig(config) {
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
     * Met à jour la configuration MIDI
     */
    async updateMidiConfig(config) {
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
    // ACTIONS SYSTÈME
    // ========================================================================
    
    /**
     * Redémarre le système
     */
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
     * Arrête le système
     */
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
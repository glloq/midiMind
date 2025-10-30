// ============================================================================
// Fichier: frontend/js/models/SystemModel.js
// Version: v3.0.8 - FIXED LOGGER PROTECTION
// Date: 2025-10-30
// ============================================================================
// CORRECTIONS v3.0.8:
// ✅ CRITIQUE: Protection contre logger undefined
// ✅ Utilise logger || window.logger || console comme fallback
// ✅ Vérification avant chaque appel logger.info/warn/debug/error
// ============================================================================

class SystemModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super({}, {
            persistKey: 'systemmodel',
            eventPrefix: 'system',
            autoPersist: true
        });
        
        // ✅ PROTECTION: Fallback sur window.logger ou console
        this.eventBus = eventBus || window.EventBus || window.eventBus;
        this.backend = backend || window.backendService || window.app?.services?.backend;
        this.logger = logger || window.logger || console;
        
        // Validation des dépendances
        if (!this.eventBus) {
            console.error('[SystemModel] EventBus not available!');
        }
        if (!this.backend) {
            console.warn('[SystemModel] BackendService not available');
        }
        
        // Initialisation des données
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
        
        // ✅ Vérification avant utilisation
        if (this.logger && typeof this.logger.info === 'function') {
            this.logger.info('SystemModel', '✓ Model initialized v3.0.8');
        }
        
        // Démarrer monitoring simple
        this.startMonitoring();
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    _isBackendAvailable() {
        return this.backend && typeof this.backend.sendCommand === 'function';
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
        
        if (this.logger && typeof this.logger.debug === 'function') {
            this.logger.debug('SystemModel', 'Monitoring started');
        }
    }
    
    stopMonitoring() {
        if (this.monitoringTimer) {
            clearInterval(this.monitoringTimer);
            this.monitoringTimer = null;
        }
        
        if (this.logger && typeof this.logger.debug === 'function') {
            this.logger.debug('SystemModel', 'Monitoring stopped');
        }
    }
    
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
                
                // Mettre à jour silencieusement
                this.update({
                    status: info.status || 'ready',
                    cpu: info.cpu || 0,
                    memory: info.memory || 0,
                    temperature: info.temperature || 0,
                    uptime: info.uptime || 0
                }, { silent: true });
                
                // Émettre événement
                if (this.eventBus) {
                    this.eventBus.emit('system:info-updated', { info });
                }
                
                return info;
            }
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('SystemModel', `Refresh failed: ${error.message}`);
            }
        }
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    async updateAudioConfig(config) {
        if (!this._isBackendAvailable()) {
            if (this.logger && typeof this.logger.warn === 'function') {
                this.logger.warn('SystemModel', 'Cannot update audio config: backend not available');
            }
            return false;
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('SystemModel', 'Updating audio config');
            }
            
            const response = await this.backend.sendCommand('system.set-audio-config', {
                config: config
            });
            
            if (response.success) {
                this.update({
                    audioLatency: config.latency || this.get('audioLatency'),
                    bufferSize: config.bufferSize || this.get('bufferSize'),
                    sampleRate: config.sampleRate || this.get('sampleRate')
                });
                
                if (this.logger && typeof this.logger.info === 'function') {
                    this.logger.info('SystemModel', 'Audio config updated');
                }
                return true;
            }
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('SystemModel', `Audio config update failed: ${error.message}`);
            }
        }
        
        return false;
    }
    
    async updateMidiConfig(config) {
        if (!this._isBackendAvailable()) {
            if (this.logger && typeof this.logger.warn === 'function') {
                this.logger.warn('SystemModel', 'Cannot update MIDI config: backend not available');
            }
            return false;
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('SystemModel', 'Updating MIDI config');
            }
            
            const response = await this.backend.sendCommand('system.set-midi-config', {
                config: config
            });
            
            if (response.success) {
                this.update({
                    midiLatency: config.latency || this.get('midiLatency')
                });
                
                if (this.logger && typeof this.logger.info === 'function') {
                    this.logger.info('SystemModel', 'MIDI config updated');
                }
                return true;
            }
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('SystemModel', `MIDI config update failed: ${error.message}`);
            }
        }
        
        return false;
    }
    
    // ========================================================================
    // ACTIONS SYSTÈME
    // ========================================================================
    
    async restart() {
        if (!this._isBackendAvailable()) {
            if (this.logger && typeof this.logger.warn === 'function') {
                this.logger.warn('SystemModel', 'Cannot restart: backend not available');
            }
            return false;
        }
        
        try {
            if (this.logger && typeof this.logger.warn === 'function') {
                this.logger.warn('SystemModel', 'System restart requested');
            }
            
            const response = await this.backend.sendCommand('system.restart', {});
            
            if (response.success) {
                if (this.logger && typeof this.logger.info === 'function') {
                    this.logger.info('SystemModel', 'System restarting...');
                }
                return true;
            }
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('SystemModel', `Restart failed: ${error.message}`);
            }
        }
        
        return false;
    }
    
    async shutdown() {
        if (!this._isBackendAvailable()) {
            if (this.logger && typeof this.logger.warn === 'function') {
                this.logger.warn('SystemModel', 'Cannot shutdown: backend not available');
            }
            return false;
        }
        
        try {
            if (this.logger && typeof this.logger.warn === 'function') {
                this.logger.warn('SystemModel', 'System shutdown requested');
            }
            
            const response = await this.backend.sendCommand('system.shutdown', {});
            
            if (response.success) {
                if (this.logger && typeof this.logger.info === 'function') {
                    this.logger.info('SystemModel', 'System shutting down...');
                }
                return true;
            }
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('SystemModel', `Shutdown failed: ${error.message}`);
            }
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
// ============================================================================
// Fichier: frontend/js/models/InstrumentModel.js
// Version: v3.0.6 - MINIMAL (Constructor fixed + basic functions only)
// Date: 2025-10-19
// ============================================================================
// SIMPLIFICATION: Seulement les fonctions de base
// - Scanner les instruments
// - Lister les instruments
// - Connecter/DÃ©connecter
// - Pas de SysEx complexe
// - Pas de capabilities avancÃ©es
// ============================================================================


class InstrumentModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // âœ… FIX: Correct super() call
        super({}, {
            persistKey: 'instrumentmodel',
            eventPrefix: 'instrument',
            autoPersist: false
        });
        
        // âœ… FIX: Assign immediately
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // Cache des instruments
        this.instruments = new Map();
        
        // Ã‰tat
        this.state = {
            scanning: false,
            lastScan: null,
            totalInstruments: 0,
            connectedCount: 0
        };
        
        this.logger.info('InstrumentModel', 'âœ“ Model initialized (minimal version)');
    }
    
    // ========================================================================
    // SCAN ET CHARGEMENT
    // ========================================================================
    
    /**
     * Scanne les instruments disponibles
     */
    async scan() {
        if (this.state.scanning) {
            this.logger.warn('InstrumentModel', 'Scan already in progress');
            return Array.from(this.instruments.values());
        }
        
        this.state.scanning = true;
        this.eventBus.emit('instruments:scan:started');
        
        try {
            this.logger.info('InstrumentModel', 'Scanning for instruments...');
            
            const response = await this.backend.sendCommand('instruments.scan', {});
            
            if (response.success) {
                const instruments = response.data.instruments || [];
                
                // Mettre Ã  jour le cache
                this.instruments.clear();
                instruments.forEach(inst => {
                    this.instruments.set(inst.id, inst);
                });
                
                this.state.totalInstruments = instruments.length;
                this.state.connectedCount = instruments.filter(i => i.connected).length;
                this.state.lastScan = Date.now();
                
                this.logger.info('InstrumentModel', 
                    `Found ${instruments.length} instruments (${this.state.connectedCount} connected)`
                );
                
                this.eventBus.emit('instruments:scan:complete', {
                    instruments,
                    total: instruments.length,
                    connected: this.state.connectedCount
                });
                
                return instruments;
            }
            
            throw new Error(response.error || 'Scan failed');
            
        } catch (error) {
            this.logger.error('InstrumentModel', `Scan error: ${error.message}`);
            this.eventBus.emit('instruments:scan:error', { error: error.message });
            throw error;
            
        } finally {
            this.state.scanning = false;
        }
    }
    
    /**
     * Charge les dÃ©tails d'un instrument
     */
    async loadInstrument(instrumentId) {
        try {
            this.logger.info('InstrumentModel', `Loading instrument: ${instrumentId}`);
            
            const response = await this.backend.sendCommand('instruments.get', {
                instrument_id: instrumentId
            });
            
            if (response.success) {
                const instrument = response.data;
                
                // Mettre Ã  jour le cache
                this.instruments.set(instrumentId, instrument);
                
                this.eventBus.emit('instrument:loaded', { instrument });
                
                return instrument;
            }
            
            throw new Error(response.error || 'Failed to load instrument');
            
        } catch (error) {
            this.logger.error('InstrumentModel', `Load failed: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // CONNEXION/DÃ‰CONNEXION
    // ========================================================================
    
    /**
     * Connecte un instrument
     */
    async connect(instrumentId) {
        try {
            this.logger.info('InstrumentModel', `Connecting: ${instrumentId}`);
            
            const response = await this.backend.sendCommand('instruments.connect', {
                instrument_id: instrumentId
            });
            
            if (response.success) {
                const instrument = this.instruments.get(instrumentId);
                if (instrument) {
                    instrument.connected = true;
                    this.instruments.set(instrumentId, instrument);
                    this.state.connectedCount++;
                }
                
                this.eventBus.emit('instrument:connected', { instrumentId });
                
                return true;
            }
            
            throw new Error(response.error || 'Connection failed');
            
        } catch (error) {
            this.logger.error('InstrumentModel', `Connect failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * DÃ©connecte un instrument
     */
    async disconnect(instrumentId) {
        try {
            this.logger.info('InstrumentModel', `Disconnecting: ${instrumentId}`);
            
            const response = await this.backend.sendCommand('instruments.disconnect', {
                instrument_id: instrumentId
            });
            
            if (response.success) {
                const instrument = this.instruments.get(instrumentId);
                if (instrument) {
                    instrument.connected = false;
                    this.instruments.set(instrumentId, instrument);
                    this.state.connectedCount--;
                }
                
                this.eventBus.emit('instrument:disconnected', { instrumentId });
                
                return true;
            }
            
            throw new Error(response.error || 'Disconnection failed');
            
        } catch (error) {
            this.logger.error('InstrumentModel', `Disconnect failed: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // GESTION INSTRUMENTS
    // ========================================================================
    
    /**
     * RÃ©cupÃ¨re un instrument par ID
     */
    getInstrument(instrumentId) {
        return this.instruments.get(instrumentId) || null;
    }
    
    /**
     * RÃ©cupÃ¨re tous les instruments
     */
    getAllInstruments() {
        return Array.from(this.instruments.values());
    }
    
    /**
     * RÃ©cupÃ¨re les instruments connectÃ©s
     */
    getConnectedInstruments() {
        return this.getAllInstruments().filter(inst => inst.connected);
    }
    
    /**
     * RÃ©cupÃ¨re les instruments par type
     */
    getInstrumentsByType(type) {
        return this.getAllInstruments().filter(inst => inst.type === type);
    }
    
    /**
     * VÃ©rifie si un instrument est connectÃ©
     */
    isConnected(instrumentId) {
        const instrument = this.instruments.get(instrumentId);
        return instrument ? instrument.connected : false;
    }
    
    /**
     * RÃ©cupÃ¨re le nombre d'instruments
     */
    getInstrumentCount() {
        return this.instruments.size;
    }
    
    /**
     * RÃ©cupÃ¨re le nombre d'instruments connectÃ©s
     */
    getConnectedCount() {
        return this.state.connectedCount;
    }
    
    // ========================================================================
    // CACHE
    // ========================================================================
    
    /**
     * Vide le cache
     */
    clearCache() {
        this.instruments.clear();
        this.state.totalInstruments = 0;
        this.state.connectedCount = 0;
        this.state.lastScan = null;
        
        this.logger.info('InstrumentModel', 'Cache cleared');
        
        this.eventBus.emit('instruments:cache-cleared');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = InstrumentModel;
}

if (typeof window !== 'undefined') {
    window.InstrumentModel = InstrumentModel;
}

// Export par défaut
window.InstrumentModel = InstrumentModel;

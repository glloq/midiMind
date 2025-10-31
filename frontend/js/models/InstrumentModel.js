// ============================================================================
// Fichier: frontend/js/models/InstrumentModel.js
// Version: v3.0.8 - CORRECTION INTERFACE
// Date: 2025-10-31
// ============================================================================
// CORRECTIONS v3.0.8:
// ✅ Ajout méthode getConnectedInstruments() (alias pour getConnected)
// ✅ Cohérence avec appels NavigationController
// ============================================================================

class InstrumentModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // ✅ Appel super() CORRECT avec BaseModel(initialData, options)
        super({}, {
            persistKey: 'instrumentmodel',
            eventPrefix: 'instrument',
            autoPersist: false
        });
        
        // ✅ CRITIQUE: Assigner IMMÉDIATEMENT après super()
        // Accepter paramètres OU utiliser globaux
        this.eventBus = eventBus || window.EventBus || window.eventBus;
        this.backend = backend || window.backendService || window.app?.services?.backend;
        this.logger = logger || window.logger || console;
        
        // ✅ Validation des dépendances critiques
        if (!this.eventBus) {
            console.error('[InstrumentModel] EventBus not available!');
        }
        if (!this.backend) {
            console.warn('[InstrumentModel] BackendService not available - instrument operations will fail');
        }
        
        // Cache des instruments
        this.instruments = new Map();
        
        // État
        this.state = {
            scanning: false,
            lastScan: null,
            totalInstruments: 0,
            connectedCount: 0
        };
        
        if (this.logger && typeof this.logger.info === 'function') {
            this.logger.info('InstrumentModel', '✓ InstrumentModel v3.0.8 initialized');
        }
    }
    
    // ========================================================================
    // SCAN ET CHARGEMENT
    // ========================================================================
    
    /**
     * Scanne les instruments disponibles
     */
    async scan() {
        if (this.state.scanning) {
            if (this.logger && typeof this.logger.warn === 'function') {
                this.logger.warn('InstrumentModel', 'Scan already in progress');
            }
            return Array.from(this.instruments.values());
        }
        
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        this.state.scanning = true;
        if (this.eventBus) {
            this.eventBus.emit('instruments:scan:started');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('InstrumentModel', 'Scanning for instruments...');
            }
            
            const response = await this.backend.sendCommand('instruments.scan', {});
            
            if (response.success) {
                const instruments = response.data.instruments || [];
                
                // Mettre à jour le cache
                this.instruments.clear();
                instruments.forEach(inst => {
                    this.instruments.set(inst.id, inst);
                });
                
                this.state.totalInstruments = instruments.length;
                this.state.connectedCount = instruments.filter(i => i.connected).length;
                this.state.lastScan = Date.now();
                
                if (this.logger && typeof this.logger.info === 'function') {
                    this.logger.info('InstrumentModel', 
                        `Found ${instruments.length} instruments (${this.state.connectedCount} connected)`
                    );
                }
                
                if (this.eventBus) {
                    this.eventBus.emit('instruments:scan:complete', {
                        instruments,
                        total: instruments.length,
                        connected: this.state.connectedCount
                    });
                }
                
                return instruments;
            }
            
            throw new Error(response.error || 'Scan failed');
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('InstrumentModel', `Scan error: ${error.message}`);
            }
            if (this.eventBus) {
                this.eventBus.emit('instruments:scan:error', { error: error.message });
            }
            throw error;
            
        } finally {
            this.state.scanning = false;
        }
    }
    
    /**
     * Connecte un instrument
     */
    async connect(instrumentId) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('InstrumentModel', `Connecting instrument: ${instrumentId}`);
            }
            
            const response = await this.backend.sendCommand('instruments.connect', {
                instrument_id: instrumentId
            });
            
            if (response.success) {
                // Mettre à jour le cache
                const instrument = this.instruments.get(instrumentId);
                if (instrument) {
                    instrument.connected = true;
                    this.instruments.set(instrumentId, instrument);
                    this.state.connectedCount++;
                }
                
                if (this.eventBus) {
                    this.eventBus.emit('instruments:connected', {
                        instrumentId,
                        instrument
                    });
                }
                
                return true;
            }
            
            throw new Error(response.error || 'Connection failed');
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('InstrumentModel', `Connection error: ${error.message}`);
            }
            throw error;
        }
    }
    
    /**
     * Déconnecte un instrument
     */
    async disconnect(instrumentId) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('InstrumentModel', `Disconnecting instrument: ${instrumentId}`);
            }
            
            const response = await this.backend.sendCommand('instruments.disconnect', {
                instrument_id: instrumentId
            });
            
            if (response.success) {
                // Mettre à jour le cache
                const instrument = this.instruments.get(instrumentId);
                if (instrument) {
                    instrument.connected = false;
                    this.instruments.set(instrumentId, instrument);
                    this.state.connectedCount = Math.max(0, this.state.connectedCount - 1);
                }
                
                if (this.eventBus) {
                    this.eventBus.emit('instruments:disconnected', {
                        instrumentId,
                        instrument
                    });
                }
                
                return true;
            }
            
            throw new Error(response.error || 'Disconnection failed');
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('InstrumentModel', `Disconnection error: ${error.message}`);
            }
            throw error;
        }
    }
    
    // ========================================================================
    // GETTERS / HELPERS
    // ========================================================================
    
    /**
     * Obtient tous les instruments
     */
    getAll() {
        return Array.from(this.instruments.values());
    }
    
    /**
     * Obtient un instrument par ID
     */
    getById(instrumentId) {
        return this.instruments.get(instrumentId);
    }
    
    /**
     * Obtient les instruments connectés
     */
    getConnected() {
        return this.getAll().filter(i => i.connected);
    }
    
    /**
     * Alias pour getConnected() - compatibilité NavigationController
     */
    getConnectedInstruments() {
        return this.getConnected();
    }
    
    /**
     * Obtient les instruments déconnectés
     */
    getDisconnected() {
        return this.getAll().filter(i => !i.connected);
    }
    
    /**
     * Vérifie si un instrument est connecté
     */
    isConnected(instrumentId) {
        const instrument = this.instruments.get(instrumentId);
        return instrument ? instrument.connected : false;
    }
    
    /**
     * Obtient le nombre total d'instruments
     */
    getCount() {
        return this.state.totalInstruments;
    }
    
    /**
     * Obtient le nombre d'instruments connectés
     */
    getConnectedCount() {
        return this.state.connectedCount;
    }
    
    /**
     * Obtient les statistiques
     */
    getStats() {
        return {
            total: this.state.totalInstruments,
            connected: this.state.connectedCount,
            disconnected: this.state.totalInstruments - this.state.connectedCount,
            scanning: this.state.scanning,
            lastScan: this.state.lastScan
        };
    }
    
    /**
     * Filtre les instruments par nom
     */
    filterByName(query) {
        const lowerQuery = query.toLowerCase();
        return this.getAll().filter(i => 
            i.name.toLowerCase().includes(lowerQuery) ||
            (i.model && i.model.toLowerCase().includes(lowerQuery))
        );
    }
    
    /**
     * Filtre les instruments par type
     */
    filterByType(type) {
        return this.getAll().filter(i => i.type === type);
    }
}

// ============================================================================
// EXPORT GLOBAL
// ============================================================================
if (typeof window !== 'undefined') {
    window.InstrumentModel = InstrumentModel;
}
// ============================================================================
// Fichier: frontend/js/models/InstrumentModel.js
// Version: v3.2.0 - SIGNATURE COHÃ‰RENTE
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.2.0:
// âœ… CRITIQUE: Signature cohÃ©rente avec BaseModel
// âœ… Appel super(eventBus, backend, logger, initialData, options)
// âœ… Ajout mÃ©thode getConnectedInstruments() (alias pour getConnected)
// âœ… CohÃ©rence avec appels NavigationController
// ============================================================================

class InstrumentModel extends BaseModel {
    constructor(eventBus, backend, logger, initialData = {}, options = {}) {
        // âœ… NOUVEAU: Appel super() avec signature cohÃ©rente
        super(eventBus, backend, logger, {
            ...initialData
        }, {
            persistKey: 'instrumentmodel',
            eventPrefix: 'instrument',
            autoPersist: false,
            ...options
        });
        
        // Cache des instruments
        this.instruments = new Map();
        
        // Ã‰tat
        this.state = {
            scanning: false,
            lastScan: null,
            totalInstruments: 0,
            connectedCount: 0
        };
        
        this.log('info', 'InstrumentModel', 'âœ… InstrumentModel v3.2.0 initialized');
    }
    
    // ========================================================================
    // SCAN ET CHARGEMENT
    // ========================================================================
    
    /**
     * Scanne les instruments disponibles
     */
    async scan() {
        if (this.state.scanning) {
            this.log('warn', 'InstrumentModel', 'Scan already in progress');
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
            this.log('info', 'InstrumentModel', 'Scanning for instruments...');
            
            const response = await this.backend.sendCommand('list_devices', {});
            
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
                
                this.log('info', 'InstrumentModel', 
                    \`Found \${instruments.length} instruments (\${this.state.connectedCount} connected)\`
                );
                
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
            this.log('error', 'InstrumentModel', \`Scan error: \${error.message}\`);
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
            this.log('info', 'InstrumentModel', \`Connecting instrument: \${instrumentId}\`);
            
            const response = await this.backend.sendCommand('connect_device', {
                device_id: instrumentId
            });
            
            if (response.success) {
                // Mettre Ã  jour le cache
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
            this.log('error', 'InstrumentModel', \`Connection error: \${error.message}\`);
            throw error;
        }
    }
    
    /**
     * DÃ©connecte un instrument
     */
    async disconnect(instrumentId) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'InstrumentModel', \`Disconnecting instrument: \${instrumentId}\`);
            
            const response = await this.backend.sendCommand('disconnect_device', {
                device_id: instrumentId
            });
            
            if (response.success) {
                // Mettre Ã  jour le cache
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
            this.log('error', 'InstrumentModel', \`Disconnection error: \${error.message}\`);
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
     * Obtient les instruments connectÃ©s
     */
    getConnected() {
        return this.getAll().filter(i => i.connected);
    }
    
    /**
     * Alias pour getConnected() - compatibilitÃ© NavigationController
     */
    getConnectedInstruments() {
        return this.getConnected();
    }
    
    /**
     * Obtient les instruments dÃ©connectÃ©s
     */
    getDisconnected() {
        return this.getAll().filter(i => !i.connected);
    }
    
    /**
     * VÃ©rifie si un instrument est connectÃ©
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
     * Obtient le nombre d'instruments connectÃ©s
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
// ============================================================================
// Fichier: frontend/js/models/InstrumentModel.js
// Version: v3.0.1 - CORRIGÉ (Backend Integration)
// Date: 2025-10-08
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Modèle de gestion des instruments MIDI connectés.
//   Cache local, état de connexion, capacités SysEx.
//   PHASE 1.3 - Support complet SysEx DIY (Layers 01 & 02)
//
// CORRECTIONS v3.0.1:
//   ✅ Remplacé fetch() par backend.sendCommand()
//   ✅ Ajout dépendance BackendService dans constructeur
//   ✅ Gestion erreurs améliorée
//   ✅ Utilisation correcte de EventBus via BaseModel
//
// Auteur: midiMind Team
// ============================================================================

class InstrumentModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super({}, {
            persistKey: 'instrumentmodel',
            eventPrefix: 'instrument',
            autoPersist: false
        });
        
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // Cache des instruments
        this.instruments = new Map();
        
        // État
        this.state = {
            scanning: false,
            lastScan: null,
            totalInstruments: 0,
            connectedCount: 0
        };
        
        // Configuration
        this.config = {
            autoQueryCapabilities: true,  // Auto-interroger identité
            cacheTimeout: 300000,          // 5 minutes
            maxInstruments: 50
        };
        
        this.logger.info('InstrumentModel', '✓ Model initialized');
    }
    
    // ========================================================================
    // SCAN ET CHARGEMENT - ✅ CORRIGÉ (utilise backend)
    // ========================================================================
    
    /**
     * Scanne les instruments disponibles via backend
     * @returns {Promise<Array>}
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
            
            // ✅ CORRIGÉ: Utilise backend.sendCommand au lieu de fetch
            const response = await this.backend.sendCommand('instruments.scan');
            
            if (!response.success) {
                throw new Error(response.error || 'Scan failed');
            }
            
            const instruments = response.data?.instruments || response.instruments || [];
            
            // Mettre à jour le cache
            this.instruments.clear();
            instruments.forEach(inst => {
                this.instruments.set(inst.id, this._initializeInstrument(inst));
            });
            
            this.state.lastScan = Date.now();
            this._updateStats();
            
            this.logger.info('InstrumentModel', `✓ Scan completed: ${instruments.length} instruments found`);
            
            this.eventBus.emit('instruments:scan:completed', {
                instruments: instruments,
                count: instruments.length
            });
            
            return instruments;
            
        } catch (error) {
            this.logger.error('InstrumentModel', 'Scan error:', error);
            
            this.eventBus.emit('instruments:scan:error', {
                error: error.message
            });
            
            throw error;
            
        } finally {
            this.state.scanning = false;
        }
    }
    
    /**
     * Charge tous les instruments depuis le backend
     * @returns {Promise<Array>}
     */
    async loadAll() {
        try {
            this.logger.info('InstrumentModel', 'Loading all instruments...');
            
            // ✅ CORRIGÉ: Utilise backend.sendCommand au lieu de fetch
            const response = await this.backend.sendCommand('instruments.list');
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to load instruments');
            }
            
            const instruments = response.data?.instruments || response.instruments || [];
            
            this.instruments.clear();
            instruments.forEach(inst => {
                this.instruments.set(inst.id, this._initializeInstrument(inst));
            });
            
            this._updateStats();
            
            this.logger.info('InstrumentModel', `✓ Loaded ${instruments.length} instruments`);
            
            this.eventBus.emit('instruments:loaded', {
                instruments: instruments,
                count: instruments.length
            });
            
            return instruments;
            
        } catch (error) {
            this.logger.error('InstrumentModel', 'Load error:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // GESTION DES INSTRUMENTS
    // ========================================================================
    
    /**
     * Obtient un instrument par ID
     * @param {string} instrumentId
     * @returns {Object|undefined}
     */
    get(instrumentId) {
        return this.instruments.get(instrumentId);
    }
    
    /**
     * Obtient tous les instruments
     * @returns {Array}
     */
    getAll() {
        return Array.from(this.instruments.values());
    }
    
    /**
     * Ajoute un instrument
     * @param {Object} instrumentData
     * @returns {Object}
     */
    add(instrumentData) {
        const instrument = this._initializeInstrument(instrumentData);
        this.instruments.set(instrument.id, instrument);
        
        this._updateStats();
        
        this.eventBus.emit('instrument:added', { instrument });
        
        return instrument;
    }
    
    /**
     * Met à jour un instrument
     * @param {string} instrumentId
     * @param {Object} updates
     * @returns {Object}
     */
    update(instrumentId, updates) {
        const instrument = this.instruments.get(instrumentId);
        
        if (!instrument) {
            throw new Error(`Instrument not found: ${instrumentId}`);
        }
        
        // Fusionner les mises à jour
        Object.assign(instrument, updates);
        instrument.lastUpdated = Date.now();
        
        this.instruments.set(instrumentId, instrument);
        
        this._updateStats();
        
        this.eventBus.emit('instrument:updated', {
            instrumentId,
            instrument,
            updates
        });
        
        return instrument;
    }
    
    /**
     * Supprime un instrument
     * @param {string} instrumentId
     * @returns {boolean}
     */
    remove(instrumentId) {
        const existed = this.instruments.delete(instrumentId);
        
        if (existed) {
            this._updateStats();
            
            this.eventBus.emit('instrument:removed', { instrumentId });
        }
        
        return existed;
    }
    
    // ========================================================================
    // REQUÊTES SYSEX (via Backend)
    // ========================================================================
    
    /**
     * Demande l'identité d'un instrument (Bloc 1)
     * @param {string} instrumentId
     * @returns {Promise}
     */
    async requestIdentity(instrumentId) {
        try {
            const response = await this.backend.requestIdentity(instrumentId);
            
            if (response.success && response.data) {
                this.update(instrumentId, {
                    identity: response.data,
                    identityReceived: true,
                    lastSysexUpdate: Date.now()
                });
            }
            
            return response;
            
        } catch (error) {
            this.logger.error('InstrumentModel', `Failed to request identity for ${instrumentId}:`, error);
            throw error;
        }
    }
    
    /**
     * Demande le mapping de notes (Bloc 2)
     * @param {string} instrumentId
     * @returns {Promise}
     */
    async requestNoteMapping(instrumentId) {
        try {
            const response = await this.backend.requestNoteMapping(instrumentId);
            
            if (response.success && response.data) {
                this.update(instrumentId, {
                    noteMapping: response.data,
                    mappingReceived: true,
                    lastSysexUpdate: Date.now()
                });
            }
            
            return response;
            
        } catch (error) {
            this.logger.error('InstrumentModel', `Failed to request note mapping for ${instrumentId}:`, error);
            throw error;
        }
    }
    
    /**
     * Demande le profil complet (tous les blocs SysEx)
     * @param {string} instrumentId
     * @returns {Promise}
     */
    async requestCompleteProfile(instrumentId) {
        try {
            const response = await this.backend.requestCompleteProfile(instrumentId);
            
            if (response.success && response.data) {
                this.update(instrumentId, {
                    profile: response.data,
                    lastSysexUpdate: Date.now()
                });
            }
            
            return response;
            
        } catch (error) {
            this.logger.error('InstrumentModel', `Failed to request complete profile for ${instrumentId}:`, error);
            throw error;
        }
    }
    
    // ========================================================================
    // UTILITAIRES ET STATISTIQUES
    // ========================================================================
    
    /**
     * Obtient les statistiques du modèle
     * @returns {Object}
     */
    getStats() {
        const instruments = this.getAll();
        
        return {
            total: this.state.totalInstruments,
            connected: this.state.connectedCount,
            disconnected: this.state.totalInstruments - this.state.connectedCount,
            byType: this._countByProperty(instruments, 'type'),
            byManufacturer: this._countByProperty(instruments, 'manufacturer'),
            sysexCapable: instruments.filter(i => i.sysexCapable).length,
            lastScan: this.state.lastScan ? new Date(this.state.lastScan).toLocaleString() : 'Never'
        };
    }
    
    /**
     * Obtient le statut d'un instrument
     * @param {string} instrumentId
     * @returns {Object}
     */
    getInstrumentStatus(instrumentId) {
        const instrument = this.get(instrumentId);
        
        if (!instrument) {
            return null;
        }
        
        const now = Date.now();
        
        return {
            id: instrument.id,
            name: instrument.name,
            connected: instrument.connected,
            uptime: instrument.connectionTime ? now - instrument.connectionTime : 0,
            lastSeen: instrument.lastSeen,
            lastUpdate: instrument.lastSysexUpdate,
            timeSinceUpdate: instrument.lastSysexUpdate ? 
                             (now - instrument.lastSysexUpdate) : null,
            isStale: instrument.lastSysexUpdate ? 
                     (now - instrument.lastSysexUpdate) > this.config.cacheTimeout : true
        };
    }
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * Initialise un instrument avec les valeurs par défaut
     * @private
     */
    _initializeInstrument(data) {
        return {
            // Identité de base
            id: data.id,
            name: data.name || 'Unknown',
            type: data.type || 'unknown',
            manufacturer: data.manufacturer || 'Unknown',
            model: data.model || 'Unknown',
            
            // État de connexion
            connected: data.connected || false,
            discovered: data.discovered || false,
            
            // Capacités
            sysexCapable: data.sysexCapable || false,
            noteRange: data.noteRange || { min: 0, max: 127 },
            polyphony: data.polyphony || 1,
            
            // Capacités techniques
            hasAir: data.hasAir || false,
            hasLights: data.hasLights || false,
            hasSensors: data.hasSensors || false,
            hasCC: data.hasCC || false,
            
            // Mapping
            noteMapping: data.noteMapping || null,
            mappingReceived: false,
            identityReceived: false,
            
            // Timestamps
            createdAt: data.createdAt || Date.now(),
            lastUpdated: Date.now(),
            lastSeen: data.lastSeen || null,
            lastSysexUpdate: data.lastSysexUpdate || null,
            connectionTime: data.connectionTime || null,
            discoveredAt: data.discoveredAt || null,
            
            // Métadonnées
            version: data.version || '1.0.0',
            serialNumber: data.serialNumber || null,
            
            // Conserver autres données
            ...data
        };
    }
    
    /**
     * Met à jour les statistiques globales
     * @private
     */
    _updateStats() {
        const instruments = this.getAll();
        
        this.state.totalInstruments = instruments.length;
        this.state.connectedCount = instruments.filter(i => i.connected).length;
        
        this.eventBus.emit('instruments:stats:updated', this.state);
    }
    
    /**
     * Compte les instruments par propriété
     * @private
     */
    _countByProperty(instruments, property) {
        const counts = {};
        
        instruments.forEach(inst => {
            const value = inst[property] || 'unknown';
            counts[value] = (counts[value] || 0) + 1;
        });
        
        return counts;
    }
    
    // ========================================================================
    // NETTOYAGE
    // ========================================================================
    
    /**
     * Nettoie les instruments déconnectés depuis longtemps
     * @param {number} threshold - Seuil en ms (défaut: 24h)
     */
    cleanup(threshold = 86400000) {
        const now = Date.now();
        let cleaned = 0;
        
        this.instruments.forEach((instrument, id) => {
            if (!instrument.connected && 
                instrument.lastSeen && 
                (now - instrument.lastSeen) > threshold) {
                
                this.instruments.delete(id);
                cleaned++;
            }
        });
        
        if (cleaned > 0) {
            this._updateStats();
            this.eventBus.emit('instruments:cleanup:completed', { cleaned });
            this.logger.info('InstrumentModel', `Cleaned ${cleaned} stale instruments`);
        }
        
        return cleaned;
    }
    
    /**
     * Efface tous les instruments
     */
    clear() {
        this.instruments.clear();
        this._updateStats();
        this.eventBus.emit('instruments:cleared');
        this.logger.info('InstrumentModel', 'All instruments cleared');
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
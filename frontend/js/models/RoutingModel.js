// ============================================================================
// Fichier: frontend/js/models/RoutingModel.js
// Version: v3.0.2 - COMPLET (Transformations + Validation)
// Date: 2025-10-08
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Modèle gérant la configuration de routage MIDI (canaux vers devices).
//   Gère l'assignation des canaux, mute/solo, volume et transformations.
//
// CORRECTIONS v3.0.2:
//   ✅ Velocity mapping avec courbes
//   ✅ Note filtering avancé
//   ✅ CC transformations
//   ✅ Validation complète des configurations
//   ✅ Presets de transformation
//
// Auteur: midiMind Team
// ============================================================================

class RoutingModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super({}, {
            persistKey: 'routingmodel',
            eventPrefix: 'routing',
            autoPersist: true
        });
        
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // Initialiser avec 16 canaux MIDI
        this.initialize({
            // Configuration des 16 canaux MIDI
            channels: this.createDefaultChannels(),
            
            // Liste des devices disponibles
            devices: [],
            
            // Map de routage actuel (channelNumber -> deviceId)
            routingMap: new Map(),
            
            // Presets de routage sauvegardés
            presets: [],
            activePreset: null,
            
            // État global
            globalMute: false,
            globalSolo: false,
            masterVolume: 100,
            
            // Statistiques
            stats: {
                messagesRouted: 0,
                activeChannels: 0,
                lastActivity: {}
            }
        });
        
        // Configuration par défaut
        this.config = {
            maxPresets: 10,
            defaultVolume: 100,
            volumeRange: { min: 0, max: 127 },
            transposeRange: { min: -24, max: 24 },
            velocityRange: { min: 1, max: 127 },
            noteRange: { min: 0, max: 127 }
        };
        
        // Couleurs par défaut pour les canaux
        this.channelColors = [
            '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A',
            '#98D8C8', '#F7DC6F', '#BB8FCE', '#85C1E2',
            '#F8B739', '#52C7B8', '#FF8C94', '#A8E6CF',
            '#FFD93D', '#BCB3E5', '#FAB1A0', '#81C784'
        ];
        
        // Courbes de vélocité prédéfinies
        this.velocityCurves = {
            linear: (v) => v,
            compress: (v) => Math.round(64 + (v - 64) * 0.5),
            expand: (v) => Math.round(64 + (v - 64) * 1.5),
            soft: (v) => Math.round(Math.pow(v / 127, 1.5) * 127),
            hard: (v) => Math.round(Math.pow(v / 127, 0.7) * 127),
            fixed: (v, value) => value || 64
        };
        
        this.logger.info('RoutingModel', '✓ Model initialized with transformations');
    }
    
    // ========================================================================
    // CRÉATION CANAUX PAR DÉFAUT
    // ========================================================================
    
    createDefaultChannels() {
        const channels = [];
        
        for (let i = 0; i < 16; i++) {
            channels.push({
                number: i,
                name: `Channel ${i + 1}`,
                device: null,
                enabled: true,
                muted: false,
                solo: false,
                
                // Contrôles de base
                volume: 100,
                pan: 64,
                transpose: 0,
                
                // Transformations avancées - ✅ NOUVEAU
                transformations: {
                    // Velocity mapping
                    velocity: {
                        enabled: false,
                        curve: 'linear',      // linear, compress, expand, soft, hard, fixed
                        min: 1,
                        max: 127,
                        customCurve: null,    // Fonction personnalisée
                        fixedValue: 64        // Pour curve: 'fixed'
                    },
                    
                    // Note filtering
                    noteFilter: {
                        enabled: false,
                        mode: 'range',        // range, whitelist, blacklist
                        minNote: 0,
                        maxNote: 127,
                        allowedNotes: [],
                        blockedNotes: []
                    },
                    
                    // CC transformations
                    ccMapping: {
                        enabled: false,
                        mappings: []          // [{from: 1, to: 11}, ...]
                    },
                    
                    // Note remapping
                    noteRemap: {
                        enabled: false,
                        mappings: []          // [{from: 60, to: 72}, ...]
                    },
                    
                    // Humanize
                    humanize: {
                        enabled: false,
                        timing: 0,            // ms de variation
                        velocity: 0           // variation de vélocité
                    }
                },
                
                // Métadonnées
                color: this.channelColors[i],
                lastActivity: null,
                messagesCount: 0
            });
        }
        
        return channels;
    }
    
    // ========================================================================
    // GESTION CANAUX - BASE
    // ========================================================================
    
    getChannel(channelNumber) {
        const channels = this.get('channels');
        return channels[channelNumber] || null;
    }
    
    getAllChannels() {
        return this.get('channels');
    }
    
    getActiveChannels() {
        const channels = this.get('channels');
        return channels.filter(ch => ch.enabled && !ch.muted && ch.device);
    }
    
    // ========================================================================
    // ASSIGNATION DEVICES
    // ========================================================================
    
// Intégrer dans le flux playback
async assignChannel(channel, instrumentId, config = {}) {
    const routing = {
        channel: channel,
        instrument: instrumentId,
        transpose: config.transpose || 0,
        velocityCurve: config.velocityCurve || 'linear',
        noteMapping: config.noteMapping || null,
        ...config
    };
    
    this.routingModel.setChannelRouting(channel, routing);
    
    // ✅ NOUVEAU: Activer les transformations
    this.eventBus.emit('routing:assigned', {
        channel,
        routing,
        transforms: true  // Flag pour activer applyTransformations
    });
    
    await this.syncWithBackend();
}
  
    unassignChannel(channelNumber) {
        return this.assignChannel(channelNumber, null);
    }
    
    // ========================================================================
    // CONTRÔLES DE BASE
    // ========================================================================
    
    muteChannel(channelNumber, muted = null) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return;
        
        channel.muted = muted !== null ? muted : !channel.muted;
        
        this.set('channels', [...channels]);
        
        this.eventBus.emit('routing:channel-muted', {
            channel: channelNumber,
            muted: channel.muted
        });
    }
    
    soloChannel(channelNumber, solo = null) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return;
        
        channel.solo = solo !== null ? solo : !channel.solo;
        
        this.set('channels', [...channels]);
        
        this.eventBus.emit('routing:channel-solo', {
            channel: channelNumber,
            solo: channel.solo
        });
    }
    
    setChannelVolume(channelNumber, volume) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return;
        
        channel.volume = Math.max(0, Math.min(127, volume));
        
        this.set('channels', [...channels]);
        
        this.eventBus.emit('routing:channel-volume', {
            channel: channelNumber,
            volume: channel.volume
        });
    }
    
    setChannelTranspose(channelNumber, semitones) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return;
        
        channel.transpose = Math.max(
            this.config.transposeRange.min,
            Math.min(this.config.transposeRange.max, semitones)
        );
        
        this.set('channels', [...channels]);
        
        this.eventBus.emit('routing:channel-transpose', {
            channel: channelNumber,
            transpose: channel.transpose
        });
    }
    
    setChannelPan(channelNumber, pan) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return;
        
        channel.pan = Math.max(0, Math.min(127, pan));
        
        this.set('channels', [...channels]);
        
        this.eventBus.emit('routing:channel-pan', {
            channel: channelNumber,
            pan: channel.pan
        });
    }
    
    // ========================================================================
    // TRANSFORMATIONS AVANCÉES - ✅ NOUVEAU
    // ========================================================================
    
    /**
     * Configure le velocity mapping d'un canal
     */
    setVelocityMapping(channelNumber, config) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return false;
        
        // Validation
        if (!this.validateVelocityConfig(config)) {
            this.logger.warn('RoutingModel', 'Invalid velocity config');
            return false;
        }
        
        channel.transformations.velocity = {
            ...channel.transformations.velocity,
            ...config
        };
        
        this.set('channels', [...channels]);
        
        this.logger.info('RoutingModel', 
            `Velocity mapping updated for channel ${channelNumber}: ${config.curve}`);
        
        this.eventBus.emit('routing:velocity-mapping', {
            channel: channelNumber,
            config: channel.transformations.velocity
        });
        
        return true;
    }
    
    /**
     * Applique une transformation de vélocité
     */
    transformVelocity(channelNumber, velocity) {
        const channel = this.getChannel(channelNumber);
        
        if (!channel || !channel.transformations.velocity.enabled) {
            return velocity;
        }
        
        const config = channel.transformations.velocity;
        const curveFn = this.velocityCurves[config.curve];
        
        if (!curveFn) {
            this.logger.warn('RoutingModel', `Unknown velocity curve: ${config.curve}`);
            return velocity;
        }
        
        // Appliquer la courbe
        let transformed = curveFn(velocity, config.fixedValue);
        
        // Appliquer min/max
        transformed = Math.max(config.min, Math.min(config.max, transformed));
        
        return Math.round(transformed);
    }
    
    /**
     * Configure le filtre de notes
     */
    setNoteFilter(channelNumber, config) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return false;
        
        // Validation
        if (!this.validateNoteFilterConfig(config)) {
            this.logger.warn('RoutingModel', 'Invalid note filter config');
            return false;
        }
        
        channel.transformations.noteFilter = {
            ...channel.transformations.noteFilter,
            ...config
        };
        
        this.set('channels', [...channels]);
        
        this.logger.info('RoutingModel', 
            `Note filter updated for channel ${channelNumber}: ${config.mode}`);
        
        this.eventBus.emit('routing:note-filter', {
            channel: channelNumber,
            config: channel.transformations.noteFilter
        });
        
        return true;
    }
    
    /**
     * Vérifie si une note passe le filtre
     */
    isNoteAllowed(channelNumber, note) {
        const channel = this.getChannel(channelNumber);
        
        if (!channel || !channel.transformations.noteFilter.enabled) {
            return true;
        }
        
        const filter = channel.transformations.noteFilter;
        
        switch (filter.mode) {
            case 'range':
                return note >= filter.minNote && note <= filter.maxNote;
                
            case 'whitelist':
                return filter.allowedNotes.length === 0 || 
                       filter.allowedNotes.includes(note);
                
            case 'blacklist':
                return !filter.blockedNotes.includes(note);
                
            default:
                return true;
        }
    }
    
    /**
     * Configure le mapping de CC
     */
    setCCMapping(channelNumber, mappings) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return false;
        
        // Validation
        if (!this.validateCCMappings(mappings)) {
            this.logger.warn('RoutingModel', 'Invalid CC mappings');
            return false;
        }
        
        channel.transformations.ccMapping.mappings = mappings;
        channel.transformations.ccMapping.enabled = mappings.length > 0;
        
        this.set('channels', [...channels]);
        
        this.logger.info('RoutingModel', 
            `CC mapping updated for channel ${channelNumber}: ${mappings.length} mappings`);
        
        this.eventBus.emit('routing:cc-mapping', {
            channel: channelNumber,
            mappings: mappings
        });
        
        return true;
    }
    
    /**
     * Transforme un numéro de CC selon le mapping
     */
    transformCC(channelNumber, ccNumber) {
        const channel = this.getChannel(channelNumber);
        
        if (!channel || !channel.transformations.ccMapping.enabled) {
            return ccNumber;
        }
        
        const mapping = channel.transformations.ccMapping.mappings.find(
            m => m.from === ccNumber
        );
        
        return mapping ? mapping.to : ccNumber;
    }
    
    /**
     * Configure le remapping de notes
     */
    setNoteRemap(channelNumber, mappings) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return false;
        
        // Validation
        if (!this.validateNoteMappings(mappings)) {
            this.logger.warn('RoutingModel', 'Invalid note mappings');
            return false;
        }
        
        channel.transformations.noteRemap.mappings = mappings;
        channel.transformations.noteRemap.enabled = mappings.length > 0;
        
        this.set('channels', [...channels]);
        
        this.logger.info('RoutingModel', 
            `Note remap updated for channel ${channelNumber}: ${mappings.length} mappings`);
        
        this.eventBus.emit('routing:note-remap', {
            channel: channelNumber,
            mappings: mappings
        });
        
        return true;
    }
    
    /**
     * Remappe une note selon la configuration
     */
    remapNote(channelNumber, note) {
        const channel = this.getChannel(channelNumber);
        
        if (!channel || !channel.transformations.noteRemap.enabled) {
            return note;
        }
        
        const mapping = channel.transformations.noteRemap.mappings.find(
            m => m.from === note
        );
        
        return mapping ? mapping.to : note;
    }
    
    /**
     * Configure l'humanisation
     */
    setHumanize(channelNumber, config) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return false;
        
        channel.transformations.humanize = {
            ...channel.transformations.humanize,
            ...config
        };
        
        this.set('channels', [...channels]);
        
        this.eventBus.emit('routing:humanize', {
            channel: channelNumber,
            config: channel.transformations.humanize
        });
        
        return true;
    }
    
    // ========================================================================
    // VALIDATION - ✅ NOUVEAU
    // ========================================================================
    
    validateDevice(deviceId) {
        const devices = this.get('devices');
        return devices.some(d => d.id === deviceId);
    }
    
    validateVelocityConfig(config) {
        if (!config) return false;
        
        // Vérifier la courbe
        if (config.curve && !this.velocityCurves[config.curve]) {
            return false;
        }
        
        // Vérifier min/max
        if (config.min !== undefined && config.max !== undefined) {
            if (config.min < 1 || config.min > 127) return false;
            if (config.max < 1 || config.max > 127) return false;
            if (config.min > config.max) return false;
        }
        
        return true;
    }
    
    validateNoteFilterConfig(config) {
        if (!config) return false;
        
        // Vérifier mode
        const validModes = ['range', 'whitelist', 'blacklist'];
        if (config.mode && !validModes.includes(config.mode)) {
            return false;
        }
        
        // Vérifier range
        if (config.minNote !== undefined && config.maxNote !== undefined) {
            if (config.minNote < 0 || config.minNote > 127) return false;
            if (config.maxNote < 0 || config.maxNote > 127) return false;
            if (config.minNote > config.maxNote) return false;
        }
        
        // Vérifier listes
        if (config.allowedNotes && !Array.isArray(config.allowedNotes)) return false;
        if (config.blockedNotes && !Array.isArray(config.blockedNotes)) return false;
        
        return true;
    }
    
    validateCCMappings(mappings) {
        if (!Array.isArray(mappings)) return false;
        
        for (const mapping of mappings) {
            if (!mapping.from || !mapping.to) return false;
            if (mapping.from < 0 || mapping.from > 127) return false;
            if (mapping.to < 0 || mapping.to > 127) return false;
        }
        
        return true;
    }
    
    validateNoteMappings(mappings) {
        if (!Array.isArray(mappings)) return false;
        
        for (const mapping of mappings) {
            if (mapping.from === undefined || mapping.to === undefined) return false;
            if (mapping.from < 0 || mapping.from > 127) return false;
            if (mapping.to < 0 || mapping.to > 127) return false;
        }
        
        return true;
    }
    
    validateRoutingConfig(config) {
        if (!config || typeof config !== 'object') return false;
        
        // Vérifier canal
        if (config.channel === undefined || 
            config.channel < 0 || 
            config.channel > 15) {
            return false;
        }
        
        // Vérifier device si présent
        if (config.device && !this.validateDevice(config.device)) {
            return false;
        }
        
        // Vérifier transformations si présentes
        if (config.transformations) {
            const t = config.transformations;
            
            if (t.velocity && !this.validateVelocityConfig(t.velocity)) {
                return false;
            }
            
            if (t.noteFilter && !this.validateNoteFilterConfig(t.noteFilter)) {
                return false;
            }
            
            if (t.ccMapping && !this.validateCCMappings(t.ccMapping.mappings || [])) {
                return false;
            }
        }
        
        return true;
    }
    
    // ========================================================================
    // PRESETS
    // ========================================================================
    
    savePreset(name, description = '') {
        const presets = this.get('presets');
        
        if (presets.length >= this.config.maxPresets) {
            this.logger.warn('RoutingModel', 'Maximum presets reached');
            return null;
        }
        
        const preset = {
            id: `preset_${Date.now()}`,
            name: name,
            description: description,
            channels: JSON.parse(JSON.stringify(this.get('channels'))),
            createdAt: Date.now()
        };
        
        presets.push(preset);
        this.set('presets', presets);
        
        this.logger.info('RoutingModel', `Preset saved: ${name}`);
        
        this.eventBus.emit('routing:preset-saved', { preset });
        
        return preset;
    }
    
    loadPreset(presetId) {
        const presets = this.get('presets');
        const preset = presets.find(p => p.id === presetId);
        
        if (!preset) {
            this.logger.warn('RoutingModel', `Preset not found: ${presetId}`);
            return false;
        }
        
        this.set('channels', JSON.parse(JSON.stringify(preset.channels)));
        this.set('activePreset', presetId);
        
        this.logger.info('RoutingModel', `Preset loaded: ${preset.name}`);
        
        this.eventBus.emit('routing:preset-loaded', { preset });
        
        return true;
    }
    
    deletePreset(presetId) {
        const presets = this.get('presets');
        const index = presets.findIndex(p => p.id === presetId);
        
        if (index === -1) return false;
        
        presets.splice(index, 1);
        this.set('presets', presets);
        
        if (this.get('activePreset') === presetId) {
            this.set('activePreset', null);
        }
        
        this.eventBus.emit('routing:preset-deleted', { presetId });
        
        return true;
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    resetAll() {
        this.set('channels', this.createDefaultChannels());
        this.set('globalMute', false);
        this.set('globalSolo', false);
        
        this.logger.info('RoutingModel', 'All routing reset');
        
        this.eventBus.emit('routing:reset');
    }
    
    getRoutingConfiguration() {
        return {
            channels: this.get('channels'),
            devices: this.get('devices'),
            globalMute: this.get('globalMute'),
            globalSolo: this.get('globalSolo'),
            masterVolume: this.get('masterVolume'),
            activePreset: this.get('activePreset'),
            stats: this.get('stats')
        };
    }
    
    updateDevices(devices) {
        this.set('devices', devices);
        this.eventBus.emit('routing:devices-updated', { devices });
    }
    
    getStats() {
        return {
            ...this.get('stats'),
            assignedChannels: this.getActiveChannels().length,
            totalChannels: 16
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RoutingModel;
}

if (typeof window !== 'undefined') {
    window.RoutingModel = RoutingModel;
}
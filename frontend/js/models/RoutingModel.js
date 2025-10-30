// ============================================================================
// Fichier: frontend/js/models/RoutingModel.js
// Version: v3.1.02 - FIXED LOGGER PROTECTION
// Date: 2025-10-30
// ============================================================================
// CORRECTIONS v3.1.02:
// ✓ Toutes les méthodes sont maintenant DANS la classe
// ✓ Suppression du code hors classe
// ✓ Méthodes setCurrentFile, getRouting, clearAll, etc. correctement placées
// ✓ Protection logger pour éviter erreurs undefined
// ============================================================================


class RoutingModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super({}, {
            persistKey: 'routingmodel',
            eventPrefix: 'routing',
            autoPersist: true
        });
        
        this.eventBus = eventBus || window.eventBus || window.eventBus;
        this.backend = backend || window.backendService || window.app?.services?.backend;
        this.logger = logger || window.logger || console;
        
        if (!this.eventBus) console.error('[RoutingModel] EventBus not available!');
        if (!this.backend) console.warn('[RoutingModel] BackendService not available');
        
        this.data = {
            channels: this.createDefaultChannels(),
            devices: [],
            globalMute: false,
            globalSolo: false,
            masterVolume: 100,
            currentFile: null
        };
        
        if (this.logger && typeof this.logger.info === 'function') {
            if (this.logger && typeof this.logger.info === 'function') this.logger.info('RoutingModel', '✓ Model initialized v3.1.02');
        }
    }
    
    // ========================================================================
    // CRÉATION CANAUX PAR DÃ‰FAUT - SIMPLIFIÉ
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
                volume: 100,
                pan: 64,
                transpose: 0
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
    // ASSIGNATION DEVICES - SIMPLIFIÉ
    // ========================================================================
    
    assignChannelToDevice(channelNumber, deviceId) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) {
            if (this.logger && typeof this.logger.warn === 'function') this.logger.warn('RoutingModel', `Invalid channel: ${channelNumber}`);
            return false;
        }
        
        const oldDevice = channel.device;
        channel.device = deviceId;
        
        this.set('channels', [...channels]);
        
        if (this.logger && typeof this.logger.info === 'function') this.logger.info('RoutingModel', 
            `Channel ${channelNumber} â†’ ${deviceId || 'unassigned'}`);
        
        this.eventBus.emit('routing:channel-assigned', {
            channel: channelNumber,
            oldDevice: oldDevice,
            newDevice: deviceId
        });
        
        return true;
    }
    
    unassignChannel(channelNumber) {
        return this.assignChannelToDevice(channelNumber, null);
    }
    
    // ========================================================================
    // CONTRÃ”LES DE BASE
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
    
    setChannelTranspose(channelNumber, semitones) {
        const channels = this.get('channels');
        const channel = channels[channelNumber];
        
        if (!channel) return;
        
        channel.transpose = Math.max(-24, Math.min(24, semitones));
        
        this.set('channels', [...channels]);
        
        this.eventBus.emit('routing:channel-transpose', {
            channel: channelNumber,
            transpose: channel.transpose
        });
    }
    
    // ========================================================================
    // MUTE/SOLO GLOBAL
    // ========================================================================
    
    setGlobalMute(muted) {
        this.set('globalMute', muted);
        
        this.eventBus.emit('routing:global-mute', { muted });
    }
    
    setGlobalSolo(solo) {
        this.set('globalSolo', solo);
        
        this.eventBus.emit('routing:global-solo', { solo });
    }
    
    setMasterVolume(volume) {
        this.set('masterVolume', Math.max(0, Math.min(127, volume)));
        
        this.eventBus.emit('routing:master-volume', { 
            volume: this.get('masterVolume') 
        });
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    resetAll() {
        this.set('channels', this.createDefaultChannels());
        this.set('globalMute', false);
        this.set('globalSolo', false);
        this.set('masterVolume', 100);
        
        if (this.logger && typeof this.logger.info === 'function') this.logger.info('RoutingModel', 'All routing reset');
        
        this.eventBus.emit('routing:reset');
    }
    
    getRoutingConfiguration() {
        return {
            channels: this.get('channels'),
            devices: this.get('devices'),
            globalMute: this.get('globalMute'),
            globalSolo: this.get('globalSolo'),
            masterVolume: this.get('masterVolume')
        };
    }
    
    updateDevices(devices) {
        this.set('devices', devices);
        this.eventBus.emit('routing:devices-updated', { devices });
    }
    
    getStats() {
        return {
            assignedChannels: this.getActiveChannels().length,
            totalChannels: 16,
            mutedChannels: this.get('channels').filter(ch => ch.muted).length,
            soloedChannels: this.get('channels').filter(ch => ch.solo).length
        };
    }
    
    // ========================================================================
    // MÃ‰THODES AJOUTÃ‰ES POUR COMPATIBILITÃ‰ (maintenant DANS la classe)
    // ========================================================================
    
    /**
     * Définit le fichier courant
     */
    setCurrentFile(fileId) {
        this.set('currentFile', fileId);
        this.eventBus.emit('routing:file-changed', { fileId });
    }
    
    /**
     * Récupère le routing d'un canal spécifique ou la configuration complète
     */
    getRouting(channelId = null) {
        if (channelId !== null) {
            return this.getChannel(channelId);
        }
        return this.getRoutingConfiguration();
    }
    
    /**
     * Efface tous les routings
     */
    clearAll() {
        this.resetAll();
    }
    
    /**
     * Supprime le routing d'un canal
     */
    removeRouting(channelId) {
        this.assignChannelToDevice(channelId, null);
    }
    
    /**
     * Récupère tous les routings
     */
    getAllRoutings() {
        return this.getAllChannels();
    }
    
    /**
     * Assigne un instrument Ã  un canal
     */
    assignInstrument(channelId, instrumentId) {
        this.assignChannelToDevice(channelId, instrumentId);
    }
    
    /**
     * Auto-route les canaux
     */
    autoRoute() {
        const channels = this.getAllChannels();
        // TODO: Implémenter la logique d'auto-routing
        this.eventBus.emit('routing:auto-routed');
    }
    
    /**
     * Récupère la compatibilité globale
     */
    getGlobalCompatibility() {
        return { midiCompatible: true, version: '1.0' };
    }
    
    /**
     * Sauvegarde un preset
     */
    savePreset(name) {
        const preset = {
            name,
            routing: this.getRoutingConfiguration(),
            timestamp: Date.now()
        };
        const presets = JSON.parse(localStorage.getItem('routing_presets') || '[]');
        presets.push(preset);
        localStorage.setItem('routing_presets', JSON.stringify(presets));
        this.eventBus.emit('routing:preset-saved', { preset });
        return preset;
    }
    
    /**
     * Charge un preset
     */
    loadPreset(presetId) {
        const presets = JSON.parse(localStorage.getItem('routing_presets') || '[]');
        const preset = presets.find(p => p.name === presetId);
        if (preset) {
            // Restaurer la configuration
            if (preset.routing) {
                if (preset.routing.channels) {
                    this.set('channels', preset.routing.channels);
                }
                if (preset.routing.globalMute !== undefined) {
                    this.set('globalMute', preset.routing.globalMute);
                }
                if (preset.routing.globalSolo !== undefined) {
                    this.set('globalSolo', preset.routing.globalSolo);
                }
                if (preset.routing.masterVolume !== undefined) {
                    this.set('masterVolume', preset.routing.masterVolume);
                }
            }
            this.eventBus.emit('routing:preset-loaded', { preset });
        }
        return preset;
    }
    
    // ========================================================================
    // MÃ‰THODES HÃ‰RITÃ‰ES/OVERRIDES
    // ========================================================================
    
    /**
     * Override get() depuis BaseModel si nécessaire
     */
    get(key) {
        return this.data[key];
    }
    
    /**
     * Override set() depuis BaseModel si nécessaire
     */
    set(key, value) {
        this.data[key] = value;
        if (this.eventBus) {
            this.eventBus.emit('routing:' + key + '-changed', value);
        }
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

// Export par défaut
window.RoutingModel = RoutingModel;
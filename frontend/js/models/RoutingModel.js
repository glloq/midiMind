// ============================================================================
// Fichier: frontend/js/models/RoutingModel.js
// Version: v3.0.6 - MINIMAL (Constructor fixed + basic functions only)
// Date: 2025-10-19
// ============================================================================
// SIMPLIFICATION: Seulement les fonctions de base pour le routing
// - Assignation canal → device
// - Mute/Solo
// - Volume/Pan basique
// - Pas de transformations avancées
// - Pas de presets complexes
// ============================================================================

class RoutingModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // ✅ FIX: Correct super() call
        super({}, {
            persistKey: 'routingmodel',
            eventPrefix: 'routing',
            autoPersist: true
        });
        
        // ✅ FIX: Assign immediately
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // ✅ FIX: Initialize data directly
        this.data = {
            channels: this.createDefaultChannels(),
            devices: [],
            globalMute: false,
            globalSolo: false,
            masterVolume: 100
        };
        
        this.logger.info('RoutingModel', '✓ Model initialized (minimal version)');
    }
    
    // ========================================================================
    // CRÉATION CANAUX PAR DÉFAUT - SIMPLIFIÉ
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
            this.logger.warn('RoutingModel', `Invalid channel: ${channelNumber}`);
            return false;
        }
        
        const oldDevice = channel.device;
        channel.device = deviceId;
        
        this.set('channels', [...channels]);
        
        this.logger.info('RoutingModel', 
            `Channel ${channelNumber} → ${deviceId || 'unassigned'}`);
        
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
        
        this.logger.info('RoutingModel', 'All routing reset');
        
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
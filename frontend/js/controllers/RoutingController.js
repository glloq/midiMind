// ============================================================================
// Fichier: frontend/js/controllers/RoutingController.js
// Version: v3.0.3 - CORRIGÉ COMPLET
// Date: 2025-10-09
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Contrôleur gérant le routage MIDI avec transformations avancées.
//   Intégration complète avec RoutingModel v3.0.2.
//
// CORRECTIONS v3.0.3:
//   ✅ Validation avant assignation (validateRouting)
//   ✅ Support transformations MIDI (velocity, transpose, note mapping)
//   ✅ Configuration avancée (velocity curves, filters)
//   ✅ Intégration applyTransformations() dans le flux playback
//   ✅ Méthodes de configuration des transformations
//
// Auteur: midiMind Team
// ============================================================================

class RoutingController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Référence au backend (sera injectée par Application)
        this.backend = null;
        
        // Logger - Initialize FIRST
        this.logger = window.logger || console;
        
        // État local
        this.localState = {
            isInitialized: false,
            isSyncing: false,
            lastSync: 0,
            pendingChanges: []
        };
        
        // Configuration
        this.config = {
            syncInterval: 5000,
            autoSave: true,
            confirmReset: true,
            enablePresets: true,
            maxPresets: 10,
            validateBeforeAssign: true,  // ✅ NOUVEAU
            applyTransformations: true    // ✅ NOUVEAU
        };
        
        // Composants UI
        this.routingMatrix = null;
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        // Now initialize
        this.initialize();
    }
    
    // Safe logging helper
    logDebug(category, message, data = null) {
        if (!this.logger) {
            console.log(`[${category}] ${message}`, data || '');
            return;
        }
        
        if (typeof this.logger.debug === 'function') {
            this.logger.debug(category, message, data);
        } else if (typeof this.logger.info === 'function') {
            this.logger.info(category, message, data);
        } else {
            console.log(`[${category}] ${message}`, data || '');
        }
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        // Only initialize if fully ready
        if (!this._fullyInitialized) {
            return;
        }
        
        this.logDebug('routing', '🔀 Initializing RoutingController v3.0.3');
        
        // Créer le modèle s'il n'existe pas
		if (!this.getModel('routing')) {
			if (typeof RoutingModel !== 'undefined') {
				// ✅ Ajouter backend et logger
				const backend = this.models?.backend || window.backendService;
				this.models.routing = new RoutingModel(
					this.eventBus,
					backend,
					this.logger
				);
				this.logDebug('routing', 'RoutingModel created with backend & logger');
			}
		}
        
        // Créer la vue si elle n'existe pas
        if (!this.getView('routing')) {
            if (typeof RoutingView !== 'undefined') {
                this.views.routing = new RoutingView('routing-page', this.eventBus);
                this.logDebug('routing', 'RoutingView created');
            }
        }
        
        this.bindEvents();
        this.setupAutoSync();
        
        this.localState.isInitialized = true;
    }
    
    bindEvents() {
        // Événements du modèle
        this.eventBus.on('routing:channel-assigned', (data) => this.onChannelAssigned(data));
        this.eventBus.on('routing:channel-muted', (data) => this.onChannelMuted(data));
        this.eventBus.on('routing:channel-solo', (data) => this.onChannelSolo(data));
        this.eventBus.on('routing:preset-saved', (data) => this.onPresetSaved(data));
        this.eventBus.on('routing:preset-loaded', (data) => this.onPresetLoaded(data));
        this.eventBus.on('routing:reset', () => this.onReset());
        
        // ✅ NOUVEAU: Événements de transformations
        this.eventBus.on('routing:velocity-mapping', (data) => this.onVelocityMappingChanged(data));
        this.eventBus.on('routing:note-filter', (data) => this.onNoteFilterChanged(data));
        this.eventBus.on('routing:note-remap', (data) => this.onNoteRemapChanged(data));
        this.eventBus.on('routing:cc-remap', (data) => this.onCCRemapChanged(data));
        
        // Événements du backend
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        this.eventBus.on('backend:status', (data) => this.onBackendStatus(data));
        this.eventBus.on('backend:event:routing_changed', (data) => this.onBackendRoutingChanged(data));
        this.eventBus.on('backend:event:devices_changed', (data) => this.onBackendDevicesChanged(data));
        this.eventBus.on('backend:event:channel_activity', (data) => this.onChannelActivity(data));
        
        // Événements UI
        this.eventBus.on('ui:routing-matrix-click', (data) => this.onMatrixClick(data));
    }
    
    /**
     * Configurer la synchronisation automatique
     */
    setupAutoSync() {
        if (this.config.syncInterval > 0) {
            setInterval(() => {
                if (!this.localState.isSyncing) {
                    this.syncWithBackend();
                }
            }, this.config.syncInterval);
        }
    }
    
    // ========================================================================
    // GESTION DU BACKEND
    // ========================================================================
    
    onBackendConnected() {
        this.logDebug('routing', '✓ Backend connected, loading routing configuration');
        this.loadFromBackend();
    }
    
    onBackendDisconnected() {
        this.logDebug('routing', '✗ Backend disconnected');
        
        if (this.config.autoSave) {
            this.saveToLocalStorage();
        }
    }
    
    onBackendStatus(data) {
        if (data.routing) {
            this.updateRoutingFromBackend(data.routing);
        }
        
        if (data.devices) {
            this.updateDevicesFromBackend(data.devices);
        }
    }
    
    onBackendRoutingChanged(data) {
        this.logDebug('routing', 'Routing changed on backend', data);
        this.updateRoutingFromBackend(data);
    }
    
    onBackendDevicesChanged(data) {
        this.logDebug('routing', 'Devices changed on backend', data);
        this.updateDevicesFromBackend(data.devices);
    }
    
    onChannelActivity(data) {
        const model = this.getModel('routing');
        if (model) {
            model.updateChannelActivity(data.channel, data);
        }
        
        if (this.routingMatrix) {
            this.routingMatrix.updateChannelActivity(data.channel, true);
        }
    }
    
    // ========================================================================
    // SYNCHRONISATION
    // ========================================================================
    
    async loadFromBackend() {
        if (!this.backend || !this.backend.isConnected()) {
            this.logDebug('routing', 'Cannot load from backend: not connected');
            return;
        }
        
        this.localState.isSyncing = true;
        
        try {
            const routing = await this.backend.getRouting();
            this.updateRoutingFromBackend(routing);
            
            const devices = await this.backend.listDevices();
            this.updateDevicesFromBackend(devices);
            
            this.localState.lastSync = Date.now();
            this.logDebug('routing', '✓ Routing configuration loaded from backend');
            
        } catch (error) {
            this.logDebug('routing', 'Error loading from backend:', error);
            this.showNotification('Erreur lors du chargement du routage', 'error');
            
        } finally {
            this.localState.isSyncing = false;
        }
    }
    
    async syncWithBackend() {
        if (!this.backend || !this.backend.isConnected()) {
            return;
        }
        
        if (this.localState.pendingChanges.length > 0) {
            await this.applyPendingChanges();
        }
        
        await this.loadFromBackend();
    }
    
    async applyPendingChanges() {
        const changes = [...this.localState.pendingChanges];
        this.localState.pendingChanges = [];
        
        for (const change of changes) {
            try {
                switch (change.type) {
                    case 'assign':
                        await this.backend.setChannelRouting(change.channel, change.device);
                        break;
                    case 'mute':
                        await this.backend.muteChannel(change.channel, change.muted);
                        break;
                    case 'solo':
                        await this.backend.soloChannel(change.channel, change.solo);
                        break;
                    case 'volume':
                        await this.backend.setChannelVolume(change.channel, change.volume);
                        break;
                    case 'transpose':
                        await this.backend.setChannelTranspose(change.channel, change.transpose);
                        break;
                    // ✅ NOUVEAU: Transformations
                    case 'velocity_mapping':
                        await this.backend.sendCommand('routing.set_velocity_mapping', {
                            channel: change.channel,
                            config: change.config
                        });
                        break;
                    case 'note_filter':
                        await this.backend.sendCommand('routing.set_note_filter', {
                            channel: change.channel,
                            config: change.config
                        });
                        break;
                }
            } catch (error) {
                this.logDebug('routing', `Error applying change: ${change.type}`, error);
                this.localState.pendingChanges.push(change);
            }
        }
    }
    
    updateRoutingFromBackend(routingData) {
        const model = this.getModel('routing');
        if (!model) return;
        
        if (routingData.channels) {
            routingData.channels.forEach(channelData => {
                const channel = model.getChannel(channelData.number);
                if (channel) {
                    Object.assign(channel, channelData);
                }
            });
            
            model.set('channels', [...model.get('channels')]);
        }
        
        this.updateView();
    }
    
    updateDevicesFromBackend(devices) {
        const model = this.getModel('routing');
        if (!model) return;
        
        model.updateDevices(devices);
        this.updateView();
    }
    
    // ========================================================================
    // ACTIONS CANAUX - ✅ AVEC VALIDATION
    // ========================================================================
    
    /**
     * Assigner un canal à un device
     * ✅ CORRIGÉ: Avec validation et transformations
     */
    async assignChannelToDevice(channelNumber, deviceId, config = {}) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        // ✅ Construire la configuration complète
        const routing = {
            channel: channelNumber,
            device: deviceId,
            ...config
        };
        
        // ✅ NOUVEAU: Valider AVANT d'assigner
        if (this.config.validateBeforeAssign) {
            const validation = model.validateRouting(routing);
            
            if (!validation.valid) {
                const errors = validation.errors.join(', ');
                this.showError(`Invalid routing: ${errors}`);
                this.logDebug('routing', `Validation failed: ${errors}`);
                return false;
            }
        }
        
        // Mise à jour locale immédiate
        model.assignChannelToDevice(channelNumber, deviceId);
        
        // Appliquer les transformations si configurées
        if (config.transpose !== undefined) {
            model.setChannelTranspose(channelNumber, config.transpose);
        }
        
        if (config.velocity) {
            model.setVelocityMapping(channelNumber, config.velocity);
        }
        
        if (config.noteFilter) {
            model.setNoteFilter(channelNumber, config.noteFilter);
        }
        
        // Ajouter à la file des changements
        this.localState.pendingChanges.push({
            type: 'assign',
            channel: channelNumber,
            device: deviceId
        });
        
        // ✅ NOUVEAU: Activer les transformations dans le flux
        this.eventBus.emit('routing:assigned', {
            channel: channelNumber,
            device: deviceId,
            routing: routing,
            applyTransformations: this.config.applyTransformations
        });
        
        // Envoyer au backend si connecté
        if (this.backend && this.backend.isConnected()) {
            try {
                await this.backend.setChannelRouting(channelNumber, deviceId);
                this.logDebug('routing', `✓ Channel ${channelNumber} assigned to ${deviceId}`);
            } catch (error) {
                this.logDebug('routing', 'Error assigning channel:', error);
                this.showNotification('Erreur lors de l\'assignation', 'error');
            }
        }
        
        return true;
    }
    
    /**
     * Muter/Démuter un canal
     */
    muteChannel(channelNumber, muted = null) {
        const model = this.getModel('routing');
        if (!model) return;
        
        model.muteChannel(channelNumber, muted);
        
        const channel = model.getChannel(channelNumber);
        
        this.localState.pendingChanges.push({
            type: 'mute',
            channel: channelNumber,
            muted: channel.muted
        });
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.muteChannel(channelNumber, channel.muted)
                .catch(error => {
                    this.logDebug('routing', 'Error muting channel:', error);
                });
        }
    }
    
    soloChannel(channelNumber, solo = null) {
        const model = this.getModel('routing');
        if (!model) return;
        
        model.soloChannel(channelNumber, solo);
        
        const channel = model.getChannel(channelNumber);
        
        this.localState.pendingChanges.push({
            type: 'solo',
            channel: channelNumber,
            solo: channel.solo
        });
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.soloChannel(channelNumber, channel.solo)
                .catch(error => {
                    this.logDebug('routing', 'Error soloing channel:', error);
                });
        }
    }
    
    setChannelVolume(channelNumber, volume) {
        const model = this.getModel('routing');
        if (!model) return;
        
        model.setChannelVolume(channelNumber, volume);
        
        this.localState.pendingChanges.push({
            type: 'volume',
            channel: channelNumber,
            volume: volume
        });
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.setChannelVolume(channelNumber, volume)
                .catch(error => {
                    this.logDebug('routing', 'Error setting volume:', error);
                });
        }
    }
    
    setChannelTranspose(channelNumber, semitones) {
        const model = this.getModel('routing');
        if (!model) return;
        
        model.setChannelTranspose(channelNumber, semitones);
        
        this.localState.pendingChanges.push({
            type: 'transpose',
            channel: channelNumber,
            transpose: semitones
        });
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.setChannelTranspose(channelNumber, semitones)
                .catch(error => {
                    this.logDebug('routing', 'Error setting transpose:', error);
                });
        }
    }
    
    setChannelPan(channelNumber, pan) {
        const model = this.getModel('routing');
        if (!model) return;
        
        model.setChannelPan(channelNumber, pan);
        
        this.localState.pendingChanges.push({
            type: 'pan',
            channel: channelNumber,
            pan: pan
        });
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.setChannelPan(channelNumber, pan)
                .catch(error => {
                    this.logDebug('routing', 'Error setting pan:', error);
                });
        }
    }
    
    // ========================================================================
    // TRANSFORMATIONS AVANCÉES - ✅ NOUVEAU
    // ========================================================================
    
    /**
     * Configure le velocity mapping d'un canal
     * @param {number} channelNumber - Numéro de canal (0-15)
     * @param {Object} config - Configuration velocity
     */
    setVelocityMapping(channelNumber, config) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        // Valider la configuration
        if (!model.validateVelocityConfig(config)) {
            this.showError('Invalid velocity configuration');
            return false;
        }
        
        const success = model.setVelocityMapping(channelNumber, config);
        
        if (success) {
            this.localState.pendingChanges.push({
                type: 'velocity_mapping',
                channel: channelNumber,
                config: config
            });
            
            this.showSuccess(`Velocity curve set to "${config.curve}"`);
            
            // Sync avec backend
            if (this.backend && this.backend.isConnected()) {
                this.backend.sendCommand('routing.set_velocity_mapping', {
                    channel: channelNumber,
                    config: config
                }).catch(error => {
                    this.logDebug('routing', 'Error setting velocity mapping:', error);
                });
            }
        }
        
        return success;
    }
    
    /**
     * Configure le filtre de notes
     * @param {number} channelNumber - Numéro de canal (0-15)
     * @param {Object} config - Configuration filtre
     */
    setNoteFilter(channelNumber, config) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        // Valider la configuration
        if (!model.validateNoteFilterConfig(config)) {
            this.showError('Invalid note filter configuration');
            return false;
        }
        
        const success = model.setNoteFilter(channelNumber, config);
        
        if (success) {
            this.localState.pendingChanges.push({
                type: 'note_filter',
                channel: channelNumber,
                config: config
            });
            
            this.showSuccess('Note filter updated');
            
            // Sync avec backend
            if (this.backend && this.backend.isConnected()) {
                this.backend.sendCommand('routing.set_note_filter', {
                    channel: channelNumber,
                    config: config
                }).catch(error => {
                    this.logDebug('routing', 'Error setting note filter:', error);
                });
            }
        }
        
        return success;
    }
    
    /**
     * Configure le remapping de notes
     * @param {number} channelNumber - Numéro de canal (0-15)
     * @param {Array} mappings - Liste de mappings {from, to}
     */
    setNoteRemap(channelNumber, mappings) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        const success = model.setNoteRemap(channelNumber, mappings);
        
        if (success) {
            this.showSuccess(`${mappings.length} note mapping${mappings.length > 1 ? 's' : ''} applied`);
            
            // Sync avec backend
            if (this.backend && this.backend.isConnected()) {
                this.backend.sendCommand('routing.set_note_remap', {
                    channel: channelNumber,
                    mappings: mappings
                }).catch(error => {
                    this.logDebug('routing', 'Error setting note remap:', error);
                });
            }
        }
        
        return success;
    }
    
    /**
     * Configure le remapping de CC
     * @param {number} channelNumber - Numéro de canal (0-15)
     * @param {Array} mappings - Liste de mappings {from, to, invert}
     */
    setCCRemap(channelNumber, mappings) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        const success = model.setCCRemap(channelNumber, mappings);
        
        if (success) {
            this.showSuccess(`${mappings.length} CC mapping${mappings.length > 1 ? 's' : ''} applied`);
            
            // Sync avec backend
            if (this.backend && this.backend.isConnected()) {
                this.backend.sendCommand('routing.set_cc_remap', {
                    channel: channelNumber,
                    mappings: mappings
                }).catch(error => {
                    this.logDebug('routing', 'Error setting CC remap:', error);
                });
            }
        }
        
        return success;
    }
    
    /**
     * Raccourcis pour velocity curves communes
     */
    setVelocityCurveLinear(channelNumber) {
        return this.setVelocityMapping(channelNumber, {
            enabled: true,
            curve: 'linear',
            min: 1,
            max: 127
        });
    }
    
    setVelocityCurveCompress(channelNumber) {
        return this.setVelocityMapping(channelNumber, {
            enabled: true,
            curve: 'compress',
            min: 40,
            max: 100
        });
    }
    
    setVelocityCurveExpand(channelNumber) {
        return this.setVelocityMapping(channelNumber, {
            enabled: true,
            curve: 'expand',
            min: 1,
            max: 127
        });
    }
    
    /**
     * Désactive toutes les transformations d'un canal
     */
    disableAllTransformations(channelNumber) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        const channel = model.getChannel(channelNumber);
        if (!channel) return false;
        
        // Désactiver toutes les transformations
        channel.transformations.velocity.enabled = false;
        channel.transformations.noteFilter.enabled = false;
        channel.transformations.noteRemap.enabled = false;
        channel.transformations.ccRemap.enabled = false;
        
        model.set('channels', [...model.get('channels')]);
        
        this.showSuccess('All transformations disabled');
        
        return true;
    }
    
    // ========================================================================
    // ACTIONS GLOBALES
    // ========================================================================
    
    muteAll() {
        const model = this.getModel('routing');
        if (!model) return;
        
        model.muteAll();
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.sendCommand('mute_all')
                .catch(error => {
                    this.logDebug('routing', 'Error muting all:', error);
                });
        }
        
        this.showNotification('Tous les canaux ont été mutés', 'info');
    }
    
    unmuteAll() {
        const model = this.getModel('routing');
        if (!model) return;
        
        model.unmuteAll();
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.sendCommand('unmute_all')
                .catch(error => {
                    this.logDebug('routing', 'Error unmuting all:', error);
                });
        }
        
        this.showNotification('Tous les canaux ont été démutés', 'info');
    }
    
    async resetAll() {
        if (this.config.confirmReset) {
            const confirmed = await this.confirmAction(
                'Êtes-vous sûr de vouloir réinitialiser tout le routage ?',
                'Réinitialisation'
            );
            
            if (!confirmed) return;
        }
        
        const model = this.getModel('routing');
        if (!model) return;
        
        model.resetAll();
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.sendCommand('reset_routing')
                .catch(error => {
                    this.logDebug('routing', 'Error resetting routing:', error);
                });
        }
        
        this.showNotification('Routage réinitialisé', 'success');
    }
    
    setMasterVolume(volume) {
        const model = this.getModel('routing');
        if (!model) return;
        
        model.setMasterVolume(volume);
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.sendCommand('set_master_volume', { volume })
                .catch(error => {
                    this.logDebug('routing', 'Error setting master volume:', error);
                });
        }
    }
    
    // ========================================================================
    // PRESETS
    // ========================================================================
    
    async savePreset(name = null) {
        if (!name) {
            name = await this.promptInput('Nom du preset:', 'Enregistrer preset', 'Mon Preset');
            if (!name) return;
        }
        
        const model = this.getModel('routing');
        if (!model) return;
        
        const preset = model.savePreset(name);
        
        if (preset) {
            this.showNotification(`Preset "${name}" enregistré`, 'success');
            this.savePresetsToLocalStorage();
            
            if (this.backend && this.backend.isConnected()) {
                this.backend.sendCommand('save_preset', preset)
                    .catch(error => {
                        this.logDebug('routing', 'Error saving preset to backend:', error);
                    });
            }
        } else {
            this.showNotification('Limite de presets atteinte', 'warning');
        }
    }
    
    async loadPreset(presetId) {
        const model = this.getModel('routing');
        if (!model) return;
        
        const preset = model.loadPreset(presetId);
        
        if (preset) {
            this.showNotification(`Preset "${preset.name}" chargé`, 'success');
            
            if (this.backend && this.backend.isConnected()) {
                await this.syncWithBackend();
            }
        } else {
            this.showNotification('Preset introuvable', 'error');
        }
    }
    
    async deletePreset(presetId) {
        const model = this.getModel('routing');
        if (!model) return;
        
        const preset = model.get('presets').find(p => p.id === presetId);
        if (!preset) return;
        
        const confirmed = await this.confirmAction(
            `Supprimer le preset "${preset.name}" ?`,
            'Confirmation'
        );
        
        if (!confirmed) return;
        
        model.deletePreset(presetId);
        this.showNotification(`Preset "${preset.name}" supprimé`, 'success');
        this.savePresetsToLocalStorage();
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.sendCommand('delete_preset', { id: presetId })
                .catch(error => {
                    this.logDebug('routing', 'Error deleting preset from backend:', error);
                });
        }
    }
    
    // ========================================================================
    // STOCKAGE LOCAL
    // ========================================================================
    
    saveToLocalStorage() {
        const model = this.getModel('routing');
        if (!model) return;
        
        const config = model.getRoutingConfiguration();
        
        try {
            localStorage.setItem('routing_configuration', JSON.stringify(config));
            this.logDebug('routing', 'Configuration saved to localStorage');
        } catch (error) {
            this.logDebug('routing', 'Error saving to localStorage:', error);
        }
    }
    
    loadFromLocalStorage() {
        try {
            const saved = localStorage.getItem('routing_configuration');
            if (saved) {
                const config = JSON.parse(saved);
                
                const model = this.getModel('routing');
                if (model) {
                    model.set('channels', config.channels);
                    model.set('masterVolume', config.masterVolume || 100);
                    
                    this.updateView();
                    this.logDebug('routing', 'Configuration loaded from localStorage');
                }
            }
        } catch (error) {
            this.logDebug('routing', 'Error loading from localStorage:', error);
        }
    }
    
    savePresetsToLocalStorage() {
        const model = this.getModel('routing');
        if (!model) return;
        
        const presets = model.get('presets');
        
        try {
            localStorage.setItem('routing_presets', JSON.stringify(presets));
            this.logDebug('routing', 'Presets saved to localStorage');
        } catch (error) {
            this.logDebug('routing', 'Error saving presets:', error);
        }
    }
    
    loadPresetsFromLocalStorage() {
        try {
            const saved = localStorage.getItem('routing_presets');
            if (saved) {
                const presets = JSON.parse(saved);
                
                const model = this.getModel('routing');
                if (model) {
                    model.set('presets', presets);
                    this.logDebug('routing', `${presets.length} presets loaded from localStorage`);
                }
            }
        } catch (error) {
            this.logDebug('routing', 'Error loading presets:', error);
        }
    }
    
    // ========================================================================
    // MISE À JOUR DE LA VUE
    // ========================================================================
    
    updateView() {
        const view = this.getView('routing');
        const model = this.getModel('routing');
        
        if (view && model) {
            const data = model.getRoutingConfiguration();
            view.render(data);
            
            if (this.routingMatrix) {
                this.routingMatrix.updateData(data);
            }
        }
    }
    
    showRoutingPage() {
        this.eventBus.emit('navigation:show-page', { page: 'routing' });
        
        if (!this.localState.isInitialized) {
            this.initialize();
        }
        
        if (Date.now() - this.localState.lastSync > 30000) {
            this.loadFromBackend();
        }
    }
    
    // ========================================================================
    // ÉVÉNEMENTS DU MODÈLE
    // ========================================================================
    
    onChannelAssigned(data) {
        this.logDebug('routing', `Channel ${data.channel} assigned to ${data.device}`);
        this.updateView();
    }
    
    onChannelMuted(data) {
        this.logDebug('routing', `Channel ${data.channel} ${data.muted ? 'muted' : 'unmuted'}`);
    }
    
    onChannelSolo(data) {
        this.logDebug('routing', `Channel ${data.channel} ${data.solo ? 'soloed' : 'unsoloed'}`);
    }
    
    onPresetSaved(data) {
        this.logDebug('routing', `Preset saved: ${data.preset.name}`);
    }
    
    onPresetLoaded(data) {
        this.logDebug('routing', `Preset loaded: ${data.preset.name}`);
        this.updateView();
    }
    
    onReset() {
        this.logDebug('routing', 'Routing reset');
        this.updateView();
    }
    
    // ✅ NOUVEAU: Callbacks transformations
    onVelocityMappingChanged(data) {
        this.logDebug('routing', `Velocity mapping changed for channel ${data.channel}: ${data.config.curve}`);
        this.updateView();
    }
    
    onNoteFilterChanged(data) {
        this.logDebug('routing', `Note filter changed for channel ${data.channel}: ${data.config.mode}`);
        this.updateView();
    }
    
    onNoteRemapChanged(data) {
        this.logDebug('routing', `Note remap changed for channel ${data.channel}: ${data.mappings.length} mappings`);
        this.updateView();
    }
    
    onCCRemapChanged(data) {
        this.logDebug('routing', `CC remap changed for channel ${data.channel}: ${data.mappings.length} mappings`);
        this.updateView();
    }
    
    // ========================================================================
    // ÉVÉNEMENTS UI
    // ========================================================================
    
    onMatrixClick(data) {
        if (data.action === 'assign') {
            this.assignChannelToDevice(data.channel, data.device);
        } else if (data.action === 'mute') {
            this.muteChannel(data.channel);
        } else if (data.action === 'solo') {
            this.soloChannel(data.channel);
        }
    }
    
    // ========================================================================
    // MÉTHODES D'AIDE UI
    // ========================================================================
    
    async confirmAction(message, title) {
        return new Promise(resolve => {
            if (typeof Modal !== 'undefined') {
                Modal.confirm(message, title, 
                    () => resolve(true),
                    () => resolve(false)
                );
            } else {
                resolve(confirm(message));
            }
        });
    }
    
    async promptInput(message, title, defaultValue) {
        return new Promise(resolve => {
            if (typeof Modal !== 'undefined') {
                Modal.prompt(message, title, defaultValue, 
                    (value) => resolve(value)
                );
            } else {
                resolve(prompt(message, defaultValue));
            }
        });
    }
    
    
    // ========================================================================
    // API PUBLIQUE - Méthodes de la référence
    // ========================================================================
    
    /**
     * Charge la configuration de routage
     * @returns {Promise<boolean>}
     */
    async loadRouting() {
        try {
            await this.loadFromBackend();
            return true;
        } catch (error) {
            this.logDebug('routing', 'Error loading routing:', error);
            return false;
        }
    }
    
    /**
     * Sauvegarde la configuration de routage
     * @returns {Promise<boolean>}
     */
    async saveRouting() {
        try {
            await this.syncWithBackend();
            this.showNotification('Routing saved', 'success');
            return true;
        } catch (error) {
            this.logDebug('routing', 'Error saving routing:', error);
            this.showNotification('Error saving routing', 'error');
            return false;
        }
    }
    
    /**
     * Assigne un canal à un périphérique (alias pour assignChannelToDevice)
     * @param {number} channelId - Numéro du canal
     * @param {string} deviceId - ID du périphérique
     * @returns {Promise<boolean>}
     */
    async assignChannel(channelId, deviceId) {
        return await this.assignChannelToDevice(channelId, deviceId);
    }
    
    /**
     * Teste une route en envoyant une note test
     * @param {string} routeId - ID de la route (format: "channel-device")
     * @returns {Promise<boolean>}
     */
    async testRoute(routeId) {
        try {
            const [channel, device] = routeId.split('-');
            
            if (!channel || !device) {
                this.showError('Invalid route ID');
                return false;
            }
            
            // Envoyer une note de test via le backend
            if (this.backend && this.backend.isConnected()) {
                await this.backend.sendCommand('midi.test_note', {
                    channel: parseInt(channel),
                    device: device,
                    note: 60,  // Middle C
                    velocity: 100,
                    duration: 500
                });
                
                this.showNotification(`Testing route: Channel ${channel} → ${device}`, 'info');
                return true;
            } else {
                this.showError('Backend not connected');
                return false;
            }
            
        } catch (error) {
            this.logDebug('routing', 'Error testing route:', error);
            this.showError('Route test failed');
            return false;
        }
    
    
    // ========================================================================
    // FONCTIONS PUBLIQUES (CONFORMITÉ RÉFÉRENCE)
    // ========================================================================
    
    /**
     * Crée une nouvelle route entre un canal et un périphérique
     * @param {number} channel - Canal MIDI (1-16)
     * @param {string} deviceId - ID du périphérique
     * @param {Object} options - Options de routage
     * @returns {Promise<boolean>}
     */
    async createRoute(channel, deviceId, options = {}) {
        return await this.assignChannel(channel, deviceId, options);
    }
    
    /**
     * Supprime une route
     * @param {number} channel - Canal MIDI (1-16)
     * @returns {Promise<boolean>}
     */
    async removeRoute(channel) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        model.unassignChannel(channel);
        await this.syncWithBackend();
        this.eventBus.emit('routing:route-removed', { channel });
        return true;
    }
    
    /**
     * Met à jour une route existante
     * @param {number} channel - Canal MIDI (1-16)
     * @param {Object} options - Nouvelles options
     * @returns {Promise<boolean>}
     */
    async updateRoute(channel, options) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        const currentRoute = model.getChannelRoute(channel);
        if (!currentRoute) {
            this.showError(`No route for channel ${channel}`);
            return false;
        }
        
        return await this.assignChannel(channel, currentRoute.deviceId, {
            ...currentRoute,
            ...options
        });
    }
    
    /**
     * Obtient toutes les routes configurées
     * @returns {Array}
     */
    getRoutes() {
        const model = this.getModel('routing');
        if (!model) return [];
        
        return model.getAllRoutes();
    }
    
    /**
     * Auto-routage automatique des canaux aux périphériques
     * @returns {Promise<boolean>}
     */
    async autoRoute() {
        const model = this.getModel('routing');
        if (!model) {
            this.showError('Routing model not available');
            return false;
        }
        
        try {
            this.logDebug('routing', 'Starting auto-route...');
            
            const devices = model.getAvailableDevices();
            if (devices.length === 0) {
                this.showWarning('No devices available for routing');
                return false;
            }
            
            let routedCount = 0;
            for (let channel = 1; channel <= 16; channel++) {
                const deviceIndex = (channel - 1) % devices.length;
                const device = devices[deviceIndex];
                
                await this.assignChannel(channel, device.id);
                routedCount++;
            }
            
            this.showSuccess(`Auto-routed ${routedCount} channels to ${devices.length} device(s)`);
            this.logDebug('routing', `✅ Auto-route complete: ${routedCount} channels`);
            
            return true;
            
        } catch (error) {
            this.logDebug('error', 'Auto-route failed:', error);
            this.showError('Auto-route failed: ' + error.message);
            return false;
        }
    }
    
    /**
     * Efface tout le routage
     * @returns {Promise<boolean>}
     */
    async clearRouting() {
        if (this.config.confirmReset) {
            const confirmed = await this.confirmAction(
                'Clear all routing?',
                'This will remove all channel assignments.'
            );
            if (!confirmed) return false;
        }
        
        const model = this.getModel('routing');
        if (!model) return false;
        
        try {
            this.logDebug('routing', 'Clearing all routing...');
            
            model.reset();
            await this.syncWithBackend();
            
            this.showSuccess('All routing cleared');
            this.logDebug('routing', '✅ Routing cleared');
            
            return true;
            
        } catch (error) {
            this.logDebug('error', 'Clear routing failed:', error);
            this.showError('Failed to clear routing: ' + error.message);
            return false;
        }
    }


}

    // ========================================================================
    // EXPORT/IMPORT
    // ========================================================================
    
    exportConfiguration() {
        const model = this.getModel('routing');
        if (!model) return;
        
        const config = model.getRoutingConfiguration();
        const json = JSON.stringify(config, null, 2);
        
        const blob = new Blob([json], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        
        const a = document.createElement('a');
        a.href = url;
        a.download = `routing_config_${new Date().getTime()}.json`;
        a.click();
        
        URL.revokeObjectURL(url);
        
        this.showNotification('Configuration exportée', 'success');
    }
    
    importConfiguration() {
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.json';
        
        input.onchange = (event) => {
            const file = event.target.files[0];
            if (!file) return;
            
            const reader = new FileReader();
            reader.onload = (e) => {
                try {
                    const config = JSON.parse(e.target.result);
                    
                    if (!config.channels || !Array.isArray(config.channels)) {
                        throw new Error('Format invalide');
                    }
                    
                    const model = this.getModel('routing');
                    if (model) {
                        model.set('channels', config.channels);
                        model.set('masterVolume', config.masterVolume || 100);
                        
                        if (config.presets) {
                            model.set('presets', config.presets);
                        }
                        
                        this.updateView();
                        this.syncWithBackend();
                        
                        this.showNotification('Configuration importée', 'success');
                    }
                    
                } catch (error) {
                    this.logDebug('routing', 'Error importing configuration:', error);
                    this.showNotification('Erreur lors de l\'import', 'error');
                }
            };
            
            reader.readAsText(file);
        };
        
        input.click();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RoutingController;
}

if (typeof window !== 'undefined') {
    window.RoutingController = RoutingController;
}
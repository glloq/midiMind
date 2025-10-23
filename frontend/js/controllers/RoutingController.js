// ============================================================================
// Fichier: frontend/js/controllers/RoutingController.js
// Version: v3.0.3 - CORRIGÃƒâ€° COMPLET
// Date: 2025-10-09
// Projet: midiMind v3.0 - SystÃƒÂ¨me d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   ContrÃƒÂ´leur gÃƒÂ©rant le routage MIDI avec transformations avancÃƒÂ©es.
//   IntÃƒÂ©gration complÃƒÂ¨te avec RoutingModel v3.0.2.
//
// CORRECTIONS v3.0.3:
//   Ã¢Å“â€¦ Validation avant assignation (validateRouting)
//   Ã¢Å“â€¦ Support transformations MIDI (velocity, transpose, note mapping)
//   Ã¢Å“â€¦ Configuration avancÃƒÂ©e (velocity curves, filters)
//   Ã¢Å“â€¦ IntÃƒÂ©gration applyTransformations() dans le flux playback
//   Ã¢Å“â€¦ MÃƒÂ©thodes de configuration des transformations
//
// Auteur: midiMind Team
// ============================================================================

class RoutingController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // RÃƒÂ©fÃƒÂ©rence au backend (sera injectÃƒÂ©e par Application)
        this.backend = null;
        
        // Logger - Initialize FIRST
        this.logger = window.logger || console;
        
        // Ãƒâ€°tat local
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
            validateBeforeAssign: true,  // Ã¢Å“â€¦ NOUVEAU
            applyTransformations: true    // Ã¢Å“â€¦ NOUVEAU
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
        
        this.logDebug('routing', 'Ã°Å¸â€â‚¬ Initializing RoutingController v3.0.3');
        
        // CrÃƒÂ©er le modÃƒÂ¨le s'il n'existe pas
		if (!this.getModel('routing')) {
			if (typeof RoutingModel !== 'undefined') {
				// Ã¢Å“â€¦ Ajouter backend et logger
				const backend = this.models?.backend || window.backendService;
				this.models.routing = new RoutingModel(
					this.eventBus,
					backend,
					this.logger
				);
				this.logDebug('routing', 'RoutingModel created with backend & logger');
			}
		}
        
        // CrÃƒÂ©er la vue si elle n'existe pas
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
        // Ãƒâ€°vÃƒÂ©nements du modÃƒÂ¨le
        this.eventBus.on('routing:channel-assigned', (data) => this.onChannelAssigned(data));
        this.eventBus.on('routing:channel-muted', (data) => this.onChannelMuted(data));
        this.eventBus.on('routing:channel-solo', (data) => this.onChannelSolo(data));
        this.eventBus.on('routing:preset-saved', (data) => this.onPresetSaved(data));
        this.eventBus.on('routing:preset-loaded', (data) => this.onPresetLoaded(data));
        this.eventBus.on('routing:reset', () => this.onReset());
        
        // Ã¢Å“â€¦ NOUVEAU: Ãƒâ€°vÃƒÂ©nements de transformations
        this.eventBus.on('routing:velocity-mapping', (data) => this.onVelocityMappingChanged(data));
        this.eventBus.on('routing:note-filter', (data) => this.onNoteFilterChanged(data));
        this.eventBus.on('routing:note-remap', (data) => this.onNoteRemapChanged(data));
        this.eventBus.on('routing:cc-remap', (data) => this.onCCRemapChanged(data));
        
        // Ãƒâ€°vÃƒÂ©nements du backend
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        this.eventBus.on('backend:status', (data) => this.onBackendStatus(data));
        this.eventBus.on('backend:event:routing_changed', (data) => this.onBackendRoutingChanged(data));
        this.eventBus.on('backend:event:devices_changed', (data) => this.onBackendDevicesChanged(data));
        this.eventBus.on('backend:event:channel_activity', (data) => this.onChannelActivity(data));
        
        // Ãƒâ€°vÃƒÂ©nements UI
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
        this.logDebug('routing', 'Ã¢Å“â€œ Backend connected, loading routing configuration');
        this.loadFromBackend();
    }
    
    onBackendDisconnected() {
        this.logDebug('routing', 'Ã¢Å“â€” Backend disconnected');
        
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
            this.logDebug('routing', 'Ã¢Å“â€œ Routing configuration loaded from backend');
            
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
                    // Ã¢Å“â€¦ NOUVEAU: Transformations
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
    // ACTIONS CANAUX - Ã¢Å“â€¦ AVEC VALIDATION
    // ========================================================================
    
    /**
     * Assigner un canal ÃƒÂ  un device
     * Ã¢Å“â€¦ CORRIGÃƒâ€°: Avec validation et transformations
     */
    async assignChannelToDevice(channelNumber, deviceId, config = {}) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        // Ã¢Å“â€¦ Construire la configuration complÃƒÂ¨te
        const routing = {
            channel: channelNumber,
            device: deviceId,
            ...config
        };
        
        // Ã¢Å“â€¦ NOUVEAU: Valider AVANT d'assigner
        if (this.config.validateBeforeAssign) {
            const validation = model.validateRouting(routing);
            
            if (!validation.valid) {
                const errors = validation.errors.join(', ');
                this.showError(`Invalid routing: ${errors}`);
                this.logDebug('routing', `Validation failed: ${errors}`);
                return false;
            }
        }
        
        // Mise ÃƒÂ  jour locale immÃƒÂ©diate
        model.assignChannelToDevice(channelNumber, deviceId);
        
        // Appliquer les transformations si configurÃƒÂ©es
        if (config.transpose !== undefined) {
            model.setChannelTranspose(channelNumber, config.transpose);
        }
        
        if (config.velocity) {
            model.setVelocityMapping(channelNumber, config.velocity);
        }
        
        if (config.noteFilter) {
            model.setNoteFilter(channelNumber, config.noteFilter);
        }
        
        // Ajouter ÃƒÂ  la file des changements
        this.localState.pendingChanges.push({
            type: 'assign',
            channel: channelNumber,
            device: deviceId
        });
        
        // Ã¢Å“â€¦ NOUVEAU: Activer les transformations dans le flux
        this.eventBus.emit('routing:assigned', {
            channel: channelNumber,
            device: deviceId,
            routing: routing,
            applyTransformations: this.config.applyTransformations
        });
        
        // Envoyer au backend si connectÃƒÂ©
        if (this.backend && this.backend.isConnected()) {
            try {
                await this.backend.setChannelRouting(channelNumber, deviceId);
                this.logDebug('routing', `Ã¢Å“â€œ Channel ${channelNumber} assigned to ${deviceId}`);
            } catch (error) {
                this.logDebug('routing', 'Error assigning channel:', error);
                this.showNotification('Erreur lors de l\'assignation', 'error');
            }
        }
        
        return true;
    }
    
    /**
     * Muter/DÃƒÂ©muter un canal
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
    // TRANSFORMATIONS AVANCÃƒâ€°ES - Ã¢Å“â€¦ NOUVEAU
    // ========================================================================
    
    /**
     * Configure le velocity mapping d'un canal
     * @param {number} channelNumber - NumÃƒÂ©ro de canal (0-15)
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
     * @param {number} channelNumber - NumÃƒÂ©ro de canal (0-15)
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
     * @param {number} channelNumber - NumÃƒÂ©ro de canal (0-15)
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
     * @param {number} channelNumber - NumÃƒÂ©ro de canal (0-15)
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
     * DÃƒÂ©sactive toutes les transformations d'un canal
     */
    disableAllTransformations(channelNumber) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        const channel = model.getChannel(channelNumber);
        if (!channel) return false;
        
        // DÃƒÂ©sactiver toutes les transformations
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
        
        this.showNotification('Tous les canaux ont ÃƒÂ©tÃƒÂ© mutÃƒÂ©s', 'info');
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
        
        this.showNotification('Tous les canaux ont ÃƒÂ©tÃƒÂ© dÃƒÂ©mutÃƒÂ©s', 'info');
    }
    
    async resetAll() {
        if (this.config.confirmReset) {
            const confirmed = await this.confirmAction(
                'ÃƒÅ tes-vous sÃƒÂ»r de vouloir rÃƒÂ©initialiser tout le routage ?',
                'RÃƒÂ©initialisation'
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
        
        this.showNotification('Routage rÃƒÂ©initialisÃƒÂ©', 'success');
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
    
    async savePreset() {
        if (!this.config.enablePresets) return;
        
        const name = await this.promptInput(
            'Nom du preset:',
            'Sauvegarder le preset',
            `Preset ${new Date().toLocaleDateString()}`
        );
        
        if (!name) return;
        
        const model = this.getModel('routing');
        if (!model) return;
        
        const preset = model.savePreset(name);
        
        this.savePresetsToLocalStorage();
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.sendCommand('save_routing_preset', { preset })
                .catch(error => {
                    this.logDebug('routing', 'Error saving preset:', error);
                });
        }
        
        this.showNotification(`Preset "${name}" sauvegardÃƒÂ©`, 'success');
    }
    
    loadPreset(presetId) {
        const model = this.getModel('routing');
        if (!model) return;
        
        try {
            model.loadPreset(presetId);
            
            if (this.backend && this.backend.isConnected()) {
                const config = model.getRoutingConfiguration();
                this.backend.sendCommand('load_routing_preset', { config })
                    .catch(error => {
                        this.logDebug('routing', 'Error loading preset:', error);
                    });
            }
            
            this.showNotification('Preset chargÃƒÂ©', 'success');
            
        } catch (error) {
            this.logDebug('routing', 'Error loading preset:', error);
            this.showNotification('Erreur lors du chargement du preset', 'error');
        }
    }
    
    async deletePreset(presetId) {
        const confirmed = await this.confirmAction(
            'ÃƒÅ tes-vous sÃƒÂ»r de vouloir supprimer ce preset ?',
            'Suppression'
        );
        
        if (!confirmed) return;
        
        const model = this.getModel('routing');
        if (!model) return;
        
        model.deletePreset(presetId);
        
        this.savePresetsToLocalStorage();
        
        if (this.backend && this.backend.isConnected()) {
            this.backend.sendCommand('delete_routing_preset', { presetId })
                .catch(error => {
                    this.logDebug('routing', 'Error deleting preset:', error);
                });
        }
        
        this.showNotification('Preset supprimÃƒÂ©', 'success');
    }
    
    deleteCurrentPreset() {
        const model = this.getModel('routing');
        if (!model) return;
        
        const activePreset = model.get('activePreset');
        if (activePreset) {
            this.deletePreset(activePreset);
        }
    }
    
    // ========================================================================
    // PERSISTANCE LOCALE
    // ========================================================================
    
    saveToLocalStorage() {
        const model = this.getModel('routing');
        if (!model) return;
        
        const config = model.getRoutingConfiguration();
        
        try {
            localStorage.setItem('midiMind_routing', JSON.stringify(config));
            this.logDebug('routing', 'Routing saved to localStorage');
        } catch (error) {
            this.logDebug('routing', 'Error saving to localStorage:', error);
        }
    }
    
    loadFromLocalStorage() {
        try {
            const saved = localStorage.getItem('midiMind_routing');
            if (saved) {
                const config = JSON.parse(saved);
                
                const model = this.getModel('routing');
                if (model && config.channels) {
                    model.set('channels', config.channels);
                    model.set('masterVolume', config.masterVolume || 100);
                    
                    this.logDebug('routing', 'Routing loaded from localStorage');
                    this.updateView();
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
            localStorage.setItem('midiMind_routing_presets', JSON.stringify(presets));
            this.logDebug('routing', 'Presets saved to localStorage');
        } catch (error) {
            this.logDebug('routing', 'Error saving presets:', error);
        }
    }
    
    loadPresetsFromLocalStorage() {
        try {
            const saved = localStorage.getItem('midiMind_routing_presets');
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
    // MISE Ãƒâ‚¬ JOUR DE LA VUE
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
    // Ãƒâ€°VÃƒâ€°NEMENTS DU MODÃƒË†LE
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
    
    // Ã¢Å“â€¦ NOUVEAU: Callbacks transformations
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
    // Ãƒâ€°VÃƒâ€°NEMENTS UI
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
    // MÃƒâ€°THODES D'AIDE UI
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
        
        this.showNotification('Configuration exportÃƒÂ©e', 'success');
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
                        
                        this.showNotification('Configuration importÃƒÂ©e', 'success');
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
window.RoutingController = RoutingController;
// ============================================================================
// Fichier: frontend/js/controllers/RoutingController.js
// Version: 4.2.1 - API BACKEND FULL COMPATIBILITY + FEATURES COMPLÃˆTES
// Date: 2025-10-28
// ============================================================================
// Description:
//   ContrÃ´leur gÃ©rant le routage MIDI avec transformations avancÃ©es.
//   Support complet API v4.2.1 + toutes fonctionnalitÃ©s existantes.
//
// MODIFICATIONS v4.2.1:
//   âœ… Support routing.enableRoute, routing.disableRoute
//   âœ… Gestion format API (request/response standardisÃ©)
//   âœ… Statistiques routing via routing.getStats
//   âœ… Conservation TOUTES fonctionnalitÃ©s existantes
//   âœ… Transformations MIDI (velocity, transpose, filters)
//   âœ… Presets complets
//   âœ… LocalStorage
//   âœ… Auto-routing
//   âœ… Test routes
//
// Auteur: MidiMind Team
// ============================================================================

class RoutingController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // RÃ©fÃ©rence au backend (sera injectÃ©e par Application)
        this.backend = null;
        
        // Logger - Initialize FIRST
        this.logger = window.logger || console;
        
        // Ã‰tat local
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
            validateBeforeAssign: true,  // âœ… Validation avant assignation
            applyTransformations: true    // âœ… Application transformations
        };
        
        // âœ… NOUVEAU v4.2.1: Ã‰tat des routes avec enable/disable
        this.routes = new Map(); // key: "source_id:destination_id" -> route object
        this.routeStats = {
            total: 0,
            enabled: 0,
            disabled: 0,
            messagesProcessed: 0,
            lastUpdate: null
        };
        
        // âœ… NOUVEAU v4.2.1: Synchronisation auto
        this.autoSyncEnabled = true;
        this.autoSyncTimer = null;
        
        // Composants UI
        this.routingMatrix = null;
        this.selectedRoute = null;
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        // Now initialize
        // ✅ REMOVED: this.initialize() - BaseController calls it via autoInitialize
    }
    
    // ========================================================================
    // LOGGING HELPER
    // ========================================================================
    
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
        
        this.logDebug('routing', 'ðŸ”€ Initializing RoutingController v4.2.1');
        
        // CrÃ©er le modÃ¨le s'il n'existe pas
        if (!this.getModel('routing')) {
            if (typeof RoutingModel !== 'undefined') {
                // âœ… Ajouter backend et logger
                const backend = this.models?.backend || window.backendService;
                this.models.routing = new RoutingModel(
                    this.eventBus,
                    backend,
                    this.logger
                );
                this.logDebug('routing', 'RoutingModel created with backend & logger');
            }
        }
        
        // CrÃ©er la vue si elle n'existe pas
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
        // Ã‰vÃ©nements du modÃ¨le
        this.eventBus.on('routing:channel-assigned', (data) => this.onChannelAssigned(data));
        this.eventBus.on('routing:channel-muted', (data) => this.onChannelMuted(data));
        this.eventBus.on('routing:channel-solo', (data) => this.onChannelSolo(data));
        this.eventBus.on('routing:preset-saved', (data) => this.onPresetSaved(data));
        this.eventBus.on('routing:preset-loaded', (data) => this.onPresetLoaded(data));
        this.eventBus.on('routing:reset', () => this.onReset());
        
        // Ã‰vÃ©nements de transformations
        this.eventBus.on('routing:velocity-mapping', (data) => this.onVelocityMappingChanged(data));
        this.eventBus.on('routing:note-filter', (data) => this.onNoteFilterChanged(data));
        this.eventBus.on('routing:note-remap', (data) => this.onNoteRemapChanged(data));
        this.eventBus.on('routing:cc-remap', (data) => this.onCCRemapChanged(data));
        
        // Ã‰vÃ©nements du backend
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        this.eventBus.on('backend:status', (data) => this.onBackendStatus(data));
        this.eventBus.on('backend:event:routing_changed', (data) => this.onBackendRoutingChanged(data));
        this.eventBus.on('backend:event:devices_changed', (data) => this.onBackendDevicesChanged(data));
        this.eventBus.on('backend:event:channel_activity', (data) => this.onChannelActivity(data));
        
        // âœ… NOUVEAU v4.2.1: Ã‰vÃ©nements routes
        this.eventBus.on('backend:route:added', (data) => this.onBackendRoutingChanged(data));
        this.eventBus.on('backend:route:removed', (data) => this.onBackendRoutingChanged(data));
        this.eventBus.on('backend:route:updated', (data) => this.onBackendRoutingChanged(data));
        
        // Ã‰vÃ©nements UI
        this.eventBus.on('ui:routing-matrix-click', (data) => this.onMatrixClick(data));
        
        // Navigation
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'routing') {
                this.onRoutingPageActive();
            } else {
                this.onRoutingPageInactive();
            }
        });
        
        // Demandes de refresh
        this.eventBus.on('routing:request_refresh', () => this.syncWithBackend());
    }
    
    /**
     * Configurer la synchronisation automatique
     */
    setupAutoSync() {
        if (this.config.syncInterval > 0 && this.autoSyncEnabled) {
            this.autoSyncTimer = setInterval(() => {
                if (!this.localState.isSyncing && this.backend?.isConnected()) {
                    this.syncWithBackend();
                }
            }, this.config.syncInterval);
            
            this.logDebug('routing', 'âœ“ Auto-sync started');
        }
    }
    
    stopAutoSync() {
        if (this.autoSyncTimer) {
            clearInterval(this.autoSyncTimer);
            this.autoSyncTimer = null;
        }
    }
    
    // ========================================================================
    // GESTION DU BACKEND
    // ========================================================================
    
    onBackendConnected() {
        this.logDebug('routing', 'âœ“ Backend connected, loading routing configuration');
        
        // Charger configuration
        this.loadFromBackend();
        
        // DÃ©marrer auto-sync
        if (this.autoSyncEnabled) {
            this.setupAutoSync();
        }
    }
    
    onBackendDisconnected() {
        this.logDebug('routing', 'âœ— Backend disconnected');
        
        // ArrÃªter auto-sync
        this.stopAutoSync();
        
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
        
        if (data.connected && !this.autoSyncTimer && this.autoSyncEnabled) {
            this.setupAutoSync();
        }
    }
    
    onBackendRoutingChanged(data) {
        this.logDebug('routing', 'Routing changed on backend', data);
        this.updateRoutingFromBackend(data);
        this.syncWithBackend();
    }
    
    onBackendDevicesChanged(data) {
        this.logDebug('routing', 'Devices changed on backend', data);
        this.updateDevicesFromBackend(data.devices);
        this.syncWithBackend();
    }
    
    onChannelActivity(data) {
        const model = this.getModel('routing');
        if (model) {
            model.updateChannelActivity(data.channel, data);
        }
        
        if (this.routingMatrix) {
            this.routingMatrix.updateChannelActivity(data.channel, true);
        }
        
        this.eventBus.emit('routing:channel_activity', data);
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
            // Charger routing traditionnel
            const routing = await this.backend.getRouting();
            this.updateRoutingFromBackend(routing);
            
            // Charger devices
            const devices = await this.backend.listDevices();
            this.updateDevicesFromBackend(devices);
            
            // âœ… NOUVEAU v4.2.1: Charger toutes les routes
            await this.listRoutesAPI();
            
            // âœ… NOUVEAU v4.2.1: Charger stats
            try {
                await this.getRoutingStatsAPI();
            } catch (error) {
                // Stats non critique
                this.logDebug('routing', 'Stats not available');
            }
            
            this.localState.lastSync = Date.now();
            this.logDebug('routing', 'âœ“ Routing configuration loaded from backend');
            
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
                    case 'pan':
                        await this.backend.setChannelPan(change.channel, change.pan);
                        break;
                    // Transformations
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
    // âœ… NOUVEAU v4.2.1: API ROUTES (ENABLE/DISABLE)
    // ========================================================================
    
    /**
     * Ajoute une route MIDI
     * @param {string} sourceId - ID pÃ©riphÃ©rique source
     * @param {string} destinationId - ID pÃ©riphÃ©rique destination
     * @returns {Promise<boolean>}
     */
    async addRouteAPI(sourceId, destinationId) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('routing.addRoute', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (response.success) {
                // Ajouter au cache local
                const routeKey = this.getRouteKey(sourceId, destinationId);
                this.routes.set(routeKey, {
                    source_id: sourceId,
                    destination_id: destinationId,
                    enabled: true,
                    created_at: Date.now()
                });
                
                this.updateRouteStats();
                
                this.logDebug('routing', `âœ“ Route added: ${sourceId} â†’ ${destinationId}`);
                
                this.eventBus.emit('routing:route_added', { sourceId, destinationId });
                this.showNotification(`Route created: ${sourceId} â†’ ${destinationId}`, 'success');
                
                this.updateView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Add route failed');
            }
        } catch (error) {
            this.logDebug('routing', 'addRouteAPI failed:', error);
            this.showNotification(`Failed to add route: ${error.message}`, 'error');
            throw error;
        }
    }

    /**
     * Retire une route MIDI
     */
    async removeRouteAPI(sourceId, destinationId) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('routing.removeRoute', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (response.success) {
                const routeKey = this.getRouteKey(sourceId, destinationId);
                this.routes.delete(routeKey);
                
                this.updateRouteStats();
                
                this.logDebug('routing', `âœ“ Route removed: ${sourceId} â†’ ${destinationId}`);
                
                this.eventBus.emit('routing:route_removed', { sourceId, destinationId });
                this.showNotification(`Route deleted: ${sourceId} â†’ ${destinationId}`, 'info');
                
                this.updateView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Remove route failed');
            }
        } catch (error) {
            this.logDebug('routing', 'removeRouteAPI failed:', error);
            this.showNotification(`Failed to remove route: ${error.message}`, 'error');
            throw error;
        }
    }

    /**
     * Active une route (sans la supprimer)
     */
    async enableRoute(sourceId, destinationId) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('routing.enableRoute', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (response.success) {
                const routeKey = this.getRouteKey(sourceId, destinationId);
                const route = this.routes.get(routeKey);
                
                if (route) {
                    route.enabled = true;
                    this.routes.set(routeKey, route);
                }
                
                this.updateRouteStats();
                
                this.logDebug('routing', `âœ“ Route enabled: ${sourceId} â†’ ${destinationId}`);
                
                this.eventBus.emit('routing:route_enabled', { sourceId, destinationId });
                this.showNotification(`Route enabled: ${sourceId} â†’ ${destinationId}`, 'success');
                
                this.updateView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Enable route failed');
            }
        } catch (error) {
            this.logDebug('routing', 'enableRoute failed:', error);
            this.showNotification(`Failed to enable route: ${error.message}`, 'error');
            throw error;
        }
    }

    /**
     * DÃ©sactive une route (sans la supprimer)
     */
    async disableRoute(sourceId, destinationId) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('routing.disableRoute', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (response.success) {
                const routeKey = this.getRouteKey(sourceId, destinationId);
                const route = this.routes.get(routeKey);
                
                if (route) {
                    route.enabled = false;
                    this.routes.set(routeKey, route);
                }
                
                this.updateRouteStats();
                
                this.logDebug('routing', `âœ“ Route disabled: ${sourceId} â†’ ${destinationId}`);
                
                this.eventBus.emit('routing:route_disabled', { sourceId, destinationId });
                this.showNotification(`Route disabled: ${sourceId} â†’ ${destinationId}`, 'info');
                
                this.updateView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Disable route failed');
            }
        } catch (error) {
            this.logDebug('routing', 'disableRoute failed:', error);
            this.showNotification(`Failed to disable route: ${error.message}`, 'error');
            throw error;
        }
    }

    /**
     * Toggle l'Ã©tat enabled/disabled d'une route
     */
    async toggleRoute(sourceId, destinationId) {
        const routeKey = this.getRouteKey(sourceId, destinationId);
        const route = this.routes.get(routeKey);
        
        if (!route) {
            throw new Error('Route not found');
        }
        
        if (route.enabled) {
            return await this.disableRoute(sourceId, destinationId);
        } else {
            return await this.enableRoute(sourceId, destinationId);
        }
    }

    /**
     * Efface toutes les routes
     */
    async clearRoutesAPI() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('routing.clearRoutes', {});
            
            if (response.success) {
                this.routes.clear();
                this.updateRouteStats();
                
                this.logDebug('routing', 'âœ“ All routes cleared');
                
                this.eventBus.emit('routing:routes_cleared');
                this.showNotification('All routes cleared', 'success');
                
                this.updateView();
                
                return true;
            } else {
                throw new Error(response.error_message || 'Clear routes failed');
            }
        } catch (error) {
            this.logDebug('routing', 'clearRoutesAPI failed:', error);
            this.showNotification(`Failed to clear routes: ${error.message}`, 'error');
            throw error;
        }
    }

    /**
     * Liste toutes les routes
     */
    async listRoutesAPI() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('routing.listRoutes', {});
            
            if (response.success) {
                const routes = response.data?.routes || [];
                
                this.routes.clear();
                routes.forEach(route => {
                    const routeKey = this.getRouteKey(route.source_id, route.destination_id);
                    this.routes.set(routeKey, {
                        ...route,
                        enabled: route.enabled !== false
                    });
                });
                
                this.updateRouteStats();
                
                this.logDebug('routing', `âœ“ Listed ${routes.length} routes`);
                
                return routes;
            } else {
                throw new Error(response.error_message || 'List routes failed');
            }
        } catch (error) {
            this.logDebug('routing', 'listRoutesAPI failed:', error);
            throw error;
        }
    }

    /**
     * Obtient les statistiques de routing
     */
    async getRoutingStatsAPI() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('routing.getStats', {});
            
            if (response.success) {
                const stats = response.data;
                
                this.routeStats = {
                    ...this.routeStats,
                    ...stats,
                    lastUpdate: Date.now()
                };
                
                return stats;
            } else {
                throw new Error(response.error_message || 'Get routing stats failed');
            }
        } catch (error) {
            this.logDebug('routing', 'getRoutingStatsAPI failed:', error);
            throw error;
        }
    }

    // ========================================================================
    // ROUTE HELPERS
    // ========================================================================

    getRouteKey(sourceId, destinationId) {
        return `${sourceId}:${destinationId}`;
    }

    parseRouteKey(routeKey) {
        const [sourceId, destinationId] = routeKey.split(':');
        return { sourceId, destinationId };
    }

    hasRoute(sourceId, destinationId) {
        const routeKey = this.getRouteKey(sourceId, destinationId);
        return this.routes.has(routeKey);
    }

    getRoute(sourceId, destinationId) {
        const routeKey = this.getRouteKey(sourceId, destinationId);
        return this.routes.get(routeKey) || null;
    }

    isRouteEnabled(sourceId, destinationId) {
        const route = this.getRoute(sourceId, destinationId);
        return route ? route.enabled !== false : false;
    }

    getAllRoutes() {
        return Array.from(this.routes.values());
    }

    getEnabledRoutes() {
        return Array.from(this.routes.values()).filter(route => route.enabled !== false);
    }

    getDisabledRoutes() {
        return Array.from(this.routes.values()).filter(route => route.enabled === false);
    }

    getRoutesFromSource(sourceId) {
        return Array.from(this.routes.values()).filter(route => 
            route.source_id === sourceId
        );
    }

    getRoutesToDestination(destinationId) {
        return Array.from(this.routes.values()).filter(route => 
            route.destination_id === destinationId
        );
    }

    updateRouteStats() {
        this.routeStats.total = this.routes.size;
        this.routeStats.enabled = this.getEnabledRoutes().length;
        this.routeStats.disabled = this.getDisabledRoutes().length;
        this.routeStats.lastUpdate = Date.now();
    }

    selectRoute(sourceId, destinationId) {
        this.selectedRoute = { sourceId, destinationId };
        this.updateView();
    }

    deselectRoute() {
        this.selectedRoute = null;
        this.updateView();
    }

    // ========================================================================
    // ACTIONS EN MASSE
    // ========================================================================

    async enableAllRoutes() {
        const disabledRoutes = this.getDisabledRoutes();
        let count = 0;

        for (const route of disabledRoutes) {
            try {
                await this.enableRoute(route.source_id, route.destination_id);
                count++;
            } catch (error) {
                this.logDebug('routing', 
                    `Failed to enable route ${route.source_id} â†’ ${route.destination_id}`);
            }
        }

        if (count > 0) {
            this.showNotification(`Enabled ${count} routes`, 'success');
        }

        return count;
    }

    async disableAllRoutes() {
        const enabledRoutes = this.getEnabledRoutes();
        let count = 0;

        for (const route of enabledRoutes) {
            try {
                await this.disableRoute(route.source_id, route.destination_id);
                count++;
            } catch (error) {
                this.logDebug('routing', 
                    `Failed to disable route ${route.source_id} â†’ ${route.destination_id}`);
            }
        }

        if (count > 0) {
            this.showNotification(`Disabled ${count} routes`, 'info');
        }

        return count;
    }
    
    // ========================================================================
    // ACTIONS CANAUX - âœ… AVEC VALIDATION
    // ========================================================================
    
    /**
     * Assigner un canal Ã  un device
     * âœ… CORRIGÃ‰: Avec validation et transformations
     */
    async assignChannelToDevice(channelNumber, deviceId, config = {}) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        // Construire la configuration complÃ¨te
        const routing = {
            channel: channelNumber,
            device: deviceId,
            ...config
        };
        
        // Valider AVANT d'assigner
        if (this.config.validateBeforeAssign) {
            const validation = model.validateRouting(routing);
            
            if (!validation.valid) {
                const errors = validation.errors.join(', ');
                this.showError(`Invalid routing: ${errors}`);
                this.logDebug('routing', `Validation failed: ${errors}`);
                return false;
            }
        }
        
        // Mise Ã  jour locale immÃ©diate
        model.assignChannelToDevice(channelNumber, deviceId);
        
        // Appliquer les transformations si configurÃ©es
        if (config.transpose !== undefined) {
            model.setChannelTranspose(channelNumber, config.transpose);
        }
        
        if (config.velocity) {
            model.setVelocityMapping(channelNumber, config.velocity);
        }
        
        if (config.noteFilter) {
            model.setNoteFilter(channelNumber, config.noteFilter);
        }
        
        // Ajouter Ã  la file des changements
        this.localState.pendingChanges.push({
            type: 'assign',
            channel: channelNumber,
            device: deviceId
        });
        
        // Activer les transformations dans le flux
        this.eventBus.emit('routing:assigned', {
            channel: channelNumber,
            device: deviceId,
            routing: routing,
            applyTransformations: this.config.applyTransformations
        });
        
        // Envoyer au backend si connectÃ©
        if (this.backend && this.backend.isConnected()) {
            try {
                await this.backend.setChannelRouting(channelNumber, deviceId);
                this.logDebug('routing', `âœ“ Channel ${channelNumber} assigned to ${deviceId}`);
            } catch (error) {
                this.logDebug('routing', 'Error assigning channel:', error);
                this.showNotification('Erreur lors de l\'assignation', 'error');
            }
        }
        
        return true;
    }
    
    /**
     * Muter/DÃ©muter un canal
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
    // TRANSFORMATIONS AVANCÃ‰ES
    // ========================================================================
    
    /**
     * Configure le velocity mapping d'un canal
     */
    setVelocityMapping(channelNumber, config) {
        const model = this.getModel('routing');
        if (!model) return false;
        
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
     */
    setNoteFilter(channelNumber, config) {
        const model = this.getModel('routing');
        if (!model) return false;
        
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
     */
    setNoteRemap(channelNumber, mappings) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        const success = model.setNoteRemap(channelNumber, mappings);
        
        if (success) {
            this.showSuccess(`${mappings.length} note mapping${mappings.length > 1 ? 's' : ''} applied`);
            
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
     */
    setCCRemap(channelNumber, mappings) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        const success = model.setCCRemap(channelNumber, mappings);
        
        if (success) {
            this.showSuccess(`${mappings.length} CC mapping${mappings.length > 1 ? 's' : ''} applied`);
            
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
     * DÃ©sactive toutes les transformations d'un canal
     */
    disableAllTransformations(channelNumber) {
        const model = this.getModel('routing');
        if (!model) return false;
        
        const channel = model.getChannel(channelNumber);
        if (!channel) return false;
        
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
        
        this.showNotification('Tous les canaux ont Ã©tÃ© mutÃ©s', 'info');
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
        
        this.showNotification('Tous les canaux ont Ã©tÃ© dÃ©mutÃ©s', 'info');
    }
    
    async resetAll() {
        if (this.config.confirmReset) {
            const confirmed = await this.confirmAction(
                'ÃŠtes-vous sÃ»r de vouloir rÃ©initialiser tout le routage ?',
                'RÃ©initialisation'
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
        
        this.showNotification('Routage rÃ©initialisÃ©', 'success');
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
            this.showNotification(`Preset "${name}" enregistrÃ©`, 'success');
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
            this.showNotification(`Preset "${preset.name}" chargÃ©`, 'success');
            
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
        this.showNotification(`Preset "${preset.name}" supprimÃ©`, 'success');
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
    // MISE Ã€ JOUR DE LA VUE
    // ========================================================================
    
    updateView() {
        const view = this.getView('routing');
        const model = this.getModel('routing');
        
        if (view && model) {
            const data = model.getRoutingConfiguration();
            
            // âœ… Ajouter les nouvelles donnÃ©es v4.2.1
            data.routes = Array.from(this.routes.values());
            data.routeStats = this.routeStats;
            data.selectedRoute = this.selectedRoute;
            data.backendConnected = this.backend?.isConnected() || false;
            data.autoSyncEnabled = this.autoSyncEnabled;
            data.pendingChanges = this.localState.pendingChanges.length;
            
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
    // Ã‰VÃ‰NEMENTS DU MODÃˆLE
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
    // Ã‰VÃ‰NEMENTS UI
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
    // Ã‰VÃ‰NEMENTS PAGE
    // ========================================================================
    
    onRoutingPageActive() {
        this.logDebug('routing', 'Routing page active');
        
        // Synchroniser immÃ©diatement
        this.syncWithBackend();
        
        // DÃ©marrer auto-sync
        if (this.autoSyncEnabled) {
            this.setupAutoSync();
        }
    }
    
    onRoutingPageInactive() {
        this.logDebug('routing', 'Routing page inactive');
        
        // ArrÃªter auto-sync pour Ã©conomiser ressources
        this.stopAutoSync();
    }
    
    // ========================================================================
    // MÃ‰THODES D'AIDE UI
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
    
    showNotification(message, type = 'info') {
        if (this.notifications && typeof this.notifications.show === 'function') {
            this.notifications.show(message, type);
        } else {
            this.eventBus.emit('notification:show', { message, type });
        }
    }
    
    showSuccess(message) {
        this.showNotification(message, 'success');
    }
    
    showError(message) {
        this.showNotification(message, 'error');
    }
    
    showWarning(message) {
        this.showNotification(message, 'warning');
    }
    
    // ========================================================================
    // API PUBLIQUE - CONFORMITÃ‰ RÃ‰FÃ‰RENCE
    // ========================================================================
    
    /**
     * Charge la configuration de routage
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
     * Assigne un canal Ã  un pÃ©riphÃ©rique (alias)
     */
    async assignChannel(channelId, deviceId) {
        return await this.assignChannelToDevice(channelId, deviceId);
    }
    
    /**
     * Teste une route en envoyant une note test
     */
    async testRoute(routeId) {
        try {
            const [channel, device] = routeId.split('-');
            
            if (!channel || !device) {
                this.showError('Invalid route ID');
                return false;
            }
            
            if (this.backend && this.backend.isConnected()) {
                await this.backend.sendCommand('midi.test_note', {
                    channel: parseInt(channel),
                    device: device,
                    note: 60,  // Middle C
                    velocity: 100,
                    duration: 500
                });
                
                this.showNotification(`Testing route: Channel ${channel} â†’ ${device}`, 'info');
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
    
    /**
     * CrÃ©e une nouvelle route
     */
    async createRoute(channel, deviceId, options = {}) {
        return await this.assignChannel(channel, deviceId, options);
    }
    
    /**
     * Supprime une route
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
     * Met Ã  jour une route existante
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
     * Obtient toutes les routes configurÃ©es
     */
    getRoutes() {
        const model = this.getModel('routing');
        if (!model) return [];
        
        return model.getAllRoutes();
    }
    
    /**
     * Auto-routage automatique des canaux aux pÃ©riphÃ©riques
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
            this.logDebug('routing', `âœ… Auto-route complete: ${routedCount} channels`);
            
            return true;
            
        } catch (error) {
            this.logDebug('error', 'Auto-route failed:', error);
            this.showError('Auto-route failed: ' + error.message);
            return false;
        }
    }
    
    /**
     * Efface tout le routage
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
            this.logDebug('routing', 'âœ… Routing cleared');
            
            return true;
            
        } catch (error) {
            this.logDebug('error', 'Clear routing failed:', error);
            this.showError('Failed to clear routing: ' + error.message);
            return false;
        }
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    getStats() {
        return {
            ...this.routeStats,
            autoSyncEnabled: this.autoSyncEnabled,
            syncInterval: this.config.syncInterval,
            pendingChanges: this.localState.pendingChanges.length,
            backendConnected: this.backend?.isConnected() || false,
            lastSync: this.localState.lastSync
        };
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        this.stopAutoSync();
        this.routes.clear();
        this.localState.pendingChanges = [];
        
        this.logDebug('routing', 'RoutingController destroyed');
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
// ============================================================================
// Fichier: frontend/js/controllers/RoutingController.js
// Chemin réel: frontend/js/controllers/RoutingController.js
// Version: v4.2.4 - API FULLY COMPLIANT
// Date: 2025-11-06
// ============================================================================
// CORRECTIONS v4.2.4:
// ✅ addRoute: destination_id (pas dest_id)
// ✅ removeRoute: (source_id, destination_id) pas route_id
// ✅ enableRoute: (source_id, destination_id) pas route_id
// ✅ disableRoute: (source_id, destination_id) pas route_id
// ✅ 100% conforme API Documentation v4.2.2
// ============================================================================

class RoutingController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        this.logger = window.logger || console;
        this.model = models.routing;
        this.view = views.routing;
        
        // Map: "source_id:destination_id" => route object
        this.routes = new Map();
        
        this.localState = {
            isInitialized: false,
            isSyncing: false,
            lastSync: 0
        };
        
        this._fullyInitialized = true;
        this.bindEvents();
    }
    
    bindEvents() {
        this.eventBus.on('routing:refresh', () => this.loadRoutes());
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('routing:add_route', (data) => this.addRoute(data));
        this.eventBus.on('routing:remove_route', (data) => this.removeRoute(data.source_id, data.destination_id));
        this.eventBus.on('routing:enable_route', (data) => this.enableRoute(data.source_id, data.destination_id));
        this.eventBus.on('routing:disable_route', (data) => this.disableRoute(data.source_id, data.destination_id));
        this.eventBus.on('routing:clear_all', () => this.clearAllRoutes());
    }
    
    async initialize() {
        if (!this.localState) {
            this.localState = {
                isInitialized: false,
                isSyncing: false,
                lastSync: 0
            };
        }
        
        this.logger?.info?.('RoutingController', 'Initializing v4.2.4 (API Compliant)...');
        
        if (this.backend?.isConnected()) {
            await this.loadRoutes();
        }
        
        this.localState.isInitialized = true;
    }
    
    async onBackendConnected() {
        await this.loadRoutes();
    }
    
    /**
     * ✅ Charge routes depuis backend
     */
    async loadRoutes() {
        if (!this.backend?.isConnected()) return;
        
        try {
            const response = await this.backend.listRoutes();
            const routes = response.routes || [];
            
            this.routes.clear();
            routes.forEach(route => {
                const key = `${route.source_id}:${route.destination_id}`;
                this.routes.set(key, route);
            });
            
            this.eventBus.emit('routing:routes_loaded', { routes });
            this.refreshView();
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'Failed to load routes:', error);
        }
    }
    
    /**
     * ✅ CORRIGÉ: Ajoute route avec source_id, destination_id
     */
    async addRoute(params) {
        const { source_id, destination_id } = params;
        
        if (!source_id || !destination_id) {
            throw new Error('source_id and destination_id are required');
        }
        
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            // ✅ API v4.2.2: destination_id (pas dest_id)
            const response = await this.backend.addRoute(source_id, destination_id);
            
            await this.loadRoutes();
            
            this.notifications?.success('Route added', `Route ${source_id} → ${destination_id}`);
            
            return response;
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'addRoute failed:', error);
            this.notifications?.error('Add route failed', error.message);
            throw error;
        }
    }
    
    /**
     * ✅ CORRIGÉ: Utilise (source_id, destination_id) pas route_id
     */
    async removeRoute(source_id, destination_id) {
        if (!source_id || !destination_id) {
            throw new Error('source_id and destination_id are required');
        }
        
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            // ✅ API v4.2.2: (source_id, destination_id)
            await this.backend.removeRoute(source_id, destination_id);
            
            await this.loadRoutes();
            
            this.notifications?.success('Route removed', `Route ${source_id} → ${destination_id} deleted`);
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'removeRoute failed:', error);
            this.notifications?.error('Remove route failed', error.message);
            throw error;
        }
    }
    
    /**
     * ✅ CORRIGÉ: Active route avec (source_id, destination_id)
     */
    async enableRoute(source_id, destination_id) {
        if (!source_id || !destination_id) {
            throw new Error('source_id and destination_id are required');
        }
        
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            // ✅ API v4.2.2: (source_id, destination_id)
            await this.backend.enableRoute(source_id, destination_id);
            
            await this.loadRoutes();
            
            this.notifications?.success('Route enabled', `Route ${source_id} → ${destination_id} enabled`);
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'enableRoute failed:', error);
            this.notifications?.error('Enable route failed', error.message);
            throw error;
        }
    }
    
    /**
     * ✅ CORRIGÉ: Désactive route avec (source_id, destination_id)
     */
    async disableRoute(source_id, destination_id) {
        if (!source_id || !destination_id) {
            throw new Error('source_id and destination_id are required');
        }
        
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            // ✅ API v4.2.2: (source_id, destination_id)
            await this.backend.disableRoute(source_id, destination_id);
            
            await this.loadRoutes();
            
            this.notifications?.success('Route disabled', `Route ${source_id} → ${destination_id} disabled`);
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'disableRoute failed:', error);
            this.notifications?.error('Disable route failed', error.message);
            throw error;
        }
    }
    
    async clearAllRoutes() {
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            await this.backend.clearRoutes();
            
            this.routes.clear();
            
            this.notifications?.success('Routes cleared', 'All routes removed');
            
            this.refreshView();
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'clearAllRoutes failed:', error);
            this.notifications?.error('Clear routes failed', error.message);
            throw error;
        }
    }
    
    /**
     * Helper: Trouve une route par IDs
     */
    findRoute(source_id, destination_id) {
        const key = `${source_id}:${destination_id}`;
        return this.routes.get(key);
    }
    
    /**
     * Retourne toutes les routes
     */
    getRoutes() {
        return Array.from(this.routes.values());
    }
    
    /**
     * Retourne les routes d'une source
     */
    getRoutesBySource(source_id) {
        return this.getRoutes().filter(r => r.source_id === source_id);
    }
    
    /**
     * Retourne les routes vers une destination
     */
    getRoutesByDestination(destination_id) {
        return this.getRoutes().filter(r => r.destination_id === destination_id);
    }
    
    /**
     * Vérifie si une route existe
     */
    routeExists(source_id, destination_id) {
        const key = `${source_id}:${destination_id}`;
        return this.routes.has(key);
    }
    
    refreshView() {
        if (!this.view) return;
        
        this.view.render({
            routes: this.getRoutes()
        });
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RoutingController;
}

if (typeof window !== 'undefined') {
    window.RoutingController = RoutingController;
}
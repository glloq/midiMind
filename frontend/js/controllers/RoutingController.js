// ============================================================================
// Fichier: frontend/js/controllers/RoutingController.js
// Chemin réel: frontend/js/controllers/RoutingController.js
// Version: v4.2.3 - FIXED BACKEND SIGNATURE - API CORRECTED (CRITICAL FIXES)
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v4.2.3:
// âœ… CRITIQUE: Ajout paramètre backend au constructeur (6ème paramètre)
// âœ… Fix: super() appelle BaseController avec backend
// âœ… this.backend initialisé automatiquement via BaseController
// ============================================================================
// ============================================================================
// CORRECTIONS v4.2.2 CRITIQUES:
// ✓ routing_id (pas id)
// ✓ source_id, dest_id, device_id (snake_case)
// ✓ route_id pour enable/disable
// 
// NOTE: Fichier simplifié - Contient uniquement les corrections critiques API.
// Les fonctionnalités avancées (transformations, presets) nécessitent adaptation complète.
// ============================================================================

class RoutingController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        // âœ… this.backend initialisé automatiquement par BaseController
        this.logger = window.logger || console;
        this.model = models.routing;
        this.view = views.routing;
        
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
        this.eventBus.on('routing:remove_route', (data) => this.removeRoute(data.route_id));
        this.eventBus.on('routing:enable_route', (data) => this.enableRoute(data.route_id));
        this.eventBus.on('routing:disable_route', (data) => this.disableRoute(data.route_id));
        this.eventBus.on('routing:clear_all', () => this.clearAllRoutes());
    }
    
    async initialize() {
        // ✓ FIX: Initialiser localState si appelé avant fin du constructor
        if (!this.localState) {
            this.localState = {
                isInitialized: false,
                isSyncing: false,
                lastSync: 0
            };
        }
        
        this.logger?.info?.('RoutingController', 'Initializing v4.2.2...');
        
        if (this.backend?.isConnected()) {
            await this.loadRoutes();
        }
        
        this.localState.isInitialized = true;
    }
    
    async onBackendConnected() {
        await this.loadRoutes();
    }
    
    /**
     * ✓ CORRECTION: Charge routes depuis backend
     */
    async loadRoutes() {
        if (!this.backend?.isConnected()) return;
        
        try {
            const response = await this.backend.listRoutes();
            const routes = response.routes || [];
            
            this.routes.clear();
            routes.forEach(route => {
                const key = `${route.source_id}:${route.dest_id}`;
                this.routes.set(key, route);
            });
            
            this.eventBus.emit('routing:routes_loaded', { routes });
            this.refreshView();
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'Failed to load routes:', error);
        }
    }
    
    /**
     * ✓ CORRECTION: Ajoute route avec source_id, dest_id
     */
    async addRoute(params) {
        const { source_id, dest_id, filters = {} } = params;
        
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            const response = await this.backend.addRoute(source_id, dest_id, filters);
            
            await this.loadRoutes();
            
            this.notifications?.success('Route added', `Route ${source_id} → ${dest_id}`);
            
            return response;
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'addRoute failed:', error);
            this.notifications?.error('Add route failed', error.message);
            throw error;
        }
    }
    
    /**
     * ✓ CORRECTION: Utilise route_id
     */
    async removeRoute(route_id) {
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            await this.backend.removeRoute(route_id);
            
            await this.loadRoutes();
            
            this.notifications?.success('Route removed', `Route ${route_id} deleted`);
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'removeRoute failed:', error);
            this.notifications?.error('Remove route failed', error.message);
            throw error;
        }
    }
    
    /**
     * ✓ CORRECTION: Active route avec route_id
     */
    async enableRoute(route_id) {
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            await this.backend.enableRoute(route_id);
            
            await this.loadRoutes();
            
            this.notifications?.success('Route enabled', `Route ${route_id} enabled`);
            
        } catch (error) {
            this.logger?.error?.('RoutingController', 'enableRoute failed:', error);
            this.notifications?.error('Enable route failed', error.message);
            throw error;
        }
    }
    
    /**
     * ✓ CORRECTION: Désactive route avec route_id
     */
    async disableRoute(route_id) {
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            await this.backend.disableRoute(route_id);
            
            await this.loadRoutes();
            
            this.notifications?.success('Route disabled', `Route ${route_id} disabled`);
            
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
    
    getRoutes() {
        return Array.from(this.routes.values());
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
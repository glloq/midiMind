// ============================================================================
// Fichier: frontend/js/models/RoutingModel.js
// Chemin rÃ©el: frontend/js/models/RoutingModel.js
// Version: v3.3.0 - API FORMAT SIMPLIFIÃ‰
// Date: 2025-11-01
// ============================================================================
// MODIFICATIONS v3.3.0:
// âœ… Utilisation sendCommand() au lieu de send()
// âœ… Format API simplifiÃ© (id, command, params)
// âœ… Signature constructeur cohÃ©rente avec BaseModel
// ============================================================================

class RoutingModel extends BaseModel {
    constructor(eventBus, backend, logger, initialData = {}, options = {}) {
        super(eventBus, backend, logger, initialData, {
            persistKey: 'routingmodel',
            eventPrefix: 'routing',
            autoPersist: false,
            ...options
        });
        
        // Initialiser les routes
        this.data.routes = initialData.routes || [];
        
        this.log('debug', 'RoutingModel', 'Initialized v3.3.0');
    }
    
    /**
     * Liste toutes les routes configurÃ©es
     */
    async listRoutes() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'RoutingModel.listRoutes', 'Backend not connected');
            return [];
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('get_routing');
            
            if (data && data.routes) {
                this.data.routes = data.routes;
                this.emit('routes:updated', { routes: this.data.routes });
                return this.data.routes;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.listRoutes', error.message);
        }
        
        return [];
    }
    
    /**
     * Ajoute une nouvelle route
     * @param {string} sourceId - ID du pÃ©riphÃ©rique source
     * @param {string} destinationId - ID du pÃ©riphÃ©rique destination
     */
    async addRoute(sourceId, destinationId) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'RoutingModel.addRoute', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('add_route', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (data) {
                // Recharger la liste des routes
                await this.listRoutes();
                this.emit('route:added', { sourceId, destinationId });
                return true;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.addRoute', error.message);
        }
        
        return false;
    }
    
    /**
     * Supprime une route existante
     * @param {string} sourceId - ID du pÃ©riphÃ©rique source
     * @param {string} destinationId - ID du pÃ©riphÃ©rique destination
     */
    async removeRoute(sourceId, destinationId) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'RoutingModel.removeRoute', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('remove_route', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (data) {
                // Recharger la liste des routes
                await this.listRoutes();
                this.emit('route:removed', { sourceId, destinationId });
                return true;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.removeRoute', error.message);
        }
        
        return false;
    }
    
    /**
     * Efface toutes les routes
     */
    async clearRoutes() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'RoutingModel.clearRoutes', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('clear_routes');
            
            if (data) {
                this.data.routes = [];
                this.emit('routes:cleared');
                return true;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.clearRoutes', error.message);
        }
        
        return false;
    }
    
    /**
     * Active une route
     * @param {string} sourceId - ID du pÃ©riphÃ©rique source
     * @param {string} destinationId - ID du pÃ©riphÃ©rique destination
     */
    async enableRoute(sourceId, destinationId) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'RoutingModel.enableRoute', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('enable_route', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (data) {
                await this.listRoutes();
                this.emit('route:enabled', { sourceId, destinationId });
                return true;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.enableRoute', error.message);
        }
        
        return false;
    }
    
    /**
     * DÃ©sactive une route
     * @param {string} sourceId - ID du pÃ©riphÃ©rique source
     * @param {string} destinationId - ID du pÃ©riphÃ©rique destination
     */
    async disableRoute(sourceId, destinationId) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'RoutingModel.disableRoute', 'Backend not connected');
            return false;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('disable_route', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (data) {
                await this.listRoutes();
                this.emit('route:disabled', { sourceId, destinationId });
                return true;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.disableRoute', error.message);
        }
        
        return false;
    }
    
    /**
     * Obtient les statistiques de routage
     */
    async getStats() {
        if (!this.backend || !this.backend.isConnected()) {
            return null;
        }
        
        try {
            // âœ… Nouveau format API simplifiÃ©
            const data = await this.backend.sendCommand('get_routing');
            return data;
        } catch (error) {
            this.log('error', 'RoutingModel.getStats', error.message);
            return null;
        }
    }
    
    /**
     * Retourne toutes les routes (depuis le cache local)
     */
    getRoutes() {
        return this.data.routes || [];
    }
    
    /**
     * Trouve une route spÃ©cifique
     */
    findRoute(sourceId, destinationId) {
        return this.data.routes.find(
            route => route.source_id === sourceId && route.destination_id === destinationId
        );
    }
    
    /**
     * VÃ©rifie si une route existe
     */
    hasRoute(sourceId, destinationId) {
        return !!this.findRoute(sourceId, destinationId);
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
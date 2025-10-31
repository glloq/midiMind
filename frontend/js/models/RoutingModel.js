// ============================================================================
// Fichier: frontend/js/models/RoutingModel.js
// Chemin réel: frontend/js/models/RoutingModel.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE
// Date: 2025-10-31
// ============================================================================

class RoutingModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, backend, logger, {}, {
            persistKey: 'routingmodel',
            eventPrefix: 'routing',
            autoPersist: false
        });
        
        this.data.routes = [];
        
        this.log('debug', 'RoutingModel', 'Initialized');
    }
    
    async listRoutes() {
        if (!this.backend || !this.backend.isConnected()) return [];
        
        try {
            const response = await this.backend.send('routing.listRoutes', {});
            if (response.success && response.data.routes) {
                this.data.routes = response.data.routes;
                this.emit('routes:updated', { routes: this.data.routes });
                return this.data.routes;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.listRoutes', error);
        }
        
        return [];
    }
    
    async addRoute(sourceId, destinationId) {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            const response = await this.backend.send('routing.addRoute', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (response.success) {
                await this.listRoutes();
                this.emit('route:added', { sourceId, destinationId });
                return true;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.addRoute', error);
        }
        
        return false;
    }
    
    async removeRoute(sourceId, destinationId) {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            const response = await this.backend.send('routing.removeRoute', {
                source_id: sourceId,
                destination_id: destinationId
            });
            
            if (response.success) {
                await this.listRoutes();
                this.emit('route:removed', { sourceId, destinationId });
                return true;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.removeRoute', error);
        }
        
        return false;
    }
    
    async clearRoutes() {
        if (!this.backend || !this.backend.isConnected()) return false;
        
        try {
            const response = await this.backend.send('routing.clearRoutes', {});
            
            if (response.success) {
                this.data.routes = [];
                this.emit('routes:cleared');
                return true;
            }
        } catch (error) {
            this.log('error', 'RoutingModel.clearRoutes', error);
        }
        
        return false;
    }
    
    getRoutes() {
        return this.data.routes;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RoutingModel;
}

if (typeof window !== 'undefined') {
    window.RoutingModel = RoutingModel;
}
// ============================================================================
// Fichier: frontend/js/models/StateModel.js
// Chemin réel: frontend/js/models/StateModel.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE
// Date: 2025-10-31
// ============================================================================

class StateModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, backend, logger, {}, {
            persistKey: 'statemodel',
            eventPrefix: 'state',
            autoPersist: true
        });
        
        this.data.currentPage = 'home';
        this.data.backendConnected = false;
        this.data.theme = 'dark';
        this.data.performanceMode = true;
        
        this.log('debug', 'StateModel', 'Initialized');
    }
    
    setCurrentPage(page) {
        this.data.currentPage = page;
        this.emit('page:changed', { page });
    }
    
    getCurrentPage() {
        return this.data.currentPage;
    }
    
    setBackendConnected(connected) {
        this.data.backendConnected = connected;
        this.emit('backend:status', { connected });
    }
    
    isBackendConnected() {
        return this.data.backendConnected;
    }
    
    setTheme(theme) {
        this.data.theme = theme;
        this.emit('theme:changed', { theme });
    }
    
    getTheme() {
        return this.data.theme;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = StateModel;
}

if (typeof window !== 'undefined') {
    window.StateModel = StateModel;
}
// ============================================================================
// Fichier: frontend/js/models/StateModel.js
// Chemin réel: frontend/js/models/StateModel.js
// Version: v3.3.0 - SIGNATURE CORRIGÉE (5 PARAMÈTRES)
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.3.0:
// ✅ CRITIQUE: Ajout paramètres initialData et options manquants
// ✅ Signature cohérente: (eventBus, backend, logger, initialData = {}, options = {})
// ✅ Merge intelligente des options par défaut
// ============================================================================

class StateModel extends BaseModel {
    constructor(eventBus, backend, logger, initialData = {}, options = {}) {
        // ✅ NOUVEAU: Appel super() avec les 5 paramètres
        super(eventBus, backend, logger, initialData, {
            persistKey: 'statemodel',
            eventPrefix: 'state',
            autoPersist: true,
            ...options  // Permet override des options par défaut
        });
        
        // Initialisation des données d'état avec valeurs par défaut
        this.data.currentPage = this.data.currentPage || 'home';
        this.data.backendConnected = this.data.backendConnected || false;
        this.data.theme = this.data.theme || 'dark';
        this.data.performanceMode = this.data.performanceMode !== undefined ? this.data.performanceMode : true;
        
        this.log('debug', 'StateModel', 'Initialized v3.3.0');
    }
    
    /**
     * Définit la page courante
     * @param {string} page - Nom de la page
     */
    setCurrentPage(page) {
        this.data.currentPage = page;
        this.emit('page:changed', { page });
    }
    
    /**
     * Récupère la page courante
     * @returns {string}
     */
    getCurrentPage() {
        return this.data.currentPage;
    }
    
    /**
     * Définit l'état de connexion au backend
     * @param {boolean} connected
     */
    setBackendConnected(connected) {
        this.data.backendConnected = connected;
        this.emit('backend:status', { connected });
    }
    
    /**
     * Vérifie si le backend est connecté
     * @returns {boolean}
     */
    isBackendConnected() {
        return this.data.backendConnected;
    }
    
    /**
     * Définit le thème de l'interface
     * @param {string} theme - 'dark' ou 'light'
     */
    setTheme(theme) {
        this.data.theme = theme;
        this.emit('theme:changed', { theme });
    }
    
    /**
     * Récupère le thème actuel
     * @returns {string}
     */
    getTheme() {
        return this.data.theme;
    }
    
    /**
     * Active/désactive le mode performance
     * @param {boolean} enabled
     */
    setPerformanceMode(enabled) {
        this.data.performanceMode = enabled;
        this.emit('performance:changed', { enabled });
    }
    
    /**
     * Vérifie si le mode performance est actif
     * @returns {boolean}
     */
    isPerformanceModeEnabled() {
        return this.data.performanceMode;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = StateModel;
}

if (typeof window !== 'undefined') {
    window.StateModel = StateModel;
}
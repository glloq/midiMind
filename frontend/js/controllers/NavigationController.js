// ============================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Chemin rÃƒÆ’Ã‚Â©el: frontend/js/controllers/NavigationController.js
// Version: v4.0.1 - FIXED BACKEND SIGNATURE - FIX PAGES NOT DISPLAYING
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v4.0.1:
// âœ… CRITIQUE: Ajout paramÃ¨tre backend au constructeur (6Ã¨me paramÃ¨tre)
// âœ… Fix: super() appelle BaseController avec backend
// âœ… this.backend initialisÃ© automatiquement via BaseController
// ============================================================================
// ============================================================================
// CORRECTIONS v4.0.0:
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ CRITIQUE: Affichage/masquage direct des ÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©ments DOM
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ CRITIQUE: Initialisation des vues quand la page est affichÃƒÆ’Ã‚Â©e
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ CRITIQUE: Suppression de la gÃƒÆ’Ã‚Â©nÃƒÆ’Ã‚Â©ration dynamique de contenu
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Fix: Toutes les pages s'affichent correctement
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Simplification: Navigation plus directe et fiable
// ============================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        // ÃƒÆ’Ã¢â‚¬Â°tat de navigation
        this.currentPage = 'home';
        this.previousPage = null;
        this.navigationHistory = ['home'];
        this.historyIndex = 0;
        
        // Configuration des pages
        this.pages = {
            home: {
                id: 'home-view',
                element: null,
                title: 'ÃƒÂ°Ã…Â¸Ã‚ÂÃ‚Â  Accueil',
                icon: 'ÃƒÂ°Ã…Â¸Ã‚ÂÃ‚Â ',
                shortcut: 'h',
                viewKey: 'home',
                initialized: false
            },
            files: {
                id: 'file-view',
                element: null,
                title: 'ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã‚Â Fichiers',
                icon: 'ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã‚Â',
                shortcut: 'f',
                viewKey: 'file',
                initialized: false
            },
            editor: {
                id: 'editor-view',
                element: null,
                title: 'ÃƒÂ¢Ã…â€œÃ‚ÂÃƒÂ¯Ã‚Â¸Ã‚Â ÃƒÆ’Ã¢â‚¬Â°diteur',
                icon: 'ÃƒÂ¢Ã…â€œÃ‚ÂÃƒÂ¯Ã‚Â¸Ã‚Â',
                shortcut: 'e',
                viewKey: 'editor',
                initialized: false
            },
            routing: {
                id: 'routing-view',
                element: null,
                title: 'ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ¢â€šÂ¬ Routage',
                icon: 'ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ¢â€šÂ¬',
                shortcut: 'r',
                viewKey: 'routing',
                initialized: false
            },
            keyboard: {
                id: 'keyboard-view',
                element: null,
                title: 'ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¹ Clavier',
                icon: 'ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¹',
                shortcut: 'k',
                viewKey: 'keyboard',
                initialized: false
            },
            instruments: {
                id: 'instruments-view',
                element: null,
                title: 'ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¸ Instruments',
                icon: 'ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¸',
                shortcut: 'i',
                viewKey: 'instrument',
                initialized: false
            },
            playlist: {
                id: 'playlist-view',
                element: null,
                title: 'ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã¢â‚¬Â¹ Playlist',
                icon: 'ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã¢â‚¬Â¹',
                shortcut: 'p',
                viewKey: 'playlist',
                initialized: false
            },
            system: {
                id: 'system-view',
                element: null,
                title: 'ÃƒÂ¢Ã…Â¡Ã¢â€žÂ¢ÃƒÂ¯Ã‚Â¸Ã‚Â SystÃƒÆ’Ã‚Â¨me',
                icon: 'ÃƒÂ¢Ã…Â¡Ã¢â€žÂ¢ÃƒÂ¯Ã‚Â¸Ã‚Â',
                shortcut: 's',
                viewKey: 'system',
                initialized: false
            },
            visualizer: {
                id: 'visualizer-view',
                element: null,
                title: 'ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã…Â  Visualiseur',
                icon: 'ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã…Â ',
                shortcut: 'v',
                viewKey: 'visualizer',
                initialized: false
            }
        };
        
        // Configuration des transitions
        this.transitionState = {
            inProgress: false,
            duration: 300
        };
        
        this.logDebug('info', 'NavigationController', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Initialized v4.0.0');
        
        this.initializeNavigation();
    }

    /**
     * Liaison des ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
     */
    bindEvents() {
        this.eventBus.on('navigation:page_request', (data) => {
            this.showPage(data.page, data.options);
        });
        
        this.eventBus.on('navigation:back', () => {
            this.goBack();
        });
        
        this.eventBus.on('navigation:forward', () => {
            this.goForward();
        });
    }

    /**
     * Initialisation
     */
    initializeNavigation() {
        // Cacher l'ÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©ment #app d'abord
        this.cachePageElements();
        this.setupKeyboardShortcuts();
        this.setupBrowserHistory();
        this.setupNavigationLinks();
        
        // Afficher la page initiale
        this.showPage('home', { skipHistory: true });
        
        this.logDebug('debug', 'NavigationController', 'Navigation system initialized');
    }

    /**
     * Cache les rÃƒÆ’Ã‚Â©fÃƒÆ’Ã‚Â©rences aux ÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©ments DOM des pages
     */
    cachePageElements() {
        Object.keys(this.pages).forEach(pageKey => {
            const pageConfig = this.pages[pageKey];
            pageConfig.element = document.getElementById(pageConfig.id);
            
            if (pageConfig.element) {
                // Masquer toutes les pages sauf home
                if (pageKey !== 'home') {
                    pageConfig.element.style.display = 'none';
                    pageConfig.element.classList.remove('active');
                }
            } else {
                this.logDebug('warn', 'NavigationController', `Page element not found: ${pageConfig.id}`);
            }
        });
    }

    /**
     * Configure les liens de navigation
     */
    setupNavigationLinks() {
        document.querySelectorAll('.nav-item').forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const page = link.getAttribute('data-page');
                if (page) {
                    this.showPage(page);
                }
            });
        });
    }

    /**
     * Configure les raccourcis clavier
     */
    setupKeyboardShortcuts() {
        document.addEventListener('keydown', (event) => {
            // Ignorer dans les champs de saisie
            if (event.target.tagName === 'INPUT' || event.target.tagName === 'TEXTAREA') {
                return;
            }
            
            if (event.altKey) {
                Object.entries(this.pages).forEach(([pageKey, pageConfig]) => {
                    if (event.key.toLowerCase() === pageConfig.shortcut) {
                        event.preventDefault();
                        this.showPage(pageKey);
                    }
                });
            }
        });
    }

    /**
     * Configure l'historique du navigateur
     */
    setupBrowserHistory() {
        window.addEventListener('popstate', (event) => {
            if (event.state && event.state.page) {
                this.showPage(event.state.page, { 
                    skipHistory: true, 
                    skipPushState: true 
                });
            }
        });
    }

    // ========================================================================
    // NAVIGATION
    // ========================================================================

    /**
     * Affiche une page
     */
    async showPage(pageKey, options = {}) {
        const {
            forceRefresh = false,
            skipHistory = false,
            skipPushState = false
        } = options;
        
        if (!this.pages[pageKey]) {
            this.logDebug('warn', 'NavigationController', `Page not found: ${pageKey}`);
            return false;
        }
        
        if (pageKey === this.currentPage && !forceRefresh) {
            this.logDebug('debug', 'NavigationController', `Page already active: ${pageKey}`);
            return true;
        }
        
        if (this.transitionState.inProgress) {
            this.logDebug('debug', 'NavigationController', 'Transition in progress');
            return false;
        }
        
        this.logDebug('debug', 'NavigationController', `Navigating to: ${pageKey}`);
        
        try {
            this.transitionState.inProgress = true;
            this.previousPage = this.currentPage;
            
            this.eventBus.emit('navigation:page_changing', {
                from: this.currentPage,
                to: pageKey
            });
            
            const success = await this.performPageTransition(pageKey, forceRefresh);
            
            if (success) {
                this.currentPage = pageKey;
                
                if (!skipHistory) {
                    this.addToHistory(pageKey);
                }
                
                if (!skipPushState) {
                    history.pushState({ page: pageKey }, '', `#${pageKey}`);
                }
                
                this.updateNavigationUI();
                
                this.eventBus.emit('navigation:page_changed', {
                    from: this.previousPage,
                    to: pageKey,
                    page: pageKey
                });
                
                this.logDebug('debug', 'NavigationController', `Navigation successful: ${this.previousPage} ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ ${pageKey}`);
                return true;
            }
            
        } catch (error) {
            this.handleError('Erreur navigation', error);
            return false;
            
        } finally {
            this.transitionState.inProgress = false;
        }
        
        return false;
    }

    /**
     * Effectue la transition de page
     */
    async performPageTransition(pageKey, forceRefresh) {
        const currentPageConfig = this.pages[this.currentPage];
        const targetPageConfig = this.pages[pageKey];
        
        const currentPageElement = currentPageConfig.element;
        const targetPageElement = targetPageConfig.element;
        
        if (!currentPageElement || !targetPageElement) {
            this.logDebug('error', 'NavigationController', 'Missing page elements', {
                currentId: currentPageConfig.id,
                targetId: targetPageConfig.id
            });
            return false;
        }
        
        try {
            // Masquer la page actuelle
            currentPageElement.classList.remove('active');
            currentPageElement.style.display = 'none';
            
            // Afficher la nouvelle page
            targetPageElement.style.display = 'block';
            targetPageElement.classList.add('active');
            
            // Initialiser la vue si ce n'est pas dÃƒÆ’Ã‚Â©jÃƒÆ’Ã‚Â  fait
            if (!targetPageConfig.initialized || forceRefresh) {
                await this.initializePageView(pageKey);
                targetPageConfig.initialized = true;
            }
            
            return true;
            
        } catch (error) {
            this.logDebug('error', 'NavigationController', 'Transition error', error);
            return false;
        }
    }

    /**
     * Initialise la vue d'une page
     */
    async initializePageView(pageKey) {
        const pageConfig = this.pages[pageKey];
        const view = this.getView(pageConfig.viewKey);
        
        if (!view) {
            this.logDebug('warn', 'NavigationController', `View not found: ${pageConfig.viewKey}`);
            return;
        }
        
        try {
            // Si la vue a une mÃƒÆ’Ã‚Â©thode init, l'appeler
            if (typeof view.init === 'function') {
                await view.init();
                this.logDebug('debug', 'NavigationController', `View initialized: ${pageConfig.viewKey}`);
            }
            
            // Si la vue a une mÃƒÆ’Ã‚Â©thode render, l'appeler
            if (typeof view.render === 'function') {
                await view.render();
                this.logDebug('debug', 'NavigationController', `View rendered: ${pageConfig.viewKey}`);
            }
            
        } catch (error) {
            this.logDebug('error', 'NavigationController', `View initialization error: ${pageConfig.viewKey}`, error);
        }
    }

    // ========================================================================
    // HISTORIQUE
    // ========================================================================

    /**
     * Ajoute ÃƒÆ’Ã‚Â  l'historique
     */
    addToHistory(pageKey) {
        // Si on n'est pas ÃƒÆ’Ã‚Â  la fin de l'historique, supprimer tout ce qui suit
        if (this.historyIndex < this.navigationHistory.length - 1) {
            this.navigationHistory = this.navigationHistory.slice(0, this.historyIndex + 1);
        }
        
        this.navigationHistory.push(pageKey);
        this.historyIndex = this.navigationHistory.length - 1;
        
        // Limiter la taille de l'historique
        if (this.navigationHistory.length > 50) {
            this.navigationHistory.shift();
            this.historyIndex--;
        }
    }

    /**
     * Retour en arriÃƒÆ’Ã‚Â¨re
     */
    async goBack() {
        if (this.historyIndex > 0) {
            this.historyIndex--;
            const pageKey = this.navigationHistory[this.historyIndex];
            await this.showPage(pageKey, { skipHistory: true });
        }
    }

    /**
     * Avance
     */
    async goForward() {
        if (this.historyIndex < this.navigationHistory.length - 1) {
            this.historyIndex++;
            const pageKey = this.navigationHistory[this.historyIndex];
            await this.showPage(pageKey, { skipHistory: true });
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    /**
     * Met ÃƒÆ’Ã‚Â  jour l'UI de navigation
     */
    updateNavigationUI() {
        document.querySelectorAll('.nav-item').forEach(link => {
            const page = link.getAttribute('data-page');
            if (page === this.currentPage) {
                link.classList.add('active');
            } else {
                link.classList.remove('active');
            }
        });
    }

    /**
     * RafraÃƒÆ’Ã‚Â®chit la page courante
     */
    async refreshCurrentPage() {
        await this.showPage(this.currentPage, { forceRefresh: true });
    }

    /**
     * Obtient la page courante
     */
    getCurrentPage() {
        return this.currentPage;
    }

    /**
     * Obtient l'ÃƒÆ’Ã‚Â©tat de navigation
     */
    getNavigationState() {
        return {
            currentPage: this.currentPage,
            previousPage: this.previousPage,
            historyLength: this.navigationHistory.length,
            canGoBack: this.historyIndex > 0,
            canGoForward: this.historyIndex < this.navigationHistory.length - 1
        };
    }

    /**
     * Nettoie les ressources
     */
    destroy() {
        super.destroy();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = NavigationController;
}

if (typeof window !== 'undefined') {
    window.NavigationController = NavigationController;
}
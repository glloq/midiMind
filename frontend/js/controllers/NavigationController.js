// ============================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Chemin rÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©el: frontend/js/controllers/NavigationController.js
// Version: v4.0.3 - FIXED BACKEND SIGNATURE - FIX PAGES NOT DISPLAYING
// Date: 2025-11-04
// ============================================================================
// CORRECTIONS v4.0.3:
// ✦ CRITIQUE: Gestion des parents des pages (ex: <div id="files"> parent de file-view)
// ✦ Fix: Affichage/masquage des éléments parents pour éviter display:none hérité
// ✦ Les pages sont maintenant vraiment visibles
// ============================================================================
// CORRECTIONS v4.0.1:
// Ã¢Å“â€¦ CRITIQUE: Ajout paramÃƒÂ¨tre backend au constructeur (6ÃƒÂ¨me paramÃƒÂ¨tre)
// Ã¢Å“â€¦ Fix: super() appelle BaseController avec backend
// Ã¢Å“â€¦ this.backend initialisÃƒÂ© automatiquement via BaseController
// ============================================================================
// ============================================================================
// CORRECTIONS v4.0.0:
// ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ CRITIQUE: Affichage/masquage direct des ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ments DOM
// ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ CRITIQUE: Initialisation des vues quand la page est affichÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e
// ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ CRITIQUE: Suppression de la gÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ration dynamique de contenu
// ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Fix: Toutes les pages s'affichent correctement
// ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Simplification: Navigation plus directe et fiable
// ============================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        // ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°tat de navigation
        this.currentPage = 'home';
        this.previousPage = null;
        this.navigationHistory = ['home'];
        this.historyIndex = 0;
        
        // Configuration des pages
        this.pages = {
            home: {
                id: 'home-view',
                element: null,
                title: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸Ãƒâ€šÃ‚ÂÃƒâ€šÃ‚Â  Accueil',
                icon: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸Ãƒâ€šÃ‚ÂÃƒâ€šÃ‚Â ',
                shortcut: 'h',
                viewKey: 'home',
                initialized: false
            },
            files: {
                id: 'file-view',
                element: null,
                title: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œÃƒâ€šÃ‚Â Fichiers',
                icon: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œÃƒâ€šÃ‚Â',
                shortcut: 'f',
                viewKey: 'file',
                initialized: false
            },
            editor: {
                id: 'editor-view',
                element: null,
                title: 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“Ãƒâ€šÃ‚ÂÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¸Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°diteur',
                icon: 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“Ãƒâ€šÃ‚ÂÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¸Ãƒâ€šÃ‚Â',
                shortcut: 'e',
                viewKey: 'editor',
                initialized: false
            },
            routing: {
                id: 'routing-view',
                element: null,
                title: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸ÃƒÂ¢Ã¢â€šÂ¬Ã‚ÂÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ Routage',
                icon: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸ÃƒÂ¢Ã¢â€šÂ¬Ã‚ÂÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬',
                shortcut: 'r',
                viewKey: 'routing',
                initialized: false
            },
            keyboard: {
                id: 'keyboard-view',
                element: null,
                title: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸Ãƒâ€¦Ã‚Â½Ãƒâ€šÃ‚Â¹ Clavier',
                icon: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸Ãƒâ€¦Ã‚Â½Ãƒâ€šÃ‚Â¹',
                shortcut: 'k',
                viewKey: 'keyboard',
                initialized: false
            },
            instruments: {
                id: 'instruments-view',
                element: null,
                title: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸Ãƒâ€¦Ã‚Â½Ãƒâ€šÃ‚Â¸ Instruments',
                icon: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸Ãƒâ€¦Ã‚Â½Ãƒâ€šÃ‚Â¸',
                shortcut: 'i',
                viewKey: 'instrument',
                initialized: false
            },
            playlist: {
                id: 'playlist-view',
                element: null,
                title: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¹ Playlist',
                icon: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¹',
                shortcut: 'p',
                viewKey: 'playlist',
                initialized: false
            },
            system: {
                id: 'system-view',
                element: null,
                title: 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã‚Â¡ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¸Ãƒâ€šÃ‚Â SystÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨me',
                icon: 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã‚Â¡ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã‚Â¯Ãƒâ€šÃ‚Â¸Ãƒâ€šÃ‚Â',
                shortcut: 's',
                viewKey: 'system',
                initialized: false
            },
            visualizer: {
                id: 'visualizer-view',
                element: null,
                title: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œÃƒâ€¦Ã‚Â  Visualiseur',
                icon: 'ÃƒÆ’Ã‚Â°Ãƒâ€¦Ã‚Â¸ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œÃƒâ€¦Ã‚Â ',
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
        
        this.log('info', 'NavigationController', 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Initialized v4.0.0');
        
        this.initializeNavigation();
    }

    /**
     * Liaison des ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nements
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
        // Cacher l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ment #app d'abord
        this.cachePageElements();
        this.setupKeyboardShortcuts();
        this.setupBrowserHistory();
        this.setupNavigationLinks();
        
        // Afficher la page initiale
        this.showPage('home', { skipHistory: true });
        
        this.log('debug', 'NavigationController', 'Navigation system initialized');
    }

    /**
     * Cache les rÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©fÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rences aux ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ments DOM des pages
     */
    cachePageElements() {
        Object.keys(this.pages).forEach(pageKey => {
            const pageConfig = this.pages[pageKey];
            pageConfig.element = document.getElementById(pageConfig.id);
            
            if (pageConfig.element) {
                // Cacher le parent aussi si c'est une section
                const parent = pageConfig.element.parentElement;
                if (parent && parent.id && parent.id !== 'app-main' && parent.id !== 'app') {
                    pageConfig.parentElement = parent;
                }
                
                // Masquer toutes les pages sauf home
                if (pageKey !== 'home') {
                    pageConfig.element.style.display = 'none';
                    pageConfig.element.classList.remove('active');
                    
                    // Masquer le parent aussi
                    if (pageConfig.parentElement) {
                        pageConfig.parentElement.style.display = 'none';
                    }
                } else {
                    // S'assurer que home est visible
                    pageConfig.element.style.display = 'block';
                    if (pageConfig.parentElement) {
                        pageConfig.parentElement.style.display = 'block';
                    }
                }
            } else {
                this.log('warn', 'NavigationController', `Page element not found: ${pageConfig.id}`);
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
            this.log('warn', 'NavigationController', `Page not found: ${pageKey}`);
            return false;
        }
        
        if (pageKey === this.currentPage && !forceRefresh) {
            this.log('debug', 'NavigationController', `Page already active: ${pageKey}`);
            return true;
        }
        
        if (this.transitionState.inProgress) {
            this.log('debug', 'NavigationController', 'Transition in progress');
            return false;
        }
        
        this.log('debug', 'NavigationController', `Navigating to: ${pageKey}`);
        
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
                
                this.log('debug', 'NavigationController', `Navigation successful: ${this.previousPage} ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ ${pageKey}`);
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
            this.log('error', 'NavigationController', 'Missing page elements', {
                currentId: currentPageConfig.id,
                targetId: targetPageConfig.id
            });
            return false;
        }
        
        try {
            // Masquer la page actuelle
            currentPageElement.classList.remove('active');
            currentPageElement.style.display = 'none';
            
            // Masquer le parent de la page actuelle aussi
            if (currentPageConfig.parentElement) {
                currentPageConfig.parentElement.style.display = 'none';
            }
            
            // Afficher la nouvelle page
            targetPageElement.style.display = 'block';
            targetPageElement.classList.add('active');
            
            // Afficher le parent de la nouvelle page aussi
            if (targetPageConfig.parentElement) {
                targetPageConfig.parentElement.style.display = 'block';
            }
            
            // Initialiser la vue si ce n'est pas dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©jÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  fait
            if (!targetPageConfig.initialized || forceRefresh) {
                await this.initializePageView(pageKey);
                targetPageConfig.initialized = true;
            }
            
            return true;
            
        } catch (error) {
            this.log('error', 'NavigationController', 'Transition error', error);
            return false;
        }
    }

    /**
     * Initialise la vue d'une page
     */
    async initializePageView(pageKey) {
        const pageConfig = this.pages[pageKey];
        const view = this.views[pageConfig.viewKey];
        
        if (!view) {
            this.log('warn', 'NavigationController', `View not found: ${pageConfig.viewKey}`);
            return;
        }
        
        try {
            // Si la vue a une mÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©thode init, l'appeler
            if (typeof view.init === 'function') {
                await view.init();
                this.log('debug', 'NavigationController', `View initialized: ${pageConfig.viewKey}`);
            }
            
            // Si la vue a une mÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©thode render, l'appeler
            if (typeof view.render === 'function') {
                await view.render();
                this.log('debug', 'NavigationController', `View rendered: ${pageConfig.viewKey}`);
            }
            
        } catch (error) {
            this.log('error', 'NavigationController', `View initialization error: ${pageConfig.viewKey}`, error);
        }
    }

    // ========================================================================
    // HISTORIQUE
    // ========================================================================

    /**
     * Ajoute ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  l'historique
     */
    addToHistory(pageKey) {
        // Si on n'est pas ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  la fin de l'historique, supprimer tout ce qui suit
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
     * Retour en arriÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re
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
     * Met ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour l'UI de navigation
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
     * RafraÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â®chit la page courante
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
     * Obtient l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©tat de navigation
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
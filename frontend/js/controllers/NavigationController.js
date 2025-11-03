// ============================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Chemin rÃ©el: frontend/js/controllers/NavigationController.js
// Version: v4.0.0 - FIX PAGES NOT DISPLAYING
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v4.0.0:
// âœ… CRITIQUE: Affichage/masquage direct des Ã©lÃ©ments DOM
// âœ… CRITIQUE: Initialisation des vues quand la page est affichÃ©e
// âœ… CRITIQUE: Suppression de la gÃ©nÃ©ration dynamique de contenu
// âœ… Fix: Toutes les pages s'affichent correctement
// âœ… Simplification: Navigation plus directe et fiable
// ============================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole, backend) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        // Ã‰tat de navigation
        this.currentPage = 'home';
        this.previousPage = null;
        this.navigationHistory = ['home'];
        this.historyIndex = 0;
        
        // Configuration des pages
        this.pages = {
            home: {
                id: 'home-view',
                element: null,
                title: 'ðŸ  Accueil',
                icon: 'ðŸ ',
                shortcut: 'h',
                viewKey: 'home',
                initialized: false
            },
            files: {
                id: 'file-view',
                element: null,
                title: 'ðŸ“ Fichiers',
                icon: 'ðŸ“',
                shortcut: 'f',
                viewKey: 'file',
                initialized: false
            },
            editor: {
                id: 'editor-view',
                element: null,
                title: 'âœï¸ Ã‰diteur',
                icon: 'âœï¸',
                shortcut: 'e',
                viewKey: 'editor',
                initialized: false
            },
            routing: {
                id: 'routing-view',
                element: null,
                title: 'ðŸ”€ Routage',
                icon: 'ðŸ”€',
                shortcut: 'r',
                viewKey: 'routing',
                initialized: false
            },
            keyboard: {
                id: 'keyboard-view',
                element: null,
                title: 'ðŸŽ¹ Clavier',
                icon: 'ðŸŽ¹',
                shortcut: 'k',
                viewKey: 'keyboard',
                initialized: false
            },
            instruments: {
                id: 'instruments-view',
                element: null,
                title: 'ðŸŽ¸ Instruments',
                icon: 'ðŸŽ¸',
                shortcut: 'i',
                viewKey: 'instrument',
                initialized: false
            },
            playlist: {
                id: 'playlist-view',
                element: null,
                title: 'ðŸ“‹ Playlist',
                icon: 'ðŸ“‹',
                shortcut: 'p',
                viewKey: 'playlist',
                initialized: false
            },
            system: {
                id: 'system-view',
                element: null,
                title: 'âš™ï¸ SystÃ¨me',
                icon: 'âš™ï¸',
                shortcut: 's',
                viewKey: 'system',
                initialized: false
            },
            visualizer: {
                id: 'visualizer-view',
                element: null,
                title: 'ðŸ“Š Visualiseur',
                icon: 'ðŸ“Š',
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
        
        this.logDebug('info', 'NavigationController', 'âœ… Initialized v4.0.0');
        
        this.initializeNavigation();
    }

    /**
     * Liaison des Ã©vÃ©nements
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
        // Cacher l'Ã©lÃ©ment #app d'abord
        this.cachePageElements();
        this.setupKeyboardShortcuts();
        this.setupBrowserHistory();
        this.setupNavigationLinks();
        
        // Afficher la page initiale
        this.showPage('home', { skipHistory: true });
        
        this.logDebug('debug', 'NavigationController', 'Navigation system initialized');
    }

    /**
     * Cache les rÃ©fÃ©rences aux Ã©lÃ©ments DOM des pages
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
                
                this.logDebug('debug', 'NavigationController', `Navigation successful: ${this.previousPage} â†’ ${pageKey}`);
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
            
            // Initialiser la vue si ce n'est pas dÃ©jÃ  fait
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
            // Si la vue a une mÃ©thode init, l'appeler
            if (typeof view.init === 'function') {
                await view.init();
                this.logDebug('debug', 'NavigationController', `View initialized: ${pageConfig.viewKey}`);
            }
            
            // Si la vue a une mÃ©thode render, l'appeler
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
     * Ajoute Ã  l'historique
     */
    addToHistory(pageKey) {
        // Si on n'est pas Ã  la fin de l'historique, supprimer tout ce qui suit
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
     * Retour en arriÃ¨re
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
     * Met Ã  jour l'UI de navigation
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
     * RafraÃ®chit la page courante
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
     * Obtient l'Ã©tat de navigation
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
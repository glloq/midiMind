// ============================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Chemin réel: frontend/js/controllers/NavigationController.js
// Version: v4.2.0 - CORRECTION INITIALIZATION ORDER
// Date: 2025-11-08
// ============================================================================
// CORRECTIONS v4.2.0:
// ✅ CRITIQUE: Désactivation autoInitialize pour éviter erreur initialization
// ✅ CRITIQUE: this.elements initialisé AVANT que onInitialize soit appelé
// ✅ Solution: config.autoInitialize = false dans le constructeur
//
// CORRECTIONS v4.1.0:
// ✅ CRITIQUE: Suppression référence inexistante this.controllers
// ✅ CRITIQUE: Suppression pageControllerMap (non nécessaire)
// ✅ Communication controllers via EventBus (architecture correcte)
// ============================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        // ✅ CRITIQUE: Désactiver l'auto-initialisation héritée
        // Car nous devons initialiser this.elements AVANT onInitialize()
        this.config.autoInitialize = false;
        
        // Configuration
        this.config = {
            ...this.config,
            pageSelector: '.page',
            navItemSelector: '.nav-item',
            activeClass: 'active',
            transitionDuration: 300,
            useTransitions: true,
            defaultPage: 'home'
        };
        
        // État
        this.state = {
            ...this.state,
            currentPage: null,
            previousPage: null,
            isTransitioning: false,
            initialized: false,
            history: []
        };
        
        // Cache des éléments DOM
        this.elements = {
            pages: null,
            navItems: null,
            appMain: null
        };
        
        // Mapping page -> vue
        this.pageViewMap = new Map();
        
        this.log('debug', 'NavigationController', '✓ NavigationController v4.2.0 created');
        
        // ✅ CRITIQUE: Maintenant initialiser manuellement
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    onInitialize() {
        this.log('info', 'NavigationController', 'Initializing navigation system...');
        
        // Cacher tous les éléments DOM
        this.cacheElements();
        
        // Enregistrer les vues
        this.registerPageMappings();
        
        // Attacher événements navigation
        this.attachNavigationEvents();
        
        // Initialiser toutes les vues
        this.initializeViews();
        
        // Écouter événements
        this.setupEventListeners();
        
        this.state.initialized = true;
        
        this.log('info', 'NavigationController', '✓ Navigation system initialized');
    }
    
    /**
     * Cacher les éléments DOM
     */
    cacheElements() {
        this.elements.pages = document.querySelectorAll(this.config.pageSelector);
        this.elements.navItems = document.querySelectorAll(this.config.navItemSelector);
        this.elements.appMain = document.querySelector('.app-main');
        
        this.log('debug', 'NavigationController', `Cached ${this.elements.pages.length} pages, ${this.elements.navItems.length} nav items`);
    }
    
    /**
     * Enregistrer les mappings page -> vue
     * ✅ CORRECTION: Suppression référence this.controllers (n'existe pas)
     */
    registerPageMappings() {
        // Mapper les pages aux vues disponibles
        const pageViewPairs = [
            ['home', 'home'],
            ['files', 'files'],
            ['instruments', 'instruments'],
            ['keyboard', 'keyboard'],
            ['routing', 'routing'],
            ['editor', 'editor'],
            ['system', 'system'],
            ['visualizer', 'visualizer']
        ];
        
        pageViewPairs.forEach(([pageName, viewName]) => {
            const view = this.views[viewName];
            if (view) {
                this.pageViewMap.set(pageName, view);
                this.log('debug', 'NavigationController', `✓ Mapped page '${pageName}' to view '${viewName}'`);
            } else {
                this.log('warn', 'NavigationController', `View '${viewName}' not found for page '${pageName}'`);
            }
        });
    }
    
    /**
     * Attacher les événements de navigation
     */
    attachNavigationEvents() {
        // Événements sur les éléments de navigation
        this.elements.navItems.forEach(navItem => {
            navItem.addEventListener('click', (e) => {
                e.preventDefault();
                
                const page = navItem.dataset.page || navItem.getAttribute('href')?.replace('#', '');
                
                if (page) {
                    this.showPage(page);
                }
            });
        });
        
        // Événements sur le hash change
        window.addEventListener('hashchange', () => {
            const hash = window.location.hash.replace('#', '');
            const page = hash || this.config.defaultPage;
            
            this.showPage(page, { fromHashChange: true });
        });
        
        this.log('debug', 'NavigationController', 'Navigation events attached');
    }
    
    /**
     * Initialiser toutes les vues
     */
    initializeViews() {
        let initializedCount = 0;
        
        for (const [page, view] of this.pageViewMap) {
            if (view && typeof view.init === 'function' && !view.state?.initialized) {
                try {
                    view.init();
                    initializedCount++;
                    this.log('debug', 'NavigationController', `✓ Initialized view: ${page}`);
                } catch (error) {
                    this.log('error', 'NavigationController', `Failed to init view ${page}:`, error);
                }
            }
        }
        
        this.log('info', 'NavigationController', `✓ Initialized ${initializedCount} views`);
    }
    
    /**
     * Écouter les événements globaux
     */
    setupEventListeners() {
        // Écouter les demandes de navigation via EventBus
        if (this.eventBus) {
            this.on('navigation:goto', (data) => {
                this.showPage(data.page, data.options);
            });
            
            this.on('navigation:back', () => {
                this.goBack();
            });
            
            this.on('navigation:forward', () => {
                this.goForward();
            });
        }
    }
    
    // ========================================================================
    // NAVIGATION
    // ========================================================================
    
    /**
     * Afficher une page
     * @param {string} pageName - Nom de la page (home, files, instruments, etc.)
     * @param {Object} options - Options de navigation
     */
    async showPage(pageName, options = {}) {
        // Validation
        if (!pageName) {
            this.log('warn', 'NavigationController', 'showPage called without pageName');
            return false;
        }
        
        // Si déjà en transition, ignorer (sauf si force)
        if (this.state.isTransitioning && !options.force) {
            this.log('debug', 'NavigationController', `Already transitioning, ignoring showPage(${pageName})`);
            return false;
        }
        
        // Si c'est déjà la page actuelle, ignorer (sauf si reload)
        if (this.state.currentPage === pageName && !options.reload) {
            this.log('debug', 'NavigationController', `Already on page ${pageName}`);
            return false;
        }
        
        this.state.isTransitioning = true;
        
        try {
            this.log('info', 'NavigationController', `Navigating to page: ${pageName}`);
            
            const previousPage = this.state.currentPage;
            
            // ✅ Émettre événement before navigation
            // Les controllers écoutent cet événement et se désactivent si nécessaire
            this.emit('navigation:before', {
                from: previousPage,
                to: pageName
            });
            
            // Transition sortie de la page actuelle
            if (previousPage && this.config.useTransitions) {
                await this.transitionOut(previousPage);
            }
            
            // Masquer toutes les pages
            this.hideAllPages();
            
            // Afficher la nouvelle page
            const pageElement = document.getElementById(pageName);
            if (!pageElement) {
                throw new Error(`Page element #${pageName} not found`);
            }
            
            pageElement.classList.add(this.config.activeClass);
            pageElement.style.display = 'block';
            
            // Mettre à jour la navigation
            this.updateNavigation(pageName);
            
            // Initialiser/Rendre la vue si nécessaire
            const view = this.pageViewMap.get(pageName);
            if (view) {
                // Si la vue n'est pas initialisée, l'initialiser
                if (!view.state?.initialized && typeof view.init === 'function') {
                    this.log('debug', 'NavigationController', `Initializing view: ${pageName}`);
                    view.init();
                }
                
                // Si la vue a une méthode render, la rendre
                if (typeof view.render === 'function') {
                    this.log('debug', 'NavigationController', `Rendering view: ${pageName}`);
                    view.render();
                }
                
                // Si la vue a une méthode show, l'appeler
                if (typeof view.show === 'function') {
                    view.show();
                }
            } else {
                this.log('warn', 'NavigationController', `No view found for page: ${pageName}`);
            }
            
            // Transition entrée
            if (this.config.useTransitions) {
                await this.transitionIn(pageName);
            }
            
            // Mettre à jour l'état
            this.state.previousPage = previousPage;
            this.state.currentPage = pageName;
            this.state.history.push({
                page: pageName,
                timestamp: Date.now()
            });
            
            // Mettre à jour le hash URL si pas déjà fait par hashchange
            if (!options.fromHashChange) {
                window.location.hash = pageName;
            }
            
            // ✅ Émettre événement after navigation
            // Les controllers écoutent cet événement et s'activent si nécessaire
            this.emit('navigation:after', {
                from: previousPage,
                to: pageName
            });
            
            // Notification si activée
            if (this.notifications && options.notify) {
                this.notifications.show(`Page: ${pageName}`, 'info', 2000);
            }
            
            this.log('info', 'NavigationController', `✓ Navigated to page: ${pageName}`);
            
            return true;
            
        } catch (error) {
            this.handleError(`Failed to show page ${pageName}`, error);
            return false;
            
        } finally {
            this.state.isTransitioning = false;
        }
    }
    
    /**
     * Masquer toutes les pages
     */
    hideAllPages() {
        this.elements.pages.forEach(page => {
            page.classList.remove(this.config.activeClass);
            
            // Pour les pages modales, les masquer complètement
            if (page.classList.contains('page-modal') || page.classList.contains('page-fullscreen')) {
                page.style.display = 'none';
            }
        });
    }
    
    /**
     * Mettre à jour la navigation active
     */
    updateNavigation(pageName) {
        this.elements.navItems.forEach(navItem => {
            const itemPage = navItem.dataset.page || navItem.getAttribute('href')?.replace('#', '');
            
            if (itemPage === pageName) {
                navItem.classList.add(this.config.activeClass);
            } else {
                navItem.classList.remove(this.config.activeClass);
            }
        });
    }
    
    /**
     * Revenir à la page précédente
     */
    goBack() {
        if (this.state.history.length > 1) {
            // Retirer la page actuelle
            this.state.history.pop();
            
            // Obtenir la page précédente
            const previousEntry = this.state.history[this.state.history.length - 1];
            
            if (previousEntry) {
                this.showPage(previousEntry.page, { fromHistory: true });
            }
        } else {
            // Si pas d'historique, aller à la page par défaut
            this.showPage(this.config.defaultPage);
        }
    }
    
    /**
     * Aller vers l'avant dans l'historique (si implémenté)
     */
    goForward() {
        // TODO: Implémenter si nécessaire
        this.log('warn', 'NavigationController', 'goForward not implemented yet');
    }
    
    // ========================================================================
    // TRANSITIONS
    // ========================================================================
    
    /**
     * Transition sortie d'une page
     */
    async transitionOut(pageName) {
        const pageElement = document.getElementById(pageName);
        if (!pageElement) return;
        
        return new Promise(resolve => {
            pageElement.classList.add('transitioning-out');
            
            setTimeout(() => {
                pageElement.classList.remove('transitioning-out');
                resolve();
            }, this.config.transitionDuration);
        });
    }
    
    /**
     * Transition entrée d'une page
     */
    async transitionIn(pageName) {
        const pageElement = document.getElementById(pageName);
        if (!pageElement) return;
        
        return new Promise(resolve => {
            pageElement.classList.add('transitioning-in');
            
            setTimeout(() => {
                pageElement.classList.remove('transitioning-in');
                resolve();
            }, this.config.transitionDuration);
        });
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * Obtenir la page actuelle
     */
    getCurrentPage() {
        return this.state.currentPage;
    }
    
    /**
     * Obtenir la page précédente
     */
    getPreviousPage() {
        return this.state.previousPage;
    }
    
    /**
     * Obtenir l'historique
     */
    getHistory() {
        return [...this.state.history];
    }
    
    /**
     * Vérifier si une page est active
     */
    isPageActive(pageName) {
        return this.state.currentPage === pageName;
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES
    // ========================================================================
    
    /**
     * Recharger la page actuelle
     */
    reloadCurrentPage() {
        if (this.state.currentPage) {
            this.showPage(this.state.currentPage, { reload: true });
        }
    }
    
    /**
     * Aller à la page par défaut
     */
    goHome() {
        this.showPage(this.config.defaultPage);
    }
    
    /**
     * Obtenir la vue d'une page
     */
    getPageView(pageName) {
        return this.pageViewMap.get(pageName);
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    onDestroy() {
        this.log('info', 'NavigationController', 'Destroying navigation system...');
        
        // Retirer les événements
        this.elements.navItems.forEach(navItem => {
            navItem.replaceWith(navItem.cloneNode(true));
        });
        
        // Nettoyer les caches
        this.pageViewMap.clear();
        
        this.log('info', 'NavigationController', '✓ Navigation system destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof window !== 'undefined') {
    window.NavigationController = NavigationController;
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = NavigationController;
}
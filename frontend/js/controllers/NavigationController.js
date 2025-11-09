// ============================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Chemin réel: frontend/js/controllers/NavigationController.js
// Version: v4.2.0 - CORRECTION INITIALISATION
// Date: 2025-11-09
// ============================================================================
// CORRECTIONS v4.2.0:
// ✅ CRITIQUE: Initialisation this.elements avant super() impossible
// ✅ CRITIQUE: cacheElements() vérifie et initialise this.elements si nécessaire
// ✅ Protection contre appel onInitialize() avant fin constructeur
// ✅ Correction encodage UTF-8 (✓ au lieu de âœ…)
//
// CORRECTIONS v4.1.0:
// ✅ CRITIQUE: Suppression référence inexistante this.controllers
// ✅ CRITIQUE: Suppression pageControllerMap (non nécessaire)
// ✅ Communication controllers via EventBus (architecture correcte)
// ✅ Les controllers s'activent/désactivent via événements navigation:before/after
//
// CRÉATION v4.0.0:
// ✅ CRITIQUE: Recréation complète du NavigationController manquant
// ✅ Gestion affichage/masquage des pages
// ✅ Synchronisation avec Router et hash URL
// ✅ Mise à jour états active sur navigation
// ✅ Appel automatique init/render des vues
// ✅ Support transitions
// ============================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
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
        
        // ✅ CRITIQUE: Initialisation des éléments (peut être null au début)
        // cacheElements() initialisera si nécessaire
        if (!this.elements) {
            this.elements = {
                pages: null,
                navItems: null,
                appMain: null
            };
        }
        
        // Mapping page -> vue
        this.pageViewMap = new Map();
        
        this.log('debug', 'NavigationController', '✓ NavigationController v4.2.0 created');
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
     * ✅ CORRECTION: Vérifie et initialise this.elements si nécessaire
     */
    cacheElements() {
        // ✅ Protection: Initialiser this.elements si pas déjà fait
        if (!this.elements || typeof this.elements !== 'object') {
            this.elements = {
                pages: null,
                navItems: null,
                appMain: null
            };
        }
        
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
        // Mapping pages -> vues (selon les IDs dans index.html)
        const pageMappings = {
            'home': this.views.home,
            'files': this.views.file,
            'instruments': this.views.instrument,
            'keyboard': this.views.keyboard,
            'system': this.views.system,
            'editor': this.views.editor,
            'routing': this.views.routing,
            'playlist': this.views.playlist,
            'visualizer': this.views.visualizer
        };
        
        for (const [page, view] of Object.entries(pageMappings)) {
            if (view) {
                this.pageViewMap.set(page, view);
            }
        }
        
        this.log('debug', 'NavigationController', `Registered ${this.pageViewMap.size} page-view mappings`);
    }
    
    /**
     * Attacher les événements de navigation
     */
    attachNavigationEvents() {
        // Cliquer sur les items de navigation
        this.elements.navItems.forEach(navItem => {
            navItem.addEventListener('click', (e) => {
                e.preventDefault();
                const page = navItem.dataset.page || navItem.getAttribute('href')?.replace('#', '');
                if (page) {
                    this.showPage(page);
                }
            });
        });
        
        // Écouter les changements de hash
        window.addEventListener('hashchange', () => {
            const hash = window.location.hash.slice(1); // Retirer le #
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
            if (pageElement) {
                pageElement.classList.add(this.config.activeClass);
                
                // Pour les pages modales, les afficher
                if (pageElement.classList.contains('page-modal') || pageElement.classList.contains('page-fullscreen')) {
                    pageElement.style.display = '';
                }
                
                // Appeler render de la vue si elle existe
                const view = this.pageViewMap.get(pageName);
                if (view && typeof view.render === 'function') {
                    try {
                        view.render();
                    } catch (error) {
                        this.log('error', 'NavigationController', `Failed to render view for ${pageName}:`, error);
                    }
                }
            } else {
                this.log('warn', 'NavigationController', `Page element not found: #${pageName}`);
            }
            
            // Mettre à jour la navigation
            this.updateNavigation(pageName);
            
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
            // Si pas d'historique, aller à home
            this.showPage(this.config.defaultPage);
        }
    }
    
    /**
     * Aller en avant (si applicable)
     */
    goForward() {
        // TODO: Implémenter si besoin d'un système de navigation avant/arrière complet
        this.log('debug', 'NavigationController', 'goForward not implemented');
    }
    
    // ========================================================================
    // TRANSITIONS
    // ========================================================================
    
    /**
     * Transition de sortie d'une page
     */
    async transitionOut(pageName) {
        const pageElement = document.getElementById(pageName);
        
        if (pageElement) {
            pageElement.style.transition = `opacity ${this.config.transitionDuration}ms ease`;
            pageElement.style.opacity = '0';
            
            await this.wait(this.config.transitionDuration);
        }
    }
    
    /**
     * Transition d'entrée d'une page
     */
    async transitionIn(pageName) {
        const pageElement = document.getElementById(pageName);
        
        if (pageElement) {
            pageElement.style.opacity = '0';
            pageElement.style.transition = `opacity ${this.config.transitionDuration}ms ease`;
            
            // Force reflow
            pageElement.offsetHeight;
            
            pageElement.style.opacity = '1';
            
            await this.wait(this.config.transitionDuration);
        }
    }
    
    /**
     * Attendre un délai
     */
    wait(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
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
        if (this.elements && this.elements.navItems) {
            this.elements.navItems.forEach(navItem => {
                navItem.replaceWith(navItem.cloneNode(true));
            });
        }
        
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
// ============================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Chemin réel: frontend/js/controllers/NavigationController.js
// Version: v4.7.0 - FIX DOUBLE INITIALIZATION
// Date: 2025-11-13
// ============================================================================
// CORRECTIONS v4.7.0:
// ✅ CRITIQUE: Fix double initialisation qui causait freeze au démarrage
// ✅ Early return dans onInitialize() si pageViewMap pas prêt
// ✅ Initialisation contrôlée dans le constructeur après création de pageViewMap
// ✅ Application.js ne rappelle plus init() manuellement
//
// CORRECTIONS v4.6.0:
// ✅ CRITIQUE: Fix boucle infinie causée par double écoute de 'hashchange'
// ✅ Suppression du listener 'hashchange' dans NavigationController
// ✅ Le Router gère maintenant seul les changements de hash
// ✅ Ajout du flag 'fromRouter' pour éviter la mise à jour cyclique du hash
//
// CORRECTIONS v4.5.0:
// ✓ Encodage UTF-8 corrigé (émojis, accents)
// ✓ Amélioration du logging des erreurs
// ✓ Vérification renforcée du mapping page->vue
//
// CORRECTIONS v4.2.0:
// ✓ CRITIQUE: Fix pageViewMap undefined
// ✓ Solution: pageViewMap créé AVANT appel à super()
// ✓ onInitialize() vérifie existence de pageViewMap
//
// CORRECTIONS v4.1.0:
// ✓ CRITIQUE: Suppression référence inexistante this.controllers
// ✓ Communication controllers via EventBus
// ============================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        // ✓ CRITIQUE: Créer les structures AVANT super()
        // Note: En JavaScript, on ne peut pas accéder à `this` avant super()

        super(eventBus, models, views, notifications, debugConsole, backend);

        // ✅ FIX v4.7.0: Désactiver l'auto-initialisation IMMÉDIATEMENT après super()
        // Cela empêche BaseController.initialize() de s'exécuter
        this.config.autoInitialize = false;
        this.state.isInitialized = false;
        
        // Configuration spécifique
        Object.assign(this.config, {
            pageSelector: '.page',
            navItemSelector: '.nav-item',
            activeClass: 'active',
            transitionDuration: 300,
            useTransitions: true,
            defaultPage: 'home'
        });
        
        // État spécifique
        Object.assign(this.state, {
            currentPage: null,
            previousPage: null,
            isTransitioning: false,
            history: []
        });
        
        // Cache des éléments DOM
        this.elements = {
            pages: null,
            navItems: null,
            appMain: null
        };
        
        // ✓ CRITIQUE: Mapping page -> vue créé ICI
        this.pageViewMap = new Map();

        // ✅ FIX v4.7.0: S'initialiser MAINTENANT de manière contrôlée
        // On appelle initialize() qui va appeler onInitialize() de manière sûre
        this.initialize();

        this.log('debug', 'NavigationController', '✓ NavigationController v4.7.0 created and initialized');
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    onInitialize() {
        // ✅ FIX v4.7.0: Early return si pageViewMap n'existe pas encore
        // Cela se produit quand BaseController.constructor() appelle initialize()
        // avant que NavigationController ait pu créer pageViewMap.
        // Dans ce cas, on skip l'initialisation et on la fera plus tard explicitement.
        if (!this.pageViewMap) {
            this.log('debug', 'NavigationController', 'Skipping early onInitialize (pageViewMap not ready)');
            return;
        }

        this.log('info', 'NavigationController', 'Initializing navigation system...');
        
        try {
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
        } catch (error) {
            this.log('error', 'NavigationController', 'Initialization failed', error);
        }
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
     */
    registerPageMappings() {
        // ✓ Double vérification de sécurité
        if (!this.pageViewMap) {
            this.log('error', 'NavigationController', 'pageViewMap is null in registerPageMappings!');
            return;
        }
        
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
        
        // Enregistrer les mappings et logger les manquants
        for (const [page, view] of Object.entries(pageMappings)) {
            if (view) {
                this.pageViewMap.set(page, view);
                this.log('debug', 'NavigationController', `✓ Registered mapping: ${page} -> ${view.constructor.name}`);
            } else {
                this.log('warn', 'NavigationController', `⚠ Missing view for page: ${page}`);
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

        // ✅ FIX: NE PAS écouter 'hashchange' directement ici
        // Le Router gère déjà les changements de hash et appelle showPage()
        // via l'événement 'route-changed' dans Application.js
        // Écouter hashchange ici créait une BOUCLE INFINIE car:
        // 1. Router écoute hashchange → émet 'route-changed' → appelle showPage()
        // 2. NavigationController écoute hashchange → appelle showPage()
        // 3. showPage() met à jour window.location.hash
        // 4. Cela déclenche hashchange → retour à l'étape 1 = BOUCLE INFINIE

        this.log('debug', 'NavigationController', 'Navigation events attached (hashchange handled by Router)');
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
        console.log(`🔵 [1] showPage called: ${pageName}, options:`, options);

        // Validation
        if (!pageName) {
            console.log(`🔵 [RETURN] No pageName provided`);
            this.log('warn', 'NavigationController', 'showPage called without pageName');
            return false;
        }

        console.log(`🔵 [2] Checking isTransitioning: ${this.state.isTransitioning}, force: ${options.force}`);

        // Si déjà en transition, ignorer (sauf si force)
        if (this.state.isTransitioning && !options.force) {
            console.log(`🔵 [RETURN] Already transitioning`);
            this.log('debug', 'NavigationController', `Already transitioning, ignoring showPage(${pageName})`);
            return false;
        }

        console.log(`🔵 [3] currentPage: ${this.state.currentPage}, reload: ${options.reload}`);

        // Si c'est déjà la page actuelle, ignorer (sauf si reload)
        if (this.state.currentPage === pageName && !options.reload) {
            console.log(`🔵 [RETURN] Already on page ${pageName}`);
            this.log('debug', 'NavigationController', `Already on page ${pageName}`);
            return false;
        }

        console.log(`🔵 [4] Setting isTransitioning = true`);
        this.state.isTransitioning = true;

        try {
            console.log(`🔵 [5] Starting navigation to: ${pageName}`);
            this.log('info', 'NavigationController', `Navigating to page: ${pageName}`);

            const previousPage = this.state.currentPage;
            console.log(`🔵 [6] previousPage: ${previousPage}`);

            // ✓ Émettre événement before navigation
            console.log(`🔵 [7] Emitting navigation:before`);
            this.emit('navigation:before', {
                from: previousPage,
                to: pageName
            });
            console.log(`🔵 [8] navigation:before emitted`);

            // ✅ Appeler hide() sur la vue précédente pour arrêter ses animations
            console.log(`🔵 [9] Hiding previous view`);
            if (previousPage) {
                const previousView = this.pageViewMap.get(previousPage);
                if (previousView && typeof previousView.hide === 'function') {
                    this.log('debug', 'NavigationController', `Hiding previous view: ${previousPage}`);
                    previousView.hide();
                }
            }
            console.log(`🔵 [10] Previous view hidden`);

            // Transition sortie de la page actuelle
            console.log(`🔵 [11] Transition out`);
            if (previousPage && this.config.useTransitions) {
                await this.transitionOut(previousPage);
            }

            console.log(`🔵 [12] Calling hideAllPages`);
            // Masquer toutes les pages
            this.hideAllPages();

            console.log(`🔵 [13] Finding page element #${pageName}`);
            // Afficher la nouvelle page
            const pageElement = document.getElementById(pageName);
            if (!pageElement) {
                throw new Error(`Page element #${pageName} not found in DOM`);
            }

            console.log(`🔵 [14] Showing page element`);
            pageElement.classList.add(this.config.activeClass);
            pageElement.style.display = 'block';

            console.log(`🔵 [15] Calling updateNavigation`);
            // Mettre à jour la navigation
            this.updateNavigation(pageName);

            console.log(`🔵 [16] Getting view from pageViewMap`);
            // Initialiser/Rendre la vue si nécessaire
            const view = this.pageViewMap.get(pageName);
            console.log(`🔵 [17] View found: ${!!view}, initialized: ${view?.state?.initialized}`);

            if (view) {
                // Si la vue n'est pas initialisée, l'initialiser
                if (!view.state?.initialized && typeof view.init === 'function') {
                    console.log(`🔵 [18] CALLING view.init()`);
                    this.log('debug', 'NavigationController', `Initializing view: ${pageName}`);
                    view.init();
                    console.log(`🔵 [19] view.init() COMPLETED`);
                }

                // Si la vue a une méthode render, la rendre
                if (typeof view.render === 'function') {
                    console.log(`🔵 [20] CALLING view.render()`);
                    this.log('debug', 'NavigationController', `Rendering view: ${pageName}`);
                    view.render();
                    console.log(`🔵 [21] view.render() COMPLETED`);
                }

                // Si la vue a une méthode show, l'appeler
                if (typeof view.show === 'function') {
                    console.log(`🔵 [22] CALLING view.show()`);
                    view.show();
                    console.log(`🔵 [23] view.show() COMPLETED`);
                }
            } else {
                this.log('warn', 'NavigationController', `No view found for page: ${pageName}`);
                this.log('debug', 'NavigationController', `Available pages: ${Array.from(this.pageViewMap.keys()).join(', ')}`);
            }

            console.log(`🔵 [24] Post-view operations`);

            console.log(`🔵 [25] Transition in`);
            console.log(`🔵 [25.1] Checking if transitions enabled: ${this.config.useTransitions}`);
            // Transition entrée
            if (this.config.useTransitions) {
                console.log(`🔵 [25.2] Transitions enabled, calling transitionIn(${pageName})...`);
                await this.transitionIn(pageName);
                console.log(`🔵 [25.3] transitionIn() returned`);
            } else {
                console.log(`🔵 [25.2] Transitions disabled, skipping`);
            }
            console.log(`🔵 [25.4] After transition block`);

            console.log(`🔵 [26] Updating state`);
            // Mettre à jour l'état
            console.log(`🔵 [26.1] Setting previousPage to: ${previousPage}`);
            this.state.previousPage = previousPage;

            console.log(`🔵 [26.2] Setting currentPage to: ${pageName}`);
            this.state.currentPage = pageName;

            console.log(`🔵 [26.3] Pushing to history array (current length: ${this.state.history.length})`);
            this.state.history.push({
                page: pageName,
                timestamp: Date.now()
            });
            console.log(`🔵 [26.4] History push completed (new length: ${this.state.history.length})`);

            console.log(`🔵 [27] Checking fromRouter: ${options.fromRouter}`);
            console.log(`🔵 [27.5] Current window.location.hash: ${window.location.hash}`);
            // ✅ FIX: Ne mettre à jour le hash QUE si l'appel ne vient PAS du Router
            // ET si le hash est différent de la page actuelle
            if (!options.fromRouter) {
                const currentHash = window.location.hash.replace('#', '');
                console.log(`🔵 [27.6] Comparing currentHash '${currentHash}' with pageName '${pageName}'`);

                if (currentHash !== pageName) {
                    console.log(`🔵 [28] SETTING window.location.hash = #${pageName}`);
                    console.log(`🔵 [28.1] ⚠️ WARNING: This will trigger hashchange event!`);
                    const beforeHash = window.location.hash;
                    window.location.hash = pageName;
                    const afterHash = window.location.hash;
                    console.log(`🔵 [29] Hash updated from ${beforeHash} to ${afterHash}`);
                    console.log(`🔵 [29.1] Hash change will be processed asynchronously`);
                } else {
                    console.log(`🔵 [28] SKIPPING hash update (already #${pageName})`);
                }
            } else {
                console.log(`🔵 [28] SKIPPING hash update (fromRouter=true)`);
            }

            console.log(`🔵 [30] Emitting navigation:after`);
            console.log(`🔵 [30.1] About to call this.emit() with event data:`, { from: previousPage, to: pageName });
            // ✓ Émettre événement after navigation
            this.emit('navigation:after', {
                from: previousPage,
                to: pageName
            });
            console.log(`🔵 [31] navigation:after emitted`);

            console.log(`🔵 [32] Checking notifications`);
            // Notification si activée
            if (this.notifications && options.notify) {
                this.notifications.show(`Page: ${pageName}`, 'info', 2000);
            }

            console.log(`🔵 [33] ✓ Navigation SUCCESS`);
            this.log('info', 'NavigationController', `✓ Navigated to page: ${pageName}`);

            console.log(`🔵 [34] Returning true from showPage()`);
            return true;

        } catch (error) {
            // Logging détaillé de l'erreur
            console.error('❌ NavigationController.showPage() exception:', error);
            console.error('Stack trace:', error.stack);
            console.error('Error details:', {
                message: error.message,
                name: error.name,
                pageName: pageName,
                pageElement: !!document.getElementById(pageName),
                elementsPages: !!this.elements.pages,
                pagesCount: this.elements.pages?.length || 0,
                pageViewMapSize: this.pageViewMap?.size || 0,
                availablePages: this.pageViewMap ? Array.from(this.pageViewMap.keys()) : []
            });

            this.log('error', 'NavigationController', `Failed to show page ${pageName}:`, error.message);
            this.handleError(`Failed to show page ${pageName}`, error);

            console.log(`🔵 [ERROR-RETURN] Returning false from showPage() after error`);
            return false;

        } finally {
            console.log(`🔵 [FINALLY] Entering finally block, setting isTransitioning = false`);
            this.state.isTransitioning = false;
            console.log(`🔵 [FINALLY] isTransitioning set to false, exiting showPage()`);
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
        console.log(`🟤 [transitionOut] START for page: ${pageName}`);
        const pageElement = document.getElementById(pageName);
        console.log(`🟤 [transitionOut] pageElement found: ${!!pageElement}`);

        if (pageElement) {
            console.log(`🟤 [transitionOut] Setting transition and opacity`);
            pageElement.style.transition = `opacity ${this.config.transitionDuration}ms ease`;
            pageElement.style.opacity = '0';

            console.log(`🟤 [transitionOut] Waiting ${this.config.transitionDuration}ms...`);
            await this.wait(this.config.transitionDuration);
            console.log(`🟤 [transitionOut] Wait completed`);
        }

        console.log(`🟤 [transitionOut] COMPLETED`);
    }
    
    /**
     * Transition d'entrée d'une page
     */
    async transitionIn(pageName) {
        console.log(`🟣 [transitionIn] START for page: ${pageName}`);
        console.log(`🟣 [transitionIn] useTransitions: ${this.config.useTransitions}, duration: ${this.config.transitionDuration}`);

        const pageElement = document.getElementById(pageName);
        console.log(`🟣 [transitionIn] pageElement found: ${!!pageElement}`);

        if (pageElement) {
            console.log(`🟣 [transitionIn] Setting opacity to 0`);
            pageElement.style.opacity = '0';
            pageElement.style.transition = `opacity ${this.config.transitionDuration}ms ease`;

            console.log(`🟣 [transitionIn] Forcing reflow`);
            // Force reflow
            pageElement.offsetHeight;

            console.log(`🟣 [transitionIn] Setting opacity to 1`);
            pageElement.style.opacity = '1';

            console.log(`🟣 [transitionIn] Waiting ${this.config.transitionDuration}ms...`);
            await this.wait(this.config.transitionDuration);
            console.log(`🟣 [transitionIn] Wait completed`);
        }

        console.log(`🟣 [transitionIn] COMPLETED`);
    }
    
    /**
     * Attendre un délai
     */
    wait(ms) {
        console.log(`⏱️ [wait] Creating Promise to wait ${ms}ms`);
        return new Promise(resolve => {
            console.log(`⏱️ [wait] Setting setTimeout for ${ms}ms`);
            const timeoutId = setTimeout(() => {
                console.log(`⏱️ [wait] setTimeout callback fired after ${ms}ms, resolving Promise`);
                resolve();
            }, ms);
            console.log(`⏱️ [wait] setTimeout set with ID: ${timeoutId}`);
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
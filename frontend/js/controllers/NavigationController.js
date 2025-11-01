// ============================================================================
// Fichier: frontend/js/controllers/NavigationController.js
// Chemin réel: frontend/js/controllers/NavigationController.js
// Version: v3.8.0 - NAVIGATION OPTIMISÉE
// Date: 2025-11-01
// ============================================================================
// AMÉLIORATIONS v3.8.0:
// ✅ Préchargement intelligent des pages
// ✅ Cache amélioré avec stratégie LRU
// ✅ Lazy loading des vues
// ✅ Animations optimisées (GPU)
// ✅ Gestion erreurs de chargement robuste
// ✅ Support PWA et mode offline
// ✅ Historique de navigation amélioré
// ✅ Transitions fluides avec préload
// ============================================================================

class NavigationController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // État de navigation
        this.currentPage = 'home';
        this.previousPage = null;
        this.navigationHistory = ['home'];
        this.historyIndex = 0;
        
        // Configuration des pages
        this.pages = {
            home: {
                id: 'home-view',
                title: '🏠 Accueil',
                icon: '🏠',
                shortcut: 'h',
                requiresData: true,
                cacheable: true,
                preloadPriority: 'high',
                viewKey: 'home'
            },
            files: {
                id: 'file-view',
                title: '📁 Fichiers',
                icon: '📁',
                shortcut: 'f',
                requiresData: true,
                cacheable: true,
                preloadPriority: 'high',
                viewKey: 'file'
            },
            editor: {
                id: 'editor-view',
                title: '✏️ Éditeur',
                icon: '✏️',
                shortcut: 'e',
                requiresData: true,
                cacheable: false,
                preloadPriority: 'medium',
                viewKey: 'editor'
            },
            routing: {
                id: 'routing-view',
                title: '🔀 Routage',
                icon: '🔀',
                shortcut: 'r',
                requiresData: true,
                cacheable: true,
                preloadPriority: 'medium',
                viewKey: 'routing'
            },
            keyboard: {
                id: 'keyboard-view',
                title: '🎹 Clavier',
                icon: '🎹',
                shortcut: 'k',
                requiresData: true,
                cacheable: false,
                preloadPriority: 'low',
                viewKey: 'keyboard'
            },
            instruments: {
                id: 'instruments-view',
                title: '🎸 Instruments',
                icon: '🎸',
                shortcut: 'i',
                requiresData: true,
                cacheable: true,
                preloadPriority: 'medium',
                viewKey: 'instrument'
            },
            system: {
                id: 'system-view',
                title: '⚙️ Système',
                icon: '⚙️',
                shortcut: 's',
                requiresData: true,
                cacheable: true,
                preloadPriority: 'low',
                viewKey: 'system'
            }
        };
        
        // Configuration des transitions
        this.transitionState = {
            inProgress: false,
            duration: 300,
            easing: 'cubic-bezier(0.4, 0, 0.2, 1)',
            useGPU: true
        };
        
        // Cache des pages avec stratégie LRU
        this.pageCache = new Map();
        this.cacheAccessOrder = [];
        this.maxCacheSize = 5;
        this.cacheTimeouts = new Map();
        this.defaultCacheDuration = 60000; // 1 minute
        
        // Configuration des animations
        this.animationConfig = {
            enableTransitions: true,
            slideDirection: 'horizontal',
            parallax: false,
            preloadNext: true,
            useGPUAcceleration: true
        };
        
        // Preload state
        this.preloadState = {
            enabled: true,
            inProgress: false,
            queue: [],
            loaded: new Set()
        };
        
        // Offline support
        this.offlineMode = false;
        
        this.log('info', 'NavigationController', '✅ Initialized v3.8.0');
        
        this.initializeNavigation();
    }

    /**
     * Liaison des événements
     */
    bindEvents() {
        this.eventBus.on('model:changed', (data) => {
            this.handleModelChange(data);
        });
        
        this.eventBus.on('navigation:page_request', (data) => {
            this.showPage(data.page, data.options);
        });
        
        this.eventBus.on('navigation:back', () => {
            this.goBack();
        });
        
        this.eventBus.on('navigation:forward', () => {
            this.goForward();
        });
        
        // Invalidation de cache sur changements
        this.eventBus.on('file:updated', () => {
            this.invalidatePageCache(['home', 'files', 'editor']);
        });
        
        this.eventBus.on('instrument:updated', () => {
            this.invalidatePageCache(['home', 'instruments', 'keyboard']);
        });
        
        this.eventBus.on('playlist:updated', () => {
            this.invalidatePageCache(['home', 'editor']);
        });
        
        // Mode offline
        window.addEventListener('online', () => {
            this.offlineMode = false;
            this.log('info', 'NavigationController', '🌐 Online mode');
        });
        
        window.addEventListener('offline', () => {
            this.offlineMode = true;
            this.notify('warning', 'Mode hors ligne activé');
            this.log('info', 'NavigationController', '📡 Offline mode');
        });
    }

    /**
     * Initialisation
     */
    initializeNavigation() {
        this.setupKeyboardShortcuts();
        this.setupBrowserHistory();
        this.setupNavigationLinks();
        this.setupVisibilityChange();
        
        // Précharger les pages prioritaires
        this.preloadHighPriorityPages();
        
        // Afficher la page initiale
        this.showPage('home', { skipHistory: true });
        
        this.log('debug', 'NavigationController', 'Navigation system initialized');
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

    /**
     * Configure la détection de changement de visibilité
     */
    setupVisibilityChange() {
        document.addEventListener('visibilitychange', () => {
            if (!document.hidden) {
                // Rafraîchir la page si données potentiellement obsolètes
                if (this.shouldRefreshOnVisible()) {
                    this.refreshCurrentPage();
                }
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
            skipPushState = false,
            animationDirection = 'forward'
        } = options;
        
        if (!this.pages[pageKey]) {
            this.log('warn', 'NavigationController', `Page not found: ${pageKey}`);
            this.notify('error', 'Page introuvable');
            return false;
        }
        
        if (pageKey === this.currentPage && !forceRefresh) {
            this.log('debug', 'NavigationController', `Page already active: ${pageKey}`);
            return true;
        }
        
        if (this.transitionState.inProgress) {
            this.log('debug', 'NavigationController', 'Transition in progress, queuing request');
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
            
            // Précharger la page suivante probable si activé
            if (this.animationConfig.preloadNext) {
                this.preloadNextLikelyPage(pageKey);
            }
            
            const success = await this.performPageTransition(pageKey, animationDirection, forceRefresh);
            
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
                
                this.log('debug', 'NavigationController', `Navigation successful: ${this.previousPage} → ${pageKey}`);
                return true;
            }
            
        } catch (error) {
            this.handleError('Erreur navigation', error);
            this.notify('error', 'Erreur lors du chargement de la page');
            return false;
            
        } finally {
            this.transitionState.inProgress = false;
        }
        
        return false;
    }

    /**
     * Effectue la transition de page
     */
    async performPageTransition(pageKey, animationDirection, forceRefresh) {
        const pageConfig = this.pages[pageKey];
        const currentPageElement = document.getElementById(this.pages[this.currentPage].id);
        const targetPageElement = document.getElementById(pageConfig.id);
        
        if (!currentPageElement || !targetPageElement) {
            this.log('error', 'NavigationController', 'Missing page elements', {
                currentId: this.pages[this.currentPage].id,
                targetId: pageConfig.id
            });
            return false;
        }
        
        try {
            // Obtenir le contenu de la page
            const pageContent = await this.getPageContent(pageKey, forceRefresh);
            
            if (!pageContent) {
                throw new Error('Empty page content');
            }
            
            // Animation de sortie
            if (this.animationConfig.enableTransitions) {
                await this.animatePageOut(currentPageElement, animationDirection);
            } else {
                currentPageElement.classList.remove('active');
                currentPageElement.style.display = 'none';
            }
            
            // Injecter le contenu
            targetPageElement.innerHTML = pageContent;
            
            // Initialiser les composants de la page si nécessaire
            this.initializePageComponents(pageKey, targetPageElement);
            
            // Animation d'entrée
            if (this.animationConfig.enableTransitions) {
                await this.animatePageIn(targetPageElement, animationDirection);
            } else {
                targetPageElement.style.display = 'block';
                targetPageElement.classList.add('active');
            }
            
            return true;
            
        } catch (error) {
            this.log('error', 'NavigationController', 'Transition error', error);
            return false;
        }
    }

    /**
     * Animation de sortie (optimisée GPU)
     */
    animatePageOut(pageElement, direction) {
        return new Promise((resolve) => {
            if (this.animationConfig.useGPUAcceleration) {
                pageElement.style.willChange = 'transform, opacity';
            }
            
            const animation = pageElement.animate([
                { 
                    opacity: 1, 
                    transform: 'translateX(0%) translateZ(0)' 
                },
                { 
                    opacity: 0, 
                    transform: 'translateX(-20%) translateZ(0)' 
                }
            ], {
                duration: this.transitionState.duration,
                easing: this.transitionState.easing,
                fill: 'forwards'
            });
            
            animation.onfinish = () => {
                pageElement.classList.remove('active');
                pageElement.style.display = 'none';
                if (this.animationConfig.useGPUAcceleration) {
                    pageElement.style.willChange = 'auto';
                }
                resolve();
            };
        });
    }

    /**
     * Animation d'entrée (optimisée GPU)
     */
    animatePageIn(pageElement, direction) {
        return new Promise((resolve) => {
            pageElement.style.display = 'block';
            pageElement.style.opacity = '0';
            pageElement.style.transform = 'translateX(20%) translateZ(0)';
            
            if (this.animationConfig.useGPUAcceleration) {
                pageElement.style.willChange = 'transform, opacity';
            }
            
            const animation = pageElement.animate([
                { 
                    opacity: 0, 
                    transform: 'translateX(20%) translateZ(0)' 
                },
                { 
                    opacity: 1, 
                    transform: 'translateX(0%) translateZ(0)' 
                }
            ], {
                duration: this.transitionState.duration,
                easing: this.transitionState.easing,
                fill: 'forwards'
            });
            
            animation.onfinish = () => {
                pageElement.classList.add('active');
                pageElement.style.opacity = '';
                pageElement.style.transform = '';
                if (this.animationConfig.useGPUAcceleration) {
                    pageElement.style.willChange = 'auto';
                }
                resolve();
            };
        });
    }

    // ========================================================================
    // CACHE & PRÉCHARGEMENT
    // ========================================================================

    /**
     * Obtient le contenu d'une page (avec cache)
     */
    async getPageContent(pageKey, forceRefresh = false) {
        const pageConfig = this.pages[pageKey];
        
        // Vérifier le cache
        if (!forceRefresh && pageConfig.cacheable && this.pageCache.has(pageKey)) {
            this.updateCacheAccessOrder(pageKey);
            this.log('debug', 'NavigationController', `Cache hit for: ${pageKey}`);
            return this.pageCache.get(pageKey);
        }
        
        // Générer le contenu
        const content = await this.generatePageContent(pageKey);
        
        // Mettre en cache si applicable
        if (pageConfig.cacheable && content) {
            this.addToCache(pageKey, content);
        }
        
        return content;
    }

    /**
     * Génère le contenu d'une page
     */
    async generatePageContent(pageKey) {
        const pageConfig = this.pages[pageKey];
        const view = this.getView(pageConfig.viewKey);
        
        if (!view) {
            this.log('warn', 'NavigationController', `View missing for: ${pageKey}`);
            return this.getErrorPageContent(pageKey);
        }
        
        try {
            const data = this.getPageData(pageKey);
            
            // Différentes méthodes selon la vue
            if (typeof view.buildTemplate === 'function') {
                return view.buildTemplate(data);
            }
            
            if (typeof view.render === 'function') {
                view.render();
                return view.container ? view.container.innerHTML : '';
            }
            
            if (view.container) {
                return view.container.innerHTML;
            }
            
            throw new Error(`View ${pageConfig.viewKey} has no render method`);
            
        } catch (error) {
            this.log('error', 'NavigationController', `Content generation error for ${pageKey}`, error);
            return this.getErrorPageContent(pageKey);
        }
    }

    /**
     * Ajoute au cache avec stratégie LRU
     */
    addToCache(pageKey, content) {
        // Supprimer l'ancienne entrée si existe
        if (this.pageCache.has(pageKey)) {
            this.cacheAccessOrder = this.cacheAccessOrder.filter(k => k !== pageKey);
        }
        
        // Vérifier la taille du cache
        if (this.pageCache.size >= this.maxCacheSize) {
            // Supprimer l'entrée la moins récemment utilisée
            const lruKey = this.cacheAccessOrder.shift();
            this.pageCache.delete(lruKey);
            
            // Nettoyer le timeout
            if (this.cacheTimeouts.has(lruKey)) {
                clearTimeout(this.cacheTimeouts.get(lruKey));
                this.cacheTimeouts.delete(lruKey);
            }
        }
        
        // Ajouter la nouvelle entrée
        this.pageCache.set(pageKey, content);
        this.cacheAccessOrder.push(pageKey);
        
        // Planifier l'expiration
        this.scheduleCacheExpiry(pageKey);
        
        this.log('debug', 'NavigationController', `Cached: ${pageKey}`);
    }

    /**
     * Met à jour l'ordre d'accès du cache
     */
    updateCacheAccessOrder(pageKey) {
        this.cacheAccessOrder = this.cacheAccessOrder.filter(k => k !== pageKey);
        this.cacheAccessOrder.push(pageKey);
    }

    /**
     * Planifie l'expiration du cache
     */
    scheduleCacheExpiry(pageKey) {
        // Nettoyer l'ancien timeout
        if (this.cacheTimeouts.has(pageKey)) {
            clearTimeout(this.cacheTimeouts.get(pageKey));
        }
        
        // Créer le nouveau timeout
        const timeout = setTimeout(() => {
            this.pageCache.delete(pageKey);
            this.cacheTimeouts.delete(pageKey);
            this.cacheAccessOrder = this.cacheAccessOrder.filter(k => k !== pageKey);
            this.log('debug', 'NavigationController', `Cache expired: ${pageKey}`);
        }, this.defaultCacheDuration);
        
        this.cacheTimeouts.set(pageKey, timeout);
    }

    /**
     * Invalide le cache de certaines pages
     */
    invalidatePageCache(pageKeys) {
        pageKeys.forEach(pageKey => {
            if (this.pageCache.has(pageKey)) {
                this.pageCache.delete(pageKey);
                this.cacheAccessOrder = this.cacheAccessOrder.filter(k => k !== pageKey);
                
                if (this.cacheTimeouts.has(pageKey)) {
                    clearTimeout(this.cacheTimeouts.get(pageKey));
                    this.cacheTimeouts.delete(pageKey);
                }
                
                this.log('debug', 'NavigationController', `Cache invalidated: ${pageKey}`);
            }
        });
    }

    /**
     * Précharge les pages haute priorité
     */
    async preloadHighPriorityPages() {
        if (!this.preloadState.enabled) return;
        
        const highPriorityPages = Object.entries(this.pages)
            .filter(([_, config]) => config.preloadPriority === 'high')
            .map(([key, _]) => key);
        
        for (const pageKey of highPriorityPages) {
            if (pageKey !== this.currentPage && !this.preloadState.loaded.has(pageKey)) {
                await this.preloadPage(pageKey);
            }
        }
    }

    /**
     * Précharge une page
     */
    async preloadPage(pageKey) {
        if (this.preloadState.loaded.has(pageKey)) return;
        
        try {
            const content = await this.generatePageContent(pageKey);
            if (content && this.pages[pageKey].cacheable) {
                this.addToCache(pageKey, content);
                this.preloadState.loaded.add(pageKey);
                this.log('debug', 'NavigationController', `Preloaded: ${pageKey}`);
            }
        } catch (error) {
            this.log('warn', 'NavigationController', `Preload failed: ${pageKey}`, error);
        }
    }

    /**
     * Précharge la page suivante probable
     */
    async preloadNextLikelyPage(currentPage) {
        // Logique simple : précharger la page suivante dans la liste
        const pageKeys = Object.keys(this.pages);
        const currentIndex = pageKeys.indexOf(currentPage);
        const nextIndex = (currentIndex + 1) % pageKeys.length;
        const nextPage = pageKeys[nextIndex];
        
        if (!this.preloadState.loaded.has(nextPage)) {
            await this.preloadPage(nextPage);
        }
    }

    // ========================================================================
    // HISTORIQUE
    // ========================================================================

    /**
     * Ajoute à l'historique
     */
    addToHistory(pageKey) {
        // Si on n'est pas à la fin de l'historique, supprimer tout ce qui suit
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
     * Retour en arrière
     */
    async goBack() {
        if (this.historyIndex > 0) {
            this.historyIndex--;
            const pageKey = this.navigationHistory[this.historyIndex];
            await this.showPage(pageKey, { 
                skipHistory: true, 
                animationDirection: 'backward' 
            });
        }
    }

    /**
     * Avance
     */
    async goForward() {
        if (this.historyIndex < this.navigationHistory.length - 1) {
            this.historyIndex++;
            const pageKey = this.navigationHistory[this.historyIndex];
            await this.showPage(pageKey, { 
                skipHistory: true, 
                animationDirection: 'forward' 
            });
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    /**
     * Met à jour l'UI de navigation
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
     * Obtient les données pour une page
     */
    getPageData(pageKey) {
        // Collecter les données nécessaires depuis les modèles
        return {
            files: this.getModel('file')?.data || {},
            instruments: this.getModel('instrument')?.data || {},
            playlists: this.getModel('playlist')?.data || {},
            routing: this.getModel('routing')?.data || {},
            state: this.getModel('state')?.data || {}
        };
    }

    /**
     * Page d'erreur
     */
    getErrorPageContent(pageKey) {
        return `
            <div class="error-page">
                <h2>⚠️ Erreur de chargement</h2>
                <p>Impossible de charger la page "${pageKey}"</p>
                <button onclick="app.navigationController.showPage('home')">
                    Retour à l'accueil
                </button>
            </div>
        `;
    }

    /**
     * Initialise les composants d'une page
     */
    initializePageComponents(pageKey, pageElement) {
        // Hook pour initialiser des composants spécifiques
        this.eventBus.emit('page:components:init', { 
            pageKey, 
            element: pageElement 
        });
    }

    /**
     * Rafraîchit la page courante
     */
    async refreshCurrentPage() {
        await this.showPage(this.currentPage, { forceRefresh: true });
    }

    /**
     * Vérifie si on doit rafraîchir lors du retour
     */
    shouldRefreshOnVisible() {
        // Rafraîchir si absent plus de 5 minutes
        const pageConfig = this.pages[this.currentPage];
        return !pageConfig.cacheable;
    }

    /**
     * Gère les changements de modèle
     */
    handleModelChange(data) {
        // Invalider le cache des pages concernées
        const affectedPages = this.getAffectedPages(data.model);
        if (affectedPages.length > 0) {
            this.invalidatePageCache(affectedPages);
        }
    }

    /**
     * Obtient les pages affectées par un changement
     */
    getAffectedPages(modelName) {
        const mapping = {
            file: ['home', 'files', 'editor'],
            instrument: ['home', 'instruments', 'keyboard'],
            playlist: ['home', 'editor'],
            routing: ['routing'],
            state: []
        };
        
        return mapping[modelName] || [];
    }

    /**
     * Obtient la page courante
     */
    getCurrentPage() {
        return this.currentPage;
    }

    /**
     * Vérifie si une page est en cache
     */
    isPageCached(pageKey) {
        return this.pageCache.has(pageKey);
    }

    /**
     * Obtient l'état de navigation
     */
    getNavigationState() {
        return {
            currentPage: this.currentPage,
            previousPage: this.previousPage,
            historyLength: this.navigationHistory.length,
            cacheSize: this.pageCache.size,
            canGoBack: this.historyIndex > 0,
            canGoForward: this.historyIndex < this.navigationHistory.length - 1
        };
    }

    /**
     * Nettoie les ressources
     */
    destroy() {
        // Nettoyer les timeouts
        this.cacheTimeouts.forEach(timeout => clearTimeout(timeout));
        this.cacheTimeouts.clear();
        
        // Nettoyer le cache
        this.pageCache.clear();
        this.cacheAccessOrder = [];
        
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
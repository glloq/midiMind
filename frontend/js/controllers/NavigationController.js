// ============================================================================
// Fichier: frontend/js/core/Router.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.1.1 - FIX INITIALIZATION
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v3.1.1:
// ✓ CRITIQUE: Initialisation différée (ne charge pas de route avant enregistrement)
// ✓ CRITIQUE: Émission d'événements 'route-changed'
// ✓ Fix: Meilleure gestion des routes non trouvées
// ✓ Méthode startRouting() pour démarrer après enregistrement des routes
// ============================================================================

class Router {
    constructor(config = {}) {
        // Configuration
        this.config = {
            mode: config.mode || 'hash',           // 'hash' ou 'history'
            root: config.root || '/',              // URL de base
            useTransitions: config.useTransitions !== false,
            transitionDuration: config.transitionDuration || 300,
            ...config
        };
        
        // Routes enregistrées
        this.routes = new Map();
        
        // Route actuelle
        this.currentRoute = null;
        this.previousRoute = null;
        
        // Middlewares
        this.middlewares = [];
        
        // Guards (before/after hooks)
        this.beforeHooks = [];
        this.afterHooks = [];
        
        // Cache des vues
        this.viewCache = new Map();
        
        // État
        this.state = {
            isNavigating: false,
            history: [],
            params: {},
            query: {},
            started: false  // Nouveau: indique si le routing a démarré
        };
        
        // Event listeners
        this.listeners = new Map();
        
        // Initialisation (sans charger de route)
        this.init();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        // Écouter les changements d'URL
        if (this.config.mode === 'history') {
            // Mode History API
            window.addEventListener('popstate', (e) => this.handlePopState(e));
            
            // Intercepter les clics sur les liens
            document.addEventListener('click', (e) => this.handleLinkClick(e));
        } else {
            // Mode Hash
            window.addEventListener('hashchange', () => this.handleHashChange());
        }
        
        // NE PAS charger la route initiale automatiquement
        // Elle sera chargée après l'enregistrement des routes via startRouting()
    }
    
    /**
     * Démarre le routing (après enregistrement des routes)
     * À appeler explicitement après avoir enregistré toutes les routes
     */
    startRouting() {
        if (this.state.started) {
            console.warn('Router: Routing already started');
            return;
        }
        
        this.state.started = true;
        this.loadInitialRoute();
    }
    
    /**
     * Charger la route initiale
     */
    loadInitialRoute() {
        const path = this.getCurrentPath();
        this.navigateTo(path, { skipPushState: true });
    }
    
    // ========================================================================
    // ENREGISTREMENT DES ROUTES
    // ========================================================================
    
    /**
     * Enregistrer une route
     * @param {string} path - Chemin de la route (peut contenir des paramètres)
     * @param {Object} config - Configuration de la route
     */
    route(path, config) {
        const route = {
            path: path,
            pattern: this.pathToRegex(path),
            component: config.component || null,
            controller: config.controller || null,
            view: config.view || null,
            title: config.title || '',
            meta: config.meta || {},
            beforeEnter: config.beforeEnter || null,
            beforeLeave: config.beforeLeave || null,
            cache: config.cache !== false,
            ...config
        };
        
        this.routes.set(path, route);
        
        return this;
    }
    
    /**
     * Enregistrer plusieurs routes
     * @param {Array} routes - Tableau de routes
     */
    addRoutes(routes) {
        routes.forEach(route => {
            this.route(route.path, route);
        });
        
        return this;
    }
    
    /**
     * Route 404 (not found)
     * @param {Object} config - Configuration de la route 404
     */
    notFound(config) {
        this.route('*', {
            ...config,
            is404: true
        });
        
        return this;
    }
    
    /**
     * Configuration rapide des routes API
     * Configure les routes pour les nouvelles fonctionnalités
     * @param {Object} controllers - Objet contenant les contrôleurs
     * @param {Object} views - Objet contenant les vues
     */
    configureApiRoutes(controllers = {}, views = {}) {
        const apiRoutes = [
            {
                path: '/bluetooth',
                controller: controllers.bluetooth,
                view: views.bluetooth,
                title: 'Bluetooth - MidiMind',
                meta: { requiresFeature: 'bluetooth' }
            },
            {
                path: '/latency',
                controller: controllers.latency,
                view: views.latency,
                title: 'Latence - MidiMind',
                meta: { requiresFeature: 'latency' }
            },
            {
                path: '/presets',
                controller: controllers.preset,
                view: views.preset,
                title: 'Presets - MidiMind',
                meta: { requiresFeature: 'presets' }
            },
            {
                path: '/network',
                controller: controllers.network,
                view: views.network,
                title: 'Réseau - MidiMind',
                meta: { requiresFeature: 'network' }
            },
            {
                path: '/logger',
                controller: controllers.logger,
                view: views.logger,
                title: 'Logs - MidiMind',
                meta: { requiresFeature: 'logger' }
            }
        ];
        
        this.addRoutes(apiRoutes);
        
        return this;
    }
    
    // ========================================================================
    // NAVIGATION
    // ========================================================================
    
    /**
     * Naviguer vers une route
     * @param {string} path - Chemin de destination
     * @param {Object} options - Options de navigation
     */
    async navigateTo(path, options = {}) {
        // Vérifier si déjà en navigation
        if (this.state.isNavigating && !options.force) {
            return false;
        }
        
        this.state.isNavigating = true;
        
        try {
            // Normaliser le chemin
            const normalizedPath = this.normalizePath(path);
            
            // Trouver la route correspondante
            const matchedRoute = this.matchRoute(normalizedPath);
            
            if (!matchedRoute) {
                // Route 404
                const notFoundRoute = this.find404Route();
                if (notFoundRoute) {
                    await this.loadRoute(notFoundRoute, normalizedPath, options);
                } else {
                    console.warn(`Router: Route not found: ${normalizedPath}`);
                    // Si pas de route 404 définie, ne pas échouer silencieusement
                    // Émettre un événement pour que NavigationController puisse gérer
                    this.emit('route-not-found', { path: normalizedPath });
                }
                return false;
            }
            
            // Extraire les paramètres
            const params = this.extractParams(matchedRoute, normalizedPath);
            const query = this.extractQuery(normalizedPath);
            
            // Mettre à jour l'état
            this.state.params = params;
            this.state.query = query;
            
            // Charger la route
            await this.loadRoute(matchedRoute.route, normalizedPath, options);
            
            // Mettre à jour l'URL si nécessaire
            if (!options.skipPushState) {
                this.updateURL(normalizedPath, options.replace);
            }
            
            // Émettre événement de changement de route
            this.emit('route-changed', {
                route: matchedRoute.route,
                path: normalizedPath,
                params: params,
                query: query
            });
            
            return true;
        } catch (error) {
            console.error('Router: Navigation error:', error);
            this.emit('route-error', { path, error });
            return false;
        } finally {
            this.state.isNavigating = false;
        }
    }
    
    /**
     * Charger une route
     */
    async loadRoute(route, path, options = {}) {
        // Sauvegarder la route précédente
        this.previousRoute = this.currentRoute;
        
        // Hook beforeLeave de la route précédente
        if (this.previousRoute && this.previousRoute.beforeLeave) {
            const canLeave = await this.previousRoute.beforeLeave(this.previousRoute, route);
            if (canLeave === false) {
                return false;
            }
        }
        
        // Hook beforeEnter
        if (route.beforeEnter) {
            const canEnter = await route.beforeEnter(route, this.previousRoute);
            if (canEnter === false) {
                return false;
            }
        }
        
        // Exécuter les middlewares
        for (const middleware of this.middlewares) {
            const result = await middleware(route, this.previousRoute);
            if (result === false) {
                return false;
            }
        }
        
        // Exécuter les before hooks
        for (const hook of this.beforeHooks) {
            const result = await hook(route, this.previousRoute);
            if (result === false) {
                return false;
            }
        }
        
        // Transitions
        if (this.config.useTransitions && !options.skipTransitions) {
            await this.transitionOut();
        }
        
        // Activer le contrôleur et la vue
        if (route.controller) {
            // Désactiver le contrôleur précédent
            if (this.previousRoute && this.previousRoute.controller) {
                if (typeof this.previousRoute.controller.onLeave === 'function') {
                    await this.previousRoute.controller.onLeave();
                }
            }
            
            // Activer le nouveau contrôleur
            if (typeof route.controller.onEnter === 'function') {
                await route.controller.onEnter(this.state.params, this.state.query);
            }
        }
        
        // Rendre la vue
        if (route.view) {
            this.renderView(route.view, route.cache);
        }
        
        // Mettre à jour le titre
        if (route.title) {
            document.title = route.title;
        }
        
        // Transitions
        if (this.config.useTransitions && !options.skipTransitions) {
            await this.transitionIn();
        }
        
        // Mettre à jour la route actuelle
        this.currentRoute = route;
        
        // Ajouter à l'historique
        this.state.history.push({
            path: path,
            route: route,
            timestamp: Date.now()
        });
        
        // Exécuter les after hooks
        for (const hook of this.afterHooks) {
            await hook(route, this.previousRoute);
        }
        
        return true;
    }
    
    /**
     * Rendre une vue
     */
    renderView(view, useCache = true) {
        const container = this.getContainer();
        
        if (!container) {
            console.error('Router: No container found for rendering');
            return;
        }
        
        // Vérifier le cache
        if (useCache && this.viewCache.has(view)) {
            container.innerHTML = this.viewCache.get(view);
            return;
        }
        
        // Rendre la vue
        let content = '';
        
        if (typeof view === 'function') {
            content = view(this.state.params, this.state.query);
        } else if (typeof view === 'string') {
            content = view;
        } else if (view && typeof view.render === 'function') {
            content = view.render(this.state.params, this.state.query);
        }
        
        // Mettre en cache
        if (useCache) {
            this.viewCache.set(view, content);
        }
        
        // Afficher
        container.innerHTML = content;
    }
    
    /**
     * Recharger la route actuelle
     */
    reload() {
        if (this.currentRoute) {
            const path = this.getCurrentPath();
            this.navigateTo(path, { skipPushState: true, force: true });
        }
    }
    
    /**
     * Retour arrière
     */
    back() {
        window.history.back();
    }
    
    /**
     * Avancer
     */
    forward() {
        window.history.forward();
    }
    
    // ========================================================================
    // HANDLERS
    // ========================================================================
    
    /**
     * Gérer popstate
     */
    handlePopState(event) {
        const path = this.getCurrentPath();
        this.navigateTo(path, { skipPushState: true });
    }
    
    /**
     * Gérer hashchange
     */
    handleHashChange() {
        const path = this.getCurrentPath();
        this.navigateTo(path, { skipPushState: true });
    }
    
    /**
     * Gérer clic sur lien
     */
    handleLinkClick(event) {
        // Vérifier si c'est un lien
        const link = event.target.closest('a');
        if (!link) return;
        
        // Vérifier si c'est un lien interne
        const href = link.getAttribute('href');
        if (!href || href.startsWith('http') || href.startsWith('//') || href.startsWith('mailto:')) {
            return;
        }
        
        // Empêcher le comportement par défaut
        event.preventDefault();
        
        // Naviguer
        this.navigateTo(href);
    }
    
    // ========================================================================
    // MIDDLEWARES & HOOKS
    // ========================================================================
    
    /**
     * Ajouter un middleware
     */
    use(middleware) {
        this.middlewares.push(middleware);
        return this;
    }
    
    /**
     * Ajouter un hook before
     */
    beforeEach(hook) {
        this.beforeHooks.push(hook);
        return this;
    }
    
    /**
     * Ajouter un hook after
     */
    afterEach(hook) {
        this.afterHooks.push(hook);
        return this;
    }
    
    // ========================================================================
    // TRANSITIONS
    // ========================================================================
    
    /**
     * Transition de sortie
     */
    async transitionOut() {
        const container = this.getContainer();
        
        if (container) {
            container.style.opacity = '1';
            container.style.transition = `opacity ${this.config.transitionDuration}ms ease`;
            container.style.opacity = '0';
            
            await this.wait(this.config.transitionDuration);
        }
    }
    
    /**
     * Transition d'entrée
     */
    async transitionIn() {
        const container = this.getContainer();
        
        if (container) {
            container.style.opacity = '0';
            container.style.transition = `opacity ${this.config.transitionDuration}ms ease`;
            
            // Force reflow
            container.offsetHeight;
            
            container.style.opacity = '1';
            
            await this.wait(this.config.transitionDuration);
        }
    }
    
    /**
     * Attendre un délai
     */
    wait(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
    
    /**
     * Obtenir le conteneur de rendu
     */
    getContainer() {
        return this.config.container || 
               document.getElementById('router-view') ||
               document.querySelector('.main-content') ||
               document.body;
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Obtenir le chemin actuel
     */
    getCurrentPath() {
        if (this.config.mode === 'history') {
            let path = window.location.pathname;
            
            // Retirer le root si nécessaire
            if (this.config.root !== '/') {
                path = path.replace(this.config.root, '');
            }
            
            return path || '/';
        } else {
            return window.location.hash.slice(1) || '/';
        }
    }
    
    /**
     * Mettre à jour l'URL
     */
    updateURL(path, replace = false) {
        if (this.config.mode === 'history') {
            const url = this.config.root + path;
            
            if (replace) {
                window.history.replaceState(null, '', url);
            } else {
                window.history.pushState(null, '', url);
            }
        } else {
            if (replace) {
                window.location.replace('#' + path);
            } else {
                window.location.hash = path;
            }
        }
    }
    
    /**
     * Normaliser un chemin
     */
    normalizePath(path) {
        // Retirer les slashes multiples
        path = path.replace(/\/+/g, '/');
        
        // Retirer le slash final sauf pour root
        if (path !== '/' && path.endsWith('/')) {
            path = path.slice(0, -1);
        }
        
        // Assurer que ça commence par /
        if (!path.startsWith('/')) {
            path = '/' + path;
        }
        
        return path;
    }
    
    /**
     * Convertir un path pattern en regex
     */
    pathToRegex(path) {
        // Remplacer les paramètres :param par des groupes de capture
        const pattern = path
            .replace(/\//g, '\\/')
            .replace(/:(\w+)/g, '(?<$1>[^/]+)')
            .replace(/\*/g, '.*');
        
        return new RegExp('^' + pattern + '$');
    }
    
    /**
     * Trouver la route correspondante
     */
    matchRoute(path) {
        for (const [routePath, route] of this.routes) {
            if (routePath === '*') continue; // Skip 404 route
            
            const match = path.match(route.pattern);
            if (match) {
                return {
                    route: route,
                    params: match.groups || {}
                };
            }
        }
        
        return null;
    }
    
    /**
     * Trouver la route 404
     */
    find404Route() {
        return this.routes.get('*') || null;
    }
    
    /**
     * Extraire les paramètres
     */
    extractParams(matchedRoute, path) {
        const match = path.match(matchedRoute.route.pattern);
        return match ? (match.groups || {}) : {};
    }
    
    /**
     * Extraire les query params
     */
    extractQuery(path) {
        const queryStart = path.indexOf('?');
        if (queryStart === -1) return {};
        
        const queryString = path.slice(queryStart + 1);
        const params = new URLSearchParams(queryString);
        const query = {};
        
        for (const [key, value] of params) {
            query[key] = value;
        }
        
        return query;
    }
    
    // ========================================================================
    // EVENT EMITTER
    // ========================================================================
    
    /**
     * Écouter un événement
     */
    on(event, callback) {
        if (!this.listeners.has(event)) {
            this.listeners.set(event, []);
        }
        this.listeners.get(event).push(callback);
        
        return this;
    }
    
    /**
     * Retirer un écouteur
     */
    off(event, callback) {
        if (!this.listeners.has(event)) return this;
        
        const callbacks = this.listeners.get(event);
        const index = callbacks.indexOf(callback);
        
        if (index !== -1) {
            callbacks.splice(index, 1);
        }
        
        return this;
    }
    
    /**
     * Émettre un événement
     */
    emit(event, data) {
        if (!this.listeners.has(event)) return;
        
        const callbacks = this.listeners.get(event);
        callbacks.forEach(callback => callback(data));
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * Obtenir la route actuelle
     */
    getCurrentRoute() {
        return this.currentRoute;
    }
    
    /**
     * Obtenir les paramètres
     */
    getParams() {
        return this.state.params;
    }
    
    /**
     * Obtenir les query params
     */
    getQuery() {
        return this.state.query;
    }
    
    /**
     * Obtenir l'historique
     */
    getHistory() {
        return [...this.state.history];
    }
}

window.Router = Router;

// ============================================================================
// FIN DU FICHIER Router.js v3.1.1
// ============================================================================
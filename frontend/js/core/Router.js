// ============================================================================
// Fichier: frontend/js/core/Router.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Routeur pour navigation SPA (Single Page Application).
//   Gère les routes, l'historique et la navigation sans rechargement.
//
// Auteur: midiMind Team
// Date: 2025-10-04
// Version: 3.0.0
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
            query: {}
        };
        
        // Event listeners
        this.listeners = new Map();
        
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
        
        // Charger la route initiale
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
                    console.error(`Route not found: ${normalizedPath}`);
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
            
            return true;
            
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
        
        // Exécuter les hooks beforeLeave de la route précédente
        if (this.previousRoute && this.previousRoute.beforeLeave) {
            const canLeave = await this.executeHook(this.previousRoute.beforeLeave, this.previousRoute);
            if (canLeave === false) {
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
        
        // Exécuter les beforeHooks globaux
        for (const hook of this.beforeHooks) {
            const result = await hook(route, this.previousRoute);
            if (result === false) {
                return false;
            }
        }
        
        // Exécuter le beforeEnter de la nouvelle route
        if (route.beforeEnter) {
            const canEnter = await this.executeHook(route.beforeEnter, route);
            if (canEnter === false) {
                return false;
            }
        }
        
        // Transition si activée
        if (this.config.useTransitions && !options.skipTransition) {
            await this.transitionOut();
        }
        
        // Charger le composant/vue
        await this.loadComponent(route);
        
        // Mettre à jour le titre
        if (route.title) {
            document.title = typeof route.title === 'function' ? 
                route.title(this.state.params) : route.title;
        }
        
        // Mettre à jour la route actuelle
        this.currentRoute = {
            ...route,
            path: path,
            params: this.state.params,
            query: this.state.query
        };
        
        // Ajouter à l'historique
        this.state.history.push({
            path: path,
            timestamp: Date.now()
        });
        
        // Limiter la taille de l'historique
        if (this.state.history.length > 50) {
            this.state.history.shift();
        }
        
        // Transition si activée
        if (this.config.useTransitions && !options.skipTransition) {
            await this.transitionIn();
        }
        
        // Exécuter les afterHooks
        for (const hook of this.afterHooks) {
            await hook(route, this.previousRoute);
        }
        
        // Émettre l'événement de changement de route
        this.emit('route:changed', {
            current: this.currentRoute,
            previous: this.previousRoute
        });
        
        return true;
    }
    
    /**
     * Charger le composant d'une route
     */
    async loadComponent(route) {
        // Vérifier le cache
        if (route.cache && this.viewCache.has(route.path)) {
            const cached = this.viewCache.get(route.path);
            this.renderComponent(cached);
            return;
        }
        
        // Charger le composant
        if (route.component) {
            // Si c'est une fonction, l'exécuter
            if (typeof route.component === 'function') {
                const component = await route.component(this.state.params, this.state.query);
                this.renderComponent(component);
                
                if (route.cache) {
                    this.viewCache.set(route.path, component);
                }
            } 
            // Si c'est un string (nom de classe ou HTML)
            else if (typeof route.component === 'string') {
                this.renderComponent(route.component);
            }
        }
        
        // Exécuter le contrôleur si défini
        if (route.controller) {
            if (typeof route.controller === 'function') {
                await route.controller(this.state.params, this.state.query);
            } else if (typeof route.controller === 'string') {
                // Nom du contrôleur à instancier
                const ControllerClass = window[route.controller];
                if (ControllerClass) {
                    new ControllerClass(this.state.params, this.state.query);
                }
            }
        }
    }
    
    /**
     * Rendre un composant
     */
    renderComponent(component) {
        const container = this.getContainer();
        
        if (!container) {
            console.error('Router: No container found for rendering');
            return;
        }
        
        // Si c'est du HTML
        if (typeof component === 'string') {
            container.innerHTML = component;
        }
        // Si c'est un élément DOM
        else if (component instanceof HTMLElement) {
            container.innerHTML = '';
            container.appendChild(component);
        }
        // Si c'est un objet avec une méthode render
        else if (component && typeof component.render === 'function') {
            component.render(container);
        }
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
    // NAVIGATION HELPERS
    // ========================================================================
    
    /**
     * Aller à la page précédente
     */
    back() {
        if (this.config.mode === 'history') {
            window.history.back();
        } else {
            if (this.state.history.length > 1) {
                const previous = this.state.history[this.state.history.length - 2];
                this.navigateTo(previous.path);
            }
        }
    }
    
    /**
     * Aller à la page suivante
     */
    forward() {
        if (this.config.mode === 'history') {
            window.history.forward();
        }
    }
    
    /**
     * Recharger la route actuelle
     */
    reload() {
        if (this.currentRoute) {
            this.navigateTo(this.currentRoute.path, { 
                force: true, 
                skipPushState: true 
            });
        }
    }
    
    /**
     * Naviguer avec remplacement
     */
    replace(path) {
        this.navigateTo(path, { replace: true });
    }
    
    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================
    
    /**
     * Gérer le popstate (boutons précédent/suivant)
     */
    handlePopState(event) {
        const path = this.getCurrentPath();
        this.navigateTo(path, { skipPushState: true });
    }
    
    /**
     * Gérer le changement de hash
     */
    handleHashChange() {
        const path = this.getCurrentPath();
        this.navigateTo(path, { skipPushState: true });
    }
    
    /**
     * Intercepter les clics sur les liens
     */
    handleLinkClick(event) {
        // Vérifier si c'est un lien
        const link = event.target.closest('a');
        if (!link) return;
        
        // Vérifier les attributs
        const href = link.getAttribute('href');
        if (!href || href.startsWith('#') || href.startsWith('http') || 
            link.hasAttribute('download') || link.getAttribute('target') === '_blank') {
            return;
        }
        
        // Vérifier si c'est une route interne
        if (link.hasAttribute('data-router') || link.classList.contains('router-link')) {
            event.preventDefault();
            this.navigateTo(href);
        }
    }
    
    // ========================================================================
    // MIDDLEWARES ET GUARDS
    // ========================================================================
    
    /**
     * Ajouter un middleware
     */
    use(middleware) {
        if (typeof middleware === 'function') {
            this.middlewares.push(middleware);
        }
        return this;
    }
    
    /**
     * Ajouter un hook before
     */
    beforeEach(hook) {
        if (typeof hook === 'function') {
            this.beforeHooks.push(hook);
        }
        return this;
    }
    
    /**
     * Ajouter un hook after
     */
    afterEach(hook) {
        if (typeof hook === 'function') {
            this.afterHooks.push(hook);
        }
        return this;
    }
    
    /**
     * Exécuter un hook
     */
    async executeHook(hook, context) {
        try {
            const result = await hook(context, this);
            return result;
        } catch (error) {
            console.error('Router: Hook execution error', error);
            return false;
        }
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
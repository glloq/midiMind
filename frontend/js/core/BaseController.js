/* ======================================================================================
   CONTRÔLEUR DE BASE - PATTERN CONTROLLER DU MVC
   ======================================================================================
   Fichier: frontend/js/core/BaseController.js
   Version: v3.3.0 - FIXED EVENTBUS FALLBACKS
   Date: 2025-10-31
   ======================================================================================
   CORRECTIONS v3.3.0:
   ✅ CRITIQUE: Fallback robuste pour eventBus (window.eventBus)
   ✅ Validation eventBus avant toute utilisation
   ✅ Signature cohérente maintenue
   ✅ Protection contre eventBus null
   ====================================================================================== */

class BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null) {
        // ✅ CRITIQUE: EventBus avec fallback robuste
        this.eventBus = eventBus || window.eventBus || null;
        
        // Validation EventBus
        if (!this.eventBus) {
            console.error(`[${this.constructor.name}] CRITIQUE: EventBus non disponible!`);
            // Créer un fallback minimal pour éviter les crashes
            this.eventBus = {
                on: () => () => {},
                once: () => () => {},
                emit: () => {},
                off: () => {}
            };
        }
        
        // Références aux composants principaux
        this.models = models;
        this.views = views;
        this.notifications = notifications;
        this.debugConsole = debugConsole;
        
        // État du contrôleur
        this.state = {
            isInitialized: false,
            isActive: false,
            isDestroyed: false,
            lastAction: null,
            errors: []
        };
        
        // Configuration
        this.config = {
            autoInitialize: true,
            handleErrors: true,
            logActions: true,
            validateInputs: true,
            debounceActions: {},
            cacheTTL: 5 * 60 * 1000 // 5 minutes
        };
        
        // Gestion des événements
        this.eventSubscriptions = [];
        this.actionQueue = [];
        
        // Métriques et monitoring
        this.metrics = {
            actionsExecuted: 0,
            errorsHandled: 0,
            notificationsSent: 0,
            startTime: Date.now()
        };
        
        // Cache pour optimisation
        this.cache = new Map();
        this.cacheTimestamps = new Map();
        this._lastCacheClean = null;
		
        // Validateurs d'entrée
        this.validators = {};
        
        // Actions debouncées
        this.debouncedActions = new Map();
        
        // Initialisation automatique si configurée
        if (this.config.autoInitialize) {
            this.initialize();
        }
    }

    /**
     * Initialiser le contrôleur
     */
    initialize() {
        if (this.state.isInitialized) {
            this.logDebug('warning', `Contrôleur ${this.constructor.name} déjà initialisé`);
            return;
        }
        
        try {
            // Exécuter l'initialisation personnalisée
            this.onInitialize();
            
            // Lier les événements
            this.bindEvents();
            
            // Configurer les actions debouncées
            this.setupDebouncedActions();
            
            // Marquer comme initialisé
            this.state.isInitialized = true;
            this.state.isActive = true;
            
            this.logDebug('system', `Contrôleur ${this.constructor.name} initialisé`);
            this.emitEvent('controller:initialized', {
                controller: this.constructor.name
            });
            
        } catch (error) {
            this.handleError('Erreur lors de l\'initialisation', error);
        }
    }

    /**
     * Hook d'initialisation personnalisée (à surcharger)
     */
    onInitialize() {
        // À implémenter dans les classes filles
    }

    /**
     * Lier les événements du contrôleur (à surcharger)
     */
    bindEvents() {
        // À surcharger dans les classes filles
        this.logDebug('info', `Liaison des événements pour ${this.constructor.name}`);
    }

    /**
     * S'abonner à un événement avec gestion automatique
     * @param {string} event - Nom de l'événement
     * @param {function} handler - Gestionnaire
     * @param {Object} options - Options
     * @returns {function} Fonction de désabonnement
     */
    subscribe(event, handler, options = {}) {
        if (!this.eventBus || !this.eventBus.on) {
            console.error(`[${this.constructor.name}] Cannot subscribe: EventBus not available`);
            return () => {};
        }
        
        const opts = {
            once: false,
            validate: true,
            debounce: 0,
            ...options
        };
        
        // Wrapper du gestionnaire avec gestion d'erreur et validation
        const wrappedHandler = (data) => {
            try {
                // Validation optionnelle des données
                if (opts.validate && !this.validateEventData(event, data)) {
                    this.logDebug('warning', `Données invalides pour l'événement ${event}`);
                    return;
                }
                
                // Exécution du gestionnaire
                handler.call(this, data);
                
            } catch (error) {
                this.handleError(`Erreur dans gestionnaire pour ${event}`, error);
            }
        };
        
        // Debouncing si spécifié
        const finalHandler = opts.debounce > 0 
            ? this.debounce(wrappedHandler, opts.debounce)
            : wrappedHandler;
        
        // S'abonner à l'événement
        const unsubscribe = opts.once
            ? this.eventBus.once(event, finalHandler)
            : this.eventBus.on(event, finalHandler);
        
        // Garder trace pour nettoyage
        this.eventSubscriptions.push({
            event,
            unsubscribe,
            handler: finalHandler
        });
        
        return unsubscribe;
    }

    /**
     * Émettre un événement
     * @param {string} event - Nom de l'événement
     * @param {*} data - Données de l'événement
     */
    emitEvent(event, data) {
        if (!this.eventBus || !this.eventBus.emit) {
            console.warn(`[${this.constructor.name}] Cannot emit event: EventBus not available`);
            return;
        }
        
        try {
            this.eventBus.emit(event, data);
        } catch (error) {
            this.logDebug('error', `Erreur lors de l'émission de l'événement ${event}:`, error);
        }
    }

    /**
     * Exécuter une action avec gestion d'erreur et logging
     * @param {string} actionName - Nom de l'action
     * @param {function} action - Fonction d'action
     * @param {*} data - Données pour l'action
     * @param {Object} options - Options d'exécution
     * @returns {*} Résultat de l'action
     */
    async executeAction(actionName, action, data = null, options = {}) {
        const opts = {
            validate: this.config.validateInputs,
            log: this.config.logActions,
            notify: false,
            cache: false,
            timeout: 0,
            retry: 0,
            ...options
        };
        
        // Vérifier si le contrôleur est actif
        if (!this.state.isActive) {
            throw new Error(`Contrôleur ${this.constructor.name} n'est pas actif`);
        }
        
        // Validation des données d'entrée
        if (opts.validate && !this.validateActionData(actionName, data)) {
            throw new Error(`Données invalides pour l'action ${actionName}`);
        }
        
        // Nettoyer cache si nécessaire
        if (opts.cache && this.shouldCleanCache()) {
            this.cleanCache();
            this._lastCacheClean = Date.now();
        }
        
        // Vérifier cache
        if (opts.cache) {
            const cached = this.getCached(actionName);
            if (cached !== null) {
                this.logDebug('debug', `Cache hit for ${actionName}`);
                return cached;
            }
        }
        
        const startTime = performance.now();
        let attempts = 0;
        let lastError = null;
        
        // Boucle de retry
        do {
            attempts++;
            try {
                // Logging de l'action
                if (opts.log) {
                    this.logDebug('action', `Exécution: ${actionName}`, data);
                }
                
                // Exécution avec timeout optionnel
                let result;
                if (opts.timeout > 0) {
                    result = await this.executeWithTimeout(action, data, opts.timeout);
                } else {
                    result = await action.call(this, data);
                }
                
                // Mise en cache du résultat
                if (opts.cache && result !== undefined) {
                    const cacheKey = `${actionName}:${JSON.stringify(data)}`;
                    this.cache.set(cacheKey, result);
                    this.cacheTimestamps.set(cacheKey, Date.now());
                }
                
                // Métriques
                this.metrics.actionsExecuted++;
                this.state.lastAction = {
                    name: actionName,
                    timestamp: new Date().toISOString(),
                    duration: performance.now() - startTime,
                    success: true
                };
                
                // Notification de succès
                if (opts.notify && opts.notify.success) {
                    this.showNotification(opts.notify.success, 'success');
                }
                
                return result;
                
            } catch (error) {
                lastError = error;
                
                // Retry si demandé
                if (attempts <= opts.retry) {
                    this.logDebug('warning', `Retry ${attempts}/${opts.retry} pour ${actionName}`);
                    await this.sleep(Math.min(1000 * attempts, 5000)); // Backoff exponentiel
                    continue;
                }
                
                // Logging de l'erreur
                this.logDebug('error', `Erreur dans action ${actionName}:`, error);
                
                // Métriques
                this.metrics.errorsHandled++;
                this.state.lastAction = {
                    name: actionName,
                    timestamp: new Date().toISOString(),
                    duration: performance.now() - startTime,
                    success: false,
                    error: error.message
                };
                
                // Notification d'erreur
                if (opts.notify && opts.notify.error) {
                    this.showNotification(opts.notify.error, 'error');
                }
                
                // Gérer l'erreur ou la propager
                if (this.config.handleErrors) {
                    this.handleError(actionName, error);
                } else {
                    throw error;
                }
            }
        } while (attempts <= opts.retry);
        
        // Si on arrive ici, toutes les tentatives ont échoué
        throw lastError;
    }

    /**
     * Valider les données d'un événement
     * @param {string} event - Nom de l'événement
     * @param {*} data - Données à valider
     * @returns {boolean} True si valide
     */
    validateEventData(event, data) {
        // Implémentation basique - à surcharger si nécessaire
        return true;
    }

    /**
     * Valider les données d'une action
     * @param {string} action - Nom de l'action
     * @param {*} data - Données à valider
     * @returns {boolean} True si valide
     */
    validateActionData(action, data) {
        if (this.validators[action]) {
            return this.validators[action](data);
        }
        return true;
    }

    /**
     * Ajouter un validateur pour une action
     * @param {string} action - Nom de l'action
     * @param {function} validator - Fonction de validation
     */
    addValidator(action, validator) {
        this.validators[action] = validator;
    }

    /**
     * Gérer une erreur
     * @param {string} context - Contexte de l'erreur
     * @param {Error} error - Erreur
     */
    handleError(context, error) {
        const errorInfo = {
            context,
            message: error.message,
            stack: error.stack,
            timestamp: new Date().toISOString()
        };
        
        this.state.errors.push(errorInfo);
        this.logDebug('error', `[${context}]`, error);
        
        // Émettre un événement d'erreur
        this.emitEvent('controller:error', errorInfo);
    }

    /**
     * Afficher une notification
     * @param {string} message - Message
     * @param {string} type - Type (success, error, warning, info)
     */
    showNotification(message, type = 'info') {
        this.metrics.notificationsSent++;
        
        if (this.notifications && typeof this.notifications.show === 'function') {
            this.notifications.show(message, type);
        } else {
            // Fallback console
            console.log(`[${type.toUpperCase()}] ${message}`);
        }
    }

    showSuccess(message) {
        this.showNotification(message, 'success');
    }

    showError(message) {
        this.showNotification(message, 'error');
    }

    showWarning(message) {
        this.showNotification(message, 'warning');
    }

    showInfo(message) {
        this.showNotification(message, 'info');
    }

    /**
     * Logger avec niveau de debug
     * @param {string} level - Niveau (debug, info, warning, error, action, system)
     * @param {...*} args - Arguments à logger
     */
    logDebug(level, ...args) {
        if (this.debugConsole && typeof this.debugConsole.log === 'function') {
            this.debugConsole.log(level, ...args);
        } else {
            const prefix = `[${this.constructor.name}]`;
            switch (level) {
                case 'error':
                    console.error(prefix, ...args);
                    break;
                case 'warning':
                    console.warn(prefix, ...args);
                    break;
                default:
                    console.log(prefix, ...args);
            }
        }
    }

    /**
     * Configurer les actions debouncées
     */
    setupDebouncedActions() {
        for (const [action, delay] of Object.entries(this.config.debounceActions)) {
            if (typeof this[action] === 'function') {
                const original = this[action];
                this.debouncedActions.set(action, this.debounce(original.bind(this), delay));
            }
        }
    }

    /**
     * Obtenir l'état du contrôleur
     * @returns {Object} État
     */
    getState() {
        return {
            ...this.state,
            metrics: { ...this.metrics }
        };
    }

    /**
     * Obtenir les métriques du contrôleur
     * @returns {Object} Métriques
     */
    getMetrics() {
        return {
            ...this.metrics,
            uptime: Date.now() - this.metrics.startTime,
            cacheSize: this.cache.size
        };
    }

    /**
     * Obtenir une valeur du cache
     * @param {string} key - Clé
     * @returns {*} Valeur ou null
     */
    getCached(key) {
        if (!this.cache.has(key)) {
            return null;
        }
        
        const timestamp = this.cacheTimestamps.get(key);
        if (timestamp && Date.now() - timestamp > this.config.cacheTTL) {
            this.cache.delete(key);
            this.cacheTimestamps.delete(key);
            return null;
        }
        
        return this.cache.get(key);
    }

    /**
     * Mettre une valeur en cache
     * @param {string} key - Clé
     * @param {*} value - Valeur
     */
    setCached(key, value) {
        this.cache.set(key, value);
        this.cacheTimestamps.set(key, Date.now());
    }

    /**
     * Nettoyer le cache (tout ou par pattern)
     * @param {string|RegExp} pattern - Pattern optionnel
     */
    clearCache(pattern = null) {
        if (pattern) {
            const regex = typeof pattern === 'string' ? new RegExp(pattern) : pattern;
            for (const key of this.cache.keys()) {
                if (regex.test(key)) {
                    this.cache.delete(key);
                    this.cacheTimestamps.delete(key);
                }
            }
        } else {
            this.cache.clear();
            this.cacheTimestamps.clear();
        }
        
        this.logDebug('info', `Cache nettoyé${pattern ? ` (pattern: ${pattern})` : ''}`);
    }

    /**
     * Nettoie les entrées de cache expirées
     * @private
     */
    cleanCache() {
        const now = Date.now();
        
        for (const [key, timestamp] of this.cacheTimestamps.entries()) {
            if (now - timestamp > this.config.cacheTTL) {
                this.cache.delete(key);
                this.cacheTimestamps.delete(key);
                this.logDebug('debug', `Cache entry expired: ${key}`);
            }
        }
    }

    /**
     * Vérifie si le cache doit être nettoyé
     * @private
     */
    shouldCleanCache() {
        const now = Date.now();
        const fiveMinutes = 5 * 60 * 1000;
        
        return this.cache.size > 100 || 
               (this._lastCacheClean && now - this._lastCacheClean > fiveMinutes);
    }

    /**
     * Activer le contrôleur
     */
    activate() {
        this.state.isActive = true;
        this.logDebug('system', `Contrôleur ${this.constructor.name} activé`);
        this.emitEvent('controller:activated', { controller: this.constructor.name });
    }

    /**
     * Désactiver le contrôleur
     */
    deactivate() {
        this.state.isActive = false;
        this.logDebug('system', `Contrôleur ${this.constructor.name} désactivé`);
        this.emitEvent('controller:deactivated', { controller: this.constructor.name });
    }

    /**
     * Détruire le contrôleur et nettoyer les ressources
     */
    destroy() {
        if (this.state.isDestroyed) {
            return;
        }
        
        // Hook de destruction personnalisé
        if (typeof this.onDestroy === 'function') {
            try {
                this.onDestroy();
            } catch (error) {
                this.logDebug('error', 'Erreur lors du hook onDestroy:', error);
            }
        }
        
        // Nettoyer les abonnements aux événements
        this.eventSubscriptions.forEach(({ unsubscribe }) => {
            if (typeof unsubscribe === 'function') {
                unsubscribe();
            }
        });
        this.eventSubscriptions = [];
        
        // Nettoyer le cache
        this.cache.clear();
        this.cacheTimestamps.clear();
        
        // Nettoyer les actions debouncées
        this.debouncedActions.clear();
        
        // Marquer comme détruit
        this.state.isDestroyed = true;
        this.state.isActive = false;
        
        this.logDebug('system', `Contrôleur ${this.constructor.name} détruit`);
        this.emitEvent('controller:destroyed', { controller: this.constructor.name });
    }

    // ===== MÉTHODES UTILITAIRES =====

    /**
     * Créer une fonction debouncée
     * @param {function} func - Fonction à debouncer
     * @param {number} delay - Délai en ms
     * @returns {function} Fonction debouncée
     */
    debounce(func, delay) {
        let timeoutId;
        return function (...args) {
            clearTimeout(timeoutId);
            timeoutId = setTimeout(() => func.apply(this, args), delay);
        };
    }

    /**
     * Créer une fonction throttlée
     * @param {function} func - Fonction à throttler
     * @param {number} delay - Délai en ms
     * @returns {function} Fonction throttlée
     */
    throttle(func, delay) {
        let lastCall = 0;
        return function (...args) {
            const now = Date.now();
            if (now - lastCall >= delay) {
                lastCall = now;
                return func.apply(this, args);
            }
        };
    }

    /**
     * Exécuter une fonction avec timeout
     * @param {function} func - Fonction à exécuter
     * @param {*} data - Données
     * @param {number} timeout - Timeout en ms
     * @returns {Promise} Résultat ou timeout
     */
    executeWithTimeout(func, data, timeout) {
        return Promise.race([
            func.call(this, data),
            new Promise((_, reject) => 
                setTimeout(() => reject(new Error('Timeout')), timeout)
            )
        ]);
    }

    /**
     * Attendre un délai
     * @param {number} ms - Délai en ms
     * @returns {Promise} Promise qui se résout après le délai
     */
    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    /**
     * Formater une durée en millisecondes
     * @param {number} ms - Millisecondes
     * @returns {string} Durée formatée
     */
    formatDuration(ms) {
        if (ms < 1000) return `${ms}ms`;
        if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
        return `${(ms / 60000).toFixed(1)}min`;
    }

    /**
     * Générer un ID unique
     * @returns {string} ID unique
     */
    generateId() {
        return `${this.constructor.name.toLowerCase()}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    }

    /**
     * Obtenir un modèle de manière sécurisée
     * @param {string} name - Nom du modèle
     * @returns {Object|null} Modèle ou null
     */
    getModel(name) {
        if (!this.models || typeof this.models !== 'object') {
            return null;
        }
        return this.models[name] || null;
    }

    /**
     * Obtenir une vue de manière sécurisée
     * @param {string} name - Nom de la vue
     * @returns {Object|null} Vue ou null
     */
    getView(name) {
        if (!this.views || typeof this.views !== 'object') {
            return null;
        }
        return this.views[name] || null;
    }

    /**
     * Définir un modèle
     * @param {string} name - Nom du modèle
     * @param {Object} model - Instance du modèle
     */
    setModel(name, model) {
        if (!this.models || typeof this.models !== 'object') {
            this.models = {};
        }
        this.models[name] = model;
        this.logDebug('info', `Model '${name}' set`);
    }

    /**
     * Définir une vue
     * @param {string} name - Nom de la vue
     * @param {Object} view - Instance de la vue
     */
    setView(name, view) {
        if (!this.views || typeof this.views !== 'object') {
            this.views = {};
        }
        this.views[name] = view;
        this.logDebug('info', `View '${name}' set`);
    }

    /**
     * Vérifier si un modèle existe
     * @param {string} name - Nom du modèle
     * @returns {boolean} True si le modèle existe
     */
    hasModel(name) {
        return this.getModel(name) !== null;
    }

    /**
     * Vérifier si une vue existe
     * @param {string} name - Nom de la vue
     * @returns {boolean} True si la vue existe
     */
    hasView(name) {
        return this.getView(name) !== null;
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BaseController;
}

if (typeof window !== 'undefined') {
    window.BaseController = BaseController;
}
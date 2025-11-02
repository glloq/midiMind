/* ======================================================================================
   CONTRÃ”LEUR DE BASE - PATTERN CONTROLLER DU MVC
   ======================================================================================
   Fichier: frontend/js/core/BaseController.js
   Version: v3.3.0 - FIXED EVENTBUS FALLBACKS
   Date: 2025-10-31
   ======================================================================================
   CORRECTIONS v3.3.0:
   âœ… CRITIQUE: Fallback robuste pour eventBus (window.eventBus)
   âœ… Validation eventBus avant toute utilisation
   âœ… Signature cohÃ©rente maintenue
   âœ… Protection contre eventBus null
   ====================================================================================== */

class BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null) {
        // âœ… CRITIQUE: EventBus avec fallback robuste
        this.eventBus = eventBus || window.eventBus || null;
        
        // Validation EventBus
        if (!this.eventBus) {
            console.error(`[${this.constructor.name}] CRITIQUE: EventBus non disponible!`);
            // CrÃ©er un fallback minimal pour Ã©viter les crashes
            this.eventBus = {
                on: () => () => {},
                once: () => () => {},
                emit: () => {},
                off: () => {}
            };
        }
        
        // RÃ©fÃ©rences aux composants principaux
        this.models = models;
        this.views = views;
        this.notifications = notifications;
        this.debugConsole = debugConsole;
        
        // Ã‰tat du contrÃ´leur
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
        
        // Gestion des Ã©vÃ©nements
        this.eventSubscriptions = [];
        this.actionQueue = [];
        
        // MÃ©triques et monitoring
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
		
        // Validateurs d'entrÃ©e
        this.validators = {};
        
        // Actions debouncÃ©es
        this.debouncedActions = new Map();
        
        // Initialisation automatique si configurÃ©e
        if (this.config.autoInitialize) {
            this.initialize();
        }
    }

    /**
     * Initialiser le contrÃ´leur
     */
    initialize() {
        if (this.state.isInitialized) {
            this.logDebug('warning', `ContrÃ´leur ${this.constructor.name} dÃ©jÃ  initialisÃ©`);
            return;
        }
        
        try {
            // ExÃ©cuter l'initialisation personnalisÃ©e
            this.onInitialize();
            
            // Lier les Ã©vÃ©nements
            this.bindEvents();
            
            // Configurer les actions debouncÃ©es
            this.setupDebouncedActions();
            
            // Marquer comme initialisÃ©
            this.state.isInitialized = true;
            this.state.isActive = true;
            
            this.logDebug('system', `ContrÃ´leur ${this.constructor.name} initialisÃ©`);
            this.emitEvent('controller:initialized', {
                controller: this.constructor.name
            });
            
        } catch (error) {
            this.handleError('Erreur lors de l\'initialisation', error);
        }
    }

    /**
     * Hook d'initialisation personnalisÃ©e (Ã  surcharger)
     */
    onInitialize() {
        // Ã€ implÃ©menter dans les classes filles
    }

    /**
     * Lier les Ã©vÃ©nements du contrÃ´leur (Ã  surcharger)
     */
    bindEvents() {
        // Ã€ surcharger dans les classes filles
        this.logDebug('info', `Liaison des Ã©vÃ©nements pour ${this.constructor.name}`);
    }

    /**
     * S'abonner Ã  un Ã©vÃ©nement avec gestion automatique
     * @param {string} event - Nom de l'Ã©vÃ©nement
     * @param {function} handler - Gestionnaire
     * @param {Object} options - Options
     * @returns {function} Fonction de dÃ©sabonnement
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
                // Validation optionnelle des donnÃ©es
                if (opts.validate && !this.validateEventData(event, data)) {
                    this.logDebug('warning', `DonnÃ©es invalides pour l'Ã©vÃ©nement ${event}`);
                    return;
                }
                
                // ExÃ©cution du gestionnaire
                handler.call(this, data);
                
            } catch (error) {
                this.handleError(`Erreur dans gestionnaire pour ${event}`, error);
            }
        };
        
        // Debouncing si spÃ©cifiÃ©
        const finalHandler = opts.debounce > 0 
            ? this.debounce(wrappedHandler, opts.debounce)
            : wrappedHandler;
        
        // S'abonner Ã  l'Ã©vÃ©nement
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
     * Ã‰mettre un Ã©vÃ©nement
     * @param {string} event - Nom de l'Ã©vÃ©nement
     * @param {*} data - DonnÃ©es de l'Ã©vÃ©nement
     */
    emitEvent(event, data) {
        if (!this.eventBus || !this.eventBus.emit) {
            console.warn(`[${this.constructor.name}] Cannot emit event: EventBus not available`);
            return;
        }
        
        try {
            this.eventBus.emit(event, data);
        } catch (error) {
            this.logDebug('error', `Erreur lors de l'Ã©mission de l'Ã©vÃ©nement ${event}:`, error);
        }
    }

    /**
     * ExÃ©cuter une action avec gestion d'erreur et logging
     * @param {string} actionName - Nom de l'action
     * @param {function} action - Fonction d'action
     * @param {*} data - DonnÃ©es pour l'action
     * @param {Object} options - Options d'exÃ©cution
     * @returns {*} RÃ©sultat de l'action
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
        
        // VÃ©rifier si le contrÃ´leur est actif
        if (!this.state.isActive) {
            throw new Error(`ContrÃ´leur ${this.constructor.name} n'est pas actif`);
        }
        
        // Validation des donnÃ©es d'entrÃ©e
        if (opts.validate && !this.validateActionData(actionName, data)) {
            throw new Error(`DonnÃ©es invalides pour l'action ${actionName}`);
        }
        
        // Nettoyer cache si nÃ©cessaire
        if (opts.cache && this.shouldCleanCache()) {
            this.cleanCache();
            this._lastCacheClean = Date.now();
        }
        
        // VÃ©rifier cache
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
                    this.logDebug('action', `ExÃ©cution: ${actionName}`, data);
                }
                
                // ExÃ©cution avec timeout optionnel
                let result;
                if (opts.timeout > 0) {
                    result = await this.executeWithTimeout(action, data, opts.timeout);
                } else {
                    result = await action.call(this, data);
                }
                
                // Mise en cache du rÃ©sultat
                if (opts.cache && result !== undefined) {
                    const cacheKey = `${actionName}:${JSON.stringify(data)}`;
                    this.cache.set(cacheKey, result);
                    this.cacheTimestamps.set(cacheKey, Date.now());
                }
                
                // MÃ©triques
                this.metrics.actionsExecuted++;
                this.state.lastAction = {
                    name: actionName,
                    timestamp: new Date().toISOString(),
                    duration: performance.now() - startTime,
                    success: true
                };
                
                // Notification de succÃ¨s
                if (opts.notify && opts.notify.success) {
                    this.showNotification(opts.notify.success, 'success');
                }
                
                return result;
                
            } catch (error) {
                lastError = error;
                
                // Retry si demandÃ©
                if (attempts <= opts.retry) {
                    this.logDebug('warning', `Retry ${attempts}/${opts.retry} pour ${actionName}`);
                    await this.sleep(Math.min(1000 * attempts, 5000)); // Backoff exponentiel
                    continue;
                }
                
                // Logging de l'erreur
                this.logDebug('error', `Erreur dans action ${actionName}:`, error);
                
                // MÃ©triques
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
                
                // GÃ©rer l'erreur ou la propager
                if (this.config.handleErrors) {
                    this.handleError(actionName, error);
                } else {
                    throw error;
                }
            }
        } while (attempts <= opts.retry);
        
        // Si on arrive ici, toutes les tentatives ont Ã©chouÃ©
        throw lastError;
    }

    /**
     * Valider les donnÃ©es d'un Ã©vÃ©nement
     * @param {string} event - Nom de l'Ã©vÃ©nement
     * @param {*} data - DonnÃ©es Ã  valider
     * @returns {boolean} True si valide
     */
    validateEventData(event, data) {
        // ImplÃ©mentation basique - Ã  surcharger si nÃ©cessaire
        return true;
    }

    /**
     * Valider les donnÃ©es d'une action
     * @param {string} action - Nom de l'action
     * @param {*} data - DonnÃ©es Ã  valider
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
     * GÃ©rer une erreur
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
        
        // Ã‰mettre un Ã©vÃ©nement d'erreur
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
     * @param {...*} args - Arguments Ã  logger
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
     * Méthode de logging flexible
     * Compatible avec signatures multiples
     * @param {string} level - Niveau (info, warn, error, debug)
     * @param {...*} args - Arguments
     */
    log(level, ...args) {
        // Rediriger vers logDebug pour cohérence
        this.logDebug(level, ...args);
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
     * Obtenir l'Ã©tat du contrÃ´leur
     * @returns {Object} Ã‰tat
     */
    getState() {
        return {
            ...this.state,
            metrics: { ...this.metrics }
        };
    }

    /**
     * Obtenir les mÃ©triques du contrÃ´leur
     * @returns {Object} MÃ©triques
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
     * @param {string} key - ClÃ©
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
     * @param {string} key - ClÃ©
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
        
        this.logDebug('info', `Cache nettoyÃ©${pattern ? ` (pattern: ${pattern})` : ''}`);
    }

    /**
     * Nettoie les entrÃ©es de cache expirÃ©es
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
     * VÃ©rifie si le cache doit Ãªtre nettoyÃ©
     * @private
     */
    shouldCleanCache() {
        const now = Date.now();
        const fiveMinutes = 5 * 60 * 1000;
        
        return this.cache.size > 100 || 
               (this._lastCacheClean && now - this._lastCacheClean > fiveMinutes);
    }

    /**
     * Activer le contrÃ´leur
     */
    activate() {
        this.state.isActive = true;
        this.logDebug('system', `ContrÃ´leur ${this.constructor.name} activÃ©`);
        this.emitEvent('controller:activated', { controller: this.constructor.name });
    }

    /**
     * DÃ©sactiver le contrÃ´leur
     */
    deactivate() {
        this.state.isActive = false;
        this.logDebug('system', `ContrÃ´leur ${this.constructor.name} dÃ©sactivÃ©`);
        this.emitEvent('controller:deactivated', { controller: this.constructor.name });
    }

    /**
     * DÃ©truire le contrÃ´leur et nettoyer les ressources
     */
    destroy() {
        if (this.state.isDestroyed) {
            return;
        }
        
        // Hook de destruction personnalisÃ©
        if (typeof this.onDestroy === 'function') {
            try {
                this.onDestroy();
            } catch (error) {
                this.logDebug('error', 'Erreur lors du hook onDestroy:', error);
            }
        }
        
        // Nettoyer les abonnements aux Ã©vÃ©nements
        this.eventSubscriptions.forEach(({ unsubscribe }) => {
            if (typeof unsubscribe === 'function') {
                unsubscribe();
            }
        });
        this.eventSubscriptions = [];
        
        // Nettoyer le cache
        this.cache.clear();
        this.cacheTimestamps.clear();
        
        // Nettoyer les actions debouncÃ©es
        this.debouncedActions.clear();
        
        // Marquer comme dÃ©truit
        this.state.isDestroyed = true;
        this.state.isActive = false;
        
        this.logDebug('system', `ContrÃ´leur ${this.constructor.name} dÃ©truit`);
        this.emitEvent('controller:destroyed', { controller: this.constructor.name });
    }

    // ===== MÃ‰THODES UTILITAIRES =====

    /**
     * CrÃ©er une fonction debouncÃ©e
     * @param {function} func - Fonction Ã  debouncer
     * @param {number} delay - DÃ©lai en ms
     * @returns {function} Fonction debouncÃ©e
     */
    debounce(func, delay) {
        let timeoutId;
        return function (...args) {
            clearTimeout(timeoutId);
            timeoutId = setTimeout(() => func.apply(this, args), delay);
        };
    }

    /**
     * CrÃ©er une fonction throttlÃ©e
     * @param {function} func - Fonction Ã  throttler
     * @param {number} delay - DÃ©lai en ms
     * @returns {function} Fonction throttlÃ©e
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
     * ExÃ©cuter une fonction avec timeout
     * @param {function} func - Fonction Ã  exÃ©cuter
     * @param {*} data - DonnÃ©es
     * @param {number} timeout - Timeout en ms
     * @returns {Promise} RÃ©sultat ou timeout
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
     * Attendre un dÃ©lai
     * @param {number} ms - DÃ©lai en ms
     * @returns {Promise} Promise qui se rÃ©sout aprÃ¨s le dÃ©lai
     */
    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    /**
     * Formater une durÃ©e en millisecondes
     * @param {number} ms - Millisecondes
     * @returns {string} DurÃ©e formatÃ©e
     */
    formatDuration(ms) {
        if (ms < 1000) return `${ms}ms`;
        if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
        return `${(ms / 60000).toFixed(1)}min`;
    }

    /**
     * GÃ©nÃ©rer un ID unique
     * @returns {string} ID unique
     */
    generateId() {
        return `${this.constructor.name.toLowerCase()}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    }

    /**
     * Obtenir un modÃ¨le de maniÃ¨re sÃ©curisÃ©e
     * @param {string} name - Nom du modÃ¨le
     * @returns {Object|null} ModÃ¨le ou null
     */
    getModel(name) {
        if (!this.models || typeof this.models !== 'object') {
            return null;
        }
        return this.models[name] || null;
    }

    /**
     * Obtenir une vue de maniÃ¨re sÃ©curisÃ©e
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
     * DÃ©finir un modÃ¨le
     * @param {string} name - Nom du modÃ¨le
     * @param {Object} model - Instance du modÃ¨le
     */
    setModel(name, model) {
        if (!this.models || typeof this.models !== 'object') {
            this.models = {};
        }
        this.models[name] = model;
        this.logDebug('info', `Model '${name}' set`);
    }

    /**
     * DÃ©finir une vue
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
     * VÃ©rifier si un modÃ¨le existe
     * @param {string} name - Nom du modÃ¨le
     * @returns {boolean} True si le modÃ¨le existe
     */
    hasModel(name) {
        return this.getModel(name) !== null;
    }

    /**
     * VÃ©rifier si une vue existe
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
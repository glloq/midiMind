/* ======================================================================================
   CONTRÃ”LEUR DE BASE - PATTERN CONTROLLER DU MVC
   ======================================================================================
   Classe de base pour tous les contrÃ´leurs de l'application
   GÃ¨re la logique mÃ©tier, la coordination entre modÃ¨les et vues
   Fournit des mÃ©thodes communes pour la gestion d'erreurs, notifications, etc.
   ====================================================================================== */

class BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null) {
        // RÃ©fÃ©rences aux composants principaux
        this.eventBus = eventBus;
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
            debounceActions: {}
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
            this.log('debug', `Cache hit for ${actionName}`);
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
                
                // Ã‰vÃ©nement de succÃ¨s
                this.emitEvent('controller:action:success', {
                    controller: this.constructor.name,
                    action: actionName,
                    duration: this.state.lastAction.duration,
                    result
                });
                
                return result;
                
            } catch (error) {
                lastError = error;
                
                if (attempts <= opts.retry) {
                    this.logDebug('warning', `Tentative ${attempts}/${opts.retry + 1} Ã©chouÃ©e pour ${actionName}: ${error.message}`);
                    await this.sleep(Math.pow(2, attempts - 1) * 1000); // Backoff exponentiel
                    continue;
                }
                
                // Toutes les tentatives Ã©chouÃ©es
                this.handleActionError(actionName, error, data, opts);
                throw error;
            }
        } while (attempts <= opts.retry);
    }

    /**
     * GÃ©rer une erreur d'action
     * @param {string} actionName - Nom de l'action
     * @param {Error} error - Erreur
     * @param {*} data - DonnÃ©es de l'action
     * @param {Object} options - Options
     */
    handleActionError(actionName, error, data, options) {
        // MÃ©triques
        this.metrics.errorsHandled++;
        this.state.errors.push({
            action: actionName,
            error: error.message,
            timestamp: new Date().toISOString(),
            data
        });
        
        // Logging
        this.logDebug('error', `Erreur dans ${actionName}: ${error.message}`, { data, stack: error.stack });
        
        // Notification d'erreur
        if (options.notify && options.notify.error) {
            this.showNotification(options.notify.error, 'error');
        }
        
        // Ã‰vÃ©nement d'erreur
        this.emitEvent('controller:action:error', {
            controller: this.constructor.name,
            action: actionName,
            error: error.message,
            data
        });
        
        // Gestion d'erreur globale
        if (this.config.handleErrors) {
            this.handleError(`Erreur dans l'action ${actionName}`, error);
        }
    }

    /**
     * Configurer les actions debouncÃ©es
     */
    setupDebouncedActions() {
        Object.keys(this.config.debounceActions).forEach(actionName => {
            const delay = this.config.debounceActions[actionName];
            if (typeof this[actionName] === 'function') {
                this.debouncedActions.set(actionName, this.debounce(this[actionName], delay));
            }
        });
    }

    /**
     * ExÃ©cuter une action debouncÃ©e
     * @param {string} actionName - Nom de l'action
     * @param {...*} args - Arguments
     */
    executeDebouncedAction(actionName, ...args) {
        const debouncedAction = this.debouncedActions.get(actionName);
        if (debouncedAction) {
            return debouncedAction.apply(this, args);
        } else {
            this.logDebug('warning', `Action debouncÃ©e ${actionName} non trouvÃ©e`);
        }
    }

    /**
     * Obtenir un modÃ¨le par nom
     * @param {string} modelName - Nom du modÃ¨le
     * @returns {BaseModel} Instance du modÃ¨le
     */
    getModel(modelName) {
        const model = this.models[modelName];
        if (!model) {
            this.logDebug('warning', `ModÃ¨le '${modelName}' non trouvÃ©`);
        }
        return model;
    }

    /**
     * Obtenir une vue par nom
     * @param {string} viewName - Nom de la vue
     * @returns {BaseView} Instance de la vue
     */
    getView(viewName) {
        const view = this.views[viewName];
        if (!view) {
            this.logDebug('warning', `Vue '${viewName}' non trouvÃ©e`);
        }
        return view;
    }

    /**
     * Afficher une notification
     * @param {string} message - Message Ã  afficher
     * @param {string} type - Type de notification (info, success, warning, error)
     * @param {Object} options - Options de notification
     */
    showNotification(message, type = 'info', options = {}) {
        if (this.notifications && typeof this.notifications.show === 'function') {
            this.notifications.show(message, type, options);
            this.metrics.notificationsSent++;
            
            this.logDebug('notification', `Notification ${type}: ${message}`);
        }
    }

    /**
     * Logger un message de debug
     * @param {string} category - CatÃ©gorie du message
     * @param {string} message - Message Ã  logger
     * @param {*} data - DonnÃ©es additionnelles
     */
    logDebug(category, message, data = null) {
        if (this.debugConsole && typeof this.debugConsole.log === 'function') {
            const logMessage = `[${this.constructor.name}] ${message}`;
            this.debugConsole.log(category, logMessage, data);
        }
    }

    /**
     * GÃ©rer une erreur gÃ©nÃ©rale
     * @param {string} context - Contexte de l'erreur
     * @param {Error} error - Erreur
     */
    handleError(context, error) {
        const errorMessage = `${context}: ${error.message}`;
        
        // Logging
        this.logDebug('error', errorMessage, { stack: error.stack });
        
        // Ajouter Ã  l'historique des erreurs
        this.state.errors.push({
            context,
            error: error.message,
            timestamp: new Date().toISOString(),
            stack: error.stack
        });
        
        // Ã‰mettre un Ã©vÃ©nement d'erreur
        this.emitEvent('controller:error', {
            controller: this.constructor.name,
            context,
            error: error.message
        });
        
        // Notification optionnelle
        if (this.config.notifyErrors) {
            this.showNotification(`Erreur: ${context}`, 'error');
        }
    }


    /**
     * Afficher une erreur
     * @param {string} message - Message d'erreur
     */
    showError(message) {
        this.showNotification(message, 'error');
    }

    /**
     * Valider les donnÃ©es d'une action
     * @param {string} actionName - Nom de l'action
     * @param {*} data - DonnÃ©es Ã  valider
     * @returns {boolean} True si valide
     */
    validateActionData(actionName, data) {
        const validator = this.validators[actionName];
        if (!validator) {
            return true; // Pas de validation dÃ©finie
        }
        
        try {
            return validator.call(this, data);
        } catch (error) {
            this.logDebug('error', `Erreur de validation pour ${actionName}: ${error.message}`);
            return false;
        }
    }

    /**
     * Valider les donnÃ©es d'un Ã©vÃ©nement
     * @param {string} event - Nom de l'Ã©vÃ©nement
     * @param {*} data - DonnÃ©es Ã  valider
     * @returns {boolean} True si valide
     */
    validateEventData(event, data) {
        // Validation basique - Ã  surcharger dans les classes filles
        return true;
    }

    /**
     * Ajouter un validateur pour une action
     * @param {string} actionName - Nom de l'action
     * @param {function} validator - Fonction de validation
     */
    addValidator(actionName, validator) {
        this.validators[actionName] = validator;
    }

    /**
     * Ã‰mettre un Ã©vÃ©nement
     * @param {string} event - Nom de l'Ã©vÃ©nement
     * @param {*} data - DonnÃ©es Ã  Ã©mettre
     */
    emitEvent(event, data = null) {
        if (this.eventBus && typeof this.eventBus.emit === 'function') {
            this.eventBus.emit(event, data);
        }
    }

    /**
     * Obtenir les mÃ©triques du contrÃ´leur
     * @returns {Object} MÃ©triques
     */
    getMetrics() {
        return {
            ...this.metrics,
            uptime: Date.now() - this.metrics.startTime,
            errorRate: this.metrics.errorsHandled / Math.max(1, this.metrics.actionsExecuted),
            cacheSize: this.cache.size,
            subscriptionsCount: this.eventSubscriptions.length
        };
    }

    /**
     * Obtenir l'Ã©tat du contrÃ´leur
     * @returns {Object} Ã‰tat
     */
    getState() {
        return {
            ...this.state,
            recentErrors: this.state.errors.slice(-5)
        };
    }

    /**
     * Nettoyer le cache
     * @param {string} pattern - Motif Ã  nettoyer (optionnel)
     */
    clearCache(pattern = null) {
        if (pattern) {
            const regex = new RegExp(pattern);
            for (const [key] of this.cache.entries()) {
                if (regex.test(key)) {
                    this.cache.delete(key);
                }
            }
        } else {
            this.cache.clear();
        }
        
        this.logDebug('info', `Cache nettoyÃ©${pattern ? ` (pattern: ${pattern})` : ''}`);
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
        
        // Nettoyer les abonnements aux Ã©vÃ©nements
        this.eventSubscriptions.forEach(({ unsubscribe }) => {
            if (typeof unsubscribe === 'function') {
                unsubscribe();
            }
        });
        this.eventSubscriptions = [];
        
        // Nettoyer le cache
        this.cache.clear();
        
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
 * Nettoie les entrÃ©es de cache expirÃ©es
 * @private
 */
cleanCache() {
    const now = Date.now();
    
    for (const [key, timestamp] of this.cacheTimestamps.entries()) {
        if (now - timestamp > this.config.cacheTTL) {
            this.cache.delete(key);
            this.cacheTimestamps.delete(key);
            this.log('debug', `Cache entry expired: ${key}`);
        }
    }
}

/**
 * VÃ©rifie si le cache doit Ãªtre nettoyÃ©
 * @private
 */
shouldCleanCache() {
    // Nettoyer si:
    // 1. Cache > 100 entrÃ©es
    // 2. Ou toutes les 5 minutes
    const now = Date.now();
    const fiveMinutes = 5 * 60 * 1000;
    
    return this.cache.size > 100 || 
           (this._lastCacheClean && now - this._lastCacheClean > fiveMinutes);
}
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
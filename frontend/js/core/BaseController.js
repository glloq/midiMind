/* ======================================================================================
   CONTRÃƒâ€LEUR DE BASE - PATTERN CONTROLLER DU MVC
   ======================================================================================
   Fichier: frontend/js/core/BaseController.js
   Version: v3.3.0 - FIXED EVENTBUS FALLBACKS
   Date: 2025-10-31
   ======================================================================================
   CORRECTIONS v3.3.0:
   Ã¢Å“â€¦ CRITIQUE: Fallback robuste pour eventBus (window.eventBus)
   Ã¢Å“â€¦ Validation eventBus avant toute utilisation
   Ã¢Å“â€¦ Signature cohÃƒÂ©rente maintenue
   Ã¢Å“â€¦ Protection contre eventBus null
   ====================================================================================== */

class BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null) {
        // Ã¢Å“â€¦ CRITIQUE: EventBus avec fallback robuste
        this.eventBus = eventBus || window.eventBus || null;
        
        // Validation EventBus
        if (!this.eventBus) {
            console.error(`[${this.constructor.name}] CRITIQUE: EventBus non disponible!`);
            // CrÃƒÂ©er un fallback minimal pour ÃƒÂ©viter les crashes
            this.eventBus = {
                on: () => () => {},
                once: () => () => {},
                emit: () => {},
                off: () => {}
            };
        }
        
        // RÃƒÂ©fÃƒÂ©rences aux composants principaux
        this.models = models;
        this.views = views;
        this.notifications = notifications;
        this.debugConsole = debugConsole;
        
        // Ãƒâ€°tat du contrÃƒÂ´leur
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
        
        // Gestion des ÃƒÂ©vÃƒÂ©nements
        this.eventSubscriptions = [];
        this.actionQueue = [];
        
        // MÃƒÂ©triques et monitoring
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
		
        // Validateurs d'entrÃƒÂ©e
        this.validators = {};
        
        // Actions debouncÃƒÂ©es
        this.debouncedActions = new Map();
        
        // Initialisation automatique si configurÃƒÂ©e
        if (this.config.autoInitialize) {
            this.initialize();
        }
    }

    /**
     * Initialiser le contrÃƒÂ´leur
     */
    initialize() {
        if (this.state.isInitialized) {
            this.logDebug('warning', `ContrÃƒÂ´leur ${this.constructor.name} dÃƒÂ©jÃƒÂ  initialisÃƒÂ©`);
            return;
        }
        
        try {
            // ExÃƒÂ©cuter l'initialisation personnalisÃƒÂ©e
            this.onInitialize();
            
            // Lier les ÃƒÂ©vÃƒÂ©nements
            this.bindEvents();
            
            // Configurer les actions debouncÃƒÂ©es
            this.setupDebouncedActions();
            
            // Marquer comme initialisÃƒÂ©
            this.state.isInitialized = true;
            this.state.isActive = true;
            
            this.logDebug('system', `ContrÃƒÂ´leur ${this.constructor.name} initialisÃƒÂ©`);
            this.emitEvent('controller:initialized', {
                controller: this.constructor.name
            });
            
        } catch (error) {
            this.handleError('Erreur lors de l\'initialisation', error);
        }
    }

    /**
     * Hook d'initialisation personnalisÃƒÂ©e (ÃƒÂ  surcharger)
     */
    onInitialize() {
        // Ãƒâ‚¬ implÃƒÂ©menter dans les classes filles
    }

    /**
     * Lier les ÃƒÂ©vÃƒÂ©nements du contrÃƒÂ´leur (ÃƒÂ  surcharger)
     */
    bindEvents() {
        // Ãƒâ‚¬ surcharger dans les classes filles
        this.logDebug('info', `Liaison des ÃƒÂ©vÃƒÂ©nements pour ${this.constructor.name}`);
    }

    /**
     * S'abonner ÃƒÂ  un ÃƒÂ©vÃƒÂ©nement avec gestion automatique
     * @param {string} event - Nom de l'ÃƒÂ©vÃƒÂ©nement
     * @param {function} handler - Gestionnaire
     * @param {Object} options - Options
     * @returns {function} Fonction de dÃƒÂ©sabonnement
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
                // Validation optionnelle des donnÃƒÂ©es
                if (opts.validate && !this.validateEventData(event, data)) {
                    this.logDebug('warning', `DonnÃƒÂ©es invalides pour l'ÃƒÂ©vÃƒÂ©nement ${event}`);
                    return;
                }
                
                // ExÃƒÂ©cution du gestionnaire
                handler.call(this, data);
                
            } catch (error) {
                this.handleError(`Erreur dans gestionnaire pour ${event}`, error);
            }
        };
        
        // Debouncing si spÃƒÂ©cifiÃƒÂ©
        const finalHandler = opts.debounce > 0 
            ? this.debounce(wrappedHandler, opts.debounce)
            : wrappedHandler;
        
        // S'abonner ÃƒÂ  l'ÃƒÂ©vÃƒÂ©nement
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
     * Ãƒâ€°mettre un ÃƒÂ©vÃƒÂ©nement
     * @param {string} event - Nom de l'ÃƒÂ©vÃƒÂ©nement
     * @param {*} data - DonnÃƒÂ©es de l'ÃƒÂ©vÃƒÂ©nement
     */
    emitEvent(event, data) {
        if (!this.eventBus || !this.eventBus.emit) {
            console.warn(`[${this.constructor.name}] Cannot emit event: EventBus not available`);
            return;
        }
        
        try {
            this.eventBus.emit(event, data);
        } catch (error) {
            this.logDebug('error', `Erreur lors de l'ÃƒÂ©mission de l'ÃƒÂ©vÃƒÂ©nement ${event}:`, error);
        }
    }

    /**
     * ExÃƒÂ©cuter une action avec gestion d'erreur et logging
     * @param {string} actionName - Nom de l'action
     * @param {function} action - Fonction d'action
     * @param {*} data - DonnÃƒÂ©es pour l'action
     * @param {Object} options - Options d'exÃƒÂ©cution
     * @returns {*} RÃƒÂ©sultat de l'action
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
        
        // VÃƒÂ©rifier si le contrÃƒÂ´leur est actif
        if (!this.state.isActive) {
            throw new Error(`ContrÃƒÂ´leur ${this.constructor.name} n'est pas actif`);
        }
        
        // Validation des donnÃƒÂ©es d'entrÃƒÂ©e
        if (opts.validate && !this.validateActionData(actionName, data)) {
            throw new Error(`DonnÃƒÂ©es invalides pour l'action ${actionName}`);
        }
        
        // Nettoyer cache si nÃƒÂ©cessaire
        if (opts.cache && this.shouldCleanCache()) {
            this.cleanCache();
            this._lastCacheClean = Date.now();
        }
        
        // VÃƒÂ©rifier cache
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
                    this.logDebug('action', `ExÃƒÂ©cution: ${actionName}`, data);
                }
                
                // ExÃƒÂ©cution avec timeout optionnel
                let result;
                if (opts.timeout > 0) {
                    result = await this.executeWithTimeout(action, data, opts.timeout);
                } else {
                    result = await action.call(this, data);
                }
                
                // Mise en cache du rÃƒÂ©sultat
                if (opts.cache && result !== undefined) {
                    const cacheKey = `${actionName}:${JSON.stringify(data)}`;
                    this.cache.set(cacheKey, result);
                    this.cacheTimestamps.set(cacheKey, Date.now());
                }
                
                // MÃƒÂ©triques
                this.metrics.actionsExecuted++;
                this.state.lastAction = {
                    name: actionName,
                    timestamp: new Date().toISOString(),
                    duration: performance.now() - startTime,
                    success: true
                };
                
                // Notification de succÃƒÂ¨s
                if (opts.notify && opts.notify.success) {
                    this.showNotification(opts.notify.success, 'success');
                }
                
                return result;
                
            } catch (error) {
                lastError = error;
                
                // Retry si demandÃƒÂ©
                if (attempts <= opts.retry) {
                    this.logDebug('warning', `Retry ${attempts}/${opts.retry} pour ${actionName}`);
                    await this.sleep(Math.min(1000 * attempts, 5000)); // Backoff exponentiel
                    continue;
                }
                
                // Logging de l'erreur
                this.logDebug('error', `Erreur dans action ${actionName}:`, error);
                
                // MÃƒÂ©triques
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
                
                // GÃƒÂ©rer l'erreur ou la propager
                if (this.config.handleErrors) {
                    this.handleError(actionName, error);
                } else {
                    throw error;
                }
            }
        } while (attempts <= opts.retry);
        
        // Si on arrive ici, toutes les tentatives ont ÃƒÂ©chouÃƒÂ©
        throw lastError;
    }

    /**
     * Valider les donnÃƒÂ©es d'un ÃƒÂ©vÃƒÂ©nement
     * @param {string} event - Nom de l'ÃƒÂ©vÃƒÂ©nement
     * @param {*} data - DonnÃƒÂ©es ÃƒÂ  valider
     * @returns {boolean} True si valide
     */
    validateEventData(event, data) {
        // ImplÃƒÂ©mentation basique - ÃƒÂ  surcharger si nÃƒÂ©cessaire
        return true;
    }

    /**
     * Valider les donnÃƒÂ©es d'une action
     * @param {string} action - Nom de l'action
     * @param {*} data - DonnÃƒÂ©es ÃƒÂ  valider
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
     * GÃƒÂ©rer une erreur
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
        
        // Ãƒâ€°mettre un ÃƒÂ©vÃƒÂ©nement d'erreur
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
     * @param {...*} args - Arguments ÃƒÂ  logger
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
     * MÃ©thode de logging flexible
     * Compatible avec signatures multiples
     * @param {string} level - Niveau (info, warn, error, debug)
     * @param {...*} args - Arguments
     */
    log(level, ...args) {
        // Rediriger vers logDebug pour cohÃ©rence
        this.logDebug(level, ...args);
    }

    /**
     * Configurer les actions debouncÃ©es
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
     * Obtenir l'ÃƒÂ©tat du contrÃƒÂ´leur
     * @returns {Object} Ãƒâ€°tat
     */
    getState() {
        return {
            ...this.state,
            metrics: { ...this.metrics }
        };
    }

    /**
     * Obtenir les mÃƒÂ©triques du contrÃƒÂ´leur
     * @returns {Object} MÃƒÂ©triques
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
     * @param {string} key - ClÃƒÂ©
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
     * @param {string} key - ClÃƒÂ©
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
        
        this.logDebug('info', `Cache nettoyÃƒÂ©${pattern ? ` (pattern: ${pattern})` : ''}`);
    }

    /**
     * Nettoie les entrÃƒÂ©es de cache expirÃƒÂ©es
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
     * VÃƒÂ©rifie si le cache doit ÃƒÂªtre nettoyÃƒÂ©
     * @private
     */
    shouldCleanCache() {
        const now = Date.now();
        const fiveMinutes = 5 * 60 * 1000;
        
        return this.cache.size > 100 || 
               (this._lastCacheClean && now - this._lastCacheClean > fiveMinutes);
    }

    /**
     * Activer le contrÃƒÂ´leur
     */
    activate() {
        this.state.isActive = true;
        this.logDebug('system', `ContrÃƒÂ´leur ${this.constructor.name} activÃƒÂ©`);
        this.emitEvent('controller:activated', { controller: this.constructor.name });
    }

    /**
     * DÃƒÂ©sactiver le contrÃƒÂ´leur
     */
    deactivate() {
        this.state.isActive = false;
        this.logDebug('system', `ContrÃƒÂ´leur ${this.constructor.name} dÃƒÂ©sactivÃƒÂ©`);
        this.emitEvent('controller:deactivated', { controller: this.constructor.name });
    }

    /**
     * DÃƒÂ©truire le contrÃƒÂ´leur et nettoyer les ressources
     */
    destroy() {
        if (this.state.isDestroyed) {
            return;
        }
        
        // Hook de destruction personnalisÃƒÂ©
        if (typeof this.onDestroy === 'function') {
            try {
                this.onDestroy();
            } catch (error) {
                this.logDebug('error', 'Erreur lors du hook onDestroy:', error);
            }
        }
        
        // Nettoyer les abonnements aux ÃƒÂ©vÃƒÂ©nements
        this.eventSubscriptions.forEach(({ unsubscribe }) => {
            if (typeof unsubscribe === 'function') {
                unsubscribe();
            }
        });
        this.eventSubscriptions = [];
        
        // Nettoyer le cache
        this.cache.clear();
        this.cacheTimestamps.clear();
        
        // Nettoyer les actions debouncÃƒÂ©es
        this.debouncedActions.clear();
        
        // Marquer comme dÃƒÂ©truit
        this.state.isDestroyed = true;
        this.state.isActive = false;
        
        this.logDebug('system', `ContrÃƒÂ´leur ${this.constructor.name} dÃƒÂ©truit`);
        this.emitEvent('controller:destroyed', { controller: this.constructor.name });
    }

    // ===== MÃƒâ€°THODES UTILITAIRES =====

    /**
     * CrÃƒÂ©er une fonction debouncÃƒÂ©e
     * @param {function} func - Fonction ÃƒÂ  debouncer
     * @param {number} delay - DÃƒÂ©lai en ms
     * @returns {function} Fonction debouncÃƒÂ©e
     */
    debounce(func, delay) {
        let timeoutId;
        return function (...args) {
            clearTimeout(timeoutId);
            timeoutId = setTimeout(() => func.apply(this, args), delay);
        };
    }

    /**
     * CrÃƒÂ©er une fonction throttlÃƒÂ©e
     * @param {function} func - Fonction ÃƒÂ  throttler
     * @param {number} delay - DÃƒÂ©lai en ms
     * @returns {function} Fonction throttlÃƒÂ©e
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
     * ExÃƒÂ©cuter une fonction avec timeout
     * @param {function} func - Fonction ÃƒÂ  exÃƒÂ©cuter
     * @param {*} data - DonnÃƒÂ©es
     * @param {number} timeout - Timeout en ms
     * @returns {Promise} RÃƒÂ©sultat ou timeout
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
     * Attendre un dÃƒÂ©lai
     * @param {number} ms - DÃƒÂ©lai en ms
     * @returns {Promise} Promise qui se rÃƒÂ©sout aprÃƒÂ¨s le dÃƒÂ©lai
     */
    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    /**
     * Formater une durÃƒÂ©e en millisecondes
     * @param {number} ms - Millisecondes
     * @returns {string} DurÃƒÂ©e formatÃƒÂ©e
     */
    formatDuration(ms) {
        if (ms < 1000) return `${ms}ms`;
        if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
        return `${(ms / 60000).toFixed(1)}min`;
    }

    /**
     * GÃƒÂ©nÃƒÂ©rer un ID unique
     * @returns {string} ID unique
     */
    generateId() {
        return `${this.constructor.name.toLowerCase()}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    }

    /**
     * Obtenir un modÃƒÂ¨le de maniÃƒÂ¨re sÃƒÂ©curisÃƒÂ©e
     * @param {string} name - Nom du modÃƒÂ¨le
     * @returns {Object|null} ModÃƒÂ¨le ou null
     */
    getModel(name) {
        if (!this.models || typeof this.models !== 'object') {
            return null;
        }
        return this.models[name] || null;
    }

    /**
     * Obtenir une vue de maniÃƒÂ¨re sÃƒÂ©curisÃƒÂ©e
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
     * DÃƒÂ©finir un modÃƒÂ¨le
     * @param {string} name - Nom du modÃƒÂ¨le
     * @param {Object} model - Instance du modÃƒÂ¨le
     */
    setModel(name, model) {
        if (!this.models || typeof this.models !== 'object') {
            this.models = {};
        }
        this.models[name] = model;
        this.logDebug('info', `Model '${name}' set`);
    }

    /**
     * DÃƒÂ©finir une vue
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
     * VÃƒÂ©rifier si un modÃƒÂ¨le existe
     * @param {string} name - Nom du modÃƒÂ¨le
     * @returns {boolean} True si le modÃƒÂ¨le existe
     */
    hasModel(name) {
        return this.getModel(name) !== null;
    }

    /**
     * VÃƒÂ©rifier si une vue existe
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
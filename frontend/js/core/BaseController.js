/* ======================================================================================
   CONTRÃƒÆ’Ã¢â‚¬ÂLEUR DE BASE - PATTERN CONTROLLER DU MVC
   ======================================================================================
   Fichier: frontend/js/core/BaseController.js
   Version: v3.3.0 - FIXED EVENTBUS FALLBACKS
   Date: 2025-10-31
   ======================================================================================
   CORRECTIONS v3.3.0:
   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ CRITIQUE: Fallback robuste pour eventBus (window.eventBus)
   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Validation eventBus avant toute utilisation
   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Signature cohÃƒÆ’Ã‚Â©rente maintenue
   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Protection contre eventBus null
   ====================================================================================== */

class BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null) {
        // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ CRITIQUE: EventBus avec fallback robuste
        this.eventBus = eventBus || window.eventBus || null;
        
        // Validation EventBus
        if (!this.eventBus) {
            console.error(`[${this.constructor.name}] CRITIQUE: EventBus non disponible!`);
            // CrÃƒÆ’Ã‚Â©er un fallback minimal pour ÃƒÆ’Ã‚Â©viter les crashes
            this.eventBus = {
                on: () => () => {},
                once: () => () => {},
                emit: () => {},
                off: () => {}
            };
        }
        
        // RÃƒÆ’Ã‚Â©fÃƒÆ’Ã‚Â©rences aux composants principaux
        this.models = models;
        this.views = views;
        this.notifications = notifications;
        this.debugConsole = debugConsole;
        
        // ÃƒÆ’Ã¢â‚¬Â°tat du contrÃƒÆ’Ã‚Â´leur
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
        
        // Gestion des ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
        this.eventSubscriptions = [];
        this.actionQueue = [];
        
        // MÃƒÆ’Ã‚Â©triques et monitoring
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
		
        // Validateurs d'entrÃƒÆ’Ã‚Â©e
        this.validators = {};
        
        // Actions debouncÃƒÆ’Ã‚Â©es
        this.debouncedActions = new Map();
        
        // Initialisation automatique si configurÃƒÆ’Ã‚Â©e
        if (this.config.autoInitialize) {
            this.initialize();
        }
    }

    /**
     * Initialiser le contrÃƒÆ’Ã‚Â´leur
     */
    initialize() {
        if (this.state.isInitialized) {
            this.logDebug('warning', `ContrÃƒÆ’Ã‚Â´leur ${this.constructor.name} dÃƒÆ’Ã‚Â©jÃƒÆ’Ã‚Â  initialisÃƒÆ’Ã‚Â©`);
            return;
        }
        
        try {
            // ExÃƒÆ’Ã‚Â©cuter l'initialisation personnalisÃƒÆ’Ã‚Â©e
            this.onInitialize();
            
            // Lier les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
            this.bindEvents();
            
            // Configurer les actions debouncÃƒÆ’Ã‚Â©es
            this.setupDebouncedActions();
            
            // Marquer comme initialisÃƒÆ’Ã‚Â©
            this.state.isInitialized = true;
            this.state.isActive = true;
            
            this.logDebug('system', `ContrÃƒÆ’Ã‚Â´leur ${this.constructor.name} initialisÃƒÆ’Ã‚Â©`);
            this.emitEvent('controller:initialized', {
                controller: this.constructor.name
            });
            
        } catch (error) {
            this.handleError('Erreur lors de l\'initialisation', error);
        }
    }

    /**
     * Hook d'initialisation personnalisÃƒÆ’Ã‚Â©e (ÃƒÆ’Ã‚Â  surcharger)
     */
    onInitialize() {
        // ÃƒÆ’Ã¢â€šÂ¬ implÃƒÆ’Ã‚Â©menter dans les classes filles
    }

    /**
     * Lier les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements du contrÃƒÆ’Ã‚Â´leur (ÃƒÆ’Ã‚Â  surcharger)
     */
    bindEvents() {
        // ÃƒÆ’Ã¢â€šÂ¬ surcharger dans les classes filles
        this.logDebug('info', `Liaison des ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements pour ${this.constructor.name}`);
    }

    /**
     * S'abonner ÃƒÆ’Ã‚Â  un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement avec gestion automatique
     * @param {string} event - Nom de l'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {function} handler - Gestionnaire
     * @param {Object} options - Options
     * @returns {function} Fonction de dÃƒÆ’Ã‚Â©sabonnement
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
                // Validation optionnelle des donnÃƒÆ’Ã‚Â©es
                if (opts.validate && !this.validateEventData(event, data)) {
                    this.logDebug('warning', `DonnÃƒÆ’Ã‚Â©es invalides pour l'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement ${event}`);
                    return;
                }
                
                // ExÃƒÆ’Ã‚Â©cution du gestionnaire
                handler.call(this, data);
                
            } catch (error) {
                this.handleError(`Erreur dans gestionnaire pour ${event}`, error);
            }
        };
        
        // Debouncing si spÃƒÆ’Ã‚Â©cifiÃƒÆ’Ã‚Â©
        const finalHandler = opts.debounce > 0 
            ? this.debounce(wrappedHandler, opts.debounce)
            : wrappedHandler;
        
        // S'abonner ÃƒÆ’Ã‚Â  l'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
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
     * ÃƒÆ’Ã¢â‚¬Â°mettre un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {string} event - Nom de l'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {*} data - DonnÃƒÆ’Ã‚Â©es de l'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     */
    emitEvent(event, data) {
        if (!this.eventBus || !this.eventBus.emit) {
            console.warn(`[${this.constructor.name}] Cannot emit event: EventBus not available`);
            return;
        }
        
        try {
            this.eventBus.emit(event, data);
        } catch (error) {
            this.logDebug('error', `Erreur lors de l'ÃƒÆ’Ã‚Â©mission de l'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement ${event}:`, error);
        }
    }

    /**
     * ExÃƒÆ’Ã‚Â©cuter une action avec gestion d'erreur et logging
     * @param {string} actionName - Nom de l'action
     * @param {function} action - Fonction d'action
     * @param {*} data - DonnÃƒÆ’Ã‚Â©es pour l'action
     * @param {Object} options - Options d'exÃƒÆ’Ã‚Â©cution
     * @returns {*} RÃƒÆ’Ã‚Â©sultat de l'action
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
        
        // VÃƒÆ’Ã‚Â©rifier si le contrÃƒÆ’Ã‚Â´leur est actif
        if (!this.state.isActive) {
            throw new Error(`ContrÃƒÆ’Ã‚Â´leur ${this.constructor.name} n'est pas actif`);
        }
        
        // Validation des donnÃƒÆ’Ã‚Â©es d'entrÃƒÆ’Ã‚Â©e
        if (opts.validate && !this.validateActionData(actionName, data)) {
            throw new Error(`DonnÃƒÆ’Ã‚Â©es invalides pour l'action ${actionName}`);
        }
        
        // Nettoyer cache si nÃƒÆ’Ã‚Â©cessaire
        if (opts.cache && this.shouldCleanCache()) {
            this.cleanCache();
            this._lastCacheClean = Date.now();
        }
        
        // VÃƒÆ’Ã‚Â©rifier cache
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
                    this.logDebug('action', `ExÃƒÆ’Ã‚Â©cution: ${actionName}`, data);
                }
                
                // ExÃƒÆ’Ã‚Â©cution avec timeout optionnel
                let result;
                if (opts.timeout > 0) {
                    result = await this.executeWithTimeout(action, data, opts.timeout);
                } else {
                    result = await action.call(this, data);
                }
                
                // Mise en cache du rÃƒÆ’Ã‚Â©sultat
                if (opts.cache && result !== undefined) {
                    const cacheKey = `${actionName}:${JSON.stringify(data)}`;
                    this.cache.set(cacheKey, result);
                    this.cacheTimestamps.set(cacheKey, Date.now());
                }
                
                // MÃƒÆ’Ã‚Â©triques
                this.metrics.actionsExecuted++;
                this.state.lastAction = {
                    name: actionName,
                    timestamp: new Date().toISOString(),
                    duration: performance.now() - startTime,
                    success: true
                };
                
                // Notification de succÃƒÆ’Ã‚Â¨s
                if (opts.notify && opts.notify.success) {
                    this.showNotification(opts.notify.success, 'success');
                }
                
                return result;
                
            } catch (error) {
                lastError = error;
                
                // Retry si demandÃƒÆ’Ã‚Â©
                if (attempts <= opts.retry) {
                    this.logDebug('warning', `Retry ${attempts}/${opts.retry} pour ${actionName}`);
                    await this.sleep(Math.min(1000 * attempts, 5000)); // Backoff exponentiel
                    continue;
                }
                
                // Logging de l'erreur
                this.logDebug('error', `Erreur dans action ${actionName}:`, error);
                
                // MÃƒÆ’Ã‚Â©triques
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
                
                // GÃƒÆ’Ã‚Â©rer l'erreur ou la propager
                if (this.config.handleErrors) {
                    this.handleError(actionName, error);
                } else {
                    throw error;
                }
            }
        } while (attempts <= opts.retry);
        
        // Si on arrive ici, toutes les tentatives ont ÃƒÆ’Ã‚Â©chouÃƒÆ’Ã‚Â©
        throw lastError;
    }

    /**
     * Valider les donnÃƒÆ’Ã‚Â©es d'un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {string} event - Nom de l'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {*} data - DonnÃƒÆ’Ã‚Â©es ÃƒÆ’Ã‚Â  valider
     * @returns {boolean} True si valide
     */
    validateEventData(event, data) {
        // ImplÃƒÆ’Ã‚Â©mentation basique - ÃƒÆ’Ã‚Â  surcharger si nÃƒÆ’Ã‚Â©cessaire
        return true;
    }

    /**
     * Valider les donnÃƒÆ’Ã‚Â©es d'une action
     * @param {string} action - Nom de l'action
     * @param {*} data - DonnÃƒÆ’Ã‚Â©es ÃƒÆ’Ã‚Â  valider
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
     * GÃƒÆ’Ã‚Â©rer une erreur
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
        
        // ÃƒÆ’Ã¢â‚¬Â°mettre un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement d'erreur
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
     * @param {...*} args - Arguments ÃƒÆ’Ã‚Â  logger
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
     * MÃƒÂ©thode de logging flexible
     * Compatible avec signatures multiples
     * @param {string} level - Niveau (info, warn, error, debug)
     * @param {...*} args - Arguments
     */
    log(level, ...args) {
        // Rediriger vers logDebug pour cohÃƒÂ©rence
        this.logDebug(level, ...args);
    }

    /**
     * Configurer les actions debouncÃƒÂ©es
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
     * Obtenir l'ÃƒÆ’Ã‚Â©tat du contrÃƒÆ’Ã‚Â´leur
     * @returns {Object} ÃƒÆ’Ã¢â‚¬Â°tat
     */
    getState() {
        return {
            ...this.state,
            metrics: { ...this.metrics }
        };
    }

    /**
     * Obtenir les mÃƒÆ’Ã‚Â©triques du contrÃƒÆ’Ã‚Â´leur
     * @returns {Object} MÃƒÆ’Ã‚Â©triques
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
     * @param {string} key - ClÃƒÆ’Ã‚Â©
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
     * @param {string} key - ClÃƒÆ’Ã‚Â©
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
        
        this.logDebug('info', `Cache nettoyÃƒÆ’Ã‚Â©${pattern ? ` (pattern: ${pattern})` : ''}`);
    }

    /**
     * Nettoie les entrÃƒÆ’Ã‚Â©es de cache expirÃƒÆ’Ã‚Â©es
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
     * VÃƒÆ’Ã‚Â©rifie si le cache doit ÃƒÆ’Ã‚Âªtre nettoyÃƒÆ’Ã‚Â©
     * @private
     */
    shouldCleanCache() {
        const now = Date.now();
        const fiveMinutes = 5 * 60 * 1000;
        
        return this.cache.size > 100 || 
               (this._lastCacheClean && now - this._lastCacheClean > fiveMinutes);
    }

    /**
     * Activer le contrÃƒÆ’Ã‚Â´leur
     */
    activate() {
        this.state.isActive = true;
        this.logDebug('system', `ContrÃƒÆ’Ã‚Â´leur ${this.constructor.name} activÃƒÆ’Ã‚Â©`);
        this.emitEvent('controller:activated', { controller: this.constructor.name });
    }

    /**
     * DÃƒÆ’Ã‚Â©sactiver le contrÃƒÆ’Ã‚Â´leur
     */
    deactivate() {
        this.state.isActive = false;
        this.logDebug('system', `ContrÃƒÆ’Ã‚Â´leur ${this.constructor.name} dÃƒÆ’Ã‚Â©sactivÃƒÆ’Ã‚Â©`);
        this.emitEvent('controller:deactivated', { controller: this.constructor.name });
    }

    /**
     * DÃƒÆ’Ã‚Â©truire le contrÃƒÆ’Ã‚Â´leur et nettoyer les ressources
     */
    destroy() {
        if (this.state.isDestroyed) {
            return;
        }
        
        // Hook de destruction personnalisÃƒÆ’Ã‚Â©
        if (typeof this.onDestroy === 'function') {
            try {
                this.onDestroy();
            } catch (error) {
                this.logDebug('error', 'Erreur lors du hook onDestroy:', error);
            }
        }
        
        // Nettoyer les abonnements aux ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
        this.eventSubscriptions.forEach(({ unsubscribe }) => {
            if (typeof unsubscribe === 'function') {
                unsubscribe();
            }
        });
        this.eventSubscriptions = [];
        
        // Nettoyer le cache
        this.cache.clear();
        this.cacheTimestamps.clear();
        
        // Nettoyer les actions debouncÃƒÆ’Ã‚Â©es
        this.debouncedActions.clear();
        
        // Marquer comme dÃƒÆ’Ã‚Â©truit
        this.state.isDestroyed = true;
        this.state.isActive = false;
        
        this.logDebug('system', `ContrÃƒÆ’Ã‚Â´leur ${this.constructor.name} dÃƒÆ’Ã‚Â©truit`);
        this.emitEvent('controller:destroyed', { controller: this.constructor.name });
    }

    // ===== MÃƒÆ’Ã¢â‚¬Â°THODES UTILITAIRES =====

    /**
     * CrÃƒÆ’Ã‚Â©er une fonction debouncÃƒÆ’Ã‚Â©e
     * @param {function} func - Fonction ÃƒÆ’Ã‚Â  debouncer
     * @param {number} delay - DÃƒÆ’Ã‚Â©lai en ms
     * @returns {function} Fonction debouncÃƒÆ’Ã‚Â©e
     */
    debounce(func, delay) {
        let timeoutId;
        return function (...args) {
            clearTimeout(timeoutId);
            timeoutId = setTimeout(() => func.apply(this, args), delay);
        };
    }

    /**
     * CrÃƒÆ’Ã‚Â©er une fonction throttlÃƒÆ’Ã‚Â©e
     * @param {function} func - Fonction ÃƒÆ’Ã‚Â  throttler
     * @param {number} delay - DÃƒÆ’Ã‚Â©lai en ms
     * @returns {function} Fonction throttlÃƒÆ’Ã‚Â©e
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
     * ExÃƒÆ’Ã‚Â©cuter une fonction avec timeout
     * @param {function} func - Fonction ÃƒÆ’Ã‚Â  exÃƒÆ’Ã‚Â©cuter
     * @param {*} data - DonnÃƒÆ’Ã‚Â©es
     * @param {number} timeout - Timeout en ms
     * @returns {Promise} RÃƒÆ’Ã‚Â©sultat ou timeout
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
     * Attendre un dÃƒÆ’Ã‚Â©lai
     * @param {number} ms - DÃƒÆ’Ã‚Â©lai en ms
     * @returns {Promise} Promise qui se rÃƒÆ’Ã‚Â©sout aprÃƒÆ’Ã‚Â¨s le dÃƒÆ’Ã‚Â©lai
     */
    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    /**
     * Formater une durÃƒÆ’Ã‚Â©e en millisecondes
     * @param {number} ms - Millisecondes
     * @returns {string} DurÃƒÆ’Ã‚Â©e formatÃƒÆ’Ã‚Â©e
     */
    formatDuration(ms) {
        if (ms < 1000) return `${ms}ms`;
        if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
        return `${(ms / 60000).toFixed(1)}min`;
    }

    /**
     * GÃƒÆ’Ã‚Â©nÃƒÆ’Ã‚Â©rer un ID unique
     * @returns {string} ID unique
     */
    generateId() {
        return `${this.constructor.name.toLowerCase()}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    }

    /**
     * Obtenir un modÃƒÆ’Ã‚Â¨le de maniÃƒÆ’Ã‚Â¨re sÃƒÆ’Ã‚Â©curisÃƒÆ’Ã‚Â©e
     * @param {string} name - Nom du modÃƒÆ’Ã‚Â¨le
     * @returns {Object|null} ModÃƒÆ’Ã‚Â¨le ou null
     */
    getModel(name) {
        if (!this.models || typeof this.models !== 'object') {
            return null;
        }
        return this.models[name] || null;
    }

    /**
     * Obtenir une vue de maniÃƒÆ’Ã‚Â¨re sÃƒÆ’Ã‚Â©curisÃƒÆ’Ã‚Â©e
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
     * DÃƒÆ’Ã‚Â©finir un modÃƒÆ’Ã‚Â¨le
     * @param {string} name - Nom du modÃƒÆ’Ã‚Â¨le
     * @param {Object} model - Instance du modÃƒÆ’Ã‚Â¨le
     */
    setModel(name, model) {
        if (!this.models || typeof this.models !== 'object') {
            this.models = {};
        }
        this.models[name] = model;
        this.logDebug('info', `Model '${name}' set`);
    }

    /**
     * DÃƒÆ’Ã‚Â©finir une vue
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
     * VÃƒÆ’Ã‚Â©rifier si un modÃƒÆ’Ã‚Â¨le existe
     * @param {string} name - Nom du modÃƒÆ’Ã‚Â¨le
     * @returns {boolean} True si le modÃƒÆ’Ã‚Â¨le existe
     */
    hasModel(name) {
        return this.getModel(name) !== null;
    }

    /**
     * VÃƒÆ’Ã‚Â©rifier si une vue existe
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
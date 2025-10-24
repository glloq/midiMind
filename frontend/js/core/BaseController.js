/* ======================================================================================
   CONTRÔLEUR DE BASE - PATTERN CONTROLLER DU MVC
   ======================================================================================
   Classe de base pour tous les contrôleurs de l'application
   Gère la logique métier, la coordination entre modèles et vues
   Fournit des méthodes communes pour la gestion d'erreurs, notifications, etc.
   ====================================================================================== */

class BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null) {
        // Références aux composants principaux
        this.eventBus = eventBus;
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
                
                // Événement de succès
                this.emitEvent('controller:action:success', {
                    controller: this.constructor.name,
                    action: actionName,
                    duration: performance.now() - startTime
                });
                
                return result;
                
            } catch (error) {
                lastError = error;
                
                // Logging de l'erreur
                this.logDebug('error', `Erreur dans ${actionName} (tentative ${attempts}/${opts.retry + 1}):`, error.message);
                
                // Notification d'erreur
                if (opts.notify && opts.notify.error) {
                    this.showNotification(opts.notify.error, 'error');
                }
                
                // Si c'est la dernière tentative, propager l'erreur
                if (attempts > opts.retry) {
                    // Enregistrer l'erreur
                    this.state.errors.push({
                        action: actionName,
                        error: error.message,
                        timestamp: new Date().toISOString()
                    });
                    
                    this.metrics.errorsHandled++;
                    this.state.lastAction = {
                        name: actionName,
                        timestamp: new Date().toISOString(),
                        duration: performance.now() - startTime,
                        success: false,
                        error: error.message
                    };
                    
                    // Événement d'erreur
                    this.emitEvent('controller:action:error', {
                        controller: this.constructor.name,
                        action: actionName,
                        error: error.message,
                        duration: performance.now() - startTime
                    });
                    
                    throw error;
                }
                
                // Attendre avant retry
                await this.sleep(500 * attempts);
            }
        } while (attempts <= opts.retry);
        
        throw lastError;
    }

    /**
     * Configurer les actions debouncées
     * @private
     */
    setupDebouncedActions() {
        if (!this.config.debounceActions || Object.keys(this.config.debounceActions).length === 0) {
            return;
        }
        
        for (const [actionName, delay] of Object.entries(this.config.debounceActions)) {
            if (typeof this[actionName] === 'function') {
                const original = this[actionName].bind(this);
                this.debouncedActions.set(actionName, this.debounce(original, delay));
            }
        }
    }

    /**
     * Gérer une erreur
     * @param {string} context - Contexte de l'erreur
     * @param {Error} error - Erreur
     */
    handleError(context, error) {
        // Logging
        this.logDebug('error', `[${this.constructor.name}] ${context}:`, error);
        
        // Enregistrer l'erreur
        this.state.errors.push({
            context,
            message: error.message,
            stack: error.stack,
            timestamp: new Date().toISOString()
        });
        
        // Limiter le nombre d'erreurs en mémoire
        if (this.state.errors.length > 50) {
            this.state.errors = this.state.errors.slice(-50);
        }
        
        // Métriques
        this.metrics.errorsHandled++;
        
        // Notification utilisateur si système de notification disponible
        if (this.notifications && typeof this.notifications.showError === 'function') {
            this.notifications.showError(`${context}: ${error.message}`);
        }
        
        // Émettre événement d'erreur
        this.emitEvent('controller:error', {
            controller: this.constructor.name,
            context,
            error: error.message
        });
        
        // Re-throw si configuré pour ne pas gérer les erreurs
        if (!this.config.handleErrors) {
            throw error;
        }
    }

    /**
     * Afficher une notification
     * @param {string} message - Message
     * @param {string} type - Type (success, error, info, warning)
     * @param {Object} options - Options
     */
    showNotification(message, type = 'info', options = {}) {
        // Vérifier si le système de notification est disponible
        if (!this.notifications) {
            this.logDebug('warning', 'Notification system not available');
            return;
        }
        
        // Métriques
        this.metrics.notificationsSent++;
        
        // Afficher la notification selon le type
        const method = `show${type.charAt(0).toUpperCase() + type.slice(1)}`;
        
        if (typeof this.notifications[method] === 'function') {
            this.notifications[method](message, options);
        } else if (typeof this.notifications.show === 'function') {
            this.notifications.show(message, type, options);
        } else {
            // Fallback: log dans la console
            this.logDebug(type, message);
        }
        
        // Émettre événement
        this.emitEvent('controller:notification', {
            controller: this.constructor.name,
            message,
            type
        });
    }

    /**
     * Afficher un message d'erreur
     * @param {string} message - Message d'erreur
     * @param {Object} options - Options
     */
    showError(message, options = {}) {
        this.showNotification(message, 'error', options);
    }

    /**
     * Afficher un message de succès
     * @param {string} message - Message de succès
     * @param {Object} options - Options
     */
    showSuccess(message, options = {}) {
        this.showNotification(message, 'success', options);
    }

    /**
     * Afficher un message d'information
     * @param {string} message - Message d'information
     * @param {Object} options - Options
     */
    showInfo(message, options = {}) {
        this.showNotification(message, 'info', options);
    }

    /**
     * Afficher un avertissement
     * @param {string} message - Message d'avertissement
     * @param {Object} options - Options
     */
    showWarning(message, options = {}) {
        this.showNotification(message, 'warning', options);
    }

    /**
     * Logger un message de debug
     * @param {string} category - Catégorie
     * @param {string} message - Message
     * @param {*} data - Données additionnelles
     */
    logDebug(category, message, data = null) {
        if (this.debugConsole && typeof this.debugConsole.log === 'function') {
            this.debugConsole.log(category, `[${this.constructor.name}] ${message}`, data);
        } else if (window.Logger && typeof window.Logger.log === 'function') {
            window.Logger.log(category, `[${this.constructor.name}] ${message}`, data);
        } else {
            // Fallback: console standard
            const prefix = `[${this.constructor.name}]`;
            switch (category) {
                case 'error':
                    console.error(prefix, message, data);
                    break;
                case 'warning':
                case 'warn':
                    console.warn(prefix, message, data);
                    break;
                case 'info':
                    console.info(prefix, message, data);
                    break;
                default:
                    console.log(prefix, message, data);
            }
        }
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
        
        // Vérifier si le cache n'a pas expiré
        const timestamp = this.cacheTimestamps.get(key);
        if (timestamp && Date.now() - timestamp > this.config.cacheTTL) {
            this.cache.delete(key);
            this.cacheTimestamps.delete(key);
            return null;
        }
        
        return this.cache.get(key);
    }

    /**
     * Définir une valeur dans le cache
     * @param {string} key - Clé
     * @param {*} value - Valeur
     */
    setCached(key, value) {
        this.cache.set(key, value);
        this.cacheTimestamps.set(key, Date.now());
        
        // Nettoyer le cache si nécessaire
        if (this.shouldCleanCache()) {
            this.cleanCache();
            this._lastCacheClean = Date.now();
        }
    }

    /**
     * Valider les données d'une action
     * @param {string} actionName - Nom de l'action
     * @param {*} data - Données à valider
     * @returns {boolean} True si valide
     */
    validateActionData(actionName, data) {
        const validator = this.validators[actionName];
        
        if (!validator) {
            return true; // Pas de validation définie
        }
        
        try {
            return validator.call(this, data);
        } catch (error) {
            this.logDebug('error', `Erreur de validation pour ${actionName}: ${error.message}`);
            return false;
        }
    }

    /**
     * Valider les données d'un événement
     * @param {string} event - Nom de l'événement
     * @param {*} data - Données à valider
     * @returns {boolean} True si valide
     */
    validateEventData(event, data) {
        // Validation basique - à surcharger dans les classes filles
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
     * Émettre un événement
     * @param {string} event - Nom de l'événement
     * @param {*} data - Données à émettre
     */
    emitEvent(event, data = null) {
        if (this.eventBus && typeof this.eventBus.emit === 'function') {
            this.eventBus.emit(event, data);
        }
    }

    /**
     * Obtenir les métriques du contrôleur
     * @returns {Object} Métriques
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
     * Obtenir l'état du contrôleur
     * @returns {Object} État
     */
    getState() {
        return {
            ...this.state,
            recentErrors: this.state.errors.slice(-5)
        };
    }

    /**
     * Nettoyer le cache
     * @param {string} pattern - Motif à nettoyer (optionnel)
     */
    clearCache(pattern = null) {
        if (pattern) {
            const regex = new RegExp(pattern);
            for (const [key] of this.cache.entries()) {
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
    // Nettoyer si:
    // 1. Cache > 100 entrées
    // 2. Ou toutes les 5 minutes
    const now = Date.now();
    const fiveMinutes = 5 * 60 * 1000;
    
    return this.cache.size > 100 || 
           (this._lastCacheClean && now - this._lastCacheClean > fiveMinutes);
}
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
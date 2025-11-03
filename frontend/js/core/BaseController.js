/* ======================================================================================
   CONTRÔLEUR DE BASE - PATTERN CONTROLLER DU MVC
   ======================================================================================
   Fichier: frontend/js/core/BaseController.js
   Version: v3.4.0 - BACKEND INTEGRATION FIX
   Date: 2025-11-03
   ======================================================================================
   CORRECTIONS v3.4.0:
   ✦ CRITIQUE: Ajout paramètre backend au constructeur
   ✦ CRITIQUE: this.backend initialisé avec fallback window.backendService
   ✦ Protection contre backend null
   ✦ Signature cohérente pour tous les contrôleurs
   
   CORRECTIONS v3.3.0:
   ✦ CRITIQUE: Fallback robuste pour eventBus (window.eventBus)
   ✦ Validation eventBus avant toute utilisation
   ✦ Signature cohérente maintenue
   ✦ Protection contre eventBus null
   ====================================================================================== */

class BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        // ✦ CRITIQUE: EventBus avec fallback robuste
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
        
        // ✦ CRITIQUE: Backend avec fallback robuste
        this.backend = backend || window.backendService || null;
        
        // Validation Backend (warning seulement, pas d'erreur critique)
        if (!this.backend) {
            console.warn(`[${this.constructor.name}] Backend service not available - offline mode`);
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
    executeAction(actionName, action, data, options = {}) {
        const opts = {
            validateInput: this.config.validateInputs,
            logExecution: this.config.logActions,
            handleErrors: this.config.handleErrors,
            timeout: null,
            ...options
        };
        
        try {
            // Validation des entrées
            if (opts.validateInput && !this.validateActionInput(actionName, data)) {
                throw new Error(`Invalid input for action: ${actionName}`);
            }
            
            // Logging
            if (opts.logExecution) {
                this.logDebug('action', `Executing ${actionName}`);
            }
            
            // Exécution
            const startTime = Date.now();
            const result = opts.timeout
                ? this.executeWithTimeout(action, data, opts.timeout)
                : action.call(this, data);
            
            // Métriques
            this.metrics.actionsExecuted++;
            this.state.lastAction = {
                name: actionName,
                timestamp: Date.now(),
                duration: Date.now() - startTime
            };
            
            // Succès
            if (opts.logExecution) {
                this.logDebug('action', `✓ ${actionName} completed in ${Date.now() - startTime}ms`);
            }
            
            return result;
            
        } catch (error) {
            if (opts.handleErrors) {
                this.handleError(`Error in action ${actionName}`, error);
            } else {
                throw error;
            }
        }
    }

    /**
     * Gestion centralisée des erreurs
     * @param {string} message - Message d'erreur
     * @param {Error} error - Erreur
     */
    handleError(message, error) {
        // Log l'erreur
        this.logDebug('error', message, error);
        
        // Ajouter aux erreurs
        this.state.errors.push({
            message,
            error: error.message,
            stack: error.stack,
            timestamp: Date.now()
        });
        
        // Métriques
        this.metrics.errorsHandled++;
        
        // Notification si disponible
        if (this.notifications && typeof this.notifications.show === 'function') {
            this.notifications.show(message, 'error', 5000);
        }
        
        // Émettre événement d'erreur
        this.emitEvent('controller:error', {
            controller: this.constructor.name,
            message,
            error: error.message
        });
    }

    /**
     * Logging avec DebugConsole
     * @param {string} level - Niveau de log
     * @param {...*} args - Arguments
     */
    logDebug(level, ...args) {
        const prefix = `[${this.constructor.name}]`;
        
        if (this.debugConsole && typeof this.debugConsole.log === 'function') {
            this.debugConsole.log(level, prefix, ...args);
        } else {
            // Fallback vers console
            if (console[level]) {
                console[level](prefix, ...args);
            } else {
                console.log(prefix, ...args);
            }
        }
    }

    /**
     * Valider les données d'un événement
     * @param {string} event - Nom de l'événement
     * @param {*} data - Données à valider
     * @returns {boolean} True si valide
     */
    validateEventData(event, data) {
        // Validation de base par défaut
        return true;
    }

    /**
     * Valider les entrées d'une action
     * @param {string} actionName - Nom de l'action
     * @param {*} data - Données à valider
     * @returns {boolean} True si valide
     */
    validateActionInput(actionName, data) {
        if (this.validators[actionName]) {
            return this.validators[actionName](data);
        }
        return true;
    }

    /**
     * Enregistrer un validateur pour une action
     * @param {string} actionName - Nom de l'action
     * @param {function} validator - Fonction de validation
     */
    registerValidator(actionName, validator) {
        this.validators[actionName] = validator;
    }

    /**
     * Configurer les actions debouncées
     */
    setupDebouncedActions() {
        Object.entries(this.config.debounceActions).forEach(([action, delay]) => {
            if (typeof this[action] === 'function') {
                const original = this[action];
                this.debouncedActions.set(action, this.debounce(original.bind(this), delay));
            }
        });
    }

    /**
     * Obtenir les métriques du contrôleur
     * @returns {Object} Métriques
     */
    getMetrics() {
        return {
            ...this.metrics,
            uptime: Date.now() - this.metrics.startTime,
            eventSubscriptions: this.eventSubscriptions.length,
            cacheSize: this.cache.size,
            errors: this.state.errors.length
        };
    }

    /**
     * Réinitialiser les métriques
     */
    resetMetrics() {
        this.metrics = {
            actionsExecuted: 0,
            errorsHandled: 0,
            notificationsSent: 0,
            startTime: Date.now()
        };
        this.state.errors = [];
    }

    /**
     * Gestion du cache
     */
    
    /**
     * Obtenir une valeur du cache
     * @param {string} key - Clé
     * @returns {*} Valeur ou null
     */
    getCached(key) {
        const timestamp = this.cacheTimestamps.get(key);
        if (!timestamp) return null;
        
        const age = Date.now() - timestamp;
        if (age > this.config.cacheTTL) {
            this.cache.delete(key);
            this.cacheTimestamps.delete(key);
            return null;
        }
        
        return this.cache.get(key);
    }

    /**
     * Mettre en cache une valeur
     * @param {string} key - Clé
     * @param {*} value - Valeur
     */
    setCached(key, value) {
        this.cache.set(key, value);
        this.cacheTimestamps.set(key, Date.now());
        this.cleanupCache();
    }

    /**
     * Supprimer une entrée du cache
     * @param {string} key - Clé
     */
    deleteCached(key) {
        this.cache.delete(key);
        this.cacheTimestamps.delete(key);
    }

    /**
     * Vider le cache
     */
    clearCache() {
        this.cache.clear();
        this.cacheTimestamps.clear();
    }

    /**
     * Nettoyage périodique du cache
     */
    cleanupCache() {
        const now = Date.now();
        
        // Limiter la fréquence de nettoyage
        if (this._lastCacheClean && (now - this._lastCacheClean) < 60000) {
            return;
        }
        
        this._lastCacheClean = now;
        
        // Supprimer les entrées expirées
        for (const [key, timestamp] of this.cacheTimestamps.entries()) {
            if (now - timestamp > this.config.cacheTTL) {
                this.cache.delete(key);
                this.cacheTimestamps.delete(key);
            }
        }
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
        
        // Hook de destruction personnalisée
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
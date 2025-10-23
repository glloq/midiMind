// ============================================================================
// Fichier: frontend/js/core/BaseModel.js
// Version: v3.0.2 - CORRECTED (removed ES6 imports)
// Date: 2025-10-10
// Projet: MidiMind v3.0 - SystÃƒÆ’Ã‚Â¨me d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// CORRECTIONS v3.0.2:
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Å“ Removed ES6 import statements (not compatible with script tags)
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Å“ Using global variables instead (EventBus, Logger, etc.)
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Å“ Compatible with non-module script loading
// ============================================================================
// Description:
//   Classe de base pour tous les modÃƒÆ’Ã‚Â¨les de donnÃƒÆ’Ã‚Â©es de l'application.
//   Fournit la structure commune pour la gestion d'ÃƒÆ’Ã‚Â©tat, validation,
//   persistence et ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements.
//
// FonctionnalitÃƒÆ’Ã‚Â©s:
//   - Gestion d'ÃƒÆ’Ã‚Â©tat avec get/set
//   - Validation de donnÃƒÆ’Ã‚Â©es
//   - Persistence (localStorage/IndexedDB)
//   - SystÃƒÆ’Ã‚Â¨me d'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
//   - Historique des changements
//
// Architecture:
//   - Pattern Observer pour notifications
//   - Validation dÃƒÆ’Ã‚Â©clarative avec rÃƒÆ’Ã‚Â¨gles
//   - Storage abstrait (localStorage par dÃƒÆ’Ã‚Â©faut)
// ============================================================================

/**
 * @class BaseModel
 * @description Classe de base abstraite pour tous les modÃƒÆ’Ã‚Â¨les de donnÃƒÆ’Ã‚Â©es
 */
class BaseModel {
    /**
     * Constructeur du modÃƒÆ’Ã‚Â¨le de base
     * @param {Object} initialData - DonnÃƒÆ’Ã‚Â©es initiales
     * @param {Object} options - Options de configuration
     */
    constructor(initialData = {}, options = {}) {
        // DonnÃƒÆ’Ã‚Â©es du modÃƒÆ’Ã‚Â¨le
        this.data = { ...initialData };
        
        // MÃƒÆ’Ã‚Â©tadonnÃƒÆ’Ã‚Â©es
        this.meta = {
            initialized: false,
            dirty: false,
            lastModified: null,
            version: 1,
            errors: {}
        };
        
        // Configuration
        this.config = {
            autoPersist: options.autoPersist !== false,
            persistKey: options.persistKey || this.constructor.name.toLowerCase(),
            storageType: options.storageType || 'localStorage', // 'localStorage' | 'indexedDB'
            validateOnSet: options.validateOnSet !== false,
            eventPrefix: options.eventPrefix || this.constructor.name.toLowerCase(),
            debounceMs: options.debounceMs || 300,
            ...options
        };
        
        // RÃƒÆ’Ã‚Â¨gles de validation (ÃƒÆ’Ã‚Â  dÃƒÆ’Ã‚Â©finir dans les classes filles)
        this.validationRules = {};
        
        // Historique des changements (pour undo/redo)
        this.history = {
            past: [],
            future: [],
            maxSize: options.historyMaxSize || 50
        };
        
        // EventBus pour communication (global)
        this.eventBus = window.eventBus || null;
        
        // Logger (global)
        this.logger = window.logger || console;
        
        // Timers pour debounce
        this._persistTimer = null;
        this._validationTimer = null;
        
        if (this.logger && typeof this.logger.debug === "function") this.logger.debug(`${this.constructor.name}`, 'Model created');
    }

    // ========================================================================
    // MÃƒÆ’Ã¢â‚¬Â°THODES D'ACCÃƒÆ’Ã‹â€ S AUX DONNÃƒÆ’Ã¢â‚¬Â°ES
    // ========================================================================

    /**
     * RÃƒÆ’Ã‚Â©cupÃƒÆ’Ã‚Â¨re une valeur du modÃƒÆ’Ã‚Â¨le
     * @param {string} key - ClÃƒÆ’Ã‚Â© ÃƒÆ’Ã‚Â  rÃƒÆ’Ã‚Â©cupÃƒÆ’Ã‚Â©rer (supporte notation pointÃƒÆ’Ã‚Â©e: 'user.name')
     * @param {*} defaultValue - Valeur par dÃƒÆ’Ã‚Â©faut si non trouvÃƒÆ’Ã‚Â©e
     * @returns {*} Valeur
     */
    get(key, defaultValue = undefined) {
        if (key.includes('.')) {
            // Navigation profonde
            return this._getDeep(this.data, key.split('.'), defaultValue);
        }
        
        return this.data.hasOwnProperty(key) ? this.data[key] : defaultValue;
    }

    /**
     * DÃƒÆ’Ã‚Â©finit une valeur dans le modÃƒÆ’Ã‚Â¨le
     * @param {string} key - ClÃƒÆ’Ã‚Â© ÃƒÆ’Ã‚Â  dÃƒÆ’Ã‚Â©finir
     * @param {*} value - Valeur
     * @param {boolean} silent - Si true, ne dÃƒÆ’Ã‚Â©clenche pas d'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
     * @returns {boolean} SuccÃƒÆ’Ã‚Â¨s
     */
    set(key, value, silent = false) {
        const oldValue = this.get(key);
        
        // Validation si activÃƒÆ’Ã‚Â©e
        if (this.config.validateOnSet && this.validationRules[key]) {
            if (!this.validateField(key, value)) {
                this.logger.warn(`${this.constructor.name}`, `Validation failed for ${key}`);
                return false;
            }
        }
        
        // Sauvegarder dans historique
        if (this.config.enableHistory !== false) {
            this._addToHistory(key, oldValue);
        }
        
        // DÃƒÆ’Ã‚Â©finir la valeur
        if (key.includes('.')) {
            this._setDeep(this.data, key.split('.'), value);
        } else {
            this.data[key] = value;
        }
        
        // Mettre ÃƒÆ’Ã‚Â  jour mÃƒÆ’Ã‚Â©tadonnÃƒÆ’Ã‚Â©es
        this.meta.dirty = true;
        this.meta.lastModified = Date.now();
        
        // ÃƒÆ’Ã¢â‚¬Â°mettre ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
        if (!silent && this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:changed`, {
                key,
                value,
                oldValue,
                model: this
            });
        }
        
        // Auto-persistence
        if (this.config.autoPersist) {
            this._debouncedPersist();
        }
        
        return true;
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour plusieurs valeurs
     * @param {Object} updates - Objet avec les mises ÃƒÆ’Ã‚Â  jour
     * @param {boolean} silent - Si true, ne dÃƒÆ’Ã‚Â©clenche pas d'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
     * @returns {boolean} SuccÃƒÆ’Ã‚Â¨s
     */
    update(updates, silent = false) {
        if (!updates || typeof updates !== 'object') {
            return false;
        }
        
        let success = true;
        
        for (const [key, value] of Object.entries(updates)) {
            if (!this.set(key, value, true)) {
                success = false;
            }
        }
        
        // ÃƒÆ’Ã¢â‚¬Â°mettre un seul ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement pour toutes les modifications
        if (!silent && this.eventBus && success) {
            this.eventBus.emit(`${this.config.eventPrefix}:updated`, {
                updates,
                model: this
            });
        }
        
        return success;
    }

    /**
     * Supprime une clÃƒÆ’Ã‚Â©
     * @param {string} key - ClÃƒÆ’Ã‚Â© ÃƒÆ’Ã‚Â  supprimer
     * @returns {boolean} SuccÃƒÆ’Ã‚Â¨s
     */
    delete(key) {
        if (!this.data.hasOwnProperty(key)) {
            return false;
        }
        
        const oldValue = this.data[key];
        delete this.data[key];
        
        this.meta.dirty = true;
        this.meta.lastModified = Date.now();
        
        if (this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:deleted`, {
                key,
                oldValue,
                model: this
            });
        }
        
        return true;
    }

    /**
     * RÃƒÆ’Ã‚Â©initialise le modÃƒÆ’Ã‚Â¨le
     * @param {Object} newData - Nouvelles donnÃƒÆ’Ã‚Â©es (optionnel)
     */
    reset(newData = {}) {
        const oldData = { ...this.data };
        
        this.data = { ...newData };
        this.meta.dirty = false;
        this.meta.lastModified = Date.now();
        
        if (this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:reset`, {
                oldData,
                newData: this.data,
                model: this
            });
        }
    }

    // ========================================================================
    // NAVIGATION PROFONDE
    // ========================================================================

    /**
     * RÃƒÆ’Ã‚Â©cupÃƒÆ’Ã‚Â¨re une valeur en profondeur
     * @private
     */
    _getDeep(obj, keys, defaultValue) {
        let current = obj;
        
        for (const key of keys) {
            if (current === null || current === undefined) {
                return defaultValue;
            }
            
            current = current[key];
        }
        
        return current !== undefined ? current : defaultValue;
    }

    /**
     * DÃƒÆ’Ã‚Â©finit une valeur en profondeur
     * @private
     */
    _setDeep(obj, keys, value) {
        if (keys.length === 0) return;
        
        const lastKey = keys.pop();
        let current = obj;
        
        for (const key of keys) {
            if (!(key in current) || typeof current[key] !== 'object') {
                current[key] = {};
            }
            current = current[key];
        }
        
        current[lastKey] = value;
    }

    // ========================================================================
    // VALIDATION
    // ========================================================================

    /**
     * Valide un champ
     * @param {string} field - Champ ÃƒÆ’Ã‚Â  valider
     * @param {*} value - Valeur
     * @returns {boolean} RÃƒÆ’Ã‚Â©sultat de validation
     */
    validateField(field, value) {
        const rules = this.validationRules[field];
        if (!rules) return true;
        
        const rulesArray = Array.isArray(rules) ? rules : [rules];
        
        for (const rule of rulesArray) {
            try {
                const result = rule(value, this.data);
                if (result !== true) {
                    this.meta.errors[field] = result || `Validation ÃƒÆ’Ã‚Â©chouÃƒÆ’Ã‚Â©e pour ${field}`;
                    return false;
                }
            } catch (error) {
                this.meta.errors[field] = error.message;
                return false;
            }
        }
        
        delete this.meta.errors[field];
        return true;
    }

    /**
     * Valide toutes les donnÃƒÆ’Ã‚Â©es
     * @returns {boolean} RÃƒÆ’Ã‚Â©sultat global de la validation
     */
    validateAll() {
        this.meta.errors = {};
        let isValid = true;
        
        for (const field in this.validationRules) {
            if (this.data.hasOwnProperty(field)) {
                if (!this.validateField(field, this.data[field])) {
                    isValid = false;
                }
            }
        }
        
        return isValid;
    }

    /**
     * RÃƒÆ’Ã‚Â©cupÃƒÆ’Ã‚Â¨re les erreurs de validation
     * @returns {Object} Erreurs par champ
     */
    getErrors() {
        return { ...this.meta.errors };
    }

    /**
     * VÃƒÆ’Ã‚Â©rifie si le modÃƒÆ’Ã‚Â¨le est valide
     * @returns {boolean} ValiditÃƒÆ’Ã‚Â©
     */
    isValid() {
        return Object.keys(this.meta.errors).length === 0;
    }

    // ========================================================================
    // PERSISTENCE
    // ========================================================================

    /**
     * Sauvegarde les donnÃƒÆ’Ã‚Â©es
     * @returns {Promise<boolean>} SuccÃƒÆ’Ã‚Â¨s de la sauvegarde
     */
    async persist() {
        try {
            this.logger.debug(`${this.constructor.name}`, 
                `Persisting to ${this.config.storageType}`);
            
            const dataToSave = {
                data: this.data,
                meta: {
                    version: this.meta.version,
                    lastModified: Date.now(),
                    savedAt: new Date().toISOString()
                }
            };
            
            if (this.config.storageType === 'localStorage') {
                localStorage.setItem(
                    this.config.persistKey,
                    JSON.stringify(dataToSave)
                );
            } else if (this.config.storageType === 'indexedDB') {
                // TODO: ImplÃƒÆ’Ã‚Â©menter IndexedDB
                console.warn('IndexedDB not implemented yet');
            }
            
            this.meta.dirty = false;
            
            if (this.eventBus) {
                this.eventBus.emit(`${this.config.eventPrefix}:persisted`, {
                    model: this
                });
            }
            
            return true;
            
        } catch (error) {
            this.logger.error(`${this.constructor.name}`, 
                `Persist error: ${error.message}`);
            return false;
        }
    }

    /**
     * Charge les donnÃƒÆ’Ã‚Â©es depuis le storage
     * @returns {Promise<boolean>} SuccÃƒÆ’Ã‚Â¨s du chargement
     */
    async load() {
        try {
            this.logger.debug(`${this.constructor.name}`, 
                `Loading from ${this.config.storageType}`);
            
            let savedData = null;
            
            if (this.config.storageType === 'localStorage') {
                const stored = localStorage.getItem(this.config.persistKey);
                if (stored) {
                    savedData = JSON.parse(stored);
                }
            }
            
            if (savedData && savedData.data) {
                this.data = savedData.data;
                this.meta.lastModified = savedData.meta.lastModified;
                this.meta.version = savedData.meta.version || 1;
                this.meta.dirty = false;
                
                if (this.eventBus) {
                    this.eventBus.emit(`${this.config.eventPrefix}:loaded`, {
                        model: this
                    });
                }
                
                return true;
            }
            
            return false;
            
        } catch (error) {
            this.logger.error(`${this.constructor.name}`, 
                `Load error: ${error.message}`);
            return false;
        }
    }

    /**
     * Persistence avec debounce
     * @private
     */
    _debouncedPersist() {
        if (this._persistTimer) {
            clearTimeout(this._persistTimer);
        }
        
        this._persistTimer = setTimeout(() => {
            this.persist();
        }, this.config.debounceMs);
    }

    // ========================================================================
    // HISTORIQUE
    // ========================================================================

    /**
     * Ajoute ÃƒÆ’Ã‚Â  l'historique
     * @private
     */
    _addToHistory(key, oldValue) {
        this.history.past.push({
            key,
            value: oldValue,
            timestamp: Date.now()
        });
        
        // Limiter la taille
        if (this.history.past.length > this.history.maxSize) {
            this.history.past.shift();
        }
        
        // Vider future
        this.history.future = [];
    }

    /**
     * Annuler la derniÃƒÆ’Ã‚Â¨re modification
     * @returns {boolean} SuccÃƒÆ’Ã‚Â¨s
     */
    undo() {
        if (this.history.past.length === 0) {
            return false;
        }
        
        const entry = this.history.past.pop();
        const currentValue = this.get(entry.key);
        
        this.history.future.push({
            key: entry.key,
            value: currentValue,
            timestamp: Date.now()
        });
        
        this.set(entry.key, entry.value, true);
        
        if (this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:undo`, {
                model: this
            });
        }
        
        return true;
    }

    /**
     * Refaire la derniÃƒÆ’Ã‚Â¨re modification annulÃƒÆ’Ã‚Â©e
     * @returns {boolean} SuccÃƒÆ’Ã‚Â¨s
     */
    redo() {
        if (this.history.future.length === 0) {
            return false;
        }
        
        const entry = this.history.future.pop();
        const currentValue = this.get(entry.key);
        
        this.history.past.push({
            key: entry.key,
            value: currentValue,
            timestamp: Date.now()
        });
        
        this.set(entry.key, entry.value, true);
        
        if (this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:redo`, {
                model: this
            });
        }
        
        return true;
    }

    // ========================================================================
    // MÃƒÆ’Ã¢â‚¬Â°THODES UTILITAIRES
    // ========================================================================

    /**
     * VÃƒÆ’Ã‚Â©rifie si le modÃƒÆ’Ã‚Â¨le a ÃƒÆ’Ã‚Â©tÃƒÆ’Ã‚Â© modifiÃƒÆ’Ã‚Â©
     * @returns {boolean}
     */
    isDirty() {
        return this.meta.dirty;
    }

    /**
     * RÃƒÆ’Ã‚Â©initialise le flag dirty
     */
    markClean() {
        this.meta.dirty = false;
    }

    /**
     * Obtient toutes les donnÃƒÆ’Ã‚Â©es
     * @returns {Object}
     */
    getData() {
        return { ...this.data };
    }

    /**
     * Convertit le modÃƒÆ’Ã‚Â¨le en JSON
     * @returns {Object}
     */
    toJSON() {
        return {
            data: this.data,
            meta: this.meta
        };
    }

    /**
     * Nettoie les ressources
     */
    destroy() {
        if (this._persistTimer) {
            clearTimeout(this._persistTimer);
        }
        if (this._validationTimer) {
            clearTimeout(this._validationTimer);
        }
        
        this.logger.debug(`${this.constructor.name}`, 'Model destroyed');
    }

    // ========================================================================
    // MÉTHODES ADDITIONNELLES POUR STATEMODEL
    // ========================================================================

    /**
     * Initialise le modèle avec des données
     * @param {Object} data - Données d'initialisation
     */
    initialize(data) {
        if (data && typeof data === 'object') {
            Object.assign(this.data, data);
        }
        this.meta.initialized = true;
        this.meta.lastModified = Date.now();
    }

    /**
     * Ajoute une règle de validation
     * @param {string} field - Champ à valider
     * @param {Function} rule - Fonction de validation (value) => true|string
     */
    addValidationRule(field, rule) {
        if (typeof rule !== 'function') {
            this.logger.warn(`${this.constructor.name}`, `Invalid validation rule for ${field}`);
            return;
        }
        this.validationRules[field] = rule;
    }

    /**
     * Ajoute une propriété calculée
     * @param {string} name - Nom de la propriété
     * @param {Function} fn - Fonction de calcul (data) => value
     */
    addComputed(name, fn) {
        if (!this.computedProperties) {
            this.computedProperties = {};
        }
        if (typeof fn !== 'function') {
            this.logger.warn(`${this.constructor.name}`, `Invalid computed function for ${name}`);
            return;
        }
        this.computedProperties[name] = fn;
    }

    /**
     * Observe un champ et exécute un callback lors de changements
     * @param {string} key - Champ à observer
     * @param {Function} callback - Callback (newValue, oldValue) => void
     */
    watch(key, callback) {
        if (!this.watchers) {
            this.watchers = {};
        }
        if (!this.watchers[key]) {
            this.watchers[key] = [];
        }
        if (typeof callback !== 'function') {
            this.logger.warn(`${this.constructor.name}`, `Invalid watcher callback for ${key}`);
            return;
        }
        this.watchers[key].push(callback);
        
        // Trigger watchers when setting values
        const originalSet = this.set.bind(this);
        this.set = (k, value, silent) => {
            const oldValue = this.get(k);
            const result = originalSet(k, value, silent);
            
            if (result && this.watchers && this.watchers[k]) {
                this.watchers[k].forEach(watcher => {
                    try {
                        watcher(value, oldValue);
                    } catch (error) {
                        this.logger.error(`${this.constructor.name}`, 
                            `Watcher error for ${k}: ${error.message}`);
                    }
                });
            }
            
            return result;
        };
    }

    /**
     * Valide les données du modèle
     * @returns {Object} {valid: boolean, errors: Array}
     */
    validate() {
        const errors = [];
        
        // Vérifier si des validateurs sont définis
        if (this.validationRules && typeof this.validationRules === 'object') {
            for (const [key, validator] of Object.entries(this.validationRules)) {
                const value = this.get(key);
                
                if (typeof validator === 'function') {
                    const result = validator(value);
                    if (result !== true) {
                        errors.push(result || `Validation failed for ${key}`);
                    }
                }
            }
        }
        
        this.meta.errors = errors.length > 0 ? { validation: errors } : {};
        
        return {
            valid: errors.length === 0,
            errors
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BaseModel;
}

if (typeof window !== 'undefined') {
    window.BaseModel = BaseModel;
}

// Export par défaut
window.BaseModel = BaseModel;
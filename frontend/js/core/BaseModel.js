// ============================================================================
// Fichier: frontend/js/core/BaseModel.js
// Version: v3.1.0 - LOGGER ROBUSTE
// Date: 2025-10-30
// ============================================================================
// CORRECTIONS v3.1.0:
// ✅ Accepte eventBus et logger comme paramètres
// ✅ Fallback logger robuste avec toutes méthodes
// ✅ Compatible avec anciens modèles (backward compatible)
// ============================================================================

class BaseModel {
    /**
     * Constructeur du modèle de base
     * @param {EventBus} eventBus - Bus d'événements
     * @param {Logger} logger - Logger (optionnel)
     * @param {Object} initialData - Données initiales (optionnel)
     * @param {Object} options - Options de configuration
     */
    constructor(eventBus, logger, initialData = {}, options = {}) {
        // Support ancien style (backward compatible)
        // Si eventBus est un objet et pas une instance EventBus, c'est initialData
        if (eventBus && typeof eventBus === 'object' && !eventBus.emit) {
            options = logger || {};
            initialData = eventBus;
            logger = null;
            eventBus = null;
        }
        
        // EventBus
        this.eventBus = eventBus || window.eventBus || null;
        
        // Logger avec fallback robuste
        this.logger = logger || this.createFallbackLogger();
        
        // Données du modèle
        this.data = { ...initialData };
        
        // Métadonnées
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
            storageType: options.storageType || 'localStorage',
            validateOnSet: options.validateOnSet !== false,
            eventPrefix: options.eventPrefix || this.constructor.name.toLowerCase(),
            debounceMs: options.debounceMs || 300,
            ...options
        };
        
        // Règles de validation (à définir dans les classes filles)
        this.validationRules = {};
        
        // Historique des changements
        this.history = {
            past: [],
            future: [],
            maxSize: options.historyMaxSize || 50
        };
        
        // Timers pour debounce
        this._persistTimer = null;
        this._validationTimer = null;
        
        this.log('debug', `${this.constructor.name}`, 'Model created');
    }
    
    /**
     * Crée un logger fallback robuste
     */
    createFallbackLogger() {
        // Si window.logger existe et a les bonnes méthodes
        if (window.logger && typeof window.logger.info === 'function') {
            return window.logger;
        }
        
        // Fallback vers console avec interface complète
        return {
            debug: (...args) => console.log('[DEBUG]', ...args),
            info: (...args) => console.info('[INFO]', ...args),
            warn: (...args) => console.warn('[WARN]', ...args),
            error: (...args) => console.error('[ERROR]', ...args),
            log: (...args) => console.log(...args)
        };
    }
    
    /**
     * Log sécurisé
     */
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        } else {
            console[level]?.(...args) || console.log(...args);
        }
    }

    // ========================================================================
    // MÉTHODES D'ACCÈS AUX DONNÉES
    // ========================================================================

    get(key, defaultValue = undefined) {
        if (key.includes('.')) {
            return this._getDeep(this.data, key.split('.'), defaultValue);
        }
        return this.data.hasOwnProperty(key) ? this.data[key] : defaultValue;
    }

    set(key, value, silent = false) {
        const oldValue = this.get(key);
        
        // Validation si activée
        if (this.config.validateOnSet && this.validationRules[key]) {
            if (!this.validateField(key, value)) {
                this.log('warn', `${this.constructor.name}`, `Validation failed for ${key}`);
                return false;
            }
        }
        
        // Sauvegarder dans historique
        if (this.config.enableHistory !== false) {
            this._addToHistory(key, oldValue);
        }
        
        // Définir la valeur
        if (key.includes('.')) {
            this._setDeep(this.data, key.split('.'), value);
        } else {
            this.data[key] = value;
        }
        
        // Mettre à jour métadonnées
        this.meta.dirty = true;
        this.meta.lastModified = Date.now();
        
        // Émettre événement
        if (!silent && this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:changed`, {
                key,
                value,
                oldValue,
                model: this
            });
        }
        
        // Auto-persist si activé
        if (this.config.autoPersist) {
            this._debouncePersist();
        }
        
        return true;
    }

    setMultiple(values, silent = false) {
        Object.entries(values).forEach(([key, value]) => {
            this.set(key, value, true);
        });
        
        if (!silent && this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:changed`, {
                keys: Object.keys(values),
                model: this
            });
        }
        
        return true;
    }

    getAll() {
        return { ...this.data };
    }

    reset() {
        this.data = {};
        this.meta.dirty = false;
        this.history.past = [];
        this.history.future = [];
        
        if (this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:reset`, { model: this });
        }
    }

    // ========================================================================
    // NAVIGATION PROFONDE
    // ========================================================================

    _getDeep(obj, path, defaultValue) {
        let current = obj;
        for (const key of path) {
            if (current && typeof current === 'object' && key in current) {
                current = current[key];
            } else {
                return defaultValue;
            }
        }
        return current;
    }

    _setDeep(obj, path, value) {
        const last = path.pop();
        let current = obj;
        
        for (const key of path) {
            if (!(key in current)) {
                current[key] = {};
            }
            current = current[key];
        }
        
        current[last] = value;
    }

    // ========================================================================
    // VALIDATION
    // ========================================================================

    validateField(key, value) {
        const rules = this.validationRules[key];
        if (!rules) return true;
        
        for (const [ruleName, ruleConfig] of Object.entries(rules)) {
            const validator = this._getValidator(ruleName);
            if (validator && !validator(value, ruleConfig)) {
                this.meta.errors[key] = {
                    rule: ruleName,
                    message: ruleConfig.message || `Validation failed for ${key}`
                };
                return false;
            }
        }
        
        delete this.meta.errors[key];
        return true;
    }

    validate() {
        this.meta.errors = {};
        let isValid = true;
        
        for (const key of Object.keys(this.validationRules)) {
            if (!this.validateField(key, this.get(key))) {
                isValid = false;
            }
        }
        
        return isValid;
    }

    _getValidator(ruleName) {
        const validators = {
            required: (value) => value !== null && value !== undefined && value !== '',
            minLength: (value, { min }) => String(value).length >= min,
            maxLength: (value, { max }) => String(value).length <= max,
            min: (value, { min }) => Number(value) >= min,
            max: (value, { max }) => Number(value) <= max,
            pattern: (value, { regex }) => new RegExp(regex).test(String(value)),
            email: (value) => /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(String(value))
        };
        
        return validators[ruleName];
    }

    // ========================================================================
    // PERSISTENCE
    // ========================================================================

    async persist() {
        if (!this.meta.dirty) return;
        
        try {
            const data = this.serialize();
            
            if (this.config.storageType === 'localStorage') {
                localStorage.setItem(this.config.persistKey, JSON.stringify(data));
            } else if (this.config.storageType === 'indexedDB') {
                await this._persistToIndexedDB(data);
            }
            
            this.meta.dirty = false;
            this.log('debug', `${this.constructor.name}`, 'Persisted to storage');
            
        } catch (error) {
            this.log('error', `${this.constructor.name}`, 'Persist failed:', error);
        }
    }

    async load() {
        try {
            let data;
            
            if (this.config.storageType === 'localStorage') {
                const stored = localStorage.getItem(this.config.persistKey);
                if (stored) {
                    data = JSON.parse(stored);
                }
            } else if (this.config.storageType === 'indexedDB') {
                data = await this._loadFromIndexedDB();
            }
            
            if (data) {
                this.deserialize(data);
                this.log('debug', `${this.constructor.name}`, 'Loaded from storage');
                return true;
            }
            
        } catch (error) {
            this.log('error', `${this.constructor.name}`, 'Load failed:', error);
        }
        
        return false;
    }

    _debouncePersist() {
        clearTimeout(this._persistTimer);
        this._persistTimer = setTimeout(() => {
            this.persist();
        }, this.config.debounceMs);
    }

    // ========================================================================
    // SERIALIZATION
    // ========================================================================

    serialize() {
        return {
            data: this.data,
            meta: {
                version: this.meta.version,
                lastModified: this.meta.lastModified
            }
        };
    }

    deserialize(serialized) {
        if (serialized.data) {
            this.data = serialized.data;
        }
        if (serialized.meta) {
            this.meta.version = serialized.meta.version || 1;
            this.meta.lastModified = serialized.meta.lastModified;
        }
        this.meta.dirty = false;
    }

    // ========================================================================
    // HISTORIQUE (UNDO/REDO)
    // ========================================================================

    _addToHistory(key, oldValue) {
        this.history.past.push({ key, value: oldValue, timestamp: Date.now() });
        
        if (this.history.past.length > this.history.maxSize) {
            this.history.past.shift();
        }
        
        this.history.future = [];
    }

    undo() {
        if (this.history.past.length === 0) return false;
        
        const lastChange = this.history.past.pop();
        const currentValue = this.get(lastChange.key);
        
        this.history.future.push({
            key: lastChange.key,
            value: currentValue,
            timestamp: Date.now()
        });
        
        this.set(lastChange.key, lastChange.value, true);
        
        if (this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:undo`, { 
                key: lastChange.key,
                model: this 
            });
        }
        
        return true;
    }

    redo() {
        if (this.history.future.length === 0) return false;
        
        const nextChange = this.history.future.pop();
        const currentValue = this.get(nextChange.key);
        
        this.history.past.push({
            key: nextChange.key,
            value: currentValue,
            timestamp: Date.now()
        });
        
        this.set(nextChange.key, nextChange.value, true);
        
        if (this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:redo`, { 
                key: nextChange.key,
                model: this 
            });
        }
        
        return true;
    }

    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================

    on(event, callback) {
        if (this.eventBus) {
            this.eventBus.on(`${this.config.eventPrefix}:${event}`, callback);
        }
    }

    off(event, callback) {
        if (this.eventBus) {
            this.eventBus.off(`${this.config.eventPrefix}:${event}`, callback);
        }
    }

    emit(event, data) {
        if (this.eventBus) {
            this.eventBus.emit(`${this.config.eventPrefix}:${event}`, data);
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    toJSON() {
        return this.serialize();
    }

    toString() {
        return JSON.stringify(this.data, null, 2);
    }

    clone() {
        const ModelClass = this.constructor;
        return new ModelClass(this.eventBus, this.logger, { ...this.data }, { ...this.config });
    }

    destroy() {
        clearTimeout(this._persistTimer);
        clearTimeout(this._validationTimer);
        
        if (this.meta.dirty && this.config.autoPersist) {
            this.persist();
        }
        
        this.log('debug', `${this.constructor.name}`, 'Model destroyed');
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
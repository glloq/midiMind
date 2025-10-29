// ============================================================================
// Fichier: frontend/js/core/BaseModel.js
// Version: v3.0.3 - CORRECTED LOGGER INSTANCE
// Date: 2025-10-24
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// CORRECTIONS v3.0.3:
// ✅ Fixed logger initialization: use window.logger (instance) instead of window.logger (class)
// ✅ This fixes "TypeError: this.logger.debug is not a function" error
// ============================================================================
// CORRECTIONS v3.0.2:
// ✓ Removed ES6 import statements (not compatible with script tags)
// ✓ Using global variables instead (EventBus, Logger, etc.)
// ✓ Compatible with non-module script loading
// ============================================================================
// Description:
//   Classe de base pour tous les modèles de données de l'application.
//   Fournit la structure commune pour la gestion d'état, validation,
//   persistence et événements.
//
// Fonctionnalités:
//   - Gestion d'état avec get/set
//   - Validation de données
//   - Persistence (localStorage/IndexedDB)
//   - Système d'événements
//   - Historique des changements
//
// Architecture:
//   - Pattern Observer pour notifications
//   - Validation déclarative avec règles
//   - Storage abstrait (localStorage par défaut)
// ============================================================================

/**
 * @class BaseModel
 * @description Classe de base abstraite pour tous les modèles de données
 */
class BaseModel {
    /**
     * Constructeur du modèle de base
     * @param {Object} initialData - Données initiales
     * @param {Object} options - Options de configuration
     */
    constructor(initialData = {}, options = {}) {
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
            storageType: options.storageType || 'localStorage', // 'localStorage' | 'indexedDB'
            validateOnSet: options.validateOnSet !== false,
            eventPrefix: options.eventPrefix || this.constructor.name.toLowerCase(),
            debounceMs: options.debounceMs || 300,
            ...options
        };
        
        // Règles de validation (à définir dans les classes filles)
        this.validationRules = {};
        
        // Historique des changements (pour undo/redo)
        this.history = {
            past: [],
            future: [],
            maxSize: options.historyMaxSize || 50
        };
        
        // EventBus pour communication (global)
        this.eventBus = window.EventBus || null;
        
        // Logger (global)
        this.logger = window.logger || console;
        
        // Timers pour debounce
        this._persistTimer = null;
        this._validationTimer = null;
        
        this.logger.debug(`${this.constructor.name}`, 'Model created');
    }

    // ========================================================================
    // MÉTHODES D'ACCÈS AUX DONNÉES
    // ========================================================================

    /**
     * Récupère une valeur du modèle
     * @param {string} key - Clé à récupérer (supporte notation pointée: 'user.name')
     * @param {*} defaultValue - Valeur par défaut si non trouvée
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
     * Définit une valeur dans le modèle
     * @param {string} key - Clé à définir
     * @param {*} value - Valeur
     * @param {boolean} silent - Si true, ne déclenche pas d'événements
     * @returns {boolean} Succès
     */
    set(key, value, silent = false) {
        const oldValue = this.get(key);
        
        // Validation si activée
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
        
        // Auto-persistence
        if (this.config.autoPersist) {
            this._debouncedPersist();
        }
        
        return true;
    }

    /**
     * Met à jour plusieurs valeurs
     * @param {Object} updates - Objet avec les mises à jour
     * @param {boolean} silent - Si true, ne déclenche pas d'événements
     * @returns {boolean} Succès
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
        
        // Émettre un seul événement pour toutes les modifications
        if (!silent && this.eventBus && success) {
            this.eventBus.emit(`${this.config.eventPrefix}:updated`, {
                updates,
                model: this
            });
        }
        
        return success;
    }

    /**
     * Supprime une clé
     * @param {string} key - Clé à supprimer
     * @returns {boolean} Succès
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
     * Réinitialise le modèle
     * @param {Object} newData - Nouvelles données (optionnel)
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
     * Récupère une valeur en profondeur
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
     * Définit une valeur en profondeur
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
     * @param {string} field - Champ à valider
     * @param {*} value - Valeur
     * @returns {boolean} Résultat de validation
     */
    validateField(field, value) {
        const rules = this.validationRules[field];
        if (!rules) return true;
        
        const rulesArray = Array.isArray(rules) ? rules : [rules];
        
        for (const rule of rulesArray) {
            try {
                const result = rule(value, this.data);
                if (result !== true) {
                    this.meta.errors[field] = result || `Validation échouée pour ${field}`;
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
     * Valide toutes les données
     * @returns {boolean} Résultat global de la validation
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
     * Récupère les erreurs de validation
     * @returns {Object} Erreurs par champ
     */
    getErrors() {
        return { ...this.meta.errors };
    }

    /**
     * Vérifie si le modèle est valide
     * @returns {boolean} Validité
     */
    isValid() {
        return Object.keys(this.meta.errors).length === 0;
    }

    // ========================================================================
    // PERSISTENCE
    // ========================================================================

    /**
     * Sauvegarde les données
     * @returns {Promise<boolean>} Succès de la sauvegarde
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
                // TODO: Implémenter IndexedDB
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
     * Charge les données depuis le storage
     * @returns {Promise<boolean>} Succès du chargement
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
     * Ajoute à l'historique
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
     * Annuler la dernière modification
     * @returns {boolean} Succès
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
     * Refaire la dernière modification annulée
     * @returns {boolean} Succès
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
    // MÉTHODES UTILITAIRES
    // ========================================================================

    /**
     * Vérifie si le modèle a été modifié
     * @returns {boolean}
     */
    isDirty() {
        return this.meta.dirty;
    }

    /**
     * Réinitialise le flag dirty
     */
    markClean() {
        this.meta.dirty = false;
    }

    /**
     * Obtient toutes les données
     * @returns {Object}
     */
    getData() {
        return { ...this.data };
    }

    /**
     * Convertit le modèle en JSON
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
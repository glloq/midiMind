// ============================================================================
// Fichier: frontend/js/core/BaseView.js
// Version: v3.2.0 - FIXED EVENTBUS FALLBACKS
// Date: 2025-10-31
// ============================================================================
// CORRECTIONS v3.2.0:
// ✅ CRITIQUE: Fallback robuste pour eventBus (window.eventBus)
// ✅ Validation eventBus avant utilisation
// ✅ Protection contre eventBus null
// ============================================================================

/**
 * @class BaseView
 * @description Classe de base abstraite pour toutes les vues
 */
class BaseView {
    /**
     * Constructeur de la vue de base
     * @param {string|HTMLElement} containerId - ID du conteneur ou élément DOM
     * @param {EventBus} eventBus - Bus d'événements global
     */
    constructor(containerId, eventBus) {
        // Résoudre le conteneur
        this.container = this.resolveContainer(containerId);
        
        // ✅ CRITIQUE: EventBus avec fallback robuste
        this.eventBus = eventBus || window.eventBus || null;
        
        // Validation EventBus
        if (!this.eventBus) {
            console.error(`[${this.constructor.name}] AVERTISSEMENT: EventBus non disponible`);
        }
        
        // Formatter (utilise global si disponible)
        this.formatter = window.Formatter || null;
        
        // Template et données
        this.template = ''; // Template HTML de base (à surcharger)
        this.data = {}; // Données actuelles de la vue
        this.previousData = {}; // Données précédentes pour comparaison
        
        // État de la vue
        this.state = {
            isVisible: false,
            isRendered: false,
            isDestroyed: false,
            lastRender: null
        };
        
        // Configuration
        this.config = {
            autoRender: true,
            preserveState: false,
            sanitizeHTML: true,
            debounceRender: 0, // ms
            trackChanges: true,
            name: this.constructor.name
        };
        
        // Gestionnaires d'événements
        this.eventHandlers = new Map();
        this.domEventListeners = [];
        
        // Cache et performance
        this.renderCache = new Map();
        this.renderQueue = null;
        
        // Éléments DOM cachés pour performance
        this.elements = {};
        
        // Hooks de cycle de vie
        this.lifecycleHooks = {
            beforeRender: [],
            afterRender: [],
            beforeShow: [],
            afterShow: [],
            beforeHide: [],
            afterHide: [],
            beforeDestroy: [],
            afterDestroy: []
        };
        
        // Logger par défaut
        this.logger = window.logger || console;
    }
    
    // ========================================================================
    // INITIALISATION / LIFECYCLE
    // ========================================================================
    
    /**
     * Initialisation de la vue
     */
    initialize() {
        if (!this.container) {
            console.error(`[${this.config.name}] Container not found`);
            return;
        }
        
        // Hook personnalisé
        if (typeof this.onInitialize === 'function') {
            this.onInitialize();
        }
        
        // Rendu initial si auto
        if (this.config.autoRender) {
            this.render();
        }
    }
    
    /**
     * Résout le conteneur DOM
     * @param {string|HTMLElement} containerId - ID ou élément
     * @returns {HTMLElement|null}
     */
    resolveContainer(containerId) {
        if (typeof containerId === 'string') {
            // ID ou sélecteur
            let element = document.getElementById(containerId);
            if (!element) {
                element = document.querySelector(containerId);
            }
            return element;
        } else if (containerId instanceof HTMLElement) {
            return containerId;
        }
        return null;
    }
    
    /**
     * Détruit la vue
     */
    destroy() {
        if (this.state.isDestroyed) {
            return;
        }
        
        this.runHooks('beforeDestroy');
        
        // Nettoyer les événements DOM
        this.removeAllEventListeners();
        
        // Nettoyer les event handlers
        this.eventHandlers.clear();
        
        // Vider le conteneur
        if (this.container) {
            this.container.innerHTML = '';
        }
        
        // Nettoyer le cache
        this.renderCache.clear();
        
        // Hook personnalisé
        if (typeof this.onDestroy === 'function') {
            this.onDestroy();
        }
        
        this.state.isDestroyed = true;
        this.state.isVisible = false;
        
        this.runHooks('afterDestroy');
    }
    
    // ========================================================================
    // RENDU
    // ========================================================================
    
    /**
     * Rend la vue
     * @param {Object} data - Données à rendre
     * @param {Object} options - Options de rendu
     */
    render(data = null, options = {}) {
        if (this.state.isDestroyed) {
            console.warn(`[${this.config.name}] Cannot render destroyed view`);
            return;
        }
        
        if (!this.container) {
            console.error(`[${this.config.name}] No container for rendering`);
            return;
        }
        
        // Mettre à jour les données
        if (data) {
            this.previousData = { ...this.data };
            this.data = { ...this.data, ...data };
        }
        
        // Vérifier si re-rendu nécessaire
        if (!options.force && this.config.trackChanges && !this.shouldRerender(data)) {
            return;
        }
        
        // Debounce si configuré
        if (this.config.debounceRender > 0 && !options.immediate) {
            this.debounceRender(data, options);
            return;
        }
        
        try {
            this.runHooks('beforeRender');
            
            // Construire le template
            const html = this.buildTemplate();
            
            // Sanitize si configuré
            const sanitized = this.config.sanitizeHTML 
                ? this.sanitizeHTML(html) 
                : html;
            
            // Injecter dans le DOM
            this.container.innerHTML = sanitized;
            
            // Cacher éléments DOM
            this.cacheElements();
            
            // Attacher événements
            this.attachEvents();
            
            this.state.isRendered = true;
            this.state.lastRender = Date.now();
            
            this.runHooks('afterRender');
            
            // Hook personnalisé
            if (typeof this.onRender === 'function') {
                this.onRender();
            }
            
            // Émettre événement
            this.emit('view:rendered', {
                view: this.config.name,
                data: this.data
            });
            
        } catch (error) {
            this.handleRenderError(error, data);
        }
    }
    
    /**
     * Construit le template HTML
     * @returns {string} HTML
     */
    buildTemplate() {
        // À surcharger dans les classes filles
        return this.template || '<div>No template defined</div>';
    }
    
    /**
     * Vérifie si un re-rendu est nécessaire
     * @param {Object} newData - Nouvelles données
     * @returns {boolean}
     */
    shouldRerender(newData) {
        if (!newData || !this.state.isRendered) return true;
        return !this.deepEqual(this.previousData, this.data);
    }
    
    /**
     * Debounce du rendu
     * @param {Object} data - Données
     * @param {Object} options - Options
     */
    debounceRender(data, options) {
        clearTimeout(this.renderQueue);
        this.renderQueue = setTimeout(() => {
            this.render(data, { ...options, immediate: true });
        }, this.config.debounceRender);
    }
    
    /**
     * Update la vue avec de nouvelles données
     * @param {Object} data - Données
     * @param {Object} options - Options
     */
    update(data, options = {}) {
        this.render(data, options);
    }
    
    // ========================================================================
    // AFFICHAGE / MASQUAGE
    // ========================================================================
    
    /**
     * Affiche la vue
     */
    show() {
        if (this.state.isDestroyed) {
            console.warn(`[${this.config.name}] Cannot show destroyed view`);
            return;
        }
        
        if (this.state.isVisible) return;
        
        this.runHooks('beforeShow');
        
        if (this.container) {
            this.container.style.display = '';
        }
        
        this.state.isVisible = true;
        
        this.runHooks('afterShow');
        this.emit('view:shown', { view: this.config.name });
        
        // Hook personnalisé
        if (typeof this.onShow === 'function') {
            this.onShow();
        }
    }
    
    /**
     * Masque la vue
     */
    hide() {
        if (this.state.isDestroyed) {
            console.warn(`[${this.config.name}] Cannot hide destroyed view`);
            return;
        }
        
        if (!this.state.isVisible) return;
        
        this.runHooks('beforeHide');
        
        if (this.container) {
            this.container.style.display = 'none';
        }
        
        this.state.isVisible = false;
        
        this.runHooks('afterHide');
        this.emit('view:hidden', { view: this.config.name });
        
        // Hook personnalisé
        if (typeof this.onHide === 'function') {
            this.onHide();
        }
    }
    
    /**
     * Toggle visibility
     */
    toggle() {
        if (this.state.isVisible) {
            this.hide();
        } else {
            this.show();
        }
    }
    
    // ========================================================================
    // ÉVÉNEMENTS DOM
    // ========================================================================
    
    /**
     * Attache les événements DOM
     * À surcharger dans les classes filles
     */
    attachEvents() {
        // À implémenter dans les classes filles
    }
    
    /**
     * Ajoute un listener d'événement DOM
     * @param {string|HTMLElement} selector - Sélecteur ou élément
     * @param {string} event - Type d'événement
     * @param {Function} handler - Gestionnaire
     * @param {boolean} useCapture - UseCapture
     */
    addEventListener(selector, event, handler, useCapture = false) {
        let element;
        
        if (typeof selector === 'string') {
            element = this.container.querySelector(selector);
        } else {
            element = selector;
        }
        
        if (!element) {
            console.warn(`[${this.config.name}] Element not found for event listener: ${selector}`);
            return;
        }
        
        element.addEventListener(event, handler, useCapture);
        
        // Garder trace pour nettoyage
        this.domEventListeners.push({
            element,
            event,
            handler,
            useCapture
        });
    }
    
    /**
     * Retire tous les listeners DOM
     */
    removeAllEventListeners() {
        this.domEventListeners.forEach(({ element, event, handler, useCapture }) => {
            element.removeEventListener(event, handler, useCapture);
        });
        this.domEventListeners = [];
    }
    
    // ========================================================================
    // ÉVÉNEMENTS EVENTBUS
    // ========================================================================
    
    /**
     * Émet un événement via EventBus
     * @param {string} eventName - Nom de l'événement
     * @param {*} data - Données
     */
    emit(eventName, data) {
        if (this.eventBus && typeof this.eventBus.emit === 'function') {
            this.eventBus.emit(eventName, data);
        }
    }
    
    /**
     * Écoute un événement via EventBus
     * @param {string} eventName - Nom de l'événement
     * @param {Function} handler - Gestionnaire
     */
    on(eventName, handler) {
        if (this.eventBus && typeof this.eventBus.on === 'function') {
            this.eventBus.on(eventName, handler);
            this.eventHandlers.set(eventName, handler);
        }
    }
    
    // ========================================================================
    // CACHE D'ÉLÉMENTS
    // ========================================================================
    
    /**
     * Cache des éléments DOM pour accès rapide
     * À surcharger dans les classes filles
     */
    cacheElements() {
        // À implémenter dans les classes filles
        // Exemple:
        // this.elements.button = this.container.querySelector('.btn');
    }
    
    /**
     * Obtient un élément caché
     * @param {string} name - Nom de l'élément
     * @returns {HTMLElement|null}
     */
    getElement(name) {
        return this.elements[name] || null;
    }
    
    // ========================================================================
    // HOOKS DE CYCLE DE VIE
    // ========================================================================
    
    /**
     * Enregistre un hook
     * @param {string} hookName - Nom du hook
     * @param {Function} fn - Fonction
     */
    addHook(hookName, fn) {
        if (this.lifecycleHooks[hookName]) {
            this.lifecycleHooks[hookName].push(fn);
        }
    }
    
    /**
     * Exécute les hooks d'un événement
     * @param {string} hookName - Nom du hook
     */
    runHooks(hookName) {
        if (this.lifecycleHooks[hookName]) {
            this.lifecycleHooks[hookName].forEach(fn => {
                try {
                    fn.call(this);
                } catch (error) {
                    console.error(`[${this.config.name}] Hook ${hookName} error:`, error);
                }
            });
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Formate une date
     * @param {Date|number} date - Date
     * @param {string} format - Format
     * @returns {string}
     */
    formatDate(date, format = 'default') {
        if (this.formatter && typeof this.formatter.formatDate === 'function') {
            return this.formatter.formatDate(date, format);
        }
        
        // Fallback basique
        const d = new Date(date);
        return d.toLocaleDateString();
    }
    
    /**
     * Formate une durée
     * @param {number} ms - Durée en millisecondes
     * @returns {string}
     */
    formatDuration(ms) {
        if (this.formatter && typeof this.formatter.formatDuration === 'function') {
            return this.formatter.formatDuration(ms);
        }
        
        // Fallback basique
        const seconds = Math.floor(ms / 1000);
        const minutes = Math.floor(seconds / 60);
        const secs = seconds % 60;
        return `${minutes}:${secs.toString().padStart(2, '0')}`;
    }
    
    /**
     * Formate une taille de fichier
     * @param {number} bytes - Taille en octets
     * @returns {string}
     */
    formatFileSize(bytes) {
        if (this.formatter && typeof this.formatter.formatFileSize === 'function') {
            return this.formatter.formatFileSize(bytes);
        }
        
        // Fallback basique
        const units = ['B', 'KB', 'MB', 'GB'];
        let size = bytes;
        let unitIndex = 0;
        
        while (size >= 1024 && unitIndex < units.length - 1) {
            size /= 1024;
            unitIndex++;
        }
        
        return `${size.toFixed(1)} ${units[unitIndex]}`;
    }
    
    /**
     * Échappe le HTML
     * @param {string} text - Texte
     * @returns {string}
     */
    escapeHTML(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    /**
     * Sanitize HTML (basique)
     * @param {string} html - HTML
     * @returns {string}
     */
    sanitizeHTML(html) {
        // Sanitization basique - à améliorer selon les besoins
        return html;
    }
    
    /**
     * Comparaison profonde d'objets
     * @param {Object} obj1 - Objet 1
     * @param {Object} obj2 - Objet 2
     * @returns {boolean}
     */
    deepEqual(obj1, obj2) {
        if (obj1 === obj2) return true;
        
        if (typeof obj1 !== 'object' || typeof obj2 !== 'object' || 
            obj1 === null || obj2 === null) {
            return false;
        }
        
        const keys1 = Object.keys(obj1);
        const keys2 = Object.keys(obj2);
        
        if (keys1.length !== keys2.length) return false;
        
        for (const key of keys1) {
            if (!keys2.includes(key)) return false;
            if (!this.deepEqual(obj1[key], obj2[key])) return false;
        }
        
        return true;
    }
    
    /**
     * Obtient une valeur imbriquée
     * @param {Object} obj - Objet
     * @param {string} path - Chemin (ex: 'user.name')
     * @returns {*}
     */
    getNestedValue(obj, path) {
        return path.split('.').reduce((current, key) => current?.[key], obj);
    }
    
    /**
     * Gère les erreurs de rendu
     * @param {Error} error - Erreur
     * @param {Object} data - Données
     */
    handleRenderError(error, data) {
        console.error(`[${this.config.name}] Render error:`, error);
        
        this.emit('view:render:error', {
            view: this.config.name,
            error: error.message,
            data
        });
        
        // Template d'erreur
        if (this.container) {
            this.container.innerHTML = this.getErrorTemplate(error);
        }
    }
    
    /**
     * Obtient le template d'erreur
     * @param {Error} error - Erreur
     * @returns {string}
     */
    getErrorTemplate(error) {
        return `
            <div class="view-error" style="padding: 20px; text-align: center; color: #dc3545;">
                <h3>Erreur de rendu</h3>
                <p>${this.escapeHTML(error.message)}</p>
                <button onclick="location.reload()" class="btn btn-primary">Recharger</button>
            </div>
        `;
    }
    
    /**
     * Obtient l'état de la vue
     * @returns {Object}
     */
    getState() {
        return {
            ...this.state,
            hasData: Object.keys(this.data).length > 0,
            hasContainer: !!this.container,
            eventListeners: this.domEventListeners.length
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BaseView;
}

if (typeof window !== 'undefined') {
    window.BaseView = BaseView;
}
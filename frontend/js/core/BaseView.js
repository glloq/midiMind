// ============================================================================
// Fichier: frontend/js/core/BaseView.js
// Version: v3.1.0 - NO ES6 IMPORTS
// Date: 2025-10-13
// ============================================================================
// CORRECTIONS v3.1.0:
// âœ… Suppression imports ES6 (import ... from ...)
// âœ… Utilisation variables globales (window.EventBus, window.Formatter)
// âœ… Compatible avec chargement <script> tags
// âœ… Architecture MVC maintenue
// ============================================================================
// Description:
//   Classe de base pour toutes les vues de l'application.
//   GÃ¨re le rendu HTML, la liaison d'Ã©vÃ©nements, l'affichage/masquage.
//   Fournit des mÃ©thodes utilitaires communes (formatage, dates, etc.)
//
// FonctionnalitÃ©s:
//   - Rendu de templates HTML
//   - Gestion du cycle de vie (show/hide/destroy)
//   - Liaison d'Ã©vÃ©nements DOM
//   - Mise Ã  jour rÃ©active
//   - Sanitization HTML
//   - Cache de rendu
//
// Auteur: MidiMind Team
// ============================================================================

/**
 * @class BaseView
 * @description Classe de base abstraite pour toutes les vues
 */
class BaseView {
    /**
     * Constructeur de la vue de base
     * @param {string|HTMLElement} containerId - ID du conteneur ou Ã©lÃ©ment DOM
     * @param {EventBus} eventBus - Bus d'Ã©vÃ©nements global
     */
    constructor(containerId, eventBus) {
        // RÃ©soudre le conteneur
        this.container = this.resolveContainer(containerId);
        
        // EventBus (utilise global si non fourni)
        this.eventBus = eventBus || window.EventBus;
        
        // Formatter (utilise global si disponible)
        this.formatter = window.Formatter || null;
        
        // Template et donnÃ©es
        this.template = ''; // Template HTML de base (Ã  surcharger)
        this.data = {}; // DonnÃ©es actuelles de la vue
        this.previousData = {}; // DonnÃ©es prÃ©cÃ©dentes pour comparaison
        
        // Ã‰tat de la vue
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
        
        // Gestionnaires d'Ã©vÃ©nements
        this.eventHandlers = new Map();
        this.domEventListeners = [];
        
        // Cache et performance
        this.renderCache = new Map();
        this.renderQueue = null;
        
        // Ã‰lÃ©ments DOM cachÃ©s pour performance
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
        
        // Hook personnalisÃ©
        if (typeof this.onInitialize === 'function') {
            this.onInitialize();
        }
        
        // Rendu initial si auto
        if (this.config.autoRender) {
            this.render();
        }
    }
    
    /**
     * RÃ©sout le conteneur DOM
     * @param {string|HTMLElement} containerId - ID ou Ã©lÃ©ment
     * @returns {HTMLElement|null}
     */
    resolveContainer(containerId) {
        if (typeof containerId === 'string') {
            // ID ou sÃ©lecteur
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
     * DÃ©truit la vue
     */
    destroy() {
        if (this.state.isDestroyed) {
            return;
        }
        
        this.runHooks('beforeDestroy');
        
        // Nettoyer les Ã©vÃ©nements DOM
        this.removeAllEventListeners();
        
        // Nettoyer les event handlers
        this.eventHandlers.clear();
        
        // Vider le conteneur
        if (this.container) {
            this.container.innerHTML = '';
        }
        
        // Nettoyer le cache
        this.renderCache.clear();
        
        // Hook personnalisÃ©
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
     * @param {Object} data - DonnÃ©es Ã  rendre
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
        
        // Mettre Ã  jour les donnÃ©es
        if (data) {
            this.previousData = { ...this.data };
            this.data = { ...this.data, ...data };
        }
        
        // VÃ©rifier si re-rendu nÃ©cessaire
        if (!options.force && this.config.trackChanges && !this.shouldRerender(data)) {
            return;
        }
        
        // Debounce si configurÃ©
        if (this.config.debounceRender > 0 && !options.immediate) {
            this.debounceRender(data, options);
            return;
        }
        
        try {
            this.runHooks('beforeRender');
            
            // Construire le template
            const html = this.buildTemplate();
            
            // Sanitize si configurÃ©
            const sanitized = this.config.sanitizeHTML 
                ? this.sanitizeHTML(html) 
                : html;
            
            // Injecter dans le DOM
            this.container.innerHTML = sanitized;
            
            // Cacher Ã©lÃ©ments DOM
            this.cacheElements();
            
            // Attacher Ã©vÃ©nements
            this.attachEvents();
            
            this.state.isRendered = true;
            this.state.lastRender = Date.now();
            
            this.runHooks('afterRender');
            
            // Hook personnalisÃ©
            if (typeof this.onRender === 'function') {
                this.onRender();
            }
            
            // Ã‰mettre Ã©vÃ©nement
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
        // Ã€ surcharger dans les classes filles
        if (typeof this.template === 'function') {
            return this.template(this.data);
        }
        return this.template || '';
    }
    
    /**
     * Met Ã  jour une partie de la vue
     * @param {string} selector - SÃ©lecteur CSS
     * @param {string} html - Nouveau HTML
     */
    updatePartial(selector, html) {
        if (!this.container) return;
        
        const element = this.container.querySelector(selector);
        if (element) {
            element.innerHTML = this.config.sanitizeHTML 
                ? this.sanitizeHTML(html) 
                : html;
        }
    }
    
    /**
     * DÃ©termine si un re-rendu est nÃ©cessaire
     * @param {Object} newData - Nouvelles donnÃ©es
     * @returns {boolean}
     */
    shouldRerender(newData) {
        if (!this.config.trackChanges) return true;
        if (!this.state.isRendered) return true;
        
        return !this.deepEqual(this.data, { ...this.data, ...newData });
    }
    
    /**
     * Debounce le rendu
     * @param {Object} data - DonnÃ©es
     * @param {Object} options - Options
     */
    debounceRender(data, options) {
        if (this.renderQueue) {
            clearTimeout(this.renderQueue);
        }
        
        this.renderQueue = setTimeout(() => {
            this.render(data, { ...options, force: true });
            this.renderQueue = null;
        }, this.config.debounceRender);
    }
    
    // ========================================================================
    // AFFICHAGE / MASQUAGE
    // ========================================================================
    
    /**
     * Affiche la vue
     */
    show() {
        if (this.state.isVisible) return;
        
        this.runHooks('beforeShow');
        
        if (this.container) {
            this.container.style.display = 'block';
            this.container.classList.remove('hidden');
        }
        
        this.state.isVisible = true;
        
        this.runHooks('afterShow');
        
        // Hook personnalisÃ©
        if (typeof this.onShow === 'function') {
            this.onShow();
        }
        
        this.emit('view:shown', { view: this.config.name });
    }
    
    /**
     * Masque la vue
     */
    hide() {
        if (!this.state.isVisible) return;
        
        this.runHooks('beforeHide');
        
        if (this.container) {
            this.container.style.display = 'none';
            this.container.classList.add('hidden');
        }
        
        this.state.isVisible = false;
        
        this.runHooks('afterHide');
        
        // Hook personnalisÃ©
        if (typeof this.onHide === 'function') {
            this.onHide();
        }
        
        this.emit('view:hidden', { view: this.config.name });
    }
    
    /**
     * Toggle affichage/masquage
     */
    toggle() {
        if (this.state.isVisible) {
            this.hide();
        } else {
            this.show();
        }
    }
    
    // ========================================================================
    // GESTION DES Ã‰VÃ‰NEMENTS
    // ========================================================================
    
    /**
     * Attache les Ã©vÃ©nements DOM
     * Ã€ surcharger dans les classes filles
     */
    attachEvents() {
        // Ã€ implÃ©menter dans les classes filles
    }
    
    /**
     * Ajoute un Ã©couteur d'Ã©vÃ©nement DOM
     * @param {string} selector - SÃ©lecteur CSS
     * @param {string} eventType - Type d'Ã©vÃ©nement
     * @param {Function} handler - Gestionnaire
     * @param {Object} options - Options
     */
    addEventListener(selector, eventType, handler, options = {}) {
        if (!this.container) return;
        
        const elements = this.container.querySelectorAll(selector);
        
        elements.forEach(element => {
            const wrappedHandler = (event) => {
                try {
                    handler.call(this, event);
                } catch (error) {
                    console.error(`[${this.config.name}] Event handler error:`, error);
                }
            };
            
            element.addEventListener(eventType, wrappedHandler, options);
            
            // Garder rÃ©fÃ©rence pour cleanup
            this.domEventListeners.push({
                element,
                eventType,
                handler: wrappedHandler,
                options
            });
        });
    }
    
    /**
     * Retire tous les Ã©couteurs d'Ã©vÃ©nements
     */
    removeAllEventListeners() {
        this.domEventListeners.forEach(({ element, eventType, handler, options }) => {
            element.removeEventListener(eventType, handler, options);
        });
        
        this.domEventListeners = [];
    }
    
    /**
     * Ã‰met un Ã©vÃ©nement via EventBus
     * @param {string} eventName - Nom de l'Ã©vÃ©nement
     * @param {*} data - DonnÃ©es
     */
    emit(eventName, data) {
        if (this.eventBus && typeof this.eventBus.emit === 'function') {
            this.eventBus.emit(eventName, data);
        }
    }
    
    /**
     * Ã‰coute un Ã©vÃ©nement via EventBus
     * @param {string} eventName - Nom de l'Ã©vÃ©nement
     * @param {Function} handler - Gestionnaire
     */
    on(eventName, handler) {
        if (this.eventBus && typeof this.eventBus.on === 'function') {
            this.eventBus.on(eventName, handler);
            this.eventHandlers.set(eventName, handler);
        }
    }
    
    // ========================================================================
    // CACHE D'Ã‰LÃ‰MENTS
    // ========================================================================
    
    /**
     * Cache des Ã©lÃ©ments DOM pour accÃ¨s rapide
     * Ã€ surcharger dans les classes filles
     */
    cacheElements() {
        // Ã€ implÃ©menter dans les classes filles
        // Exemple:
        // this.elements.button = this.container.querySelector('.btn');
    }
    
    /**
     * Obtient un Ã©lÃ©ment cachÃ©
     * @param {string} name - Nom de l'Ã©lÃ©ment
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
     * ExÃ©cute les hooks d'un Ã©vÃ©nement
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
     * Formate une durÃ©e
     * @param {number} ms - DurÃ©e en millisecondes
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
     * Ã‰chappe le HTML
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
        // Sanitization basique - Ã  amÃ©liorer selon les besoins
        const div = document.createElement('div');
        div.textContent = html;
        return div.innerHTML;
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
     * Obtient une valeur imbriquÃ©e
     * @param {Object} obj - Objet
     * @param {string} path - Chemin (ex: 'user.name')
     * @returns {*}
     */
    getNestedValue(obj, path) {
        return path.split('.').reduce((current, key) => current?.[key], obj);
    }
    
    /**
     * GÃ¨re les erreurs de rendu
     * @param {Error} error - Erreur
     * @param {Object} data - DonnÃ©es
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
     * Obtient l'Ã©tat de la vue
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
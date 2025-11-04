// ============================================================================
// Fichier: frontend/js/core/BaseView.js
// Version: v3.3.0 - FIXED LOG METHOD
// Date: 2025-11-04
// ============================================================================
// CORRECTIONS v3.2.1:
// CORRECTIONS v3.3.0:
// ✦ Ajout méthode async init() pour compatibilité avec FileView/KeyboardView
// ✦ Hook onInit() pour initialisation asynchrone
// ============================================================================
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Ajout mÃƒÆ’Ã‚Â©thode log() manquante (flexible signature)
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Compatible avec FileView et autres vues
// ============================================================================

/**
 * @class BaseView
 * @description Classe de base abstraite pour toutes les vues
 */
class BaseView {
    /**
     * Constructeur de la vue de base
     * @param {string|HTMLElement} containerId - ID du conteneur ou ÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©ment DOM
     * @param {EventBus} eventBus - Bus d'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements global
     */
    constructor(containerId, eventBus) {
        // RÃƒÆ’Ã‚Â©soudre le conteneur
        this.container = this.resolveContainer(containerId);
        
        // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ CRITIQUE: EventBus avec fallback robuste
        this.eventBus = eventBus || window.eventBus || null;
        
        // Validation EventBus
        if (!this.eventBus) {
            console.error(`[${this.constructor.name}] AVERTISSEMENT: EventBus non disponible`);
        }
        
        // Formatter (utilise global si disponible)
        this.formatter = window.Formatter || null;
        
        // Template et donnÃƒÆ’Ã‚Â©es
        this.template = ''; // Template HTML de base (ÃƒÆ’Ã‚Â  surcharger)
        this.data = {}; // DonnÃƒÆ’Ã‚Â©es actuelles de la vue
        this.previousData = {}; // DonnÃƒÆ’Ã‚Â©es prÃƒÆ’Ã‚Â©cÃƒÆ’Ã‚Â©dentes pour comparaison
        
        // ÃƒÆ’Ã¢â‚¬Â°tat de la vue
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
        
        // Gestionnaires d'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
        this.eventHandlers = new Map();
        this.domEventListeners = [];
        
        // Cache et performance
        this.renderCache = new Map();
        this.renderQueue = null;
        
        // ÃƒÆ’Ã¢â‚¬Â°lÃƒÆ’Ã‚Â©ments DOM cachÃƒÆ’Ã‚Â©s pour performance
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
        
        // Logger par dÃƒÆ’Ã‚Â©faut
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
        
        // Hook personnalisÃƒÆ’Ã‚Â©
        if (typeof this.onInitialize === 'function') {
            this.onInitialize();
        }
        
        // Rendu initial si auto
        if (this.config.autoRender) {
            this.render();
        }
    }
    
    /**
     * Méthode init() async pour compatibilité
     * Alias de initialize() mais async pour les vues qui en ont besoin
     */
    async init() {
        // Appeler initialize() si elle existe
        if (typeof this.initialize === 'function' && this.initialize !== this.init) {
            this.initialize();
        }
        
        // Hook pour initialisation async
        if (typeof this.onInit === 'function') {
            await this.onInit();
        }
    }
    
    /**
     * RÃƒÆ’Ã‚Â©sout le conteneur DOM
     * @param {string|HTMLElement} containerId - ID ou ÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©ment
     * @returns {HTMLElement|null}
     */
    resolveContainer(containerId) {
        if (typeof containerId === 'string') {
            // ID ou sÃƒÆ’Ã‚Â©lecteur
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
     * DÃƒÆ’Ã‚Â©truit la vue
     */
    destroy() {
        if (this.state.isDestroyed) {
            return;
        }
        
        this.runHooks('beforeDestroy');
        
        // Nettoyer les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements DOM
        this.removeAllEventListeners();
        
        // Nettoyer les event handlers
        this.eventHandlers.clear();
        
        // Vider le conteneur
        if (this.container) {
            this.container.innerHTML = '';
        }
        
        // Nettoyer le cache
        this.renderCache.clear();
        
        // Hook personnalisÃƒÆ’Ã‚Â©
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
     * @param {Object} data - DonnÃƒÆ’Ã‚Â©es ÃƒÆ’Ã‚Â  rendre
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
        
        // Mettre ÃƒÆ’Ã‚Â  jour les donnÃƒÆ’Ã‚Â©es
        if (data) {
            this.previousData = { ...this.data };
            this.data = { ...this.data, ...data };
        }
        
        // VÃƒÆ’Ã‚Â©rifier si re-rendu nÃƒÆ’Ã‚Â©cessaire
        if (!options.force && this.config.trackChanges && !this.shouldRerender(data)) {
            return;
        }
        
        // Debounce si configurÃƒÆ’Ã‚Â©
        if (this.config.debounceRender > 0 && !options.immediate) {
            this.debounceRender(data, options);
            return;
        }
        
        try {
            this.runHooks('beforeRender');
            
            // Construire le template
            const html = this.buildTemplate();
            
            // Sanitize si configurÃƒÆ’Ã‚Â©
            const sanitized = this.config.sanitizeHTML 
                ? this.sanitizeHTML(html) 
                : html;
            
            // Injecter dans le DOM
            this.container.innerHTML = sanitized;
            
            // Cacher ÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©ments DOM
            this.cacheElements();
            
            // Attacher ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
            this.attachEvents();
            
            this.state.isRendered = true;
            this.state.lastRender = Date.now();
            
            this.runHooks('afterRender');
            
            // Hook personnalisÃƒÆ’Ã‚Â©
            if (typeof this.onRender === 'function') {
                this.onRender();
            }
            
            // ÃƒÆ’Ã¢â‚¬Â°mettre ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
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
        // ÃƒÆ’Ã¢â€šÂ¬ surcharger dans les classes filles
        return this.template || '<div>No template defined</div>';
    }
    
    /**
     * VÃƒÆ’Ã‚Â©rifie si un re-rendu est nÃƒÆ’Ã‚Â©cessaire
     * @param {Object} newData - Nouvelles donnÃƒÆ’Ã‚Â©es
     * @returns {boolean}
     */
    shouldRerender(newData) {
        if (!newData || !this.state.isRendered) return true;
        return !this.deepEqual(this.previousData, this.data);
    }
    
    /**
     * Debounce du rendu
     * @param {Object} data - DonnÃƒÆ’Ã‚Â©es
     * @param {Object} options - Options
     */
    debounceRender(data, options) {
        if (this.renderQueue) {
            clearTimeout(this.renderQueue);
        }
        
        this.renderQueue = setTimeout(() => {
            this.render(data, { ...options, immediate: true });
            this.renderQueue = null;
        }, this.config.debounceRender);
    }
    
    /**
     * Met ÃƒÆ’Ã‚Â  jour des donnÃƒÆ’Ã‚Â©es spÃƒÆ’Ã‚Â©cifiques sans re-rendu complet
     * @param {Object} updates - Mises ÃƒÆ’Ã‚Â  jour
     */
    update(updates) {
        this.previousData = { ...this.data };
        this.data = { ...this.data, ...updates };
        
        // Hook pour mise ÃƒÆ’Ã‚Â  jour personnalisÃƒÆ’Ã‚Â©e
        if (typeof this.onUpdate === 'function') {
            this.onUpdate(updates);
        }
        
        this.emit('view:updated', {
            view: this.config.name,
            updates
        });
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
        
        if (this.state.isVisible) {
            return;
        }
        
        this.runHooks('beforeShow');
        
        if (this.container) {
            this.container.style.display = '';
            this.container.classList.remove('hidden');
        }
        
        this.state.isVisible = true;
        
        // Hook personnalisÃƒÆ’Ã‚Â©
        if (typeof this.onShow === 'function') {
            this.onShow();
        }
        
        this.runHooks('afterShow');
        
        this.emit('view:shown', {
            view: this.config.name
        });
    }
    
    /**
     * Masque la vue
     */
    hide() {
        if (!this.state.isVisible) {
            return;
        }
        
        this.runHooks('beforeHide');
        
        if (this.container) {
            this.container.style.display = 'none';
            this.container.classList.add('hidden');
        }
        
        this.state.isVisible = false;
        
        // Hook personnalisÃƒÆ’Ã‚Â©
        if (typeof this.onHide === 'function') {
            this.onHide();
        }
        
        this.runHooks('afterHide');
        
        this.emit('view:hidden', {
            view: this.config.name
        });
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
    // ÃƒÆ’Ã¢â‚¬Â°VÃƒÆ’Ã¢â‚¬Â°NEMENTS DOM
    // ========================================================================
    
    /**
     * Attache les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements DOM
     * ÃƒÆ’Ã¢â€šÂ¬ surcharger dans les classes filles
     */
    attachEvents() {
        // ÃƒÆ’Ã¢â€šÂ¬ implÃƒÆ’Ã‚Â©menter dans les classes filles
    }
    
    /**
     * Ajoute un event listener DOM
     * @param {HTMLElement|string} element - ÃƒÆ’Ã¢â‚¬Â°lÃƒÆ’Ã‚Â©ment ou sÃƒÆ’Ã‚Â©lecteur
     * @param {string} event - Type d'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {Function} handler - Gestionnaire
     * @param {Object} options - Options
     */
    addEventListener(element, event, handler, options = {}) {
        const el = typeof element === 'string'
            ? this.container.querySelector(element)
            : element;
        
        if (!el) {
            console.warn(`[${this.config.name}] Element not found for event ${event}`);
            return;
        }
        
        el.addEventListener(event, handler, options);
        
        // Garder rÃƒÆ’Ã‚Â©fÃƒÆ’Ã‚Â©rence pour nettoyage
        this.domEventListeners.push({
            element: el,
            event,
            handler,
            options
        });
    }
    
    /**
     * Ajoute un event listener avec dÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©gation
     * @param {string} selector - SÃƒÆ’Ã‚Â©lecteur CSS
     * @param {string} event - Type d'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {Function} handler - Gestionnaire
     */
    addDelegatedListener(selector, event, handler) {
        const delegatedHandler = (e) => {
            const target = e.target.closest(selector);
            if (target && this.container.contains(target)) {
                handler.call(target, e);
            }
        };
        
        this.container.addEventListener(event, delegatedHandler);
        
        this.domEventListeners.push({
            element: this.container,
            event,
            handler: delegatedHandler
        });
    }
    
    /**
     * Supprime tous les event listeners
     */
    removeAllEventListeners() {
        this.domEventListeners.forEach(({ element, event, handler, options }) => {
            element.removeEventListener(event, handler, options);
        });
        this.domEventListeners = [];
    }
    
    // ========================================================================
    // ÃƒÆ’Ã¢â‚¬Â°VÃƒÆ’Ã¢â‚¬Â°NEMENTS EVENTBUS
    // ========================================================================
    
    /**
     * ÃƒÆ’Ã¢â‚¬Â°met un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement via EventBus
     * @param {string} eventName - Nom de l'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {*} data - DonnÃƒÆ’Ã‚Â©es
     */
    emit(eventName, data) {
        if (this.eventBus && typeof this.eventBus.emit === 'function') {
            this.eventBus.emit(eventName, data);
        }
    }
    
    /**
     * ÃƒÆ’Ã¢â‚¬Â°coute un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement via EventBus
     * @param {string} eventName - Nom de l'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {Function} handler - Gestionnaire
     */
    on(eventName, handler) {
        if (this.eventBus && typeof this.eventBus.on === 'function') {
            this.eventBus.on(eventName, handler);
            this.eventHandlers.set(eventName, handler);
        }
    }
    
    // ========================================================================
    // CACHE D'ÃƒÆ’Ã¢â‚¬Â°LÃƒÆ’Ã¢â‚¬Â°MENTS
    // ========================================================================
    
    /**
     * Cache des ÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©ments DOM pour accÃƒÆ’Ã‚Â¨s rapide
     * ÃƒÆ’Ã¢â€šÂ¬ surcharger dans les classes filles
     */
    cacheElements() {
        // ÃƒÆ’Ã¢â€šÂ¬ implÃƒÆ’Ã‚Â©menter dans les classes filles
        // Exemple:
        // this.elements.button = this.container.querySelector('.btn');
    }
    
    /**
     * Obtient un ÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©ment cachÃƒÆ’Ã‚Â©
     * @param {string} name - Nom de l'ÃƒÆ’Ã‚Â©lÃƒÆ’Ã‚Â©ment
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
     * ExÃƒÆ’Ã‚Â©cute les hooks d'un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
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
    // LOGGING
    // ========================================================================
    
    /**
     * MÃƒÆ’Ã‚Â©thode de logging flexible
     * Supporte plusieurs signatures:
     * - log(level, message)
     * - log(level, source, message)
     * - log(level, source, message, data)
     */
    log(level, ...args) {
        if (!this.logger) return;
        
        // Si logger a une mÃƒÆ’Ã‚Â©thode pour ce niveau, l'utiliser directement
        if (typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        } else {
            // Sinon fallback sur console
            console.log(`[${level.toUpperCase()}]`, ...args);
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
     * Formate une durÃƒÆ’Ã‚Â©e
     * @param {number} ms - DurÃƒÆ’Ã‚Â©e en millisecondes
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
     * ÃƒÆ’Ã¢â‚¬Â°chappe le HTML
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
        // Sanitization basique - ÃƒÆ’Ã‚Â  amÃƒÆ’Ã‚Â©liorer selon les besoins
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
     * Obtient une valeur imbriquÃƒÆ’Ã‚Â©e
     * @param {Object} obj - Objet
     * @param {string} path - Chemin (ex: 'user.name')
     * @returns {*}
     */
    getNestedValue(obj, path) {
        return path.split('.').reduce((current, key) => current?.[key], obj);
    }
    
    /**
     * GÃƒÆ’Ã‚Â¨re les erreurs de rendu
     * @param {Error} error - Erreur
     * @param {Object} data - DonnÃƒÆ’Ã‚Â©es
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
     * Obtient l'ÃƒÆ’Ã‚Â©tat de la vue
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
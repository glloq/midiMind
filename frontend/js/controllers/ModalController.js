// ============================================================================
// Fichier: frontend/js/controllers/ModalController.js
// Chemin rÃƒÂ©el: frontend/js/controllers/ModalController.js
// Version: v3.4.1 - FIXED BACKEND SIGNATURE - AMÃƒâ€°LIORATIONS NOTIFICATIONS & VALIDATIONS
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.4.1:
// âœ… CRITIQUE: Ajout paramÃ¨tre backend au constructeur (6Ã¨me paramÃ¨tre)
// âœ… Fix: super() appelle BaseController avec backend
// âœ… this.backend initialisÃ© automatiquement via BaseController
// ============================================================================
// ============================================================================
// AMÃƒâ€°LIORATIONS v3.4.0:
// Ã¢Å“â€¦ Meilleure intÃƒÂ©gration NotificationManager
// Ã¢Å“â€¦ Validation formulaires robuste
// Ã¢Å“â€¦ MÃƒÂ©thodes helper pour modales communes
// Ã¢Å“â€¦ Gestion erreurs amÃƒÂ©liorÃƒÂ©e
// Ã¢Å“â€¦ Feedback utilisateur optimisÃƒÂ©
// Ã¢Å“â€¦ Support formulaires asynchrones
// ============================================================================

class ModalController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        // RÃƒÂ©fÃƒÂ©rence au backend
        // âœ… this.backend initialisÃ© automatiquement par BaseController
        
        // Registre des modales disponibles
        this.modals = {
            channelSettings: {
                id: 'channelModal',
                title: 'Attribution des canaux MIDI',
                size: 'large',
                closable: true,
                backdrop: true
            },
            instrumentConfig: {
                id: 'instrumentConfigModal',
                title: 'Configuration de l\'instrument',
                size: 'medium',
                closable: true,
                backdrop: true
            },
            playlistEditor: {
                id: 'playlistEditorModal',
                title: 'Ãƒâ€°diteur de playlist',
                size: 'large',
                closable: true,
                backdrop: true
            },
            fileUpload: {
                id: 'fileUploadModal',
                title: 'Upload de fichiers MIDI',
                size: 'medium',
                closable: true,
                backdrop: true
            },
            systemConfig: {
                id: 'systemConfigModal',
                title: 'Configuration systÃƒÂ¨me',
                size: 'extra-large',
                closable: true,
                backdrop: true
            },
            confirmation: {
                id: 'confirmationModal',
                title: 'Confirmation',
                size: 'small',
                closable: true,
                backdrop: true
            },
            information: {
                id: 'informationModal',
                title: 'Information',
                size: 'medium',
                closable: true,
                backdrop: true
            },
            error: {
                id: 'errorModal',
                title: 'Erreur',
                size: 'medium',
                closable: true,
                backdrop: true
            }
        };

        // Ãƒâ€°tat interne
        this._draggedPlaylistItem = null;
        
        // Ãƒâ€°tat des modales
        this.modalState = {
            activeModals: new Set(),
            modalStack: [],
            currentFocus: null,
            lastActiveElement: null,
            validationErrors: new Map()
        };
        
        // Configuration d'animation
        this.animationConfig = {
            duration: 300,
            easing: 'cubic-bezier(0.4, 0, 0.2, 1)',
            enableBackdrop: true,
            closeOnBackdropClick: true,
            closeOnEscape: true
        };
        
        // Cache et handlers
        this.contentCache = new Map();
        this.eventHandlers = new Map();
        this.validationRules = new Map();
        this.pendingActions = new Map();
        
        this.initializeModals();
        this.log('info', 'ModalController', 'Ã¢Å“â€¦ Initialized v3.4.0');
    }

    /**
     * Configuration des ÃƒÂ©vÃƒÂ©nements
     */
    bindEvents() {
        this.eventBus.on('modal:open', (data) => {
            this.open(data.modalId, data.options);
        });
        
        this.eventBus.on('modal:close', (data) => {
            this.close(data.modalId);
        });
        
        this.eventBus.on('modal:confirm', (data) => {
            this.confirm(data.message, data.callback, data.options);
        });
        
        // Invalider cache sur changements
        this.eventBus.on('instrument:updated', () => {
            this.invalidateCache(['channelSettings', 'instrumentConfig']);
        });
        
        this.eventBus.on('file:added', () => {
            this.invalidateCache(['playlistEditor']);
        });
    }

    /**
     * Initialise le systÃƒÂ¨me de modales
     */
    initializeModals() {
        this.createModalContainers();
        this.setupGlobalEvents();
        this.setupValidationRules();
        this.log('debug', 'ModalController', 'SystÃƒÂ¨me de modales initialisÃƒÂ©');
    }

    /**
     * Configure les rÃƒÂ¨gles de validation par dÃƒÂ©faut
     */
    setupValidationRules() {
        // Validation nom de playlist
        this.validationRules.set('playlistName', {
            required: true,
            minLength: 1,
            maxLength: 100,
            pattern: /^[a-zA-Z0-9\s\-_]+$/,
            message: 'Le nom doit contenir entre 1 et 100 caractÃƒÂ¨res alphanumÃƒÂ©riques'
        });
        
        // Validation nom de fichier
        this.validationRules.set('filename', {
            required: true,
            pattern: /^[a-zA-Z0-9\s\-_\.]+$/,
            message: 'Nom de fichier invalide'
        });
        
        // Validation tempo
        this.validationRules.set('tempo', {
            required: true,
            min: 20,
            max: 300,
            message: 'Le tempo doit ÃƒÂªtre entre 20 et 300 BPM'
        });
    }

    /**
     * CrÃƒÂ©e les conteneurs HTML
     */
    createModalContainers() {
        const existingContainer = document.getElementById('modal-container');
        if (existingContainer) {
            existingContainer.remove();
        }
        
        const container = document.createElement('div');
        container.id = 'modal-container';
        container.innerHTML = this.buildAllModalsHTML();
        document.body.appendChild(container);
    }

    /**
     * Configure les ÃƒÂ©vÃƒÂ©nements globaux
     */
    setupGlobalEvents() {
        document.addEventListener('keydown', (event) => {
            if (event.key === 'Escape' && this.animationConfig.closeOnEscape) {
                this.closeTopModal();
            }
        });
        
        document.addEventListener('keydown', (event) => {
            if (event.key === 'Tab' && this.modalState.activeModals.size > 0) {
                this.handleTabKey(event);
            }
        });
    }

    // ========================================================================
    // GESTION DES MODALES
    // ========================================================================

    /**
     * Ouvre une modale
     */
    async open(modalId, options = {}) {
        const modalConfig = this.modals[modalId];
        if (!modalConfig) {
            this.notify('error', `Modale inconnue: ${modalId}`);
            return false;
        }
        
        if (this.modalState.activeModals.has(modalId)) {
            this.log('debug', 'ModalController', `Modale dÃƒÂ©jÃƒÂ  ouverte: ${modalId}`);
            return false;
        }
        
        try {
            this.modalState.lastActiveElement = document.activeElement;
            
            const content = await this.generateModalContent(modalId, options);
            const modalElement = document.getElementById(modalConfig.id);
            
            if (modalElement) {
                this.updateModalContent(modalElement, content, modalConfig.title);
                this.modalState.activeModals.add(modalId);
                this.modalState.modalStack.push(modalId);
                
                await this.animateModalOpen(modalElement);
                this.setupModalFocus(modalElement);
                
                this.eventBus.emit('modal:opened', { modalId, options });
                this.log('debug', 'ModalController', `Modale ouverte: ${modalId}`);
                return true;
            }
        } catch (error) {
            this.handleError('Erreur ouverture modale', error);
            return false;
        }
        
        return false;
    }

    /**
     * Ferme une modale
     */
    async close(modalId) {
        if (!this.modalState.activeModals.has(modalId)) {
            return false;
        }
        
        const modalConfig = this.modals[modalId];
        const modalElement = document.getElementById(modalConfig.id);
        
        if (modalElement) {
            try {
                await this.animateModalClose(modalElement);
                
                this.modalState.activeModals.delete(modalId);
                this.modalState.modalStack = this.modalState.modalStack.filter(id => id !== modalId);
                this.modalState.validationErrors.delete(modalId);
                
                if (this.modalState.activeModals.size === 0) {
                    this.restoreFocus();
                } else {
                    const prevModalId = this.modalState.modalStack[this.modalState.modalStack.length - 1];
                    if (prevModalId) {
                        const prevModal = document.getElementById(this.modals[prevModalId].id);
                        this.setupModalFocus(prevModal);
                    }
                }
                
                this.eventBus.emit('modal:closed', { modalId });
                this.log('debug', 'ModalController', `Modale fermÃƒÂ©e: ${modalId}`);
                return true;
            } catch (error) {
                this.handleError('Erreur fermeture modale', error);
            }
        }
        
        return false;
    }

    /**
     * Ferme la modale du dessus
     */
    closeTopModal() {
        if (this.modalState.modalStack.length > 0) {
            const topModalId = this.modalState.modalStack[this.modalState.modalStack.length - 1];
            this.close(topModalId);
        }
    }

    /**
     * Ferme toutes les modales
     */
    closeAll() {
        const modalIds = [...this.modalState.activeModals];
        modalIds.forEach(modalId => this.close(modalId));
    }

    // ========================================================================
    // MODALES SPÃƒâ€°CIALISÃƒâ€°ES (HELPERS)
    // ========================================================================

    /**
     * Affiche une modale de confirmation
     */
    confirm(message, callback, options = {}) {
        const confirmOptions = {
            message,
            callback,
            confirmText: options.confirmText || 'Confirmer',
            cancelText: options.cancelText || 'Annuler',
            type: options.type || 'warning'
        };
        
        return this.open('confirmation', confirmOptions);
    }

    /**
     * Affiche une modale d'information
     */
    alert(title, message, options = {}) {
        const alertOptions = {
            title,
            message,
            type: options.type || 'info',
            okText: options.okText || 'OK'
        };
        
        return this.open('information', alertOptions);
    }

    /**
     * Affiche une modale d'erreur
     */
    error(error, exception = null, options = {}) {
        const errorOptions = {
            error,
            exception,
            showDetails: options.showDetails || false,
            okText: options.okText || 'Fermer'
        };
        
        this.notify('error', error);
        return this.open('error', errorOptions);
    }

    /**
     * Modale d'ÃƒÂ©dition de playlist avec validation
     */
    async openPlaylistEditor(playlistId = null) {
        return this.open('playlistEditor', { playlistId });
    }

    /**
     * Modale de crÃƒÂ©ation de playlist simplifiÃƒÂ©e
     */
    async promptCreatePlaylist() {
        const name = prompt('Nom de la playlist:');
        if (!name) return null;
        
        // Valider le nom
        const validation = this.validate('playlistName', name);
        if (!validation.valid) {
            this.notify('error', validation.message);
            return null;
        }
        
        try {
            const playlistModel = this.getModel('playlist');
            const playlist = playlistModel.createPlaylist(name);
            
            this.notify('success', `Playlist "${name}" crÃƒÂ©ÃƒÂ©e`);
            this.eventBus.emit('playlist:created', { playlist });
            
            return playlist;
        } catch (error) {
            this.handleError('Erreur crÃƒÂ©ation playlist', error);
            return null;
        }
    }

    /**
     * Modale de confirmation de suppression
     */
    async confirmDelete(itemType, itemName, onConfirm) {
        return this.confirm(
            `ÃƒÅ tes-vous sÃƒÂ»r de vouloir supprimer ${itemType} "${itemName}" ?`,
            onConfirm,
            {
                type: 'warning',
                confirmText: 'Supprimer',
                cancelText: 'Annuler'
            }
        );
    }

    // ========================================================================
    // VALIDATION
    // ========================================================================

    /**
     * Valide une valeur selon une rÃƒÂ¨gle
     */
    validate(ruleName, value) {
        const rule = this.validationRules.get(ruleName);
        if (!rule) {
            return { valid: true };
        }
        
        // Required
        if (rule.required && (!value || value.trim() === '')) {
            return { valid: false, message: 'Ce champ est requis' };
        }
        
        // MinLength
        if (rule.minLength && value.length < rule.minLength) {
            return { valid: false, message: `Minimum ${rule.minLength} caractÃƒÂ¨res` };
        }
        
        // MaxLength
        if (rule.maxLength && value.length > rule.maxLength) {
            return { valid: false, message: `Maximum ${rule.maxLength} caractÃƒÂ¨res` };
        }
        
        // Pattern
        if (rule.pattern && !rule.pattern.test(value)) {
            return { valid: false, message: rule.message || 'Format invalide' };
        }
        
        // Min/Max (nombres)
        const numValue = parseFloat(value);
        if (!isNaN(numValue)) {
            if (rule.min !== undefined && numValue < rule.min) {
                return { valid: false, message: `Minimum: ${rule.min}` };
            }
            if (rule.max !== undefined && numValue > rule.max) {
                return { valid: false, message: `Maximum: ${rule.max}` };
            }
        }
        
        return { valid: true };
    }

    /**
     * Valide un formulaire complet
     */
    validateForm(formElement) {
        const errors = new Map();
        const inputs = formElement.querySelectorAll('[data-validate]');
        
        inputs.forEach(input => {
            const ruleName = input.getAttribute('data-validate');
            const value = input.value;
            const validation = this.validate(ruleName, value);
            
            if (!validation.valid) {
                errors.set(input.name, validation.message);
                this.showFieldError(input, validation.message);
            } else {
                this.clearFieldError(input);
            }
        });
        
        return { valid: errors.size === 0, errors };
    }

    /**
     * Affiche une erreur de champ
     */
    showFieldError(input, message) {
        input.classList.add('error');
        
        let errorEl = input.nextElementSibling;
        if (!errorEl || !errorEl.classList.contains('field-error')) {
            errorEl = document.createElement('div');
            errorEl.className = 'field-error';
            input.parentNode.insertBefore(errorEl, input.nextSibling);
        }
        
        errorEl.textContent = message;
    }

    /**
     * Efface une erreur de champ
     */
    clearFieldError(input) {
        input.classList.remove('error');
        
        const errorEl = input.nextElementSibling;
        if (errorEl && errorEl.classList.contains('field-error')) {
            errorEl.remove();
        }
    }

    // ========================================================================
    // GÃƒâ€°NÃƒâ€°RATION DE CONTENU (stubs - ÃƒÂ  implÃƒÂ©menter selon besoins)
    // ========================================================================

    async generateModalContent(modalId, options) {
        const cacheKey = `${modalId}_${JSON.stringify(options)}`;
        if (this.contentCache.has(cacheKey)) {
            return this.contentCache.get(cacheKey);
        }
        
        let content = '';
        
        switch (modalId) {
            case 'confirmation':
                content = this.buildConfirmationContent(options);
                break;
            case 'information':
                content = this.buildInformationContent(options);
                break;
            case 'error':
                content = this.buildErrorContent(options);
                break;
            default:
                content = this.buildDefaultContent(modalId, options);
        }
        
        if (options.cache !== false) {
            this.contentCache.set(cacheKey, content);
        }
        
        return content;
    }

    buildConfirmationContent(options) {
        return `
            <div class="modal-confirmation">
                <p>${options.message}</p>
                <div class="modal-actions">
                    <button class="btn-cancel" onclick="app.modalController.close('confirmation')">
                        ${options.cancelText || 'Annuler'}
                    </button>
                    <button class="btn-confirm" onclick="app.modalController.handleConfirm()">
                        ${options.confirmText || 'Confirmer'}
                    </button>
                </div>
            </div>
        `;
    }

    buildInformationContent(options) {
        return `
            <div class="modal-information">
                <p>${options.message}</p>
                <div class="modal-actions">
                    <button class="btn-primary" onclick="app.modalController.close('information')">
                        ${options.okText || 'OK'}
                    </button>
                </div>
            </div>
        `;
    }

    buildErrorContent(options) {
        return `
            <div class="modal-error">
                <p class="error-message">${options.error}</p>
                ${options.exception && options.showDetails ? `
                    <details class="error-details">
                        <summary>DÃƒÂ©tails techniques</summary>
                        <pre>${options.exception.stack || options.exception.message}</pre>
                    </details>
                ` : ''}
                <div class="modal-actions">
                    <button class="btn-primary" onclick="app.modalController.close('error')">
                        ${options.okText || 'Fermer'}
                    </button>
                </div>
            </div>
        `;
    }

    buildDefaultContent(modalId, options) {
        return `<div>Contenu par dÃƒÂ©faut pour ${modalId}</div>`;
    }

    buildAllModalsHTML() {
        return `
            <!-- Les modales sont crÃƒÂ©ÃƒÂ©es dynamiquement selon les besoins -->
            <div id="channelModal" class="modal"></div>
            <div id="instrumentConfigModal" class="modal"></div>
            <div id="playlistEditorModal" class="modal"></div>
            <div id="fileUploadModal" class="modal"></div>
            <div id="systemConfigModal" class="modal"></div>
            <div id="confirmationModal" class="modal"></div>
            <div id="informationModal" class="modal"></div>
            <div id="errorModal" class="modal"></div>
            <div class="modal-backdrop"></div>
        `;
    }

    // ========================================================================
    // ANIMATIONS & HELPERS
    // ========================================================================

    updateModalContent(modalElement, content, title) {
        modalElement.innerHTML = `
            <div class="modal-content">
                <div class="modal-header">
                    <h3>${title}</h3>
                    <button class="modal-close" onclick="app.modalController.closeTopModal()">Ã¢Å“â€¢</button>
                </div>
                <div class="modal-body">${content}</div>
            </div>
        `;
    }

    async animateModalOpen(modalElement) {
        modalElement.style.display = 'flex';
        const backdrop = document.querySelector('.modal-backdrop');
        if (backdrop) backdrop.style.display = 'block';
        
        await new Promise(resolve => requestAnimationFrame(() => {
            modalElement.classList.add('modal-open');
            if (backdrop) backdrop.classList.add('modal-backdrop-open');
            setTimeout(resolve, this.animationConfig.duration);
        }));
    }

    async animateModalClose(modalElement) {
        modalElement.classList.remove('modal-open');
        
        const backdrop = document.querySelector('.modal-backdrop');
        if (this.modalState.activeModals.size === 1 && backdrop) {
            backdrop.classList.remove('modal-backdrop-open');
        }
        
        await new Promise(resolve => setTimeout(resolve, this.animationConfig.duration));
        modalElement.style.display = 'none';
        if (this.modalState.activeModals.size === 1 && backdrop) {
            backdrop.style.display = 'none';
        }
    }

    setupModalFocus(modalElement) {
        const firstFocusable = modalElement.querySelector('button, input, select, textarea, [tabindex]:not([tabindex="-1"])');
        if (firstFocusable) {
            firstFocusable.focus();
        }
    }

    restoreFocus() {
        if (this.modalState.lastActiveElement && typeof this.modalState.lastActiveElement.focus === 'function') {
            this.modalState.lastActiveElement.focus();
        }
    }

    handleTabKey(event) {
        // Focus trap - ÃƒÂ  implÃƒÂ©menter si nÃƒÂ©cessaire
    }

    invalidateCache(modalIds) {
        if (Array.isArray(modalIds)) {
            modalIds.forEach(id => {
                for (const key of this.contentCache.keys()) {
                    if (key.startsWith(id)) {
                        this.contentCache.delete(key);
                    }
                }
            });
        }
    }

    /**
     * Nettoie les ressources
     */
    destroy() {
        this.closeAll();
        this.contentCache?.clear();
        this.eventHandlers?.clear();
        this.validationRules?.clear();
        
        const container = document.getElementById('modal-container');
        if (container) {
            container.remove();
        }
        
        super.destroy();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = ModalController;
}

if (typeof window !== 'undefined') {
    window.ModalController = ModalController;
}
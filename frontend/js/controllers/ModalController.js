// ============================================================================
// Fichier: frontend/js/controllers/ModalController.js
// Projet: MidiMind v3.0 - SystÃƒÂ¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   ContrÃƒÂ´leur de gestion des fenÃƒÂªtres modales de l'application.
//   GÃƒÂ¨re l'ouverture, fermeture, validation et construction de contenu
//   des modales (ÃƒÂ©dition instruments, playlists, paramÃƒÂ¨tres, etc.).
//
// FonctionnalitÃƒÂ©s:
//   - CrÃƒÂ©ation modales dynamiques (Alert, Confirm, Prompt, Custom)
//   - Gestion pile de modales (stack)
//   - Validation donnÃƒÂ©es avant fermeture
//   - Callbacks personnalisÃƒÂ©s (onOpen, onClose, onValidate)
//   - Modales prÃƒÂ©dÃƒÂ©finies (instruments, playlists, settings)
//   - Formulaires avec validation
//   - Templates rÃƒÂ©utilisables
//   - Animation d'ouverture/fermeture
//
// Architecture:
//   ModalController extends BaseController
//   - Utilise Modal component (views/components/)
//   - ValidationController pour formulaires
//   - EventBus pour communication
//
// Auteur: MidiMind Team
// ============================================================================

class ModalController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
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

	    // Ãƒâ€°tat interne playlist editor
        this._draggedPlaylistItem = null;	
		
        // Ãƒâ€°tat des modales
        this.modalState = {
            activeModals: new Set(),
            modalStack: [], // Pour gÃƒÂ©rer l'empilement
            currentFocus: null,
            lastActiveElement: null
        };
        
        // Configuration d'animation
        this.animationConfig = {
            duration: 300,
            easing: 'cubic-bezier(0.4, 0, 0.2, 1)',
            enableBackdrop: true,
            closeOnBackdropClick: true,
            closeOnEscape: true
        };
        
        // Cache pour les contenus de modales
        this.contentCache = new Map();
        
        // Callbacks et handlers
        this.eventHandlers = new Map();
        this.validationRules = new Map();
        
        this.initializeModals();
    }

    /**
     * Configuration des ÃƒÂ©vÃƒÂ©nements
     */
    bindEvents() {
        // Ãƒâ€°couter les ÃƒÂ©vÃƒÂ©nements de modales
        this.eventBus.on('modal:open', (data) => {
            this.open(data.modalId, data.options);
        });
        
        this.eventBus.on('modal:close', (data) => {
            this.close(data.modalId);
        });
        
        this.eventBus.on('modal:confirm', (data) => {
            this.confirm(data.message, data.callback, data.options);
        });
        
        // Ãƒâ€°couter les changements d'instruments pour les modales
        this.eventBus.on('instrument:updated', () => {
            this.invalidateCache(['channelSettings', 'instrumentConfig']);
        });
        
        // Ãƒâ€°couter les changements de fichiers pour les modales
        this.eventBus.on('file:added', () => {
            this.invalidateCache(['playlistEditor']);
        });
    }

    /**
     * Initialise le systÃƒÂ¨me de modales
     */
    initializeModals() {
        // CrÃƒÂ©er les conteneurs de modales
        this.createModalContainers();
        
        // Configurer les ÃƒÂ©vÃƒÂ©nements globaux
        this.setupGlobalEvents();
        
        this.logDebug('modals', 'SystÃƒÂ¨me de modales initialisÃƒÂ©');
    }

    /**
     * CrÃƒÂ©e les conteneurs HTML pour toutes les modales
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
        
        this.logDebug('modals', 'Conteneurs de modales crÃƒÂ©ÃƒÂ©s');
    }

    /**
     * Configure les ÃƒÂ©vÃƒÂ©nements globaux pour les modales
     */
    setupGlobalEvents() {
        // Gestion de la touche Ãƒâ€°chap
        document.addEventListener('keydown', (event) => {
            if (event.key === 'Escape' && this.animationConfig.closeOnEscape) {
                this.closeTopModal();
            }
        });
        
        // Gestion du focus trap
        document.addEventListener('keydown', (event) => {
            if (event.key === 'Tab' && this.modalState.activeModals.size > 0) {
                this.handleTabKey(event);
            }
        });
    }

    // ===== GESTION DES MODALES =====

    /**
     * Ouvre une modale
     * @param {string} modalId - ID de la modale
     * @param {Object} options - Options d'ouverture
     */
    async open(modalId, options = {}) {
        const modalConfig = this.modals[modalId];
        if (!modalConfig) {
            this.logDebug('modals', `Modale inconnue: ${modalId}`);
            return false;
        }
        
        // VÃƒÂ©rifier si la modale est dÃƒÂ©jÃƒÂ  ouverte
        if (this.modalState.activeModals.has(modalId)) {
            this.logDebug('modals', `Modale dÃƒÂ©jÃƒÂ  ouverte: ${modalId}`);
            return false;
        }
        
        try {
            this.logDebug('modals', `Ouverture modale: ${modalId}`);
            
            // Sauvegarder l'ÃƒÂ©lÃƒÂ©ment actuellement focalisÃƒÂ©
            this.modalState.lastActiveElement = document.activeElement;
            
            // GÃƒÂ©nÃƒÂ©rer le contenu de la modale
            const content = await this.generateModalContent(modalId, options);
            
            // Mettre ÃƒÂ  jour la modale
            const modalElement = document.getElementById(modalConfig.id);
            if (modalElement) {
                this.updateModalContent(modalElement, content, modalConfig.title);
                
                // Ajouter ÃƒÂ  la pile des modales actives
                this.modalState.activeModals.add(modalId);
                this.modalState.modalStack.push(modalId);
                
                // Animer l'ouverture
                await this.animateModalOpen(modalElement);
                
                // Configurer le focus
                this.setupModalFocus(modalElement);
                
                // Ãƒâ€°mettre l'ÃƒÂ©vÃƒÂ©nement d'ouverture
                this.eventBus.emit('modal:opened', { modalId, options });
                
                return true;
            }
            
        } catch (error) {
            this.logDebug('modals', `Erreur ouverture modale ${modalId}: ${error.message}`);
            return false;
        }
        
        return false;
    }

    /**
     * Ferme une modale
     * @param {string} modalId - ID de la modale ÃƒÂ  fermer
     */
    async close(modalId) {
        if (!this.modalState.activeModals.has(modalId)) {
            return false;
        }
        
        const modalConfig = this.modals[modalId];
        const modalElement = document.getElementById(modalConfig.id);
        
        if (modalElement) {
            try {
                // Animer la fermeture
                await this.animateModalClose(modalElement);
                
                // Retirer de la pile des modales actives
                this.modalState.activeModals.delete(modalId);
                this.modalState.modalStack = this.modalState.modalStack.filter(id => id !== modalId);
                
                // Restaurer le focus si c'ÃƒÂ©tait la derniÃƒÂ¨re modale
                if (this.modalState.activeModals.size === 0) {
                    this.restoreFocus();
                } else {
                    // Redonner le focus ÃƒÂ  la modale prÃƒÂ©cÃƒÂ©dente
                    const prevModalId = this.modalState.modalStack[this.modalState.modalStack.length - 1];
                    if (prevModalId) {
                        const prevModal = document.getElementById(this.modals[prevModalId].id);
                        this.setupModalFocus(prevModal);
                    }
                }
                
                this.logDebug('modals', `Modale fermÃƒÂ©e: ${modalId}`);
                
                // Ãƒâ€°mettre l'ÃƒÂ©vÃƒÂ©nement de fermeture
                this.eventBus.emit('modal:closed', { modalId });
                
                return true;
                
            } catch (error) {
                this.logDebug('modals', `Erreur fermeture modale ${modalId}: ${error.message}`);
            }
        }
        
        return false;
    }

    /**
     * Ferme la modale du dessus de la pile
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

    // ===== MODALES SPÃƒâ€°CIALISÃƒâ€°ES =====

    /**
     * Affiche une modale de confirmation
     * @param {string} message - Message de confirmation
     * @param {Function} callback - Callback de confirmation
     * @param {Object} options - Options de la modale
     */
    confirm(message, callback, options = {}) {
        const confirmOptions = {
            message,
            callback,
            confirmText: options.confirmText || 'Confirmer',
            cancelText: options.cancelText || 'Annuler',
            type: options.type || 'warning' // success, warning, error, info
        };
        
        return this.open('confirmation', confirmOptions);
    }

    /**
     * Affiche une modale d'information
     * @param {string} title - Titre de la modale
     * @param {string} message - Message d'information
     * @param {Object} options - Options de la modale
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
     * @param {string} error - Message d'erreur
     * @param {Error} exception - Exception optionnelle
     * @param {Object} options - Options de la modale
     */
    error(error, exception = null, options = {}) {
        const errorOptions = {
            error,
            exception,
            showDetails: options.showDetails || false,
            okText: options.okText || 'Fermer'
        };
        
        return this.open('error', errorOptions);
    }

    /**
     * Ouvre la modale de configuration des canaux MIDI
     * @param {string} fileId - ID du fichier MIDI
     */
    openChannelSettings(fileId) {
        return this.open('channelSettings', { fileId });
    }

    /**
     * Ouvre la modale de configuration d'un instrument
     * @param {string} instrumentId - ID de l'instrument
     */
    openInstrumentConfig(instrumentId) {
        return this.open('instrumentConfig', { instrumentId });
    }

    /**
     * Ouvre l'ÃƒÂ©diteur de playlist
     * @param {string} playlistId - ID de la playlist (optionnel pour crÃƒÂ©ation)
     */
    openPlaylistEditor(playlistId = null) {
        return this.open('playlistEditor', { playlistId });
    }

    // ===== GÃƒâ€°NÃƒâ€°RATION DE CONTENU =====

    /**
     * GÃƒÂ©nÃƒÂ¨re le contenu d'une modale
     * @param {string} modalId - ID de la modale
     * @param {Object} options - Options pour le contenu
     * @returns {Promise<string>} - Contenu HTML
     */
    async generateModalContent(modalId, options) {
        // VÃƒÂ©rifier le cache
        const cacheKey = `${modalId}_${JSON.stringify(options)}`;
        if (this.contentCache.has(cacheKey)) {
            return this.contentCache.get(cacheKey);
        }
        
        let content = '';
        
        switch (modalId) {
            case 'channelSettings':
                content = this.buildChannelSettingsContent(options);
                break;
                
            case 'instrumentConfig':
                content = this.buildInstrumentConfigContent(options);
                break;
                
            case 'playlistEditor':
                content = this.buildPlaylistEditorContent(options);
                break;
                
            case 'fileUpload':
                content = this.buildFileUploadContent(options);
                break;
                
            case 'systemConfig':
                content = this.buildSystemConfigContent(options);
                break;
                
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
        
        // Mettre en cache
        this.contentCache.set(cacheKey, content);
        
        return content;
    }

    /**
     * Construit le contenu de la modale de configuration des canaux
     * @param {Object} options - Options de configuration
     * @returns {string} - HTML du contenu
     */
    buildChannelSettingsContent(options) {
        const fileModel = this.getModel('file');
        const instrumentModel = this.getModel('instrument');
        
        const file = fileModel.getFileById(options.fileId);
        const instruments = instrumentModel.getConnectedInstruments();
        
        if (!file) {
            return '<div class="error">Fichier introuvable</div>';
        }
        
        return `
            <div class="channel-settings">
                <div class="file-info">
                    <h4>Ã°Å¸â€œÂ ${file.name}</h4>
                    <p>${file.tracks?.length || 0} piste(s) Ã¢â‚¬Â¢ ${file.duration?.toFixed(1) || 0}s</p>
                </div>
                
                <div class="channel-assignment">
                    <h5>Attribution des canaux MIDI</h5>
                    ${this.buildChannelAssignmentTable(file, instruments)}
                </div>
                
                <div class="routing-options">
                    <h5>Options de routage</h5>
                    ${this.buildRoutingOptions()}
                </div>
                
                <div class="modal-actions">
                    <button class="btn btn-secondary" onclick="app.modalController.close('channelSettings')">
                        Annuler
                    </button>
                    <button class="btn btn-primary" onclick="app.modalController.saveChannelSettings('${options.fileId}')">
                        Appliquer
                    </button>
                </div>
            </div>
        `;
    }

    /**
     * Construit le contenu de la modale de configuration d'instrument
     * @param {Object} options - Options de configuration
     * @returns {string} - HTML du contenu
     */
    buildInstrumentConfigContent(options) {
        const instrumentModel = this.getModel('instrument');
        const instrument = instrumentModel.getInstrumentById(options.instrumentId);
        
        if (!instrument) {
            return '<div class="error">Instrument introuvable</div>';
        }
        
        return `
            <div class="instrument-config">
                <div class="instrument-header">
                    <h4>Ã°Å¸Å½Â¼ ${instrument.name}</h4>
                    <span class="instrument-type">${instrument.type}</span>
                </div>
                
                <div class="config-sections">
                    <div class="config-section">
                        <h5>Configuration MIDI</h5>
                        ${this.buildMidiConfigSection(instrument)}
                    </div>
                    
                    <div class="config-section">
                        <h5>Performance</h5>
                        ${this.buildPerformanceSection(instrument)}
                    </div>
                    
                    <div class="config-section">
                        <h5>AvancÃƒÂ©</h5>
                        ${this.buildAdvancedSection(instrument)}
                    </div>
                </div>
                
                <div class="modal-actions">
                    <button class="btn btn-secondary" onclick="app.modalController.close('instrumentConfig')">
                        Annuler
                    </button>
                    <button class="btn btn-warning" onclick="app.modalController.calibrateInstrument('${options.instrumentId}')">
                        Ã°Å¸â€œÅ  Calibrer
                    </button>
                    <button class="btn btn-primary" onclick="app.modalController.saveInstrumentConfig('${options.instrumentId}')">
                        Sauvegarder
                    </button>
                </div>
            </div>
        `;
    }

    /**
     * Construit le contenu de l'ÃƒÂ©diteur de playlist
     * @param {Object} options - Options d'ÃƒÂ©dition
     * @returns {string} - HTML du contenu
     */
    buildPlaylistEditorContent(options) {
        const playlistModel = this.getModel('playlist');
        const fileModel = this.getModel('file');
        
        const playlist = options.playlistId ? playlistModel.getPlaylistById(options.playlistId) : null;
        const availableFiles = fileModel.get('files')?.filter(f => f.type === 'file') || [];
        
        return `
            <div class="playlist-editor">
                <div class="playlist-header">
                    <div class="form-group">
                        <label for="playlistName">Nom de la playlist:</label>
                        <input type="text" id="playlistName" class="form-control" 
                               value="${playlist?.name || ''}" placeholder="Ma nouvelle playlist">
                    </div>
                    
                    <div class="form-group">
                        <label for="playlistDescription">Description:</label>
                        <textarea id="playlistDescription" class="form-control" rows="2" 
                                  placeholder="Description optionnelle">${playlist?.description || ''}</textarea>
                    </div>
                </div>
                
                <div class="playlist-content">
                    <div class="available-files">
                        <h5>Ã°Å¸â€œÂ Fichiers disponibles</h5>
                        <div class="file-list" id="availableFilesList">
                            ${this.buildAvailableFilesList(availableFiles, playlist?.files || [])}
                        </div>
                    </div>
                    
                    <div class="playlist-files">
                        <h5>Ã°Å¸â€œâ€¹ Playlist</h5>
                        <div class="file-list sortable" id="playlistFilesList">
                            ${this.buildPlaylistFilesList(playlist?.files || [], availableFiles)}
                        </div>
                    </div>
                </div>
                
                <div class="modal-actions">
                    <button class="btn btn-secondary" onclick="app.modalController.close('playlistEditor')">
                        Annuler
                    </button>
                    <button class="btn btn-primary" onclick="app.modalController.savePlaylist('${options.playlistId || ''}')">
                        ${playlist ? 'Modifier' : 'CrÃƒÂ©er'}
                    </button>
                </div>
            </div>
        `;
    }

    /**
     * Construit le contenu de confirmation
     * @param {Object} options - Options de confirmation
     * @returns {string} - HTML du contenu
     */
    buildConfirmationContent(options) {
        const typeIcons = {
            success: 'Ã¢Å“â€¦',
            warning: 'Ã¢Å¡Â Ã¯Â¸Â',
            error: 'Ã¢ÂÅ’',
            info: 'Ã¢â€žÂ¹Ã¯Â¸Â'
        };
        
        const icon = typeIcons[options.type] || 'Ã¢Ââ€œ';
        
        return `
            <div class="confirmation-content">
                <div class="confirmation-icon">${icon}</div>
                <div class="confirmation-message">${options.message}</div>
                
                <div class="modal-actions">
                    <button class="btn btn-secondary" onclick="app.modalController.close('confirmation')">
                        ${options.cancelText}
                    </button>
                    <button class="btn btn-primary" onclick="app.modalController.handleConfirmation(true, '${options.callback}')">
                        ${options.confirmText}
                    </button>
                </div>
            </div>
        `;
    }

    /**
     * Construit le contenu d'information
     * @param {Object} options - Options d'information
     * @returns {string} - HTML du contenu
     */
    buildInformationContent(options) {
        const typeIcons = {
            success: 'Ã¢Å“â€¦',
            warning: 'Ã¢Å¡Â Ã¯Â¸Â',
            error: 'Ã¢ÂÅ’',
            info: 'Ã¢â€žÂ¹Ã¯Â¸Â'
        };
        
        const icon = typeIcons[options.type] || 'Ã¢â€žÂ¹Ã¯Â¸Â';
        
        return `
            <div class="information-content">
                <div class="information-icon">${icon}</div>
                <div class="information-message">${options.message}</div>
                
                <div class="modal-actions">
                    <button class="btn btn-primary" onclick="app.modalController.close('information')">
                        ${options.okText}
                    </button>
                </div>
            </div>
        `;
    }

    /**
     * Construit le contenu d'erreur
     * @param {Object} options - Options d'erreur
     * @returns {string} - HTML du contenu
     */
    buildErrorContent(options) {
        return `
            <div class="error-content">
                <div class="error-icon">Ã¢ÂÅ’</div>
                <div class="error-message">${options.error}</div>
                
                ${options.exception && options.showDetails ? `
                    <details class="error-details">
                        <summary>DÃƒÂ©tails techniques</summary>
                        <pre class="error-stack">${options.exception.stack || options.exception.message}</pre>
                    </details>
                ` : ''}
                
                <div class="modal-actions">
                    <button class="btn btn-primary" onclick="app.modalController.close('error')">
                        ${options.okText}
                    </button>
                </div>
            </div>
        `;
    }

    // ===== CONSTRUCTION HTML =====

    /**
     * Construit le HTML de toutes les modales
     * @returns {string} - HTML complet des modales
     */
    buildAllModalsHTML() {
        return Object.entries(this.modals).map(([modalId, config]) => `
            <div class="modal" id="${config.id}" style="display: none;">
                <div class="modal-backdrop" onclick="${this.animationConfig.closeOnBackdropClick ? `app.modalController.close('${modalId}')` : ''}"></div>
                <div class="modal-dialog modal-${config.size}">
                    <div class="modal-content">
                        <div class="modal-header">
                            <h3 class="modal-title">${config.title}</h3>
                            ${config.closable ? `
                                <button class="modal-close" onclick="app.modalController.close('${modalId}')" aria-label="Fermer">
                                    Ãƒâ€”
                                </button>
                            ` : ''}
                        </div>
                        <div class="modal-body">
                            <!-- Contenu dynamique -->
                        </div>
                    </div>
                </div>
            </div>
        `).join('');
    }

    // ===== UTILITAIRES =====

    /**
     * Met ÃƒÂ  jour le contenu d'une modale
     * @param {HTMLElement} modalElement - Ãƒâ€°lÃƒÂ©ment de la modale
     * @param {string} content - Nouveau contenu
     * @param {string} title - Nouveau titre
     */
    updateModalContent(modalElement, content, title) {
        const titleElement = modalElement.querySelector('.modal-title');
        const bodyElement = modalElement.querySelector('.modal-body');
        
        if (titleElement && title) {
            titleElement.textContent = title;
        }
        
        if (bodyElement) {
            bodyElement.innerHTML = content;
        }
    }

    /**
     * Anime l'ouverture d'une modale
     * @param {HTMLElement} modalElement - Ãƒâ€°lÃƒÂ©ment de la modale
     * @returns {Promise} - Promise d'animation
     */
    animateModalOpen(modalElement) {
        return new Promise((resolve) => {
            modalElement.style.display = 'flex';
            modalElement.style.opacity = '0';
            
            const dialog = modalElement.querySelector('.modal-dialog');
            dialog.style.transform = 'scale(0.8) translateY(-20px)';
            
            requestAnimationFrame(() => {
                modalElement.style.transition = `opacity ${this.animationConfig.duration}ms ${this.animationConfig.easing}`;
                dialog.style.transition = `transform ${this.animationConfig.duration}ms ${this.animationConfig.easing}`;
                
                modalElement.style.opacity = '1';
                dialog.style.transform = 'scale(1) translateY(0)';
                
                setTimeout(resolve, this.animationConfig.duration);
            });
        });
    }

    /**
     * Anime la fermeture d'une modale
     * @param {HTMLElement} modalElement - Ãƒâ€°lÃƒÂ©ment de la modale
     * @returns {Promise} - Promise d'animation
     */
    animateModalClose(modalElement) {
        return new Promise((resolve) => {
            const dialog = modalElement.querySelector('.modal-dialog');
            
            modalElement.style.opacity = '0';
            dialog.style.transform = 'scale(0.8) translateY(-20px)';
            
            setTimeout(() => {
                modalElement.style.display = 'none';
                modalElement.style.transition = '';
                dialog.style.transition = '';
                resolve();
            }, this.animationConfig.duration);
        });
    }

    /**
     * Configure le focus pour une modale
     * @param {HTMLElement} modalElement - Ãƒâ€°lÃƒÂ©ment de la modale
     */
    setupModalFocus(modalElement) {
        // Trouver le premier ÃƒÂ©lÃƒÂ©ment focalisable
        const focusable = modalElement.querySelector('input, button, select, textarea, [tabindex]:not([tabindex="-1"])');
        if (focusable) {
            focusable.focus();
        }
    }

    /**
     * Restaure le focus aprÃƒÂ¨s fermeture des modales
     */
    restoreFocus() {
        if (this.modalState.lastActiveElement) {
            this.modalState.lastActiveElement.focus();
            this.modalState.lastActiveElement = null;
        }
    }

    /**
     * GÃƒÂ¨re la navigation au clavier (Tab)
     * @param {KeyboardEvent} event - Ãƒâ€°vÃƒÂ©nement clavier
     */
    handleTabKey(event) {
        const topModalId = this.modalState.modalStack[this.modalState.modalStack.length - 1];
        if (!topModalId) return;
        
        const modal = document.getElementById(this.modals[topModalId].id);
        const focusableElements = modal.querySelectorAll(
            'input, button, select, textarea, [tabindex]:not([tabindex="-1"])'
        );
        
        if (focusableElements.length === 0) return;
        
        const firstElement = focusableElements[0];
        const lastElement = focusableElements[focusableElements.length - 1];
        
        if (event.shiftKey) {
            if (document.activeElement === firstElement) {
                event.preventDefault();
                lastElement.focus();
            }
        } else {
            if (document.activeElement === lastElement) {
                event.preventDefault();
                firstElement.focus();
            }
        }
    }

    /**
     * Invalide le cache pour certaines modales
     * @param {Array<string>} modalIds - IDs des modales ÃƒÂ  invalider
     */
    invalidateCache(modalIds = []) {
        modalIds.forEach(modalId => {
            for (const key of this.contentCache.keys()) {
                if (key.startsWith(modalId)) {
                    this.contentCache.delete(key);
                }
            }
        });
    }

    /**
     * MÃƒÂ©thodes de construction pour sections spÃƒÂ©cifiques
     */ /**
     * Ã¢Å“â€¦ IMPLÃƒâ€°MENTÃƒâ€° - Construit la liste des fichiers disponibles
     * @param {Array} availableFiles - Tous les fichiers
     * @param {Array} playlistFiles - Fichiers dÃƒÂ©jÃƒÂ  dans la playlist
     * @returns {string} HTML
     */
    buildAvailableFilesList(availableFiles, playlistFiles) {
        // Extraire IDs des fichiers dÃƒÂ©jÃƒÂ  dans playlist
        const playlistFileIds = (playlistFiles || []).map(f => f.id || f);
        
        // Filtrer les fichiers disponibles
        const filtered = availableFiles.filter(file => 
            !playlistFileIds.includes(file.id)
        );
        
        if (filtered.length === 0) {
            return `
                <div class="empty-file-list">
                    <p>Ã°Å¸â€œÂ­ Tous les fichiers sont dÃƒÂ©jÃƒÂ  dans la playlist</p>
                </div>
            `;
        }
        
        // Ajouter barre de recherche
        let html = `
            <div class="file-search-box">
                <input type="text" 
                       class="file-search-input" 
                       placeholder="Ã°Å¸â€Â Rechercher un fichier..."
                       onkeyup="app.modalController.filterAvailableFiles(this.value)">
            </div>
            <div class="available-files-list" data-available-files>
        `;
        
        // GÃƒÂ©nÃƒÂ©rer items
        filtered.forEach(file => {
            const duration = this.formatDuration(file.duration || 0);
            const metadata = file.metadata || {};
            
            html += `
                <div class="modal-file-item available" 
                     data-file-id="${file.id}"
                     data-file-name="${this.escapeHtml(file.name || file.filename)}">
                    
                    <div class="file-item-icon">Ã°Å¸Å½Âµ</div>
                    
                    <div class="file-item-info">
                        <div class="file-item-name" title="${this.escapeHtml(file.name || file.filename)}">
                            ${this.escapeHtml(file.name || file.filename)}
                        </div>
                        <div class="file-item-meta">
                            ${duration ? `<span>Ã¢ÂÂ±Ã¯Â¸Â ${duration}</span>` : ''}
                            ${metadata.trackCount ? `<span>Ã°Å¸Å½Â¹ ${metadata.trackCount} pistes</span>` : ''}
                            ${metadata.bpm ? `<span>Ã°Å¸Â¥Â ${metadata.bpm} BPM</span>` : ''}
                        </div>
                    </div>
                    
                    <button class="btn-add-file" 
                            onclick="app.modalController.addFileToEditingPlaylist('${file.id}')"
                            title="Ajouter ÃƒÂ  la playlist">
                        Ã¢Å¾â€¢
                    </button>
                    
                </div>
            `;
        });
        
        html += '</div>';
        
        return html;
    }
    
    /**
     * Ã¢Å“â€¦ IMPLÃƒâ€°MENTÃƒâ€° - Construit la liste des fichiers dans la playlist
     * @param {Array} playlistFiles - Fichiers de la playlist (IDs ou objets)
     * @param {Array} allFiles - Tous les fichiers disponibles
     * @returns {string} HTML
     */
    buildPlaylistFilesList(playlistFiles, allFiles) {
        if (!playlistFiles || playlistFiles.length === 0) {
            return `
                <div class="empty-playlist-editor">
                    <div class="empty-icon">Ã°Å¸â€œÂ­</div>
                    <p>La playlist est vide</p>
                    <small>Ajoutez des fichiers depuis la liste de gauche</small>
                </div>
            `;
        }
        
        // RÃƒÂ©soudre les fichiers (si IDs uniquement, rÃƒÂ©cupÃƒÂ©rer objets complets)
        const resolvedFiles = playlistFiles.map(fileOrId => {
            if (typeof fileOrId === 'string') {
                // C'est un ID, chercher le fichier complet
                return allFiles.find(f => f.id === fileOrId) || { id: fileOrId, name: 'Unknown' };
            }
            return fileOrId; // C'est dÃƒÂ©jÃƒÂ  un objet
        });
        
        let html = '<div class="playlist-files-list sortable" data-playlist-files>';
        
        resolvedFiles.forEach((file, index) => {
            const duration = this.formatDuration(file.duration || 0);
            
            html += `
                <div class="modal-file-item playlist-file" 
                     data-file-id="${file.id}"
                     data-index="${index}"
                     draggable="true"
                     ondragstart="app.modalController.onPlaylistFileDragStart(event, ${index})"
                     ondragover="app.modalController.onPlaylistFileDragOver(event)"
                     ondrop="app.modalController.onPlaylistFileDrop(event, ${index})"
                     ondragend="app.modalController.onPlaylistFileDragEnd(event)">
                    
                    <div class="file-drag-handle" title="Glisser pour rÃƒÂ©organiser">
                        Ã¢â€¹Â®Ã¢â€¹Â®
                    </div>
                    
                    <div class="file-item-number">${index + 1}</div>
                    
                    <div class="file-item-info">
                        <div class="file-item-name" title="${this.escapeHtml(file.name || file.filename)}">
                            ${this.escapeHtml(file.name || file.filename)}
                        </div>
                        ${duration ? `<div class="file-item-duration">${duration}</div>` : ''}
                    </div>
                    
                    <button class="btn-remove-file" 
                            onclick="app.modalController.removeFileFromEditingPlaylist(${index})"
                            title="Retirer de la playlist">
                        Ã¢Å“â€“Ã¯Â¸Â
                    </button>
                    
                </div>
            `;
        });
        
        html += '</div>';
        
        // Ajouter statistiques
        const totalDuration = resolvedFiles.reduce((sum, f) => sum + (f.duration || 0), 0);
        html += `
            <div class="playlist-editor-stats">
                <span>${resolvedFiles.length} fichier${resolvedFiles.length > 1 ? 's' : ''}</span>
                <span>Ã¢â‚¬Â¢</span>
                <span>DurÃƒÂ©e totale: ${this.formatDuration(totalDuration)}</span>
            </div>
        `;
        
        return html;
    }
    
    /**
     * Ã¢Å“â€¦ IMPLÃƒâ€°MENTÃƒâ€° COMPLET - Sauvegarde la playlist ÃƒÂ©ditÃƒÂ©e
     * @param {string} playlistId - ID playlist (vide si crÃƒÂ©ation)
     */
    
    // ========================================================================
    // NOUVELLES MÃƒâ€°THODES - Gestion des fichiers dans l'ÃƒÂ©diteur
    // ========================================================================
    
    /**
     * Ajoute un fichier ÃƒÂ  la playlist en cours d'ÃƒÂ©dition
     */
    addFileToEditingPlaylist(fileId) {
        this.logDebug('modals', `Adding file ${fileId} to editing playlist`);
        
        // RÃƒÂ©cupÃƒÂ©rer le fichier depuis fileModel
        const fileModel = this.getModel('file');
        const file = fileModel?.getFileById?.(fileId) || 
                     fileModel?.get('files')?.find(f => f.id === fileId);
        
        if (!file) {
            this.showError('Fichier introuvable');
            return;
        }
        
        // Ajouter ÃƒÂ  la liste de droite
        const playlistContainer = document.querySelector('[data-playlist-files]');
        const availableContainer = document.querySelector('[data-available-files]');
        
        if (!playlistContainer) return;
        
        // Retirer item de la liste disponible
        const availableItem = availableContainer?.querySelector(
            `[data-file-id="${fileId}"]`
        );
        if (availableItem) {
            availableItem.remove();
        }
        
        // Ajouter ÃƒÂ  la playlist
        const index = playlistContainer.querySelectorAll('.modal-file-item').length;
        const duration = this.formatDuration(file.duration || 0);
        
        const newItem = document.createElement('div');
        newItem.className = 'modal-file-item playlist-file';
        newItem.setAttribute('data-file-id', fileId);
        newItem.setAttribute('data-index', index);
        newItem.setAttribute('draggable', 'true');
        
        newItem.innerHTML = `
            <div class="file-drag-handle" title="Glisser pour rÃƒÂ©organiser">Ã¢â€¹Â®Ã¢â€¹Â®</div>
            <div class="file-item-number">${index + 1}</div>
            <div class="file-item-info">
                <div class="file-item-name">${this.escapeHtml(file.name || file.filename)}</div>
                ${duration ? `<div class="file-item-duration">${duration}</div>` : ''}
            </div>
            <button class="btn-remove-file" 
                    onclick="app.modalController.removeFileFromEditingPlaylist(${index})"
                    title="Retirer de la playlist">
                Ã¢Å“â€“Ã¯Â¸Â
            </button>
        `;
        
        // Ajouter ÃƒÂ©vÃƒÂ©nements drag
        newItem.ondragstart = (e) => this.onPlaylistFileDragStart(e, index);
        newItem.ondragover = (e) => this.onPlaylistFileDragOver(e);
        newItem.ondrop = (e) => this.onPlaylistFileDrop(e, index);
        newItem.ondragend = (e) => this.onPlaylistFileDragEnd(e);
        
        playlistContainer.appendChild(newItem);
        
        // Mettre ÃƒÂ  jour les numÃƒÂ©ros
        this.updatePlaylistFileNumbers();
        
        // VÃƒÂ©rifier si liste disponible vide
        if (availableContainer && availableContainer.querySelectorAll('.modal-file-item').length === 0) {
            availableContainer.innerHTML = `
                <div class="empty-file-list">
                    <p>Ã°Å¸â€œÂ­ Tous les fichiers sont dans la playlist</p>
                </div>
            `;
        }
    }
    
    /**
     * Retire un fichier de la playlist en cours d'ÃƒÂ©dition
     */
    removeFileFromEditingPlaylist(index) {
        this.logDebug('modals', `Removing file at index ${index}`);
        
        const playlistContainer = document.querySelector('[data-playlist-files]');
        if (!playlistContainer) return;
        
        const items = playlistContainer.querySelectorAll('.modal-file-item');
        const itemToRemove = items[index];
        
        if (!itemToRemove) return;
        
        const fileId = itemToRemove.getAttribute('data-file-id');
        
        // Retirer l'item
        itemToRemove.remove();
        
        // Mettre ÃƒÂ  jour les numÃƒÂ©ros
        this.updatePlaylistFileNumbers();
        
        // Rajouter ÃƒÂ  la liste disponible si elle existe
        const availableContainer = document.querySelector('[data-available-files]');
        if (availableContainer && fileId) {
            // RÃƒÂ©cupÃƒÂ©rer le fichier complet
            const fileModel = this.getModel('file');
            const file = fileModel?.getFileById?.(fileId) || 
                         fileModel?.get('files')?.find(f => f.id === fileId);
            
            if (file) {
                // Supprimer l'empty state si prÃƒÂ©sent
                const emptyState = availableContainer.querySelector('.empty-file-list');
                if (emptyState) emptyState.remove();
                
                // Ajouter le fichier
                const duration = this.formatDuration(file.duration || 0);
                const metadata = file.metadata || {};
                
                const newItem = document.createElement('div');
                newItem.className = 'modal-file-item available';
                newItem.setAttribute('data-file-id', fileId);
                newItem.setAttribute('data-file-name', file.name || file.filename);
                
                newItem.innerHTML = `
                    <div class="file-item-icon">Ã°Å¸Å½Âµ</div>
                    <div class="file-item-info">
                        <div class="file-item-name">${this.escapeHtml(file.name || file.filename)}</div>
                        <div class="file-item-meta">
                            ${duration ? `<span>Ã¢ÂÂ±Ã¯Â¸Â ${duration}</span>` : ''}
                            ${metadata.trackCount ? `<span>Ã°Å¸Å½Â¹ ${metadata.trackCount} pistes</span>` : ''}
                        </div>
                    </div>
                    <button class="btn-add-file" 
                            onclick="app.modalController.addFileToEditingPlaylist('${fileId}')"
                            title="Ajouter ÃƒÂ  la playlist">
                        Ã¢Å¾â€¢
                    </button>
                `;
                
                availableContainer.appendChild(newItem);
            }
        }
    }
    
    /**
     * Met ÃƒÂ  jour les numÃƒÂ©ros des fichiers dans la playlist
     */
    updatePlaylistFileNumbers() {
        const playlistContainer = document.querySelector('[data-playlist-files]');
        if (!playlistContainer) return;
        
        const items = playlistContainer.querySelectorAll('.modal-file-item');
        items.forEach((item, index) => {
            item.setAttribute('data-index', index);
            const numberEl = item.querySelector('.file-item-number');
            if (numberEl) {
                numberEl.textContent = index + 1;
            }
            
            // Mettre ÃƒÂ  jour le onclick du bouton remove
            const removeBtn = item.querySelector('.btn-remove-file');
            if (removeBtn) {
                removeBtn.setAttribute('onclick', 
                    `app.modalController.removeFileFromEditingPlaylist(${index})`
                );
            }
        });
    }
    
    // ========================================================================
    // NOUVELLES MÃƒâ€°THODES - Drag & Drop
    // ========================================================================
    
    onPlaylistFileDragStart(event, index) {
        this._draggedPlaylistItem = index;
        event.dataTransfer.effectAllowed = 'move';
        event.dataTransfer.setData('text/plain', index);
        event.target.classList.add('dragging');
    }
    
    onPlaylistFileDragOver(event) {
        event.preventDefault();
        event.dataTransfer.dropEffect = 'move';
    }
    
    onPlaylistFileDrop(event, targetIndex) {
        event.preventDefault();
        
        const sourceIndex = this._draggedPlaylistItem;
        
        if (sourceIndex === null || sourceIndex === targetIndex) {
            return;
        }
        
        // RÃƒÂ©organiser les items
        const playlistContainer = document.querySelector('[data-playlist-files]');
        if (!playlistContainer) return;
        
        const items = Array.from(playlistContainer.querySelectorAll('.modal-file-item'));
        const [movedItem] = items.splice(sourceIndex, 1);
        items.splice(targetIndex, 0, movedItem);
        
        // RÃƒÂ©afficher dans le nouvel ordre
        playlistContainer.innerHTML = '';
        items.forEach(item => playlistContainer.appendChild(item));
        
        // Mettre ÃƒÂ  jour les numÃƒÂ©ros
        this.updatePlaylistFileNumbers();
    }
    
    onPlaylistFileDragEnd(event) {
        event.target.classList.remove('dragging');
        this._draggedPlaylistItem = null;
    }
    
    // ========================================================================
    // NOUVELLES MÃƒâ€°THODES - Utilitaires
    // ========================================================================
    
    /**
     * Filtre les fichiers disponibles par recherche
     */
    filterAvailableFiles(searchTerm) {
        const container = document.querySelector('[data-available-files]');
        if (!container) return;
        
        const items = container.querySelectorAll('.modal-file-item');
        const term = searchTerm.toLowerCase();
        
        items.forEach(item => {
            const name = item.getAttribute('data-file-name')?.toLowerCase() || '';
            if (name.includes(term)) {
                item.style.display = 'flex';
            } else {
                item.style.display = 'none';
            }
        });
    }
    
    /**
     * Formate une durÃƒÂ©e en ms
     */
    formatDuration(ms) {
        if (!ms) return '00:00';
        
        const seconds = Math.floor(ms / 1000);
        const minutes = Math.floor(seconds / 60);
        const s = seconds % 60;
        
        return `${minutes}:${s.toString().padStart(2, '0')}`;
    }
    
    /**
     * Ãƒâ€°chappe HTML
     */
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    // ========================================================================
    // MÃƒâ€°THODES EXISTANTES CONSERVÃƒâ€°ES (le reste du code original)
    // ========================================================================
    
    saveChannelSettings(fileId) {
        this.logDebug('modals', `Sauvegarde config canaux: ${fileId}`);
        this.close('channelSettings');
    }
    
    saveInstrumentConfig(instrumentId) {
        this.logDebug('modals', `Sauvegarde config instrument: ${instrumentId}`);
        this.close('instrumentConfig');
    }
    
    calibrateInstrument(instrumentId) {
        this.logDebug('modals', `Calibration instrument: ${instrumentId}`);
        window.app?.instrumentController?.calibrateInstrument(instrumentId);
    }
    
    handleConfirmation(confirmed, callbackId) {
        this.logDebug('modals', `Confirmation: ${confirmed}`);
        this.close('confirmation');
        if (confirmed && window[callbackId]) {
            window[callbackId]();
        }
    }

    destroy() {
        this.closeAll();
        this.contentCache?.clear();
        this.eventHandlers?.clear();
        
        const container = document.getElementById('modal-container');
        if (container) {
            container.remove();
        }
    }

	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
    buildDefaultContent(modalId, options) { 
        return `<div>Contenu par dÃƒÂ©faut pour ${modalId}</div>`; 
    }

    /**
     * Actions des modales
     */
    
    
    
    savePlaylist(playlistId) {
        this.logDebug('modals', `Sauvegarde playlist: ${playlistId || 'nouvelle'}`);
        this.close('playlistEditor');
    }
    

    /**
     * Nettoie les ressources du contrÃƒÂ´leur
     */
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

window.ModalController = ModalController;
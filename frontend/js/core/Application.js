// ============================================================================
// Fichier: frontend/js/core/Application.js
// Version: v3.8 - FIXED ALL DUPLICATIONS AND INITIALIZATION ISSUES
// Date: 2025-10-23
// Projet: midiMind v3.0 - SystÃ¨me d'Orchestration MIDI
// ============================================================================
// CORRECTIONS v3.8:
// âœ“ Suppression des duplications (HomeView, FileController)
// âœ“ Correction de la structure de initViews()
// âœ“ Correction de la structure de initControllers()
// âœ“ Correction du double export default
// âœ“ Correction de l'accolade mal placÃ©e dans initPageController
// âœ“ AmÃ©lioration de la gestion d'erreurs
// ============================================================================

// ============================================================================
// IMPORTS
// ============================================================================


class Application {
    constructor() {
        // Ã‰tat de l'application
        this.state = {
            initialized: false,
            ready: false,
            currentPage: 'home',
            backendConnected: false,
            offlineMode: false
        };
        
        // Composants de l'application
        this.eventBus = null;
        this.logger = null;
        this.debugConsole = null;
        this.notifications = null;
        
        // Services
        this.services = {
            backend: null,
            storage: null,
            midi: null,
            file: null
        };
        
        // ModÃ¨les
        this.models = {
            file: null,
            playlist: null,
            instrument: null,
            system: null,
            playback: null,
            editor: null,
            routing: null
        };
        
        // Vues
        this.views = {
            home: null,
            file: null,
            instrument: null,
            keyboard: null,
            system: null,
            routing: null,
            editor: null
        };
        
        // ContrÃ´leurs
        this.controllers = {
            navigation: null,
            file: null,
            playlist: null,
            instrument: null,
            playback: null,
            globalPlayback: null,
            system: null,
            search: null,
            routing: null,
            editor: null,
            home: null
        };
        
        // Configuration
        this.config = {
            backendUrl: 'ws://localhost:8080',
            autoReconnect: true,
            reconnectInterval: 5000,
            logLevel: 'debug',
            enableDebugConsole: true,
            offlineMode: {
                enabled: true,
                showNotification: true,
                allowLocalOperations: true
            }
        };
        
        // RÃ©fÃ©rence globale
        window.app = this;
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * Initialise l'application complÃ¨te
     */
    async init() {
        console.log('ðŸš€ Initializing midiMind v3.0...');
        
        try {
            // Ã‰tape 1: Fondations
            await this.initFoundations();
            
            // Ã‰tape 2: Services
            await this.initServices();
            
            // Ã‰tape 3: ModÃ¨les
            await this.initModels();
            
            // Ã‰tape 4: Vues (CRITIQUE)
            await this.initViews();
            
            // Ã‰tape 5: ContrÃ´leurs
            await this.initControllers();
            
            // Ã‰tape 6: Navigation
            await this.initNavigation();
            
            // Ã‰tape 7: Connexion backend (non-bloquant)
            await this.connectBackend();
            
            // Ã‰tape 8: Finalisation
            await this.finalize();
            
            this.state.initialized = true;
            this.state.ready = true;
            
            console.log('âœ… midiMind initialized successfully');
            this.logger.info('Application', 'âœ… Application ready');
            
            // Ã‰mettre Ã©vÃ©nement ready
            this.eventBus.emit('app:ready');
            
        } catch (error) {
            console.error('âŒ Failed to initialize application:', error);
            if (this.logger && this.logger.error) {
                this.logger.error('Application', 'Initialization failed:', error);
            }
            this.handleInitError(error);
        }
    }
    
    /**
     * Initialise les composants de base
     */
    async initFoundations() {
        console.log('ðŸ”¦ Initializing foundations...');
        
        // EventBus (dÃ©jÃ  instanciÃ© globalement ou crÃ©er nouveau)
        this.eventBus = window.eventBus || new EventBus();
        window.eventBus = this.eventBus;
        
        // Logger - CORRIGÃ‰ : crÃ©er une nouvelle instance
        if (window.Logger && typeof window.Logger === 'function') {
            this.logger = window.logger || new Logger({
                level: this.config.logLevel,
                eventBus: this.eventBus
            });
            window.logger = this.logger;
        } else {
            // Fallback vers console si Logger n'est pas disponible
            console.warn('Logger class not available, using console as fallback');
            this.logger = console;
            window.logger = console;
        }
        
        // DebugConsole
        if (this.config.enableDebugConsole && window.DebugConsole) {
            if (typeof window.DebugConsole === 'function') {
                this.debugConsole = new DebugConsole(this.eventBus, this.logger);
                window.debugConsole = this.debugConsole;
            } else {
                this.debugConsole = window.DebugConsole;
            }
        }
        
        // Notifications
        if (window.Notifications && typeof window.Notifications === 'function') {
            this.notifications = new Notifications(this.eventBus);
            window.notifications = this.notifications;
        } else if (window.NotificationManager) {
            this.notifications = window.NotificationManager;
        }
        
        this.logger.info('Application', 'âœ“ Foundations initialized');
    }
    
    /**
     * Initialise les services
     */
    async initServices() {
        console.log('ðŸ”§ Initializing services...');
        
        // BackendService
        if (window.BackendService) {
            this.services.backend = new BackendService(
                this.config.backendUrl,
                this.eventBus,
                this.logger
            );
        }
        
        // StorageService
        if (window.StorageService) {
            this.services.storage = new StorageService(this.eventBus, this.logger);
        }
        
        // MidiService
        if (window.MidiService) {
            this.services.midi = new MidiService(this.eventBus, this.logger);
        }
        
        // FileService - CORRIGÃ‰ : passer les paramÃ¨tres requis
        if (window.FileService) {
            this.services.file = new FileService(
                this.services.backend,
                this.eventBus,
                this.logger
            );
        }
        
        this.logger.info('Application', 'âœ“ Services initialized');
    }
    
    /**
     * Initialise les modÃ¨les de donnÃ©es
     */
    async initModels() {
        console.log('ðŸ—‚ï¸ Initializing models...');
        
        // FileModel - CORRIGÃ‰ : 3 paramÃ¨tres requis (eventBus, backend, logger)
        if (window.FileModel) {
            this.models.file = new FileModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // PlaylistModel - CORRIGÃ‰ : 3 paramÃ¨tres requis
        if (window.PlaylistModel) {
            this.models.playlist = new PlaylistModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // InstrumentModel - CORRIGÃ‰ : 3 paramÃ¨tres requis
        if (window.InstrumentModel) {
            this.models.instrument = new InstrumentModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // SystemModel - CORRIGÃ‰ : 3 paramÃ¨tres requis
        if (window.SystemModel) {
            this.models.system = new SystemModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // PlaybackModel - CORRIGÃ‰ : 3 paramÃ¨tres requis
        if (window.PlaybackModel) {
            this.models.playback = new PlaybackModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // EditorModel - CORRIGÃ‰ : 3 paramÃ¨tres requis
        if (window.EditorModel) {
            this.models.editor = new EditorModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // RoutingModel - CORRIGÃ‰ : 3 paramÃ¨tres requis
        if (window.RoutingModel) {
            this.models.routing = new RoutingModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        this.logger.info('Application', 'âœ“ Models initialized');
    }
    
    /**
     * Initialise les vues - CORRIGÃ‰ (sans duplication)
     */
    async initViews() {
        console.log('ðŸ–¼ï¸ Initializing views...');
        
        // HomeView - Conteneur 'home' - CORRIGÃ‰ : une seule initialisation
        if (window.HomeView) {
            this.views.home = new HomeView('home', this.eventBus);
            if (typeof this.views.home.init === 'function') {
                this.views.home.init();
            } else if (typeof this.views.home.render === 'function') {
                this.views.home.render();
            }
            console.log('âœ“ HomeView initialized');
        }
        
        // EditorView - Conteneur 'editor'
        const editorElement = document.getElementById('editor');
        if (editorElement && window.EditorView) {
            this.views.editor = new EditorView(editorElement, this.eventBus, this.logger);
            console.log('âœ“ EditorView initialized');
        }
        
        // RoutingView - Conteneur 'routing'
        const routingElement = document.getElementById('routing');
        if (routingElement && window.RoutingView) {
            this.views.routing = new RoutingView(routingElement, this.eventBus, this.logger);
            console.log('âœ“ RoutingView initialized');
        }
        
        // KeyboardView - Conteneur 'keyboard'
        const keyboardElement = document.getElementById('keyboard');
        if (keyboardElement && window.KeyboardView) {
            this.views.keyboard = new KeyboardView(keyboardElement, this.eventBus);
            if (typeof this.views.keyboard.init === 'function') {
                this.views.keyboard.init();
            } else if (typeof this.views.keyboard.render === 'function') {
                this.views.keyboard.render();
            }
            console.log('âœ“ KeyboardView initialized');
        }
        
        // InstrumentView - Conteneur 'instruments'
        const instrumentElement = document.getElementById('instruments');
        if (instrumentElement && window.InstrumentView) {
            this.views.instrument = new InstrumentView(instrumentElement, this.eventBus);
            if (typeof this.views.instrument.init === 'function') {
                this.views.instrument.init();
            } else if (typeof this.views.instrument.render === 'function') {
                this.views.instrument.render();
            }
            console.log('âœ“ InstrumentView initialized');
        }
        
        // SystemView - Conteneur 'system'
        const systemElement = document.getElementById('system');
        if (systemElement && window.SystemView) {
            this.views.system = new SystemView(systemElement, this.eventBus);
            if (typeof this.views.system.init === 'function') {
                this.views.system.init();
            } else if (typeof this.views.system.render === 'function') {
                this.views.system.render();
            }
            console.log('âœ“ SystemView initialized');
        }
        
        // FileView - si disponible
        const fileElement = document.querySelector('.files-list');
        if (fileElement && window.FileView) {
            this.views.file = new FileView(fileElement, this.eventBus);
            console.log('âœ“ FileView initialized');
        }
        
        this.logger.info('Application', 'âœ“ Views initialized');
    }
    
    /**
     * Initialise les contrÃ´leurs - CORRIGÃ‰ (sans duplication)
     */
    async initControllers() {
        console.log('ðŸŽ® Initializing controllers...');
        
        // NavigationController
        if (window.NavigationController) {
            this.controllers.navigation = new NavigationController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // HomeController
        if (window.HomeController) {
            this.controllers.home = new HomeController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // FileController - CORRIGÃ‰ : une seule initialisation
        if (window.FileController) {
            this.controllers.file = new FileController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // PlaylistController
        if (window.PlaylistController) {
            this.controllers.playlist = new PlaylistController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }

        // InstrumentController
        if (window.InstrumentController) {
            this.controllers.instrument = new InstrumentController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // SystemController
        if (window.SystemController) {
            this.controllers.system = new SystemController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // PlaybackController
        if (window.PlaybackController) {
            this.controllers.playback = new PlaybackController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // RoutingController
        if (window.RoutingController) {
            this.controllers.routing = new RoutingController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // EditorController
        if (window.EditorController) {
            this.controllers.editor = new EditorController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // KeyboardController
        if (window.KeyboardController) {
            this.controllers.keyboard = new KeyboardController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // GlobalPlaybackController
        if (window.GlobalPlaybackController) {
            this.controllers.globalPlayback = new GlobalPlaybackController(
                this.eventBus,
                this.services.backend,
                this.models.file,
                this.logger
            );
        }
        
        // SearchController
        if (window.SearchController) {
            this.controllers.search = new SearchController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        this.logger.info('Application', 'âœ“ Controllers initialized');
    }
    
    /**
     * Initialise la navigation
     */
    async initNavigation() {
        console.log('ðŸ§­ Initializing navigation...');
        
        // Gestionnaire de navigation
        window.addEventListener('hashchange', () => this.handleNavigation());
        
        // Navigation initiale
        this.handleNavigation();
        
        this.logger.info('Application', 'âœ“ Navigation initialized');
    }
    
    /**
     * Connexion au backend (non-bloquant)
     */
    async connectBackend() {
        console.log('ðŸ”Œ Connecting to backend...');
        
        if (!this.services.backend) {
            console.warn('âš ï¸ Backend service not available');
            this.enableOfflineMode('Backend service not available');
            return;
        }
        
        try {
            // Tentative de connexion
            await this.services.backend.connect();
            
            // Attendre confirmation de connexion (timeout 3s)
            const connected = await this.waitForConnection(3000);
            
            if (connected) {
                this.state.backendConnected = true;
                this.logger.info('Application', 'âœ… Backend connected');
                this.showConnectionStatus(true);
            } else {
                throw new Error('Connection timeout');
            }
            
        } catch (error) {
            console.warn('âš ï¸ Backend connection failed:', error.message);
            this.logger.warn('Application', 'Backend connection failed, continuing in offline mode');
            this.enableOfflineMode('Backend connection failed');
        }
    }
    
    /**
     * Attend la connexion backend avec timeout
     */
    waitForConnection(timeout) {
        return new Promise((resolve) => {
            let timeoutId;
            
            const checkConnection = () => {
                if (this.services.backend.isConnected()) {
                    clearTimeout(timeoutId);
                    resolve(true);
                }
            };
            
            // Ã‰couter l'Ã©vÃ©nement de connexion
            this.eventBus.once('backend:connected', () => {
                clearTimeout(timeoutId);
                resolve(true);
            });
            
            // Timeout
            timeoutId = setTimeout(() => {
                resolve(false);
            }, timeout);
            
            // VÃ©rification immÃ©diate
            checkConnection();
        });
    }
    
    /**
     * Active le mode offline
     */
    enableOfflineMode(reason) {
        this.state.offlineMode = true;
        this.state.backendConnected = false;
        
        this.logger.warn('Application', `Offline mode enabled: ${reason}`);
        
        if (this.config.offlineMode.showNotification && this.notifications) {
            this.notifications.show(
                'Working in offline mode - Some features may be limited',
                'warning',
                { duration: 5000 }
            );
        }
        
        this.showConnectionStatus(false);
        
        // Planifier une tentative de reconnexion
        if (this.config.autoReconnect) {
            this.scheduleReconnect();
        }
    }
    
    /**
     * Affiche le statut de connexion
     */
    showConnectionStatus(connected) {
        const statusElement = document.getElementById('connection-status');
        if (statusElement) {
            statusElement.className = connected ? 'online' : 'offline';
            statusElement.textContent = connected ? 'Online' : 'Offline';
        }
    }
    
    /**
     * Planifie une tentative de reconnexion
     */
    scheduleReconnect() {
        if (this.reconnectTimeout) {
            clearTimeout(this.reconnectTimeout);
        }
        
        this.reconnectTimeout = setTimeout(() => {
            this.logger.info('Application', 'Attempting to reconnect...');
            this.connectBackend();
        }, this.config.reconnectInterval);
    }
    
    /**
     * Finalisation de l'initialisation
     */
    async finalize() {
        console.log('ðŸŽ¯ Finalizing initialization...');
        
        // Attacher les gestionnaires d'Ã©vÃ©nements
        this.attachAppEvents();
        this.attachErrorHandlers();
        
        // Afficher l'interface
        this.showInterface();
        
        this.logger.info('Application', 'âœ“ Finalization complete');
    }
    
    // ========================================================================
    // NAVIGATION
    // ========================================================================
    
    handleNavigation() {
        const hash = window.location.hash.slice(1) || 'home';
        const page = hash.split('/')[0];
        
        this.logger.debug('Application', `Navigating to: ${page}`);
        
        // Masquer toutes les pages
        this.hideAllPages();
        
        // Afficher la page demandÃ©e
        const pageElement = document.getElementById(page);
        if (pageElement) {
            pageElement.style.display = 'block';
            this.state.currentPage = page;
            
            // Ã‰mettre Ã©vÃ©nement de navigation
            this.eventBus.emit('navigation:changed', { page, hash });
            
            // Initialiser le contrÃ´leur de la page si nÃ©cessaire
            this.initPageController(page);
        } else {
            this.logger.warn('Application', `Page not found: ${page}`);
            window.location.hash = '#home';
        }
    }
    
    hideAllPages() {
        const pages = ['home', 'editor', 'routing', 'keyboard', 'instruments', 'system'];
        pages.forEach(page => {
            const element = document.getElementById(page);
            if (element) {
                element.style.display = 'none';
            }
        });
    }
    
    initPageController(page) {
        // Initialiser les contrÃ´leurs et vues spÃ©cifiques aux pages
        switch (page) {
            case 'home':
                // Initialiser le contrÃ´leur s'il existe
                if (this.controllers.home && typeof this.controllers.home.init === 'function') {
                    this.controllers.home.init();
                }
                // Forcer le rendu de la vue si pas encore fait
                else if (this.views.home && typeof this.views.home.render === 'function') {
                    this.views.home.render();
                }
                break;
            case 'system':
                if (this.controllers.system && typeof this.controllers.system.init === 'function') {
                    this.controllers.system.init();
                }
                else if (this.views.system && typeof this.views.system.render === 'function') {
                    this.views.system.render();
                }
                break;
            case 'editor':
                if (this.controllers.editor && typeof this.controllers.editor.init === 'function') {
                    this.controllers.editor.init();
                }
                else if (this.views.editor && typeof this.views.editor.render === 'function') {
                    this.views.editor.render();
                }
                break;
        }
    }
    
    // ========================================================================
    // GESTION DES ERREURS
    // ========================================================================
    
    attachErrorHandlers() {
        // Erreurs JavaScript non capturÃ©es
        window.addEventListener('error', (event) => {
            if (this.logger && this.logger.error) {
                this.logger.error('Application', 'Uncaught error:', event.error);
            }
            this.handleError(event.error);
        });
        
        // Promesses rejetÃ©es non capturÃ©es
        window.addEventListener('unhandledrejection', (event) => {
            if (this.logger && this.logger.error) {
                this.logger.error('Application', 'Unhandled rejection:', event.reason);
            }
            this.handleError(event.reason);
        });
    }
    
    handleError(error) {
        // Afficher notification
        if (this.notifications && this.notifications.show) {
            this.notifications.show(
                `Error: ${error.message || error}`,
                'error'
            );
        }
        
        // Logger dÃ©taillÃ©
        if (this.logger && this.logger.error) {
            this.logger.error('Application', 'Error occurred:', {
                message: error.message,
                stack: error.stack
            });
        }
        
        // Ã‰mettre Ã©vÃ©nement erreur
        this.eventBus.emit('app:error', { error });
    }
    
    handleInitError(error) {
        if (this.logger && this.logger.error) {
            this.logger.error('Application', 'Initialization error:', error);
        }
        
        // Afficher erreur Ã  l'utilisateur
        if (this.notifications && this.notifications.show) {
            this.notifications.show(
                `Failed to initialize: ${error.message}`,
                'error',
                { duration: 0, closable: true }
            );
        }
        
        // Essayer de continuer en mode dÃ©gradÃ©
        this.state.initialized = false;
        this.state.ready = false;
    }
    
    // ========================================================================
    // Ã‰VÃ‰NEMENTS APPLICATION
    // ========================================================================
    
    attachAppEvents() {
        // Ã‰couter la connexion backend
        this.eventBus.on('backend:connected', () => {
            this.logger.info('Application', 'âœ… Backend connected event received');
            this.state.backendConnected = true;
            this.state.offlineMode = false;
            this.showConnectionStatus(true);
        });
        
        // Ã‰couter la dÃ©connexion backend
        this.eventBus.on('websocket:disconnected', () => {
            this.logger.warn('Application', 'ðŸ”´ Backend disconnected');
            this.state.backendConnected = false;
            
            if (!this.state.offlineMode) {
                this.enableOfflineMode('Connection lost');
            }
        });
        
        // Ã‰couter les erreurs backend
        this.eventBus.on('backend:connection-failed', (data) => {
            this.logger.error('Application', 'Backend connection failed:', data);
        });
    }
    
    // ========================================================================
    // INTERFACE UTILISATEUR
    // ========================================================================
    
    showInterface() {
        // Masquer le loading indicator
        const loading = document.getElementById('loading-indicator');
        if (loading) {
            loading.style.display = 'none';
        }
        
        // Afficher l'interface principale
        const app = document.getElementById('app');
        if (app) {
            app.style.display = 'block';
        }
        
        // S'assurer que la page home est visible au dÃ©marrage
        const homePage = document.getElementById('home');
        if (homePage) {
            homePage.style.display = 'block';
        }
    }
    
    // ========================================================================
    // API PUBLIQUE
    // ========================================================================
    
    /**
     * Obtient l'Ã©tat de l'application
     * @returns {Object}
     */
    getState() {
        return { ...this.state };
    }
    
    /**
     * VÃ©rifie si le backend est connectÃ©
     * @returns {boolean}
     */
    isBackendConnected() {
        return this.state.backendConnected && this.services.backend?.isConnected();
    }
    
    /**
     * VÃ©rifie si en mode offline
     * @returns {boolean}
     */
    isOfflineMode() {
        return this.state.offlineMode;
    }
    
    /**
     * Obtient un service par nom
     * @param {string} name - Nom du service
     * @returns {Object|null}
     */
    getService(name) {
        return this.services[name] || null;
    }
    
    /**
     * Obtient un modèle par nom
     * @param {string} name - Nom du modèle
     * @returns {Object|null}
     */
    getModel(name) {
        return this.models[name] || null;
    }
    
    /**
     * Obtient une vue par nom
     * @param {string} name - Nom de la vue
     * @returns {Object|null}
     */
    getView(name) {
        return this.views[name] || null;
    }
    
    /**
     * Obtient un contrôleur par nom
     * @param {string} name - Nom du contrôleur
     * @returns {Object|null}
     */
    getController(name) {
        return this.controllers[name] || null;
    }
}

// Export par dÃ©faut
window.Application = Application;

// ============================================================================
// FIN DU FICHIER Application.js v3.8
// ============================================================================
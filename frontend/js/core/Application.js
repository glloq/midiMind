// ============================================================================
// Fichier: frontend/js/core/Application.js
// Version: v3.9.3 - FIX ROUTER INITIALIZATION
// Date: 2025-11-02
// Projet: MidiMind v3.1
// ============================================================================
// CORRECTIONS v3.9.3:
// Ã¢Å“â€¦ CRITIQUE: Initialisation correcte du Router avec objet config
// Ã¢Å“â€¦ CRITIQUE: Enregistrement des routes avant initialisation
// Ã¢Å“â€¦ CRITIQUE: Connexion Router <-> NavigationController
// Ã¢Å“â€¦ Fix: Route not found errors
// ============================================================================

class Application {
    constructor() {
        // Ãƒâ€°tat de l'application
        this.state = {
            initialized: false,
            ready: false,
            currentPage: 'home',
            backendConnected: false,
            offlineMode: false,
            reconnectAttempts: 0
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
        
        // ModÃƒÂ¨les
        this.models = {
            state: null,
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
            editor: null,
            playlist: null,
            visualizer: null
        };
        
        // ContrÃƒÂ´leurs
        this.controllers = {
            navigation: null,
            file: null,
            playlist: null,
            instrument: null,
            keyboard: null,
            playback: null,
            globalPlayback: null,
            system: null,
            home: null,
            routing: null,
            editor: null,
            visualizer: null
        };
        
        // Configuration
        this.config = {
            backendUrl: (typeof AppConfig !== 'undefined' && AppConfig.backend) ? AppConfig.backend.url : 'ws://localhost:8080',
            autoReconnect: true,
            reconnectInterval: 5000,
            maxReconnectAttempts: 10,
            logLevel: 'info',
            enableDebugConsole: true,
            offlineMode: {
                enabled: true,
                showNotification: true,
                allowLocalOperations: true
            }
        };
        
        // RÃƒÂ©fÃƒÂ©rence globale
        window.app = this;
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * Initialise l'application complÃƒÂ¨te
     */
    async init() {
        console.log('Ã°Å¸Å¡â‚¬ Initializing MidiMind v3.1...');
        
        try {
            // Ãƒâ€°tape 1: Fondations
            await this.initFoundations();
            
            // Ãƒâ€°tape 2: Services
            await this.initServices();
            
            // Ãƒâ€°tape 3: ModÃƒÂ¨les
            await this.initModels();
            
            // Ãƒâ€°tape 4: Vues
            await this.initViews();
            
            // Ãƒâ€°tape 5: ContrÃƒÂ´leurs
            await this.initControllers();
            
            // Ãƒâ€°tape 6: Navigation & Router
            await this.initNavigation();
            
            // Ãƒâ€°tape 7: Connexion backend (non-bloquant)
            this.connectBackend().catch(err => {
                this.log('warn', 'Backend connection failed, continuing in offline mode', err);
            });
            
            // Ãƒâ€°tape 8: Finalisation
            await this.finalize();
            
            this.state.initialized = true;
            this.state.ready = true;
            
            console.log('Ã¢Å“â€¦ MidiMind v3.1 initialized successfully');
            this.log('info', 'Ã¢Å“â€¦ Application ready');
            
            // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement ready
            if (this.eventBus) {
                this.eventBus.emit('app:ready');
            }
            
        } catch (error) {
            console.error('Ã¢ÂÅ’ Failed to initialize application:', error);
            this.log('error', 'Initialization failed', error);
            this.handleInitError(error);
        }
    }
    
    /**
     * Initialise les composants de base
     */
    async initFoundations() {
        console.log('Ã°Å¸â€œÂ¦ Initializing foundations...');
        
        // EventBus
        this.eventBus = window.eventBus || new EventBus();
        window.eventBus = this.eventBus;
        
        // Logger avec fallback robuste
        this.logger = this.createLogger();
        window.logger = this.logger;
        
        // DebugConsole
        if (this.config.enableDebugConsole && window.DebugConsole) {
            try {
                this.debugConsole = new DebugConsole(this.eventBus, this.logger);
                window.debugConsole = this.debugConsole;
            } catch (e) {
                console.warn('DebugConsole initialization failed:', e);
            }
        }
        
        // NotificationManager
        if (window.NotificationManager) {
            try {
                this.notifications = new NotificationManager();
                window.notificationManager = this.notifications;
            } catch (e) {
                console.warn('NotificationManager initialization failed:', e);
            }
        } else if (window.Notifications) {
            try {
                this.notifications = new Notifications(this.eventBus);
                window.notifications = this.notifications;
            } catch (e) {
                console.warn('Notifications initialization failed:', e);
            }
        }
        
        this.log('info', 'Ã¢Å“â€ Foundations initialized');
    }
    
    /**
     * CrÃƒÂ©e un logger robuste avec fallback
     */
    createLogger() {
        // Si Logger est disponible comme classe
        if (window.Logger && typeof window.Logger === 'function') {
            try {
                return new Logger({
                    level: this.config.logLevel,
                    eventBus: this.eventBus
                });
            } catch (e) {
                console.warn('Failed to create Logger, using fallback:', e);
            }
        }
        
        // Si logger existe dÃƒÂ©jÃƒÂ 
        if (window.logger && typeof window.logger.info === 'function') {
            return window.logger;
        }
        
        // Fallback: wrapper console avec interface Logger
        return {
            debug: (...args) => console.log(...args),
            info: (...args) => console.info(...args),
            warn: (...args) => console.warn(...args),
            error: (...args) => console.error(...args),
            log: (...args) => console.log(...args)
        };
    }
    
    /**
     * MÃƒÂ©thode helper pour logger de faÃƒÂ§on sÃƒÂ©curisÃƒÂ©e
     */
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        } else {
            console[level]?.(...args) || console.log(...args);
        }
    }
    
    /**
     * Initialise les services
     */
    async initServices() {
        console.log('Ã°Å¸â€Â§ Initializing services...');
        
        // BackendService
        if (window.BackendService) {
            try {
                this.services.backend = new BackendService(
                    this.config.backendUrl,
                    this.eventBus,
                    this.logger
                );
                window.backendService = this.services.backend;
            } catch (e) {
                this.log('warn', 'BackendService initialization failed:', e);
            }
        }
        
        // StorageService
        if (window.StorageService) {
            try {
                this.services.storage = new StorageService(this.eventBus, this.logger);
                window.storageService = this.services.storage;
            } catch (e) {
                this.log('warn', 'StorageService initialization failed:', e);
            }
        }
        
        // MidiService
        if (window.MidiService) {
            try {
                this.services.midi = new MidiService(this.eventBus, this.logger);
                window.midiService = this.services.midi;
            } catch (e) {
                this.log('warn', 'MidiService initialization failed:', e);
            }
        }
        
        // FileService
        if (window.FileService) {
            try {
                this.services.file = new FileService(
                    this.services.backend,
                    this.eventBus,
                    this.logger
                );
                window.fileService = this.services.file;
            } catch (e) {
                this.log('warn', 'FileService initialization failed:', e);
            }
        }
        
        this.log('info', 'Ã¢Å“â€ Services initialized');
    }
    
    /**
     * Initialise les modÃƒÂ¨les
     */
    async initModels() {
        console.log('Ã°Å¸â€œÅ  Initializing models...');
        
        // StateModel
        if (window.StateModel) {
            this.models.state = new StateModel(this.eventBus);
            window.stateModel = this.models.state;
        }
        
        // FileModel
        if (window.FileModel) {
            this.models.file = new FileModel(this.eventBus, this.services.backend);
            window.fileModel = this.models.file;
        }
        
        // PlaylistModel
        if (window.PlaylistModel) {
            this.models.playlist = new PlaylistModel(this.eventBus, this.services.backend);
            window.playlistModel = this.models.playlist;
        }
        
        // InstrumentModel
        if (window.InstrumentModel) {
            this.models.instrument = new InstrumentModel(this.eventBus, this.services.backend);
            window.instrumentModel = this.models.instrument;
        }
        
        // SystemModel
        if (window.SystemModel) {
            this.models.system = new SystemModel(this.eventBus);
            window.systemModel = this.models.system;
        }
        
        // PlaybackModel
        if (window.PlaybackModel) {
            this.models.playback = new PlaybackModel(this.eventBus);
            window.playbackModel = this.models.playback;
        }
        
        // EditorModel
        if (window.EditorModel) {
            this.models.editor = new EditorModel(this.eventBus);
            window.editorModel = this.models.editor;
        }
        
        // RoutingModel
        if (window.RoutingModel) {
            this.models.routing = new RoutingModel(this.eventBus, this.services.backend);
            window.routingModel = this.models.routing;
        }
        
        this.log('info', 'Ã¢Å“â€ Models initialized');
    }
    
    /**
     * Initialise les vues
     */
    async initViews() {
        console.log('Ã°Å¸â€“Â¨ Initializing views...');
        
        // HomeView
        if (window.HomeView) {
            this.views.home = new HomeView('home-view', this.eventBus);
            window.homeView = this.views.home;
        }
        
        // FileView
        if (window.FileView) {
            this.views.file = new FileView('file-view', this.eventBus);
            window.fileView = this.views.file;
        }
        
        // InstrumentView
        if (window.InstrumentView) {
            this.views.instrument = new InstrumentView('instrument-view', this.eventBus);
            window.instrumentView = this.views.instrument;
        }
        
        // KeyboardView
        if (window.KeyboardView) {
            this.views.keyboard = new KeyboardView('keyboard-view', this.eventBus);
            window.keyboardView = this.views.keyboard;
        }
        
        // SystemView
        if (window.SystemView) {
            this.views.system = new SystemView('system-view', this.eventBus);
            window.systemView = this.views.system;
        }
        
        // RoutingView
        if (window.RoutingView) {
            this.views.routing = new RoutingView('routing-view', this.eventBus);
            window.routingView = this.views.routing;
        }
        
        // EditorView
        if (window.EditorView) {
            this.views.editor = new EditorView('editor-view', this.eventBus);
            window.editorView = this.views.editor;
        }
        
        // PlaylistView
        if (window.PlaylistView) {
            this.views.playlist = new PlaylistView('playlist-view', this.eventBus);
            window.playlistView = this.views.playlist;
        }
        
        // VisualizerView
        if (window.VisualizerView) {
            this.views.visualizer = new VisualizerView('visualizer-view', this.eventBus);
            window.visualizerView = this.views.visualizer;
        }
        
        this.log('info', 'Ã¢Å“â€ Views initialized');
    }
    
    /**
     * Initialise les contrÃƒÂ´leurs
     */
    async initControllers() {
        console.log('Ã°Å¸Å½Â® Initializing controllers...');
        
        // FileController
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
        
        // GlobalPlaybackController
        if (window.GlobalPlaybackController) {
            this.controllers.globalPlayback = new GlobalPlaybackController(
                this.eventBus,
                this.models,
                this.services.backend,
                this.notifications
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
        
        // VisualizerController
        if (window.VisualizerController) {
            this.controllers.visualizer = new VisualizerController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        this.log('info', 'Ã¢Å“â€ Controllers initialized');
    }
    
    /**
     * Initialise la navigation
     */
    async initNavigation() {
        console.log('Ã°Å¸â€”Âº Initializing navigation...');
        
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
        
        // Router avec configuration correcte
        if (window.Router) {
            this.router = new Router({
                mode: 'hash',
                useTransitions: true,
                transitionDuration: 300
            });
            
            // Enregistrer les routes principales
            this.registerRoutes();
            
            // Connecter le Router au NavigationController
            if (this.controllers.navigation) {
                this.router.on('route-changed', (data) => {
                    const pageKey = data.path.replace('/', '') || 'home';
                    this.controllers.navigation.showPage(pageKey);
                });
            }
        }
        
        this.log('info', 'Ã¢Å“â€ Navigation initialized');
    }
    
    /**
     * Enregistre les routes de l'application
     */
    registerRoutes() {
        if (!this.router) return;
        
        // Routes principales
        this.router.route('/home', {
            title: 'MidiMind - Accueil',
            view: 'home'
        });
        
        this.router.route('/files', {
            title: 'MidiMind - Fichiers',
            view: 'files'
        });
        
        this.router.route('/editor', {
            title: 'MidiMind - Ãƒâ€°diteur',
            view: 'editor'
        });
        
        this.router.route('/routing', {
            title: 'MidiMind - Routage',
            view: 'routing'
        });
        
        this.router.route('/instruments', {
            title: 'MidiMind - Instruments',
            view: 'instruments'
        });
        
        this.router.route('/keyboard', {
            title: 'MidiMind - Clavier',
            view: 'keyboard'
        });
        
        this.router.route('/playlist', {
            title: 'MidiMind - Playlist',
            view: 'playlist'
        });
        
        this.router.route('/system', {
            title: 'MidiMind - SystÃƒÂ¨me',
            view: 'system'
        });
        
        this.router.route('/visualizer', {
            title: 'MidiMind - Visualiseur',
            view: 'visualizer'
        });
        
        // Route par dÃƒÂ©faut (redirige vers home)
        this.router.route('/', {
            title: 'MidiMind - Accueil',
            view: 'home'
        });
        
        // Route 404
        this.router.notFound({
            title: 'MidiMind - Page non trouvÃƒÂ©e',
            view: 'home' // Rediriger vers home en cas de route inconnue
        });
        
        this.log('info', 'Ã¢Å“â€ Routes registered');
        
        // DÃƒÂ©marrer le routing maintenant que les routes sont enregistrÃƒÂ©es
        this.router.startRouting();
        this.log('info', 'Ã¢Å“â€ Routing started');
    }
    
    /**
     * Connexion au backend
     */
    async connectBackend() {
        if (!this.services.backend) {
            this.log('warn', 'BackendService not available');
            return false;
        }
        
        this.log('info', 'Connecting to backend...');
        
        try {
            const success = await this.services.backend.connect();
            
            if (success) {
                this.state.backendConnected = true;
                this.log('info', 'Ã¢Å“â€¦ Backend connected');
                
                if (this.eventBus) {
                    this.eventBus.emit('app:backend-connected');
                }
                
                return true;
            } else {
                throw new Error('Connection failed');
            }
            
        } catch (error) {
            this.log('warn', 'Backend connection failed:', error.message);
            this.state.backendConnected = false;
            
            if (this.eventBus) {
                this.eventBus.emit('app:backend-connection-failed', { error });
            }
            
            return false;
        }
    }
    
    /**
     * Finalisation de l'initialisation
     */
    async finalize() {
        console.log('Ã°Å¸ÂÂ Finalizing initialization...');
        
        // Ãƒâ€°vÃƒÂ©nements d'erreur globaux
        this.setupErrorHandlers();
        
        // Ãƒâ€°vÃƒÂ©nements de connexion
        this.setupConnectionHandlers();
        
        // Raccourcis clavier
        if (window.KeyboardShortcuts) {
            this.keyboardShortcuts = new KeyboardShortcuts(this.eventBus, this.logger);
        }
        
        this.log('info', 'Ã¢Å“â€ Finalization complete');
    }
    
    /**
     * Configure les gestionnaires d'erreurs
     */
    setupErrorHandlers() {
        window.addEventListener('error', (event) => {
            this.log('error', 'Global error:', event.error);
            
            if (this.eventBus) {
                this.eventBus.emit('app:error', { 
                    error: event.error,
                    message: event.message,
                    filename: event.filename,
                    lineno: event.lineno,
                    colno: event.colno
                });
            }
        });
        
        window.addEventListener('unhandledrejection', (event) => {
            this.log('error', 'Unhandled promise rejection:', event.reason);
            
            if (this.eventBus) {
                this.eventBus.emit('app:unhandled-rejection', { 
                    reason: event.reason 
                });
            }
        });
    }
    
    /**
     * Configure les gestionnaires de connexion
     */
    setupConnectionHandlers() {
        if (!this.eventBus) return;
        
        // Backend connectÃƒÂ©
        this.eventBus.on('backend:connected', (data) => {
            this.state.backendConnected = true;
            this.state.offlineMode = false;
            this.state.reconnectAttempts = 0;
            
            this.log('info', 'Ã¢Å“â€¦ Backend connected');
            
            if (this.notifications) {
                this.notifications.show('Backend connected', 'success', 3000);
            }
        });
        
        // Backend dÃƒÂ©connectÃƒÂ©
        this.eventBus.on('backend:disconnected', (data) => {
            this.state.backendConnected = false;
            
            this.log('warn', 'Ã¢Å¡Â Ã¯Â¸Â Backend disconnected');
            
            if (this.notifications && !this.state.offlineMode) {
                this.notifications.show('Backend disconnected', 'warning', 5000);
            }
        });
        
        // Mode offline
        this.eventBus.on('backend:offline-mode', (data) => {
            this.state.offlineMode = true;
            this.state.backendConnected = false;
            
            this.log('warn', 'Ã¢Å¡Â Ã¯Â¸Â Offline mode activated');
            
            if (this.notifications && this.config.offlineMode.showNotification) {
                this.notifications.show(
                    'Offline mode - Backend unavailable',
                    'warning',
                    0
                );
            }
        });
        
        // Tentative de reconnexion
        this.eventBus.on('backend:reconnect-scheduled', (data) => {
            this.state.reconnectAttempts = data.attempt;
            
            this.log('info', `Reconnect attempt ${data.attempt}/${data.maxAttempts}`);
        });
    }
    
    /**
     * GÃƒÂ¨re les erreurs d'initialisation
     */
    handleInitError(error) {
        console.error('Ã¢ÂÅ’ Initialization error:', error);
        
        if (this.notifications) {
            this.notifications.show(
                'Application initialization failed: ' + error.message,
                'error',
                0
            );
        }
        
        // Afficher un message d'erreur dans l'interface
        const errorDiv = document.createElement('div');
        errorDiv.style.cssText = `
            position: fixed;
            top: 20px;
            left: 50%;
            transform: translateX(-50%);
            background: #dc3545;
            color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
            z-index: 10000;
            max-width: 500px;
        `;
        errorDiv.innerHTML = `
            <h3 style="margin: 0 0 10px 0;">Ã¢ÂÅ’ Initialization Failed</h3>
            <p style="margin: 0;">${error.message}</p>
            <button onclick="location.reload()" 
                    style="margin-top: 15px; padding: 8px 16px; border: none; background: white; color: #dc3545; cursor: pointer; border-radius: 4px;">
                Reload Application
            </button>
        `;
        document.body.appendChild(errorDiv);
    }
    
    // ========================================================================
    // MÃƒâ€°THODES PUBLIQUES
    // ========================================================================
    
    /**
     * Obtient l'ÃƒÂ©tat de l'application
     */
    getState() {
        return { ...this.state };
    }
    
    /**
     * Navigue vers une page
     */
    navigateTo(page) {
        if (this.controllers.navigation) {
            this.controllers.navigation.showPage(page);
        }
    }
    
    /**
     * Reconnexion manuelle au backend
     */
    async reconnectBackend() {
        if (this.services.backend) {
            this.state.offlineMode = false;
            this.state.reconnectAttempts = 0;
            
            if (typeof this.services.backend.enableReconnection === 'function') {
                this.services.backend.enableReconnection();
            }
            
            return await this.connectBackend();
        }
        return false;
    }
    
    /**
     * DÃƒÂ©truit l'application
     */
    destroy() {
        this.log('info', 'Destroying application...');
        
        // DÃƒÂ©truire les contrÃƒÂ´leurs
        Object.values(this.controllers).forEach(controller => {
            if (controller && typeof controller.destroy === 'function') {
                controller.destroy();
            }
        });
        
        // DÃƒÂ©truire les vues
        Object.values(this.views).forEach(view => {
            if (view && typeof view.destroy === 'function') {
                view.destroy();
            }
        });
        
        // DÃƒÂ©connecter le backend
        if (this.services.backend && typeof this.services.backend.disconnect === 'function') {
            this.services.backend.disconnect();
        }
        
        this.state.initialized = false;
        this.state.ready = false;
        
        this.log('info', 'Ã¢Å“â€ Application destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = Application;
}

window.Application = Application;
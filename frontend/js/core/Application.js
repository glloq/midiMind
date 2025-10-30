// ============================================================================
// Fichier: frontend/js/core/Application.js
// Version: v3.9.2 - FIX ORDRE PARAMÃˆTRES CONTRÃ”LEURS
// Date: 2025-10-30
// Projet: MidiMind v3.1
// ============================================================================
// CORRECTIONS v3.9.2:
// âœ… CRITIQUE: Ordre correct des paramÃ¨tres pour TOUS les contrÃ´leurs
// âœ… BaseController attend: (eventBus, models, views, notifications, debugConsole)
// âœ… Correction: FileController, HomeController, InstrumentController, etc.
// ============================================================================
// CORRECTIONS v3.9.1:
// âœ… Encodage UTF-8 correct (tous caractÃ¨res spÃ©ciaux)
// âœ… Logger avec fallback robuste
// âœ… VÃ©rification mÃ©thodes logger avant utilisation
// ============================================================================

class Application {
    constructor() {
        // Ã‰tat de l'application
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
        
        // ModÃ¨les
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
        
        // ContrÃ´leurs
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
            backendUrl: 'ws://localhost:8080',
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
        console.log('ðŸš€ Initializing MidiMind v3.1...');
        
        try {
            // Ã‰tape 1: Fondations
            await this.initFoundations();
            
            // Ã‰tape 2: Services
            await this.initServices();
            
            // Ã‰tape 3: ModÃ¨les
            await this.initModels();
            
            // Ã‰tape 4: Vues
            await this.initViews();
            
            // Ã‰tape 5: ContrÃ´leurs
            await this.initControllers();
            
            // Ã‰tape 6: Navigation & Router
            await this.initNavigation();
            
            // Ã‰tape 7: Connexion backend (non-bloquant)
            this.connectBackend().catch(err => {
                this.log('warn', 'Backend connection failed, continuing in offline mode', err);
            });
            
            // Ã‰tape 8: Finalisation
            await this.finalize();
            
            this.state.initialized = true;
            this.state.ready = true;
            
            console.log('âœ… MidiMind v3.1 initialized successfully');
            this.log('info', 'âœ… Application ready');
            
            // Ã‰mettre Ã©vÃ©nement ready
            if (this.eventBus) {
                this.eventBus.emit('app:ready');
            }
            
        } catch (error) {
            console.error('âŒ Failed to initialize application:', error);
            this.log('error', 'Initialization failed', error);
            this.handleInitError(error);
        }
    }
    
    /**
     * Initialise les composants de base
     */
    async initFoundations() {
        console.log('ðŸ“¦ Initializing foundations...');
        
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
        
        this.log('info', 'âœ“ Foundations initialized');
    }
    
    /**
     * CrÃ©e un logger robuste avec fallback
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
        
        // Si logger existe dÃ©jÃ 
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
     * MÃ©thode helper pour logger de faÃ§on sÃ©curisÃ©e
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
        
        // FileService
        if (window.FileService) {
            this.services.file = new FileService(
                this.services.backend,
                this.eventBus,
                this.logger
            );
        }
        
        // Rendre services disponibles globalement
        window.backendService = this.services.backend;
        window.storageService = this.services.storage;
        
        this.log('info', 'âœ“ Services initialized');
    }
    
    /**
     * Initialise les modÃ¨les
     */
    async initModels() {
        console.log('ðŸ“Š Initializing models...');
        
        // StateModel
        if (window.StateModel) {
            this.models.state = new StateModel(this.eventBus);
        }
        
        // FileModel
        if (window.FileModel) {
            this.models.file = new FileModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // PlaylistModel
        if (window.PlaylistModel) {
            this.models.playlist = new PlaylistModel(this.eventBus);
        }
        
        // InstrumentModel
        if (window.InstrumentModel) {
            this.models.instrument = new InstrumentModel(this.eventBus);
        }
        
        // SystemModel
        if (window.SystemModel) {
            this.models.system = new SystemModel(
                this.eventBus,
                this.services.backend
            );
        }
        
        // PlaybackModel
        if (window.PlaybackModel) {
            this.models.playback = new PlaybackModel(this.eventBus);
        }
        
        // EditorModel
        if (window.EditorModel) {
            this.models.editor = new EditorModel(this.eventBus);
        }
        
        // RoutingModel
        if (window.RoutingModel) {
            this.models.routing = new RoutingModel(
                this.eventBus,
                this.services.backend
            );
        }
        
        this.log('info', 'âœ“ Models initialized');
    }
    
    /**
     * Initialise les vues
     */
    async initViews() {
        console.log('ðŸŽ¨ Initializing views...');
        
        // HomeView
        if (window.HomeView) {
            this.views.home = new HomeView('home-view', this.eventBus);
        }
        
        // FileView
        if (window.FileView) {
            this.views.file = new FileView('file-view', this.eventBus);
        }
        
        // InstrumentView
        if (window.InstrumentView) {
            this.views.instrument = new InstrumentView('instrument-view', this.eventBus);
        }
        
        // KeyboardView
        if (window.KeyboardView) {
            this.views.keyboard = new KeyboardView('keyboard-view', this.eventBus, this.debugConsole);
        }
        
        // SystemView
        if (window.SystemView) {
            this.views.system = new SystemView('system-view', this.eventBus);
        }
        
        // RoutingView
        if (window.RoutingView) {
            this.views.routing = new RoutingView('routing-view', this.eventBus);
        }
        
        // EditorView
        if (window.EditorView) {
            this.views.editor = new EditorView('editor-view', this.eventBus);
        }
        
        // PlaylistView
        if (window.PlaylistView) {
            this.views.playlist = new PlaylistView('playlist-view', this.eventBus);
        }
        
        // VisualizerView
        if (window.VisualizerView) {
            this.views.visualizer = new VisualizerView('visualizer-view', this.eventBus);
        }
        
        this.log('info', 'âœ“ Views initialized');
    }
    
    /**
     * Initialise les contrÃ´leurs
     */
    async initControllers() {
        console.log('ðŸŽ® Initializing controllers...');
        
        // NavigationController - ✅ CORRIGÉ
        if (window.NavigationController) {
            this.controllers.navigation = new NavigationController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.navigation.init?.();
        }
        
        // HomeController - âœ… CORRIGÃ‰
        if (window.HomeController) {
            this.controllers.home = new HomeController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.home.init?.();
        }
        
        // FileController - âœ… CORRIGÃ‰
        if (window.FileController) {
            this.controllers.file = new FileController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.file.init?.();
        }
        
        // InstrumentController - âœ… CORRIGÃ‰
        if (window.InstrumentController) {
            this.controllers.instrument = new InstrumentController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.instrument.init?.();
        }
        
        // KeyboardController - ✅ CORRIGÉ
        if (window.KeyboardController) {
            this.controllers.keyboard = new KeyboardController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.keyboard.init?.();
        }
        
        // SystemController - âœ… CORRIGÃ‰
        if (window.SystemController) {
            this.controllers.system = new SystemController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.system.init?.();
        }
        
        // PlaybackController - ✅ CORRIGÉ
        if (window.PlaybackController) {
            this.controllers.playback = new PlaybackController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.playback.init?.();
        }
        
        // GlobalPlaybackController - ✅ CORRIGÉ
        if (window.GlobalPlaybackController) {
            this.controllers.globalPlayback = new GlobalPlaybackController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.globalPlayback.init?.();
        }
        
        // PlaylistController - âœ… CORRIGÃ‰
        if (window.PlaylistController) {
            this.controllers.playlist = new PlaylistController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.playlist.init?.();
        }
        
        // EditorController - âœ… CORRIGÃ‰
        if (window.EditorController) {
            this.controllers.editor = new EditorController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.editor.init?.();
        }
        
        // RoutingController - âœ… CORRIGÃ‰
        if (window.RoutingController) {
            this.controllers.routing = new RoutingController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.routing.init?.();
        }
        
        // VisualizerController - ✅ CORRIGÉ
        if (window.VisualizerController) {
            this.controllers.visualizer = new VisualizerController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            this.controllers.visualizer.init?.();
        }
        
        this.log('info', 'âœ“ Controllers initialized');
    }
    
    /**
     * Initialise la navigation
     */
    async initNavigation() {
        console.log('ðŸ§­ Initializing navigation...');
        
        // Router
        if (window.Router) {
            this.router = new Router(this.eventBus);
            this.router.init?.();
        }
        
        // Configuration navigation
        this.setupNavigationEvents();
        
        // Page initiale
        const hash = window.location.hash.slice(1) || 'home';
        if (this.eventBus) {
            this.eventBus.emit('navigation:page_request', { page: hash });
        }
        
        this.log('info', 'âœ“ Navigation initialized');
    }
    
    /**
     * Configure les Ã©vÃ©nements de navigation
     */
    setupNavigationEvents() {
        if (!this.eventBus) return;
        
        // Navigation entre pages
        this.eventBus.on('navigation:change', (data) => {
            this.state.currentPage = data.page || 'home';
            this.log('debug', 'Page changed to:', data.page);
        });
        
        // Hash change
        window.addEventListener('hashchange', () => {
            const page = window.location.hash.slice(1) || 'home';
            if (this.eventBus) {
                this.eventBus.emit('navigation:page_request', { page });
            }
        });
    }
    
    /**
     * Connecte au backend
     */
    async connectBackend() {
        if (!this.services.backend) {
            this.log('warn', 'Backend service not available');
            return;
        }
        
        try {
            this.log('info', 'ðŸ”Œ Connecting to backend...');
            await this.services.backend.connect();
            this.state.backendConnected = true;
            this.log('info', 'âœ“ Backend connected');
        } catch (error) {
            this.log('warn', 'Backend connection failed:', error.message);
            this.state.backendConnected = false;
            this.state.offlineMode = true;
            
            if (this.config.offlineMode.showNotification && this.notifications) {
                this.notifications.show({
                    type: 'warning',
                    message: 'Mode hors ligne - FonctionnalitÃ©s limitÃ©es',
                    duration: 5000
                });
            }
        }
    }
    
    /**
     * Finalise l'initialisation
     */
    async finalize() {
        console.log('ðŸ Finalizing initialization...');
        
        // Afficher l'interface
        this.showInterface();
        
        // Raccourcis clavier globaux
        this.setupGlobalKeyboardShortcuts();
        
        // Performance monitoring
        if (window.PerformanceMonitor) {
            this.performanceMonitor = new PerformanceMonitor(this.eventBus);
        }
        
        this.log('info', 'âœ“ Initialization complete');
    }
    
    /**
     * Affiche l'interface
     */
    showInterface() {
        // Masquer l'indicateur de chargement
        const loadingIndicator = document.getElementById('loading-indicator');
        if (loadingIndicator) {
            loadingIndicator.style.display = 'none';
        }
        
        // Afficher l'application
        const appElement = document.getElementById('app');
        if (appElement) {
            appElement.style.display = 'flex';
        }
        
        this.log('info', 'âœ“ Interface displayed');
    }
    
    /**
     * Configure les raccourcis clavier globaux
     */
    setupGlobalKeyboardShortcuts() {
        if (!window.KeyboardShortcuts) return;
        
        try {
            const shortcuts = new KeyboardShortcuts(this.eventBus);
            shortcuts.init?.();
        } catch (e) {
            this.log('warn', 'Failed to initialize keyboard shortcuts:', e);
        }
    }
    
    /**
     * GÃ¨re les erreurs d'initialisation
     */
    handleInitError(error) {
        console.error('Initialization error:', error);
        
        // Afficher l'interface malgrÃ© l'erreur
        this.showInterface();
        
        // Notification
        if (this.notifications) {
            this.notifications.show({
                type: 'error',
                message: 'Erreur d\'initialisation - FonctionnalitÃ©s limitÃ©es',
                duration: 10000
            });
        }
        
        // Mode dÃ©gradÃ©
        this.state.offlineMode = true;
        console.warn('âš ï¸ Application initialization incomplete - showing interface anyway');
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * DÃ©truit l'application
     */
    destroy() {
        this.log('info', 'Destroying application...');
        
        // DÃ©truire contrÃ´leurs
        Object.values(this.controllers).forEach(controller => {
            if (controller && typeof controller.destroy === 'function') {
                controller.destroy();
            }
        });
        
        // DÃ©truire vues
        Object.values(this.views).forEach(view => {
            if (view && typeof view.destroy === 'function') {
                view.destroy();
            }
        });
        
        // DÃ©connecter backend
        if (this.services.backend) {
            this.services.backend.disconnect?.();
        }
        
        this.state.initialized = false;
        this.state.ready = false;
        
        this.log('info', 'âœ“ Application destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = Application;
}

if (typeof window !== 'undefined') {
    window.Application = Application;
}
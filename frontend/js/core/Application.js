// ============================================================================
// Fichier: frontend/js/core/Application.js
// Chemin réel: frontend/js/core/Application.js
// Version: v3.3.0 - INITIALISATION COHÉRENTE
// Date: 2025-10-31
// ============================================================================

class Application {
    constructor() {
        this.state = {
            initialized: false,
            ready: false,
            currentPage: 'home',
            backendConnected: false,
            offlineMode: false,
            reconnectAttempts: 0
        };
        
        this.eventBus = null;
        this.logger = null;
        this.debugConsole = null;
        this.notifications = null;
        
        this.services = {
            backend: null,
            storage: null,
            midi: null,
            file: null
        };
        
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
        
        window.app = this;
    }
    
    async init() {
        console.log('🚀 Initializing MidiMind v3.1...');
        
        try {
            await this.initFoundations();
            await this.initServices();
            await this.initModels();
            await this.initViews();
            await this.initControllers();
            await this.initNavigation();
            
            this.connectBackend().catch(err => {
                this.log('warn', 'Backend connection failed, continuing in offline mode', err);
            });
            
            await this.finalize();
            
            this.state.initialized = true;
            this.state.ready = true;
            
            console.log('✅ MidiMind v3.1 initialized successfully');
            this.log('info', '✅ Application ready');
            
            if (this.eventBus) {
                this.eventBus.emit('app:ready');
            }
            
            return true;
            
        } catch (error) {
            console.error('❌ Application initialization failed:', error);
            console.error('Stack trace:', error.stack);
            throw error;
        }
    }
    
    async initFoundations() {
        console.log('🔧 Initializing foundations...');
        
        // EventBus - déjà créé par EventBus.js
        this.eventBus = window.eventBus || new EventBus();
        window.eventBus = this.eventBus;
        
        // Logger
        this.logger = this.createLogger();
        window.logger = this.logger;
        
        // DebugConsole
        if (window.DebugConsole && this.config.enableDebugConsole) {
            this.debugConsole = new DebugConsole('debugConsole', this.eventBus);
            window.debugConsole = this.debugConsole;
        }
        
        // NotificationManager
        if (window.NotificationManager) {
            this.notifications = new NotificationManager(this.eventBus);
            window.notifications = this.notifications;
        }
        
        this.log('info', '✓ Foundations initialized');
    }
    
    createLogger() {
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
        
        if (window.logger && typeof window.logger.info === 'function') {
            return window.logger;
        }
        
        return {
            debug: (...args) => console.log('[DEBUG]', ...args),
            info: (...args) => console.log('[INFO]', ...args),
            warn: (...args) => console.warn('[WARN]', ...args),
            error: (...args) => console.error('[ERROR]', ...args),
            log: (...args) => console.log(...args)
        };
    }
    
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        } else {
            console[level]?.(...args) || console.log(...args);
        }
    }
    
    async initServices() {
        console.log('🔧 Initializing services...');
        
        if (window.BackendService) {
            this.services.backend = new BackendService(
                this.config.backendUrl,
                this.eventBus,
                this.logger
            );
        }
        
        if (window.StorageService) {
            this.services.storage = new StorageService(this.eventBus, this.logger);
        }
        
        if (window.MidiService) {
            this.services.midi = new MidiService(this.eventBus, this.logger);
        }
        
        if (window.FileService) {
            this.services.file = new FileService(
                this.services.backend,
                this.eventBus,
                this.logger
            );
        }
        
        window.backendService = this.services.backend;
        window.storageService = this.services.storage;
        
        this.log('info', '✓ Services initialized');
    }
    
    async initModels() {
        console.log('📦 Initializing models...');
        
        // ✅ CRITIQUE: TOUS les modèles avec signature cohérente (eventBus, backend, logger)
        
        if (window.StateModel) {
            this.models.state = new StateModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        if (window.FileModel) {
            this.models.file = new FileModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        if (window.PlaylistModel) {
            this.models.playlist = new PlaylistModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        if (window.InstrumentModel) {
            this.models.instrument = new InstrumentModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        if (window.SystemModel) {
            this.models.system = new SystemModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        if (window.PlaybackModel) {
            this.models.playback = new PlaybackModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        if (window.EditorModel) {
            this.models.editor = new EditorModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        if (window.RoutingModel) {
            this.models.routing = new RoutingModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        this.log('info', '✓ Models initialized');
    }
    
    async initViews() {
        console.log('🖥️ Initializing views...');
        
        if (window.HomeView) {
            this.views.home = new HomeView('home-view', this.eventBus);
        }
        
        if (window.FileView) {
            this.views.file = new FileView('file-view', this.eventBus);
        }
        
        if (window.InstrumentView) {
            this.views.instrument = new InstrumentView('instrument-view', this.eventBus);
        }
        
        if (window.KeyboardView) {
            this.views.keyboard = new KeyboardView('keyboard-view', this.eventBus, this.debugConsole);
        }
        
        if (window.SystemView) {
            this.views.system = new SystemView('system-view', this.eventBus);
        }
        
        if (window.RoutingView) {
            this.views.routing = new RoutingView('routing-view', this.eventBus);
        }
        
        if (window.EditorView) {
            this.views.editor = new EditorView('editor-view', this.eventBus);
        }
        
        if (window.PlaylistView) {
            this.views.playlist = new PlaylistView('playlist-view', this.eventBus);
        }
        
        if (window.VisualizerView) {
            this.views.visualizer = new VisualizerView('visualizer-view', this.eventBus);
        }
        
        this.log('info', '✓ Views initialized');
    }
    
    async initControllers() {
        console.log('🎮 Initializing controllers...');
        
        if (window.NavigationController) {
            this.controllers.navigation = new NavigationController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.HomeController) {
            this.controllers.home = new HomeController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.FileController) {
            this.controllers.file = new FileController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.InstrumentController) {
            this.controllers.instrument = new InstrumentController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.KeyboardController) {
            this.controllers.keyboard = new KeyboardController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.SystemController) {
            this.controllers.system = new SystemController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.PlaybackController) {
            this.controllers.playback = new PlaybackController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.GlobalPlaybackController) {
            this.controllers.globalPlayback = new GlobalPlaybackController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.PlaylistController) {
            this.controllers.playlist = new PlaylistController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.RoutingController) {
            this.controllers.routing = new RoutingController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.EditorController) {
            this.controllers.editor = new EditorController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        if (window.VisualizerController) {
            this.controllers.visualizer = new VisualizerController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        this.log('info', '✓ Controllers initialized');
    }
    
    async initNavigation() {
        console.log('🧭 Initializing navigation...');
        
        if (window.Router) {
            this.router = new Router(this.eventBus, this.controllers);
            window.router = this.router;
            await this.router.init();
        }
        
        this.log('info', '✓ Navigation initialized');
    }
    
    async connectBackend() {
        if (!this.services.backend) {
            throw new Error('BackendService not initialized');
        }
        
        try {
            await this.services.backend.connect();
            this.state.backendConnected = true;
            this.log('info', '✓ Backend connected');
        } catch (error) {
            this.log('warn', '⚠️ Backend connection failed:', error);
            this.state.offlineMode = true;
            
            if (this.config.offlineMode.showNotification && this.notifications) {
                this.notifications.warn('Mode hors-ligne', 'Backend non disponible');
            }
        }
    }
    
    async finalize() {
        this.setupGlobalHandlers();
        this.log('info', '✓ Application finalized');
    }
    
    setupGlobalHandlers() {
        window.addEventListener('error', (event) => {
            this.log('error', 'Global error:', event.error);
        });
        
        window.addEventListener('unhandledrejection', (event) => {
            this.log('error', 'Unhandled promise rejection:', event.reason);
        });
    }
    
    async shutdown() {
        this.log('info', 'Shutting down application...');
        
        if (this.services.backend) {
            await this.services.backend.disconnect();
        }
        
        for (const controller of Object.values(this.controllers)) {
            if (controller && typeof controller.destroy === 'function') {
                controller.destroy();
            }
        }
        
        this.state.initialized = false;
        this.state.ready = false;
        
        this.log('info', '✓ Application shutdown complete');
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = Application;
}

if (typeof window !== 'undefined') {
    window.Application = Application;
}
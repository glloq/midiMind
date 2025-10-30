// ============================================================================
// Fichier: frontend/js/core/Application.js
// Version: v3.9.1 - CORRECTIONS ENCODAGE + LOGGER
// Date: 2025-10-30
// Projet: MidiMind v3.1
// ============================================================================
// CORRECTIONS v3.9.1:
// ✅ Encodage UTF-8 correct (tous caractères spéciaux)
// ✅ Logger avec fallback robuste
// ✅ Vérification méthodes logger avant utilisation
// ============================================================================

class Application {
    constructor() {
        // État de l'application
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
        
        // Modèles
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
        
        // Contrôleurs
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
        
        // Référence globale
        window.app = this;
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * Initialise l'application complète
     */
    async init() {
        console.log('🚀 Initializing MidiMind v3.1...');
        
        try {
            // Étape 1: Fondations
            await this.initFoundations();
            
            // Étape 2: Services
            await this.initServices();
            
            // Étape 3: Modèles
            await this.initModels();
            
            // Étape 4: Vues
            await this.initViews();
            
            // Étape 5: Contrôleurs
            await this.initControllers();
            
            // Étape 6: Navigation & Router
            await this.initNavigation();
            
            // Étape 7: Connexion backend (non-bloquant)
            this.connectBackend().catch(err => {
                this.log('warn', 'Backend connection failed, continuing in offline mode', err);
            });
            
            // Étape 8: Finalisation
            await this.finalize();
            
            this.state.initialized = true;
            this.state.ready = true;
            
            console.log('✅ MidiMind v3.1 initialized successfully');
            this.log('info', '✅ Application ready');
            
            // Émettre événement ready
            if (this.eventBus) {
                this.eventBus.emit('app:ready');
            }
            
        } catch (error) {
            console.error('❌ Failed to initialize application:', error);
            this.log('error', 'Initialization failed', error);
            this.handleInitError(error);
        }
    }
    
    /**
     * Initialise les composants de base
     */
    async initFoundations() {
        console.log('📦 Initializing foundations...');
        
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
        
        this.log('info', '✓ Foundations initialized');
    }
    
    /**
     * Crée un logger robuste avec fallback
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
        
        // Si logger existe déjà
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
     * Méthode helper pour logger de façon sécurisée
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
        console.log('🔧 Initializing services...');
        
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
            this.services.file = new FileService(this.eventBus, this.logger);
        }
        
        this.log('info', '✓ Services initialized');
    }
    
    /**
     * Initialise les modèles
     */
    async initModels() {
        console.log('📊 Initializing models...');
        
        // StateModel
        if (window.StateModel) {
            this.models.state = new StateModel(this.eventBus);
        }
        
        // FileModel
        if (window.FileModel) {
            this.models.file = new FileModel(this.eventBus);
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
            this.models.system = new SystemModel(this.eventBus);
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
            this.models.routing = new RoutingModel(this.eventBus);
        }
        
        this.log('info', '✓ Models initialized');
    }
    
    /**
     * Initialise les vues
     */
    async initViews() {
        console.log('🎨 Initializing views...');
        
        // HomeView
        if (window.HomeView) {
            const homeContainer = document.getElementById('home');
            if (homeContainer) {
                this.views.home = new HomeView(homeContainer, this.eventBus);
                this.views.home.init?.();
            }
        }
        
        // FileView
        if (window.FileView) {
            const filesContainer = document.getElementById('files');
            if (filesContainer) {
                this.views.file = new FileView(filesContainer, this.eventBus);
                this.views.file.init?.();
            }
        }
        
        // InstrumentView
        if (window.InstrumentView) {
            const instrumentsContainer = document.getElementById('instruments');
            if (instrumentsContainer) {
                this.views.instrument = new InstrumentView(instrumentsContainer, this.eventBus);
                this.views.instrument.init?.();
            }
        }
        
        // KeyboardView
        if (window.KeyboardView) {
            const keyboardContainer = document.getElementById('keyboard');
            if (keyboardContainer) {
                this.views.keyboard = new KeyboardView(keyboardContainer, this.eventBus);
                this.views.keyboard.init?.();
            }
        }
        
        // SystemView
        if (window.SystemView) {
            const systemContainer = document.getElementById('system');
            if (systemContainer) {
                this.views.system = new SystemView(systemContainer, this.eventBus);
                this.views.system.init?.();
            }
        }
        
        // EditorView
        if (window.EditorView) {
            const editorContainer = document.getElementById('editor');
            if (editorContainer) {
                this.views.editor = new EditorView(editorContainer, this.eventBus);
                this.views.editor.init?.();
            }
        }
        
        // RoutingView
        if (window.RoutingView) {
            const routingContainer = document.getElementById('routing');
            if (routingContainer) {
                this.views.routing = new RoutingView(routingContainer, this.eventBus);
                this.views.routing.init?.();
            }
        }
        
        // PlaylistView
        if (window.PlaylistView) {
            this.views.playlist = new PlaylistView(this.eventBus);
            this.views.playlist.init?.();
        }
        
        // VisualizerView
        if (window.VisualizerView) {
            const visualizerCanvas = document.getElementById('visualizerCanvas');
            if (visualizerCanvas) {
                this.views.visualizer = new VisualizerView(visualizerCanvas, this.eventBus);
                this.views.visualizer.init?.();
            }
        }
        
        this.log('info', '✓ Views initialized');
    }
    
    /**
     * Initialise les contrôleurs
     */
    async initControllers() {
        console.log('🎮 Initializing controllers...');
        
        // NavigationController
        if (window.NavigationController) {
            this.controllers.navigation = new NavigationController(
                this.eventBus,
                this.models.state
            );
            this.controllers.navigation.init?.();
        }
        
        // HomeController
        if (window.HomeController) {
            this.controllers.home = new HomeController(
                this.views.home,
                this.models.file,
                this.eventBus
            );
            this.controllers.home.init?.();
        }
        
        // FileController
        if (window.FileController) {
            this.controllers.file = new FileController(
                this.views.file,
                this.models.file,
                this.eventBus
            );
            this.controllers.file.init?.();
        }
        
        // InstrumentController
        if (window.InstrumentController) {
            this.controllers.instrument = new InstrumentController(
                this.views.instrument,
                this.models.instrument,
                this.eventBus
            );
            this.controllers.instrument.init?.();
        }
        
        // KeyboardController
        if (window.KeyboardController) {
            this.controllers.keyboard = new KeyboardController(
                this.views.keyboard,
                this.eventBus
            );
            this.controllers.keyboard.init?.();
        }
        
        // SystemController
        if (window.SystemController) {
            this.controllers.system = new SystemController(
                this.views.system,
                this.models.system,
                this.eventBus
            );
            this.controllers.system.init?.();
        }
        
        // PlaybackController
        if (window.PlaybackController) {
            this.controllers.playback = new PlaybackController(
                this.models.playback,
                this.eventBus
            );
            this.controllers.playback.init?.();
        }
        
        // GlobalPlaybackController
        if (window.GlobalPlaybackController) {
            this.controllers.globalPlayback = new GlobalPlaybackController(
                this.eventBus,
                this.models.playback
            );
            this.controllers.globalPlayback.init?.();
        }
        
        // PlaylistController
        if (window.PlaylistController) {
            this.controllers.playlist = new PlaylistController(
                this.views.playlist,
                this.models.playlist,
                this.eventBus
            );
            this.controllers.playlist.init?.();
        }
        
        // EditorController
        if (window.EditorController) {
            this.controllers.editor = new EditorController(
                this.views.editor,
                this.models.editor,
                this.eventBus
            );
            this.controllers.editor.init?.();
        }
        
        // RoutingController
        if (window.RoutingController) {
            this.controllers.routing = new RoutingController(
                this.views.routing,
                this.models.routing,
                this.eventBus
            );
            this.controllers.routing.init?.();
        }
        
        // VisualizerController
        if (window.VisualizerController) {
            this.controllers.visualizer = new VisualizerController(
                this.views.visualizer,
                this.eventBus
            );
            this.controllers.visualizer.init?.();
        }
        
        this.log('info', '✓ Controllers initialized');
    }
    
    /**
     * Initialise la navigation
     */
    async initNavigation() {
        console.log('🧭 Initializing navigation...');
        
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
        
        this.log('info', '✓ Navigation initialized');
    }
    
    /**
     * Configure les événements de navigation
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
            this.log('info', '🔌 Connecting to backend...');
            await this.services.backend.connect();
            this.state.backendConnected = true;
            this.log('info', '✓ Backend connected');
        } catch (error) {
            this.log('warn', 'Backend connection failed:', error.message);
            this.state.backendConnected = false;
            this.state.offlineMode = true;
            
            if (this.config.offlineMode.showNotification && this.notifications) {
                this.notifications.show({
                    type: 'warning',
                    message: 'Mode hors ligne - Fonctionnalités limitées',
                    duration: 5000
                });
            }
        }
    }
    
    /**
     * Finalise l'initialisation
     */
    async finalize() {
        console.log('🏁 Finalizing initialization...');
        
        // Afficher l'interface
        this.showInterface();
        
        // Raccourcis clavier globaux
        this.setupGlobalKeyboardShortcuts();
        
        // Performance monitoring
        if (window.PerformanceMonitor) {
            this.performanceMonitor = new PerformanceMonitor(this.eventBus);
        }
        
        this.log('info', '✓ Initialization complete');
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
        
        this.log('info', '✓ Interface displayed');
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
     * Gère les erreurs d'initialisation
     */
    handleInitError(error) {
        console.error('Initialization error:', error);
        
        // Afficher l'interface malgré l'erreur
        this.showInterface();
        
        // Notification
        if (this.notifications) {
            this.notifications.show({
                type: 'error',
                message: 'Erreur d\'initialisation - Fonctionnalités limitées',
                duration: 10000
            });
        }
        
        // Mode dégradé
        this.state.offlineMode = true;
        console.warn('⚠️ Application initialization incomplete - showing interface anyway');
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Détruit l'application
     */
    destroy() {
        this.log('info', 'Destroying application...');
        
        // Détruire contrôleurs
        Object.values(this.controllers).forEach(controller => {
            if (controller && typeof controller.destroy === 'function') {
                controller.destroy();
            }
        });
        
        // Détruire vues
        Object.values(this.views).forEach(view => {
            if (view && typeof view.destroy === 'function') {
                view.destroy();
            }
        });
        
        // Déconnecter backend
        if (this.services.backend) {
            this.services.backend.disconnect?.();
        }
        
        this.state.initialized = false;
        this.state.ready = false;
        
        this.log('info', '✓ Application destroyed');
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
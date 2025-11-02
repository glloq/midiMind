// ============================================================================
// Fichier: frontend/js/core/Application.js
// Version: v4.0.0 - FIX NAVIGATION & ROUTER
// Date: 2025-11-02
// Projet: MidiMind v3.1
// ============================================================================
// CORRECTIONS v4.0.0:
// ✅ CRITIQUE: Initialisation correcte de NavigationController AVANT Router
// ✅ CRITIQUE: Connexion Router <-> NavigationController simplifiée
// ✅ CRITIQUE: StartRouting() appelé APRÈS l'enregistrement des routes
// ✅ Fix: Toutes les pages fonctionnent correctement
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
        this.router = null;
        
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
        console.log('🔧 Initializing foundations...');
        
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
        
        this.log('info', '✅ Foundations initialized');
    }
    
    /**
     * Créée un logger robuste avec fallback
     */
    createLogger() {
        if (window.Logger) {
            try {
                return new Logger(this.config.logLevel);
            } catch (e) {
                console.warn('Logger initialization failed, using fallback');
            }
        }
        
        // Fallback logger
        return {
            log: (...args) => console.log(...args),
            info: (...args) => console.info(...args),
            warn: (...args) => console.warn(...args),
            error: (...args) => console.error(...args),
            debug: (...args) => console.debug(...args)
        };
    }
    
    /**
     * Log helper
     */
    log(level, ...args) {
        if (this.logger) {
            this.logger[level](...args);
        } else {
            console[level](...args);
        }
    }
    
    /**
     * Initialise les services
     */
    async initServices() {
        console.log('🔌 Initializing services...');
        
        // BackendService
        if (window.BackendService) {
            this.services.backend = new BackendService(
                this.config.backendUrl,
                this.eventBus,
                this.logger
            );
            window.backendService = this.services.backend;
        }
        
        // StorageService
        if (window.StorageService) {
            this.services.storage = new StorageService(this.eventBus);
            window.storageService = this.services.storage;
        }
        
        // MidiService
        if (window.MidiService) {
            this.services.midi = new MidiService(this.eventBus, this.logger);
            window.midiService = this.services.midi;
        }
        
        // FileService
        if (window.FileService) {
            this.services.file = new FileService(this.eventBus, this.logger);
            window.fileService = this.services.file;
        }
        
        this.log('info', '✅ Services initialized');
    }
    
    /**
     * Initialise les modèles
     */
    async initModels() {
        console.log('📦 Initializing models...');
        
        // StateModel
        if (window.StateModel) {
            this.models.state = new StateModel(this.eventBus);
            window.stateModel = this.models.state;
        }
        
        // FileModel
        if (window.FileModel) {
            this.models.file = new FileModel(this.eventBus);
            window.fileModel = this.models.file;
        }
        
        // PlaylistModel
        if (window.PlaylistModel) {
            this.models.playlist = new PlaylistModel(this.eventBus);
            window.playlistModel = this.models.playlist;
        }
        
        // InstrumentModel
        if (window.InstrumentModel) {
            this.models.instrument = new InstrumentModel(this.eventBus);
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
            this.models.routing = new RoutingModel(this.eventBus);
            window.routingModel = this.models.routing;
        }
        
        this.log('info', '✅ Models initialized');
    }
    
    /**
     * Initialise les vues
     */
    async initViews() {
        console.log('🎨 Initializing views...');
        
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
            this.views.instrument = new InstrumentView('instruments-view', this.eventBus);
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
        
        this.log('info', '✅ Views initialized');
    }
    
    /**
     * Initialise les contrôleurs
     */
    async initControllers() {
        console.log('🎮 Initializing controllers...');
        
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
        
        this.log('info', '✅ Controllers initialized');
    }
    
    /**
     * Initialise la navigation
     */
    async initNavigation() {
        console.log('🧭 Initializing navigation...');
        
        // NavigationController DOIT être initialisé en premier
        if (window.NavigationController) {
            this.controllers.navigation = new NavigationController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
        }
        
        // Router ensuite
        if (window.Router) {
            this.router = new Router({
                mode: 'hash',
                useTransitions: false,  // Désactivé car NavigationController gère les transitions
                transitionDuration: 300
            });
            
            // Enregistrer les routes AVANT de démarrer le routing
            this.registerRoutes();
            
            // Connecter le Router au NavigationController
            if (this.controllers.navigation) {
                this.router.on('route-changed', (data) => {
                    // Extraire le nom de la page du path
                    let pageKey = data.path.replace(/^\//, '') || 'home';
                    // Mapper files -> files (le Router utilise /files, NavigationController utilise 'files')
                    this.controllers.navigation.showPage(pageKey);
                });
            }
            
            // Démarrer le routing maintenant
            this.router.startRouting();
            this.log('info', '✅ Routing started');
        }
        
        this.log('info', '✅ Navigation initialized');
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
            title: 'MidiMind - Éditeur',
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
            title: 'MidiMind - Système',
            view: 'system'
        });
        
        this.router.route('/visualizer', {
            title: 'MidiMind - Visualiseur',
            view: 'visualizer'
        });
        
        // Route par défaut
        this.router.route('/', {
            title: 'MidiMind - Accueil',
            view: 'home'
        });
        
        // Route 404
        this.router.notFound({
            title: 'MidiMind - Page non trouvée',
            view: 'home'
        });
        
        this.log('info', '✅ Routes registered');
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
                this.log('info', '✅ Backend connected');
                
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
        console.log('🏁 Finalizing initialization...');
        
        // Événements d'erreur globaux
        this.setupErrorHandlers();
        
        // Événements de connexion
        this.setupConnectionHandlers();
        
        // Raccourcis clavier
        if (window.KeyboardShortcuts) {
            this.keyboardShortcuts = new KeyboardShortcuts(this.eventBus, this.logger);
        }
        
        this.log('info', '✅ Finalization complete');
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
        
        // Backend connecté
        this.eventBus.on('backend:connected', (data) => {
            this.state.backendConnected = true;
            this.state.offlineMode = false;
            this.state.reconnectAttempts = 0;
            
            this.log('info', '✅ Backend connected');
            
            if (this.notifications) {
                this.notifications.show('Backend connected', 'success', 3000);
            }
        });
        
        // Backend déconnecté
        this.eventBus.on('backend:disconnected', (data) => {
            this.state.backendConnected = false;
            
            this.log('warn', '⚠️ Backend disconnected');
            
            if (this.notifications && !this.state.offlineMode) {
                this.notifications.show('Backend disconnected', 'warning', 5000);
            }
        });
        
        // Mode offline
        this.eventBus.on('backend:offline-mode', (data) => {
            this.state.offlineMode = true;
            this.state.backendConnected = false;
            
            this.log('warn', '⚠️ Offline mode activated');
            
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
     * Gère les erreurs d'initialisation
     */
    handleInitError(error) {
        console.error('❌ Initialization error:', error);
        
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
            <h3 style="margin: 0 0 10px 0;">❌ Initialization Failed</h3>
            <p style="margin: 0;">${error.message}</p>
            <button onclick="location.reload()" 
                    style="margin-top: 15px; padding: 8px 16px; border: none; background: white; color: #dc3545; cursor: pointer; border-radius: 4px;">
                Reload Application
            </button>
        `;
        document.body.appendChild(errorDiv);
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES
    // ========================================================================
    
    /**
     * Obtient l'état de l'application
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
     * Détruit l'application
     */
    destroy() {
        this.log('info', 'Destroying application...');
        
        // Détruire les contrôleurs
        Object.values(this.controllers).forEach(controller => {
            if (controller && typeof controller.destroy === 'function') {
                controller.destroy();
            }
        });
        
        // Détruire les vues
        Object.values(this.views).forEach(view => {
            if (view && typeof view.destroy === 'function') {
                view.destroy();
            }
        });
        
        // Déconnecter le backend
        if (this.services.backend && typeof this.services.backend.disconnect === 'function') {
            this.services.backend.disconnect();
        }
        
        this.state.initialized = false;
        this.state.ready = false;
        
        this.log('info', '✅ Application destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = Application;
}

window.Application = Application;
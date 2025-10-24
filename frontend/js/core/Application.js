// ============================================================================
// Fichier: frontend/js/core/Application.js
// Version: v3.8 - FIXED EVENT HANDLING
// Date: 2025-10-24
// Projet: midiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// CORRECTIONS v3.8:
// ✅ Événements backend cohérents (backend:disconnected au lieu de websocket:)
// ✅ Gestion améliorée des notifications de connexion
// ✅ Meilleure gestion du mode offline
// ============================================================================

class Application {
    constructor() {
        // État de l'application
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
        
        // Modèles
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
        
        // Contrôleurs
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
            backendUrl: window.MIDIMIND_BACKEND_URL || `ws://${window.location.hostname}:8080`,
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
        console.log('🚀 Initializing midiMind v3.0...');
        
        try {
            // Étape 1: Fondations
            await this.initFoundations();
            
            // Étape 2: Services
            await this.initServices();
            
            // Étape 3: Modèles
            await this.initModels();
            
            // Étape 4: Vues (CRITIQUE)
            await this.initViews();
            
            // Étape 5: Contrôleurs
            await this.initControllers();
            
            // Étape 6: Navigation
            await this.initNavigation();
            
            // Étape 7: Connexion backend (non-bloquant)
            await this.connectBackend();
            
            // Étape 8: Finalisation
            await this.finalize();
            
            this.state.initialized = true;
            this.state.ready = true;
            
            console.log('✅ midiMind initialized successfully');
            this.logger.info('Application', '✅ Application ready');
            
            // Émettre événement ready
            this.eventBus.emit('app:ready');
            
        } catch (error) {
            console.error('❌ Failed to initialize application:', error);
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
        console.log('📦 Initializing foundations...');
        
        // EventBus
        this.eventBus = window.eventBus || new EventBus();
        window.eventBus = this.eventBus;
        
        // Logger
        if (window.Logger && typeof window.Logger === 'function') {
            this.logger = window.logger || new Logger({
                level: this.config.logLevel,
                eventBus: this.eventBus
            });
            window.logger = this.logger;
        } else {
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
        
        this.logger.info('Application', '✓ Foundations initialized');
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
            this.services.file = new FileService(
                this.services.backend,
                this.eventBus,
                this.logger
            );
        }
        
        this.logger.info('Application', '✓ Services initialized');
    }
    
    /**
     * Initialise les modèles de données
     */
    async initModels() {
        console.log('📊 Initializing models...');
        
        const backend = this.services.backend;
        const eventBus = this.eventBus;
        const logger = this.logger;
        
        // FileModel
        if (window.FileModel) {
            this.models.file = new FileModel(eventBus, backend, logger);
        }
        
        // PlaylistModel
        if (window.PlaylistModel) {
            this.models.playlist = new PlaylistModel(eventBus, backend, logger);
        }
        
        // InstrumentModel
        if (window.InstrumentModel) {
            this.models.instrument = new InstrumentModel(eventBus, backend, logger);
        }
        
        // SystemModel
        if (window.SystemModel) {
            this.models.system = new SystemModel(eventBus, backend, logger);
        }
        
        // PlaybackModel
        if (window.PlaybackModel) {
            this.models.playback = new PlaybackModel(eventBus, backend, logger);
        }
        
        // EditorModel
        if (window.EditorModel) {
            this.models.editor = new EditorModel(eventBus, backend, logger);
        }
        
        // RoutingModel
        if (window.RoutingModel) {
            this.models.routing = new RoutingModel(eventBus, backend, logger);
        }
        
        this.logger.info('Application', '✓ Models initialized');
    }
    
    /**
     * Initialise les vues
     */
    async initViews() {
        console.log('🎨 Initializing views...');
        
        const eventBus = this.eventBus;
        const logger = this.logger;
        
        // HomeView
        if (window.HomeView) {
            this.views.home = new HomeView('home', eventBus);
            if (this.views.home && typeof this.views.home.init === 'function') {
                this.views.home.init();
            }
        }
        
        // FileView - Skip: no container in HTML
        // if (window.FileView) {
        //     this.views.file = new FileView('file-manager', eventBus);
        // }
        
        // InstrumentView
        if (window.InstrumentView) {
            this.views.instrument = new InstrumentView('instruments', eventBus);
        }
        
        // KeyboardView
        if (window.KeyboardView) {
            this.views.keyboard = new KeyboardView('keyboard', eventBus);
        }
        
        // SystemView
        if (window.SystemView) {
            this.views.system = new SystemView('system', eventBus);
        }
        
        // EditorView
        if (window.EditorView) {
            this.views.editor = new EditorView('editor', eventBus);
        }
        
        // RoutingView
        if (window.RoutingView) {
            this.views.routing = new RoutingView('routing', eventBus);
        }
        
        this.logger.info('Application', '✓ Views initialized');
    }
    
    /**
     * Initialise les contrôleurs
     */
    async initControllers() {
        console.log('🎮 Initializing controllers...');
        
        const eventBus = this.eventBus;
        const logger = this.logger;
        
        // NavigationController
        if (window.NavigationController) {
            this.controllers.navigation = new NavigationController(eventBus, this.models, this.views, this.notifications, this.debugConsole);
        }
        
        // FileController
        if (window.FileController) {
            this.controllers.file = new FileController(
                eventBus,
                this.models.file,
                this.views.file,
                this.notifications,
                this.debugConsole
            );
        }
        
        // PlaylistController
        if (window.PlaylistController) {
            this.controllers.playlist = new PlaylistController(
                eventBus,
                this.models.playlist,
                this.services.backend,
                logger
            );
        }
        
        // InstrumentController
        if (window.InstrumentController) {
            this.controllers.instrument = new InstrumentController(
                eventBus,
                this.models.instrument,
                this.views.instrument,
                this.services.backend,
                logger
            );
        }
        
        // PlaybackController
        if (window.PlaybackController) {
            this.controllers.playback = new PlaybackController(
                eventBus,
                this.models.playback,
                this.services.backend,
                logger
            );
        }
        
        // GlobalPlaybackController
        if (window.GlobalPlaybackController) {
            this.controllers.globalPlayback = new GlobalPlaybackController(
                eventBus,
                this.models.playback,
                this.services.backend,
                logger
            );
        }
        
        // SystemController
        if (window.SystemController) {
            this.controllers.system = new SystemController(
                eventBus,
                this.models.system,
                this.views.system,
                this.services.backend,
                logger
            );
        }
        
        // SearchController
        if (window.SearchController) {
            this.controllers.search = new SearchController(
                eventBus,
                this.models.file,
                logger
            );
        }
        
        // RoutingController
        if (window.RoutingController) {
            this.controllers.routing = new RoutingController(
                eventBus,
                this.models.routing,
                this.services.backend,
                logger
            );
        }
        
        // EditorController
        if (window.EditorController) {
            this.controllers.editor = new EditorController(
                eventBus,
                this.models.editor,
                this.views.editor,
                this.services.backend,
                logger
            );
        }
        
        // HomeController
        if (window.HomeController) {
            this.controllers.home = new HomeController(
                eventBus,
                this.models.file,
                this.views.home,
                logger
            );
        }
        
        this.logger.info('Application', '✓ Controllers initialized');
    }
    
    /**
     * Initialise la navigation
     */
    async initNavigation() {
        console.log('🧭 Initializing navigation...');
        
        // Gérer le hash pour la navigation
        window.addEventListener('hashchange', () => {
            this.handleNavigation();
        });
        
        // Navigation initiale
        this.handleNavigation();
        
        this.logger.info('Application', '✓ Navigation initialized');
    }
    
    /**
     * Se connecte au backend
     */
    async connectBackend() {
        if (!this.services.backend) {
            this.logger.warn('Application', 'BackendService not available');
            this.enableOfflineMode('Backend service not available');
            return false;
        }
        
        this.logger.info('Application', 'Connecting to backend...');
        
        try {
            const connected = await this.services.backend.connect();
            
            if (connected) {
                this.logger.info('Application', '✅ Backend connected successfully');
                this.state.backendConnected = true;
                return true;
            } else {
                this.logger.warn('Application', '⚠️ Backend connection failed, continuing in offline mode');
                this.enableOfflineMode('Backend connection failed');
                return false;
            }
        } catch (error) {
            this.logger.error('Application', 'Backend connection error:', error);
            this.enableOfflineMode('Backend connection error');
            return false;
        }
    }
    
    /**
     * Active le mode offline
     */
    enableOfflineMode(reason = 'Unknown') {
        this.state.offlineMode = true;
        this.state.backendConnected = false;
        
        this.logger.warn('Application', `📴 Offline mode enabled: ${reason}`);
        
        if (this.config.offlineMode.showNotification && this.notifications) {
            this.notifications.show(
                'Running in offline mode - some features unavailable',
                'warning',
                { duration: 5000 }
            );
        }
        
        this.eventBus.emit('app:offline-mode', { reason });
    }
    
    /**
     * Finalise l'initialisation
     */
    async finalize() {
        console.log('🏁 Finalizing initialization...');
        
        // Attacher les gestionnaires d'erreurs
        this.attachErrorHandlers();
        
        // Attacher les événements de l'application
        this.attachAppEvents();
        
        // Afficher l'interface
        this.showInterface();
        
        // Afficher statut de connexion
        this.showConnectionStatus(this.state.backendConnected);
        
        this.logger.info('Application', '✓ Initialization finalized');
    }
    
    /**
     * Affiche le statut de connexion
     */
    showConnectionStatus(connected) {
        const statusElement = document.getElementById('connection-status');
        if (!statusElement) return;
        
        if (connected) {
            statusElement.textContent = '🟢 Connected';
            statusElement.className = 'status-connected';
        } else {
            statusElement.textContent = '🔴 Offline';
            statusElement.className = 'status-offline';
        }
    }
    
    /**
     * Gère la navigation entre les pages
     */
    handleNavigation() {
        const hash = window.location.hash.slice(1) || 'home';
        const [page, ...rest] = hash.split('/');
        
        this.logger.debug('Application', `Navigating to: ${page}`);
        
        const validPages = ['home', 'editor', 'routing', 'keyboard', 'instruments', 'system'];
        
        if (validPages.includes(page)) {
            this.hideAllPages();
            
            const pageElement = document.getElementById(page);
            if (pageElement) {
                pageElement.style.display = 'block';
                this.state.currentPage = page;
            }
            
            this.eventBus.emit('navigation:changed', { page, hash });
            
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
        switch (page) {
            case 'home':
                if (this.controllers.home && typeof this.controllers.home.init === 'function') {
                    this.controllers.home.init();
                } else if (this.views.home && typeof this.views.home.render === 'function') {
                    this.views.home.render();
                }
                break;
            case 'system':
                if (this.controllers.system && typeof this.controllers.system.init === 'function') {
                    this.controllers.system.init();
                } else if (this.views.system && typeof this.views.system.render === 'function') {
                    this.views.system.render();
                }
                break;
            case 'editor':
                if (this.controllers.editor && typeof this.controllers.editor.init === 'function') {
                    this.controllers.editor.init();
                } else if (this.views.editor && typeof this.views.editor.render === 'function') {
                    this.views.editor.render();
                }
                break;
        }
    }
    
    // ========================================================================
    // GESTION DES ERREURS
    // ========================================================================
    
    attachErrorHandlers() {
        window.addEventListener('error', (event) => {
            const error = event.error || event.message || 'Unknown error';
            
            // Ignorer erreur ResizeObserver
            const errorMsg = typeof error === 'string' ? error : error.message;
            if (errorMsg && errorMsg.includes('ResizeObserver')) {
                return;
            }
            
            if (this.logger && this.logger.error) {
                this.logger.error('Application', 'Uncaught error:', error);
            }
            this.handleError(error);
        });
        
        window.addEventListener('unhandledrejection', (event) => {
            const reason = event.reason || 'Unknown rejection';
            if (this.logger && this.logger.error) {
                this.logger.error('Application', 'Unhandled rejection:', reason);
            }
            this.handleError(reason);
        });
    }
    
    handleError(error) {
        if (!error) {
            error = { message: 'Unknown error', stack: '' };
        } else if (typeof error === 'string') {
            error = { message: error, stack: '' };
        }
        
        if (this.notifications && this.notifications.show) {
            this.notifications.show(
                `Error: ${error.message || error.toString()}`,
                'error'
            );
        }
        
        if (this.logger && this.logger.error) {
            this.logger.error('Application', 'Error occurred:', {
                message: error.message || error.toString(),
                stack: error.stack || ''
            });
        }
        
        this.eventBus.emit('app:error', { error });
    }
    
    handleInitError(error) {
        if (this.logger && this.logger.error) {
            this.logger.error('Application', 'Initialization error:', error);
        }
        
        if (this.notifications && this.notifications.show) {
            this.notifications.show(
                `Failed to initialize: ${error.message}`,
                'error',
                { duration: 0, closable: true }
            );
        }
        
        this.state.initialized = false;
        this.state.ready = false;
    }
    
    // ========================================================================
    // ÉVÉNEMENTS APPLICATION
    // ========================================================================
    
    attachAppEvents() {
        // ✅ CORRIGÉ: Écouter backend:connected (cohérent avec BackendService)
        this.eventBus.on('backend:connected', (data) => {
            this.logger.info('Application', '✅ Backend connected');
            this.state.backendConnected = true;
            this.state.offlineMode = false;
            this.showConnectionStatus(true);
            
            if (this.notifications) {
                this.notifications.show('Backend connected', 'success', { duration: 3000 });
            }
        });
        
        // ✅ CORRIGÉ: Écouter backend:disconnected (cohérent avec BackendService)
        this.eventBus.on('backend:disconnected', (data) => {
            this.logger.warn('Application', '🔴 Backend disconnected', data);
            this.state.backendConnected = false;
            this.showConnectionStatus(false);
            
            if (!this.state.offlineMode) {
                if (this.notifications) {
                    this.notifications.show(
                        'Connection lost - attempting to reconnect...',
                        'warning',
                        { duration: 5000 }
                    );
                }
            }
        });
        
        // ✅ NOUVEAU: Écouter max reconnect attempts
        this.eventBus.on('backend:max-reconnect-attempts', (data) => {
            this.logger.error('Application', '❌ Max reconnection attempts reached');
            this.enableOfflineMode('Max reconnection attempts reached');
            
            if (this.notifications) {
                this.notifications.show(
                    'Backend unreachable - running in offline mode',
                    'error',
                    { duration: 0, closable: true }
                );
            }
        });
        
        // Écouter les erreurs backend
        this.eventBus.on('backend:connection-failed', (data) => {
            this.logger.error('Application', 'Backend connection failed:', data);
        });
        
        // ✅ NOUVEAU: Écouter reconnect scheduled
        this.eventBus.on('backend:reconnect-scheduled', (data) => {
            this.logger.info('Application', 
                `Reconnection scheduled (attempt ${data.attempt}/${data.maxAttempts})`);
        });
    }
    
    // ========================================================================
    // INTERFACE UTILISATEUR
    // ========================================================================
    
    showInterface() {
        const loading = document.getElementById('loading-indicator');
        if (loading) {
            loading.style.display = 'none';
        }
        
        const app = document.getElementById('app');
        if (app) {
            app.style.display = 'block';
        }
        
        const homePage = document.getElementById('home');
        if (homePage) {
            homePage.style.display = 'block';
        }
    }
    
    // ========================================================================
    // API PUBLIQUE
    // ========================================================================
    
    getState() {
        return { ...this.state };
    }
    
    isBackendConnected() {
        return this.state.backendConnected && this.services.backend?.isConnected();
    }
    
    isOfflineMode() {
        return this.state.offlineMode;
    }
    
    getService(serviceName) {
        const serviceMap = {
            'backend': this.services.backend,
            'file': this.services.file,
            'midi': this.services.midi,
            'storage': this.services.storage
        };
        
        return serviceMap[serviceName] || null;
    }
}

window.Application = Application;
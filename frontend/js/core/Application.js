// ============================================================================
// Fichier: frontend/js/core/Application.js
// Version: v3.5 - FIXED LOGGER INITIALIZATION
// Date: 2025-10-22
// Projet: midiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// CORRECTIONS v3.5:
// ✅ Logger correctement initialisé avec new Logger()
// ✅ Conteneurs corrects pour toutes les vues
// ✅ Initialisation forcée de toutes les vues
// ✅ Interface visible même sans backend
// ✅ Mode offline gracieux
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
        
        // EventBus (déjà instancié globalement ou créer nouveau)
        this.eventBus = window.eventBus || new EventBus();
        window.eventBus = this.eventBus;
        
        // Logger - CORRIGÉ : créer une nouvelle instance
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
            this.services.storage = new StorageService();
        }
        
        // MidiService
        if (window.MidiService) {
            this.services.midi = new MidiService(this.eventBus);
        }
        
        // FileService
        if (window.FileService) {
            this.services.file = new FileService();
        }
        
        this.logger.info('Application', '✓ Services initialized');
    }
    
    /**
     * Initialise les modèles de données
     */
    async initModels() {
        console.log('📊 Initializing models...');
        
        // FileModel
        if (window.FileModel) {
            this.models.file = new FileModel(this.eventBus, this.logger);
        }
        
        // PlaylistModel
        if (window.PlaylistModel) {
            this.models.playlist = new PlaylistModel(this.eventBus, this.logger);
        }
        
        // InstrumentModel
        if (window.InstrumentModel) {
            this.models.instrument = new InstrumentModel(this.eventBus, this.logger);
        }
        
        // SystemModel
        if (window.SystemModel) {
            this.models.system = new SystemModel(this.eventBus, this.logger);
        }
        
        // PlaybackModel
        if (window.PlaybackModel) {
            this.models.playback = new PlaybackModel(this.eventBus, this.logger);
        }
        
        // EditorModel
        if (window.EditorModel) {
            this.models.editor = new EditorModel(this.eventBus, this.logger);
        }
        
        // RoutingModel
        if (window.RoutingModel) {
            this.models.routing = new RoutingModel(this.eventBus, this.logger);
        }
        
        this.logger.info('Application', '✓ Models initialized');
    }
    
    /**
     * Initialise les vues - CORRIGÉ
     */
    async initViews() {
        console.log('🎨 Initializing views...');
        
        // HomeView - Conteneur 'home'
        const homeElement = document.getElementById('home');
        if (homeElement && window.HomeView) {
            this.views.home = new HomeView(homeElement, this.eventBus);
            // Initialiser la vue
            if (typeof this.views.home.init === 'function') {
                this.views.home.init();
            } else if (typeof this.views.home.render === 'function') {
                this.views.home.render();
            }
            console.log('✓ HomeView initialized');
        }
        
        // EditorView - Conteneur 'editor'
        const editorElement = document.getElementById('editor');
        if (editorElement && window.EditorView) {
            this.views.editor = new EditorView(editorElement, this.eventBus, this.logger);
            // La vue s'initialise elle-même via BaseView
            console.log('✓ EditorView initialized');
        }
        
        // RoutingView - Conteneur 'routing'
        const routingElement = document.getElementById('routing');
        if (routingElement && window.RoutingView) {
            this.views.routing = new RoutingView(routingElement, this.eventBus, this.logger);
            // La vue s'initialise elle-même via BaseView
            console.log('✓ RoutingView initialized');
        }
        
        // KeyboardView - Conteneur 'keyboard'
        const keyboardElement = document.getElementById('keyboard');
        if (keyboardElement && window.KeyboardView) {
            this.views.keyboard = new KeyboardView(keyboardElement, this.eventBus);
            // Initialiser la vue
            if (typeof this.views.keyboard.init === 'function') {
                this.views.keyboard.init();
            } else if (typeof this.views.keyboard.render === 'function') {
                this.views.keyboard.render();
            }
            console.log('✓ KeyboardView initialized');
        }
        
        // InstrumentView - Conteneur 'instruments'
        const instrumentElement = document.getElementById('instruments');
        if (instrumentElement && window.InstrumentView) {
            this.views.instrument = new InstrumentView(instrumentElement, this.eventBus, this.logger);
            // La vue s'initialise elle-même via BaseView
            console.log('✓ InstrumentView initialized');
        }
        
        // SystemView - Conteneur 'system'
        const systemElement = document.getElementById('system');
        if (systemElement && window.SystemView) {
            this.views.system = new SystemView(systemElement, this.eventBus);
            // Forcer le rendu initial
            if (typeof this.views.system.render === 'function') {
                this.views.system.render({
                    systemHealth: 'good',
                    cpu: { usage: 0, cores: 4 },
                    memory: { used: 0, total: 100 },
                    latency: { current: 0, target: 10 }
                });
            }
            console.log('✓ SystemView initialized');
        }
        
        // FileView (si élément existe)
        const fileElement = document.querySelector('.file-view-container');
        if (fileElement && window.FileView) {
            this.views.file = new FileView(fileElement, this.eventBus);
            console.log('✓ FileView initialized');
        }
        
        this.logger.info('Application', '✓ Views initialized');
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
                this.services.backend,
                this.models.instrument,
                this.views.instrument,
                this.eventBus,
                this.notifications,
                this.debugConsole
            );
        }
        
        // SystemController
        if (window.SystemController) {
            this.controllers.system = new SystemController(
                this.services.backend,
                this.models.system,
                this.views.system,
                this.eventBus,
                this.notifications,
                this.debugConsole
            );
        }
        
        // PlaybackController
        if (window.PlaybackController) {
            this.controllers.playback = new PlaybackController(
                this.services.backend,
                this.models.playback,
                this.eventBus,
                this.notifications,
                this.debugConsole
            );
        }
        
        // RoutingController
        if (window.RoutingController) {
            this.controllers.routing = new RoutingController(
                this.services.backend,
                this.models.routing,
                this.views.routing,
                this.eventBus,
                this.notifications,
                this.debugConsole
            );
        }
        
        // EditorController
        if (window.EditorController) {
            this.controllers.editor = new EditorController(
                this.models.editor,
                this.views.editor,
                this.eventBus,
                this.notifications,
                this.debugConsole
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
        
        this.logger.info('Application', '✓ Controllers initialized');
    }
    
    /**
     * Initialise le système de navigation
     */
    async initNavigation() {
        console.log('🗺️ Initializing navigation...');
        
        // Écouter les changements de hash
        window.addEventListener('hashchange', () => {
            this.handleNavigation();
        });
        
        // Navigation initiale
        this.handleNavigation();
        
        this.logger.info('Application', '✓ Navigation initialized');
    }
    
    // ========================================================================
    // CONNEXION BACKEND - AVEC MODE OFFLINE
    // ========================================================================
    
    /**
     * Connexion au backend avec gestion du mode offline
     */
    async connectBackend() {
        console.log('🔌 Connecting to backend...');
        
        if (!this.services.backend) {
            this.logger.warn('Application', 'BackendService not available');
            this.enableOfflineMode('BackendService not available');
            return;
        }
        
        try {
            // Tenter connexion avec retry automatique
            await this.services.backend.connect();
            
            // Succès !
            this.state.backendConnected = true;
            this.state.offlineMode = false;
            
            this.logger.info('Application', '✅ Backend connected');
            
            // Afficher notification de connexion
            this.showConnectionStatus(true);
            
            // Émettre événement pour init différée des contrôleurs
            this.eventBus.emit('backend:connected');
            
        } catch (error) {
            // Échec après toutes les tentatives
            this.logger.error('Application', 'Backend connection failed:', error);
            
            // Activer mode offline
            this.enableOfflineMode(error.message);
            
            // Planifier reconnexion si autoReconnect activé
            if (this.config.autoReconnect) {
                this.scheduleReconnect();
            }
        }
    }
    
    /**
     * Active le mode offline
     * @param {string} reason - Raison du mode offline
     */
    enableOfflineMode(reason) {
        this.state.offlineMode = true;
        this.state.backendConnected = false;
        
        this.logger.warn('Application', `🔴 Offline mode activated: ${reason}`);
        
        // Afficher notification à l'utilisateur
        if (this.config.offlineMode.showNotification) {
            this.showOfflineNotification(reason);
        }
        
        // Afficher indicateur visuel
        this.showConnectionStatus(false);
        
        // Émettre événement
        this.eventBus.emit('app:offline-mode', { reason });
    }
    
    /**
     * Affiche une notification de mode offline
     * @param {string} reason - Raison
     */
    showOfflineNotification(reason) {
        if (this.notifications && this.notifications.show) {
            this.notifications.show(
                `Mode Offline: ${reason}. Les fonctionnalités locales restent disponibles.`,
                'warning',
                { duration: 5000, closable: true }
            );
        }
    }
    
    /**
     * Affiche le statut de connexion
     * @param {boolean} connected - État de connexion
     */
    showConnectionStatus(connected) {
        const indicator = document.getElementById('connection-status');
        if (!indicator) return;
        
        if (connected) {
            indicator.style.backgroundColor = '#10b981';
            indicator.style.color = 'white';
            indicator.style.display = 'flex';
            indicator.style.opacity = '1';
            indicator.innerHTML = `
                <span style="width: 8px; height: 8px; background: white; border-radius: 50%; display: inline-block;"></span>
                Online
            `;
            
            // Masquer après 3 secondes
            setTimeout(() => {
                indicator.style.opacity = '0';
                setTimeout(() => indicator.style.display = 'none', 300);
            }, 3000);
            
        } else {
            indicator.style.backgroundColor = '#ef4444';
            indicator.style.color = 'white';
            indicator.style.display = 'flex';
            indicator.style.opacity = '1';
            indicator.innerHTML = `
                <span style="width: 8px; height: 8px; background: white; border-radius: 50%; display: inline-block;"></span>
                Offline
            `;
        }
    }
    
    /**
     * Planifie une reconnexion
     */
    scheduleReconnect() {
        this.logger.info('Application', `Scheduling reconnect in ${this.config.reconnectInterval}ms`);
        
        setTimeout(async () => {
            if (!this.state.backendConnected) {
                this.logger.info('Application', 'Attempting reconnection...');
                await this.connectBackend();
            }
        }, this.config.reconnectInterval);
    }
    
    /**
     * Finalisation de l'initialisation
     */
    async finalize() {
        console.log('🎯 Finalizing initialization...');
        
        // Attacher gestionnaires d'erreurs globaux
        this.attachErrorHandlers();
        
        // Attacher événements application
        this.attachAppEvents();
        
        // Rendre l'interface visible
        this.showInterface();
        
        this.logger.info('Application', '✓ Initialization finalized');
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
        
        // Afficher la page demandée
        const pageElement = document.getElementById(page);
        if (pageElement) {
            pageElement.style.display = 'block';
            this.state.currentPage = page;
            
            // Émettre événement de navigation
            this.eventBus.emit('navigation:changed', { page, hash });
            
            // Initialiser le contrôleur de la page si nécessaire
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
        // Initialiser les contrôleurs spécifiques aux pages
        switch (page) {
            case 'home':
                if (this.controllers.home && typeof this.controllers.home.init === 'function') {
                    this.controllers.home.init();
                }
                break;
            case 'editor':
                if (this.controllers.editor && typeof this.controllers.editor.init === 'function') {
                    this.controllers.editor.init();
                }
                break;
            // Autres pages...
        }
    }
    
    // ========================================================================
    // GESTION DES ERREURS
    // ========================================================================
    
    attachErrorHandlers() {
        // Erreurs JavaScript non capturées
        window.addEventListener('error', (event) => {
            if (this.logger && this.logger.error) {
                this.logger.error('Application', 'Uncaught error:', event.error);
            }
            this.handleError(event.error);
        });
        
        // Promesses rejetées non capturées
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
        
        // Logger détaillé
        if (this.logger && this.logger.error) {
            this.logger.error('Application', 'Error occurred:', {
                message: error.message,
                stack: error.stack
            });
        }
        
        // Émettre événement erreur
        this.eventBus.emit('app:error', { error });
    }
    
    handleInitError(error) {
        if (this.logger && this.logger.error) {
            this.logger.error('Application', 'Initialization error:', error);
        }
        
        // Afficher erreur à l'utilisateur
        if (this.notifications && this.notifications.show) {
            this.notifications.show(
                `Failed to initialize: ${error.message}`,
                'error',
                { duration: 0, closable: true }
            );
        }
        
        // Essayer de continuer en mode dégradé
        this.state.initialized = false;
        this.state.ready = false;
    }
    
    // ========================================================================
    // ÉVÉNEMENTS APPLICATION
    // ========================================================================
    
    attachAppEvents() {
        // Écouter la connexion backend
        this.eventBus.on('backend:connected', () => {
            this.logger.info('Application', '✅ Backend connected event received');
            this.state.backendConnected = true;
            this.state.offlineMode = false;
            this.showConnectionStatus(true);
        });
        
        // Écouter la déconnexion backend
        this.eventBus.on('websocket:disconnected', () => {
            this.logger.warn('Application', '🔴 Backend disconnected');
            this.state.backendConnected = false;
            
            if (!this.state.offlineMode) {
                this.enableOfflineMode('Connection lost');
            }
        });
        
        // Écouter les erreurs backend
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
        
        // S'assurer que la page home est visible au démarrage
        const homePage = document.getElementById('home');
        if (homePage) {
            homePage.style.display = 'block';
        }
    }
    
    // ========================================================================
    // API PUBLIQUE
    // ========================================================================
    
    /**
     * Obtient l'état de l'application
     * @returns {Object}
     */
    getState() {
        return { ...this.state };
    }
    
    /**
     * Vérifie si le backend est connecté
     * @returns {boolean}
     */
    isBackendConnected() {
        return this.state.backendConnected && this.services.backend?.isConnected();
    }
    
    /**
     * Vérifie si en mode offline
     * @returns {boolean}
     */
    isOfflineMode() {
        return this.state.offlineMode;
    }
}

// ============================================================================
// FIN DU FICHIER Application.js v3.5
// ============================================================================
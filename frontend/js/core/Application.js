// ============================================================================
// Fichier: frontend/js/core/Application.js
// Version: v3.8 - ENRICHI AVEC NOUVELLES FONCTIONNALITÉS API
// Date: 2025-10-28
// Projet: midiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// NOUVELLES FONCTIONNALITÉS v3.8:
// ✅ BluetoothController - Gestion périphériques Bluetooth
// ✅ LatencyController - Calibration et compensation latence
// ✅ PresetController - Gestion presets de configuration
// ✅ LoggerController - Configuration niveau de logs
// ✅ NetworkMonitor - Monitoring réseau
// ✅ Intégration EventBus enrichi (bluetooth, hotplug, latency, preset, logger, network)
// ✅ NotificationManager enrichi avec messages API
// ✅ Router enrichi avec configureApiRoutes()
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
            // ✅ NOUVELLES VUES v3.8
            bluetooth: null,
            latency: null,
            preset: null,
            network: null,
            logger: null
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
            home: null,
            // ✅ NOUVEAUX CONTRÔLEURS v3.8
            bluetooth: null,
            latency: null,
            preset: null,
            logger: null,
            network: null
        };
        
        // Configuration
        this.config = {
            backendUrl: 'ws://localhost:8080',
            autoReconnect: true,
            reconnectInterval: 5000,
            maxReconnectAttempts: 10,
            logLevel: 'debug',
            enableDebugConsole: true,
            offlineMode: {
                enabled: true,
                showNotification: true,
                allowLocalOperations: true
            },
            // ✅ NOUVELLES CONFIGURATIONS v3.8
            features: {
                bluetooth: true,
                latency: true,
                presets: true,
                network: true,
                logger: true
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
        console.log('🚀 Initializing midiMind v3.8...');
        
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
            await this.connectBackend();
            
            // Étape 8: Finalisation
            await this.finalize();
            
            this.state.initialized = true;
            this.state.ready = true;
            
            console.log('✅ midiMind v3.8 initialized successfully');
            this.logger.info('Application', '✅ Application ready with new API features');
            
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
        
        // NotificationManager
        if (window.NotificationManager && typeof window.NotificationManager === 'function') {
            this.notifications = new NotificationManager();
            window.notificationManager = this.notifications;
        } else if (window.Notifications && typeof window.Notifications === 'function') {
            this.notifications = new Notifications(this.eventBus);
            window.notifications = this.notifications;
        }
        
        this.logger.info('Application', '✔ Foundations initialized');
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
        
        // FileService - CORRIGÉ : passer les paramètres requis
        if (window.FileService) {
            this.services.file = new FileService(
                this.services.backend,
                this.eventBus,
                this.logger
            );
        }
        
        this.logger.info('Application', '✔ Services initialized');
    }
    
    /**
     * Initialise les modèles de données
     */
    async initModels() {
        console.log('📊 Initializing models...');
        
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
            this.models.playlist = new PlaylistModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // InstrumentModel
        if (window.InstrumentModel) {
            this.models.instrument = new InstrumentModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // SystemModel
        if (window.SystemModel) {
            this.models.system = new SystemModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // PlaybackModel
        if (window.PlaybackModel) {
            this.models.playback = new PlaybackModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // EditorModel
        if (window.EditorModel) {
            this.models.editor = new EditorModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        // RoutingModel
        if (window.RoutingModel) {
            this.models.routing = new RoutingModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
        }
        
        this.logger.info('Application', '✔ Models initialized');
    }
    
    /**
     * Initialise les vues
     */
    async initViews() {
        console.log('🎨 Initializing views...');
        
        // HomeView
        if (window.HomeView) {
            this.views.home = new HomeView('home', this.eventBus);
            if (typeof this.views.home.init === 'function') {
                this.views.home.init();
            }
            console.log('✔ HomeView initialized');
        }
        
        // EditorView
        const editorElement = document.getElementById('editor');
        if (editorElement && window.EditorView) {
            this.views.editor = new EditorView(editorElement, this.eventBus, this.logger);
            console.log('✔ EditorView initialized');
        }
        
        // RoutingView
        const routingElement = document.getElementById('routing');
        if (routingElement && window.RoutingView) {
            this.views.routing = new RoutingView(routingElement, this.eventBus, this.logger);
            console.log('✔ RoutingView initialized');
        }
        
        // KeyboardView
        const keyboardElement = document.getElementById('keyboard');
        if (keyboardElement && window.KeyboardView) {
            this.views.keyboard = new KeyboardView(keyboardElement, this.eventBus);
            if (typeof this.views.keyboard.init === 'function') {
                this.views.keyboard.init();
            }
            console.log('✔ KeyboardView initialized');
        }
        
        // InstrumentView
        const instrumentElement = document.getElementById('instruments');
        if (instrumentElement && window.InstrumentView) {
            this.views.instrument = new InstrumentView(instrumentElement, this.eventBus);
            if (typeof this.views.instrument.init === 'function') {
                this.views.instrument.init();
            }
            console.log('✔ InstrumentView initialized');
        }
        
        // SystemView
        const systemElement = document.getElementById('system');
        if (systemElement && window.SystemView) {
            this.views.system = new SystemView(systemElement, this.eventBus);
            if (typeof this.views.system.init === 'function') {
                this.views.system.init();
            }
            console.log('✔ SystemView initialized');
        }
        
        // FileView
        const fileElement = document.querySelector('.files-list');
        if (fileElement && window.FileView) {
            this.views.file = new FileView(fileElement, this.eventBus);
            console.log('✔ FileView initialized');
        }
        
        // ✅ NOUVELLES VUES v3.8
        this.initApiViews();
        
        this.logger.info('Application', '✔ Views initialized');
    }
    
    /**
     * ✅ NOUVELLE MÉTHODE v3.8: Initialise les vues API
     */
    initApiViews() {
        // BluetoothView
        if (this.config.features.bluetooth && window.BluetoothView) {
            const bluetoothElement = document.getElementById('bluetooth');
            if (bluetoothElement) {
                this.views.bluetooth = new BluetoothView(bluetoothElement, this.eventBus, this.logger);
                console.log('✔ BluetoothView initialized');
            }
        }
        
        // LatencyView
        if (this.config.features.latency && window.LatencyView) {
            const latencyElement = document.getElementById('latency');
            if (latencyElement) {
                this.views.latency = new LatencyView(latencyElement, this.eventBus, this.logger);
                console.log('✔ LatencyView initialized');
            }
        }
        
        // PresetView
        if (this.config.features.presets && window.PresetView) {
            const presetElement = document.getElementById('presets');
            if (presetElement) {
                this.views.preset = new PresetView(presetElement, this.eventBus, this.logger);
                console.log('✔ PresetView initialized');
            }
        }
        
        // NetworkMonitor (Vue de monitoring réseau)
        if (this.config.features.network && window.NetworkMonitor) {
            const networkElement = document.getElementById('network');
            if (networkElement) {
                this.views.network = new NetworkMonitor(networkElement, this.eventBus, this.logger);
                console.log('✔ NetworkMonitor initialized');
            }
        }
        
        // LoggerView
        if (this.config.features.logger && window.LoggerView) {
            const loggerElement = document.getElementById('logger');
            if (loggerElement) {
                this.views.logger = new LoggerView(loggerElement, this.eventBus, this.logger);
                console.log('✔ LoggerView initialized');
            }
        }
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
        
        // ✅ NOUVEAUX CONTRÔLEURS v3.8
        this.initApiControllers();
        
        this.logger.info('Application', '✔ Controllers initialized');
    }
    
    /**
     * ✅ NOUVELLE MÉTHODE v3.8: Initialise les contrôleurs API
     */
    initApiControllers() {
        // BluetoothController
        if (this.config.features.bluetooth && window.BluetoothController) {
            this.controllers.bluetooth = new BluetoothController(
                this.eventBus,
                this.services.backend,
                this.views.bluetooth,
                this.notifications,
                this.logger
            );
            console.log('✔ BluetoothController initialized');
        }
        
        // LatencyController
        if (this.config.features.latency && window.LatencyController) {
            this.controllers.latency = new LatencyController(
                this.eventBus,
                this.services.backend,
                this.views.latency,
                this.notifications,
                this.logger
            );
            console.log('✔ LatencyController initialized');
        }
        
        // PresetController
        if (this.config.features.presets && window.PresetController) {
            this.controllers.preset = new PresetController(
                this.eventBus,
                this.services.backend,
                this.views.preset,
                this.notifications,
                this.logger
            );
            console.log('✔ PresetController initialized');
        }
        
        // LoggerController
        if (this.config.features.logger && window.LoggerController) {
            this.controllers.logger = new LoggerController(
                this.eventBus,
                this.services.backend,
                this.views.logger,
                this.notifications,
                this.logger
            );
            console.log('✔ LoggerController initialized');
        }
        
        // NetworkMonitor (Controller)
        if (this.config.features.network && window.NetworkMonitorController) {
            this.controllers.network = new NetworkMonitorController(
                this.eventBus,
                this.services.backend,
                this.views.network,
                this.notifications,
                this.logger
            );
            console.log('✔ NetworkMonitorController initialized');
        }
    }
    
    /**
     * Initialise la navigation
     */
    async initNavigation() {
        console.log('🧭 Initializing navigation...');
        
        // Gestionnaire de navigation hashchange
        window.addEventListener('hashchange', () => this.handleNavigation());
        
        // ✅ NOUVEAU v3.8: Configuration Router (si disponible)
        if (window.Router && typeof window.Router === 'function') {
            this.router = new Router({
                mode: 'hash',
                useTransitions: true
            });
            
            // Configurer les routes API
            if (typeof this.router.configureApiRoutes === 'function') {
                this.router.configureApiRoutes(this.controllers, this.views);
                console.log('✔ API routes configured');
            }
            
            window.router = this.router;
        }
        
        // Navigation initiale
        this.handleNavigation();
        
        this.logger.info('Application', '✔ Navigation initialized');
    }
    
    /**
     * Connexion au backend (non-bloquant)
     */
    async connectBackend() {
        console.log('🔌 Connecting to backend...');
        
        if (!this.services.backend) {
            console.warn('⚠️ Backend service not available');
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
                this.logger.info('Application', '✅ Backend connected');
                this.showConnectionStatus(true);
            } else {
                throw new Error('Connection timeout');
            }
            
        } catch (error) {
            console.warn('⚠️ Backend connection failed:', error.message);
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
            
            // Écouter l'événement de connexion
            this.eventBus.once('backend:connected', () => {
                clearTimeout(timeoutId);
                resolve(true);
            });
            
            // Timeout
            timeoutId = setTimeout(() => {
                resolve(false);
            }, timeout);
            
            // Vérification immédiate
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
            statusElement.innerHTML = connected 
                ? `<span class="status-dot online"></span> Online` 
                : `<span class="status-dot offline"></span> Offline`;
        }
    }
    
    /**
     * Planifie une reconnexion
     */
    scheduleReconnect() {
        // Limiter les tentatives de reconnexion
        if (this.state.reconnectAttempts >= this.config.maxReconnectAttempts) {
            this.logger.warn('Application', 'Max reconnection attempts reached. Staying in offline mode.');
            return;
        }
        
        this.state.reconnectAttempts++;
        this.logger.info('Application', `Scheduling reconnect in ${this.config.reconnectInterval}ms (attempt ${this.state.reconnectAttempts}/${this.config.maxReconnectAttempts})`);
        
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
        
        this.logger.info('Application', '✔ Initialization finalized');
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
        const pages = ['home', 'editor', 'routing', 'keyboard', 'instruments', 'system', 
                      'bluetooth', 'latency', 'presets', 'network', 'logger'];
        pages.forEach(page => {
            const element = document.getElementById(page);
            if (element) {
                element.style.display = 'none';
            }
        });
    }
    
    initPageController(page) {
        // Initialiser les contrôleurs et vues spécifiques aux pages
        switch (page) {
            case 'home':
                if (this.controllers.home && typeof this.controllers.home.init === 'function') {
                    this.controllers.home.init();
                }
                break;
            case 'system':
                if (this.controllers.system && typeof this.controllers.system.init === 'function') {
                    this.controllers.system.init();
                }
                break;
            case 'editor':
                if (this.controllers.editor && typeof this.controllers.editor.init === 'function') {
                    this.controllers.editor.init();
                }
                break;
            // ✅ NOUVELLES PAGES v3.8
            case 'bluetooth':
                if (this.controllers.bluetooth && typeof this.controllers.bluetooth.init === 'function') {
                    this.controllers.bluetooth.init();
                }
                break;
            case 'latency':
                if (this.controllers.latency && typeof this.controllers.latency.init === 'function') {
                    this.controllers.latency.init();
                }
                break;
            case 'presets':
                if (this.controllers.preset && typeof this.controllers.preset.init === 'function') {
                    this.controllers.preset.init();
                }
                break;
            case 'network':
                if (this.controllers.network && typeof this.controllers.network.init === 'function') {
                    this.controllers.network.init();
                }
                break;
            case 'logger':
                if (this.controllers.logger && typeof this.controllers.logger.init === 'function') {
                    this.controllers.logger.init();
                }
                break;
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
    
    /**
     * Récupère un service par son nom
     * @param {string} serviceName - Nom du service
     * @returns {Object|null}
     */
    getService(serviceName) {
        return this.services[serviceName] || null;
    }
}

window.Application = Application;

// ============================================================================
// FIN DU FICHIER Application.js v3.8
// ============================================================================
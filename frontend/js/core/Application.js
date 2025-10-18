// ============================================================================
// Fichier: frontend/js/core/Application.js
// Version: v3.2 - COMPLETE
// Date: 2025-10-08
// Projet: midiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// Description:
//   Classe principale de l'application - Chef d'orchestre
//   Initialise et coordonne tous les composants (models, views, controllers)
//
// Responsabilités:
//   - Initialisation de tous les composants dans le bon ordre
//   - Gestion du cycle de vie de l'application
//   - Coordination des services
//   - Gestion de la navigation
//   - Gestion des erreurs globales
// ============================================================================

class Application {
    constructor() {
        // État de l'application
        this.state = {
            initialized: false,
            ready: false,
            currentPage: 'home',
            backendConnected: false
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
            backendUrl: 'ws://localhost:8765',
            autoReconnect: true,
            reconnectInterval: 5000,
            logLevel: 'debug',
            enableDebugConsole: true
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
            
            // Étape 4: Vues
            await this.initViews();
            
            // Étape 5: Contrôleurs
            await this.initControllers();
            
            // Étape 6: Navigation
            await this.initNavigation();
            
            // Étape 7: Connexion backend
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
            this.logger.error('Application', 'Initialization failed:', error);
            this.handleInitError(error);
        }
    }
    
    /**
     * Initialise les composants de base
     */
    async initFoundations() {
        console.log('📦 Initializing foundations...');
        
        // EventBus (déjà instancié globalement)
        this.eventBus = window.eventBus || new EventBus();
        window.eventBus = this.eventBus;
        
        // Logger
        this.logger = window.logger || new Logger({
            level: this.config.logLevel,
            eventBus: this.eventBus
        });
        window.logger = this.logger;
        
        // DebugConsole
        if (this.config.enableDebugConsole) {
            this.debugConsole = new DebugConsole(this.eventBus, this.logger);
            window.debugConsole = this.debugConsole;
        }
        
        // Notifications
        this.notifications = new Notifications(this.eventBus);
        window.notifications = this.notifications;
        
        this.logger.info('Application', '✓ Foundations initialized');
    }
    
    /**
     * Initialise les services
     */
    async initServices() {
        console.log('🔧 Initializing services...');
        
        // BackendService
        this.services.backend = new BackendService(
            this.eventBus,
            this.logger
        );
        window.backendService = this.services.backend;
        
        // StorageService
        this.services.storage = new StorageService(
            this.eventBus,
            this.logger
        );
        window.storageService = this.services.storage;
        
        // MidiService
        if (typeof MidiService !== 'undefined') {
            this.services.midi = new MidiService(
                this.eventBus,
                this.logger
            );
            window.midiService = this.services.midi;
        }
        
        // FileService
        if (typeof FileService !== 'undefined') {
            this.services.file = new FileService(
                this.services.backend,
                this.eventBus,
                this.logger
            );
            window.fileService = this.services.file;
        }
        
		// Créer ApiClient
		this.apiClient = new ApiClient({
			baseURL: this.config.api?.baseURL || 'http://localhost:8080/api',
			timeout: this.config.api?.timeout || 10000,
			retryAttempts: 3,
			cache: {
				enabled: true,
				ttl: 300000, // 5 minutes
				maxSize: 100
			},
			debug: this.config.debug || false
		});

		// Ajouter intercepteur d'authentification si nécessaire
		this.apiClient.addRequestInterceptor((config) => {
			const token = localStorage.getItem('auth_token');
			if (token) {
				config.headers['Authorization'] = `Bearer ${token}`;
			}
			return config;
		});

		// Rendre accessible globalement
		window.apiClient = this.apiClient;

        this.logger.info('Application', '✓ Services initialized');
    }
    
    /**
     * Initialise les modèles
     */
    async initModels() {
        console.log('📊 Initializing models...');
        
        // FileModel
        this.models.file = new FileModel(
            this.eventBus,
            this.services.backend,
            this.logger
        );
        window.fileModel = this.models.file;
        
        // PlaylistModel
        if (typeof PlaylistModel !== 'undefined') {
            this.models.playlist = new PlaylistModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
            window.playlistModel = this.models.playlist;
        }
        
        // InstrumentModel
        if (typeof InstrumentModel !== 'undefined') {
            this.models.instrument = new InstrumentModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
            window.instrumentModel = this.models.instrument;
        }
        
        // SystemModel
        if (typeof SystemModel !== 'undefined') {
            this.models.system = new SystemModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
            window.systemModel = this.models.system;
        }
        
        // PlaybackModel
        if (typeof PlaybackModel !== 'undefined') {
            this.models.playback = new PlaybackModel(
                this.eventBus,
                this.services.backend,
                this.logger
            );
            window.playbackModel = this.models.playback;
        }
        
        // EditorModel
        this.models.editor = new EditorModel(
            this.eventBus,
            this.services.backend,
            this.logger
        );
        window.editorModel = this.models.editor;
        
        // RoutingModel
        this.models.routing = new RoutingModel(
            this.eventBus,
            this.services.backend,
            this.logger
        );
        window.routingModel = this.models.routing;
        
        this.logger.info('Application', '✓ Models initialized');
    }
    
    /**
     * Initialise les vues
     */
    async initViews() {
        console.log('🎨 Initializing views...');
        
        // HomeView
        const homeContainer = document.getElementById('home');
        if (homeContainer && typeof HomeView !== 'undefined') {
            this.views.home = new HomeView('home', this.eventBus);
            window.homeView = this.views.home;
        }
        
        // FileView
        const fileContainer = document.getElementById('file-panel');
        if (fileContainer && typeof FileView !== 'undefined') {
            this.views.file = new FileView('file-panel', this.eventBus);
            window.fileView = this.views.file;
        }
        
        // InstrumentView
        const instrumentContainer = document.getElementById('instruments');
        if (instrumentContainer && typeof InstrumentView !== 'undefined') {
            this.views.instrument = new InstrumentView('instruments', this.eventBus);
            window.instrumentView = this.views.instrument;
        }
        
        // SystemView
        const systemContainer = document.getElementById('system');
        if (systemContainer && typeof SystemView !== 'undefined') {
            this.views.system = new SystemView('system', this.eventBus);
            window.systemView = this.views.system;
        }
        
        // RoutingView
        const routingContainer = document.getElementById('routing');
        if (routingContainer && typeof RoutingView !== 'undefined') {
            this.views.routing = new RoutingView('routing', this.eventBus);
            window.routingView = this.views.routing;
        }
        
        // EditorView
         const editorContainer = document.getElementById('editor');
		if (editorContainer && typeof EditorView !== 'undefined') {
			this.views.editor = new EditorView('editor', this.eventBus);
			window.editorView = this.views.editor;
			console.log('✅ EditorView v3.6.0 initialized');
		} else {
			console.error('❌ EditorView: container or class not found');
			if (!editorContainer) console.error('  - Container #editor missing');
			if (typeof EditorView === 'undefined') console.error('  - EditorView class not loaded');
		}
        
        this.logger.info('Application', '✓ Views initialized');
    }
    
    /**
     * Initialise les contrôleurs
     */
    async initControllers() {
        console.log('🎮 Initializing controllers...');
        
        // GlobalPlaybackController (CRITIQUE - doit être premier)
        if (typeof GlobalPlaybackController !== 'undefined') {
            this.controllers.globalPlayback = new GlobalPlaybackController(
                this.eventBus,
                this.services.backend,
                this.logger
            );
            window.globalPlaybackController = this.controllers.globalPlayback;
        }
        
        // NavigationController
        if (typeof NavigationController !== 'undefined') {
            this.controllers.navigation = new NavigationController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            window.navigationController = this.controllers.navigation;
        }
        
        // FileController
        this.controllers.file = new FileController(
            this.eventBus,
            this.models,
            this.views,
            this.notifications,
            this.debugConsole
        );
        window.fileController = this.controllers.file;
        
        // PlaybackController (utilise GlobalPlaybackController)
        if (typeof PlaybackController !== 'undefined') {
            this.controllers.playback = new PlaybackController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            window.playbackController = this.controllers.playback;
        }
        
        // EditorController
		if (typeof EditorController !== 'undefined') {
			this.controllers.editor = new EditorController(
				this.eventBus,
				this.models,
				this.views,
				this.notifications,
				this.debugConsole
			);
			window.editorController = this.controllers.editor;
			
			// CRITIQUE: Injecter le modèle dans la vue
			if (this.views.editor && this.models.editor) {
				this.views.editor.setModel(this.models.editor);
				console.log('✅ EditorModel injected into EditorView');
			}
			
			console.log('✅ EditorController initialized');
		} else {
			console.error('❌ EditorController not loaded');
		}
        // RoutingController
        if (typeof RoutingController !== 'undefined') {
            this.controllers.routing = new RoutingController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            window.routingController = this.controllers.routing;
        }
        
        // HomeController
        if (typeof HomeController !== 'undefined') {
            this.controllers.home = new HomeController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            window.homeController = this.controllers.home;
        }
        
        // Autres contrôleurs...
        if (typeof InstrumentController !== 'undefined') {
            this.controllers.instrument = new InstrumentController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            window.instrumentController = this.controllers.instrument;
        }
        
        if (typeof SystemController !== 'undefined') {
            this.controllers.system = new SystemController(
                this.eventBus,
                this.models,
                this.views,
                this.notifications,
                this.debugConsole
            );
            window.systemController = this.controllers.system;
        }
        
        this.logger.info('Application', '✓ Controllers initialized');
    }
    
    /**
     * Initialise la navigation
     */
    async initNavigation() {
        console.log('🧭 Initializing navigation...');
        
        // Gérer les changements de hash
        window.addEventListener('hashchange', () => {
            this.handleNavigation();
        });
        
        // Navigation initiale
        this.handleNavigation();
        
        this.logger.info('Application', '✓ Navigation initialized');
    }
    
    /**
     * Connexion au backend
     */
    async connectBackend() {
        console.log('🔌 Connecting to backend...');
        
        if (!this.services.backend) {
            this.logger.warn('Application', 'BackendService not available');
            return;
        }
        
        try {
            await this.services.backend.connect();
            this.state.backendConnected = true;
            this.logger.info('Application', '✓ Backend connected');
            
        } catch (error) {
            this.logger.error('Application', 'Backend connection failed:', error);
            
            // Continuer même si connexion échoue
            if (this.config.autoReconnect) {
                this.scheduleReconnect();
            }
        }
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
        const pages = ['home', 'editor', 'routing', 'system', 'instruments'];
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
            this.logger.error('Application', 'Uncaught error:', event.error);
            this.handleError(event.error);
        });
        
        // Promesses rejetées non capturées
        window.addEventListener('unhandledrejection', (event) => {
            this.logger.error('Application', 'Unhandled rejection:', event.reason);
            this.handleError(event.reason);
        });
    }
    
    handleError(error) {
        // Afficher notification
        if (this.notifications) {
            this.notifications.show(
                `Error: ${error.message || error}`,
                'error'
            );
        }
        
        // Logger détaillé
        this.logger.error('Application', 'Error occurred:', {
            message: error.message,
            stack: error.stack
        });
        
        // Émettre événement erreur
        this.eventBus.emit('app:error', { error });
    }
    
/**
 * Gère les erreurs d'initialisation
 * CORRECTION v3.0.2: Plus robuste
 */
handleInitError(error) {
    this.logger.error('Application', 'Initialization error:', error);
    
    // Afficher erreur à l'utilisateur
    const errorDiv = document.createElement('div');
    errorDiv.id = 'init-error';
    errorDiv.className = 'init-error';
    errorDiv.innerHTML = `
        <div class="error-content">
            <h2>âŒ Erreur d'initialisation</h2>
            <p class="error-message">${error.message}</p>
            
            <div class="error-details">
                <h3>Détails techniques:</h3>
                <pre>${error.stack || 'No stack trace'}</pre>
            </div>
            
            <div class="error-actions">
                <button onclick="location.reload()" class="btn-reload">
                    🔄 Recharger l'application
                </button>
                <button onclick="document.getElementById('init-error').querySelector('.error-details').style.display = 
                    document.getElementById('init-error').querySelector('.error-details').style.display === 'none' ? 'block' : 'none'"
                    class="btn-details">
                    ℹ️ Afficher/Masquer détails
                </button>
            </div>
            
            <div class="error-help">
                <h3>Que faire ?</h3>
                <ul>
                    <li>Vérifiez que le backend est démarré</li>
                    <li>Vérifiez la console navigateur (F12)</li>
                    <li>Essayez de recharger la page</li>
                    <li>Contactez le support si le problème persiste</li>
                </ul>
            </div>
        </div>
    `;
    
    // Masquer détails par défaut
    errorDiv.querySelector('.error-details').style.display = 'none';
    
    // Remplacer loader par erreur
    const loader = document.getElementById('loader');
    if (loader) {
        loader.style.display = 'none';
    }
    
    document.body.appendChild(errorDiv);
    
    // Log dans console également
    console.error('🚨 Initialization failed:', error);
}
    
    // ========================================================================
    // ÉVÉNEMENTS APPLICATION
    // ========================================================================
    
    attachAppEvents() {
        // Événement backend connecté
        this.eventBus.on('backend:connected', () => {
            this.state.backendConnected = true;
            this.logger.info('Application', 'Backend connected');
        });
        
        // Événement backend déconnecté
        this.eventBus.on('backend:disconnected', () => {
            this.state.backendConnected = false;
            this.logger.warn('Application', 'Backend disconnected');
            
            if (this.config.autoReconnect) {
                this.scheduleReconnect();
            }
        });
        
        // Événement shutdown
        this.eventBus.on('app:shutdown', () => {
            this.shutdown();
        });
    }
    
    // ========================================================================
    // RECONNEXION
    // ========================================================================
    
    scheduleReconnect() {
        if (this.reconnectTimer) return;
        
        this.logger.info('Application', `Reconnecting in ${this.config.reconnectInterval}ms...`);
        
        this.reconnectTimer = setTimeout(() => {
            this.reconnectTimer = null;
            this.connectBackend();
        }, this.config.reconnectInterval);
    }
    
    // ========================================================================
    // INTERFACE
    // ========================================================================
    
    showInterface() {
        // Masquer l'écran de chargement
        const loader = document.getElementById('loader');
        if (loader) {
            loader.style.display = 'none';
        }
        
        // Afficher l'interface principale
        const app = document.getElementById('app');
        if (app) {
            app.style.visibility = 'visible';
            app.style.opacity = '1';
        }
    }
    
    // ========================================================================
    // SHUTDOWN
    // ========================================================================
    
    async shutdown() {
        this.logger.info('Application', '🛑 Shutting down...');
        
        this.state.ready = false;
        
        // Arrêter auto-reconnect
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        
        // Déconnecter backend
        if (this.services.backend) {
            await this.services.backend.disconnect();
        }
        
        // Nettoyer les contrôleurs
        Object.values(this.controllers).forEach(controller => {
            if (controller && typeof controller.destroy === 'function') {
                controller.destroy();
            }
        });
        
        // Nettoyer les modèles
        Object.values(this.models).forEach(model => {
            if (model && typeof model.destroy === 'function') {
                model.destroy();
            }
        });
        
        this.logger.info('Application', '✓ Application shut down');
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    getState() {
        return {
            ...this.state,
            servicesReady: Object.values(this.services).filter(s => s !== null).length,
            modelsReady: Object.values(this.models).filter(m => m !== null).length,
            viewsReady: Object.values(this.views).filter(v => v !== null).length,
            controllersReady: Object.values(this.controllers).filter(c => c !== null).length
        };
    }
    
    isReady() {
        return this.state.ready;
    }
    
    isBackendConnected() {
        return this.state.backendConnected;
    }





/**
 * Détruit l'application et libère toutes les ressources
 */
destroy() {
    if (this.state.destroyed) {
        this.logger.warn('Application', 'Already destroyed');
        return;
    }
    
    this.logger.info('Application', 'Destroying application...');
    
    try {
        // 1. Déconnecter backend
        if (this.services.backend) {
            this.services.backend.disconnect();
        }
        
        // 2. Détruire contrôleurs (ordre inverse de création)
        this.destroyControllers();
        
        // 3. Détruire vues
        this.destroyViews();
        
        // 4. Détruire modèles
        this.destroyModels();
        
        // 5. Détruire services
        this.destroyServices();
        
        // 6. Détruire EventBus en dernier
        if (this.eventBus) {
            this.eventBus.destroy();
        }
        
        // 7. Nettoyer références globales
        this.cleanupGlobalReferences();
        
        // 8. Marquer comme détruit
        this.state.destroyed = true;
        this.state.ready = false;
        
        this.logger.info('Application', '✓ Application destroyed');
        
    } catch (error) {
        this.logger.error('Application', 'Error during destruction:', error);
        throw error;
    }
}

/**
 * Détruit tous les contrôleurs
 * @private
 */
destroyControllers() {
    this.logger.info('Application', 'Destroying controllers...');
    
    const controllers = [
        'home',
        'editor',
        'routing',
        'search',
        'system',
        'playback',
        'instrument',
        'playlist',
        'file',
        'navigation',
        'globalPlayback'
    ];
    
    controllers.forEach(name => {
        if (this.controllers[name] && typeof this.controllers[name].destroy === 'function') {
            try {
                this.controllers[name].destroy();
                this.logger.debug('Application', `✓ ${name} controller destroyed`);
            } catch (error) {
                this.logger.error('Application', `Failed to destroy ${name} controller:`, error);
            }
        }
    });
    
    this.controllers = {};
}

/**
 * Détruit toutes les vues
 * @private
 */
destroyViews() {
    this.logger.info('Application', 'Destroying views...');
    
    const views = [
        'editor',
        'routing',
        'system',
        'keyboard',
        'instrument',
        'file',
        'home'
    ];
    
    views.forEach(name => {
        if (this.views[name] && typeof this.views[name].destroy === 'function') {
            try {
                this.views[name].destroy();
                this.logger.debug('Application', `✓ ${name} view destroyed`);
            } catch (error) {
                this.logger.error('Application', `Failed to destroy ${name} view:`, error);
            }
        }
    });
    
    this.views = {};
}

/**
 * Détruit tous les modèles
 * @private
 */
destroyModels() {
    this.logger.info('Application', 'Destroying models...');
    
    const models = [
        'routing',
        'editor',
        'playback',
        'system',
        'instrument',
        'playlist',
        'file'
    ];
    
    models.forEach(name => {
        if (this.models[name] && typeof this.models[name].destroy === 'function') {
            try {
                this.models[name].destroy();
                this.logger.debug('Application', `✓ ${name} model destroyed`);
            } catch (error) {
                this.logger.error('Application', `Failed to destroy ${name} model:`, error);
            }
        }
    });
    
    this.models = {};
}

/**
 * Détruit tous les services
 * @private
 */
destroyServices() {
    this.logger.info('Application', 'Destroying services...');
    
    const services = [
        'file',
        'midi',
        'storage',
        'backend'
    ];
    
    services.forEach(name => {
        if (this.services[name] && typeof this.services[name].destroy === 'function') {
            try {
                this.services[name].destroy();
                this.logger.debug('Application', `✓ ${name} service destroyed`);
            } catch (error) {
                this.logger.error('Application', `Failed to destroy ${name} service:`, error);
            }
        }
    });
    
    this.services = {};
}

/**
 * Nettoie les références globales
 * @private
 */
cleanupGlobalReferences() {
    this.logger.info('Application', 'Cleaning up global references...');
    
    // Nettoyer window.*
    const globalRefs = [
        'app',
        'eventBus',
        'logger',
        'debugConsole',
        'notifications',
        'backendService',
        'storageService',
        'midiService',
        'fileService',
        'fileModel',
        'playlistModel',
        'instrumentModel',
        'systemModel',
        'playbackModel',
        'editorModel',
        'routingModel',
        'homeView',
        'fileView',
        'instrumentView',
        'systemView',
        'routingView',
        'editorView',
        'navigationController',
        'fileController',
        'playlistController',
        'instrumentController',
        'playbackController',
        'systemController',
        'searchController',
        'routingController',
        'editorController',
        'homeController',
        'globalPlaybackController'
    ];
    
    globalRefs.forEach(ref => {
        if (window[ref]) {
            delete window[ref];
        }
    });
    
    this.logger.info('Application', '✓ Global references cleaned');
}

/**
 * Redémarre l'application
 * (destroy + init)
 */
async restart() {
    this.logger.info('Application', 'Restarting application...');
    
    this.destroy();
    
    // Attendre un peu
    await new Promise(resolve => setTimeout(resolve, 500));
    
    await this.init();
    
    this.logger.info('Application', '✓ Application restarted');
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
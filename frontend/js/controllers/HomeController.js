// ============================================================================
// Fichier: frontend/js/controllers/HomeController.js
// Version: 4.0.0 - API CONFORME v4.2.2
// Date: 2025-11-01
// ============================================================================
// Description:
//   ContrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â´leur de la page d'accueil - VERSION COMPLETE
//   Fusion de BaseController (v3.1.0) + Toutes fonctionnalitÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©s (v3.0.1)
//
// CORRECTIONS v3.2.1:
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ CRITIQUE: Fix double initialisation (BaseController.autoInitialize + Application.init())
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Ajout flag _dataLoaded pour ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©viter chargement multiple
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Ajout flag _viewRendered pour ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©viter rendu multiple
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ mÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©thode init() appelle onInitialize si pas encore initialisÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ loadInitialData() protÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©gÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e contre appels multiples
//
// CORRECTIONS v3.2.0:
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ HÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rite de BaseController
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Utilise models/views/notifications
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ PrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©serve TOUTES les fonctionnalitÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©s de v3.0.1
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Ajout destroy() complet
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Gestion playlists complÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨te (manage, edit, delete, load)
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Gestion routing complÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨te (auto-route, presets)
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Gestion fichiers complÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨te (upload, refresh)
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ IntÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©gration backend v3.0
//
// FonctionnalitÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©s:
//   - Playback (play, pause, stop, seek, tempo)
//   - Playlists (create, edit, delete, load, manage)
//   - Routing (assign, auto-route, presets)
//   - Fichiers (load, upload, refresh)
//   - Visualisation (channels, note preview)
// ============================================================================

class HomeController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ ModÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨les
        this.playbackModel = models?.playback || null;
        this.routingModel = models?.routing || null;
        this.fileModel = models?.file || null;
        this.playlistModel = models?.playlist || null;
        
        // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Vue
        this.homeView = views?.home || null;
        this.view = views?.home || null; // Alias pour compatibilitÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©
        
        // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©fÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rences aux autres controllers
        this.playbackController = null;
        this.playlistController = null;
        
        // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Backend
        // Ã¢Å“â€¦ this.backend initialisÃƒÂ© automatiquement par BaseController
        
        // ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°tat du contrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â´leur home
        this.homeState = {
            currentFile: null,
            playbackState: 'stopped', // stopped, playing, paused
            currentTime: 0,
            isPlaying: false
        };
        
        // Aliases pour compatibilitÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â© avec ancien code
        this.currentFile = null;
        this.playbackState = 'stopped';
        this.currentTime = 0;
        this.isPlaying = false;
        
        // Timer pour position
        this.playbackTimer = null;
        
        // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NOUVEAU: Flags pour ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©viter double initialisation
        this._dataLoaded = false;
        this._viewRendered = false;
        
        // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NOTE: BaseController appelle automatiquement initialize() via autoInitialize
        // qui appelle onInitialize() puis bindEvents()
    }
    
    // ========================================================================
    // HOOKS BASECONTROLLER
    // ========================================================================
    
    /**
     * Hook d'initialisation personnalisÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e
     * Override de BaseController.onInitialize()
     * ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ APPELE AUTOMATIQUEMENT par BaseController.initialize()
     */
    onInitialize() {
        this.logInfo( 'HomeController.onInitialize() - v3.2.1');
        
        // RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©cupÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rer rÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©fÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rence aux controllers
        if (window.app && window.app.controllers) {
            this.playbackController = window.app.controllers.playback;
            this.playlistController = window.app.controllers.playlist;
        }
        
        // Fallback si pas dans window.app
        if (!this.playbackController) {
            this.playbackController = window.playbackController;
        }
        if (!this.playlistController) {
            this.playlistController = window.playlistController;
        }
        
        // VÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rifier dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©pendances
        if (!this.backend) {
            this.logWarn( 'BackendService not available');
            if (this.notifications) {
                this.notifications.show({
                    type: 'warning',
                    message: 'Backend service not available',
                    duration: 3000
                });
            }
        }
        
        // Initialiser la vue (si pas encore rendue)
        if (this.view && !this._viewRendered) {
            if (typeof this.view.init === 'function') {
                this.view.init();
            }
            this._viewRendered = true;
        }
        
        // Charger donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es initiales (si pas encore chargÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es)
        if (!this._dataLoaded) {
            this.loadInitialData();
        }
        
        this.logInfo( 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ HomeController.onInitialize() complete');
    }
    
    /**
     * ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ MÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©thode init() publique appelÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e par Application.js
     * PrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©vient la double initialisation grÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ce aux flags
     */
    init() {
        this.logInfo( 'HomeController.init() called by Application');
        
        // Si dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©jÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  initialisÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â© par BaseController, juste rendre la vue
        if (this.state.isInitialized) {
            this.logInfo( 'Already initialized, just rendering view');
            
            // S'assurer que la vue est rendue
            if (this.homeView && !this._viewRendered) {
                if (typeof this.homeView.render === 'function') {
                    this.homeView.render();
                    this._viewRendered = true;
                    this.logInfo( 'HomeView rendered');
                }
            }
            
            // Charger donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es si pas encore fait
            if (!this._dataLoaded) {
                this.loadInitialData();
            }
            
            return;
        }
        
        // Sinon, initialiser complÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨tement
        this.logInfo( 'Not yet initialized, calling onInitialize');
        this.onInitialize();
    }
    
    /**
     * ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Charge les donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es initiales
     * ProtÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©gÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e contre les appels multiples
     */
    loadInitialData() {
        // ProtÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ger contre double chargement
        if (this._dataLoaded) {
            this.logInfo( 'Data already loaded, skipping');
            return;
        }
        
        this.logInfo( 'Loading initial data...');
        
        try {
            // Charger fichiers rÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©cents
            if (this.fileModel) {
                const recentFiles = this.fileModel.get?.('recentFiles') || [];
                this.logInfo( `Loaded ${recentFiles.length} recent files`);
            }
            
            // Charger playlists
            if (this.playlistModel) {
                const playlists = this.playlistModel.get?.('playlists') || [];
                this.logInfo( `Loaded ${playlists.length} playlists`);
            }
            
            // Marquer comme chargÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©
            this._dataLoaded = true;
            this.logInfo( 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ Initial data loaded');
            
        } catch (error) {
            this.handleError('Failed to load initial data', error);
        }
    }
    
    /**
     * Binding des ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nements
     * Override de BaseController.bindEvents()
     * ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ APPELE AUTOMATIQUEMENT par BaseController.initialize()
     */
    bindEvents() {
        this.logInfo( 'Binding home events...');
        
        // ========================================================================
        // EVENEMENTS FICHIERS
        // ========================================================================
        
        this.subscribe('file:list:updated', (data) => {
            this.logInfo( 'File list updated', data);
            if (this.view && this.view.updateFileList) {
                this.view.updateFileList(data.files);
            }
            if (this.view && this.view.updateStats) {
                this.view.updateStats({
                    totalFiles: data.count,
                    totalDuration: this.calculateTotalDuration(data.files),
                    totalSize: this.calculateTotalSize(data.files)
                });
            }
        });
        
        this.subscribe('file:selected', (data) => this.handleFileSelected(data));
        this.subscribe('file:loaded', (data) => this.handleFileLoaded(data));
        
        // ========================================================================
        // EVENEMENTS PLAYBACK
        // ========================================================================
        
        this.subscribe('playback:started', () => {
            this.playbackState = 'playing';
            this.isPlaying = true;
            this.homeState.playbackState = 'playing';
            this.homeState.isPlaying = true;
            
            if (this.view && this.view.updatePlaybackState) {
                this.view.updatePlaybackState('playing');
            }
            this.startProgressTimer();
        });
        
        this.subscribe('playback:paused', () => {
            this.playbackState = 'paused';
            this.isPlaying = false;
            this.homeState.playbackState = 'paused';
            this.homeState.isPlaying = false;
            
            if (this.view && this.view.updatePlaybackState) {
                this.view.updatePlaybackState('paused');
            }
            this.stopProgressTimer();
        });
        
        this.subscribe('playback:stopped', () => {
            this.playbackState = 'stopped';
            this.isPlaying = false;
            this.currentTime = 0;
            this.homeState.playbackState = 'stopped';
            this.homeState.isPlaying = false;
            this.homeState.currentTime = 0;
            
            if (this.view && this.view.updatePlaybackState) {
                this.view.updatePlaybackState('stopped');
            }
            if (this.view && this.view.updateProgress) {
                this.view.updateProgress(0, this.currentFile?.duration || 0);
            }
            this.stopProgressTimer();
        });
        
        this.subscribe('playback:ended', () => this.onPlaybackEnded());
        this.subscribe('playback:finished', () => this.onPlaybackEnded());
        
        this.subscribe('playback:stateUpdated', (data) => this.updatePlaybackState(data));
        this.subscribe('playback:positionUpdated', (data) => this.updatePlaybackPosition(data));
        
        // ========================================================================
        // EVENEMENTS ROUTING
        // ========================================================================
        
        this.subscribe('routing:changed', () => this.updateRoutingDisplay());
        this.subscribe('routing:enabled', (data) => this.handleRoutingEnabled(data));
        this.subscribe('routing:disabled', (data) => this.handleRoutingDisabled(data));
        
        // ========================================================================
        // EVENEMENTS PLAYLIST
        // ========================================================================
        
        this.subscribe('playlist:changed', (data) => {
            if (data.file && data.file.fileId) {
                this.loadFile(data.file.fileId);
            }
        });
        
        this.subscribe('playlist:loaded', (data) => {
            this.logInfo( 'Playlist loaded:', data.playlist?.name);
            
            if (this.view && this.view.updatePlaylistInfo) {
                this.view.updatePlaylistInfo(data.playlist);
            }
        });
        
        this.subscribe('playlist:next', (data) => {
            this.logInfo( 'Playlist next:', data.file?.name);
            
            // Auto-charger le fichier suivant si en lecture
            if (this.isPlaying && data.file) {
                const fileId = typeof data.file === 'string' ? data.file : data.file.id;
                this.loadFile(fileId);
            }
        });
        
        // ========================================================================
        // EVENEMENTS BACKEND
        // ========================================================================
        
        this.subscribe('backend:connected', () => this.onBackendConnected());
        this.subscribe('backend:disconnected', () => this.onBackendDisconnected());
        
        this.logInfo( 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ Events bound');
    }
    
    /**
     * Cleanup complet
     * Override de BaseController.destroy()
     */
    destroy() {
        this.logInfo( 'Destroying HomeController...');
        
        // 1. ArrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Âªter timers
        this.stopProgressTimer();
        
        // 2. Cleanup state
        this.homeState.currentFile = null;
        this.homeState.isPlaying = false;
        this.currentFile = null;
        this.isPlaying = false;
        this._dataLoaded = false;
        this._viewRendered = false;
        
        // 3. Cleanup vue
        if (this.view && typeof this.view.destroy === 'function') {
            this.view.destroy();
        }
        
        // 4. Appeler parent
        super.destroy();
        
        this.logInfo( 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ HomeController destroyed');
    }
    
    // ========================================================================
    // GESTION DES FICHIERS
    // ========================================================================
    
    /**
     * GÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re la sÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lection d'un fichier
     * @param {Object} data - DonnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es du fichier
     */
    async handleFileSelected(data) {
        this.logInfo( 'File selected', data);
        
        try {
            if (data.file) {
                const fileId = typeof data.file === 'string' ? data.file : (data.file.id || data.file.fileId);
                await this.loadFile(fileId);
            }
        }
        catch (error) {
            this.handleError('File selection failed', error);
        }
    }
    
    /**
     * GÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re le chargement d'un fichier
     * @param {Object} data - DonnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es du fichier
     */
    handleFileLoaded(data) {
        this.logInfo( 'File loaded', data);
        
        this.currentFile = data.file;
        this.homeState.currentFile = data.file;
        
        // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour la vue
        if (this.view && this.view.updateCurrentFile) {
            this.view.updateCurrentFile(data.file);
        }
        
        // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour le modÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨le
        if (this.fileModel && this.fileModel.set) {
            this.fileModel.set('currentFile', data.file);
        }
        
        this.showSuccess(`File loaded: ${data.file.name}`);
    }
    
    /**
     * Charge un fichier MIDI
     * @param {string|number} fileId - ID du fichier
     */
    async loadFile(fileId) {
        this.logInfo( `Loading file: ${fileId}`);
        
        try {
            // RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©cupÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rer le fichier
            const file = await this.fileModel?.get?.(fileId);
            
            if (!file) {
                throw new Error('File not found');
            }
            
            // Convertir en MidiJSON si nÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©cessaire
            if (!file.midiJson) {
                if (typeof MidiJsonConverter !== 'undefined') {
                    const converter = new MidiJsonConverter();
                    file.midiJson = await converter.midiToJson(file.data);
                    
                    // Sauvegarder la version JSON
                    if (this.fileModel?.update) {
                        await this.fileModel.update(fileId, { midiJson: file.midiJson });
                    }
                }
            }
            
            this.currentFile = file;
            this.homeState.currentFile = file;
            
            // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour la vue
            if (this.view && this.view.updateCurrentFile) {
                this.view.updateCurrentFile(file);
            }
            
            // Configurer le routing model avec le fichier courant
            if (this.routingModel && this.routingModel.setCurrentFile) {
                this.routingModel.setCurrentFile(file);
            }
            
            // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour les canaux
            if (file.midiJson) {
                const channels = file.midiJson.channels || [];
                const instruments = window.instrumentModel?.getAll?.() || [];
                
                if (this.view && this.view.updateRoutingGrid) {
                    this.view.updateRoutingGrid(channels, instruments);
                }
                if (this.view && this.view.updateChannelToggles) {
                    this.view.updateChannelToggles(channels);
                }
            }
            
            // Charger dans le playback controller
            if (this.playbackController && this.playbackController.loadMidiJson) {
                await this.playbackController.loadMidiJson(file.midiJson);
            }
            
            this.emitEvent('file:loaded', { file });
            
            this.showSuccess(`File loaded: ${file.name}`);
            this.logInfo( `ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ File loaded: ${file.name}`);
        }
        catch (error) {
            this.handleError('Load file failed', error);
            this.showNotification(`Failed to load file`);
        }
    }
    
    /**
     * RafraÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â®chit la liste des fichiers
     */
    async refreshFiles() {
        this.logInfo( 'Refreshing files...');
        
        try {
            const files = await this.fileModel?.loadAll?.();
            
            if (files && this.view && this.view.updateFileList) {
                this.view.updateFileList(files);
            }
            
            this.showSuccess('Files refreshed');
        }
        catch (error) {
            this.handleError('Refresh files failed', error);
            this.showNotification('Failed to refresh files');
        }
    }
    
    /**
     * Upload un fichier
     */
    uploadFile() {
        this.logInfo( 'Upload file requested');
        
        // CrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©er un input file temporaire
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.mid,.midi';
        
        input.onchange = async (e) => {
            const file = e.target.files[0];
            
            if (!file) return;
            
            try {
                const arrayBuffer = await file.arrayBuffer();
                
                // Convertir en MidiJSON
                if (typeof MidiJsonConverter === 'undefined') {
                    throw new Error('MidiJsonConverter not available');
                }
                
                const converter = new MidiJsonConverter();
                const midiJson = await converter.midiToJson(arrayBuffer);
                
                // Sauvegarder
                const savedFile = await this.fileModel?.create?.({
                    name: file.name,
                    data: arrayBuffer,
                    midiJson: midiJson,
                    size: file.size,
                    duration: midiJson.metadata?.duration || 0
                });
                
                this.showSuccess(`File "${file.name}" uploaded`);
                
                // RafraÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â®chir la liste
                await this.refreshFiles();
                
                // Charger le fichier
                if (savedFile && savedFile.id) {
                    await this.loadFile(savedFile.id);
                }
            }
            catch (error) {
                this.handleError('Upload file failed', error);
                this.showNotification(`Upload failed`);
            }
        };
        
        input.click();
    }
    
    /**
     * SÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lectionne le premier fichier
     */
    async selectFirstFile() {
        try {
            const files = await this.fileModel?.getAll?.();
            
            if (files && files.length > 0) {
                await this.loadFile(files[0].id);
            }
        }
        catch (error) {
            this.handleError('Select first file failed', error);
        }
    }
    
    // ========================================================================
    // CONTROLES DE LECTURE
    // ========================================================================
    
    /**
     * Lance la lecture
     */
    async play() {
        this.logInfo( 'Play requested');
        
        if (!this.currentFile) {
            this.showWarning('No file loaded');
            return;
        }
        
        try {
            if (this.playbackState === 'paused') {
                if (this.playbackController && this.playbackController.resume) {
                    await this.playbackController.resume();
                } else if (this.playbackController && this.playbackController.play) {
                    await this.playbackController.play();
                }
            } else {
                if (this.playbackController && this.playbackController.play) {
                    await this.playbackController.play();
                }
            }
        }
        catch (error) {
            this.handleError('Play failed', error);
            this.showNotification('Playback error');
        }
    }
    
    /**
     * Met en pause
     */
    async pause() {
        this.logInfo( 'Pause requested');
        
        try {
            if (this.playbackController && this.playbackController.pause) {
                await this.playbackController.pause();
            }
        }
        catch (error) {
            this.handleError('Pause failed', error);
        }
    }
    
    /**
     * ArrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Âªte la lecture
     */
    async stop() {
        this.logInfo( 'Stop requested');
        
        try {
            if (this.playbackController && this.playbackController.stop) {
                await this.playbackController.stop();
            }
        }
        catch (error) {
            this.handleError('Stop failed', error);
        }
    }
    
    /**
     * Fichier suivant
     */
    async next() {
        this.logInfo( 'Next requested');
        
        try {
            if (!this.playlistModel || !this.playlistModel.next) {
                this.logWarn( 'PlaylistModel.next not available');
                return;
            }
            
            const nextFile = this.playlistModel.next();
            
            if (nextFile) {
                const fileId = nextFile.fileId || nextFile.id;
                await this.loadFile(fileId);
                
                if (this.playbackState === 'playing') {
                    await this.play();
                }
            }
        }
        catch (error) {
            this.handleError('Next failed', error);
        }
    }
    
    /**
     * Fichier prÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©cÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©dent
     */
    async previous() {
        this.logInfo( 'Previous requested');
        
        try {
            if (!this.playlistModel || !this.playlistModel.previous) {
                this.logWarn( 'PlaylistModel.previous not available');
                return;
            }
            
            const prevFile = this.playlistModel.previous();
            
            if (prevFile) {
                const fileId = prevFile.fileId || prevFile.id;
                await this.loadFile(fileId);
                
                if (this.playbackState === 'playing') {
                    await this.play();
                }
            }
        }
        catch (error) {
            this.handleError('Previous failed', error);
        }
    }
    
    /**
     * Recherche dans la timeline
     * @param {number} percent - Position en pourcentage (0-1)
     */
    async seek(percent) {
        if (!this.currentFile) return;
        
        try {
            const time = this.currentFile.duration * percent;
            
            if (this.playbackController && this.playbackController.seek) {
                await this.playbackController.seek(time);
            }
            
            this.currentTime = time;
            this.homeState.currentTime = time;
            
            if (this.view && this.view.updateProgress) {
                this.view.updateProgress(time, this.currentFile.duration);
            }
        }
        catch (error) {
            this.handleError('Seek failed', error);
        }
    }
    
    /**
     * DÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©finit le tempo
     * @param {number} percent - Tempo en pourcentage (0-200%)
     */
    async setTempo(percent) {
        try {
            if (this.playbackController && this.playbackController.setTempo) {
                await this.playbackController.setTempo(percent);
            }
        }
        catch (error) {
            this.handleError('Set tempo failed', error);
        }
    }
    
    /**
     * GÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re la fin de lecture
     */
    async onPlaybackEnded() {
        this.logInfo( 'Playback ended');
        
        this.stopProgressTimer();
        this.playbackState = 'stopped';
        this.isPlaying = false;
        this.homeState.playbackState = 'stopped';
        this.homeState.isPlaying = false;
        
        if (this.view && this.view.updatePlaybackState) {
            this.view.updatePlaybackState('stopped');
        }
        
        // Passer au suivant si playlist active
        if (this.playlistModel && this.playlistModel.currentPlaylist) {
            const next = this.playlistModel.next?.();
            
            if (next) {
                const fileId = next.fileId || next.id;
                await this.loadFile(fileId);
                await this.play();
            }
        }
    }
    
    /**
     * Met ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©tat de lecture depuis le backend
     * @param {Object} data - ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°tat playback
     */
    updatePlaybackState(data) {
        this.logInfo( 'Playback state updated', data);
        
        if (data.state !== undefined) {
            this.playbackState = data.state;
            this.homeState.playbackState = data.state;
            this.isPlaying = (data.state === 'playing');
            this.homeState.isPlaying = (data.state === 'playing');
            
            if (this.isPlaying) {
                this.startProgressTimer();
            } else {
                this.stopProgressTimer();
            }
        }
        
        if (data.position !== undefined) {
            this.currentTime = data.position;
            this.homeState.currentTime = data.position;
        }
        
        this.updateView();
    }
    
    /**
     * Met ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour la position de lecture depuis le backend
     * @param {Object} data - {position: number}
     */
    updatePlaybackPosition(data) {
        if (data.position !== undefined) {
            this.currentTime = data.position;
            this.homeState.currentTime = data.position;
            this.updateView();
        }
    }
    
    // ========================================================================
    // GESTION DU ROUTING
    // ========================================================================
    
    /**
     * Assigne un instrument ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  un canal
     * @param {number} channel - NumÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ro de canal
     * @param {string} instrumentId - ID de l'instrument
     */
    async assignInstrument(channel, instrumentId) {
        this.logInfo( `Assign instrument ${instrumentId} to channel ${channel}`);
        
        try {
            if (!instrumentId) {
                if (this.routingModel && this.routingModel.removeRouting) {
                    this.routingModel.removeRouting(channel);
                }
            } else {
                if (this.routingModel && this.routingModel.assignInstrument) {
                    await this.routingModel.assignInstrument(channel, instrumentId);
                }
            }
            
            // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour l'affichage
            this.updateRoutingDisplay();
            
            // Appliquer au playback controller
            if (this.routingModel && this.routingModel.getRouting) {
                const routing = this.routingModel.getRouting(channel);
                
                if (routing && this.playbackController && this.playbackController.updateRouting) {
                    await this.playbackController.updateRouting(channel, routing);
                }
            }
            
            this.showSuccess('Instrument assigned');
        }
        catch (error) {
            this.handleError('Assign instrument failed', error);
            this.showNotification('Failed to assign instrument');
        }
    }
    
    /**
     * Auto-routing intelligent
     */
    async autoRoute() {
        this.logInfo( 'Auto-route requested');
        
        if (!this.currentFile) {
            this.showWarning('No file loaded');
            return;
        }
        
        try {
            if (!this.routingModel || !this.routingModel.autoRoute) {
                this.showWarning('Auto-route not available');
                return;
            }
            
            const assignments = await this.routingModel.autoRoute();
            
            // Appliquer tous les routings
            if (assignments && this.playbackController) {
                for (const assignment of assignments) {
                    const routing = this.routingModel.getRouting?.(assignment.channel);
                    
                    if (routing && this.playbackController.updateRouting) {
                        await this.playbackController.updateRouting(assignment.channel, routing);
                    }
                }
            }
            
            this.updateRoutingDisplay();
            
            this.showSuccess(`Auto-routed ${assignments.length} channels`);
        }
        catch (error) {
            this.handleError('Auto-route failed', error);
            this.showNotification('Auto-routing failed');
        }
    }
    
    /**
     * Efface tous les routings
     */
    clearRouting() {
        this.logInfo( 'Clear routing requested');
        
        if (this.routingModel && this.routingModel.clearAll) {
            this.routingModel.clearAll();
        }
        
        this.updateRoutingDisplay();
        this.showSuccess('Routing cleared');
    }
    
    /**
     * Sauvegarde un preset de routing
     */
    async saveRoutingPreset() {
        this.logInfo( 'Save routing preset requested');
        
        const name = prompt('Enter preset name:');
        
        if (!name) return;
        
        try {
            if (!this.routingModel || !this.routingModel.savePreset) {
                this.showWarning('Save preset not available');
                return;
            }
            
            const preset = await this.routingModel.savePreset(name);
            
            this.showSuccess(`Preset "${name}" saved`);
            
            // Recharger les presets
            await this.loadRoutingPresets();
        }
        catch (error) {
            this.handleError('Save preset failed', error);
            this.showNotification('Failed to save preset');
        }
    }
    
    /**
     * Charge un preset de routing
     * @param {string} presetId - ID du preset
     */
    async loadRoutingPreset(presetId) {
        this.logInfo( `Load routing preset: ${presetId}`);
        
        try {
            if (!this.routingModel || !this.routingModel.loadPreset) {
                this.showWarning('Load preset not available');
                return;
            }
            
            await this.routingModel.loadPreset(presetId);
            
            // Appliquer tous les routings
            if (this.routingModel.getAllRoutings && this.playbackController) {
                const routings = this.routingModel.getAllRoutings();
                
                for (const routing of routings) {
                    if (this.playbackController.updateRouting) {
                        await this.playbackController.updateRouting(routing.channel, routing);
                    }
                }
            }
            
            this.updateRoutingDisplay();
            this.showSuccess('Preset loaded');
        }
        catch (error) {
            this.handleError('Load preset failed', error);
            this.showNotification('Failed to load preset');
        }
    }
    
    /**
     * Charge les presets de routing
     */
    async loadRoutingPresets() {
        this.logInfo( 'Loading routing presets...');
        
        if (!this.routingModel || !this.routingModel.presets) {
            return;
        }
        
        const presets = this.routingModel.presets;
        const select = document.getElementById('routingPresetSelect');
        
        if (!select) return;
        
        select.innerHTML = '<option value="">-- Select preset --</option>';
        
        Array.from(presets.values()).forEach(preset => {
            const option = document.createElement('option');
            option.value = preset.id;
            option.textContent = preset.name;
            select.appendChild(option);
        });
    }
    
    /**
     * Met ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour l'affichage du routing
     */
    updateRoutingDisplay() {
        this.logInfo( 'Updating routing display');
        
        if (!this.currentFile) return;
        
        try {
            if (this.currentFile.midiJson) {
                const channels = this.currentFile.midiJson.channels || [];
                const instruments = window.instrumentModel?.getAll?.() || [];
                
                if (this.view && this.view.updateRoutingGrid) {
                    this.view.updateRoutingGrid(channels, instruments);
                }
            }
            
            // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour les statistiques
            if (this.routingModel && this.routingModel.getGlobalCompatibility) {
                const stats = this.routingModel.getGlobalCompatibility();
                
                if (this.view && this.view.updateRoutingStats) {
                    this.view.updateRoutingStats(stats);
                }
            }
        }
        catch (error) {
            this.handleError('Update routing display failed', error);
        }
    }
    
    /**
     * GÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re l'activation d'une route
     * @param {Object} data - DonnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es de la route
     */
    handleRoutingEnabled(data) {
        this.logInfo( 'Route enabled', data);
        this.updateRoutingDisplay();
        this.showSuccess(`Route enabled: ${data.routeName || 'route'}`);
    }
    
    /**
     * GÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re la dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©sactivation d'une route
     * @param {Object} data - DonnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es de la route
     */
    handleRoutingDisabled(data) {
        this.logInfo( 'Route disabled', data);
        this.updateRoutingDisplay();
        this.showInfo(`Route disabled: ${data.routeName || 'route'}`);
    }
    
    // ========================================================================
    // GESTION DES PLAYLISTS
    // ========================================================================
    
    /**
     * GÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re la playlist - Ouvre l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©diteur de playlist
     */
    managePlaylist() {
        this.logInfo( 'Manage playlist requested');
        
        // VÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rifier que ModalController est disponible
        if (!window.app || !window.app.modalController) {
            this.logDebug('error', 'ModalController not available');
            this.showNotification('Modal system not initialized');
            return;
        }
        
        // VÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rifier que PlaylistController est disponible
        if (!this.playlistController) {
            this.logDebug('error', 'PlaylistController not available');
            this.showNotification('Playlist system not initialized');
            return;
        }
        
        // DÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©terminer le mode: ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©dition ou crÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ation
        const currentPlaylist = this.playlistController.state?.currentPlaylist;
        
        if (currentPlaylist) {
            this.editCurrentPlaylist();
        } else {
            this.createNewPlaylist();
        }
    }
    
    /**
     * Ouvre l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©diteur pour crÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©er une nouvelle playlist
     */
    createNewPlaylist() {
        this.logInfo( 'Create new playlist requested');
        
        const modalController = window.app?.modalController;
        
        if (!modalController) {
            this.showNotification('Modal system not available');
            return;
        }
        
        if (modalController.openPlaylistEditor) {
            modalController.openPlaylistEditor(null);
        }
    }
    
    /**
     * ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°dite la playlist courante
     */
    editCurrentPlaylist() {
        this.logInfo( 'Edit current playlist requested');
        
        const modalController = window.app?.modalController;
        
        if (!modalController || !this.playlistController) {
            this.showNotification('Controllers not available');
            return;
        }
        
        const currentPlaylist = this.playlistController.state?.currentPlaylist;
        
        if (!currentPlaylist) {
            this.showNotification('No playlist selected');
            return;
        }
        
        if (modalController.openPlaylistEditor) {
            modalController.openPlaylistEditor(currentPlaylist.id);
        }
    }
    
    /**
     * ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°dite une playlist spÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©cifique par son ID
     * @param {string} playlistId - ID de la playlist
     */
    editPlaylist(playlistId) {
        this.logInfo( `Edit playlist: ${playlistId}`);
        
        const modalController = window.app?.modalController;
        
        if (!modalController) {
            this.showNotification('Modal system not available');
            return;
        }
        
        if (!playlistId) {
            this.showNotification('Invalid playlist ID');
            return;
        }
        
        if (modalController.openPlaylistEditor) {
            modalController.openPlaylistEditor(playlistId);
        }
    }
    
    /**
     * Supprime une playlist avec confirmation
     * @param {string} playlistId - ID de la playlist ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  supprimer
     */
    async deletePlaylist(playlistId) {
        this.logInfo( `Delete playlist: ${playlistId}`);
        
        if (!this.playlistController) {
            this.showNotification('PlaylistController not available');
            return;
        }
        
        if (!playlistId) {
            this.showNotification('Invalid playlist ID');
            return;
        }
        
        try {
            // RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©cupÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rer les infos de la playlist pour le message de confirmation
            const playlistModel = this.playlistController.playlistModel;
            const playlist = playlistModel?.getPlaylist?.(playlistId);
            
            if (!playlist) {
                this.showNotification('Playlist not found');
                return;
            }
            
            // Demander confirmation
            const confirmed = confirm(
                `Voulez-vous vraiment supprimer la playlist "${playlist.name}" ?\n\n` +
                `Cette action est irrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©versible.`
            );
            
            if (!confirmed) {
                this.logInfo( 'Delete cancelled by user');
                return;
            }
            
            // Supprimer via le controller
            if (this.playlistController.deletePlaylist) {
                const success = await this.playlistController.deletePlaylist(playlistId);
                
                if (success) {
                    this.showSuccess(`Playlist "${playlist.name}" supprimÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e`);
                } else {
                    this.showNotification('ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°chec de la suppression');
                }
            }
        }
        catch (error) {
            this.handleError('Delete playlist failed', error);
            this.showNotification('Erreur lors de la suppression');
        }
    }
    
    /**
     * Charge une playlist et commence la lecture
     * @param {string} playlistId - ID de la playlist
     */
    async loadPlaylistIntoPlayer(playlistId) {
        this.logInfo( `Load playlist into player: ${playlistId}`);
        
        if (!this.playlistController) {
            this.showNotification('PlaylistController not available');
            return;
        }
        
        if (!playlistId) {
            this.showNotification('Invalid playlist ID');
            return;
        }
        
        try {
            // Charger la playlist
            let playlist = null;
            
            if (this.playlistController.loadPlaylist) {
                playlist = await this.playlistController.loadPlaylist(playlistId);
            }
            
            if (!playlist) {
                this.showNotification('Failed to load playlist');
                return;
            }
            
            this.logInfo( `Playlist loaded: ${playlist.name}`);
            
            // Si la playlist a des fichiers, charger le premier
            if (playlist.files && playlist.files.length > 0) {
                const firstFile = playlist.files[0];
                const firstFileId = typeof firstFile === 'string' ? firstFile : (firstFile.id || firstFile.fileId);
                
                // Charger le fichier dans le player
                await this.loadFile(firstFileId);
                
                this.logInfo( 'First file loaded, ready to play');
                this.showSuccess(`Playlist "${playlist.name}" chargÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e`);
            } else {
                this.showInfo('Playlist vide');
            }
        }
        catch (error) {
            this.handleError('Load playlist into player failed', error);
            this.showNotification('Erreur lors du chargement');
        }
    }
    
    /**
     * Ajoute rapidement un fichier ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  la playlist courante
     * @param {string} fileId - ID du fichier
     */
    async quickAddToPlaylist(fileId) {
        this.logInfo( `Quick add to playlist: ${fileId}`);
        
        if (!this.playlistController) {
            this.showNotification('PlaylistController not available');
            return;
        }
        
        const currentPlaylist = this.playlistController.state?.currentPlaylist;
        
        if (!currentPlaylist) {
            // Pas de playlist courante - proposer d'en crÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©er une
            const createNew = confirm(
                'Aucune playlist active.\n\n' +
                'Voulez-vous crÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©er une nouvelle playlist ?'
            );
            
            if (createNew) {
                await this.createNewPlaylistWithFile(fileId);
            }
            return;
        }
        
        try {
            // Ajouter ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  la playlist courante
            if (this.playlistController.addFileToPlaylist) {
                const success = await this.playlistController.addFileToPlaylist(currentPlaylist.id, fileId);
                
                if (success) {
                    const file = this.fileModel?.getFileById?.(fileId) || this.fileModel?.get?.(fileId);
                    const fileName = file?.name || file?.filename || 'fichier';
                    this.showSuccess(`"${fileName}" ajoutÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â© ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  "${currentPlaylist.name}"`);
                }
            }
        }
        catch (error) {
            this.handleError('Quick add to playlist failed', error);
            this.showNotification('Erreur lors de l\'ajout');
        }
    }
    
    /**
     * CrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e une nouvelle playlist avec un fichier initial
     * @param {string} fileId - ID du fichier
     */
    async createNewPlaylistWithFile(fileId) {
        this.logInfo( `Create new playlist with file: ${fileId}`);
        
        if (!this.playlistController) {
            this.showNotification('PlaylistController not available');
            return;
        }
        
        // Demander le nom de la playlist
        const playlistName = prompt('Nom de la nouvelle playlist:', 'Ma Playlist');
        
        if (!playlistName || !playlistName.trim()) {
            this.logInfo( 'Playlist creation cancelled');
            return;
        }
        
        try {
            // CrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©er la playlist avec le fichier
            if (this.playlistController.createPlaylist) {
                const playlist = await this.playlistController.createPlaylist(playlistName.trim(), [fileId]);
                
                if (playlist) {
                    this.showSuccess(`Playlist "${playlist.name}" crÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e`);
                    
                    // Charger la nouvelle playlist
                    if (this.playlistController.loadPlaylist) {
                        await this.playlistController.loadPlaylist(playlist.id);
                    }
                }
            }
        }
        catch (error) {
            this.handleError('Create playlist with file failed', error);
            this.showNotification('Erreur lors de la crÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ation');
        }
    }
    
    // ========================================================================
    // VISUALISATION & CANAUX
    // ========================================================================
    
    /**
     * Active/dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©sactive un canal dans le visualizer
     * @param {number} channel - NumÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ro de canal
     * @param {boolean} enabled - ActivÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©
     */
    toggleChannel(channel, enabled) {
        this.logInfo( `Toggle channel ${channel}: ${enabled}`);
        
        if (this.view && this.view.visualizer && this.view.visualizer.toggleChannel) {
            this.view.visualizer.toggleChannel(channel, enabled);
        }
    }
    
    /**
     * Obtient les notes ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  venir
     * @param {number} currentTime - Temps actuel en ms
     * @returns {Array} Notes ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  venir
     */
    getUpcomingNotes(currentTime) {
        if (!this.currentFile || !this.currentFile.midiJson) {
            return [];
        }
        
        const previewTime = 2000; // 2 secondes
        const endTime = currentTime + previewTime;
        
        return this.currentFile.midiJson.timeline
            .filter(event => 
                event.type === 'noteOn' &&
                event.time >= currentTime &&
                event.time <= endTime
            )
            .map(event => ({
                ...event,
                timeOffset: event.time - currentTime
            }))
            .slice(0, 10);
    }
    
    // ========================================================================
    // PROGRESSION & TIMERS
    // ========================================================================
    
    /**
     * DÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©marre le timer de progression
     */
    startProgressTimer() {
        this.stopProgressTimer();
        
        this.playbackTimer = setInterval(() => {
            this.currentTime += 100; // IncrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ment de 100ms
            this.homeState.currentTime += 100;
            
            if (this.currentFile) {
                if (this.view && this.view.updateProgress) {
                    this.view.updateProgress(this.currentTime, this.currentFile.duration);
                }
                
                // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour les notes ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  venir
                const upcomingNotes = this.getUpcomingNotes(this.currentTime);
                if (this.view && this.view.updateNotePreview) {
                    this.view.updateNotePreview(upcomingNotes);
                }
            }
            
            // ArrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Âªter si on dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©passe la durÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e
            if (this.currentFile && this.currentTime >= this.currentFile.duration) {
                this.onPlaybackEnded();
            }
        }, 100);
        
        this.logInfo( 'Progress timer started');
    }
    
    /**
     * ArrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Âªte le timer de progression
     */
    stopProgressTimer() {
        if (this.playbackTimer) {
            clearInterval(this.playbackTimer);
            this.playbackTimer = null;
            this.logInfo( 'Progress timer stopped');
        }
    }
    
    // ========================================================================
    // NAVIGATION
    // ========================================================================
    
    /**
     * Ouvre l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©diteur MIDI
     */
    openEditor() {
        this.logInfo( 'Open editor requested');
        
        if (!this.currentFile) {
            this.showWarning('No file loaded');
            return;
        }
        
        // Sauvegarder l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©tat actuel
        this.pause();
        
        // Naviguer vers l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©diteur
        window.location.hash = '#editor';
        
        // L'EditorController prendra le relais
        if (window.editorController && window.editorController.loadFile) {
            window.editorController.loadFile(this.currentFile);
        }
    }
    
    /**
     * Ouvre les rÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©glages
     */
    openSettings() {
        this.logInfo( 'Open settings requested');
        window.location.hash = '#settings';
    }
    
    // ========================================================================
    // GESTION VUE
    // ========================================================================
    
    /**
     * Met ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour la vue
     */
    updateView() {
        try {
            if (!this.view) return;
            
            // PrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©parer les donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es pour la vue
            const viewData = {
                currentFile: this.currentFile,
                playbackState: this.playbackState,
                isPlaying: this.isPlaying,
                currentTime: this.currentTime,
                playlist: this.playlistModel ? (this.playlistModel.getItems?.() || []) : [],
                routes: this.routingModel ? (this.routingModel.getRoutes?.() || []) : []
            };
            
            // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour la vue
            if (this.view.update) {
                this.view.update(viewData);
            }
        }
        catch (error) {
            this.handleError('Update view failed', error);
        }
    }
    
    // ========================================================================
    // GESTION BACKEND
    // ========================================================================
    
    /**
     * GÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re la connexion au backend
     */
    onBackendConnected() {
        this.logInfo( 'Backend connected');
        this.refreshData();
        this.showSuccess('Connected to backend');
    }
    
    /**
     * GÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re la dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©connexion du backend
     */
    onBackendDisconnected() {
        this.logInfo( 'Backend disconnected');
        
        // ArrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Âªter lecture
        this.isPlaying = false;
        this.playbackState = 'stopped';
        this.homeState.isPlaying = false;
        this.homeState.playbackState = 'stopped';
        
        this.stopProgressTimer();
        this.updateView();
        
        this.showWarning('Disconnected from backend');
    }
    
    /**
     * RafraÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â®chit les donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es depuis le backend
     */
    async refreshData() {
        this.logInfo( 'Refreshing data...');
        
        try {
            // RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©cupÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rer ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©tat playback
            if (this.backend) {
                const playbackStatus = await this.backend.sendCommand('playback.getStatus', {});
                
                if (playbackStatus.success && playbackStatus.data) {
                    this.updatePlaybackState(playbackStatus.data);
                }
            }
            
            // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour routing
            this.updateRoutingDisplay();
            
            // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour vue
            this.updateView();
        }
        catch (error) {
            this.handleError('Refresh data failed', error);
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Calcule la durÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e totale de fichiers
     * @param {Array} files - Liste de fichiers
     * @returns {number} DurÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e totale en ms
     */
    calculateTotalDuration(files) {
        if (!files || !Array.isArray(files)) return 0;
        
        return files.reduce((total, file) => {
            return total + (file.duration || 0);
        }, 0);
    }
    
    /**
     * Calcule la taille totale de fichiers
     * @param {Array} files - Liste de fichiers
     * @returns {number} Taille totale en bytes
     */
    calculateTotalSize(files) {
        if (!files || !Array.isArray(files)) return 0;
        
        return files.reduce((total, file) => {
            return total + (file.size || 0);
        }, 0);
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * Retourne l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©tat actuel
     * @returns {Object} ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°tat
     */
    getState() {
        return {
            currentFile: this.currentFile,
            playbackState: this.playbackState,
            isPlaying: this.isPlaying,
            currentTime: this.currentTime,
            hasPlaylist: this.playlistModel ? (this.playlistModel.getCount?.() > 0) : false,
            hasRoutes: this.routingModel ? (this.routingModel.getRoutes?.().length > 0) : false
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = HomeController;
}

if (typeof window !== 'undefined') {
    window.HomeController = HomeController;
}

// ============================================================================
// FIN DU FICHIER HomeController.js v3.2.1-fixed-double-init
// ============================================================================
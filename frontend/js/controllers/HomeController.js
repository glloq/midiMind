// ============================================================================
// Fichier: frontend/js/controllers/HomeController.js
// Version: 4.0.0 - API CONFORME v4.2.2
// Date: 2025-11-01
// ============================================================================
// Description:
//   Contrôleur de la page d'accueil - VERSION COMPLETE
//   Fusion de BaseController (v3.1.0) + Toutes fonctionnalités (v3.0.1)
//
// CORRECTIONS v3.2.1:
//   • CRITIQUE: Fix double initialisation (BaseController.autoInitialize + Application.init())
//   • Ajout flag _dataLoaded pour éviter chargement multiple
//   • Ajout flag _viewRendered pour éviter rendu multiple
//   • méthode init() appelle onInitialize si pas encore initialisé
//   • loadInitialData() protégée contre appels multiples
//
// CORRECTIONS v3.2.0:
//   • Hérite de BaseController
//   • Utilise models/views/notifications
//   • Préserve TOUTES les fonctionnalités de v3.0.1
//   • Ajout destroy() complet
//   • Gestion playlists complète (manage, edit, delete, load)
//   • Gestion routing complète (auto-route, presets)
//   • Gestion fichiers complète (upload, refresh)
//   • Intégration backend v3.0
//
// Fonctionnalités:
//   - Playback (play, pause, stop, seek, tempo)
//   - Playlists (create, edit, delete, load, manage)
//   - Routing (assign, auto-route, presets)
//   - Fichiers (load, upload, refresh)
//   - Visualisation (channels, note preview)
// ============================================================================

class HomeController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        // • Modèles
        this.playbackModel = models?.playback || null;
        this.routingModel = models?.routing || null;
        this.fileModel = models?.file || null;
        this.playlistModel = models?.playlist || null;
        
        // • Vue
        this.homeView = views?.home || null;
        this.view = views?.home || null; // Alias pour compatibilité
        
        // • Références aux autres controllers
        this.playbackController = null;
        this.playlistController = null;
        
        // • Backend
        // ✓ this.backend initialisé automatiquement par BaseController
        
        // État du contrôleur home
        this.homeState = {
            currentFile: null,
            playbackState: 'stopped', // stopped, playing, paused
            currentTime: 0,
            isPlaying: false
        };
        
        // Aliases pour compatibilité avec ancien code
        this.currentFile = null;
        this.playbackState = 'stopped';
        this.currentTime = 0;
        this.isPlaying = false;
        
        // Timer pour position
        this.playbackTimer = null;
        
        // • NOUVEAU: Flags pour éviter double initialisation
        this._dataLoaded = false;
        this._viewRendered = false;
        
        // • NOTE: BaseController appelle automatiquement initialize() via autoInitialize
        // qui appelle onInitialize() puis bindEvents()
    }
    
    // ========================================================================
    // HOOKS BASECONTROLLER
    // ========================================================================
    
    /**
     * Hook d'initialisation personnalisée
     * Override de BaseController.onInitialize()
     * • APPELE AUTOMATIQUEMENT par BaseController.initialize()
     */
    onInitialize() {
        this.logInfo( 'HomeController.onInitialize() - v3.2.1');
        
        // Récupérer référence aux controllers
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
        
        // Vérifier dépendances
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
        
        // Charger données initiales (si pas encore chargées)
        if (!this._dataLoaded) {
            this.loadInitialData();
        }
        
        this.logInfo( '✓ HomeController.onInitialize() complete');
    }
    
    /**
     * • Méthode init() publique appelée par Application.js
     * Prévient la double initialisation grâce aux flags
     */
    init() {
        this.logInfo( 'HomeController.init() called by Application');
        
        // Si déjà initialisé par BaseController, juste rendre la vue
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
            
            // Charger données si pas encore fait
            if (!this._dataLoaded) {
                this.loadInitialData();
            }
            
            return;
        }
        
        // Sinon, initialiser complètement
        this.logInfo( 'Not yet initialized, calling onInitialize');
        this.onInitialize();
    }
    
    /**
     * • Charge les données initiales
     * Protégée contre les appels multiples
     */
    async loadInitialData() {
        // Protéger contre double chargement
        if (this._dataLoaded) {
            this.logInfo( 'Data already loaded, skipping');
            return;
        }

        this.logInfo( 'Loading initial data...');

        try {
            // ✅ FIX: Charger les fichiers depuis le backend
            await this.loadFilesFromBackend('/midi');

            // ✅ FIX: Charger les playlists depuis le backend
            await this.loadPlaylistsFromBackend();

            // ✅ FIX: Charger les devices depuis le backend
            await this.loadDevicesFromBackend();

            // Marquer comme chargé
            this._dataLoaded = true;
            this.logInfo( '✓ Initial data loaded');

        } catch (error) {
            this.handleError('Failed to load initial data', error);
        }
    }
    
    /**
     * Binding des événements
     * Override de BaseController.bindEvents()
     * • APPELE AUTOMATIQUEMENT par BaseController.initialize()
     */
    bindEvents() {
        this.logInfo( 'Binding home events...');
        
        // ========================================================================
        // EVENEMENTS FICHIERS
        // ========================================================================

        // ✅ NOUVEAU: Écouter les requêtes de fichiers depuis HomeView
        this.subscribe('home:request_files', async (data) => {
            this.logInfo( 'Files requested from HomeView');
            await this.loadFilesFromBackend(data.path || '/midi');
        });

        this.subscribe('home:request_playlists', async () => {
            this.logInfo( 'Playlists requested from HomeView');
            await this.loadPlaylistsFromBackend();
        });

        this.subscribe('home:request_devices', async () => {
            this.logInfo( 'Devices requested from HomeView');
            await this.loadDevicesFromBackend();
        });

        // Écouter les demandes de lecture/chargement de fichiers
        this.subscribe('home:play_file_requested', async (data) => {
            this.logInfo( 'Play file requested:', data.file_path);
            await this.loadAndPlayFile(data.file_path);
        });

        this.subscribe('home:load_file_requested', async (data) => {
            this.logInfo( 'Load file requested:', data.file_path);
            await this.loadFileByPath(data.file_path);
        });

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

        // ✅ FIX Bug #16: Handler pour lecture playlist depuis HomeView
        this.subscribe('home:play_playlist_requested', async (data) => {
            this.logInfo( 'Play playlist requested:', data.playlist_id);
            await this.playPlaylistFromHome(data.playlist_id);
        });

        // ✅ FIX Bug #17: Handler pour chargement playlist depuis HomeView
        this.subscribe('home:load_playlist_requested', async (data) => {
            this.logInfo( 'Load playlist requested:', data.playlist_id);
            await this.loadPlaylistFromHome(data.playlist_id);
        });

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
        
        this.logInfo( '✓ Events bound');
    }
    
    /**
     * Cleanup complet
     * Override de BaseController.destroy()
     */
    destroy() {
        this.logInfo( 'Destroying HomeController...');
        
        // 1. Arrêter timers
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
        
        this.logInfo( '✓ HomeController destroyed');
    }
    
    // ========================================================================
    // GESTION DES FICHIERS
    // ========================================================================
    
    /**
     * Gère la sélection d'un fichier
     * @param {Object} data - Données du fichier
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
     * Gère le chargement d'un fichier
     * @param {Object} data - Données du fichier
     */
    handleFileLoaded(data) {
        this.logInfo( 'File loaded', data);
        
        this.currentFile = data.file;
        this.homeState.currentFile = data.file;
        
        // Mettre à jour la vue
        if (this.view && this.view.updateCurrentFile) {
            this.view.updateCurrentFile(data.file);
        }
        
        // Mettre à jour le modèle
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
            // Récupérer le fichier
            const file = await this.fileModel?.get?.(fileId);
            
            if (!file) {
                throw new Error('File not found');
            }
            
            // Convertir en MidiJSON si nécessaire
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
            
            // Mettre à jour la vue
            if (this.view && this.view.updateCurrentFile) {
                this.view.updateCurrentFile(file);
            }
            
            // Configurer le routing model avec le fichier courant
            if (this.routingModel && this.routingModel.setCurrentFile) {
                this.routingModel.setCurrentFile(file);
            }
            
            // Mettre à jour les canaux
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
            this.logInfo( `✓ File loaded: ${file.name}`);
        }
        catch (error) {
            this.handleError('Load file failed', error);
            this.showNotification(`Failed to load file`);
        }
    }
    
    /**
     * Rafraîchit la liste des fichiers
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
        
        // Créer un input file temporaire
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
                
                // Rafraîchir la liste
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
     * Sélectionne le premier fichier
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
     * Arrête la lecture
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
     * Fichier précédent
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
     * Définit le tempo
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
     * Gère la fin de lecture
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
     * Met à jour l'état de lecture depuis le backend
     * @param {Object} data - État playback
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
     * Met à jour la position de lecture depuis le backend
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
     * Assigne un instrument à un canal
     * @param {number} channel - Numéro de canal
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
            
            // Mettre à jour l'affichage
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
     * Met à jour l'affichage du routing
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
            
            // Mettre à jour les statistiques
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
     * Gère l'activation d'une route
     * @param {Object} data - Données de la route
     */
    handleRoutingEnabled(data) {
        this.logInfo( 'Route enabled', data);
        this.updateRoutingDisplay();
        this.showSuccess(`Route enabled: ${data.routeName || 'route'}`);
    }
    
    /**
     * Gère la désactivation d'une route
     * @param {Object} data - Données de la route
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
     * Gère la playlist - Ouvre l'éditeur de playlist
     */
    managePlaylist() {
        this.logInfo( 'Manage playlist requested');
        
        // Vérifier que ModalController est disponible
        if (!window.app || !window.app.modalController) {
            this.logDebug('error', 'ModalController not available');
            this.showNotification('Modal system not initialized');
            return;
        }
        
        // Vérifier que PlaylistController est disponible
        if (!this.playlistController) {
            this.logDebug('error', 'PlaylistController not available');
            this.showNotification('Playlist system not initialized');
            return;
        }
        
        // Déterminer le mode: édition ou création
        const currentPlaylist = this.playlistController.state?.currentPlaylist;
        
        if (currentPlaylist) {
            this.editCurrentPlaylist();
        } else {
            this.createNewPlaylist();
        }
    }
    
    /**
     * Ouvre l'éditeur pour créer une nouvelle playlist
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
     * Édite la playlist courante
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
     * Édite une playlist spécifique par son ID
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
     * @param {string} playlistId - ID de la playlist à supprimer
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
            // Récupérer les infos de la playlist pour le message de confirmation
            const playlistModel = this.playlistController.playlistModel;
            const playlist = playlistModel?.getPlaylist?.(playlistId);
            
            if (!playlist) {
                this.showNotification('Playlist not found');
                return;
            }
            
            // Demander confirmation
            const confirmed = confirm(
                `Voulez-vous vraiment supprimer la playlist "${playlist.name}" ?\n\n` +
                `Cette action est irréversible.`
            );
            
            if (!confirmed) {
                this.logInfo( 'Delete cancelled by user');
                return;
            }
            
            // Supprimer via le controller
            if (this.playlistController.deletePlaylist) {
                const success = await this.playlistController.deletePlaylist(playlistId);
                
                if (success) {
                    this.showSuccess(`Playlist "${playlist.name}" supprimée`);
                } else {
                    this.showNotification('Échec de la suppression');
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
                this.showSuccess(`Playlist "${playlist.name}" chargée`);
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
     * Ajoute rapidement un fichier à la playlist courante
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
            // Pas de playlist courante - proposer d'en créer une
            const createNew = confirm(
                'Aucune playlist active.\n\n' +
                'Voulez-vous créer une nouvelle playlist ?'
            );
            
            if (createNew) {
                await this.createNewPlaylistWithFile(fileId);
            }
            return;
        }
        
        try {
            // Ajouter à la playlist courante
            if (this.playlistController.addFileToPlaylist) {
                const success = await this.playlistController.addFileToPlaylist(currentPlaylist.id, fileId);
                
                if (success) {
                    const file = this.fileModel?.getFileById?.(fileId) || this.fileModel?.get?.(fileId);
                    const fileName = file?.name || file?.filename || 'fichier';
                    this.showSuccess(`"${fileName}" ajouté à "${currentPlaylist.name}"`);
                }
            }
        }
        catch (error) {
            this.handleError('Quick add to playlist failed', error);
            this.showNotification('Erreur lors de l\'ajout');
        }
    }
    
    /**
     * Crée une nouvelle playlist avec un fichier initial
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
            // Créer la playlist avec le fichier
            if (this.playlistController.createPlaylist) {
                const playlist = await this.playlistController.createPlaylist(playlistName.trim(), [fileId]);
                
                if (playlist) {
                    this.showSuccess(`Playlist "${playlist.name}" créée`);
                    
                    // Charger la nouvelle playlist
                    if (this.playlistController.loadPlaylist) {
                        await this.playlistController.loadPlaylist(playlist.id);
                    }
                }
            }
        }
        catch (error) {
            this.handleError('Create playlist with file failed', error);
            this.showNotification('Erreur lors de la création');
        }
    }

    /**
     * ✅ FIX Bug #16: Lit une playlist depuis HomeView
     * @param {string} playlistId - ID de la playlist
     */
    async playPlaylistFromHome(playlistId) {
        this.logInfo( `Play playlist from home: ${playlistId}`);

        if (!this.playlistController) {
            this.showWarning('PlaylistController not available');
            return;
        }

        try {
            // Charger la playlist
            let playlist = null;

            if (this.playlistController.loadPlaylist) {
                playlist = await this.playlistController.loadPlaylist(playlistId);
            } else if (this.playlistModel && this.playlistModel.getPlaylist) {
                playlist = this.playlistModel.getPlaylist(playlistId);
            }

            if (!playlist) {
                this.showWarning('Playlist introuvable');
                return;
            }

            this.logInfo( `Playlist loaded: ${playlist.name}`);

            // Si la playlist a des fichiers, charger et lire le premier
            if (playlist.files && playlist.files.length > 0) {
                const firstFile = playlist.files[0];
                const firstFileId = typeof firstFile === 'string' ? firstFile : (firstFile.id || firstFile.fileId);

                // Charger le fichier
                await this.loadFile(firstFileId);

                // Lancer la lecture
                await this.play();

                this.showSuccess(`Lecture de "${playlist.name}"`);
            } else {
                this.showInfo('Playlist vide');
            }
        } catch (error) {
            this.handleError('Play playlist from home failed', error);
            this.showNotification('Erreur lors de la lecture de la playlist');
        }
    }

    /**
     * ✅ FIX Bug #17: Charge une playlist depuis HomeView
     * @param {string} playlistId - ID de la playlist
     */
    async loadPlaylistFromHome(playlistId) {
        this.logInfo( `Load playlist from home: ${playlistId}`);

        if (!this.playlistController) {
            this.showWarning('PlaylistController not available');
            return;
        }

        try {
            // Charger la playlist
            let playlist = null;

            if (this.playlistController.loadPlaylist) {
                playlist = await this.playlistController.loadPlaylist(playlistId);
            } else if (this.playlistModel && this.playlistModel.getPlaylist) {
                playlist = this.playlistModel.getPlaylist(playlistId);
            }

            if (!playlist) {
                this.showWarning('Playlist introuvable');
                return;
            }

            this.logInfo( `Playlist loaded: ${playlist.name}`);

            // Si la playlist a des fichiers, charger le premier
            if (playlist.files && playlist.files.length > 0) {
                const firstFile = playlist.files[0];
                const firstFileId = typeof firstFile === 'string' ? firstFile : (firstFile.id || firstFile.fileId);

                // Charger le fichier dans le player
                await this.loadFile(firstFileId);

                this.showSuccess(`Playlist "${playlist.name}" chargée (${playlist.files.length} morceaux)`);
            } else {
                this.showInfo('Playlist vide');
            }
        } catch (error) {
            this.handleError('Load playlist from home failed', error);
            this.showNotification('Erreur lors du chargement de la playlist');
        }
    }

    // ========================================================================
    // VISUALISATION & CANAUX
    // ========================================================================
    
    /**
     * Active/désactive un canal dans le visualizer
     * @param {number} channel - Numéro de canal
     * @param {boolean} enabled - Activé
     */
    toggleChannel(channel, enabled) {
        this.logInfo( `Toggle channel ${channel}: ${enabled}`);
        
        if (this.view && this.view.visualizer && this.view.visualizer.toggleChannel) {
            this.view.visualizer.toggleChannel(channel, enabled);
        }
    }
    
    /**
     * Obtient les notes à venir
     * @param {number} currentTime - Temps actuel en ms
     * @returns {Array} Notes à venir
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
     * ✅ FIX Bug #13 & #14: Démarre le timer de progression
     * - Réduit fréquence à 250ms au lieu de 100ms (Bug #13)
     * - Synchronise avec backend au lieu d'incrémentation locale (Bug #14)
     */
    startProgressTimer() {
        this.stopProgressTimer();

        this.playbackTimer = setInterval(async () => {
            // ✅ FIX Bug #14: Récupérer position depuis le backend
            if (this.backend) {
                try {
                    // Tenter de récupérer la position réelle
                    const status = await this.backend.sendCommand('playback.getStatus', {});
                    if (status && status.data && status.data.position !== undefined) {
                        this.currentTime = status.data.position;
                        this.homeState.currentTime = status.data.position;
                    } else {
                        // Fallback: incrémenter localement si backend ne répond pas
                        this.currentTime += 250;
                        this.homeState.currentTime += 250;
                    }
                } catch (error) {
                    // En cas d'erreur, incrémenter localement
                    this.currentTime += 250;
                    this.homeState.currentTime += 250;
                }
            } else {
                // Pas de backend: incrémenter localement
                this.currentTime += 250;
                this.homeState.currentTime += 250;
            }

            if (this.currentFile) {
                if (this.view && this.view.updateProgress) {
                    this.view.updateProgress(this.currentTime, this.currentFile.duration);
                }

                // Mettre à jour les notes à venir
                const upcomingNotes = this.getUpcomingNotes(this.currentTime);
                if (this.view && this.view.updateNotePreview) {
                    this.view.updateNotePreview(upcomingNotes);
                }
            }

            // Arrêter si on dépasse la durée
            if (this.currentFile && this.currentTime >= this.currentFile.duration) {
                this.onPlaybackEnded();
            }
        }, 250);  // ✅ FIX Bug #13: 250ms au lieu de 100ms (4 FPS au lieu de 10 FPS)

        this.logInfo( 'Progress timer started (250ms interval)');
    }
    
    /**
     * Arrête le timer de progression
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
     * Ouvre l'éditeur MIDI
     */
    openEditor() {
        this.logInfo( 'Open editor requested');
        
        if (!this.currentFile) {
            this.showWarning('No file loaded');
            return;
        }
        
        // Sauvegarder l'état actuel
        this.pause();
        
        // Naviguer vers l'éditeur
        window.location.hash = '#editor';
        
        // L'EditorController prendra le relais
        if (window.editorController && window.editorController.loadFile) {
            window.editorController.loadFile(this.currentFile);
        }
    }
    
    /**
     * Ouvre les réglages
     */
    openSettings() {
        this.logInfo( 'Open settings requested');
        window.location.hash = '#settings';
    }
    
    // ========================================================================
    // GESTION VUE
    // ========================================================================
    
    /**
     * Met à jour la vue
     */
    updateView() {
        try {
            if (!this.view) return;
            
            // Préparer les données pour la vue
            const viewData = {
                currentFile: this.currentFile,
                playbackState: this.playbackState,
                isPlaying: this.isPlaying,
                currentTime: this.currentTime,
                playlist: this.playlistModel ? (this.playlistModel.getItems?.() || []) : [],
                routes: this.routingModel ? (this.routingModel.getRoutes?.() || []) : []
            };
            
            // Mettre à jour la vue
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
     * Gère la connexion au backend
     */
    onBackendConnected() {
        this.logInfo( 'Backend connected');
        this.refreshData();
        this.showSuccess('Connected to backend');
    }
    
    /**
     * Gère la déconnexion du backend
     */
    onBackendDisconnected() {
        this.logInfo( 'Backend disconnected');
        
        // Arrêter lecture
        this.isPlaying = false;
        this.playbackState = 'stopped';
        this.homeState.isPlaying = false;
        this.homeState.playbackState = 'stopped';
        
        this.stopProgressTimer();
        this.updateView();
        
        this.showWarning('Disconnected from backend');
    }
    
    /**
     * Rafraîchit les données depuis le backend
     */
    async refreshData() {
        this.logInfo( 'Refreshing data...');
        
        try {
            // Récupérer état playback
            if (this.backend) {
                const playbackStatus = await this.backend.sendCommand('playback.getStatus', {});
                
                if (playbackStatus.success && playbackStatus.data) {
                    this.updatePlaybackState(playbackStatus.data);
                }
            }
            
            // Mettre à jour routing
            this.updateRoutingDisplay();
            
            // Mettre à jour vue
            this.updateView();
        }
        catch (error) {
            this.handleError('Refresh data failed', error);
        }
    }
    
    // ========================================================================
    // CHARGEMENT DES DONNÉES DEPUIS LE BACKEND
    // ========================================================================

    /**
     * Charge les fichiers depuis le backend
     * @param {string} path - Chemin des fichiers
     */
    async loadFilesFromBackend(path = '/midi') {
        this.logInfo( `Loading files from backend: ${path}`);

        try {
            if (!this.backend || !this.backend.listFiles) {
                this.logWarn( 'Backend not available');
                return;
            }

            const response = await this.backend.listFiles(path);
            const files = response.files || [];

            this.logInfo( `Loaded ${files.length} files from backend`);

            // Mettre à jour la vue directement
            if (this.view && this.view.updateFileList) {
                this.view.updateFileList(files);
            }

            // Mettre à jour les stats
            if (this.view && this.view.updateStats) {
                this.view.updateStats({
                    totalFiles: files.length,
                    totalDuration: this.calculateTotalDuration(files),
                    totalSize: this.calculateTotalSize(files)
                });
            }

            // Émettre l'événement pour les autres composants
            this.emitEvent('files:loaded', { files, count: files.length });
        } catch (error) {
            this.handleError('Failed to load files from backend', error);
        }
    }

    /**
     * Charge les playlists depuis le backend
     */
    async loadPlaylistsFromBackend() {
        this.logInfo( 'Loading playlists from backend');

        try {
            if (this.playlistModel && this.playlistModel.loadAll) {
                const playlists = await this.playlistModel.loadAll();

                if (this.view && this.view.updatePlaylistList) {
                    this.view.updatePlaylistList(playlists);
                }
            }
        } catch (error) {
            this.handleError('Failed to load playlists', error);
        }
    }

    /**
     * Charge les devices depuis le backend
     */
    async loadDevicesFromBackend() {
        this.logInfo( 'Loading devices from backend');

        try {
            if (!this.backend || !this.backend.listDevices) {
                this.logWarn( 'Backend.listDevices not available');
                return;
            }

            const response = await this.backend.listDevices();
            const devices = response.devices || [];

            if (this.view && this.view.updateDevicesList) {
                this.view.updateDevicesList(devices);
            }
        } catch (error) {
            this.handleError('Failed to load devices', error);
        }
    }

    /**
     * Charge et lit un fichier
     * @param {string} filePath - Chemin du fichier
     */
    async loadAndPlayFile(filePath) {
        this.logInfo( `Load and play file: ${filePath}`);

        try {
            // Utiliser GlobalPlaybackController
            const globalPlayback = window.globalPlaybackController || window.app?.controllers?.globalPlayback;

            if (!globalPlayback) {
                this.logWarn( 'GlobalPlaybackController not available');
                return;
            }

            // Charger le fichier
            await globalPlayback.load(filePath);

            // Lancer la lecture
            await globalPlayback.play();

            this.showSuccess(`Playing: ${filePath}`);
        } catch (error) {
            this.handleError('Failed to load and play file', error);
        }
    }

    /**
     * Charge un fichier par son chemin
     * @param {string} filePath - Chemin du fichier
     */
    async loadFileByPath(filePath) {
        this.logInfo( `Load file by path: ${filePath}`);

        try {
            // ✅ FIX: Trouver le fichier dans la liste
            const file = this.homeState.files.find(f =>
                f.path === filePath || f.name === filePath
            ) || { name: filePath.split('/').pop(), path: filePath };

            // Utiliser GlobalPlaybackController
            const globalPlayback = window.globalPlaybackController || window.app?.controllers?.globalPlayback;

            if (!globalPlayback) {
                this.logWarn( 'GlobalPlaybackController not available');
                return;
            }

            // Charger le fichier
            await globalPlayback.load(filePath);

            // ✅ FIX: Passer les données au EditorView si disponible
            const editorView = window.editorView || window.app?.views?.editor;
            if (editorView && editorView.viewState) {
                editorView.viewState.currentFile = file;
                // Les tracks et notes seront chargés par le backend
                if (editorView.render) {
                    editorView.render();
                }
            }

            // ✅ FIX: Naviguer vers l'éditeur
            const navigationController = window.app?.controllers?.navigation;
            if (navigationController) {
                navigationController.showPage('editor');
            }

            // ✅ FIX: Émettre l'événement pour mettre à jour le header
            if (this.eventBus) {
                this.eventBus.emit('globalPlayback:fileLoaded', {
                    fileName: file.name,
                    filename: file.name,
                    duration: file.duration || 0,
                    filePath: file.path || filePath
                });
            }

            this.showSuccess(`Loaded: ${file.name}`);
        } catch (error) {
            this.handleError('Failed to load file', error);
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    /**
     * Calcule la durée totale de fichiers
     * @param {Array} files - Liste de fichiers
     * @returns {number} Durée totale en ms
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
     * Retourne l'état actuel
     * @returns {Object} État
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
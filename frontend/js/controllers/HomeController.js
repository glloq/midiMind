// ============================================================================
// Fichier: frontend/js/controllers/HomeController.js
// Version: 3.2.0-complete
// Date: 2025-10-15
// ============================================================================
// Description:
//   ContrÃ´leur de la page d'accueil - VERSION COMPLÃˆTE
//   Fusion de BaseController (v3.1.0) + Toutes fonctionnalitÃ©s (v3.0.1)
//
// CORRECTIONS v3.2.0:
//   âœ… HÃ©rite de BaseController
//   âœ… Utilise models/views/notifications
//   âœ… PrÃ©serve TOUTES les fonctionnalitÃ©s de v3.0.1
//   âœ… Ajout destroy() complet
//   âœ… Gestion playlists complÃ¨te (manage, edit, delete, load)
//   âœ… Gestion routing complÃ¨te (auto-route, presets)
//   âœ… Gestion fichiers complÃ¨te (upload, refresh)
//   âœ… IntÃ©gration backend v3.0
//
// FonctionnalitÃ©s:
//   - Playback (play, pause, stop, seek, tempo)
//   - Playlists (create, edit, delete, load, manage)
//   - Routing (assign, auto-route, presets)
//   - Fichiers (load, upload, refresh)
//   - Visualisation (channels, note preview)
// ============================================================================

class HomeController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // âœ… ModÃ¨les
        this.playbackModel = models.playback;
        this.routingModel = models.routing;
        this.fileModel = models.file;
        this.playlistModel = models.playlist;
        
        // âœ… Vue
        this.homeView = views.home;
        this.view = views.home; // Alias pour compatibilitÃ©
        
        // âœ… RÃ©fÃ©rences aux autres controllers
        this.playbackController = null;
        this.playlistController = null;
        
        // âœ… Backend
        this.backend = window.backendService;
        
        // Ã‰tat du contrÃ´leur home
        this.homeState = {
            currentFile: null,
            playbackState: 'stopped', // stopped, playing, paused
            currentTime: 0,
            isPlaying: false
        };
        
        // Aliases pour compatibilitÃ© avec ancien code
        this.currentFile = null;
        this.playbackState = 'stopped';
        this.currentTime = 0;
        this.isPlaying = false;
        
        // Timer pour position
        this.playbackTimer = null;
        
        // ✅ REMOVED: this.initialize() - BaseController calls it via autoInitialize
    }
    
    // ========================================================================
    // HOOKS BASECONTROLLER
    // ========================================================================
    
    /**
     * Hook d'initialisation personnalisÃ©e
     * Override de BaseController.onInitialize()
     */
    onInitialize() {
        this.logDebug('home', 'Initializing HomeController v3.2.0...');
        
        // RÃ©cupÃ©rer rÃ©fÃ©rence aux controllers
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
        
        // VÃ©rifier dÃ©pendances
        if (!this.backend) {
            this.logDebug('error', 'BackendService not available');
            this.showNotification('Backend service not available');
        }
        
        // Initialiser la vue
        if (this.view) {
            this.view.init?.();
        }
        
        // Charger donnÃ©es initiales
        this.loadInitialData();
        
        this.logDebug('home', 'âœ“ HomeController v3.2.0 initialized');
    }
    
	/**
 * MÃ©thode init() publique appelÃ©e par Application.js
 */
init() {
    this.logDebug('home', 'HomeController.init() called');
    
    // S'assurer que la vue est rendue
    if (this.homeView && typeof this.homeView.render === 'function') {
        this.homeView.render();
        this.logDebug('home', 'HomeView rendered from controller');
    }
    
    // Charger les donnÃ©es initiales
    this.loadInitialData();
}

/**
 * Charge les donnÃ©es initiales
 */
loadInitialData() {
    // Charger fichiers rÃ©cents
    if (this.fileModel) {
        const recentFiles = this.fileModel.get('recentFiles') || [];
        this.logDebug('home', `Loaded ${recentFiles.length} recent files`);
    }
    
    // Charger playlists
    if (this.playlistModel) {
        const playlists = this.playlistModel.get('playlists') || [];
        this.logDebug('home', `Loaded ${playlists.length} playlists`);
    }
}
    /**
     * Binding des Ã©vÃ©nements
     * Override de BaseController.bindEvents()
     */
    bindEvents() {
        this.logDebug('home', 'Binding home events...');
        
        // ========================================================================
        // Ã‰VÃ‰NEMENTS FICHIERS
        // ========================================================================
        
        this.subscribe('file:list:updated', (data) => {
            this.logDebug('home', 'File list updated', data);
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
        // Ã‰VÃ‰NEMENTS PLAYBACK
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
        // Ã‰VÃ‰NEMENTS ROUTING
        // ========================================================================
        
        this.subscribe('routing:changed', () => this.updateRoutingDisplay());
        this.subscribe('routing:enabled', (data) => this.handleRoutingEnabled(data));
        this.subscribe('routing:disabled', (data) => this.handleRoutingDisabled(data));
        
        // ========================================================================
        // Ã‰VÃ‰NEMENTS PLAYLIST
        // ========================================================================
        
        this.subscribe('playlist:changed', (data) => {
            if (data.file && data.file.fileId) {
                this.loadFile(data.file.fileId);
            }
        });
        
        this.subscribe('playlist:loaded', (data) => {
            this.logDebug('home', 'Playlist loaded:', data.playlist?.name);
            
            if (this.view && this.view.updatePlaylistInfo) {
                this.view.updatePlaylistInfo(data.playlist);
            }
        });
        
        this.subscribe('playlist:next', (data) => {
            this.logDebug('home', 'Playlist next:', data.file?.name);
            
            // Auto-charger le fichier suivant si en lecture
            if (this.isPlaying && data.file) {
                const fileId = typeof data.file === 'string' ? data.file : data.file.id;
                this.loadFile(fileId);
            }
        });
        
        // ========================================================================
        // Ã‰VÃ‰NEMENTS BACKEND
        // ========================================================================
        
        this.subscribe('backend:connected', () => this.onBackendConnected());
        this.subscribe('backend:disconnected', () => this.onBackendDisconnected());
        
        this.logDebug('home', 'âœ“ Events bound');
    }
    
    /**
     * Cleanup complet
     * Override de BaseController.destroy()
     */
    destroy() {
        this.logDebug('home', 'Destroying HomeController...');
        
        // 1. ArrÃªter timers
        this.stopProgressTimer();
        
        // 2. Cleanup state
        this.homeState.currentFile = null;
        this.homeState.isPlaying = false;
        this.currentFile = null;
        this.isPlaying = false;
        
        // 3. Cleanup vue
        if (this.view) {
            this.view.destroy?.();
        }
        
        // 4. Appeler parent
        super.destroy();
        
        this.logDebug('home', 'âœ“ HomeController destroyed');
    }
    
    // ========================================================================
    // GESTION DES FICHIERS
    // ========================================================================
    
    /**
     * Charge les donnÃ©es initiales
     */
    async loadInitialData() {
        this.logDebug('home', 'Loading initial data...');
        
        try {
            // Charger les fichiers
            if (this.fileModel) {
                const files = await this.fileModel.loadAll?.();
                if (files && this.view && this.view.updateFileList) {
                    this.view.updateFileList(files);
                }
            }
            
            // Charger les instruments
            if (window.instrumentModel) {
                await window.instrumentModel.loadAll?.();
            }
            
            // Charger les playlists
            if (this.playlistModel) {
                await this.playlistModel.loadAll?.();
            }
            
            // Charger les presets de routing
            if (this.routingModel) {
                await this.routingModel.loadAllPresets?.();
            }
            
            this.logDebug('home', 'âœ“ Initial data loaded');
        }
        catch (error) {
            this.handleError('Load initial data failed', error);
            this.showNotification('Failed to load initial data');
        }
    }
    
    /**
     * GÃ¨re la sÃ©lection d'un fichier
     * @param {Object} data - DonnÃ©es du fichier
     */
    async handleFileSelected(data) {
        this.logDebug('home', 'File selected', data);
        
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
     * GÃ¨re le chargement d'un fichier
     * @param {Object} data - DonnÃ©es du fichier
     */
    handleFileLoaded(data) {
        this.logDebug('home', 'File loaded', data);
        
        this.currentFile = data.file;
        this.homeState.currentFile = data.file;
        
        // Mettre Ã  jour la vue
        if (this.view && this.view.updateCurrentFile) {
            this.view.updateCurrentFile(data.file);
        }
        
        // Mettre Ã  jour le modÃ¨le
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
        this.logDebug('home', `Loading file: ${fileId}`);
        
        try {
            // RÃ©cupÃ©rer le fichier
            const file = await this.fileModel.get?.(fileId);
            
            if (!file) {
                throw new Error('File not found');
            }
            
            // Convertir en MidiJSON si nÃ©cessaire
            if (!file.midiJson) {
                if (typeof MidiJsonConverter !== 'undefined') {
                    const converter = new MidiJsonConverter();
                    file.midiJson = await converter.midiToJson(file.data);
                    
                    // Sauvegarder la version JSON
                    if (this.fileModel.update) {
                        await this.fileModel.update(fileId, { midiJson: file.midiJson });
                    }
                }
            }
            
            this.currentFile = file;
            this.homeState.currentFile = file;
            
            // Mettre Ã  jour la vue
            if (this.view && this.view.updateCurrentFile) {
                this.view.updateCurrentFile(file);
            }
            
            // Configurer le routing model avec le fichier courant
            if (this.routingModel && this.routingModel.setCurrentFile) {
                this.routingModel.setCurrentFile(file);
            }
            
            // Mettre Ã  jour les canaux
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
            this.logDebug('home', `âœ“ File loaded: ${file.name}`);
        }
        catch (error) {
            this.handleError('Load file failed', error);
            this.showNotification(`Failed to load file`);
        }
    }
    
    /**
     * RafraÃ®chit la liste des fichiers
     */
    async refreshFiles() {
        this.logDebug('home', 'Refreshing files...');
        
        try {
            const files = await this.fileModel.loadAll?.();
            
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
        this.logDebug('home', 'Upload file requested');
        
        // CrÃ©er un input file temporaire
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
                const savedFile = await this.fileModel.create?.({
                    name: file.name,
                    data: arrayBuffer,
                    midiJson: midiJson,
                    size: file.size,
                    duration: midiJson.metadata?.duration || 0
                });
                
                this.showSuccess(`File "${file.name}" uploaded`);
                
                // RafraÃ®chir la liste
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
     * SÃ©lectionne le premier fichier
     */
    async selectFirstFile() {
        try {
            const files = await this.fileModel.getAll?.();
            
            if (files && files.length > 0) {
                await this.loadFile(files[0].id);
            }
        }
        catch (error) {
            this.handleError('Select first file failed', error);
        }
    }
    
    // ========================================================================
    // CONTRÃ”LES DE LECTURE
    // ========================================================================
    
    /**
     * Lance la lecture
     */
    async play() {
        this.logDebug('home', 'Play requested');
        
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
        this.logDebug('home', 'Pause requested');
        
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
     * ArrÃªte la lecture
     */
    async stop() {
        this.logDebug('home', 'Stop requested');
        
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
        this.logDebug('home', 'Next requested');
        
        try {
            if (!this.playlistModel || !this.playlistModel.next) {
                this.logDebug('warning', 'PlaylistModel.next not available');
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
     * Fichier prÃ©cÃ©dent
     */
    async previous() {
        this.logDebug('home', 'Previous requested');
        
        try {
            if (!this.playlistModel || !this.playlistModel.previous) {
                this.logDebug('warning', 'PlaylistModel.previous not available');
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
     * DÃ©finit le tempo
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
     * GÃ¨re la fin de lecture
     */
    async onPlaybackEnded() {
        this.logDebug('home', 'Playback ended');
        
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
     * Met Ã  jour l'Ã©tat de lecture depuis le backend
     * @param {Object} data - Ã‰tat playback
     */
    updatePlaybackState(data) {
        this.logDebug('home', 'Playback state updated', data);
        
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
     * Met Ã  jour la position de lecture depuis le backend
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
     * Assigne un instrument Ã  un canal
     * @param {number} channel - NumÃ©ro de canal
     * @param {string} instrumentId - ID de l'instrument
     */
    async assignInstrument(channel, instrumentId) {
        this.logDebug('home', `Assign instrument ${instrumentId} to channel ${channel}`);
        
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
            
            // Mettre Ã  jour l'affichage
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
        this.logDebug('home', 'Auto-route requested');
        
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
        this.logDebug('home', 'Clear routing requested');
        
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
        this.logDebug('home', 'Save routing preset requested');
        
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
        this.logDebug('home', `Load routing preset: ${presetId}`);
        
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
        this.logDebug('home', 'Loading routing presets...');
        
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
     * Met Ã  jour l'affichage du routing
     */
    updateRoutingDisplay() {
        this.logDebug('home', 'Updating routing display');
        
        if (!this.currentFile) return;
        
        try {
            if (this.currentFile.midiJson) {
                const channels = this.currentFile.midiJson.channels || [];
                const instruments = window.instrumentModel?.getAll?.() || [];
                
                if (this.view && this.view.updateRoutingGrid) {
                    this.view.updateRoutingGrid(channels, instruments);
                }
            }
            
            // Mettre Ã  jour les statistiques
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
     * GÃ¨re l'activation d'une route
     * @param {Object} data - DonnÃ©es de la route
     */
    handleRoutingEnabled(data) {
        this.logDebug('home', 'Route enabled', data);
        this.updateRoutingDisplay();
        this.showSuccess(`Route enabled: ${data.routeName || 'route'}`);
    }
    
    /**
     * GÃ¨re la dÃ©sactivation d'une route
     * @param {Object} data - DonnÃ©es de la route
     */
    handleRoutingDisabled(data) {
        this.logDebug('home', 'Route disabled', data);
        this.updateRoutingDisplay();
        this.showInfo(`Route disabled: ${data.routeName || 'route'}`);
    }
    
    // ========================================================================
    // GESTION DES PLAYLISTS
    // ========================================================================
    
    /**
     * GÃ¨re la playlist - Ouvre l'Ã©diteur de playlist
     */
    managePlaylist() {
        this.logDebug('home', 'Manage playlist requested');
        
        // VÃ©rifier que ModalController est disponible
        if (!window.app || !window.app.modalController) {
            this.logDebug('error', 'ModalController not available');
            this.showNotification('Modal system not initialized');
            return;
        }
        
        // VÃ©rifier que PlaylistController est disponible
        if (!this.playlistController) {
            this.logDebug('error', 'PlaylistController not available');
            this.showNotification('Playlist system not initialized');
            return;
        }
        
        // DÃ©terminer le mode: Ã©dition ou crÃ©ation
        const currentPlaylist = this.playlistController.state?.currentPlaylist;
        
        if (currentPlaylist) {
            this.editCurrentPlaylist();
        } else {
            this.createNewPlaylist();
        }
    }
    
    /**
     * Ouvre l'Ã©diteur pour crÃ©er une nouvelle playlist
     */
    createNewPlaylist() {
        this.logDebug('home', 'Create new playlist requested');
        
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
     * Ã‰dite la playlist courante
     */
    editCurrentPlaylist() {
        this.logDebug('home', 'Edit current playlist requested');
        
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
     * Ã‰dite une playlist spÃ©cifique par son ID
     * @param {string} playlistId - ID de la playlist
     */
    editPlaylist(playlistId) {
        this.logDebug('home', `Edit playlist: ${playlistId}`);
        
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
     * @param {string} playlistId - ID de la playlist Ã  supprimer
     */
    async deletePlaylist(playlistId) {
        this.logDebug('home', `Delete playlist: ${playlistId}`);
        
        if (!this.playlistController) {
            this.showNotification('PlaylistController not available');
            return;
        }
        
        if (!playlistId) {
            this.showNotification('Invalid playlist ID');
            return;
        }
        
        try {
            // RÃ©cupÃ©rer les infos de la playlist pour le message de confirmation
            const playlistModel = this.playlistController.playlistModel;
            const playlist = playlistModel?.getPlaylist?.(playlistId);
            
            if (!playlist) {
                this.showNotification('Playlist not found');
                return;
            }
            
            // Demander confirmation
            const confirmed = confirm(
                `Voulez-vous vraiment supprimer la playlist "${playlist.name}" ?\n\n` +
                `Cette action est irrÃ©versible.`
            );
            
            if (!confirmed) {
                this.logDebug('home', 'Delete cancelled by user');
                return;
            }
            
            // Supprimer via le controller
            if (this.playlistController.deletePlaylist) {
                const success = await this.playlistController.deletePlaylist(playlistId);
                
                if (success) {
                    this.showSuccess(`Playlist "${playlist.name}" supprimÃ©e`);
                } else {
                    this.showNotification('Ã‰chec de la suppression');
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
        this.logDebug('home', `Load playlist into player: ${playlistId}`);
        
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
            
            this.logDebug('home', `Playlist loaded: ${playlist.name}`);
            
            // Si la playlist a des fichiers, charger le premier
            if (playlist.files && playlist.files.length > 0) {
                const firstFile = playlist.files[0];
                const firstFileId = typeof firstFile === 'string' ? firstFile : (firstFile.id || firstFile.fileId);
                
                // Charger le fichier dans le player
                await this.loadFile(firstFileId);
                
                this.logDebug('home', 'First file loaded, ready to play');
                this.showSuccess(`Playlist "${playlist.name}" chargÃ©e`);
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
     * Ajoute rapidement un fichier Ã  la playlist courante
     * @param {string} fileId - ID du fichier
     */
    async quickAddToPlaylist(fileId) {
        this.logDebug('home', `Quick add to playlist: ${fileId}`);
        
        if (!this.playlistController) {
            this.showNotification('PlaylistController not available');
            return;
        }
        
        const currentPlaylist = this.playlistController.state?.currentPlaylist;
        
        if (!currentPlaylist) {
            // Pas de playlist courante - proposer d'en crÃ©er une
            const createNew = confirm(
                'Aucune playlist active.\n\n' +
                'Voulez-vous crÃ©er une nouvelle playlist ?'
            );
            
            if (createNew) {
                await this.createNewPlaylistWithFile(fileId);
            }
            return;
        }
        
        try {
            // Ajouter Ã  la playlist courante
            if (this.playlistController.addFileToPlaylist) {
                const success = await this.playlistController.addFileToPlaylist(currentPlaylist.id, fileId);
                
                if (success) {
                    const file = this.fileModel?.getFileById?.(fileId) || this.fileModel?.get?.(fileId);
                    const fileName = file?.name || file?.filename || 'fichier';
                    this.showSuccess(`"${fileName}" ajoutÃ© Ã  "${currentPlaylist.name}"`);
                }
            }
        }
        catch (error) {
            this.handleError('Quick add to playlist failed', error);
            this.showNotification('Erreur lors de l\'ajout');
        }
    }
    
    /**
     * CrÃ©e une nouvelle playlist avec un fichier initial
     * @param {string} fileId - ID du fichier
     */
    async createNewPlaylistWithFile(fileId) {
        this.logDebug('home', `Create new playlist with file: ${fileId}`);
        
        if (!this.playlistController) {
            this.showNotification('PlaylistController not available');
            return;
        }
        
        // Demander le nom de la playlist
        const playlistName = prompt('Nom de la nouvelle playlist:', 'Ma Playlist');
        
        if (!playlistName || !playlistName.trim()) {
            this.logDebug('home', 'Playlist creation cancelled');
            return;
        }
        
        try {
            // CrÃ©er la playlist avec le fichier
            if (this.playlistController.createPlaylist) {
                const playlist = await this.playlistController.createPlaylist(playlistName.trim(), [fileId]);
                
                if (playlist) {
                    this.showSuccess(`Playlist "${playlist.name}" crÃ©Ã©e`);
                    
                    // Charger la nouvelle playlist
                    if (this.playlistController.loadPlaylist) {
                        await this.playlistController.loadPlaylist(playlist.id);
                    }
                }
            }
        }
        catch (error) {
            this.handleError('Create playlist with file failed', error);
            this.showNotification('Erreur lors de la crÃ©ation');
        }
    }
    
    // ========================================================================
    // VISUALISATION & CANAUX
    // ========================================================================
    
    /**
     * Active/dÃ©sactive un canal dans le visualizer
     * @param {number} channel - NumÃ©ro de canal
     * @param {boolean} enabled - ActivÃ©
     */
    toggleChannel(channel, enabled) {
        this.logDebug('home', `Toggle channel ${channel}: ${enabled}`);
        
        if (this.view && this.view.visualizer && this.view.visualizer.toggleChannel) {
            this.view.visualizer.toggleChannel(channel, enabled);
        }
    }
    
    /**
     * Obtient les notes Ã  venir
     * @param {number} currentTime - Temps actuel en ms
     * @returns {Array} Notes Ã  venir
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
     * DÃ©marre le timer de progression
     */
    startProgressTimer() {
        this.stopProgressTimer();
        
        this.playbackTimer = setInterval(() => {
            this.currentTime += 100; // IncrÃ©ment de 100ms
            this.homeState.currentTime += 100;
            
            if (this.currentFile) {
                if (this.view && this.view.updateProgress) {
                    this.view.updateProgress(this.currentTime, this.currentFile.duration);
                }
                
                // Mettre Ã  jour les notes Ã  venir
                const upcomingNotes = this.getUpcomingNotes(this.currentTime);
                if (this.view && this.view.updateNotePreview) {
                    this.view.updateNotePreview(upcomingNotes);
                }
            }
            
            // ArrÃªter si on dÃ©passe la durÃ©e
            if (this.currentFile && this.currentTime >= this.currentFile.duration) {
                this.onPlaybackEnded();
            }
        }, 100);
        
        this.logDebug('home', 'Progress timer started');
    }
    
    /**
     * ArrÃªte le timer de progression
     */
    stopProgressTimer() {
        if (this.playbackTimer) {
            clearInterval(this.playbackTimer);
            this.playbackTimer = null;
            this.logDebug('home', 'Progress timer stopped');
        }
    }
    
    // ========================================================================
    // NAVIGATION
    // ========================================================================
    
    /**
     * Ouvre l'Ã©diteur MIDI
     */
    openEditor() {
        this.logDebug('home', 'Open editor requested');
        
        if (!this.currentFile) {
            this.showWarning('No file loaded');
            return;
        }
        
        // Sauvegarder l'Ã©tat actuel
        this.pause();
        
        // Naviguer vers l'Ã©diteur
        window.location.hash = '#editor';
        
        // L'EditorController prendra le relais
        if (window.editorController && window.editorController.loadFile) {
            window.editorController.loadFile(this.currentFile);
        }
    }
    
    /**
     * Ouvre les rÃ©glages
     */
    openSettings() {
        this.logDebug('home', 'Open settings requested');
        window.location.hash = '#settings';
    }
    
    // ========================================================================
    // GESTION VUE
    // ========================================================================
    
    /**
     * Met Ã  jour la vue
     */
    updateView() {
        try {
            if (!this.view) return;
            
            // PrÃ©parer les donnÃ©es pour la vue
            const viewData = {
                currentFile: this.currentFile,
                playbackState: this.playbackState,
                isPlaying: this.isPlaying,
                currentTime: this.currentTime,
                playlist: this.playlistModel ? (this.playlistModel.getItems?.() || []) : [],
                routes: this.routingModel ? (this.routingModel.getRoutes?.() || []) : []
            };
            
            // Mettre Ã  jour la vue
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
     * GÃ¨re la connexion au backend
     */
    onBackendConnected() {
        this.logDebug('home', 'Backend connected');
        this.refreshData();
        this.showSuccess('Connected to backend');
    }
    
    /**
     * GÃ¨re la dÃ©connexion du backend
     */
    onBackendDisconnected() {
        this.logDebug('home', 'Backend disconnected');
        
        // ArrÃªter lecture
        this.isPlaying = false;
        this.playbackState = 'stopped';
        this.homeState.isPlaying = false;
        this.homeState.playbackState = 'stopped';
        
        this.stopProgressTimer();
        this.updateView();
        
        this.showWarning('Disconnected from backend');
    }
    
    /**
     * RafraÃ®chit les donnÃ©es depuis le backend
     */
    async refreshData() {
        this.logDebug('home', 'Refreshing data...');
        
        try {
            // RÃ©cupÃ©rer Ã©tat playback
            if (this.backend) {
                const playbackStatus = await this.backend.sendCommand('playback.status', {});
                
                if (playbackStatus.success && playbackStatus.data) {
                    this.updatePlaybackState(playbackStatus.data);
                }
            }
            
            // Mettre Ã  jour routing
            this.updateRoutingDisplay();
            
            // Mettre Ã  jour vue
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
     * Calcule la durÃ©e totale de fichiers
     * @param {Array} files - Liste de fichiers
     * @returns {number} DurÃ©e totale en ms
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
     * Retourne l'Ã©tat actuel
     * @returns {Object} Ã‰tat
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
window.HomeController = HomeController;

// ============================================================================
// FIN DU FICHIER HomeController.js v3.2.0-complete
// ============================================================================
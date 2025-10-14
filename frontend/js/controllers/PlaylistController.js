// ============================================================================
// Fichier: frontend/js/controllers/PlaylistController.js
// Version: v3.0.3 - COMPLETE
// Date: 2025-10-10
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Contrôleur principal de gestion des playlists
//   Coordonne PlaylistModel, PlaylistView et gère les interactions
//
// Fonctionnalités:
//   ✓ Gestion CRUD playlists (Create, Read, Update, Delete)
//   ✓ Navigation (next, previous, jump)
//   ✓ Queue temporaire
//   ✓ Modes lecture (shuffle, repeat, auto-advance)
//   ✓ Import/Export playlists (M3U, PLS, XSPF, JSON)
//   ✓ Drag & Drop
//   ✓ Historique de lecture
//
// Architecture:
//   Hérite de BaseController
//   Utilise PlaylistModel pour la logique métier
//   Coordonne avec PlaylistView pour l'affichage
//
// Auteur: midiMind Team
// ============================================================================

/**
 * PlaylistController - Contrôleur de gestion des playlists
 * @extends BaseController
 */
class PlaylistController extends BaseController {
    
    // ========================================================================
    // CONSTRUCTEUR
    // ========================================================================
    
    /**
     * Construit le contrôleur de playlist
     * @param {EventBus} eventBus - Bus d'événements global
     * @param {Object} models - Objet contenant tous les modèles
     * @param {Object} views - Objet contenant toutes les vues
     * @param {NotificationManager} notifications - Gestionnaire de notifications
     * @param {DebugConsole} debugConsole - Console de debug
     */
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Nom du contrôleur
        this.name = 'PlaylistController';
        
        // Références aux modèles
        this.playlistModel = models?.playlist || null;
        this.fileModel = models?.file || null;
        
        // Référence à la vue
        this.view = views?.playlist || null;
        
        // État du contrôleur
        this.state = {
            currentPlaylist: null,
            currentFile: null,
            currentIndex: 0,
            isPlaying: false,
            shuffleMode: false,
            repeatMode: 'none',  // 'none', 'one', 'all'
            autoAdvance: true,
            queue: [],
            queueIndex: 0,
            isPlayingQueue: false,
            history: [],
            lastAction: null,
            errors: []
        };
        
        // Configuration
        this.config = {
            maxHistorySize: 50,
            autoSaveState: true,
            autoRefreshView: true,
            autoNotifications: true,
            enableQueue: true,
            enableShuffle: true,
            enableRepeat: true,
            enableAutoAdvance: true,
            enableImportExport: true,
            notifyOnChange: true,
            persistState: true,
            debugMode: false
        };
        
        // Cache
        this.cache = {
            playlists: new Map(),
            files: new Map(),
            lastUpdate: null,
            dirty: false
        };
        
        // Statistiques
        this.stats = {
            playlistsCreated: 0,
            playlistsDeleted: 0,
            filesPlayed: 0,
            totalPlaytime: 0,
            queueOperations: 0,
            shuffleToggles: 0,
            errors: 0
        };
        
        // Import/Export handler (sera ajouté par mixin)
        this.importExport = null;
        
        // Logger
        this.logger = window.Logger || console;
        
        this.logger.info('PlaylistController', '🎵 PlaylistController v3.0.3 initialized');
    }
    

    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        this.logDebug('playlist', '🎵 Initializing PlaylistController v3.0.2');
        
        this.bindEvents();
        this.loadSavedState();
        
        this.state.isInitialized = true;
        
        this.logDebug('playlist', '✓ PlaylistController initialized with auto-advance & queue');
    }
    
    /**
     * Injection du PlaybackController (appelée par Application)
     */
    setPlaybackController(playbackController) {
        this.playbackController = playbackController;
        this.logDebug('playlist', '✓ PlaybackController linked');
    }
    
    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================
    
    bindEvents() {
        // Événements PlaylistModel
        this.eventBus.on('playlist:created', (data) => this.onPlaylistCreated(data));
        this.eventBus.on('playlist:loaded', (data) => this.onPlaylistLoaded(data));
        this.eventBus.on('playlist:updated', (data) => this.onPlaylistUpdated(data));
        this.eventBus.on('playlist:deleted', (data) => this.onPlaylistDeleted(data));
        
        // Navigation playlist
        this.eventBus.on('playlist:next', (data) => this.onNext(data));
        this.eventBus.on('playlist:previous', (data) => this.onPrevious(data));
        this.eventBus.on('playlist:jump', (data) => this.onJump(data));
        this.eventBus.on('playlist:ended', () => this.onPlaylistEnded());
        
        // Auto-advance ✅ NOUVEAU
        this.eventBus.on('playlist:auto-advance', (data) => this.onAutoAdvance(data));
        
        // Queue management ✅ NOUVEAU
        this.eventBus.on('playlist:queue-added', (data) => this.onQueueAdded(data));
        this.eventBus.on('playlist:queue-removed', (data) => this.onQueueRemoved(data));
        this.eventBus.on('playlist:queue-cleared', () => this.onQueueCleared());
        this.eventBus.on('playlist:queue-started', (data) => this.onQueueStarted(data));
        this.eventBus.on('playlist:queue-ended', () => this.onQueueEnded());
        
        // Modes de lecture
        this.eventBus.on('playlist:shuffle-changed', (data) => this.onShuffleChanged(data));
        this.eventBus.on('playlist:repeat-changed', (data) => this.onRepeatChanged(data));
        this.eventBus.on('playlist:auto-advance-changed', (data) => this.onAutoAdvanceChanged(data));
        
        // Événements Playback (pour coordination)
        this.eventBus.on('playback:started', () => this.onPlaybackStarted());
        this.eventBus.on('playback:stopped', () => this.onPlaybackStopped());
        this.eventBus.on('playback:finished', () => this.onPlaybackFinished());
        
        this.logDebug('playlist', '✓ Events bound');
    }
    
    // ========================================================================
    // GESTION PLAYLISTS
    // ========================================================================
    
    /**
     * Crée une nouvelle playlist
     */
    async createPlaylist(name, files = []) {
        if (!name || !name.trim()) {
            this.showError('Playlist name is required');
            return null;
        }
        
        try {
            this.logDebug('playlist', `Creating playlist: ${name}`);
            
            const playlist = await this.playlistModel.createPlaylist(name.trim(), files);
            
            if (this.config.autoNotifications) {
                this.showSuccess(`Playlist "${name}" created`);
            }
            
            this.refreshView();
            
            return playlist;
            
        } catch (error) {
            this.logDebug('error', `Failed to create playlist: ${error.message}`);
            this.showError(`Failed to create playlist: ${error.message}`);
            return null;
        }
    }
    
    /**
     * Charge une playlist
     */
    async loadPlaylist(playlistId) {
        if (!playlistId) {
            this.showError('Invalid playlist ID');
            return null;
        }
        
        try {
            this.logDebug('playlist', `Loading playlist: ${playlistId}`);
            
            const playlist = await this.playlistModel.loadPlaylist(playlistId);
            
            this.state.currentPlaylist = playlist;
            
            if (this.config.autoNotifications) {
                this.showSuccess(`Playlist "${playlist.name}" loaded`);
            }
            
            this.refreshView();
            
            return playlist;
            
        } catch (error) {
            this.logDebug('error', `Failed to load playlist: ${error.message}`);
            this.showError(`Failed to load playlist: ${error.message}`);
            return null;
        }
    }
    
    /**
     * Met à jour une playlist
     */
    async updatePlaylist(playlistId, updates) {
        try {
            this.logDebug('playlist', `Updating playlist: ${playlistId}`);
            
            const playlist = await this.playlistModel.updatePlaylist(playlistId, updates);
            
            if (this.config.autoNotifications) {
                this.showSuccess('Playlist updated');
            }
            
            this.refreshView();
            
            return playlist;
            
        } catch (error) {
            this.logDebug('error', `Failed to update playlist: ${error.message}`);
            this.showError(`Failed to update playlist: ${error.message}`);
            return null;
        }
    }
    
    /**
     * Supprime une playlist
     */
    async deletePlaylist(playlistId) {
        if (this.config.confirmDelete) {
            const playlist = this.playlistModel.getPlaylist(playlistId);
            if (!playlist) return false;
            
            const confirmed = confirm(`Delete playlist "${playlist.name}"?`);
            if (!confirmed) return false;
        }
        
        try {
            this.logDebug('playlist', `Deleting playlist: ${playlistId}`);
            
            await this.playlistModel.deletePlaylist(playlistId);
            
            if (this.config.autoNotifications) {
                this.showSuccess('Playlist deleted');
            }
            
            // Si c'était la playlist courante, la décharger
            if (this.state.currentPlaylist?.id === playlistId) {
                this.state.currentPlaylist = null;
            }
            
            this.refreshView();
            
            return true;
            
        } catch (error) {
            this.logDebug('error', `Failed to delete playlist: ${error.message}`);
            this.showError(`Failed to delete playlist: ${error.message}`);
            return false;
        }
    }
    
    /**
     * Ajoute un fichier à une playlist
     */
    async addFileToPlaylist(playlistId, fileId) {
        try {
            this.logDebug('playlist', `Adding file ${fileId} to playlist ${playlistId}`);
            
            await this.playlistModel.addFile(playlistId, fileId);
            
            if (this.config.autoNotifications) {
                this.showSuccess('File added to playlist');
            }
            
            this.refreshView();
            
            return true;
            
        } catch (error) {
            this.logDebug('error', `Failed to add file: ${error.message}`);
            this.showError(`Failed to add file: ${error.message}`);
            return false;
        }
    }
    
    /**
     * Retire un fichier d'une playlist
     */
    async removeFileFromPlaylist(playlistId, fileId) {
        try {
            this.logDebug('playlist', `Removing file ${fileId} from playlist ${playlistId}`);
            
            await this.playlistModel.removeFile(playlistId, fileId);
            
            if (this.config.autoNotifications) {
                this.showSuccess('File removed from playlist');
            }
            
            this.refreshView();
            
            return true;
            
        } catch (error) {
            this.logDebug('error', `Failed to remove file: ${error.message}`);
            this.showError(`Failed to remove file: ${error.message}`);
            return false;
        }
    }
    
    // ========================================================================
    // NAVIGATION PLAYLIST
    // ========================================================================
    
    /**
     * Fichier suivant
     */
    async next() {
        if (!this.playlistModel) {
            this.showError('Playlist model not available');
            return null;
        }
        
        try {
            const nextFile = this.playlistModel.next();
            
            if (nextFile) {
                this.state.currentFile = nextFile;
                
                // Charger dans le playback si en cours de lecture
                if (this.state.isPlaying && this.playbackController) {
                    await this.playbackController.loadFile(nextFile.id);
                    await this.playbackController.play();
                }
                
                return nextFile;
            }
            
            return null;
            
        } catch (error) {
            this.logDebug('error', `Failed to go to next: ${error.message}`);
            this.showError('Failed to go to next file');
            return null;
        }
    }
    
    /**
     * Fichier précédent
     */
    async previous() {
        if (!this.playlistModel) {
            this.showError('Playlist model not available');
            return null;
        }
        
        try {
            const prevFile = this.playlistModel.previous();
            
            if (prevFile) {
                this.state.currentFile = prevFile;
                
                // Charger dans le playback si en cours de lecture
                if (this.state.isPlaying && this.playbackController) {
                    await this.playbackController.loadFile(prevFile.id);
                    await this.playbackController.play();
                }
                
                return prevFile;
            }
            
            return null;
            
        } catch (error) {
            this.logDebug('error', `Failed to go to previous: ${error.message}`);
            this.showError('Failed to go to previous file');
            return null;
        }
    }
    
    /**
     * Sauter à un index
     */
    async jumpTo(index) {
        if (!this.playlistModel) {
            this.showError('Playlist model not available');
            return null;
        }
        
        try {
            const file = this.playlistModel.jumpTo(index);
            
            if (file) {
                this.state.currentFile = file;
                
                // Charger dans le playback si en cours de lecture
                if (this.state.isPlaying && this.playbackController) {
                    await this.playbackController.loadFile(file.id);
                    await this.playbackController.play();
                }
                
                return file;
            }
            
            return null;
            
        } catch (error) {
            this.logDebug('error', `Failed to jump to index: ${error.message}`);
            this.showError('Failed to jump to file');
            return null;
        }
    }
    
    // ========================================================================
    // QUEUE MANAGEMENT - ✅ NOUVEAU
    // ========================================================================
    
    /**
     * Ajoute un fichier à la queue
     */
    addToQueue(fileId) {
        if (!this.playlistModel) {
            this.showError('Playlist model not available');
            return false;
        }
        
        const success = this.playlistModel.addToQueue(fileId);
        
        if (success) {
            this.logDebug('playlist', `File ${fileId} added to queue`);
            
            if (this.config.autoNotifications) {
                this.showSuccess('Added to queue');
            }
            
            this.refreshQueueView();
        } else {
            this.showError('Failed to add to queue (queue full?)');
        }
        
        return success;
    }
    
    /**
     * Ajoute plusieurs fichiers à la queue
     */
    addMultipleToQueue(fileIds) {
        if (!this.playlistModel) {
            this.showError('Playlist model not available');
            return [];
        }
        
        const added = this.playlistModel.addMultipleToQueue(fileIds);
        
        if (added.length > 0) {
            this.logDebug('playlist', `${added.length} files added to queue`);
            
            if (this.config.autoNotifications) {
                this.showSuccess(`${added.length} files added to queue`);
            }
            
            this.refreshQueueView();
        }
        
        return added;
    }
    
    /**
     * Retire un fichier de la queue
     */
    removeFromQueue(index) {
        if (!this.playlistModel) {
            this.showError('Playlist model not available');
            return false;
        }
        
        const success = this.playlistModel.removeFromQueue(index);
        
        if (success) {
            this.logDebug('playlist', `File removed from queue at index ${index}`);
            this.refreshQueueView();
        }
        
        return success;
    }
    
    /**
     * Vide la queue
     */
    clearQueue() {
        if (!this.playlistModel) {
            this.showError('Playlist model not available');
            return false;
        }
        
        this.playlistModel.clearQueue();
        
        this.logDebug('playlist', 'Queue cleared');
        
        if (this.config.autoNotifications) {
            this.showSuccess('Queue cleared');
        }
        
        this.refreshQueueView();
        
        return true;
    }
    
    /**
     * Joue la queue
     */
    async playQueue() {
        if (!this.playlistModel) {
            this.showError('Playlist model not available');
            return false;
        }
        
        try {
            const firstFile = this.playlistModel.playQueue();
            
            if (!firstFile) {
                this.showError('Queue is empty');
                return false;
            }
            
            this.state.currentFile = firstFile;
            
            // Charger et jouer
            if (this.playbackController) {
                await this.playbackController.loadFile(firstFile.id);
                await this.playbackController.play();
            }
            
            this.logDebug('playlist', 'Playing queue');
            
            if (this.config.autoNotifications) {
                this.showSuccess('Playing queue');
            }
            
            return true;
            
        } catch (error) {
            this.logDebug('error', `Failed to play queue: ${error.message}`);
            this.showError('Failed to play queue');
            return false;
        }
    }
    
    /**
     * Réordonne la queue (drag & drop)
     */
    reorderQueue(fromIndex, toIndex) {
        if (!this.playlistModel) {
            this.showError('Playlist model not available');
            return false;
        }
        
        const success = this.playlistModel.reorderQueue(fromIndex, toIndex);
        
        if (success) {
            this.logDebug('playlist', `Queue reordered: ${fromIndex} → ${toIndex}`);
            this.refreshQueueView();
        }
        
        return success;
    }
    
    /**
     * Affiche/masque la queue
     */
    toggleQueueVisibility() {
        this.state.queueVisible = !this.state.queueVisible;
        
        if (this.view && this.view.toggleQueue) {
            this.view.toggleQueue(this.state.queueVisible);
        }
        
        this.logDebug('playlist', `Queue ${this.state.queueVisible ? 'shown' : 'hidden'}`);
    }
    
    // ========================================================================
    // MODES DE LECTURE
    // ========================================================================
    
    /**
     * Active/désactive le shuffle
     */
    toggleShuffle() {
        if (!this.playlistModel) return false;
        
        const currentState = this.playlistModel.get('shuffleMode');
        this.playlistModel.setShuffle(!currentState);
        
        this.logDebug('playlist', `Shuffle ${!currentState ? 'ON' : 'OFF'}`);
        
        if (this.config.autoNotifications) {
            this.showSuccess(`Shuffle ${!currentState ? 'enabled' : 'disabled'}`);
        }
        
        return !currentState;
    }
    
    /**
     * Change le mode repeat
     */
    setRepeatMode(mode) {
        if (!this.playlistModel) return false;
        
        const validModes = ['none', 'one', 'all'];
        
        if (!validModes.includes(mode)) {
            this.showError(`Invalid repeat mode: ${mode}`);
            return false;
        }
        
        this.playlistModel.setRepeat(mode);
        
        this.logDebug('playlist', `Repeat mode: ${mode}`);
        
        if (this.config.autoNotifications) {
            const labels = { none: 'OFF', one: 'ONE', all: 'ALL' };
            this.showSuccess(`Repeat: ${labels[mode]}`);
        }
        
        return true;
    }
    
    /**
     * Cycle entre les modes repeat (none → one → all → none)
     */
    cycleRepeatMode() {
        if (!this.playlistModel) return;
        
        const current = this.playlistModel.get('repeatMode');
        const cycle = { none: 'one', one: 'all', all: 'none' };
        
        this.setRepeatMode(cycle[current]);
    }
    
    /**
     * Active/désactive l'auto-advance
     */
    toggleAutoAdvance() {
        if (!this.playlistModel) return false;
        
        const currentState = this.playlistModel.get('autoAdvance');
        this.playlistModel.setAutoAdvance(!currentState);
        
        this.logDebug('playlist', `Auto-advance ${!currentState ? 'ON' : 'OFF'}`);
        
        if (this.config.autoNotifications) {
            this.showSuccess(`Auto-advance ${!currentState ? 'enabled' : 'disabled'}`);
        }
        
        return !currentState;
    }
    
    // ========================================================================
    // CALLBACKS ÉVÉNEMENTS PLAYLISTMODEL
    // ========================================================================
    
    onPlaylistCreated(data) {
        this.logDebug('playlist', `Playlist created: ${data.playlist.name}`);
        
        if (this.config.autoRefreshView) {
            this.refreshView();
        }
    }
    
    onPlaylistLoaded(data) {
        this.logDebug('playlist', `Playlist loaded: ${data.playlist.name}`);
        this.state.currentPlaylist = data.playlist;
        
        if (this.config.autoRefreshView) {
            this.refreshView();
        }
    }
    
    onPlaylistUpdated(data) {
        this.logDebug('playlist', `Playlist updated: ${data.playlist.id}`);
        
        if (this.config.autoRefreshView) {
            this.refreshView();
        }
    }
    
    onPlaylistDeleted(data) {
        this.logDebug('playlist', `Playlist deleted: ${data.playlistId}`);
        
        if (this.config.autoRefreshView) {
            this.refreshView();
        }
    }
    
    onNext(data) {
        this.logDebug('playlist', `Next: ${data.file?.name || data.file?.id}`);
        this.state.currentFile = data.file;
        
        if (this.view && this.view.updateCurrentFile) {
            this.view.updateCurrentFile(data.file, data.index);
        }
    }
    
    onPrevious(data) {
        this.logDebug('playlist', `Previous: ${data.file?.name || data.file?.id}`);
        this.state.currentFile = data.file;
        
        if (this.view && this.view.updateCurrentFile) {
            this.view.updateCurrentFile(data.file, data.index);
        }
    }
    
    onJump(data) {
        this.logDebug('playlist', `Jump to: ${data.file?.name || data.file?.id}`);
        this.state.currentFile = data.file;
        
        if (this.view && this.view.updateCurrentFile) {
            this.view.updateCurrentFile(data.file, data.index);
        }
    }
    
    onPlaylistEnded() {
        this.logDebug('playlist', 'Playlist ended');
        
        if (this.config.autoNotifications) {
            this.showInfo('Playlist ended');
        }
    }
    
    /**
     * Auto-advance ✅ NOUVEAU
     */
    async onAutoAdvance(data) {
        this.logDebug('playlist', `Auto-advance to: ${data.file?.name || data.file?.id}`);
        
        this.state.currentFile = data.file;
        
        // Charger et jouer automatiquement
        if (this.playbackController) {
            try {
                await this.playbackController.loadFile(data.file.id);
                await this.playbackController.play();
                
                this.logDebug('playlist', '✓ Auto-advance completed');
                
            } catch (error) {
                this.logDebug('error', `Auto-advance failed: ${error.message}`);
                this.showError('Failed to auto-advance');
            }
        }
        
        if (this.view && this.view.updateCurrentFile) {
            this.view.updateCurrentFile(data.file, data.index);
        }
    }
  
  
    // ========================================================================
    // CALLBACKS QUEUE ✅ NOUVEAU
    // ========================================================================
    
    onQueueAdded(data) {
        this.logDebug('playlist', `Queue: +1 (total: ${data.queueSize})`);
        
        if (this.config.showQueueStatus && this.view && this.view.updateQueueStatus) {
            this.view.updateQueueStatus(data.queueSize);
        }
    }
    
    onQueueRemoved(data) {
        this.logDebug('playlist', `Queue: -1 (total: ${data.queueSize})`);
        
        if (this.config.showQueueStatus && this.view && this.view.updateQueueStatus) {
            this.view.updateQueueStatus(data.queueSize);
        }
    }
    
    onQueueCleared() {
        this.logDebug('playlist', 'Queue cleared');
        
        if (this.config.showQueueStatus && this.view && this.view.updateQueueStatus) {
            this.view.updateQueueStatus(0);
        }
    }
    
    onQueueStarted(data) {
        this.logDebug('playlist', `Queue started: ${data.file?.name || data.file?.id}`);
        
        if (this.view && this.view.showQueuePlaying) {
            this.view.showQueuePlaying(true);
        }
    }
    
    onQueueEnded() {
        this.logDebug('playlist', 'Queue ended');
        
        if (this.view && this.view.showQueuePlaying) {
            this.view.showQueuePlaying(false);
        }
        
        if (this.config.autoNotifications) {
            this.showInfo('Queue finished');
        }
    }
    
    // ========================================================================
    // CALLBACKS MODES
    // ========================================================================
    
    onShuffleChanged(data) {
        this.logDebug('playlist', `Shuffle: ${data.enabled ? 'ON' : 'OFF'}`);
        
        if (this.view && this.view.updateShuffleButton) {
            this.view.updateShuffleButton(data.enabled);
        }
    }
    
    onRepeatChanged(data) {
        this.logDebug('playlist', `Repeat: ${data.mode}`);
        
        if (this.view && this.view.updateRepeatButton) {
            this.view.updateRepeatButton(data.mode);
        }
    }
    
    onAutoAdvanceChanged(data) {
        this.logDebug('playlist', `Auto-advance: ${data.enabled ? 'ON' : 'OFF'}`);
        
        if (this.view && this.view.updateAutoAdvanceButton) {
            this.view.updateAutoAdvanceButton(data.enabled);
        }
    }
    
    // ========================================================================
    // CALLBACKS PLAYBACK (Coordination)
    // ========================================================================
    
    onPlaybackStarted() {
        this.state.isPlaying = true;
        this.logDebug('playlist', 'Playback started');
    }
    
    onPlaybackStopped() {
        this.state.isPlaying = false;
        this.logDebug('playlist', 'Playback stopped');
    }
    
    /**
     * Fin de lecture d'un fichier - déclenche auto-advance
     */
    onPlaybackFinished() {
        this.logDebug('playlist', 'Playback finished - triggering auto-advance check');
        
        // Le PlaylistModel écoute déjà 'playback:finished' et gère l'auto-advance
        // Ici on peut juste mettre à jour l'UI si nécessaire
        
        if (this.view && this.view.showPlaybackComplete) {
            this.view.showPlaybackComplete();
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Rafraîchit la vue complète
     */
    refreshView() {
        if (!this.view) return;
        
        const data = {
            playlists: this.playlistModel.get('playlists'),
            currentPlaylist: this.state.currentPlaylist,
            currentFile: this.state.currentFile,
            shuffleMode: this.playlistModel.get('shuffleMode'),
            repeatMode: this.playlistModel.get('repeatMode'),
            autoAdvance: this.playlistModel.get('autoAdvance'),
            queueSize: this.playlistModel.get('queue').length,
            isPlaying: this.state.isPlaying
        };
        
        if (this.view.render) {
            this.view.render(data);
        }
        
        this.logDebug('playlist', '✓ View refreshed');
    }
    
    /**
     * Rafraîchit uniquement la vue de la queue
     */
    refreshQueueView() {
        if (!this.view || !this.view.renderQueue) return;
        
        const queue = this.playlistModel.get('queue');
        const queueIndex = this.playlistModel.get('queueIndex');
        const isPlayingQueue = this.playlistModel.get('isPlayingQueue');
        
        this.view.renderQueue({
            queue,
            queueIndex,
            isPlayingQueue
        });
    }
    
    /**
     * Sauvegarde l'état
     */
    saveState() {
        if (!this.playlistModel) return;
        
        const state = {
            currentPlaylistId: this.state.currentPlaylist?.id || null,
            shuffleMode: this.playlistModel.get('shuffleMode'),
            repeatMode: this.playlistModel.get('repeatMode'),
            autoAdvance: this.playlistModel.get('autoAdvance')
        };
        
        try {
            localStorage.setItem('midiMind_playlistState', JSON.stringify(state));
            this.logDebug('playlist', '✓ State saved');
        } catch (error) {
            this.logDebug('error', `Failed to save state: ${error.message}`);
        }
    }
    
    /**
     * Charge l'état sauvegardé
     */
    loadSavedState() {
        try {
            const saved = localStorage.getItem('midiMind_playlistState');
            if (!saved) return;
            
            const state = JSON.parse(saved);
            
            // Restaurer les modes
            if (this.playlistModel) {
                if (state.shuffleMode !== undefined) {
                    this.playlistModel.setShuffle(state.shuffleMode);
                }
                if (state.repeatMode) {
                    this.playlistModel.setRepeat(state.repeatMode);
                }
                if (state.autoAdvance !== undefined) {
                    this.playlistModel.setAutoAdvance(state.autoAdvance);
                }
                
                // Charger la playlist si nécessaire
                if (state.currentPlaylistId) {
                    this.loadPlaylist(state.currentPlaylistId).catch(() => {});
                }
            }
            
            this.logDebug('playlist', '✓ State restored');
            
        } catch (error) {
            this.logDebug('error', `Failed to load state: ${error.message}`);
        }
    }
    
    /**
     * Retourne les statistiques de la playlist
     */
    getStats() {
        if (!this.playlistModel) return null;
        
        const stats = this.playlistModel.getQueueStats();
        
        return {
            ...stats,
            currentPlaylist: this.state.currentPlaylist?.name || null,
            currentFile: this.state.currentFile?.name || null,
            isPlaying: this.state.isPlaying
        };
    }
    
    /**
     * Retourne l'état complet
     */
    getState() {
        return {
            ...this.state,
            shuffleMode: this.playlistModel?.get('shuffleMode') || false,
            repeatMode: this.playlistModel?.get('repeatMode') || 'none',
            autoAdvance: this.playlistModel?.get('autoAdvance') || true,
            queueSize: this.playlistModel?.get('queue').length || 0,
            isPlayingQueue: this.playlistModel?.get('isPlayingQueue') || false
        };
    }
    
    // =============================================
    // EXPORT
    // =============================================
    
    /**
     * Exporte une playlist
     * @param {string} playlistId - ID playlist
     * @param {string} format - 'json'|'m3u'|'m3u8'|'pls'|'xspf'
     * @returns {string} Contenu exporté
     */
    export(playlistId, format = 'json') {
        const playlist = this.controller.playlistModel.getPlaylistById(playlistId);
        
        if (!playlist) {
            throw new Error('Playlist not found');
        }
        
        switch (format.toLowerCase()) {
            case 'json':
                return this.exportJSON(playlist);
            case 'm3u':
                return this.exportM3U(playlist, false);
            case 'm3u8':
                return this.exportM3U(playlist, true);
            case 'pls':
                return this.exportPLS(playlist);
            case 'xspf':
                return this.exportXSPF(playlist);
            default:
                throw new Error(`Unsupported format: ${format}`);
        }
    }
    
    /**
     * Export JSON
     */
    exportJSON(playlist) {
        const data = {
            name: playlist.name,
            description: playlist.description || '',
            created: playlist.created,
            modified: playlist.modified || Date.now(),
            files: playlist.files.map(f => ({
                path: f.path,
                title: f.title || f.name,
                duration: f.duration || 0
            }))
        };
        
        return JSON.stringify(data, null, 2);
    }
    
    /**
     * Export M3U/M3U8
     */
    exportM3U(playlist, extended = false) {
        let content = extended ? '#EXTM3U\n' : '';
        
        for (const file of playlist.files) {
            if (extended) {
                const duration = Math.round((file.duration || 0) / 1000);
                const title = file.title || file.name;
                content += `#EXTINF:${duration},${title}\n`;
            }
            content += `${file.path}\n`;
        }
        
        return content;
    }
    
    /**
     * Export PLS
     */
    exportPLS(playlist) {
        let content = '[playlist]\n';
        content += `NumberOfEntries=${playlist.files.length}\n\n`;
        
        playlist.files.forEach((file, index) => {
            const num = index + 1;
            content += `File${num}=${file.path}\n`;
            content += `Title${num}=${file.title || file.name}\n`;
            content += `Length${num}=${Math.round((file.duration || 0) / 1000)}\n\n`;
        });
        
        content += 'Version=2\n';
        return content;
    }
    
    /**
     * Export XSPF
     */
    exportXSPF(playlist) {
        const xml = [];
        xml.push('<?xml version="1.0" encoding="UTF-8"?>');
        xml.push('<playlist version="1" xmlns="http://xspf.org/ns/0/">');
        xml.push(`  <title>${this.escapeXml(playlist.name)}</title>`);
        xml.push('  <trackList>');
        
        for (const file of playlist.files) {
            xml.push('    <track>');
            xml.push(`      <location>${this.escapeXml(file.path)}</location>`);
            xml.push(`      <title>${this.escapeXml(file.title || file.name)}</title>`);
            xml.push(`      <duration>${file.duration || 0}</duration>`);
            xml.push('    </track>');
        }
        
        xml.push('  </trackList>');
        xml.push('</playlist>');
        
        return xml.join('\n');
    }
    
    // =============================================
    // DOWNLOAD
    // =============================================
    
    /**
     * Télécharge une playlist
     * @param {string} playlistId - ID playlist
     * @param {string} format - Format d'export
     */
    download(playlistId, format = 'json') {
        const content = this.export(playlistId, format);
        const playlist = this.controller.playlistModel.getPlaylistById(playlistId);
        
        // Créer blob
        const blob = new Blob([content], { type: this.getMimeType(format) });
        const url = URL.createObjectURL(blob);
        
        // Créer lien download
        const a = document.createElement('a');
        a.href = url;
        a.download = `${playlist.name}.${format}`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        
        // Libérer URL
        setTimeout(() => URL.revokeObjectURL(url), 100);
        
        this.logger.info('PlaylistExport', `Downloaded: ${playlist.name}.${format}`);
    }
    
    // =============================================
    // HELPERS
    // =============================================
    
    getMimeType(format) {
        const types = {
            'json': 'application/json',
            'm3u': 'audio/x-mpegurl',
            'm3u8': 'application/vnd.apple.mpegurl',
            'pls': 'audio/x-scpls',
            'xspf': 'application/xspf+xml'
        };
        return types[format] || 'text/plain';
    }
    
    escapeXml(str) {
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&apos;');
    }




// ========================================================================
// NETTOYAGE
// ========================================================================

    /**
     * Nettoie les ressources
     */
    destroy() {
        this.logDebug('playlist', 'Destroying PlaylistController...');
        
        // Sauvegarder l'état
        this.saveState();
        
        // Nettoyer les références
        this.playbackController = null;
        this.state.currentPlaylist = null;
        this.state.currentFile = null;
        
        this.logDebug('playlist', '✓ PlaylistController destroyed');
    }

}


	
	





// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaylistController;
}

if (typeof window !== 'undefined') {
    window.PlaylistController = PlaylistController;
}
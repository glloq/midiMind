// ============================================================================
// Fichier: frontend/js/models/PlaylistModel.js
// Version: v3.0.2 - COMPLET (Auto-advance + Queue)
// Date: 2025-10-08
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Modèle gérant les playlists de fichiers MIDI.
//   Gère l'ordre, shuffle, repeat et auto-advance.
//
// CORRECTIONS v3.0.2:
//   ✅ Auto-advance avec événement playback:finished
//   ✅ Queue management (add to queue, clear queue)
//   ✅ Gestion transitions entre fichiers
//   ✅ Historique des fichiers joués
//   ✅ Smart shuffle (évite répétitions)
//
// Auteur: midiMind Team
// ============================================================================

class PlaylistModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, logger);
        
        // Dépendances
        this.backend = backend;
        this.logger = logger;
        
        // Initialiser
        this.initialize({
            // Playlists sauvegardées
            playlists: [],
            
            // Playlist courante
            currentPlaylist: null,
            currentIndex: 0,
            
            // Modes de lecture
            shuffleMode: false,
            repeatMode: 'none',    // none, one, all
            autoAdvance: true,     // ✅ NOUVEAU
            
            // Queue temporaire - ✅ NOUVEAU
            queue: [],
            queueIndex: 0,
            isPlayingQueue: false,
            
            // Ordre shuffle
            shuffleOrder: [],
            
            // Historique - ✅ NOUVEAU
            history: [],
            maxHistorySize: 50
        });
        
        // Configuration
        this.config = {
            maxPlaylists: 50,
            maxQueueSize: 100,
            maxHistorySize: 50,
            smartShuffle: true,        // Évite répétitions
            shuffleMemorySize: 10      // Évite les X derniers
        };
        
        this.logger.info('PlaylistModel', '✓ Model initialized with auto-advance');
        
        this.attachPlaybackEvents();
    }
	
	
	/**
     * Initialise les données du modèle
     * @param {Object} data - Données initiales
     * @returns {void}
     */
    initialize(data = {}) {
        // Fusionner avec les données existantes
        this.data = { ...this.data, ...data };
        
        // Marquer comme initialisé
        this.meta.initialized = true;
        this.meta.lastModified = Date.now();
        
        if (this.logger) {
            this.logger.debug(
                `${this.constructor.name}`,
                `Data initialized with ${Object.keys(data).length} keys`
            );
        }
    }
    
    // ========================================================================
    // ÉVÉNEMENTS PLAYBACK - ✅ NOUVEAU
    // ========================================================================
    
    attachPlaybackEvents() {
        // Écouter la fin de lecture pour auto-advance
        this.eventBus.on('playback:finished', () => {
            this.handlePlaybackFinished();
        });
        
        // Écouter les changements de fichier
        this.eventBus.on('playback:file-loaded', (data) => {
            this.addToHistory(data.fileId);
        });
    }
    
    /**
     * Gère la fin de lecture d'un fichier - ✅ NOUVEAU
     */
    handlePlaybackFinished() {
        if (!this.get('autoAdvance')) {
            this.logger.debug('PlaylistModel', 'Auto-advance disabled');
            return;
        }
        
        // Si on joue la queue, continuer dans la queue
        if (this.get('isPlayingQueue')) {
            this.nextInQueue();
            return;
        }
        
        // Sinon, suivant dans la playlist
        const nextFile = this.next();
        
        if (nextFile) {
            this.logger.info('PlaylistModel', `Auto-advancing to: ${nextFile.name || nextFile.id}`);
            
            this.eventBus.emit('playlist:auto-advance', {
                file: nextFile,
                index: this.get('currentIndex')
            });
        } else {
            this.logger.debug('PlaylistModel', 'No more files to play');
            this.eventBus.emit('playlist:ended');
        }
    }
    
    // ========================================================================
    // GESTION PLAYLISTS
    // ========================================================================
    
    async createPlaylist(name, files = []) {
        const playlists = this.get('playlists');
        
        if (playlists.length >= this.config.maxPlaylists) {
            throw new Error('Maximum playlists reached');
        }
        
        const playlist = {
            id: `playlist_${Date.now()}`,
            name: name,
            files: files,
            createdAt: Date.now(),
            updatedAt: Date.now(),
            duration: this._calculateTotalDuration(files)
        };
        
        playlists.push(playlist);
        this.set('playlists', playlists);
        
        this.logger.info('PlaylistModel', `Playlist created: ${name}`);
        
        this.eventBus.emit('playlist:created', { playlist });
        
        return playlist;
    }
    
    async loadPlaylist(playlistId) {
        const playlists = this.get('playlists');
        const playlist = playlists.find(p => p.id === playlistId);
        
        if (!playlist) {
            throw new Error(`Playlist not found: ${playlistId}`);
        }
        
        this.set('currentPlaylist', playlist);
        this.set('currentIndex', 0);
        this.set('isPlayingQueue', false);
        
        if (this.get('shuffleMode')) {
            this.generateShuffleOrder();
        }
        
        this.logger.info('PlaylistModel', `Playlist loaded: ${playlist.name}`);
        
        this.eventBus.emit('playlist:loaded', { playlist });
        
        return playlist;
    }
    
    async updatePlaylist(playlistId, updates) {
        const playlists = this.get('playlists');
        const index = playlists.findIndex(p => p.id === playlistId);
        
        if (index === -1) {
            throw new Error(`Playlist not found: ${playlistId}`);
        }
        
        const playlist = playlists[index];
        
        Object.assign(playlist, updates, {
            updatedAt: Date.now()
        });
        
        // Recalculer durée si fichiers modifiés
        if (updates.files) {
            playlist.duration = this._calculateTotalDuration(playlist.files);
        }
        
        playlists[index] = playlist;
        this.set('playlists', playlists);
        
        this.eventBus.emit('playlist:updated', { playlist });
        
        return playlist;
    }
    
    async deletePlaylist(playlistId) {
        const playlists = this.get('playlists');
        const index = playlists.findIndex(p => p.id === playlistId);
        
        if (index === -1) {
            return false;
        }
        
        playlists.splice(index, 1);
        this.set('playlists', playlists);
        
        // Si c'était la playlist courante, la désélectionner
        const current = this.get('currentPlaylist');
        if (current && current.id === playlistId) {
            this.set('currentPlaylist', null);
            this.set('currentIndex', 0);
        }
        
        this.eventBus.emit('playlist:deleted', { playlistId });
        
        return true;
    }
    
    // ========================================================================
    // NAVIGATION DANS LA PLAYLIST
    // ========================================================================
    
    getCurrentFile() {
        const playlist = this.get('currentPlaylist');
        
        if (!playlist || playlist.files.length === 0) {
            return null;
        }
        
        const index = this.get('shuffleMode') 
            ? this.get('shuffleOrder')[this.get('currentIndex')]
            : this.get('currentIndex');
        
        return playlist.files[index];
    }
    
    next() {
        // Si on joue la queue, utiliser nextInQueue
        if (this.get('isPlayingQueue')) {
            return this.nextInQueue();
        }
        
        const playlist = this.get('currentPlaylist');
        
        if (!playlist || playlist.files.length === 0) {
            return null;
        }
        
        const repeatMode = this.get('repeatMode');
        
        // Mode repeat one : rester sur le même
        if (repeatMode === 'one') {
            const file = this.getCurrentFile();
            this.eventBus.emit('playlist:next', { file, repeat: true });
            return file;
        }
        
        // Incrémenter index
        let newIndex = this.get('currentIndex') + 1;
        
        // Fin de playlist atteinte
        if (newIndex >= playlist.files.length) {
            if (repeatMode === 'all') {
                // Recommencer au début
                newIndex = 0;
                
                // Regénérer shuffle si actif
                if (this.get('shuffleMode')) {
                    this.generateShuffleOrder();
                }
                
                this.logger.debug('PlaylistModel', 'Playlist restarted (repeat all)');
            } else {
                // Fin de playlist
                this.logger.debug('PlaylistModel', 'Playlist ended');
                this.eventBus.emit('playlist:ended');
                return null;
            }
        }
        
        this.set('currentIndex', newIndex);
        
        const file = this.getCurrentFile();
        
        this.eventBus.emit('playlist:next', { 
            file, 
            index: newIndex,
            total: playlist.files.length
        });
        
        return file;
    }
    
    previous() {
        const playlist = this.get('currentPlaylist');
        
        if (!playlist || playlist.files.length === 0) {
            return null;
        }
        
        // Décrémenter index
        let newIndex = this.get('currentIndex') - 1;
        
        // Début de playlist atteint
        if (newIndex < 0) {
            if (this.get('repeatMode') === 'all') {
                newIndex = playlist.files.length - 1;
            } else {
                newIndex = 0;
            }
        }
        
        this.set('currentIndex', newIndex);
        
        const file = this.getCurrentFile();
        
        this.eventBus.emit('playlist:previous', { 
            file, 
            index: newIndex,
            total: playlist.files.length
        });
        
        return file;
    }
    
    jumpTo(index) {
        const playlist = this.get('currentPlaylist');
        
        if (!playlist || index < 0 || index >= playlist.files.length) {
            return null;
        }
        
        this.set('currentIndex', index);
        
        const file = this.getCurrentFile();
        
        this.eventBus.emit('playlist:jump', { file, index });
        
        return file;
    }
    
    // ========================================================================
    // MODES DE LECTURE
    // ========================================================================
    
    setShuffle(enabled) {
        this.set('shuffleMode', enabled);
        
        if (enabled) {
            this.generateShuffleOrder();
        }
        
        this.logger.info('PlaylistModel', `Shuffle ${enabled ? 'ON' : 'OFF'}`);
        
        this.eventBus.emit('playlist:shuffle-changed', { enabled });
    }
    
    setRepeat(mode) {
        const validModes = ['none', 'one', 'all'];
        
        if (!validModes.includes(mode)) {
            this.logger.warn('PlaylistModel', `Invalid repeat mode: ${mode}`);
            return;
        }
        
        this.set('repeatMode', mode);
        
        this.logger.info('PlaylistModel', `Repeat mode: ${mode}`);
        
        this.eventBus.emit('playlist:repeat-changed', { mode });
    }
    
    setAutoAdvance(enabled) {
        this.set('autoAdvance', enabled);
        
        this.logger.info('PlaylistModel', `Auto-advance ${enabled ? 'ON' : 'OFF'}`);
        
        this.eventBus.emit('playlist:auto-advance-changed', { enabled });
    }
    
    // ========================================================================
    // SHUFFLE - ✅ AMÉLIORÉ (Smart Shuffle)
    // ========================================================================
    
    generateShuffleOrder() {
        const playlist = this.get('currentPlaylist');
        
        if (!playlist) return;
        
        const length = playlist.files.length;
        this.set('shuffleOrder', this._generateSmartShuffle(length));
        
        this.logger.debug('PlaylistModel', 'Shuffle order generated');
    }
    
    _generateSmartShuffle(length) {
        const order = Array.from({ length }, (_, i) => i);
        
        if (!this.config.smartShuffle) {
            // Fisher-Yates shuffle classique
            for (let i = length - 1; i > 0; i--) {
                const j = Math.floor(Math.random() * (i + 1));
                [order[i], order[j]] = [order[j], order[i]];
            }
            return order;
        }
        
        // Smart shuffle : évite les répétitions récentes
        const history = this.get('history').slice(-this.config.shuffleMemorySize);
        const recentIndices = new Set(history);
        
        // Séparer en "available" et "recent"
        const available = order.filter(i => !recentIndices.has(i));
        const recent = order.filter(i => recentIndices.has(i));
        
        // Shuffle available
        for (let i = available.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [available[i], available[j]] = [available[j], available[i]];
        }
        
        // Shuffle recent
        for (let i = recent.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [recent[i], recent[j]] = [recent[j], recent[i]];
        }
        
        // Placer les recent à la fin
        return [...available, ...recent];
    }
    
    // ========================================================================
    // QUEUE MANAGEMENT - ✅ NOUVEAU
    // ========================================================================
    
    /**
     * Ajoute un fichier à la queue
     */
    addToQueue(fileId) {
        const queue = this.get('queue');
        
        if (queue.length >= this.config.maxQueueSize) {
            this.logger.warn('PlaylistModel', 'Queue is full');
            return false;
        }
        
        queue.push(fileId);
        this.set('queue', queue);
        
        this.logger.info('PlaylistModel', `Added to queue: ${fileId}`);
        
        this.eventBus.emit('playlist:queue-added', { 
            fileId, 
            queueSize: queue.length 
        });
        
        return true;
    }
    
    /**
     * Ajoute plusieurs fichiers à la queue
     */
    addMultipleToQueue(fileIds) {
        const added = [];
        
        for (const fileId of fileIds) {
            if (this.addToQueue(fileId)) {
                added.push(fileId);
            }
        }
        
        return added;
    }
    
    /**
     * Retire un fichier de la queue
     */
    removeFromQueue(index) {
        const queue = this.get('queue');
        
        if (index < 0 || index >= queue.length) {
            return false;
        }
        
        const removed = queue.splice(index, 1)[0];
        this.set('queue', queue);
        
        this.eventBus.emit('playlist:queue-removed', { 
            fileId: removed, 
            index,
            queueSize: queue.length 
        });
        
        return true;
    }
    
    /**
     * Vide la queue
     */
    clearQueue() {
        this.set('queue', []);
        this.set('queueIndex', 0);
        this.set('isPlayingQueue', false);
        
        this.logger.info('PlaylistModel', 'Queue cleared');
        
        this.eventBus.emit('playlist:queue-cleared');
    }
    
    /**
     * Commence à jouer la queue
     */
    playQueue() {
        const queue = this.get('queue');
        
        if (queue.length === 0) {
            this.logger.warn('PlaylistModel', 'Queue is empty');
            return null;
        }
        
        this.set('isPlayingQueue', true);
        this.set('queueIndex', 0);
        
        const fileId = queue[0];
        
        this.logger.info('PlaylistModel', 'Started playing queue');
        
        this.eventBus.emit('playlist:queue-started', { fileId });
        
        return fileId;
    }
    
    /**
     * Fichier suivant dans la queue
     */
    nextInQueue() {
        const queue = this.get('queue');
        let queueIndex = this.get('queueIndex');
        
        if (queue.length === 0) {
            // Queue vide, retour à la playlist
            this.set('isPlayingQueue', false);
            return this.next();
        }
        
        queueIndex++;
        
        if (queueIndex >= queue.length) {
            // Fin de queue
            this.set('isPlayingQueue', false);
            this.clearQueue();
            
            this.logger.info('PlaylistModel', 'Queue finished, returning to playlist');
            
            // Continuer avec la playlist
            return this.next();
        }
        
        this.set('queueIndex', queueIndex);
        
        const fileId = queue[queueIndex];
        
        this.eventBus.emit('playlist:queue-next', { 
            fileId, 
            index: queueIndex,
            remaining: queue.length - queueIndex - 1
        });
        
        return fileId;
    }
    
    getQueue() {
        return this.get('queue');
    }
    
    getQueueSize() {
        return this.get('queue').length;
    }
    
    // ========================================================================
    // HISTORIQUE - ✅ NOUVEAU
    // ========================================================================
    
    addToHistory(fileId) {
        const history = this.get('history');
        
        history.push(fileId);
        
        // Limiter taille
        if (history.length > this.config.maxHistorySize) {
            history.shift();
        }
        
        this.set('history', history, { silent: true });
    }
    
    getHistory() {
        return this.get('history');
    }
    
    clearHistory() {
        this.set('history', []);
        this.eventBus.emit('playlist:history-cleared');
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    _calculateTotalDuration(files) {
        return files.reduce((sum, file) => {
            return sum + (file.duration || 0);
        }, 0);
    }
    
    getAllPlaylists() {
        return this.get('playlists');
    }
    
    getCurrentPlaylist() {
        return this.get('currentPlaylist');
    }
    
    getPlaylistInfo() {
        const playlist = this.get('currentPlaylist');
        
        if (!playlist) return null;
        
        return {
            ...playlist,
            currentIndex: this.get('currentIndex'),
            currentFile: this.getCurrentFile(),
            shuffleMode: this.get('shuffleMode'),
            repeatMode: this.get('repeatMode'),
            autoAdvance: this.get('autoAdvance'),
            queueSize: this.getQueueSize(),
            isPlayingQueue: this.get('isPlayingQueue')
        };
    }
    
    getStats() {
        return {
            totalPlaylists: this.get('playlists').length,
            currentPlaylistSize: this.get('currentPlaylist')?.files.length || 0,
            queueSize: this.getQueueSize(),
            historySize: this.get('history').length,
            shuffleMode: this.get('shuffleMode'),
            repeatMode: this.get('repeatMode'),
            autoAdvance: this.get('autoAdvance')
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaylistModel;
}

if (typeof window !== 'undefined') {
    window.PlaylistModel = PlaylistModel;
}
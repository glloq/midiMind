// ============================================================================
// Fichier: frontend/js/controllers/GlobalPlaybackController.js
// Chemin rÃƒÆ’Ã‚Â©el: frontend/js/controllers/GlobalPlaybackController.js
// Version: v3.3.0 - API BACKEND CORRIGÃƒÆ’Ã¢â‚¬Â°E
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.3.0:
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ CRITIQUE: Utilise sendCommand() au lieu de send()
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Format API cohÃƒÆ’Ã‚Â©rent avec documentation backend
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Gestion ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements temps rÃƒÆ’Ã‚Â©el amÃƒÆ’Ã‚Â©liorÃƒÆ’Ã‚Â©e
// âœ… CORRECTIONS v4.0.0: CompatibilitÃ© API v4.0.0
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Synchronisation backend optimisÃƒÆ’Ã‚Â©e
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Gestion erreurs robuste
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Support ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements playback_position, playback_finished
// ============================================================================

class GlobalPlaybackController {
    static instance = null;
    
    static getInstance(eventBus, backend, fileModel, logger) {
        if (!GlobalPlaybackController.instance) {
            GlobalPlaybackController.instance = new GlobalPlaybackController(
                eventBus, backend, fileModel, logger
            );
        }
        return GlobalPlaybackController.instance;
    }
    
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        if (GlobalPlaybackController.instance) {
            return GlobalPlaybackController.instance;
        }
        
        this.eventBus = eventBus || window.eventBus;
        this.backend = backend || window.backendService;
        this.fileModel = fileModel;
        this.logger = logger || this.createFallbackLogger();
        
        // CrÃƒÆ’Ã‚Â©er le modÃƒÆ’Ã‚Â¨le de playback
        this.playbackModel = new PlaybackModel(eventBus, backend, this.logger);
        this.playbackModel.config.interpolationEnabled = true;
        
        // Fichier actuel
        this.currentFile = {
            id: null,
            name: null,
            path: null,
            midiJson: null,
            metadata: {},
            duration: 0
        };
        
        // Playlist
        this.playlist = {
            files: [],
            currentIndex: -1,
            shuffle: false,
            repeat: 'none', // none, one, all
            autoAdvance: true
        };
        
        // Routing MIDI
        this.routing = new Map();
        
        // MÃƒÆ’Ã‚Â©tronome
        this.metronome = {
            enabled: false,
            volume: 80,
            accentFirst: true,
            soundType: 'beep'
        };
        
        // Cache de fichiers
        this.cache = {
            preloadedFiles: new Map(),
            maxCacheSize: 10,
            enabled: true
        };
        
        // Statistiques
        this.stats = {
            filesPlayed: 0,
            totalPlaytime: 0,
            seeks: 0,
            tempoChanges: 0,
            startTime: null,
            lastUpdateTime: 0
        };
        
        // Configuration
        this.config = {
            latencyCompensation: 0,
            velocityCurve: 'linear',
            autoPreload: true,
            syncInterval: 1000,
            enableBackendSync: true
        };
        
        // ÃƒÆ’Ã¢â‚¬Â°tat interne
        this.state = {
            isPlaying: false,
            isPaused: false,
            currentPosition: 0,
            duration: 0,
            tempo: 120,
            volume: 100
        };
        
        this.log('info', 'GlobalPlaybackController', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Initialized v3.3.0');
        
        this.connectPlaybackModelEvents();
        this.connectBackendEvents();
        this.setupBackendSync();
        
        GlobalPlaybackController.instance = this;
    }
    
    createFallbackLogger() {
        return {
            debug: (...args) => console.log('[DEBUG]', ...args),
            info: (...args) => console.log('[INFO]', ...args),
            warn: (...args) => console.warn('[WARN]', ...args),
            error: (...args) => console.error('[ERROR]', ...args),
            log: (...args) => console.log(...args)
        };
    }
    
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        } else {
            console[level]?.(...args) || console.log(...args);
        }
    }
    
    // ========================================================================
    // ÃƒÆ’Ã¢â‚¬Â°VÃƒÆ’Ã¢â‚¬Â°NEMENTS
    // ========================================================================
    
    /**
     * Connecte les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements du modÃƒÆ’Ã‚Â¨le de playback
     */
    connectPlaybackModelEvents() {
        if (!this.eventBus) return;
        
        this.eventBus.on('playback:stateChanged', (data) => {
            this.state.isPlaying = data.newState === 'playing';
            this.state.isPaused = data.newState === 'paused';
            this.log('debug', 'GlobalPlayback', 'State changed:', data.newState);
            this.eventBus.emit('globalPlayback:stateChanged', data);
        });
        
        this.eventBus.on('playback:timeUpdate', (data) => {
            this.state.currentPosition = data.position;
            this.stats.lastUpdateTime = Date.now();
            this.eventBus.emit('globalPlayback:timeUpdate', data);
        });
        
        this.eventBus.on('playback:ended', () => {
            this.handlePlaybackEnded();
        });
        
        this.eventBus.on('playback:error', (data) => {
            this.log('error', 'GlobalPlayback', 'Playback error:', data);
            this.eventBus.emit('globalPlayback:error', data);
        });
    }
    
    /**
     * Connecte les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements du backend
     */
    connectBackendEvents() {
        if (!this.eventBus) return;
        
        // ÃƒÆ’Ã¢â‚¬Â°vÃƒÆ’Ã‚Â©nement playback_position du backend
        this.eventBus.on('backend:event:playback_position', (data) => {
            this.handleBackendPosition(data);
        });
        
        // ÃƒÆ’Ã¢â‚¬Â°vÃƒÆ’Ã‚Â©nement playback_finished du backend
        this.eventBus.on('backend:event:playback_finished', (data) => {
            this.handlePlaybackEnded();
        });
        
        // ÃƒÆ’Ã¢â‚¬Â°vÃƒÆ’Ã‚Â©nement playback_started du backend
        this.eventBus.on('backend:event:playback_started', (data) => {
            this.state.isPlaying = true;
            this.state.isPaused = false;
            this.stats.startTime = Date.now();
        });
        
        // ÃƒÆ’Ã¢â‚¬Â°vÃƒÆ’Ã‚Â©nement playback_paused du backend
        this.eventBus.on('backend:event:playback_paused', (data) => {
            this.state.isPlaying = false;
            this.state.isPaused = true;
        });
        
        // ÃƒÆ’Ã¢â‚¬Â°vÃƒÆ’Ã‚Â©nement playback_stopped du backend
        this.eventBus.on('backend:event:playback_stopped', (data) => {
            this.state.isPlaying = false;
            this.state.isPaused = false;
            this.state.currentPosition = 0;
        });
    }
    
    /**
     * GÃƒÆ’Ã‚Â¨re la mise ÃƒÆ’Ã‚Â  jour de position depuis le backend
     */
    handleBackendPosition(data) {
        if (data.position !== undefined) {
            this.state.currentPosition = data.position;
            this.playbackModel.set('currentTime', data.position);
        }
        
        if (data.duration !== undefined) {
            this.state.duration = data.duration;
            this.playbackModel.set('duration', data.duration);
        }
        
        this.eventBus.emit('globalPlayback:position', {
            position: this.state.currentPosition,
            duration: this.state.duration
        });
    }
    
    /**
     * Configure la synchronisation avec le backend
     */
    setupBackendSync() {
        if (!this.backend || typeof this.backend.isConnected !== 'function') {
            this.log('warn', 'GlobalPlayback', 'Backend not available for sync');
            return;
        }
        
        if (!this.backend.isConnected()) {
            this.log('warn', 'GlobalPlayback', 'Backend not connected, waiting...');
            this.eventBus.once('backend:connected', () => {
                this.setupBackendSync();
            });
            return;
        }
        
        // Synchronisation pÃƒÆ’Ã‚Â©riodique
        if (this.config.enableBackendSync) {
            setInterval(() => {
                if (this.state.isPlaying && this.backend.isConnected()) {
                    this.syncWithBackend();
                }
            }, this.config.syncInterval);
        }
    }
    
    /**
     * Synchronise l'ÃƒÆ’Ã‚Â©tat avec le backend
     */
    async syncWithBackend() {
        if (!this.backend || !this.backend.isConnected()) {
            return;
        }
        
        try {
            // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ NOUVEAU: Utilise sendCommand() avec format API correct
            const response = await this.backend.sendCommand('playback.getStatus');
            
            if (response && response.status) {
                // Mettre ÃƒÆ’Ã‚Â  jour l'ÃƒÆ’Ã‚Â©tat local
                this.state.currentPosition = response.status.position || 0;
                this.state.duration = response.status.duration || 0;
                this.state.tempo = response.status.tempo || 120;
                
                // Mettre ÃƒÆ’Ã‚Â  jour le modÃƒÆ’Ã‚Â¨le
                this.playbackModel.updateFromBackend(response.status);
            }
        } catch (error) {
            this.log('error', 'GlobalPlayback', 'Sync error:', error.message);
        }
    }
    
    // ========================================================================
    // COMMANDES DE PLAYBACK
    // ========================================================================
    
    /**
     * Charge un fichier MIDI
     */
    async load(filename) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('error', 'GlobalPlayback', 'Backend not connected');
            return false;
        }
        
        try {
            // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ NOUVEAU: Utilise sendCommand() avec format API correct
            const response = await this.backend.sendCommand('playback.load', { 
                filename: filename 
            });
            
            if (response) {
                this.currentFile.name = filename;
                this.currentFile.path = filename;
                this.currentFile.id = response.file_id || null;
                this.currentFile.duration = response.duration || 0;
                this.state.duration = response.duration || 0;
                
                // Charger dans le modÃƒÆ’Ã‚Â¨le
                await this.playbackModel.load(filename);
                
                this.stats.filesPlayed++;
                this.log('info', 'GlobalPlayback', `ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Loaded: ${filename}`);
                
                this.eventBus.emit('globalPlayback:fileLoaded', {
                    filename,
                    duration: this.currentFile.duration
                });
                
                return true;
            }
        } catch (error) {
            this.log('error', 'GlobalPlayback', 'Load error:', error.message);
            this.eventBus.emit('globalPlayback:error', { 
                action: 'load', 
                error: error.message 
            });
        }
        
        return false;
    }
    
    /**
     * DÃƒÆ’Ã‚Â©marre la lecture
     */
    async play() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'GlobalPlayback', 'Playing without backend connection');
            return await this.playbackModel.play();
        }
        
        try {
            // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ NOUVEAU: Utilise sendCommand()
            const response = await this.backend.sendCommand('playback.play');
            
            if (response) {
                this.state.isPlaying = true;
                this.state.isPaused = false;
                this.stats.startTime = Date.now();
                
                await this.playbackModel.play();
                
                this.log('info', 'GlobalPlayback', 'ÃƒÂ¢Ã¢â‚¬â€œÃ‚Â¶ÃƒÂ¯Ã‚Â¸Ã‚Â Playing');
                return true;
            }
        } catch (error) {
            this.log('error', 'GlobalPlayback', 'Play error:', error.message);
        }
        
        return false;
    }
    
    /**
     * Met en pause
     */
    async pause() {
        if (!this.backend || !this.backend.isConnected()) {
            return await this.playbackModel.pause();
        }
        
        try {
            // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ NOUVEAU: Utilise sendCommand()
            const response = await this.backend.sendCommand('playback.pause');
            
            if (response) {
                this.state.isPlaying = false;
                this.state.isPaused = true;
                
                await this.playbackModel.pause();
                
                this.log('info', 'GlobalPlayback', 'ÃƒÂ¢Ã‚ÂÃ‚Â¸ÃƒÂ¯Ã‚Â¸Ã‚Â Paused');
                return true;
            }
        } catch (error) {
            this.log('error', 'GlobalPlayback', 'Pause error:', error.message);
        }
        
        return false;
    }
    
    /**
     * ArrÃƒÆ’Ã‚Âªte la lecture
     */
    async stop() {
        if (!this.backend || !this.backend.isConnected()) {
            const result = await this.playbackModel.stop();
            this.state.isPlaying = false;
            this.state.isPaused = false;
            this.state.currentPosition = 0;
            this.stats.startTime = null;
            return result;
        }
        
        try {
            // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ NOUVEAU: Utilise sendCommand()
            const response = await this.backend.sendCommand('playback.stop');
            
            if (response) {
                this.state.isPlaying = false;
                this.state.isPaused = false;
                this.state.currentPosition = 0;
                this.stats.startTime = null;
                
                await this.playbackModel.stop();
                
                this.log('info', 'GlobalPlayback', 'ÃƒÂ¢Ã‚ÂÃ‚Â¹ÃƒÂ¯Ã‚Â¸Ã‚Â Stopped');
                return true;
            }
        } catch (error) {
            this.log('error', 'GlobalPlayback', 'Stop error:', error.message);
        }
        
        return false;
    }
    
    /**
     * Se dÃƒÆ’Ã‚Â©place ÃƒÆ’Ã‚Â  une position
     */
    async seek(position) {
        if (!this.backend || !this.backend.isConnected()) {
            return await this.playbackModel.seek(position);
        }
        
        try {
            // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ NOUVEAU: Utilise sendCommand()
            const response = await this.backend.sendCommand('playback.seek', { 
                position: position 
            });
            
            if (response) {
                this.state.currentPosition = position;
                this.stats.seeks++;
                
                await this.playbackModel.seek(position);
                
                this.log('debug', 'GlobalPlayback', `ÃƒÂ¢Ã‚ÂÃ‚Â© Seek to ${position.toFixed(2)}s`);
                return true;
            }
        } catch (error) {
            this.log('error', 'GlobalPlayback', 'Seek error:', error.message);
        }
        
        return false;
    }
    
    /**
     * Change le tempo
     */
    async setTempo(tempo) {
        if (!this.backend || !this.backend.isConnected()) {
            return await this.playbackModel.setTempo(tempo);
        }
        
        try {
            // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ NOUVEAU: Utilise sendCommand()
            const response = await this.backend.sendCommand('playback.setTempo', { 
                tempo: tempo 
            });
            
            if (response) {
                this.state.tempo = tempo;
                this.stats.tempoChanges++;
                
                await this.playbackModel.setTempo(tempo);
                
                this.log('debug', 'GlobalPlayback', `ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Âµ Tempo set to ${tempo} BPM`);
                return true;
            }
        } catch (error) {
            this.log('error', 'GlobalPlayback', 'SetTempo error:', error.message);
        }
        
        return false;
    }
    
    // ========================================================================
    // GESTION PLAYLIST
    // ========================================================================
    
    /**
     * GÃƒÆ’Ã‚Â¨re la fin de lecture
     */
    async handlePlaybackEnded() {
        this.log('debug', 'GlobalPlayback', 'Playback ended');
        
        this.state.isPlaying = false;
        this.state.currentPosition = 0;
        
        // Calculer temps de lecture
        if (this.stats.startTime) {
            this.stats.totalPlaytime += (Date.now() - this.stats.startTime);
            this.stats.startTime = null;
        }
        
        this.eventBus.emit('globalPlayback:ended');
        
        // Auto-advance dans la playlist
        if (this.playlist.autoAdvance) {
            if (this.playlist.repeat === 'one') {
                await this.play();
            } else {
                await this.nextTrack();
            }
        }
    }
    
    /**
     * Passe ÃƒÆ’Ã‚Â  la piste suivante
     */
    async nextTrack() {
        if (this.playlist.files.length === 0) return false;
        
        let nextIndex = this.playlist.currentIndex + 1;
        
        if (nextIndex >= this.playlist.files.length) {
            if (this.playlist.repeat === 'all') {
                nextIndex = 0;
            } else {
                this.log('info', 'GlobalPlayback', 'End of playlist');
                return false;
            }
        }
        
        this.playlist.currentIndex = nextIndex;
        const file = this.playlist.files[nextIndex];
        
        await this.load(file.name || file.path);
        await this.play();
        
        this.eventBus.emit('globalPlayback:trackChanged', {
            index: nextIndex,
            file: file
        });
        
        return true;
    }
    
    /**
     * Passe ÃƒÆ’Ã‚Â  la piste prÃƒÆ’Ã‚Â©cÃƒÆ’Ã‚Â©dente
     */
    async previousTrack() {
        if (this.playlist.files.length === 0) return false;
        
        let prevIndex = this.playlist.currentIndex - 1;
        
        if (prevIndex < 0) {
            if (this.playlist.repeat === 'all') {
                prevIndex = this.playlist.files.length - 1;
            } else {
                this.log('info', 'GlobalPlayback', 'Start of playlist');
                return false;
            }
        }
        
        this.playlist.currentIndex = prevIndex;
        const file = this.playlist.files[prevIndex];
        
        await this.load(file.name || file.path);
        await this.play();
        
        this.eventBus.emit('globalPlayback:trackChanged', {
            index: prevIndex,
            file: file
        });
        
        return true;
    }
    
    /**
     * DÃƒÆ’Ã‚Â©finit la playlist
     */
    setPlaylist(files) {
        this.playlist.files = files;
        this.playlist.currentIndex = files.length > 0 ? 0 : -1;
        
        this.log('info', 'GlobalPlayback', `Playlist set: ${files.length} files`);
        this.eventBus.emit('globalPlayback:playlistChanged', {
            files: files,
            count: files.length
        });
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    getState() {
        return {
            ...this.state,
            file: this.currentFile,
            playlist: this.playlist
        };
    }
    
    getCurrentTime() {
        return this.state.currentPosition;
    }
    
    getDuration() {
        return this.state.duration;
    }
    
    isPlaying() {
        return this.state.isPlaying;
    }
    
    isPaused() {
        return this.state.isPaused;
    }
    
    getTempo() {
        return this.state.tempo;
    }
    
    getStats() {
        return { ...this.stats };
    }
    
    getCurrentFile() {
        return { ...this.currentFile };
    }
    
    getPlaylist() {
        return { ...this.playlist };
    }
    
    // ========================================================================
    // CLEANUP
    // ========================================================================
    
    destroy() {
        if (this.playbackModel) {
            this.playbackModel.destroy();
        }
        
        this.cache.preloadedFiles.clear();
        GlobalPlaybackController.instance = null;
        this.log('info', 'GlobalPlayback', 'Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = GlobalPlaybackController;
}

if (typeof window !== 'undefined') {
    window.GlobalPlaybackController = GlobalPlaybackController;
}
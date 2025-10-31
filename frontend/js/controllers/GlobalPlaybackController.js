// ============================================================================
// Fichier: frontend/js/controllers/GlobalPlaybackController.js
// Chemin réel: frontend/js/controllers/GlobalPlaybackController.js
// Version: v3.2.0 - SAFE LOGGER
// Date: 2025-10-31
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
    
    constructor(eventBus, backend, fileModel, logger) {
        if (GlobalPlaybackController.instance) {
            return GlobalPlaybackController.instance;
        }
        
        this.eventBus = eventBus || window.eventBus;
        this.backend = backend || window.backendService;
        this.fileModel = fileModel;
        this.logger = logger || this.createFallbackLogger();
        
        this.playbackModel = new PlaybackModel(eventBus, backend, this.logger);
        this.playbackModel.config.interpolationEnabled = true;
        
        this.currentFile = {
            id: null,
            name: null,
            path: null,
            midiJson: null,
            metadata: {}
        };
        
        this.playlist = {
            files: [],
            currentIndex: -1,
            shuffle: false,
            repeat: 'none',
            autoAdvance: true
        };
        
        this.routing = new Map();
        
        this.metronome = {
            enabled: false,
            volume: 80,
            accentFirst: true,
            soundType: 'beep'
        };
        
        this.cache = {
            preloadedFiles: new Map(),
            maxCacheSize: 10,
            enabled: true
        };
        
        this.stats = {
            filesPlayed: 0,
            totalPlaytime: 0,
            seeks: 0,
            tempoChanges: 0,
            startTime: null
        };
        
        this.config = {
            latencyCompensation: 0,
            velocityCurve: 'linear',
            autoPreload: true,
            syncInterval: 1000
        };
        
        this.log('info', 'GlobalPlaybackController', '✓ Initialized v3.2.0');
        
        this.connectPlaybackModelEvents();
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
    
    connectPlaybackModelEvents() {
        if (!this.eventBus) return;
        
        this.eventBus.on('playback:stateChanged', (data) => {
            this.log('debug', 'GlobalPlayback', 'State changed:', data.newState);
            this.eventBus.emit('globalPlayback:stateChanged', data);
        });
        
        this.eventBus.on('playback:timeUpdate', (data) => {
            this.eventBus.emit('globalPlayback:timeUpdate', data);
        });
        
        this.eventBus.on('playback:ended', () => {
            this.handlePlaybackEnded();
        });
    }
    
    setupBackendSync() {
        if (!this.backend || typeof this.backend.isConnected !== 'function') {
            this.log('warn', 'GlobalPlayback', 'Backend not available');
            return;
        }
        
        if (!this.backend.isConnected()) {
            this.log('warn', 'GlobalPlayback', 'Backend not connected');
            return;
        }
        
        setInterval(() => {
            if (this.playbackModel.isPlaying()) {
                this.syncWithBackend();
            }
        }, this.config.syncInterval);
    }
    
    async syncWithBackend() {
        if (!this.backend || typeof this.backend.isConnected !== 'function' || !this.backend.isConnected()) {
            return;
        }
        
        try {
            const status = await this.backend.send('playback.getStatus', {});
            if (status.success) {
                this.playbackModel.updateFromBackend(status.data);
            }
        } catch (error) {
            this.log('error', 'GlobalPlayback', 'Sync error:', error);
        }
    }
    
    async load(filename) {
        if (!this.backend || typeof this.backend.isConnected !== 'function' || !this.backend.isConnected()) {
            this.log('error', 'GlobalPlayback', 'Backend not connected');
            return false;
        }
        
        try {
            const response = await this.backend.send('playback.load', { filename });
            
            if (response.success) {
                this.currentFile.name = filename;
                this.currentFile.path = filename;
                
                await this.playbackModel.load(filename);
                
                this.stats.filesPlayed++;
                this.log('info', 'GlobalPlayback', 'Loaded:', filename);
                
                return true;
            }
        } catch (error) {
            this.log('error', 'GlobalPlayback', 'Load error:', error);
        }
        
        return false;
    }
    
    async play() {
        return await this.playbackModel.play();
    }
    
    async pause() {
        return await this.playbackModel.pause();
    }
    
    async stop() {
        const result = await this.playbackModel.stop();
        
        if (result) {
            this.stats.startTime = null;
        }
        
        return result;
    }
    
    async seek(position) {
        this.stats.seeks++;
        return await this.playbackModel.seek(position);
    }
    
    async setTempo(tempo) {
        this.stats.tempoChanges++;
        return await this.playbackModel.setTempo(tempo);
    }
    
    async handlePlaybackEnded() {
        this.log('debug', 'GlobalPlayback', 'Playback ended');
        
        if (this.playlist.autoAdvance) {
            if (this.playlist.repeat === 'one') {
                await this.play();
            } else {
                await this.nextTrack();
            }
        }
    }
    
    async nextTrack() {
        if (this.playlist.files.length === 0) return false;
        
        let nextIndex = this.playlist.currentIndex + 1;
        
        if (nextIndex >= this.playlist.files.length) {
            if (this.playlist.repeat === 'all') {
                nextIndex = 0;
            } else {
                return false;
            }
        }
        
        this.playlist.currentIndex = nextIndex;
        const file = this.playlist.files[nextIndex];
        
        await this.load(file.name);
        await this.play();
        
        return true;
    }
    
    async previousTrack() {
        if (this.playlist.files.length === 0) return false;
        
        let prevIndex = this.playlist.currentIndex - 1;
        
        if (prevIndex < 0) {
            if (this.playlist.repeat === 'all') {
                prevIndex = this.playlist.files.length - 1;
            } else {
                return false;
            }
        }
        
        this.playlist.currentIndex = prevIndex;
        const file = this.playlist.files[prevIndex];
        
        await this.load(file.name);
        await this.play();
        
        return true;
    }
    
    getState() {
        return this.playbackModel.getState();
    }
    
    getCurrentTime() {
        return this.playbackModel.getCurrentTime();
    }
    
    getDuration() {
        return this.playbackModel.getDuration();
    }
    
    isPlaying() {
        return this.playbackModel.isPlaying();
    }
    
    destroy() {
        if (this.playbackModel) {
            this.playbackModel.destroy();
        }
        
        GlobalPlaybackController.instance = null;
        this.log('info', 'GlobalPlayback', 'Destroyed');
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = GlobalPlaybackController;
}

if (typeof window !== 'undefined') {
    window.GlobalPlaybackController = GlobalPlaybackController;
}
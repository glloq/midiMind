// ============================================================================
// Fichier: frontend/js/controllers/PlaybackController.js
// Chemin rÃ©el: frontend/js/controllers/PlaybackController.js
// Version: v3.4.0 - API v4.0.0 COMPATIBLE
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.4.0:
// âœ” load_file â†’ playback.load
// âœ” play â†’ playback.play
// âœ” pause â†’ playback.pause
// âœ” CORRECTIONS v4.0.0: CompatibilitÃ© API v4.0.0
// âœ” stop â†’ playback.stop
// âœ” seek â†’ playback.seek
// âœ” set_tempo â†’ playback.setTempo
// âœ” set_loop â†’ playback.setLoop
// âš ï¸ setVolume commentÃ© (pas dans la doc API)
// âš ï¸ getPosition supprimÃ© (utilise Ã©vÃ©nement playback_position)
// ============================================================================

class PlaybackController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.logger = window.logger || console;
        this.playbackModel = models.playback;
        this.fileModel = models.file;
        this.view = views.playback;
        this.backend = window.app?.services?.backend || window.backendService;
        
        // Ã‰tat
        this.state = {
            ...this.state,
            isPlaying: false,
            position: 0,
            duration: 0,
            tempo: 120,
            volume: 100,
            loop: {
                enabled: false,
                start: 0,
                end: 0
            },
            loadedFile: null,
            backendReady: false
        };
        
        // Configuration
        this.config = {
            ...this.config,
            positionUpdateInterval: 100, // 100ms
            autoLoadOnSelect: true
        };
        
        // Timer position
        this.positionTimer = null;
        
        this._fullyInitialized = true;
        this.bindEvents();
    }
    
    // ========================================================================
    // Ã‰VÃ‰NEMENTS
    // ========================================================================
    
    bindEvents() {
        // Actions playback
        this.eventBus.on('playback:play', () => this.play());
        this.eventBus.on('playback:pause', () => this.pause());
        this.eventBus.on('playback:stop', () => this.stop());
        this.eventBus.on('playback:seek', (data) => this.seek(data.position));
        this.eventBus.on('playback:set-tempo', (data) => this.setTempo(data.tempo));
        this.eventBus.on('playback:set-volume', (data) => this.setVolume(data.volume));
        this.eventBus.on('playback:toggle-loop', () => this.toggleLoop());
        this.eventBus.on('playback:load-file', (data) => this.loadFile(data.filePath || data.fileId));
        
        // Backend Ã©vÃ©nements
        this.eventBus.on('backend:event:playback_position', (data) => {
            this.handlePositionUpdate(data);
        });
        this.eventBus.on('backend:event:playback_finished', () => {
            this.handlePlaybackFinished();
        });
        this.eventBus.on('backend:event:playback_state', (data) => {
            this.handleStateChange(data);
        });
        
        // Backend connexion
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        
        // Fichiers
        this.eventBus.on('file:selected', (data) => {
            if (this.config.autoLoadOnSelect) {
                this.loadFile(data.filePath || data.fileId);
            }
        });
        
        this.log('info', 'PlaybackController', 'âœ” Events bound (v3.4.0 - API v4.0.0)');
    }
    
    async onBackendConnected() {
        this.state.backendReady = true;
        this.log('info', 'PlaybackController', 'âœ” Backend connected');
    }
    
    onBackendDisconnected() {
        this.state.backendReady = false;
        this.stopPositionUpdates();
        this.state.isPlaying = false;
        this.log('warn', 'PlaybackController', 'âš ï¸ Backend disconnected');
    }
    
    // ========================================================================
    // COMMANDES PLAYBACK - API v4.0.0
    // ========================================================================
    
    /**
     * Charge un fichier pour lecture
     * âœ” API v4.0.0: playback.load
     */
    async loadFile(filePath) {
        try {
            this.log('info', 'PlaybackController', `Loading file for playback: ${filePath}`);
            
            // âœ” API v4.0.0: playback.load
            const response = await this.backend.sendCommand('playback.load', {
                file_path: filePath
            });
            
            this.state.loadedFile = filePath;
            this.state.position = 0;
            this.state.duration = response.duration || 0;
            
            // Mettre Ã  jour le model
            if (this.playbackModel) {
                this.playbackModel.set('currentFile', filePath);
                this.playbackModel.set('duration', this.state.duration);
                this.playbackModel.set('position', 0);
            }
            
            this.log('info', 'PlaybackController', `âœ” File loaded: ${filePath}`);
            this.eventBus.emit('playback:file-loaded', { filePath, duration: this.state.duration });
            
            if (this.notifications) {
                this.notifications.show(
                    'File Loaded',
                    `"${filePath}" ready for playback`,
                    'success',
                    2000
                );
            }
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'loadFile failed:', error);
            if (this.notifications) {
                this.notifications.show(
                    'Error',
                    `Failed to load file: ${error.message}`,
                    'error',
                    3000
                );
            }
            throw error;
        }
    }
    
    /**
     * DÃ©marre la lecture
     * âœ” API v4.0.0: playback.play
     */
    async play(filePath = null) {
        try {
            this.log('info', 'PlaybackController', 'Starting playback...');
            
            // âœ” API v4.0.0: playback.play
            const params = filePath ? { file_path: filePath } : {};
            const response = await this.backend.sendCommand('playback.play', params);
            
            this.state.isPlaying = true;
            
            // Si file_path fourni, mettre Ã  jour l'Ã©tat
            if (filePath) {
                this.state.loadedFile = filePath;
                this.state.duration = response.duration || this.state.duration;
            }
            
            // Mettre Ã  jour le model
            if (this.playbackModel) {
                this.playbackModel.set('state', 'playing');
                this.playbackModel.set('isPlaying', true);
            }
            
            // DÃ©marrer updates position via Ã©vÃ©nements backend
            this.startPositionUpdates();
            
            this.log('info', 'PlaybackController', 'âœ” Playback started');
            this.eventBus.emit('playback:started');
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'play failed:', error);
            if (this.notifications) {
                this.notifications.show(
                    'Error',
                    `Failed to start playback: ${error.message}`,
                    'error',
                    3000
                );
            }
            throw error;
        }
    }
    
    /**
     * Met en pause la lecture
     * âœ” API v4.0.0: playback.pause
     */
    async pause() {
        try {
            this.log('info', 'PlaybackController', 'Pausing playback...');
            
            // âœ” API v4.0.0: playback.pause
            await this.backend.sendCommand('playback.pause');
            
            this.state.isPlaying = false;
            
            // Mettre Ã  jour le model
            if (this.playbackModel) {
                this.playbackModel.set('state', 'paused');
                this.playbackModel.set('isPlaying', false);
            }
            
            // ArrÃªter updates position
            this.stopPositionUpdates();
            
            this.log('info', 'PlaybackController', 'âœ” Playback paused');
            this.eventBus.emit('playback:paused');
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'pause failed:', error);
            throw error;
        }
    }
    
    /**
     * ArrÃªte la lecture
     * âœ” API v4.0.0: playback.stop
     */
    async stop() {
        try {
            this.log('info', 'PlaybackController', 'Stopping playback...');
            
            // âœ” API v4.0.0: playback.stop
            await this.backend.sendCommand('playback.stop');
            
            this.state.isPlaying = false;
            this.state.position = 0;
            
            // Mettre Ã  jour le model
            if (this.playbackModel) {
                this.playbackModel.set('state', 'stopped');
                this.playbackModel.set('isPlaying', false);
                this.playbackModel.set('position', 0);
            }
            
            // ArrÃªter updates position
            this.stopPositionUpdates();
            
            this.log('info', 'PlaybackController', 'âœ” Playback stopped');
            this.eventBus.emit('playback:stopped');
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'stop failed:', error);
            throw error;
        }
    }
    
    /**
     * DÃ©place la position de lecture
     * âœ” API v4.0.0: playback.seek
     */
    async seek(position) {
        try {
            this.log('info', 'PlaybackController', `Seeking to: ${position}`);
            
            // âœ” API v4.0.0: playback.seek
            await this.backend.sendCommand('playback.seek', {
                position: position
            });
            
            this.state.position = position;
            
            // Mettre Ã  jour le model
            if (this.playbackModel) {
                this.playbackModel.set('position', position);
            }
            
            this.eventBus.emit('playback:position-changed', { position });
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'seek failed:', error);
            throw error;
        }
    }
    
    /**
     * Change le tempo
     * âœ” API v4.0.0: playback.setTempo
     */
    async setTempo(tempo) {
        try {
            this.log('info', 'PlaybackController', `Setting tempo: ${tempo}`);
            
            // âœ” API v4.0.0: playback.setTempo
            await this.backend.sendCommand('playback.setTempo', {
                tempo: tempo
            });
            
            this.state.tempo = tempo;
            
            // Mettre Ã  jour le model
            if (this.playbackModel) {
                this.playbackModel.set('tempo', tempo);
            }
            
            this.eventBus.emit('playback:tempo-changed', { tempo });
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'setTempo failed:', error);
            throw error;
        }
    }
    
    /**
     * Change le volume
     * âš ï¸ NOTE: Cette fonction n'est pas dans la documentation API backend
     * Elle est conservÃ©e pour compatibilitÃ© mais pourrait ne pas fonctionner
     * TODO: VÃ©rifier si le backend supporte cette commande
     */
    async setVolume(volume) {
        try {
            const clampedVolume = Math.max(0, Math.min(100, volume));
            
            this.log('warn', 'PlaybackController', `Setting volume: ${clampedVolume} (âš ï¸ not in API doc)`);
            
            // âš ï¸ COMMANDE NON DOCUMENTÃ‰E - Pourrait Ã©chouer
            // Si le backend ne supporte pas cette commande, elle sera rejetÃ©e
            // Volume control removed - API v4.2.2 does not support set_volume
            // Update local state directly
            this.state.volume = clampedVolume;
            
            if (this.playbackModel) {
                this.playbackModel.set('volume', clampedVolume);
            }
            
            this.eventBus.emit('playback:volume-changed', { volume: clampedVolume });
            
            return true;
                this.eventBus.emit('playback:volume-changed', { volume: clampedVolume });
                return false;
            }
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'setVolume failed:', error);
            throw error;
        }
    }
    
    /**
     * Toggle loop
     * âœ” API v4.0.0: playback.setLoop
     */
    async toggleLoop(start = null, end = null) {
        const newEnabled = !this.state.loop.enabled;
        
        try {
            this.log('info', 'PlaybackController', `Setting loop: ${newEnabled}`);
            
            // Calculer start/end si non fournis
            const loopStart = start !== null ? start : 0;
            const loopEnd = end !== null ? end : this.state.duration;
            
            // âœ” API v4.0.0: playback.setLoop
            await this.backend.sendCommand('playback.setLoop', {
                enabled: newEnabled,
                start: loopStart,
                end: loopEnd
            });
            
            this.state.loop = {
                enabled: newEnabled,
                start: loopStart,
                end: loopEnd
            };
            
            // Mettre Ã  jour le model
            if (this.playbackModel) {
                this.playbackModel.set('loop', this.state.loop);
            }
            
            this.eventBus.emit('playback:loop-changed', { loop: this.state.loop });
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'toggleLoop failed:', error);
            throw error;
        }
    }
    
    /**
     * Active le loop avec des paramÃ¨tres spÃ©cifiques
     * âœ” API v4.0.0: playback.setLoop
     */
    async setLoop(enabled, start = 0, end = null) {
        try {
            const loopEnd = end !== null ? end : this.state.duration;
            
            this.log('info', 'PlaybackController', `Setting loop: enabled=${enabled}, start=${start}, end=${loopEnd}`);
            
            // âœ” API v4.0.0: playback.setLoop
            await this.backend.sendCommand('playback.setLoop', {
                enabled: enabled,
                start: start,
                end: loopEnd
            });
            
            this.state.loop = {
                enabled: enabled,
                start: start,
                end: loopEnd
            };
            
            // Mettre Ã  jour le model
            if (this.playbackModel) {
                this.playbackModel.set('loop', this.state.loop);
            }
            
            this.eventBus.emit('playback:loop-changed', { loop: this.state.loop });
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'setLoop failed:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // GESTION Ã‰VÃ‰NEMENTS BACKEND
    // ========================================================================
    
    handlePositionUpdate(data) {
        this.state.position = data.position || 0;
        
        if (this.playbackModel) {
            this.playbackModel.set('position', this.state.position);
        }
        
        this.eventBus.emit('playback:position-updated', { position: this.state.position });
    }
    
    handlePlaybackFinished() {
        this.log('info', 'PlaybackController', 'Playback finished');
        
        this.state.isPlaying = false;
        this.state.position = this.state.loop.enabled ? this.state.loop.start : this.state.duration;
        
        if (this.playbackModel) {
            this.playbackModel.set('state', 'stopped');
            this.playbackModel.set('isPlaying', false);
            this.playbackModel.set('position', this.state.position);
        }
        
        this.stopPositionUpdates();
        
        this.eventBus.emit('playback:finished');
        
        // Rejouer si loop activÃ©
        if (this.state.loop.enabled) {
            setTimeout(() => {
                this.play().catch(err => {
                    this.log('error', 'PlaybackController', 'Loop replay failed:', err);
                });
            }, 100);
        }
    }
    
    handleStateChange(data) {
        const state = data.state;
        
        if (state === 'playing') {
            this.state.isPlaying = true;
            this.startPositionUpdates();
        } else {
            this.state.isPlaying = false;
            this.stopPositionUpdates();
        }
        
        if (this.playbackModel) {
            this.playbackModel.set('state', state);
            this.playbackModel.set('isPlaying', state === 'playing');
        }
    }
    
    // ========================================================================
    // UPDATES POSITION
    // ========================================================================
    
    /**
     * DÃ©marre l'Ã©coute des Ã©vÃ©nements de position
     * Note: Utilise les Ã©vÃ©nements playback_position du backend
     * au lieu de polling avec playback.getPosition
     */
    startPositionUpdates() {
        // Les mises Ã  jour de position se font via l'Ã©vÃ©nement 'backend:event:playback_position'
        // qui est dÃ©jÃ  Ã©coutÃ© dans bindEvents()
        // Pas besoin de polling actif
        this.log('info', 'PlaybackController', 'Position updates enabled (via events)');
    }
    
    stopPositionUpdates() {
        // Rien Ã  faire ici car on utilise les Ã©vÃ©nements backend
        // plutÃ´t qu'un polling actif
        this.log('info', 'PlaybackController', 'Position updates disabled');
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Obtient l'Ã©tat actuel du playback
     */
    getState() {
        return {
            isPlaying: this.state.isPlaying,
            position: this.state.position,
            duration: this.state.duration,
            tempo: this.state.tempo,
            volume: this.state.volume,
            loop: this.state.loop,
            loadedFile: this.state.loadedFile,
            backendReady: this.state.backendReady
        };
    }
    
    /**
     * Obtient la position actuelle en pourcentage
     */
    getPositionPercent() {
        if (this.state.duration === 0) return 0;
        return (this.state.position / this.state.duration) * 100;
    }
    
    /**
     * Formate un temps en secondes vers mm:ss
     */
    formatTime(seconds) {
        const mins = Math.floor(seconds / 60);
        const secs = Math.floor(seconds % 60);
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    }
    
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        }
    }
}

// ============================================================================
// EXPORT
// ============================================================================
if (typeof window !== 'undefined') {
    window.PlaybackController = PlaybackController;
}
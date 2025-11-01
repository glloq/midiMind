// ============================================================================
// Fichier: frontend/js/controllers/PlaybackController.js
// Chemin réel: frontend/js/controllers/PlaybackController.js
// Version: v3.3.0 - CONFORMITÉ API BACKEND
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.3.0:
// ✅ CRITIQUE: Commandes API conformes à API_DOCUMENTATION_FRONTEND.md
// ✅ load_file (au lieu de playback.load)
// ✅ play (au lieu de playback.play)
// ✅ pause (au lieu de playback.pause)
// ✅ stop (au lieu de playback.stop)
// ✅ seek (au lieu de playback.seek)
// ✅ set_tempo (au lieu de playback.setTempo)
// ✅ set_loop (au lieu de playback.setLoop)
// ⚠️ setVolume commenté (pas dans la doc API)
// ⚠️ getPosition supprimé (utilise événement playback_position)
// ============================================================================

class PlaybackController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.logger = window.logger || console;
        this.playbackModel = models.playback;
        this.fileModel = models.file;
        this.view = views.playback;
        this.backend = window.app?.services?.backend || window.backendService;
        
        // État
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
    // ÉVÉNEMENTS
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
        
        // Backend événements
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
        
        this.log('info', 'PlaybackController', '✅ Events bound (v3.3.0 - API compliant)');
    }
    
    async onBackendConnected() {
        this.state.backendReady = true;
        this.log('info', 'PlaybackController', '✅ Backend connected');
    }
    
    onBackendDisconnected() {
        this.state.backendReady = false;
        this.stopPositionUpdates();
        this.state.isPlaying = false;
        this.log('warn', 'PlaybackController', '⚠️ Backend disconnected');
    }
    
    // ========================================================================
    // COMMANDES PLAYBACK - API CONFORME
    // ========================================================================
    
    /**
     * Charge un fichier pour lecture
     * ✅ API: load_file
     */
    async loadFile(filePath) {
        try {
            this.log('info', 'PlaybackController', `Loading file for playback: ${filePath}`);
            
            // ✅ CONFORME: load_file avec file_path
            const response = await this.backend.sendCommand('load_file', {
                file_path: filePath
            });
            
            this.state.loadedFile = filePath;
            this.state.position = 0;
            this.state.duration = response.duration || 0;
            
            // Mettre à jour le model
            if (this.playbackModel) {
                this.playbackModel.set('currentFile', filePath);
                this.playbackModel.set('duration', this.state.duration);
                this.playbackModel.set('position', 0);
            }
            
            this.log('info', 'PlaybackController', `✅ File loaded: ${filePath}`);
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
     * Démarre la lecture
     * ✅ API: play
     */
    async play(filePath = null) {
        try {
            this.log('info', 'PlaybackController', 'Starting playback...');
            
            // ✅ CONFORME: play (avec file_path optionnel)
            const params = filePath ? { file_path: filePath } : {};
            const response = await this.backend.sendCommand('play', params);
            
            this.state.isPlaying = true;
            
            // Si file_path fourni, mettre à jour l'état
            if (filePath) {
                this.state.loadedFile = filePath;
                this.state.duration = response.duration || this.state.duration;
            }
            
            // Mettre à jour le model
            if (this.playbackModel) {
                this.playbackModel.set('state', 'playing');
                this.playbackModel.set('isPlaying', true);
            }
            
            // Démarrer updates position via événements backend
            this.startPositionUpdates();
            
            this.log('info', 'PlaybackController', '✅ Playback started');
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
     * ✅ API: pause
     */
    async pause() {
        try {
            this.log('info', 'PlaybackController', 'Pausing playback...');
            
            // ✅ CONFORME: pause
            await this.backend.sendCommand('pause');
            
            this.state.isPlaying = false;
            
            // Mettre à jour le model
            if (this.playbackModel) {
                this.playbackModel.set('state', 'paused');
                this.playbackModel.set('isPlaying', false);
            }
            
            // Arrêter updates position
            this.stopPositionUpdates();
            
            this.log('info', 'PlaybackController', '✅ Playback paused');
            this.eventBus.emit('playback:paused');
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'pause failed:', error);
            throw error;
        }
    }
    
    /**
     * Arrête la lecture
     * ✅ API: stop
     */
    async stop() {
        try {
            this.log('info', 'PlaybackController', 'Stopping playback...');
            
            // ✅ CONFORME: stop
            await this.backend.sendCommand('stop');
            
            this.state.isPlaying = false;
            this.state.position = 0;
            
            // Mettre à jour le model
            if (this.playbackModel) {
                this.playbackModel.set('state', 'stopped');
                this.playbackModel.set('isPlaying', false);
                this.playbackModel.set('position', 0);
            }
            
            // Arrêter updates position
            this.stopPositionUpdates();
            
            this.log('info', 'PlaybackController', '✅ Playback stopped');
            this.eventBus.emit('playback:stopped');
            
            return true;
            
        } catch (error) {
            this.log('error', 'PlaybackController', 'stop failed:', error);
            throw error;
        }
    }
    
    /**
     * Déplace la position de lecture
     * ✅ API: seek
     */
    async seek(position) {
        try {
            this.log('info', 'PlaybackController', `Seeking to: ${position}`);
            
            // ✅ CONFORME: seek avec position
            await this.backend.sendCommand('seek', {
                position: position
            });
            
            this.state.position = position;
            
            // Mettre à jour le model
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
     * ✅ API: set_tempo
     */
    async setTempo(tempo) {
        try {
            this.log('info', 'PlaybackController', `Setting tempo: ${tempo}`);
            
            // ✅ CONFORME: set_tempo avec tempo
            await this.backend.sendCommand('set_tempo', {
                tempo: tempo
            });
            
            this.state.tempo = tempo;
            
            // Mettre à jour le model
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
     * ⚠️ NOTE: Cette fonction n'est pas dans la documentation API backend
     * Elle est conservée pour compatibilité mais pourrait ne pas fonctionner
     * TODO: Vérifier si le backend supporte cette commande
     */
    async setVolume(volume) {
        try {
            const clampedVolume = Math.max(0, Math.min(100, volume));
            
            this.log('warn', 'PlaybackController', `Setting volume: ${clampedVolume} (⚠️ not in API doc)`);
            
            // ⚠️ COMMANDE NON DOCUMENTÉE - Pourrait échouer
            // Si le backend ne supporte pas cette commande, elle sera rejetée
            try {
                await this.backend.sendCommand('set_volume', {
                    volume: clampedVolume
                });
                
                this.state.volume = clampedVolume;
                
                // Mettre à jour le model
                if (this.playbackModel) {
                    this.playbackModel.set('volume', clampedVolume);
                }
                
                this.eventBus.emit('playback:volume-changed', { volume: clampedVolume });
                
                return true;
            } catch (apiError) {
                this.log('warn', 'PlaybackController', 'set_volume not supported by backend');
                // Mettre à jour localement quand même
                this.state.volume = clampedVolume;
                if (this.playbackModel) {
                    this.playbackModel.set('volume', clampedVolume);
                }
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
     * ✅ API: set_loop
     */
    async toggleLoop(start = null, end = null) {
        const newEnabled = !this.state.loop.enabled;
        
        try {
            this.log('info', 'PlaybackController', `Setting loop: ${newEnabled}`);
            
            // Calculer start/end si non fournis
            const loopStart = start !== null ? start : 0;
            const loopEnd = end !== null ? end : this.state.duration;
            
            // ✅ CONFORME: set_loop avec enabled, start, end
            await this.backend.sendCommand('set_loop', {
                enabled: newEnabled,
                start: loopStart,
                end: loopEnd
            });
            
            this.state.loop = {
                enabled: newEnabled,
                start: loopStart,
                end: loopEnd
            };
            
            // Mettre à jour le model
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
     * Active le loop avec des paramètres spécifiques
     * ✅ API: set_loop
     */
    async setLoop(enabled, start = 0, end = null) {
        try {
            const loopEnd = end !== null ? end : this.state.duration;
            
            this.log('info', 'PlaybackController', `Setting loop: enabled=${enabled}, start=${start}, end=${loopEnd}`);
            
            // ✅ CONFORME: set_loop avec enabled, start, end
            await this.backend.sendCommand('set_loop', {
                enabled: enabled,
                start: start,
                end: loopEnd
            });
            
            this.state.loop = {
                enabled: enabled,
                start: start,
                end: loopEnd
            };
            
            // Mettre à jour le model
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
    // GESTION ÉVÉNEMENTS BACKEND
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
        
        // Rejouer si loop activé
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
     * Démarre l'écoute des événements de position
     * Note: Utilise les événements playback_position du backend
     * au lieu de polling avec playback.getPosition
     */
    startPositionUpdates() {
        // Les mises à jour de position se font via l'événement 'backend:event:playback_position'
        // qui est déjà écouté dans bindEvents()
        // Pas besoin de polling actif
        this.log('info', 'PlaybackController', 'Position updates enabled (via events)');
    }
    
    stopPositionUpdates() {
        // Rien à faire ici car on utilise les événements backend
        // plutôt qu'un polling actif
        this.log('info', 'PlaybackController', 'Position updates disabled');
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Obtient l'état actuel du playback
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
// ============================================================================
// Fichier: frontend/js/controllers/PlaybackController.js
// Version: v3.2.0 - CORRECTED (Deferred Init + Offline Mode)
// Date: 2025-10-21
// ============================================================================
// CORRECTIONS v3.2.0:
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Initialisation diffÃƒÆ’Ã‚Â©rÃƒÆ’Ã‚Â©e si backend pas disponible
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ ÃƒÆ’Ã¢â‚¬Â°coute ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement 'backend:connected' pour init
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Mode graceful si backend absent (UI dÃƒÆ’Ã‚Â©sactivÃƒÆ’Ã‚Â©e)
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Notifications utilisateur claires
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ RÃƒÆ’Ã‚Â©-activation automatique quand backend se connecte
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Conservation de toutes les fonctionnalitÃƒÆ’Ã‚Â©s v3.1.00
// ============================================================================


class PlaybackController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ ModÃƒÆ’Ã‚Â¨les
        this.playbackModel = models.playback;
        this.fileModel = models.file;
        this.playlistModel = models.playlist;
        
        // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Vue
        this.view = views.playback;
        
        // ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Backend
        this.backend = window.backendService;
        
        // ÃƒÆ’Ã¢â‚¬Â°tat local
        this.state = {
            ...this.state,  // HÃƒÆ’Ã‚Â©rite de BaseController
            playing: false,
            position: 0,
            duration: 0,
            tempo: 120,
            loop: false,
            volume: 100,
            loadedFile: null,
            // ÃƒÂ¢Ã¢â‚¬Â Ã‚Â NOUVEAU
            backendReady: false,
            deferredInit: false
        };
        
        // Timer pour mise ÃƒÆ’Ã‚Â  jour position
        this.positionUpdateTimer = null;
        
        // ÃƒÂ¢Ã¢â‚¬Â Ã‚Â MODIFIÃƒÆ’Ã¢â‚¬Â°: VÃƒÆ’Ã‚Â©rifier si backend est prÃƒÆ’Ã‚Âªt avant d'initialiser
        if (this.backend && this.backend.isConnected()) {
            // Backend disponible, initialiser normalement
            this.initialize();
        } else {
            // Backend pas prÃƒÆ’Ã‚Âªt, diffÃƒÆ’Ã‚Â©rer l'initialisation
            this.state.deferredInit = true;
            this.logDebug('playback', 'Backend not ready, deferring PlaybackController initialization');
            
            // ÃƒÆ’Ã¢â‚¬Â°couter la connexion du backend
            this.eventBus.once('backend:connected', () => {
                this.logDebug('playback', 'Backend connected, initializing PlaybackController now');
                this.initialize();
            });
            
            // DÃƒÆ’Ã‚Â©sactiver l'UI en attendant
            this.disableUI();
        }
    }
    
    // ========================================================================
    // HOOKS BASECONTROLLER
    // ========================================================================
    
    /**
     * Hook d'initialisation personnalisÃƒÆ’Ã‚Â©e
     * Override de BaseController.onInitialize()
     */
    onInitialize() {
        this.logDebug('playback', 'Initializing PlaybackController...');
        
        // VÃƒÆ’Ã‚Â©rifier dÃƒÆ’Ã‚Â©pendances
        if (!this.backend) {
            this.logDebug('error', 'BackendService not available');
            this.showError('Backend service not available');
            this.disableUI();
            return;
        }
        
        if (!this.backend.isConnected()) {
            this.logDebug('warn', 'Backend not connected yet');
            this.disableUI();
            return;
        }
        
        // Backend OK !
        this.state.backendReady = true;
        this.state.deferredInit = false;
        
        // Activer l'UI
        this.enableUI();
        
        this.logDebug('playback', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ PlaybackController initialized');
    }
    
    /**
     * Binding des ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
     * Override de BaseController.bindEvents()
     */
    bindEvents() {
        this.logDebug('playback', 'Binding playback events...');
        
        // ========================================================================
        // ÃƒÆ’Ã¢â‚¬Â°VÃƒÆ’Ã¢â‚¬Â°NEMENTS UI ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ CONTROLLER
        // ========================================================================
        
        this.subscribe('playback:play', () => this.play(), {
            debounce: 100  // Anti-rebond 100ms
        });
        
        this.subscribe('playback:pause', () => this.pause());
        this.subscribe('playback:stop', () => this.stop());
        
        this.subscribe('playback:seek', (data) => this.seek(data.position), {
            validate: true,
            debounce: 50
        });
        
        this.subscribe('playback:set-tempo', (data) => this.setTempo(data.tempo));
        this.subscribe('playback:toggle-loop', () => this.toggleLoop());
        this.subscribe('playback:set-volume', (data) => this.setVolume(data.volume));
        
        // Chargement fichier
        this.subscribe('playback:load-file', (data) => this.loadFile(data.fileId));
        
        // ========================================================================
        // ÃƒÆ’Ã¢â‚¬Â°VÃƒÆ’Ã¢â‚¬Â°NEMENTS BACKEND ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ CONTROLLER
        // ========================================================================
        
        this.subscribe('backend:playback:state-changed', (data) => {
            this.handlePlaybackStateChanged(data);
        });
        
        this.subscribe('backend:playback:position-changed', (data) => {
            this.handlePositionChanged(data);
        });
        
        this.subscribe('backend:playback:finished', () => {
            this.handlePlaybackFinished();
        });
        
        // ========================================================================
        // ÃƒÆ’Ã¢â‚¬Â°VÃƒÆ’Ã¢â‚¬Â°NEMENTS MODEL ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ CONTROLLER
        // ========================================================================
        
        if (this.playbackModel) {
            this.subscribe('playback:model:state-changed', (data) => {
                this.updateUI(data);
            });
        }
        
        // ========================================================================
        // ÃƒÆ’Ã¢â‚¬Â°VÃƒÆ’Ã¢â‚¬Â°NEMENTS BACKEND CONNECTION
        // ========================================================================
        
        // ÃƒÂ¢Ã¢â‚¬Â Ã‚Â NOUVEAU: ÃƒÆ’Ã¢â‚¬Â°couter reconnexion backend
        this.subscribe('backend:connected', () => {
            this.logDebug('playback', 'Backend reconnected, re-enabling playback');
            this.state.backendReady = true;
            this.enableUI();
            
            // Recharger le fichier si on en avait un
            if (this.state.loadedFile) {
                this.loadFile(this.state.loadedFile);
            }
        });
        
        this.subscribe('websocket:disconnected', () => {
            this.logDebug('playback', 'Backend disconnected, disabling playback');
            this.state.backendReady = false;
            this.disableUI();
            this.stop(); // ArrÃƒÆ’Ã‚Âªter la lecture
        });
        
        this.logDebug('playback', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Events bound');
    }
    
    /**
     * Nettoyage avant destruction
     * Override de BaseController.onDestroy()
     */
    onDestroy() {
        this.logDebug('playback', 'Destroying PlaybackController...');
        
        // ArrÃƒÆ’Ã‚Âªter la lecture
        this.stop();
        
        // Nettoyer le timer
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
        
        this.logDebug('playback', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Å“ PlaybackController destroyed');
    }
    
    // ========================================================================
    // GESTION UI (ACTIVER/DÃƒÆ’Ã¢â‚¬Â°SACTIVER)
    // ========================================================================
    
    /**
     * DÃƒÆ’Ã‚Â©sactive l'UI du playback
     * @private
     */
    disableUI() {
        // DÃƒÆ’Ã‚Â©sactiver tous les boutons de lecture
        const playButtons = document.querySelectorAll('.playback-control');
        playButtons.forEach(button => {
            button.disabled = true;
            button.style.opacity = '0.5';
            button.style.cursor = 'not-allowed';
        });
        
        // Afficher message dans l'UI
        const playbackContainer = document.querySelector('.playback-container');
        if (playbackContainer) {
            let notice = playbackContainer.querySelector('.playback-disabled-notice');
            
            if (!notice) {
                notice = document.createElement('div');
                notice.className = 'playback-disabled-notice';
                notice.style.cssText = `
                    padding: 12px;
                    background: #fef3c7;
                    border: 1px solid #fbbf24;
                    border-radius: 8px;
                    margin-bottom: 16px;
                    color: #92400e;
                    font-size: 14px;
                    display: flex;
                    align-items: center;
                    gap: 8px;
                `;
                notice.innerHTML = `
                    <svg width="20" height="20" fill="currentColor" viewBox="0 0 20 20">
                        <path fill-rule="evenodd" d="M8.257 3.099c.765-1.36 2.722-1.36 3.486 0l5.58 9.92c.75 1.334-.213 2.98-1.742 2.98H4.42c-1.53 0-2.493-1.646-1.743-2.98l5.58-9.92zM11 13a1 1 0 11-2 0 1 1 0 012 0zm-1-8a1 1 0 00-1 1v3a1 1 0 002 0V6a1 1 0 00-1-1z" clip-rule="evenodd"/>
                    </svg>
                    <span>Playback unavailable - Backend not connected</span>
                `;
                playbackContainer.insertBefore(notice, playbackContainer.firstChild);
            }
        }
        
        this.logDebug('playback', 'UI disabled');
    }
    
    /**
     * Active l'UI du playback
     * @private
     */
    enableUI() {
        // RÃƒÆ’Ã‚Â©activer tous les boutons de lecture
        const playButtons = document.querySelectorAll('.playback-control');
        playButtons.forEach(button => {
            button.disabled = false;
            button.style.opacity = '1';
            button.style.cursor = 'pointer';
        });
        
        // Masquer le message d'avertissement
        const notice = document.querySelector('.playback-disabled-notice');
        if (notice) {
            notice.remove();
        }
        
        this.logDebug('playback', 'UI enabled');
    }
    
    // ========================================================================
    // COMMANDES DE LECTURE
    // ========================================================================
    
    /**
     * Lance la lecture
     */
    async play() {
        if (!this.state.backendReady) {
            this.showError('Backend not connected');
            return;
        }
        
        if (!this.state.loadedFile) {
            this.showWarning('No file loaded');
            return;
        }
        
        try {
            this.logDebug('playback', 'Playing...');
            
            const response = await this.backend.sendCommand('playback.play');
            
            if (response.success) {
                this.state.playing = true;
                this.startPositionUpdate();
                this.eventBus.emit('playback:started');
                this.logDebug('playback', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Playback started');
            } else {
                throw new Error(response.error || 'Failed to start playback');
            }
            
        } catch (error) {
            this.handleError('play', error);
        }
    }
    
    /**
     * Met en pause la lecture
     */
    async pause() {
        if (!this.state.backendReady) {
            return;
        }
        
        try {
            this.logDebug('playback', 'Pausing...');
            
            const response = await this.backend.sendCommand('playback.pause');
            
            if (response.success) {
                this.state.playing = false;
                this.stopPositionUpdate();
                this.eventBus.emit('playback:paused');
                this.logDebug('playback', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Playback paused');
            } else {
                throw new Error(response.error || 'Failed to pause playback');
            }
            
        } catch (error) {
            this.handleError('pause', error);
        }
    }
    
    /**
     * ArrÃƒÆ’Ã‚Âªte la lecture
     */
    async stop() {
        if (!this.state.backendReady) {
            return;
        }
        
        try {
            this.logDebug('playback', 'Stopping...');
            
            const response = await this.backend.sendCommand('playback.stop');
            
            if (response.success) {
                this.state.playing = false;
                this.state.position = 0;
                this.stopPositionUpdate();
                this.eventBus.emit('playback:stopped');
                this.logDebug('playback', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Playback stopped');
            } else {
                throw new Error(response.error || 'Failed to stop playback');
            }
            
        } catch (error) {
            this.handleError('stop', error);
        }
    }
    
    /**
     * Se positionne dans le fichier
     * @param {number} position - Position en secondes
     */
    async seek(position) {
        if (!this.state.backendReady) {
            return;
        }
        
        try {
            this.logDebug('playback', `Seeking to ${position}s`);
            
            const response = await this.backend.sendCommand('playback.seek', {
                position: position
            });
            
            if (response.success) {
                this.state.position = position;
                this.eventBus.emit('playback:seeked', { position });
                this.logDebug('playback', `ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Seeked to ${position}s`);
            } else {
                throw new Error(response.error || 'Failed to seek');
            }
            
        } catch (error) {
            this.handleError('seek', error);
        }
    }
    
    /**
     * DÃƒÆ’Ã‚Â©finit le tempo
     * @param {number} tempo - Tempo en BPM
     */
    async setTempo(tempo) {
        if (!this.state.backendReady) {
            return;
        }
        
        try {
            this.logDebug('playback', `Setting tempo to ${tempo} BPM`);
            
            const response = await this.backend.sendCommand('playback.setTempo', {
                tempo: tempo
            });
            
            if (response.success) {
                this.state.tempo = tempo;
                this.eventBus.emit('playback:tempo-changed', { tempo });
                this.logDebug('playback', `ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Tempo set to ${tempo} BPM`);
            } else {
                throw new Error(response.error || 'Failed to set tempo');
            }
            
        } catch (error) {
            this.handleError('setTempo', error);
        }
    }
    
    /**
     * Active/dÃƒÆ’Ã‚Â©sactive le loop
     */
    async toggleLoop() {
        if (!this.state.backendReady) {
            return;
        }
        
        const newLoop = !this.state.loop;
        
        try {
            this.logDebug('playback', `Toggle loop: ${newLoop}`);
            
            const response = await this.backend.sendCommand('playback.setLoop', {
                loop: newLoop
            });
            
            if (response.success) {
                this.state.loop = newLoop;
                this.eventBus.emit('playback:loop-changed', { loop: newLoop });
                this.logDebug('playback', `ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Loop ${newLoop ? 'enabled' : 'disabled'}`);
            } else {
                throw new Error(response.error || 'Failed to toggle loop');
            }
            
        } catch (error) {
            this.handleError('toggleLoop', error);
        }
    }
    
    /**
     * DÃƒÆ’Ã‚Â©finit le volume
     * @param {number} volume - Volume (0-100)
     */
    async setVolume(volume) {
        if (!this.state.backendReady) {
            return;
        }
        
        try {
            this.logDebug('playback', `Setting volume to ${volume}`);
            
            const response = await this.backend.sendCommand('playback.setVolume', {
                volume: volume
            });
            
            if (response.success) {
                this.state.volume = volume;
                this.eventBus.emit('playback:volume-changed', { volume });
                this.logDebug('playback', `ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Volume set to ${volume}`);
            } else {
                throw new Error(response.error || 'Failed to set volume');
            }
            
        } catch (error) {
            this.handleError('setVolume', error);
        }
    }
    
    /**
     * Charge un fichier MIDI
     * @param {string} fileId - ID du fichier
     */
    async loadFile(fileId) {
        if (!this.state.backendReady) {
            this.showError('Backend not connected - cannot load file');
            return;
        }
        
        try {
            this.logDebug('playback', `Loading file: ${fileId}`);
            
            const response = await this.backend.sendCommand('playback.load', {
                fileId: fileId
            });
            
            if (response.success) {
                this.state.loadedFile = fileId;
                this.state.duration = response.data?.duration || 0;
                this.eventBus.emit('playback:file-loaded', { fileId, duration: this.state.duration });
                this.logDebug('playback', `ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ File loaded: ${fileId}`);
                this.showSuccess(`File loaded: ${fileId}`);
            } else {
                throw new Error(response.error || 'Failed to load file');
            }
            
        } catch (error) {
            this.handleError('loadFile', error);
        }
    }
    
    // ========================================================================
    // GESTION POSITION
    // ========================================================================
    
    /**
     * DÃƒÆ’Ã‚Â©marre la mise ÃƒÆ’Ã‚Â  jour de la position
     * @private
     */
    startPositionUpdate() {
        this.stopPositionUpdate(); // Au cas oÃƒÆ’Ã‚Â¹
        
        this.positionUpdateTimer = setInterval(async () => {
            try {
                const response = await this.backend.sendCommand('playback.getPosition');
                
                if (response.success) {
                    this.state.position = response.data.position;
                    this.eventBus.emit('playback:position-updated', { 
                        position: this.state.position,
                        duration: this.state.duration
                    });
                }
            } catch (error) {
                // Silencieux - ne pas polluer les logs
            }
        }, 100); // Update tous les 100ms
    }
    
    /**
     * ArrÃƒÆ’Ã‚Âªte la mise ÃƒÆ’Ã‚Â  jour de la position
     * @private
     */
    stopPositionUpdate() {
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
    }
    
    // ========================================================================
    // HANDLERS ÃƒÆ’Ã¢â‚¬Â°VÃƒÆ’Ã¢â‚¬Â°NEMENTS BACKEND
    // ========================================================================
    
    /**
     * GÃƒÆ’Ã‚Â¨re un changement d'ÃƒÆ’Ã‚Â©tat du playback
     * @param {Object} data - DonnÃƒÆ’Ã‚Â©es de l'ÃƒÆ’Ã‚Â©tat
     */
    handlePlaybackStateChanged(data) {
        this.state.playing = data.playing;
        this.state.position = data.position;
        
        if (data.playing) {
            this.startPositionUpdate();
        } else {
            this.stopPositionUpdate();
        }
        
        this.eventBus.emit('playback:state-changed', data);
    }
    
    /**
     * GÃƒÆ’Ã‚Â¨re un changement de position
     * @param {Object} data - Position
     */
    handlePositionChanged(data) {
        this.state.position = data.position;
        this.eventBus.emit('playback:position-updated', data);
    }
    
    /**
     * GÃƒÆ’Ã‚Â¨re la fin de lecture
     */
    handlePlaybackFinished() {
        this.state.playing = false;
        this.state.position = 0;
        this.stopPositionUpdate();
        this.eventBus.emit('playback:finished');
        this.logDebug('playback', 'Playback finished');
    }
    
    // ========================================================================
    // MISE ÃƒÆ’Ã¢â€šÂ¬ JOUR UI
    // ========================================================================
    
    /**
     * Met ÃƒÆ’Ã‚Â  jour l'UI avec les donnÃƒÆ’Ã‚Â©es
     * @param {Object} data - DonnÃƒÆ’Ã‚Â©es de l'ÃƒÆ’Ã‚Â©tat
     */
    updateUI(data) {
        // ÃƒÆ’Ã¢â€šÂ¬ implÃƒÆ’Ã‚Â©menter selon votre UI
        this.eventBus.emit('playback:ui-update', data);
    }
    
    // ========================================================================
    // GESTION D'ERREURS
    // ========================================================================
    
    /**
     * GÃƒÆ’Ã‚Â¨re une erreur
     * @param {string} operation - OpÃƒÆ’Ã‚Â©ration en cours
     * @param {Error} error - Erreur
     */
    handleError(operation, error) {
        this.logDebug('error', `Playback error during ${operation}:`, error.message);
        this.showError(`Playback error: ${error.message}`);
        this.eventBus.emit('playback:error', { operation, error });
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaybackController;
}

if (typeof window !== 'undefined') {
    window.PlaybackController = PlaybackController;
// ============================================================================
// Fichier: frontend/js/controllers/PlaybackController.js
// Version: v3.2.0 - CORRECTED (Deferred Init + Offline Mode)
// Date: 2025-10-21
// ============================================================================
// CORRECTIONS v3.2.0:
// Ã¢Å“â€¦ Initialisation diffÃƒÂ©rÃƒÂ©e si backend pas disponible
// Ã¢Å“â€¦ Ãƒâ€°coute ÃƒÂ©vÃƒÂ©nement 'backend:connected' pour init
// Ã¢Å“â€¦ Mode graceful si backend absent (UI dÃƒÂ©sactivÃƒÂ©e)
// Ã¢Å“â€¦ Notifications utilisateur claires
// Ã¢Å“â€¦ RÃƒÂ©-activation automatique quand backend se connecte
// Ã¢Å“â€¦ Conservation de toutes les fonctionnalitÃƒÂ©s v3.1.00
// ============================================================================


class PlaybackController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Ã¢Å“â€¦ ModÃƒÂ¨les
        this.playbackModel = models.playback;
        this.fileModel = models.file;
        this.playlistModel = models.playlist;
        
        // Ã¢Å“â€¦ Vue
        this.view = views.playback;
        
        // Ã¢Å“â€¦ Backend
        this.backend = window.backendService;
        
        // Ãƒâ€°tat local
        this.state = {
            ...this.state,  // HÃƒÂ©rite de BaseController
            playing: false,
            position: 0,
            duration: 0,
            tempo: 120,
            loop: false,
            volume: 100,
            loadedFile: null,
            // Ã¢â€ Â NOUVEAU
            backendReady: false,
            deferredInit: false
        };
        
        // Timer pour mise ÃƒÂ  jour position
        this.positionUpdateTimer = null;
        
        // Ã¢â€ Â MODIFIÃƒâ€°: VÃƒÂ©rifier si backend est prÃƒÂªt avant d'initialiser
        if (this.backend && this.backend.isConnected()) {
            // Backend disponible, initialiser normalement
            this.initialize();
        } else {
            // Backend pas prÃƒÂªt, diffÃƒÂ©rer l'initialisation
            this.state.deferredInit = true;
            this.logDebug('playback', 'Backend not ready, deferring PlaybackController initialization');
            
            // Ãƒâ€°couter la connexion du backend
            this.eventBus.once('backend:connected', () => {
                this.logDebug('playback', 'Backend connected, initializing PlaybackController now');
                this.initialize();
            });
            
            // DÃƒÂ©sactiver l'UI en attendant
            this.disableUI();
        }
    }
    
    // ========================================================================
    // HOOKS BASECONTROLLER
    // ========================================================================
    
    /**
     * Hook d'initialisation personnalisÃƒÂ©e
     * Override de BaseController.onInitialize()
     */
    onInitialize() {
        this.logDebug('playback', 'Initializing PlaybackController...');
        
        // VÃƒÂ©rifier dÃƒÂ©pendances
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
        
        this.logDebug('playback', 'Ã¢Å“â€¦ PlaybackController initialized');
    }
    
    /**
     * Binding des ÃƒÂ©vÃƒÂ©nements
     * Override de BaseController.bindEvents()
     */
    bindEvents() {
        this.logDebug('playback', 'Binding playback events...');
        
        // ========================================================================
        // Ãƒâ€°VÃƒâ€°NEMENTS UI Ã¢â€ â€™ CONTROLLER
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
        // Ãƒâ€°VÃƒâ€°NEMENTS BACKEND Ã¢â€ â€™ CONTROLLER
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
        // Ãƒâ€°VÃƒâ€°NEMENTS MODEL Ã¢â€ â€™ CONTROLLER
        // ========================================================================
        
        if (this.playbackModel) {
            this.subscribe('playback:model:state-changed', (data) => {
                this.updateUI(data);
            });
        }
        
        // ========================================================================
        // Ãƒâ€°VÃƒâ€°NEMENTS BACKEND CONNECTION
        // ========================================================================
        
        // Ã¢â€ Â NOUVEAU: Ãƒâ€°couter reconnexion backend
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
            this.stop(); // ArrÃƒÂªter la lecture
        });
        
        this.logDebug('playback', 'Ã¢Å“â€¦ Events bound');
    }
    
    /**
     * Nettoyage avant destruction
     * Override de BaseController.onDestroy()
     */
    onDestroy() {
        this.logDebug('playback', 'Destroying PlaybackController...');
        
        // ArrÃƒÂªter la lecture
        this.stop();
        
        // Nettoyer le timer
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
        
        this.logDebug('playback', 'Ã¢Å“â€œ PlaybackController destroyed');
    }
    
    // ========================================================================
    // GESTION UI (ACTIVER/DÃƒâ€°SACTIVER)
    // ========================================================================
    
    /**
     * DÃƒÂ©sactive l'UI du playback
     * @private
     */
    disableUI() {
        // DÃƒÂ©sactiver tous les boutons de lecture
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
        // RÃƒÂ©activer tous les boutons de lecture
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
                this.logDebug('playback', 'Ã¢Å“â€¦ Playback started');
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
                this.logDebug('playback', 'Ã¢Å“â€¦ Playback paused');
            } else {
                throw new Error(response.error || 'Failed to pause playback');
            }
            
        } catch (error) {
            this.handleError('pause', error);
        }
    }
    
    /**
     * ArrÃƒÂªte la lecture
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
                this.logDebug('playback', 'Ã¢Å“â€¦ Playback stopped');
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
                this.logDebug('playback', `Ã¢Å“â€¦ Seeked to ${position}s`);
            } else {
                throw new Error(response.error || 'Failed to seek');
            }
            
        } catch (error) {
            this.handleError('seek', error);
        }
    }
    
    /**
     * DÃƒÂ©finit le tempo
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
                this.logDebug('playback', `Ã¢Å“â€¦ Tempo set to ${tempo} BPM`);
            } else {
                throw new Error(response.error || 'Failed to set tempo');
            }
            
        } catch (error) {
            this.handleError('setTempo', error);
        }
    }
    
    /**
     * Active/dÃƒÂ©sactive le loop
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
                this.logDebug('playback', `Ã¢Å“â€¦ Loop ${newLoop ? 'enabled' : 'disabled'}`);
            } else {
                throw new Error(response.error || 'Failed to toggle loop');
            }
            
        } catch (error) {
            this.handleError('toggleLoop', error);
        }
    }
    
    /**
     * DÃƒÂ©finit le volume
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
                this.logDebug('playback', `Ã¢Å“â€¦ Volume set to ${volume}`);
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
                this.logDebug('playback', `Ã¢Å“â€¦ File loaded: ${fileId}`);
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
     * DÃƒÂ©marre la mise ÃƒÂ  jour de la position
     * @private
     */
    startPositionUpdate() {
        this.stopPositionUpdate(); // Au cas oÃƒÂ¹
        
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
     * ArrÃƒÂªte la mise ÃƒÂ  jour de la position
     * @private
     */
    stopPositionUpdate() {
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
    }
    
    // ========================================================================
    // HANDLERS Ãƒâ€°VÃƒâ€°NEMENTS BACKEND
    // ========================================================================
    
    /**
     * GÃƒÂ¨re un changement d'ÃƒÂ©tat du playback
     * @param {Object} data - DonnÃƒÂ©es de l'ÃƒÂ©tat
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
     * GÃƒÂ¨re un changement de position
     * @param {Object} data - Position
     */
    handlePositionChanged(data) {
        this.state.position = data.position;
        this.eventBus.emit('playback:position-updated', data);
    }
    
    /**
     * GÃƒÂ¨re la fin de lecture
     */
    handlePlaybackFinished() {
        this.state.playing = false;
        this.state.position = 0;
        this.stopPositionUpdate();
        this.eventBus.emit('playback:finished');
        this.logDebug('playback', 'Playback finished');
    }
    
    // ========================================================================
    // MISE Ãƒâ‚¬ JOUR UI
    // ========================================================================
    
    /**
     * Met ÃƒÂ  jour l'UI avec les donnÃƒÂ©es
     * @param {Object} data - DonnÃƒÂ©es de l'ÃƒÂ©tat
     */
    updateUI(data) {
        // Ãƒâ‚¬ implÃƒÂ©menter selon votre UI
        this.eventBus.emit('playback:ui-update', data);
    }
    
    // ========================================================================
    // GESTION D'ERREURS
    // ========================================================================
    
    /**
     * GÃƒÂ¨re une erreur
     * @param {string} operation - OpÃƒÂ©ration en cours
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

    // ========================================================================
    // MÉTHODES MANQUANTES AJOUTÉES v3.1.00
    // ========================================================================
    
    async loadMidiJson(midiJson, fileId) {
        this.currentMidiJson = midiJson;
        this.currentFileId = fileId;
        await this.loadFile(fileId);
    }
    
    async resume() {
        await this.play();
    }
    
    async updateRouting(routing) {
        this.routing = routing;
        this.eventBus.emit('playback:routing-updated', { routing });
    }
    
    getState() {
        return {
            isPlaying: this.isPlaying || false,
            isPaused: this.isPaused || false,
            position: this.position || 0,
            tempo: this.tempo || 120
        };
    }
    
    getUpcomingEvents(lookahead = 1000) {
        // Retourner les événements MIDI à venir dans les prochaines millisecondes
        return [];
    }

}

window.PlaybackController = PlaybackController;

// ============================================================================
// FIN DU FICHIER PlaybackController.js v3.2.0
// ============================================================================
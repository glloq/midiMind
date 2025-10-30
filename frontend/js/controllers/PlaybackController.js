// ============================================================================
// Fichier: frontend/js/controllers/PlaybackController.js
// Version: v3.2.0 - CORRECTED (Deferred Init + Offline Mode)
// Date: 2025-10-21
// ============================================================================
// CORRECTIONS v3.2.0:
// âœ… Initialisation diffÃ©rÃ©e si backend pas disponible
// âœ… Ã‰coute Ã©vÃ©nement 'backend:connected' pour init
// âœ… Mode graceful si backend absent (UI dÃ©sactivÃ©e)
// âœ… Notifications utilisateur claires
// âœ… RÃ©-activation automatique quand backend se connecte
// âœ… Conservation de toutes les fonctionnalitÃ©s v3.1.0
// ============================================================================

class PlaybackController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // âœ… ModÃ¨les
        this.playbackModel = models.playback;
        this.fileModel = models.file;
        this.playlistModel = models.playlist;
        
        // âœ… Vue
        this.view = views.playback;
        
        // âœ… Backend
        this.backend = window.backendService;
        
        // Ã‰tat local
        this.state = {
            ...this.state,  // HÃ©rite de BaseController
            playing: false,
            position: 0,
            duration: 0,
            tempo: 120,
            loop: false,
            volume: 100,
            loadedFile: null,
            // â† NOUVEAU
            backendReady: false,
            deferredInit: false
        };
        
        // Timer pour mise Ã  jour position
        this.positionUpdateTimer = null;
        
        // â† MODIFIÃ‰: VÃ©rifier si backend est prÃªt avant d'initialiser
        if (this.backend && this.backend.isConnected()) {
            // Backend disponible, initialiser normalement
            // ✅ REMOVED: this.initialize() - BaseController calls it via autoInitialize
        } else {
            // Backend pas prÃªt, diffÃ©rer l'initialisation
            this.state.deferredInit = true;
            this.logDebug('playback', 'Backend not ready, deferring PlaybackController initialization');
            
            // Ã‰couter la connexion du backend
            this.eventBus.once('backend:connected', () => {
                this.logDebug('playback', 'Backend connected, initializing PlaybackController now');
                this.initialize();
            });
            
            // DÃ©sactiver l'UI en attendant
            this.disableUI();
        }
    }
    
    // ========================================================================
    // HOOKS BASECONTROLLER
    // ========================================================================
    
    /**
     * Hook d'initialisation personnalisÃ©e
     * Override de BaseController.onInitialize()
     */
    onInitialize() {
        this.logDebug('playback', 'Initializing PlaybackController...');
        
        // VÃ©rifier dÃ©pendances
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
        
        this.logDebug('playback', 'âœ… PlaybackController initialized');
    }
    
    /**
     * Binding des Ã©vÃ©nements
     * Override de BaseController.bindEvents()
     */
    bindEvents() {
        this.logDebug('playback', 'Binding playback events...');
        
        // ========================================================================
        // Ã‰VÃ‰NEMENTS UI â†’ CONTROLLER
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
        // Ã‰VÃ‰NEMENTS BACKEND â†’ CONTROLLER
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
        // Ã‰VÃ‰NEMENTS MODEL â†’ CONTROLLER
        // ========================================================================
        
        if (this.playbackModel) {
            this.subscribe('playback:model:state-changed', (data) => {
                this.updateUI(data);
            });
        }
        
        // ========================================================================
        // Ã‰VÃ‰NEMENTS BACKEND CONNECTION
        // ========================================================================
        
        // â† NOUVEAU: Ã‰couter reconnexion backend
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
            this.stop(); // ArrÃªter la lecture
        });
        
        this.logDebug('playback', 'âœ… Events bound');
    }
    
    /**
     * Nettoyage avant destruction
     * Override de BaseController.onDestroy()
     */
    onDestroy() {
        this.logDebug('playback', 'Destroying PlaybackController...');
        
        // ArrÃªter la lecture
        this.stop();
        
        // Nettoyer le timer
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
        
        this.logDebug('playback', 'âœ“ PlaybackController destroyed');
    }
    
    // ========================================================================
    // GESTION UI (ACTIVER/DÃ‰SACTIVER)
    // ========================================================================
    
    /**
     * DÃ©sactive l'UI du playback
     * @private
     */
    disableUI() {
        // DÃ©sactiver tous les boutons de lecture
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
        // RÃ©activer tous les boutons de lecture
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
                this.logDebug('playback', 'âœ… Playback started');
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
                this.logDebug('playback', 'âœ… Playback paused');
            } else {
                throw new Error(response.error || 'Failed to pause playback');
            }
            
        } catch (error) {
            this.handleError('pause', error);
        }
    }
    
    /**
     * ArrÃªte la lecture
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
                this.logDebug('playback', 'âœ… Playback stopped');
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
                this.logDebug('playback', `âœ… Seeked to ${position}s`);
            } else {
                throw new Error(response.error || 'Failed to seek');
            }
            
        } catch (error) {
            this.handleError('seek', error);
        }
    }
    
    /**
     * DÃ©finit le tempo
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
                this.logDebug('playback', `âœ… Tempo set to ${tempo} BPM`);
            } else {
                throw new Error(response.error || 'Failed to set tempo');
            }
            
        } catch (error) {
            this.handleError('setTempo', error);
        }
    }
    
    /**
     * Initialise le playback (alias pour initialize)
     */
    init() {
        return this.initialize();
    }
    
    /**
     * DÃ©finit l'Ã©tat du loop
     * @param {boolean} loop - Activer/dÃ©sactiver le loop
     */
    async setLoop(loop) {
        if (!this.state.backendReady) {
            return;
        }
        
        try {
            this.logDebug('playback', `Setting loop: ${loop}`);
            
            const response = await this.backend.sendCommand('playback.setLoop', {
                loop: loop
            });
            
            if (response.success) {
                this.state.loop = loop;
                this.eventBus.emit('playback:loop-changed', { loop });
                this.logDebug('playback', `âœ… Loop ${loop ? 'enabled' : 'disabled'}`);
            } else {
                throw new Error(response.error || 'Failed to set loop');
            }
            
        } catch (error) {
            this.handleError('setLoop', error);
        }
    }
    
    /**
     * DÃ©marre le mÃ©tronome
     */
    async startMetronome() {
        if (!this.state.backendReady) {
            return;
        }
        
        try {
            this.logDebug('playback', 'Starting metronome...');
            
            const response = await this.backend.sendCommand('metronome.start');
            
            if (response.success) {
                this.eventBus.emit('metronome:started');
                this.logDebug('playback', 'âœ… Metronome started');
                this.showSuccess('Metronome started');
            } else {
                throw new Error(response.error || 'Failed to start metronome');
            }
            
        } catch (error) {
            this.handleError('startMetronome', error);
        }
    }
    
    /**
     * ArrÃªte le mÃ©tronome
     */
    async stopMetronome() {
        if (!this.state.backendReady) {
            return;
        }
        
        try {
            this.logDebug('playback', 'Stopping metronome...');
            
            const response = await this.backend.sendCommand('metronome.stop');
            
            if (response.success) {
                this.eventBus.emit('metronome:stopped');
                this.logDebug('playback', 'âœ… Metronome stopped');
            } else {
                throw new Error(response.error || 'Failed to stop metronome');
            }
            
        } catch (error) {
            this.handleError('stopMetronome', error);
        }
    }
    
    /**
     * Active/dÃ©sactive le loop
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
                this.logDebug('playback', `âœ… Loop ${newLoop ? 'enabled' : 'disabled'}`);
            } else {
                throw new Error(response.error || 'Failed to toggle loop');
            }
            
        } catch (error) {
            this.handleError('toggleLoop', error);
        }
    }
    
    /**
     * DÃ©finit le volume
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
                this.logDebug('playback', `âœ… Volume set to ${volume}`);
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
                this.logDebug('playback', `âœ… File loaded: ${fileId}`);
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
     * DÃ©marre la mise Ã  jour de la position
     * @private
     */
    startPositionUpdate() {
        this.stopPositionUpdate(); // Au cas oÃ¹
        
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
     * ArrÃªte la mise Ã  jour de la position
     * @private
     */
    stopPositionUpdate() {
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
    }
    
    // ========================================================================
    // HANDLERS Ã‰VÃ‰NEMENTS BACKEND
    // ========================================================================
    
    /**
     * GÃ¨re un changement d'Ã©tat du playback
     * @param {Object} data - DonnÃ©es de l'Ã©tat
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
     * GÃ¨re un changement de position
     * @param {Object} data - Position
     */
    handlePositionChanged(data) {
        this.state.position = data.position;
        this.eventBus.emit('playback:position-updated', data);
    }
    
    /**
     * GÃ¨re la fin de lecture
     */
    handlePlaybackFinished() {
        this.state.playing = false;
        this.state.position = 0;
        this.stopPositionUpdate();
        this.eventBus.emit('playback:finished');
        this.logDebug('playback', 'Playback finished');
    }
    
    // ========================================================================
    // MISE Ã€ JOUR UI
    // ========================================================================
    
    /**
     * Met Ã  jour l'UI avec les donnÃ©es
     * @param {Object} data - DonnÃ©es de l'Ã©tat
     */
    updateUI(data) {
        // Ã€ implÃ©menter selon votre UI
        this.eventBus.emit('playback:ui-update', data);
    }
    
    // ========================================================================
    // GESTION D'ERREURS
    // ========================================================================
    
    /**
     * GÃ¨re une erreur
     * @param {string} operation - OpÃ©ration en cours
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
}

// ============================================================================
// FIN DU FICHIER PlaybackController.js v3.2.0
// ============================================================================
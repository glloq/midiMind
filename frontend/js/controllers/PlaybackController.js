// ============================================================================
// Fichier: frontend/js/controllers/PlaybackController.js
// Version: v3.2.0 - CORRECTED (Deferred Init + Offline Mode)
// Date: 2025-10-21
// ============================================================================
// CORRECTIONS v3.2.0:
// ✅ Initialisation différée si backend pas disponible
// ✅ Écoute événement 'backend:connected' pour init
// ✅ Mode graceful si backend absent (UI désactivée)
// ✅ Notifications utilisateur claires
// ✅ Ré-activation automatique quand backend se connecte
// ✅ Conservation de toutes les fonctionnalités v3.1.0
// ============================================================================

class PlaybackController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // ✅ Modèles
        this.playbackModel = models.playback;
        this.fileModel = models.file;
        this.playlistModel = models.playlist;
        
        // ✅ Vue
        this.view = views.playback;
        
        // ✅ Backend
        this.backend = window.backendService;
        
        // État local
        this.state = {
            ...this.state,  // Hérite de BaseController
            playing: false,
            position: 0,
            duration: 0,
            tempo: 120,
            loop: false,
            volume: 100,
            loadedFile: null,
            // ← NOUVEAU
            backendReady: false,
            deferredInit: false
        };
        
        // Timer pour mise à jour position
        this.positionUpdateTimer = null;
        
        // ← MODIFIÉ: Vérifier si backend est prêt avant d'initialiser
        if (this.backend && this.backend.isConnected()) {
            // Backend disponible, initialiser normalement
            this.initialize();
        } else {
            // Backend pas prêt, différer l'initialisation
            this.state.deferredInit = true;
            this.logDebug('playback', 'Backend not ready, deferring PlaybackController initialization');
            
            // Écouter la connexion du backend
            this.eventBus.once('backend:connected', () => {
                this.logDebug('playback', 'Backend connected, initializing PlaybackController now');
                this.initialize();
            });
            
            // Désactiver l'UI en attendant
            this.disableUI();
        }
    }
    
    // ========================================================================
    // HOOKS BASECONTROLLER
    // ========================================================================
    
    /**
     * Hook d'initialisation personnalisée
     * Override de BaseController.onInitialize()
     */
    onInitialize() {
        this.logDebug('playback', 'Initializing PlaybackController...');
        
        // Vérifier dépendances
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
        
        this.logDebug('playback', '✅ PlaybackController initialized');
    }
    
    /**
     * Binding des événements
     * Override de BaseController.bindEvents()
     */
    bindEvents() {
        this.logDebug('playback', 'Binding playback events...');
        
        // ========================================================================
        // ÉVÉNEMENTS UI → CONTROLLER
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
        // ÉVÉNEMENTS BACKEND → CONTROLLER
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
        // ÉVÉNEMENTS MODEL → CONTROLLER
        // ========================================================================
        
        if (this.playbackModel) {
            this.subscribe('playback:model:state-changed', (data) => {
                this.updateUI(data);
            });
        }
        
        // ========================================================================
        // ÉVÉNEMENTS BACKEND CONNECTION
        // ========================================================================
        
        // ← NOUVEAU: Écouter reconnexion backend
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
            this.stop(); // Arrêter la lecture
        });
        
        this.logDebug('playback', '✅ Events bound');
    }
    
    /**
     * Nettoyage avant destruction
     * Override de BaseController.onDestroy()
     */
    onDestroy() {
        this.logDebug('playback', 'Destroying PlaybackController...');
        
        // Arrêter la lecture
        this.stop();
        
        // Nettoyer le timer
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
        
        this.logDebug('playback', '✓ PlaybackController destroyed');
    }
    
    // ========================================================================
    // GESTION UI (ACTIVER/DÉSACTIVER)
    // ========================================================================
    
    /**
     * Désactive l'UI du playback
     * @private
     */
    disableUI() {
        // Désactiver tous les boutons de lecture
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
        // Réactiver tous les boutons de lecture
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
                this.logDebug('playback', '✅ Playback started');
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
                this.logDebug('playback', '✅ Playback paused');
            } else {
                throw new Error(response.error || 'Failed to pause playback');
            }
            
        } catch (error) {
            this.handleError('pause', error);
        }
    }
    
    /**
     * Arrête la lecture
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
                this.logDebug('playback', '✅ Playback stopped');
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
                this.logDebug('playback', `✅ Seeked to ${position}s`);
            } else {
                throw new Error(response.error || 'Failed to seek');
            }
            
        } catch (error) {
            this.handleError('seek', error);
        }
    }
    
    /**
     * Définit le tempo
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
                this.logDebug('playback', `✅ Tempo set to ${tempo} BPM`);
            } else {
                throw new Error(response.error || 'Failed to set tempo');
            }
            
        } catch (error) {
            this.handleError('setTempo', error);
        }
    }
    
    /**
     * Active/désactive le loop
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
                this.logDebug('playback', `✅ Loop ${newLoop ? 'enabled' : 'disabled'}`);
            } else {
                throw new Error(response.error || 'Failed to toggle loop');
            }
            
        } catch (error) {
            this.handleError('toggleLoop', error);
        }
    }
    
    /**
     * Définit le volume
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
                this.logDebug('playback', `✅ Volume set to ${volume}`);
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
                this.logDebug('playback', `✅ File loaded: ${fileId}`);
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
     * Démarre la mise à jour de la position
     * @private
     */
    startPositionUpdate() {
        this.stopPositionUpdate(); // Au cas où
        
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
     * Arrête la mise à jour de la position
     * @private
     */
    stopPositionUpdate() {
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
    }
    
    // ========================================================================
    // HANDLERS ÉVÉNEMENTS BACKEND
    // ========================================================================
    
    /**
     * Gère un changement d'état du playback
     * @param {Object} data - Données de l'état
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
     * Gère un changement de position
     * @param {Object} data - Position
     */
    handlePositionChanged(data) {
        this.state.position = data.position;
        this.eventBus.emit('playback:position-updated', data);
    }
    
    /**
     * Gère la fin de lecture
     */
    handlePlaybackFinished() {
        this.state.playing = false;
        this.state.position = 0;
        this.stopPositionUpdate();
        this.eventBus.emit('playback:finished');
        this.logDebug('playback', 'Playback finished');
    }
    
    // ========================================================================
    // MISE À JOUR UI
    // ========================================================================
    
    /**
     * Met à jour l'UI avec les données
     * @param {Object} data - Données de l'état
     */
    updateUI(data) {
        // À implémenter selon votre UI
        this.eventBus.emit('playback:ui-update', data);
    }
    
    // ========================================================================
    // GESTION D'ERREURS
    // ========================================================================
    
    /**
     * Gère une erreur
     * @param {string} operation - Opération en cours
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
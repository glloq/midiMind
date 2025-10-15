// ============================================================================
// Fichier: frontend/js/controllers/PlaybackController.js
// Version: 3.1.0-corrections
// Date: 2025-10-15
// ============================================================================
// Description:
//   Contrôleur de lecture MIDI - CORRIGÉ pour BaseController
//   
// CORRECTIONS v3.1.0:
//   ✅ Signature constructor compatible BaseController
//   ✅ Utilisation models au lieu de paramètres directs
//   ✅ Intégration backend protocole v3.0
//   ✅ destroy() complet avec cleanup timer
//   ✅ Notifications unifiées
//   ✅ Gestion erreurs standardisée
//
// Modifications par rapport à v3.0.0:
//   - Constructor refactorisé
//   - Ajout destroy() complet
//   - Amélioration handleError()
//   - Integration backend v3.0
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
            loadedFile: null
        };
        
        // Timer pour mise à jour position
        this.positionUpdateTimer = null;
        
        // Initialisation
        this.initialize();
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
            return;
        }
        
        this.logDebug('playback', '✓ PlaybackController initialized');
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
        
        this.subscribe('playback:setTempo', (data) => this.setTempo(data.tempo), {
            validate: true,
            debounce: 100
        });
        
        this.subscribe('playback:setLoop', (data) => this.setLoop(data.enabled));
        
        this.subscribe('playback:setVolume', (data) => this.setVolume(data.volume), {
            validate: true,
            debounce: 50
        });
        
        // ========================================================================
        // ÉVÉNEMENTS BACKEND → CONTROLLER (protocole v3.0)
        // ========================================================================
        
        this.subscribe('playback.status', (data) => this.handleStateUpdate(data));
        this.subscribe('playback.position', (data) => this.handlePositionUpdate(data));
        this.subscribe('playback.finished', () => this.handlePlaybackFinished());
        
        // ========================================================================
        // ÉVÉNEMENTS FICHIERS
        // ========================================================================
        
        this.subscribe('file:loaded', (data) => this.handleFileLoaded(data));
        this.subscribe('file:unloaded', () => this.handleFileUnloaded());
        
        // ========================================================================
        // ÉVÉNEMENTS PLAYLIST
        // ========================================================================
        
        this.subscribe('playlist:next', () => this.playNext());
        this.subscribe('playlist:previous', () => this.playPrevious());
        
        this.logDebug('playback', '✓ Events bound');
    }
    
    /**
     * Cleanup complet
     * Override de BaseController.destroy()
     */
    destroy() {
        this.logDebug('playback', 'Destroying PlaybackController...');
        
        // 1. Arrêter lecture
        this.stop();
        
        // 2. Arrêter timer position
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
        }
        
        // 3. Cleanup state
        this.state.playing = false;
        this.state.loadedFile = null;
        
        // 4. Appeler parent
        super.destroy();
        
        this.logDebug('playback', '✓ PlaybackController destroyed');
    }
    
    // ========================================================================
    // VALIDATEURS
    // ========================================================================
    
    /**
     * Initialise les validateurs pour les actions
     */
    setupValidators() {
        // Validateur seek
        this.validators['playback:seek'] = (data) => {
            if (!data || typeof data.position !== 'number') {
                this.logDebug('warning', 'Invalid seek data');
                return false;
            }
            
            if (data.position < 0 || data.position > this.state.duration) {
                this.logDebug('warning', 'Position out of bounds');
                return false;
            }
            
            return true;
        };
        
        // Validateur tempo
        this.validators['playback:setTempo'] = (data) => {
            if (!data || typeof data.tempo !== 'number') {
                return false;
            }
            
            if (data.tempo < 20 || data.tempo > 300) {
                this.logDebug('warning', 'Tempo out of range (20-300)');
                return false;
            }
            
            return true;
        };
        
        // Validateur volume
        this.validators['playback:setVolume'] = (data) => {
            if (!data || typeof data.volume !== 'number') {
                return false;
            }
            
            if (data.volume < 0 || data.volume > 100) {
                this.logDebug('warning', 'Volume out of range (0-100)');
                return false;
            }
            
            return true;
        };
    }
    
    // ========================================================================
    // CONTRÔLES DE LECTURE
    // ========================================================================
    
    /**
     * Démarre la lecture
     */
    async play() {
        this.logDebug('playback', 'Play requested');
        
        try {
            // Vérifier qu'un fichier est chargé
            if (!this.state.loadedFile) {
                this.showWarning('No file loaded');
                this.logDebug('warning', 'Play: No file loaded');
                return;
            }
            
            // Déjà en lecture
            if (this.state.playing) {
                this.logDebug('playback', 'Already playing');
                return;
            }
            
            // Appeler backend avec protocole v3.0
            const result = await this.backend.sendCommand('playback.start', {});
            
            if (result.success) {
                // Mettre à jour état local
                this.state.playing = true;
                
                // Démarrer timer position
                this.startPositionUpdates();
                
                // Émettre événement
                this.emitEvent('playback:state', { state: 'playing' });
                
                // Mettre à jour model
                if (this.playbackModel) {
                    this.playbackModel.set('playing', true);
                }
                
                this.showSuccess('Playback started');
                this.logDebug('playback', '✓ Playback started');
            }
            else {
                throw new Error(result.error || 'Failed to start playback');
            }
        }
        catch (error) {
            this.handleError('Play failed', error);
            this.showError('Failed to start playback');
        }
    }
    
    /**
     * Met en pause la lecture
     */
    async pause() {
        this.logDebug('playback', 'Pause requested');
        
        try {
            if (!this.state.playing) {
                this.logDebug('playback', 'Not playing');
                return;
            }
            
            // Appeler backend
            const result = await this.backend.sendCommand('playback.pause', {});
            
            if (result.success) {
                // Mettre à jour état
                this.state.playing = false;
                
                // Arrêter timer
                this.stopPositionUpdates();
                
                // Émettre événement
                this.emitEvent('playback:state', { state: 'paused' });
                
                // Mettre à jour model
                if (this.playbackModel) {
                    this.playbackModel.set('playing', false);
                }
                
                this.showSuccess('Playback paused');
                this.logDebug('playback', '✓ Playback paused');
            }
            else {
                throw new Error(result.error || 'Failed to pause playback');
            }
        }
        catch (error) {
            this.handleError('Pause failed', error);
            this.showError('Failed to pause playback');
        }
    }
    
    /**
     * Arrête la lecture
     */
    async stop() {
        this.logDebug('playback', 'Stop requested');
        
        try {
            // Appeler backend
            const result = await this.backend.sendCommand('playback.stop', {});
            
            if (result.success) {
                // Mettre à jour état
                this.state.playing = false;
                this.state.position = 0;
                
                // Arrêter timer
                this.stopPositionUpdates();
                
                // Émettre événement
                this.emitEvent('playback:state', { state: 'stopped' });
                
                // Mettre à jour model
                if (this.playbackModel) {
                    this.playbackModel.set('playing', false);
                    this.playbackModel.set('position', 0);
                }
                
                this.showSuccess('Playback stopped');
                this.logDebug('playback', '✓ Playback stopped');
            }
            else {
                throw new Error(result.error || 'Failed to stop playback');
            }
        }
        catch (error) {
            this.handleError('Stop failed', error);
            this.showError('Failed to stop playback');
        }
    }
    
    /**
     * Déplace la position de lecture
     * @param {number} position - Position en ms
     */
    async seek(position) {
        this.logDebug('playback', `Seek to ${position}ms`);
        
        try {
            // Valider position
            position = Math.max(0, Math.min(position, this.state.duration));
            
            // Appeler backend
            const result = await this.backend.sendCommand('playback.seek', {
                position: position
            });
            
            if (result.success) {
                // Mettre à jour état
                this.state.position = position;
                
                // Émettre événement
                this.emitEvent('playback:position', { position });
                
                // Mettre à jour model
                if (this.playbackModel) {
                    this.playbackModel.set('position', position);
                }
                
                this.logDebug('playback', `✓ Seeked to ${position}ms`);
            }
            else {
                throw new Error(result.error || 'Failed to seek');
            }
        }
        catch (error) {
            this.handleError('Seek failed', error);
        }
    }
    
    /**
     * Change le tempo
     * @param {number} tempo - Tempo en BPM (20-300)
     */
    async setTempo(tempo) {
        this.logDebug('playback', `Set tempo to ${tempo} BPM`);
        
        try {
            // Valider tempo
            tempo = Math.max(20, Math.min(300, tempo));
            
            // Appeler backend
            const result = await this.backend.sendCommand('playback.setTempo', {
                tempo: tempo
            });
            
            if (result.success) {
                // Mettre à jour état
                this.state.tempo = tempo;
                
                // Émettre événement
                this.emitEvent('playback:tempo', { tempo });
                
                // Mettre à jour model
                if (this.playbackModel) {
                    this.playbackModel.set('tempo', tempo);
                }
                
                this.showSuccess(`Tempo set to ${tempo} BPM`);
                this.logDebug('playback', `✓ Tempo set to ${tempo} BPM`);
            }
            else {
                throw new Error(result.error || 'Failed to set tempo');
            }
        }
        catch (error) {
            this.handleError('Set tempo failed', error);
        }
    }
    
    /**
     * Active/désactive le loop
     * @param {boolean} enabled - Loop activé
     */
    async setLoop(enabled) {
        this.logDebug('playback', `Set loop ${enabled ? 'ON' : 'OFF'}`);
        
        try {
            // Appeler backend
            const result = await this.backend.sendCommand('playback.setLoop', {
                enabled: enabled
            });
            
            if (result.success) {
                // Mettre à jour état
                this.state.loop = enabled;
                
                // Émettre événement
                this.emitEvent('playback:loop', { enabled });
                
                // Mettre à jour model
                if (this.playbackModel) {
                    this.playbackModel.set('loop', enabled);
                }
                
                this.showSuccess(`Loop ${enabled ? 'enabled' : 'disabled'}`);
                this.logDebug('playback', `✓ Loop ${enabled ? 'enabled' : 'disabled'}`);
            }
            else {
                throw new Error(result.error || 'Failed to set loop');
            }
        }
        catch (error) {
            this.handleError('Set loop failed', error);
        }
    }
    
    /**
     * Change le volume
     * @param {number} volume - Volume 0-100
     */
    async setVolume(volume) {
        this.logDebug('playback', `Set volume to ${volume}%`);
        
        try {
            // Valider volume
            volume = Math.max(0, Math.min(100, volume));
            
            // Appeler backend
            const result = await this.backend.sendCommand('playback.setVolume', {
                volume: volume
            });
            
            if (result.success) {
                // Mettre à jour état
                this.state.volume = volume;
                
                // Émettre événement
                this.emitEvent('playback:volume', { volume });
                
                // Mettre à jour model
                if (this.playbackModel) {
                    this.playbackModel.set('volume', volume);
                }
                
                this.logDebug('playback', `✓ Volume set to ${volume}%`);
            }
            else {
                throw new Error(result.error || 'Failed to set volume');
            }
        }
        catch (error) {
            this.handleError('Set volume failed', error);
        }
    }
    
    // ========================================================================
    // HANDLERS BACKEND (protocole v3.0)
    // ========================================================================
    
    /**
     * Gère les mises à jour d'état depuis le backend
     * @param {Object} data - État playback
     */
    handleStateUpdate(data) {
        this.logDebug('playback', 'State update received', data);
        
        try {
            // Mettre à jour état local
            if (data.state !== undefined) {
                this.state.playing = (data.state === 'playing');
            }
            
            if (data.position !== undefined) {
                this.state.position = data.position;
            }
            
            if (data.duration !== undefined) {
                this.state.duration = data.duration;
            }
            
            if (data.tempo !== undefined) {
                this.state.tempo = data.tempo;
            }
            
            if (data.loop !== undefined) {
                this.state.loop = data.loop;
            }
            
            // Émettre événement
            this.emitEvent('playback:stateUpdated', data);
            
            // Mettre à jour model
            if (this.playbackModel) {
                this.playbackModel.set(data);
            }
        }
        catch (error) {
            this.handleError('State update failed', error);
        }
    }
    
    /**
     * Gère les mises à jour de position depuis le backend
     * @param {Object} data - Position
     */
    handlePositionUpdate(data) {
        if (data.position !== undefined) {
            this.state.position = data.position;
            
            // Émettre événement
            this.emitEvent('playback:positionUpdated', { position: data.position });
            
            // Mettre à jour model
            if (this.playbackModel) {
                this.playbackModel.set('position', data.position);
            }
        }
    }
    
    /**
     * Gère la fin de lecture
     */
    handlePlaybackFinished() {
        this.logDebug('playback', 'Playback finished');
        
        // Arrêter timer
        this.stopPositionUpdates();
        
        // Mettre à jour état
        this.state.playing = false;
        this.state.position = this.state.loop ? 0 : this.state.duration;
        
        // Émettre événement
        this.emitEvent('playback:finished');
        
        // Si auto-advance activé
        if (this.playlistModel && this.playlistModel.get('autoAdvance')) {
            this.playNext();
        }
    }
    
    // ========================================================================
    // GESTION FICHIERS
    // ========================================================================
    
    /**
     * Gère le chargement d'un fichier
     * @param {Object} data - Données du fichier
     */
    handleFileLoaded(data) {
        this.logDebug('playback', 'File loaded', data);
        
        this.state.loadedFile = data.file;
        this.state.duration = data.duration || 0;
        this.state.position = 0;
        
        // Émettre événement
        this.emitEvent('playback:fileLoaded', data);
    }
    
    /**
     * Gère le déchargement d'un fichier
     */
    handleFileUnloaded() {
        this.logDebug('playback', 'File unloaded');
        
        // Arrêter lecture
        this.stop();
        
        // Reset état
        this.state.loadedFile = null;
        this.state.duration = 0;
        this.state.position = 0;
        
        // Émettre événement
        this.emitEvent('playback:fileUnloaded');
    }
    
    // ========================================================================
    // GESTION PLAYLIST
    // ========================================================================
    
    /**
     * Lit le fichier suivant de la playlist
     */
    async playNext() {
        this.logDebug('playback', 'Play next requested');
        
        try {
            if (!this.playlistModel) {
                this.logDebug('warning', 'PlaylistModel not available');
                return;
            }
            
            const nextFile = this.playlistModel.getNext();
            
            if (nextFile) {
                // Charger et lire le fichier
                this.emitEvent('file:load', { file: nextFile });
                
                // Attendre chargement
                await new Promise(resolve => setTimeout(resolve, 500));
                
                // Lire
                await this.play();
            }
            else {
                this.showInfo('No next file in playlist');
                this.logDebug('playback', 'No next file');
            }
        }
        catch (error) {
            this.handleError('Play next failed', error);
        }
    }
    
    /**
     * Lit le fichier précédent de la playlist
     */
    async playPrevious() {
        this.logDebug('playback', 'Play previous requested');
        
        try {
            if (!this.playlistModel) {
                this.logDebug('warning', 'PlaylistModel not available');
                return;
            }
            
            const prevFile = this.playlistModel.getPrevious();
            
            if (prevFile) {
                // Charger et lire le fichier
                this.emitEvent('file:load', { file: prevFile });
                
                // Attendre chargement
                await new Promise(resolve => setTimeout(resolve, 500));
                
                // Lire
                await this.play();
            }
            else {
                this.showInfo('No previous file in playlist');
                this.logDebug('playback', 'No previous file');
            }
        }
        catch (error) {
            this.handleError('Play previous failed', error);
        }
    }
    
    // ========================================================================
    // TIMER POSITION
    // ========================================================================
    
    /**
     * Démarre les mises à jour de position
     */
    startPositionUpdates() {
        // Arrêter timer existant
        this.stopPositionUpdates();
        
        // Démarrer nouveau timer (100ms = 10 FPS)
        this.positionUpdateTimer = setInterval(() => {
            if (this.state.playing) {
                // Incrémenter position (estimation locale)
                this.state.position += 100;
                
                // Limiter à la durée
                if (this.state.position >= this.state.duration) {
                    this.state.position = this.state.duration;
                    this.handlePlaybackFinished();
                }
                
                // Émettre événement
                this.emitEvent('playback:positionTick', { 
                    position: this.state.position 
                });
            }
        }, 100);
        
        this.logDebug('playback', 'Position updates started');
    }
    
    /**
     * Arrête les mises à jour de position
     */
    stopPositionUpdates() {
        if (this.positionUpdateTimer) {
            clearInterval(this.positionUpdateTimer);
            this.positionUpdateTimer = null;
            
            this.logDebug('playback', 'Position updates stopped');
        }
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * Retourne l'état actuel de lecture
     * @returns {Object} État
     */
    getState() {
        return {
            playing: this.state.playing,
            position: this.state.position,
            duration: this.state.duration,
            tempo: this.state.tempo,
            loop: this.state.loop,
            volume: this.state.volume,
            loadedFile: this.state.loadedFile,
            progress: this.state.duration > 0 
                ? (this.state.position / this.state.duration) * 100 
                : 0
        };
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
// FIN DU FICHIER PlaybackController.js v3.1.0-corrections
// ============================================================================
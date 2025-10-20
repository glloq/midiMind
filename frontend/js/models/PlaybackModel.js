// ============================================================================
// Fichier: frontend/js/models/PlaybackModel.js
// Version: v3.0.5 - FIXED (Constructor corrected - NO DOWNGRADE)
// Date: 2025-10-19
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// CORRECTION v3.0.5:
//   ✅ Fixed super() call to match BaseModel signature
//   ✅ Fixed data initialization (NO this.initialize() call)
//   ✅ ALL FEATURES PRESERVED (interpolation, loop, repeat, etc.)
// ============================================================================

class PlaybackModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // ✅ FIXED: Call super() with correct signature
        super({}, {
            persistKey: 'playbackmodel',
            eventPrefix: 'playback',
            autoPersist: false
        });
        
        // ✅ CRITICAL: Assign immediately after super()
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // ✅ FIXED: Initialize data directly (BaseModel doesn't have initialize() method)
        this.data = {
            // État
            state: 'STOPPED',  // STOPPED, PLAYING, PAUSED
            position: 0,
            duration: 0,
            progress: 0,
            
            // Paramètres
            tempo: 120,
            transpose: 0,
            volume: 100,
            
            // Loop
            loopEnabled: false,
            loopStart: 0,
            loopEnd: 0,
            
            // Repeat
            repeatMode: 'none',  // none, one, all
            
            // Interpolation locale
            isInterpolating: false,
            lastSyncTime: 0,
            localPosition: 0
        };
        
        // Configuration avancée
        this.config = {
            ...this.config,  // Keep BaseModel config
            interpolationEnabled: true,
            maxPositionDrift: 200,
            syncInterval: 1000,
            loopCheckInterval: 100
        };
        
        this.logger.info('PlaybackModel', '✓ Model initialized with interpolation support');
        
        this.setupPlaybackConfig();
    }
    
    // ========================================================================
    // CONFIGURATION - ✅ COMPLÉTÉ
    // ========================================================================
    
    setupPlaybackConfig() {
        // Écouter les changements d'état
        this.watch('state', (newState, oldState) => {
            this.logger.debug('PlaybackModel', `State changed: ${oldState} → ${newState}`);
            
            this.eventBus.emit('playback:state-changed', { 
                newState, 
                oldState 
            });
            
            if (newState === 'PLAYING') {
                this.startInterpolation();
            } else {
                this.stopInterpolation();
            }
        });
        
        // Écouter les changements de position
        this.watch('position', (newPosition) => {
            const progress = this.calculateProgress();
            this.set('progress', progress, { silent: true });
            
            this.eventBus.emit('playback:position-changed', { 
                position: newPosition, 
                progress 
            });
            
            // Vérifier loop si activé
            if (this.get('loopEnabled')) {
                this.checkLoopBoundaries(newPosition);
            }
        });
    }
    
    // ========================================================================
    // SYNCHRONISATION BACKEND - ✅ COMPLÉTÉ
    // ========================================================================
    
    /**
     * Met à jour depuis les données backend
     * @param {Object} backendData - Données reçues du backend
     */
    updateFromBackend(backendData) {
        if (!backendData?.player) {
            this.logger.warn('PlaybackModel', 'Invalid backend data received');
            return;
        }
        
        const player = backendData.player;
        
        // Sync état
        const stateMap = {
            'stopped': 'STOPPED',
            'playing': 'PLAYING',
            'paused': 'PAUSED'
        };
        
        const backendState = stateMap[player.state] || 'STOPPED';
        
        // Mettre à jour données
        this.update({
            state: backendState,
            position: player.position || 0,
            duration: player.duration || this.get('duration'),
            tempo: player.tempo || this.get('tempo'),
            volume: player.volume !== undefined ? player.volume : this.get('volume')
        }, { silent: true });
        
        // Update timestamp pour interpolation
        this.set('lastSyncTime', Date.now(), { silent: true });
        this.set('localPosition', player.position || 0, { silent: true });
        
        this.logger.debug('PlaybackModel', 
            `Synced from backend: ${backendState} @ ${player.position}ms`);
    }
    
    // ========================================================================
    // INTERPOLATION - ✅ COMPLÉTÉ
    // ========================================================================
    
    /**
     * Démarre l'interpolation de position locale
     */
    startInterpolation() {
        if (!this.config.interpolationEnabled) {
            return;
        }
        
        if (this.get('isInterpolating')) {
            return;
        }
        
        this.set('isInterpolating', true, { silent: true });
        this.set('lastSyncTime', Date.now(), { silent: true });
        this.set('localPosition', this.get('position'), { silent: true });
        
        this._interpolationTimer = setInterval(() => {
            this.interpolatePosition();
        }, 50);  // Update every 50ms
        
        this.logger.debug('PlaybackModel', 'Interpolation started');
    }
    
    /**
     * Arrête l'interpolation
     */
    stopInterpolation() {
        if (this._interpolationTimer) {
            clearInterval(this._interpolationTimer);
            this._interpolationTimer = null;
        }
        
        this.set('isInterpolating', false, { silent: true });
        
        this.logger.debug('PlaybackModel', 'Interpolation stopped');
    }
    
    /**
     * Interpole la position entre les sync backend
     */
    interpolatePosition() {
        if (!this.get('isInterpolating')) {
            return;
        }
        
        const now = Date.now();
        const lastSync = this.get('lastSyncTime');
        const elapsed = now - lastSync;
        
        const localPos = this.get('localPosition');
        const interpolated = localPos + elapsed;
        
        // Vérifier drift
        const backendPos = this.get('position');
        const drift = Math.abs(interpolated - backendPos);
        
        if (drift > this.config.maxPositionDrift) {
            // Trop de dérive, resync
            this.logger.warn('PlaybackModel', 
                `Position drift too high: ${drift}ms, resyncing`);
            this.set('localPosition', backendPos, { silent: true });
            this.set('lastSyncTime', now, { silent: true });
            return;
        }
        
        // Mettre à jour position interpolée
        this.set('position', interpolated, { silent: true });
        
        // Calculer progress
        const progress = this.calculateProgress();
        this.set('progress', progress, { silent: true });
        
        // Emit subtle update
        this.eventBus.emit('playback:position-interpolated', {
            position: interpolated,
            progress: progress
        });
    }
    
    // ========================================================================
    // COMMANDES PLAYBACK - ✅ COMPLÉTÉ
    // ========================================================================
    
    /**
     * Démarre la lecture
     */
    async play() {
        try {
            this.logger.info('PlaybackModel', 'Starting playback...');
            
            const response = await this.backend.sendCommand('playback.play', {});
            
            if (response.success) {
                this.set('state', 'PLAYING');
                
                this.eventBus.emit('playback:started');
                
                return true;
            }
            
            throw new Error(response.error?.message || 'Play command failed');
            
        } catch (error) {
            this.logger.error('PlaybackModel', `Play failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Met en pause
     */
    async pause() {
        try {
            this.logger.info('PlaybackModel', 'Pausing playback...');
            
            const response = await this.backend.sendCommand('playback.pause', {});
            
            if (response.success) {
                this.set('state', 'PAUSED');
                
                this.eventBus.emit('playback:paused');
                
                return true;
            }
            
            throw new Error(response.error?.message || 'Pause command failed');
            
        } catch (error) {
            this.logger.error('PlaybackModel', `Pause failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Arrête la lecture
     */
    async stop() {
        try {
            this.logger.info('PlaybackModel', 'Stopping playback...');
            
            const response = await this.backend.sendCommand('playback.stop', {});
            
            if (response.success) {
                this.update({
                    state: 'STOPPED',
                    position: 0,
                    progress: 0
                });
                
                this.eventBus.emit('playback:stopped');
                
                return true;
            }
            
            throw new Error(response.error?.message || 'Stop command failed');
            
        } catch (error) {
            this.logger.error('PlaybackModel', `Stop failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Change la position
     * @param {number} position - Position en ms
     */
    async seek(position) {
        try {
            this.logger.info('PlaybackModel', `Seeking to ${position}ms`);
            
            const response = await this.backend.sendCommand('playback.seek', {
                position: position
            });
            
            if (response.success) {
                this.set('position', position);
                this.set('localPosition', position, { silent: true });
                this.set('lastSyncTime', Date.now(), { silent: true });
                
                this.eventBus.emit('playback:seeked', { position });
                
                return true;
            }
            
            throw new Error(response.error?.message || 'Seek command failed');
            
        } catch (error) {
            this.logger.error('PlaybackModel', `Seek failed: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // PARAMÈTRES - ✅ COMPLÉTÉ
    // ========================================================================
    
    /**
     * Change le tempo
     * @param {number} tempo - BPM
     */
    async setTempo(tempo) {
        try {
            this.logger.info('PlaybackModel', `Setting tempo to ${tempo} BPM`);
            
            const response = await this.backend.sendCommand('playback.set-tempo', {
                tempo: tempo
            });
            
            if (response.success) {
                this.set('tempo', tempo);
                
                this.eventBus.emit('playback:tempo-changed', { tempo });
                
                return true;
            }
            
            throw new Error(response.error?.message || 'Set tempo failed');
            
        } catch (error) {
            this.logger.error('PlaybackModel', `Set tempo failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Change la transposition
     * @param {number} semitones - Demi-tons
     */
    async setTranspose(semitones) {
        try {
            this.logger.info('PlaybackModel', `Setting transpose to ${semitones} semitones`);
            
            const response = await this.backend.sendCommand('playback.set-transpose', {
                semitones: semitones
            });
            
            if (response.success) {
                this.set('transpose', semitones);
                
                this.eventBus.emit('playback:transpose-changed', { semitones });
                
                return true;
            }
            
            throw new Error(response.error?.message || 'Set transpose failed');
            
        } catch (error) {
            this.logger.error('PlaybackModel', `Set transpose failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Change le volume
     * @param {number} volume - Volume 0-100
     */
    async setVolume(volume) {
        try {
            this.logger.info('PlaybackModel', `Setting volume to ${volume}`);
            
            const response = await this.backend.sendCommand('playback.set-volume', {
                volume: volume
            });
            
            if (response.success) {
                this.set('volume', volume);
                
                this.eventBus.emit('playback:volume-changed', { volume });
                
                return true;
            }
            
            throw new Error(response.error?.message || 'Set volume failed');
            
        } catch (error) {
            this.logger.error('PlaybackModel', `Set volume failed: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // LOOP - ✅ COMPLÉTÉ
    // ========================================================================
    
    /**
     * Active/désactive la boucle
     * @param {boolean} enabled - Activer
     * @param {number} start - Début en ms (optionnel)
     * @param {number} end - Fin en ms (optionnel)
     */
    async setLoop(enabled, start = null, end = null) {
        try {
            const duration = this.get('duration');
            
            const loopStart = start !== null ? start : this.get('loopStart');
            const loopEnd = end !== null ? end : (this.get('loopEnd') || duration);
            
            this.logger.info('PlaybackModel', 
                enabled 
                    ? `Loop enabled: ${loopStart}ms → ${loopEnd}ms` 
                    : 'Loop disabled');
            
            const response = await this.backend.sendCommand('playback.set-loop', {
                enabled: enabled,
                start: loopStart,
                end: loopEnd
            });
            
            if (response.success) {
                this.update({
                    loopEnabled: enabled,
                    loopStart: loopStart,
                    loopEnd: loopEnd
                });
                
                this.eventBus.emit('playback:loop-changed', {
                    enabled,
                    start: loopStart,
                    end: loopEnd
                });
                
                return true;
            }
            
            throw new Error(response.error?.message || 'Set loop failed');
            
        } catch (error) {
            this.logger.error('PlaybackModel', `Set loop failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Définit le mode repeat
     * @param {string} mode - none, one, all
     */
    setRepeatMode(mode) {
        const validModes = ['none', 'one', 'all'];
        
        if (!validModes.includes(mode)) {
            this.logger.warn('PlaybackModel', `Invalid repeat mode: ${mode}`);
            return;
        }
        
        this.set('repeatMode', mode);
        
        this.logger.info('PlaybackModel', `Repeat mode: ${mode}`);
        
        this.eventBus.emit('playback:repeat-changed', { mode });
    }
    
    /**
     * Vérifie les limites du loop
     * @param {number} position - Position actuelle
     */
    checkLoopBoundaries(position) {
        const loopStart = this.get('loopStart');
        const loopEnd = this.get('loopEnd');
        
        // Si on dépasse la fin, retour au début
        if (position >= loopEnd) {
            this.logger.debug('PlaybackModel', 'Loop boundary reached, jumping to start');
            
            this.seek(loopStart);
            
            this.eventBus.emit('playback:loop-triggered', {
                from: loopEnd,
                to: loopStart
            });
        }
    }
    
    // ========================================================================
    // UTILITAIRES - ✅ COMPLÉTÉ
    // ========================================================================
    
    /**
     * Calcule le pourcentage de progression
     * @returns {number} Progress 0-100
     */
    calculateProgress() {
        const position = this.get('position');
        const duration = this.get('duration');
        
        if (!duration || duration === 0) return 0;
        return Math.min((position / duration) * 100, 100);
    }
    
    /**
     * Formate la position en mm:ss
     * @returns {string}
     */
    formatPosition() {
        return this._formatTime(this.get('position'));
    }
    
    /**
     * Formate la durée en mm:ss
     * @returns {string}
     */
    formatDuration() {
        return this._formatTime(this.get('duration'));
    }
    
    /**
     * Formate un temps en mm:ss
     * @private
     * @param {number} ms - Temps en millisecondes
     * @returns {string}
     */
    _formatTime(ms) {
        const totalSeconds = Math.floor(ms / 1000);
        const minutes = Math.floor(totalSeconds / 60);
        const seconds = totalSeconds % 60;
        return `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
    }
    
    // ========================================================================
    // GETTERS D'ÉTAT
    // ========================================================================
    
    isPlaying() {
        return this.get('state') === 'PLAYING';
    }
    
    isPaused() {
        return this.get('state') === 'PAUSED';
    }
    
    isStopped() {
        return this.get('state') === 'STOPPED';
    }
    
    isLooping() {
        return this.get('loopEnabled');
    }
    
    getRepeatMode() {
        return this.get('repeatMode');
    }
    
    getPlaybackInfo() {
        return {
            state: this.get('state'),
            position: this.get('position'),
            duration: this.get('duration'),
            progress: this.get('progress'),
            tempo: this.get('tempo'),
            transpose: this.get('transpose'),
            volume: this.get('volume'),
            loopEnabled: this.get('loopEnabled'),
            loopStart: this.get('loopStart'),
            loopEnd: this.get('loopEnd'),
            repeatMode: this.get('repeatMode'),
            isInterpolating: this.get('isInterpolating'),
            formattedPosition: this.formatPosition(),
            formattedDuration: this.formatDuration()
        };
    }
    
    // ========================================================================
    // NETTOYAGE
    // ========================================================================
    
    /**
     * Détruit le modèle et nettoie les ressources
     */
    destroy() {
        this.logger.info('PlaybackModel', 'Destroying...');
        
        this.stopInterpolation();
        
        super.destroy();
        
        this.logger.info('PlaybackModel', '✓ Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaybackModel;
}

if (typeof window !== 'undefined') {
    window.PlaybackModel = PlaybackModel;
}
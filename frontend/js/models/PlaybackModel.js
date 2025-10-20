// ============================================================================
// Fichier: frontend/js/models/PlaybackModel.js
// Version: v3.0.1 - FIXED CONSTRUCTOR ONLY
// Date: 2025-10-19
// ============================================================================
// CORRECTION: Uniquement le constructeur (super call)
// RIEN D'AUTRE N'EST MODIFIÉ - Tout ton code reste identique
// ============================================================================

class PlaybackModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // ✅ FIX: Correct super() call
        super({}, {
            persistKey: 'playbackmodel',
            eventPrefix: 'playback',
            autoPersist: false
        });
        
        // ✅ FIX: Assign immediately
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // ✅ FIX: Initialize data directly (pas d'appel à this.initialize())
        this.data = {
            // État de lecture
            state: 'STOPPED', // PLAYING, PAUSED, STOPPED
            
            // Fichier en cours
            currentFile: null,
            currentFileName: null,
            currentFilePath: null,
            
            // Position et durée
            position: 0,          // ms
            duration: 0,          // ms
            progress: 0,          // 0-100%
            
            // Contrôles
            tempo: 1.0,           // 0.5 - 2.0
            transpose: 0,         // -12 à +12
            volume: 100,          // 0-100
            
            // Métadonnées
            bpm: 0,
            trackCount: 0,
            
            // Loop/Repeat
            loopEnabled: false,
            loopStart: 0,
            loopEnd: 0,
            repeatMode: 'none',   // none, one, all
            
            // Interpolation locale
            isInterpolating: false,
            lastServerPosition: 0,
            lastServerTimestamp: 0
        };
        
        // Timers
        this.localPositionTimer = null;
        this.backendSyncTimer = null;
        
        // Configuration
        this.config = {
            ...this.config,
            interpolationEnabled: true,
            maxPositionDrift: 200,
            syncInterval: 1000,
            loopCheckInterval: 100
        };
        
        this.logger.info('PlaybackModel', '✓ Model initialized');
    }
    
    // ========================================================================
    // SYNCHRONISATION BACKEND
    // ========================================================================
    
    updateFromBackend(backendData) {
        if (!backendData?.player) {
            this.logger.warn('PlaybackModel', 'Invalid backend data received');
            return;
        }
        
        const player = backendData.player;
        
        // Sauvegarder position serveur pour interpolation
        this.set('lastServerPosition', player.position || 0, { silent: true });
        this.set('lastServerTimestamp', Date.now(), { silent: true });
        
        // Mise à jour silencieuse pour éviter reboucle
        this.update({
            state: player.state || 'STOPPED',
            position: player.position || 0,
            duration: player.duration || 0,
            tempo: player.tempo || 1.0,
            transpose: player.transpose || 0,
            volume: player.volume || 100,
            bpm: player.bpm || 0,
            trackCount: player.trackCount || 0
        }, { silent: true });
        
        this.logger.debug('PlaybackModel', 
            `Synced with backend: pos=${player.position}ms, state=${player.state}`);
    }
    
    // ========================================================================
    // POSITION UPDATE - DUAL TIMER SYSTEM
    // ========================================================================
    
    startPositionUpdate() {
        // Timer local rapide pour fluidité (60 FPS)
        this.localPositionTimer = setInterval(() => {
            if (this.get('state') === 'PLAYING') {
                // Incrémenter position locale (estimation)
                const currentPos = this.get('position');
                const tempo = this.get('tempo');
                const increment = (1000 / 60) * (tempo / 1.0); // Approximation
                this.set('position', currentPos + increment, { silent: true });
                
                // Calculer progress
                const progress = this.calculateProgress();
                this.set('progress', progress, { silent: true });
                
                // Émettre événement
                this.eventBus.emit('playback:position-update', {
                    position: currentPos + increment,
                    progress: progress
                });
            }
        }, 1000 / 60); // 60 FPS
        
        // Timer backend pour correction (lent)
        this.backendSyncTimer = setInterval(async () => {
            if (this.get('state') === 'PLAYING') {
                try {
                    const status = await this.backend.sendCommand('playback.status', {});
                    if (status.success && status.data) {
                        // Corriger position locale
                        this.set('position', status.data.position_ms, { silent: true });
                        
                        // Mettre à jour autres infos
                        if (status.data.bar !== undefined) {
                            this.set('bar', status.data.bar, { silent: true });
                            this.set('beat', status.data.beat, { silent: true });
                            this.set('tick', status.data.tick, { silent: true });
                        }
                    }
                } catch (error) {
                    this.logger.warn('PlaybackModel', 'Failed to sync position:', error);
                }
            }
        }, 1000); // Sync toutes les 1 seconde
        
        this.logger.debug('PlaybackModel', 'Position update started');
    }
    
    stopPositionUpdate() {
        if (this.localPositionTimer) {
            clearInterval(this.localPositionTimer);
            this.localPositionTimer = null;
        }
        if (this.backendSyncTimer) {
            clearInterval(this.backendSyncTimer);
            this.backendSyncTimer = null;
        }
        this.logger.debug('PlaybackModel', 'Position update stopped');
    }
    
    // ========================================================================
    // LOOP / REPEAT
    // ========================================================================
    
    setLoop(enabled, start = null, end = null) {
        const duration = this.get('duration');
        
        const loopStart = start !== null ? start : this.get('loopStart');
        const loopEnd = end !== null ? end : (this.get('loopEnd') || duration);
        
        this.update({
            loopEnabled: enabled,
            loopStart: loopStart,
            loopEnd: loopEnd
        });
        
        this.logger.info('PlaybackModel', 
            enabled 
                ? `Loop enabled: ${loopStart}ms → ${loopEnd}ms` 
                : 'Loop disabled');
        
        this.eventBus.emit('playback:loop-changed', {
            enabled,
            start: loopStart,
            end: loopEnd
        });
    }
    
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
    
    checkLoopBoundaries(position) {
        const loopStart = this.get('loopStart');
        const loopEnd = this.get('loopEnd');
        
        if (position >= loopEnd) {
            this.logger.debug('PlaybackModel', 'Loop boundary reached');
            
            this.set('position', loopStart, { silent: false });
            
            this.eventBus.emit('playback:loop-triggered', {
                from: loopEnd,
                to: loopStart
            });
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    calculateProgress() {
        const position = this.get('position');
        const duration = this.get('duration');
        
        if (!duration || duration === 0) return 0;
        return Math.min((position / duration) * 100, 100);
    }
    
    formatPosition() {
        return this._formatTime(this.get('position'));
    }
    
    formatDuration() {
        return this._formatTime(this.get('duration'));
    }
    
    _formatTime(ms) {
        const totalSeconds = Math.floor(ms / 1000);
        const minutes = Math.floor(totalSeconds / 60);
        const seconds = totalSeconds % 60;
        return `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
    }
    
    // ========================================================================
    // GETTERS
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
            formattedPosition: this.formatPosition(),
            formattedDuration: this.formatDuration()
        };
    }
    
    // ========================================================================
    // NETTOYAGE
    // ========================================================================
    
    destroy() {
        this.logger.info('PlaybackModel', 'Destroying...');
        
        this.stopPositionUpdate();
        
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
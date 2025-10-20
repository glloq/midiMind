// ============================================================================
// Fichier: frontend/js/models/PlaybackModel.js
// Version: v3.0.1 - CORRIGÉ (Interpolation + Loop/Repeat)
// Date: 2025-10-08
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Modèle gérant l'état de lecture des fichiers MIDI.
//   Synchronise avec le backend et fournit interpolation locale.
//
// CORRECTIONS v3.0.1:
//   ✅ Interpolation locale complète
//   ✅ Modes Loop/Repeat implémentés
//   ✅ Gestion smooth des updates backend
//   ✅ Synchronisation précise tempo
//
// Responsabilités:
//   - Stocker l'état de lecture (playing, paused, stopped)
//   - Gérer position/durée avec interpolation locale
//   - Synchroniser avec le backend
//   - Loop/Repeat modes
//   - Notifier les changements d'état
//
// Design Patterns:
//   - Observer (via BaseModel)
//   - State Pattern (états de lecture)
//
// Auteur: midiMind Team
// ============================================================================

class PlaybackModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, logger);
        
        // Dépendances
        this.backend = backend;
        this.logger = logger;
        
        // Initialiser les données via BaseModel
        this.initialize({
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
            
            // Loop/Repeat - ✅ NOUVEAU
            loopEnabled: false,
            loopStart: 0,
            loopEnd: 0,
            repeatMode: 'none',   // none, one, all
            
            // Interpolation locale
            isInterpolating: false,
            lastServerPosition: 0,
            lastServerTimestamp: 0
        });
        
        // Timer d'interpolation
        this.interpolationTimer = null;
        this.interpolationInterval = 50; // ms - ✅ CORRIGÉ
 
		this.localPositionTimer = null;
		this.backendSyncTimer = null;
        // Configuration
        this.config = {
            interpolationEnabled: true,
            maxPositionDrift: 200,      // ms max de dérive acceptable
            syncInterval: 1000,          // Sync backend toutes les 1s
            loopCheckInterval: 100       // Vérifier loop toutes les 100ms
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
    // INTERPOLATION LOCALE - ✅ IMPLÉMENTÉ COMPLET
    // ========================================================================
    
    /**
     * Démarre l'interpolation locale de la position
     */
    startInterpolation() {
        if (!this.config.interpolationEnabled) {
            this.logger.debug('PlaybackModel', 'Interpolation disabled');
            return;
        }
        
        if (this.interpolationTimer) {
            this.logger.debug('PlaybackModel', 'Interpolation already running');
            return;
        }
        
        this.set('isInterpolating', true, { silent: true });
        
        this.interpolationTimer = setInterval(() => {
            this.interpolatePosition();
        }, this.interpolationInterval);
        
        this.logger.debug('PlaybackModel', 
            `Interpolation started (interval: ${this.interpolationInterval}ms)`);
    }
    
    /**
     * Arrête l'interpolation locale
     */
    stopInterpolation() {
        if (this.interpolationTimer) {
            clearInterval(this.interpolationTimer);
            this.interpolationTimer = null;
            this.logger.debug('PlaybackModel', 'Interpolation stopped');
        }
        
        this.set('isInterpolating', false, { silent: true });
    }
    
    /**
     * Interpole la position en fonction du tempo
     */
    interpolatePosition() {
        const currentPos = this.get('position');
        const duration = this.get('duration');
        const tempo = this.get('tempo');
        
        if (currentPos >= duration && !this.get('loopEnabled')) {
            // Fin du fichier atteinte
            this.handleEndReached();
            return;
        }
        
        // Calculer nouvelle position avec tempo
        const deltaTime = this.interpolationInterval * tempo;
        let newPos = currentPos + deltaTime;
        
        // Limiter à la durée sauf si loop
        if (!this.get('loopEnabled')) {
            newPos = Math.min(newPos, duration);
        }
        
        // Vérifier dérive avec serveur
        this.checkPositionDrift(newPos);
        
        // Mise à jour silencieuse pour éviter spam événements
        this.set('position', newPos, { silent: true });
    }
    
    /**
     * Vérifie la dérive entre position locale et serveur
     * @param {number} localPosition - Position locale calculée
     */
    checkPositionDrift(localPosition) {
        const lastServerPos = this.get('lastServerPosition');
        const lastServerTime = this.get('lastServerTimestamp');
        
        if (!lastServerTime) return;
        
        const timeSinceSync = Date.now() - lastServerTime;
        
        // Calculer position attendue du serveur
        const tempo = this.get('tempo');
        const expectedServerPos = lastServerPos + (timeSinceSync * tempo);
        
        // Calculer dérive
        const drift = Math.abs(localPosition - expectedServerPos);
        
        if (drift > this.config.maxPositionDrift) {
            this.logger.warn('PlaybackModel', 
                `Position drift detected: ${drift}ms (max: ${this.config.maxPositionDrift}ms)`);
            
            // Correction brutale
            this.set('position', expectedServerPos, { silent: true });
        }
    }
    
	
	/**
 * Démarre la mise à jour de position
 * CORRECTION v3.0.3: Synchronisation avec backend
 */
startPositionUpdate() {
    // Arrêter timer existant
    this.stopPositionUpdate();
    
    // Timer local pour interpolation (rapide)
    this.localPositionTimer = setInterval(() => {
        if (this.get('state') === 'playing') {
            // Incrémenter position locale (estimation)
            const currentPos = this.get('position');
            const tempo = this.get('tempo');
            const increment = (1000 / 60) * (tempo / 120); // Approximation
            this.set('position', currentPos + increment);
        }
    }, 1000 / 60); // 60 FPS pour fluidité
    
    // Timer backend pour correction (lent)
    this.backendSyncTimer = setInterval(async () => {
        if (this.get('state') === 'playing') {
            try {
                // Demander position réelle au backend
                const status = await this.backend.sendCommand('playback.status', {});
                if (status.success && status.data) {
                    // Corriger position locale
                    this.set('position', status.data.position_ms, { silent: true });
                    
                    // Mettre à jour autres infos
                    if (status.data.bar !== undefined) {
                        this.set('bar', status.data.bar);
                        this.set('beat', status.data.beat);
                        this.set('tick', status.data.tick);
                    }
                }
            } catch (error) {
                this.logger.warn('PlaybackModel', 'Failed to sync position:', error);
            }
        }
    }, 1000); // Sync toutes les 1 seconde
    
    this.logger.debug('PlaybackModel', 'Position update started (dual timer)');
}

/**
 * Arrête la mise à jour de position
 */
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
	
	/**
 * Génère un ordre aléatoire avec smart algorithm
 * CORRECTION v3.0.3: Évite répétitions
 */
generateShuffleOrder() {
    const playlist = this.get('currentPlaylist');
    if (!playlist || !playlist.files || playlist.files.length === 0) {
        this.set('shuffleOrder', []);
        return;
    }
    
    const files = playlist.files;
    const history = this.get('history');
    const memorySize = this.config.shuffleMemorySize;
    
    // Fichiers récemment joués à éviter
    const recentFiles = history.slice(-memorySize).map(h => h.fileId);
    
    // Séparer fichiers en deux groupes
    const availableFiles = files.filter(f => !recentFiles.includes(f));
    const recentFilesInPlaylist = files.filter(f => recentFiles.includes(f));
    
    // Fisher-Yates shuffle sur fichiers disponibles
    const shuffled = [...availableFiles];
    for (let i = shuffled.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [shuffled[i], shuffled[j]] = [shuffled[j], shuffled[i]];
    }
    
    // Ajouter fichiers récents à la fin (si nécessaire)
    if (availableFiles.length === 0) {
        // Tous les fichiers sont récents, shuffle tout
        const allShuffled = [...files];
        for (let i = allShuffled.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [allShuffled[i], allShuffled[j]] = [allShuffled[j], allShuffled[i]];
        }
        this.set('shuffleOrder', allShuffled);
    } else {
        // Ajouter fichiers récents après les autres
        const order = [...shuffled, ...recentFilesInPlaylist];
        this.set('shuffleOrder', order);
    }
    
    this.logger.debug('PlaylistModel', 
        `Smart shuffle generated: ${shuffled.length} fresh, ${recentFilesInPlaylist.length} recent`);
}
	
	
    // ========================================================================
    // LOOP / REPEAT MODES - ✅ IMPLÉMENTÉ
    // ========================================================================
    
    /**
     * Active/désactive le loop
     * @param {boolean} enabled - Activer le loop
     * @param {number} start - Position de début (ms)
     * @param {number} end - Position de fin (ms)
     */
    setLoop(enabled, start = 0, end = null) {
        const duration = this.get('duration');
        
        this.update({
            loopEnabled: enabled,
            loopStart: start,
            loopEnd: end || duration
        });
        
        this.logger.info('PlaybackModel', 
            enabled 
                ? `Loop enabled: ${start}ms → ${end || duration}ms` 
                : 'Loop disabled');
        
        this.eventBus.emit('playback:loop-changed', {
            enabled,
            start,
            end: end || duration
        });
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
            
            this.set('position', loopStart, { silent: false });
            
            this.eventBus.emit('playback:loop-triggered', {
                from: loopEnd,
                to: loopStart
            });
        }
    }
    
    /**
     * Gère la fin du fichier
     */
    handleEndReached() {
        const repeatMode = this.get('repeatMode');
        
        this.logger.info('PlaybackModel', `End reached, repeat mode: ${repeatMode}`);
        
        switch (repeatMode) {
            case 'one':
                // Rejouer le même fichier
                this.set('position', 0);
                this.logger.debug('PlaybackModel', 'Repeating current file');
                break;
                
            case 'all':
                // Signal pour jouer le suivant
                this.eventBus.emit('playback:next-file');
                this.logger.debug('PlaybackModel', 'Requesting next file');
                break;
                
            case 'none':
            default:
                // Arrêter
                this.set('state', 'STOPPED');
                this.set('position', 0);
                this.eventBus.emit('playback:finished');
                this.logger.debug('PlaybackModel', 'Playback finished');
                break;
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Calcule le pourcentage de progression
     * @returns {number} Progression 0-100
     */
    calculateProgress() {
        const position = this.get('position');
        const duration = this.get('duration');
        
        if (duration === 0) return 0;
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
// ============================================================================
// Fichier: frontend/js/controllers/GlobalPlaybackController.js
// Version: v3.0.1 - CORRIGÉ (Intégration PlaybackModel)
// Date: 2025-10-09
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Contrôleur global singleton de lecture MIDI.
//   Utilise PlaybackModel v3.0.1 pour l'état et l'interpolation.
//   Architecture corrigée selon audit du 2025-10-09.
//
// CORRECTIONS v3.0.1:
//   ✅ Intégration complète de PlaybackModel
//   ✅ Délégation état et interpolation à PlaybackModel
//   ✅ Suppression code dupliqué
//   ✅ Loop/Repeat via PlaybackModel
//   ✅ Architecture cohérente avec autres Controllers
//   ✅ Réduction de ~175 lignes de code (46%)
//
// Responsabilités:
//   - Singleton global de lecture (une seule instance)
//   - Gestion chargement fichiers/MidiJSON
//   - Coordination backend ↔ PlaybackModel
//   - Gestion playlist et navigation
//   - Gestion routing instruments
//   - Gestion métronome
//   - Cache et préchargement
//   - Statistiques lecture
//
// Architecture:
//   GlobalPlaybackController (singleton)
//   ├── PlaybackModel v3.0.1 (état + interpolation)
//   ├── BackendService (commandes MIDI)
//   ├── EventBus (événements)
//   └── FileModel (fichiers)
//
// Design Patterns:
//   - Singleton (instance unique)
//   - Delegation (vers PlaybackModel)
//   - Observer (événements)
//
// Auteur: midiMind Team
// ============================================================================

class GlobalPlaybackController {
    // ========================================================================
    // SINGLETON - INSTANCE UNIQUE
    // ========================================================================
    
    static instance = null;
    
    static getInstance(eventBus, backend, fileModel, logger) {
        if (!GlobalPlaybackController.instance) {
            GlobalPlaybackController.instance = new GlobalPlaybackController(
                eventBus, backend, fileModel, logger
            );
        }
        return GlobalPlaybackController.instance;
    }
    
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    constructor(eventBus, backend, fileModel, logger) {
        // Vérifier singleton
        if (GlobalPlaybackController.instance) {
            return GlobalPlaybackController.instance;
        }
        
        // Dépendances
        this.eventBus = eventBus;
        this.backend = backend;
        this.fileModel = fileModel;
        this.logger = logger;
        
        // ✅ NOUVEAU: Instancier PlaybackModel pour l'état
        this.playbackModel = new PlaybackModel(eventBus, backend, logger);
        
        // ✅ NOUVEAU: Activer interpolation locale
        this.playbackModel.config.interpolationEnabled = true;
        
        // Fichier en cours
        this.currentFile = {
            id: null,
            name: null,
            path: null,
            midiJson: null,
            metadata: {}
        };
        
        // Playlist
        this.playlist = {
            files: [],
            currentIndex: -1,
            shuffle: false,
            repeat: 'none', // none, one, all
            autoAdvance: true
        };
        
        // Routing
        this.routing = new Map(); // channel -> instrumentId
        
        // Métronome
        this.metronome = {
            enabled: false,
            volume: 80,
            accentFirst: true,
            soundType: 'beep'
        };
        
        // Cache
        this.cache = {
            preloadedFiles: new Map(),
            maxCacheSize: 10,
            enabled: true
        };
        
        // Statistiques
        this.stats = {
            filesPlayed: 0,
            totalPlaytime: 0,
            seeks: 0,
            tempoChanges: 0,
            startTime: null
        };
        
        // Configuration
        this.config = {
            latencyCompensation: 0,
            velocityCurve: 'linear',
            autoPreload: true,
            syncInterval: 1000
        };
        
        this.logger.info('GlobalPlaybackController', '✓ Initialized v3.0.1 with PlaybackModel');
        
        // Connecter événements PlaybackModel
        this.connectPlaybackModelEvents();
        
        // Synchronisation backend
        this.setupBackendSync();
        
        GlobalPlaybackController.instance = this;
    }
    
    // ========================================================================
    // ÉVÉNEMENTS - ✅ NOUVEAU: Connexion PlaybackModel
    // ========================================================================
    
    connectPlaybackModelEvents() {
        // Réémettre événements PlaybackModel vers application
        this.playbackModel.eventBus.on('playback:state-changed', (data) => {
            this.eventBus.emit('playback:state-changed', data);
            
            // Actions selon l'état
            if (data.newState === 'PLAYING') {
                this.stats.startTime = Date.now();
            } else if (data.oldState === 'PLAYING') {
                this.updatePlaytimeStats();
            }
        });
        
        this.playbackModel.eventBus.on('playback:position-changed', (data) => {
            // Émettre comme playback:time-update pour compatibilité
            this.eventBus.emit('playback:time-update', {
                time: data.position,
                duration: this.playbackModel.get('duration'),
                progress: data.progress,
                formattedTime: this.playbackModel.formatPosition(),
                formattedDuration: this.playbackModel.formatDuration()
            });
        });
        
        this.playbackModel.eventBus.on('playback:loop-triggered', (data) => {
            this.logger.debug('GlobalPlaybackController', 
                `Loop triggered: ${data.from}ms → ${data.to}ms`);
            
            this.eventBus.emit('playback:loop-triggered', data);
        });
        
        this.playbackModel.eventBus.on('playback:loop-changed', (data) => {
            this.eventBus.emit('playback:loop-changed', data);
        });
        
        this.playbackModel.eventBus.on('playback:repeat-changed', (data) => {
            // Synchroniser avec playlist repeat
            this.playlist.repeat = data.mode;
            this.eventBus.emit('playback:repeat-changed', data);
        });
        
        // Détecter fin de fichier
        this.playbackModel.watch('position', (newPos) => {
            const duration = this.playbackModel.get('duration');
            
            if (newPos >= duration && !this.playbackModel.get('loopEnabled')) {
                this.handleEndReached();
            }
        });
    }
    
    // ========================================================================
    // SYNCHRONISATION BACKEND
    // ========================================================================
    
    setupBackendSync() {
        // Écouter mises à jour backend
        this.eventBus.on('backend:playback-update', (data) => {
            this.playbackModel.updateFromBackend(data);
        });
        
        // Synchronisation périodique
        setInterval(() => {
            if (this.playbackModel.isPlaying()) {
                this.syncWithBackend();
            }
        }, this.config.syncInterval);
    }
    
    async syncWithBackend() {
        try {
            const response = await this.backend.sendCommand('playback.getState');
            
            if (response.success) {
                this.playbackModel.updateFromBackend(response.data);
            }
        } catch (error) {
            this.logger.warn('GlobalPlaybackController', 
                `Backend sync failed: ${error.message}`);
        }
    }
    
    // ========================================================================
    // CHARGEMENT FICHIER
    // ========================================================================
    
    async loadFile(fileId, options = {}) {
        this.logger.info('GlobalPlaybackController', `Loading file: ${fileId}`);
        
        const {
            autoPlay = false,
            seekTo = 0
        } = options;
        
        try {
            // Récupérer fichier
            const file = await this.fileModel.get(fileId);
            
            if (!file) {
                throw new Error('File not found');
            }
            
            // Convertir en MidiJSON si nécessaire
            if (!file.midiJson) {
                throw new Error('File has no MidiJSON data');
            }
            
            // Charger dans PlaybackModel
            await this.loadMidiJson(file.midiJson, { autoPlay, seekTo });
            
            // Mettre à jour fichier courant
            this.currentFile = {
                id: fileId,
                name: file.name,
                path: file.path,
                midiJson: file.midiJson,
                metadata: file.midiJson.metadata || {}
            };
            
            this.eventBus.emit('playback:file-loaded', {
                file: this.currentFile
            });
            
            this.stats.filesPlayed++;
            
            // Précharger suivant si auto-advance
            if (this.playlist.autoAdvance && this.config.autoPreload) {
                this.preloadNextFile();
            }
            
            return true;
            
        } catch (error) {
            this.logger.error('GlobalPlaybackController', 
                `Failed to load file: ${error.message}`);
            throw error;
        }
    }
    
    async loadMidiJson(midiJson, options = {}) {
        this.logger.info('GlobalPlaybackController', 'Loading MidiJSON');
        
        const {
            autoPlay = false,
            seekTo = 0
        } = options;
        
        try {
            // Arrêter lecture en cours
            if (this.playbackModel.isPlaying()) {
                await this.stop();
            }
            
            // Extraire métadonnées
            const metadata = midiJson.metadata || {};
            
            // ✅ Mise à jour via PlaybackModel
            this.playbackModel.update({
                currentFileName: metadata.name || 'Unknown',
                duration: metadata.duration || 0,
                bpm: metadata.bpm || 120,
                trackCount: midiJson.tracks?.length || 0,
                position: seekTo
            }, { silent: true });
            
            // Envoyer au backend
            const response = await this.backend.sendCommand('playback.load', {
                midiJson: midiJson,
                position_ms: seekTo
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Backend load failed');
            }
            
            // Synchroniser état backend
            if (response.data) {
                this.playbackModel.updateFromBackend(response.data);
            }
            
            this.eventBus.emit('playback:loaded', {
                metadata: metadata,
                duration: this.playbackModel.get('duration'),
                tracks: this.playbackModel.get('trackCount')
            });
            
            // Auto-play si demandé
            if (autoPlay) {
                await this.play();
            }
            
            return true;
            
        } catch (error) {
            this.logger.error('GlobalPlaybackController', 
                `Failed to load MidiJSON: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // CONTRÔLE LECTURE - ✅ SIMPLIFIÉ (Délégation PlaybackModel)
    // ========================================================================
    
    async play() {
        if (!this.playbackModel.get('currentFileName')) {
            this.logger.warn('GlobalPlaybackController', 'Cannot play: no file loaded');
            return false;
        }
        
        this.logger.info('GlobalPlaybackController', 'Play');
        
        try {
            // Commande backend
            const response = await this.backend.sendCommand('playback.play', {
                position_ms: this.playbackModel.get('position')
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Backend play failed');
            }
            
            // ✅ Utilise PlaybackModel pour état
            this.playbackModel.set('state', 'PLAYING');
            
            // ✅ PlaybackModel démarre son interpolation automatiquement
            // (via watch('state') dans PlaybackModel)
            
            this.eventBus.emit('playback:started', {
                time: this.playbackModel.get('position'),
                duration: this.playbackModel.get('duration'),
                tempo: this.playbackModel.get('tempo'),
                file: this.currentFile.name
            });
            
            // Démarrer métronome si activé
            if (this.metronome.enabled) {
                this.startMetronome();
            }
            
            return true;
            
        } catch (error) {
            this.logger.error('GlobalPlaybackController', 
                `Failed to play: ${error.message}`);
            throw error;
        }
    }
    
    async pause() {
        if (!this.playbackModel.isPlaying()) {
            this.logger.warn('GlobalPlaybackController', 'Not playing');
            return false;
        }
        
        this.logger.info('GlobalPlaybackController', 'Pause');
        
        try {
            // Commande backend
            const response = await this.backend.sendCommand('playback.pause');
            
            if (!response.success) {
                throw new Error(response.error || 'Backend pause failed');
            }
            
            // ✅ Utilise PlaybackModel pour état
            this.playbackModel.set('state', 'PAUSED');
            
            // ✅ PlaybackModel arrête son interpolation automatiquement
            
            this.updatePlaytimeStats();
            
            this.eventBus.emit('playback:paused', {
                time: this.playbackModel.get('position'),
                duration: this.playbackModel.get('duration')
            });
            
            // Arrêter métronome
            this.stopMetronome();
            
            return true;
            
        } catch (error) {
            this.logger.error('GlobalPlaybackController', 
                `Failed to pause: ${error.message}`);
            throw error;
        }
    }
    
    async stop() {
        this.logger.info('GlobalPlaybackController', 'Stop');
        
        try {
            // Commande backend
            const response = await this.backend.sendCommand('playback.stop');
            
            if (!response.success) {
                throw new Error(response.error || 'Backend stop failed');
            }
            
            // ✅ Utilise PlaybackModel pour état
            this.playbackModel.update({
                state: 'STOPPED',
                position: 0
            });
            
            this.updatePlaytimeStats();
            
            this.eventBus.emit('playback:stopped', {
                duration: this.playbackModel.get('duration')
            });
            
            // Arrêter métronome
            this.stopMetronome();
            
            return true;
            
        } catch (error) {
            this.logger.error('GlobalPlaybackController', 
                `Failed to stop: ${error.message}`);
            throw error;
        }
    }
    
    async seek(timeMs) {
        timeMs = Math.max(0, Math.min(timeMs, this.playbackModel.get('duration')));
        
        this.logger.info('GlobalPlaybackController', `Seek to ${timeMs}ms`);
        
        try {
            // Commande backend
            const response = await this.backend.sendCommand('playback.seek', {
                position_ms: timeMs
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Backend seek failed');
            }
            
            // ✅ Met à jour via PlaybackModel
            this.playbackModel.set('position', timeMs);
            
            // PlaybackModel calculera automatiquement le progress
            // et émettra 'playback:position-changed'
            
            this.stats.seeks++;
            
            this.eventBus.emit('playback:seeked', {
                time: timeMs,
                duration: this.playbackModel.get('duration'),
                progress: this.playbackModel.get('progress')
            });
            
            return true;
            
        } catch (error) {
            this.logger.error('GlobalPlaybackController', 
                `Failed to seek: ${error.message}`);
            throw error;
        }
    }
    
    async seekPercent(percent) {
        const duration = this.playbackModel.get('duration');
        const timeMs = (percent / 100) * duration;
        return await this.seek(timeMs);
    }
    
    // ========================================================================
    // TEMPO CONTROL
    // ========================================================================
    
    async setTempo(percent) {
        percent = Math.max(50, Math.min(200, percent));
        
        this.logger.info('GlobalPlaybackController', `Set tempo to ${percent}%`);
        
        try {
            // Commande backend
            const response = await this.backend.sendCommand('playback.setTempo', {
                tempo_percent: percent
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Backend tempo failed');
            }
            
            // ✅ Met à jour via PlaybackModel
            this.playbackModel.set('tempo', percent / 100);
            
            this.stats.tempoChanges++;
            
            this.eventBus.emit('playback:tempo-changed', {
                tempo: percent / 100,
                percent: percent
            });
            
            return true;
            
        } catch (error) {
            this.logger.error('GlobalPlaybackController', 
                `Failed to set tempo: ${error.message}`);
            throw error;
        }
    }
    
    async adjustTempo(delta) {
        const currentTempo = this.playbackModel.get('tempo') * 100;
        return await this.setTempo(currentTempo + delta);
    }
    
    async resetTempo() {
        return await this.setTempo(100);
    }
    
    // ========================================================================
    // LOOP CONTROL - ✅ SIMPLIFIÉ (Délégation PlaybackModel)
    // ========================================================================
    
    setLoop(enabled, start = 0, end = 0) {
        if (enabled) {
            // ✅ Délègue à PlaybackModel
            this.playbackModel.setLoopPoints(
                start, 
                end || this.playbackModel.get('duration')
            );
        } else {
            this.playbackModel.set('loopEnabled', false);
        }
        
        // PlaybackModel émettra 'playback:loop-changed'
    }
    
    toggleLoop() {
        const currentState = this.playbackModel.get('loopEnabled');
        this.setLoop(!currentState, 0, this.playbackModel.get('duration'));
    }
    
    setRepeatMode(mode) {
        // ✅ Nouvelle fonctionnalité de PlaybackModel v3.0.1
        this.playbackModel.setRepeatMode(mode); // none, one, all
        this.playlist.repeat = mode;
    }
    
    // ========================================================================
    // MÉTRONOME
    // ========================================================================
    
    setMetronome(enabled) {
        this.metronome.enabled = enabled;
        
        if (enabled && this.playbackModel.isPlaying()) {
            this.startMetronome();
        } else {
            this.stopMetronome();
        }
        
        this.eventBus.emit('metronome:changed', {
            enabled: enabled
        });
    }
    
    startMetronome() {
        // TODO: Implémenter métronome
        this.logger.debug('GlobalPlaybackController', 'Metronome started');
    }
    
    stopMetronome() {
        // TODO: Implémenter métronome
        this.logger.debug('GlobalPlaybackController', 'Metronome stopped');
    }
    
    // ========================================================================
    // PLAYLIST NAVIGATION
    // ========================================================================
    
    async next() {
        if (this.playlist.files.length === 0) {
            this.logger.warn('GlobalPlaybackController', 'No playlist');
            return false;
        }
        
        const wasPlaying = this.playbackModel.isPlaying();
        
        // Calculer index suivant
        let nextIndex = this.playlist.currentIndex + 1;
        
        if (nextIndex >= this.playlist.files.length) {
            if (this.playlist.repeat === 'all') {
                nextIndex = 0;
            } else {
                this.logger.info('GlobalPlaybackController', 'End of playlist');
                await this.stop();
                return false;
            }
        }
        
        this.playlist.currentIndex = nextIndex;
        const nextFile = this.playlist.files[nextIndex];
        
        // Charger et jouer si nécessaire
        await this.loadFile(nextFile.id, { autoPlay: wasPlaying });
        
        return true;
    }
    
    async previous() {
        if (this.playlist.files.length === 0) {
            this.logger.warn('GlobalPlaybackController', 'No playlist');
            return false;
        }
        
        const wasPlaying = this.playbackModel.isPlaying();
        
        // Si on est au début du fichier, aller au précédent
        // Sinon, revenir au début
        if (this.playbackModel.get('position') > 3000) {
            await this.seek(0);
            return true;
        }
        
        // Calculer index précédent
        let prevIndex = this.playlist.currentIndex - 1;
        
        if (prevIndex < 0) {
            if (this.playlist.repeat === 'all') {
                prevIndex = this.playlist.files.length - 1;
            } else {
                prevIndex = 0;
            }
        }
        
        this.playlist.currentIndex = prevIndex;
        const prevFile = this.playlist.files[prevIndex];
        
        // Charger et jouer si nécessaire
        await this.loadFile(prevFile.id, { autoPlay: wasPlaying });
        
        return true;
    }
    
    // ========================================================================
    // FIN DE FICHIER
    // ========================================================================
    
    async handleEndReached() {
        this.logger.info('GlobalPlaybackController', 'End of file reached');
        
        this.eventBus.emit('playback:ended', {
            file: this.currentFile.name
        });
        
        // Selon mode repeat
        if (this.playlist.repeat === 'one') {
            await this.seek(0);
            await this.play();
            return;
        }
        
        // Auto-advance si activé
        if (this.playlist.autoAdvance && this.playlist.files.length > 0) {
            await this.next();
        } else {
            await this.stop();
        }
    }
    
    // ========================================================================
    // PLAYLIST MANAGEMENT
    // ========================================================================
    
    setPlaylist(files) {
        this.playlist.files = files || [];
        this.playlist.currentIndex = -1;
        
        this.eventBus.emit('playlist:changed', {
            count: this.playlist.files.length
        });
    }
    
    addToPlaylist(file) {
        this.playlist.files.push(file);
        
        this.eventBus.emit('playlist:file-added', { file });
    }
    
    removeFromPlaylist(index) {
        if (index >= 0 && index < this.playlist.files.length) {
            const removed = this.playlist.files.splice(index, 1)[0];
            
            // Ajuster index courant
            if (index < this.playlist.currentIndex) {
                this.playlist.currentIndex--;
            } else if (index === this.playlist.currentIndex) {
                // Fichier courant supprimé
                this.playlist.currentIndex = -1;
            }
            
            this.eventBus.emit('playlist:file-removed', { file: removed });
        }
    }
    
    clearPlaylist() {
        this.playlist.files = [];
        this.playlist.currentIndex = -1;
        
        this.eventBus.emit('playlist:cleared');
    }
    
    // ========================================================================
    // ROUTING
    // ========================================================================
    
    setRouting(channel, instrumentId) {
        if (instrumentId) {
            this.routing.set(channel, instrumentId);
        } else {
            this.routing.delete(channel);
        }
        
        // Envoyer au backend
        this.backend.sendCommand('routing.set', {
            channel: channel,
            instrument_id: instrumentId
        }).catch(err => {
            this.logger.warn('GlobalPlaybackController', 
                `Failed to set routing: ${err.message}`);
        });
        
        this.eventBus.emit('routing:changed', {
            channel: channel,
            instrumentId: instrumentId
        });
    }
    
    clearRouting() {
        this.routing.clear();
        
        this.backend.sendCommand('routing.clear').catch(err => {
            this.logger.warn('GlobalPlaybackController', 
                `Failed to clear routing: ${err.message}`);
        });
        
        this.eventBus.emit('routing:cleared');
    }
    
    getRouting() {
        return new Map(this.routing);
    }
    
    // ========================================================================
    // CACHE & PRÉCHARGEMENT
    // ========================================================================
    
    async preloadNextFile() {
        if (!this.config.autoPreload) return;
        
        const nextIndex = this.playlist.currentIndex + 1;
        
        if (nextIndex < this.playlist.files.length) {
            const nextFile = this.playlist.files[nextIndex];
            
            try {
                const file = await this.fileModel.get(nextFile.id);
                
                if (file && file.midiJson) {
                    this.cache.preloadedFiles.set(nextFile.id, file);
                    
                    // Limiter taille cache
                    if (this.cache.preloadedFiles.size > this.cache.maxCacheSize) {
                        const firstKey = this.cache.preloadedFiles.keys().next().value;
                        this.cache.preloadedFiles.delete(firstKey);
                    }
                    
                    this.logger.debug('GlobalPlaybackController', 
                        `Preloaded: ${file.name}`);
                }
            } catch (error) {
                this.logger.warn('GlobalPlaybackController', 
                    `Failed to preload: ${error.message}`);
            }
        }
    }
    
    clearCache() {
        this.cache.preloadedFiles.clear();
        this.logger.info('GlobalPlaybackController', 'Cache cleared');
    }
    
    // ========================================================================
    // GETTERS D'ÉTAT - ✅ SIMPLIFIÉ (Délégation PlaybackModel)
    // ========================================================================
    
    getState() {
        return {
            // État PlaybackModel
            status: this.playbackModel.get('state'),
            position: this.playbackModel.get('position'),
            duration: this.playbackModel.get('duration'),
            progress: this.playbackModel.get('progress'),
            tempo: this.playbackModel.get('tempo'),
            transpose: this.playbackModel.get('transpose'),
            volume: this.playbackModel.get('volume'),
            
            // État local
            file: this.currentFile,
            playlist: {
                ...this.playlist,
                files: this.playlist.files.length
            },
            metronome: this.metronome.enabled,
            routing: this.routing.size
        };
    }
    
    getCurrentTime() {
        return this.playbackModel.get('position'); // ✅ Délègue
    }
    
    getDuration() {
        return this.playbackModel.get('duration'); // ✅ Délègue
    }
    
    getProgress() {
        return this.playbackModel.get('progress'); // ✅ Délègue
    }
    
    isPlaying() {
        return this.playbackModel.get('state') === 'PLAYING'; // ✅ Délègue
    }
    
    isPaused() {
        return this.playbackModel.get('state') === 'PAUSED'; // ✅ Délègue
    }
    
    isStopped() {
        return this.playbackModel.get('state') === 'STOPPED'; // ✅ Délègue
    }
    
    getCurrentFile() {
        return this.currentFile;
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    updatePlaytimeStats() {
        if (this.stats.startTime) {
            const elapsed = Date.now() - this.stats.startTime;
            this.stats.totalPlaytime += elapsed;
            this.stats.startTime = null;
        }
    }
    
    getStats() {
        return {
            filesPlayed: this.stats.filesPlayed,
            totalPlaytime: this.stats.totalPlaytime,
            seeks: this.stats.seeks,
            tempoChanges: this.stats.tempoChanges,
            cacheSize: this.cache.preloadedFiles.size,
            playbackInfo: this.playbackModel.getPlaybackInfo()
        };
    }
    
    resetStats() {
        this.stats = {
            filesPlayed: 0,
            totalPlaytime: 0,
            seeks: 0,
            tempoChanges: 0,
            startTime: null
        };
        
        this.logger.info('GlobalPlaybackController', 'Stats reset');
    }
    
    // ========================================================================
    // NETTOYAGE
    // ========================================================================
    
    destroy() {
        this.logger.info('GlobalPlaybackController', 'Destroying...');
        
        // Arrêter lecture
        this.stop().catch(() => {});
        
        // Nettoyer PlaybackModel
        if (this.playbackModel) {
            this.playbackModel.destroy();
        }
        
        // Nettoyer cache
        this.clearCache();
        
        // Réinitialiser singleton
        GlobalPlaybackController.instance = null;
        
        this.logger.info('GlobalPlaybackController', '✓ Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = GlobalPlaybackController;
}

if (typeof window !== 'undefined') {
    window.GlobalPlaybackController = GlobalPlaybackController;
}
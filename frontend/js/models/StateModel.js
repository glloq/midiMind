// ============================================================================
// Fichier: frontend/js/models/StateModel.js
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   ModÃ¨le d'Ã©tat global de l'application (Single Source of Truth).
//   GÃ¨re l'Ã©tat central : page courante, fichier/playlist en cours,
//   Ã©tat de lecture, paramÃ¨tres utilisateur, statistiques de session.
//
// FonctionnalitÃ©s:
//   - Ã‰tat global application (navigation, sÃ©lection, lecture)
//   - Validation automatique des donnÃ©es
//   - PropriÃ©tÃ©s calculÃ©es (computed properties)
//   - Watchers pour rÃ©activitÃ©
//   - Persistence automatique (localStorage)
//   - Gestion session et statistiques
//   - Export/Import Ã©tat complet
//
// Architecture:
//   StateModel extends BaseModel
//   - Validation avec rÃ¨gles personnalisÃ©es
//   - Computed properties automatiques
//   - Watchers sur changements d'Ã©tat
//   - SÃ©rialisation/dÃ©sÃ©rialisation
//
// Auteur: MidiMind Team
// ============================================================================


class StateModel extends BaseModel {
    constructor(eventBus) {
        super(eventBus);
        
        // Configuration du modÃ¨le
        this.config.autoValidate = true;
        this.config.persistOnChange = true;
        
        // Ã‰tat initial de l'application avec valeurs par dÃ©faut
        this.initialize({
            // Navigation et interface
            currentPage: 'home',
            previousPage: null,
            
            // SÃ©lection de contenu
            currentFile: null,
            currentPlaylist: null,
            selectorMode: 'file', // 'file' ou 'playlist'
            
            // Ã‰tat de lecture
            isPlaying: false,
            isPaused: false,
            progress: 0,
            duration: 0,
            currentTrackIndex: 0,
            playbackRate: 1.0,
            volume: 100,
            
            // Interface utilisateur
            zoomLevel: 100,
            viewMode: 'normal', // 'normal', 'compact', 'minimal'
            debugMode: false,
            
            // ParamÃ¨tres utilisateur
            settings: {
                // Lecture
                pauseBetweenTracks: 3,
                startDelay: 100,
                autoplay: false,
                repeatMode: 'none', // 'none', 'one', 'all'
                shuffleMode: false,
                crossfade: 0,
                
                // Interface
                darkMode: false,
                compactMode: false,
                hideDebugButton: false,
                showWelcomeScreen: true,
                autoSaveSettings: true,
                
                // MIDI et audio
                noteDisplayWindow: 30,
                visualizerRefreshRate: 30,
                visualizerTimeWindow: 10,
                midiLatencyCompensation: 0,
                audioBufferSize: 512,
                
                // Performance
                enableHardwareAcceleration: true,
                maxConcurrentNotes: 128,
                enableDebugLogging: false,
                enablePerformanceMetrics: false,
                
                // Personnalisation
                theme: 'default',
                language: 'fr',
                dateFormat: 'DD/MM/YYYY',
                timeFormat: '24h'
            },
            
            // Ã‰tat temporaire/cache
            lastFileAccess: null,
            recentFiles: [],
            recentPlaylists: [],
            sessionStartTime: new Date().toISOString(),
            
            // Statistiques de session
            stats: {
                filesPlayed: 0,
                totalPlayTime: 0,
                sessionsCount: 1,
                errorsCount: 0,
                actionsCount: 0
            }
        });
        
        // Configurer les rÃ¨gles de validation
        this.setupValidation();
        
        // Configurer les propriÃ©tÃ©s calculÃ©es
        this.setupComputedProperties();
        
        // Configurer les observateurs
        this.setupWatchers();
        
        // Charger les paramÃ¨tres persistants
        this.loadPersistedState();
    }

    /**
     * Configurer les rÃ¨gles de validation
     */
    setupValidation() {
        // Validation de la page courante
        this.addValidationRule('currentPage', (value) => {
            const validPages = ['home', 'files', 'instruments', 'keyboard', 'system'];
            if (!validPages.includes(value)) {
                return `Page invalide. Pages valides: ${validPages.join(', ')}`;
            }
            return true;
        });
        
        // Validation du mode sÃ©lecteur
        this.addValidationRule('selectorMode', (value) => {
            const validModes = ['file', 'playlist'];
            if (!validModes.includes(value)) {
                return 'Mode sÃ©lecteur invalide. Modes valides: file, playlist';
            }
            return true;
        });
        
        // Validation du niveau de zoom
        this.addValidationRule('zoomLevel', (value) => {
            if (typeof value !== 'number' || value < 25 || value > 500) {
                return 'Le niveau de zoom doit Ãªtre entre 25 et 500';
            }
            return true;
        });
        
        // Validation du volume
        this.addValidationRule('volume', (value) => {
            if (typeof value !== 'number' || value < 0 || value > 100) {
                return 'Le volume doit Ãªtre entre 0 et 100';
            }
            return true;
        });
        
        // Validation du taux de lecture
        this.addValidationRule('playbackRate', (value) => {
            if (typeof value !== 'number' || value < 0.25 || value > 3.0) {
                return 'Le taux de lecture doit Ãªtre entre 0.25 et 3.0';
            }
            return true;
        });
        
        // Validation des paramÃ¨tres
        this.addValidationRule('settings', (settings) => {
            if (!settings || typeof settings !== 'object') {
                return 'Les paramÃ¨tres doivent Ãªtre un objet';
            }
            
            // Validation des paramÃ¨tres spÃ©cifiques
            const validations = [
                () => settings.pauseBetweenTracks >= 0 && settings.pauseBetweenTracks <= 30 || 'Pause entre pistes: 0-30s',
                () => settings.startDelay >= 0 && settings.startDelay <= 5000 || 'DÃ©lai de dÃ©marrage: 0-5000ms',
                () => settings.noteDisplayWindow >= 5 && settings.noteDisplayWindow <= 120 || 'FenÃªtre d\'affichage: 5-120s',
                () => settings.visualizerRefreshRate >= 1 && settings.visualizerRefreshRate <= 120 || 'Taux de rafraÃ®chissement: 1-120fps',
                () => settings.maxConcurrentNotes >= 16 && settings.maxConcurrentNotes <= 512 || 'Notes simultanÃ©es max: 16-512'
            ];
            
            for (const validation of validations) {
                const result = validation();
                if (result !== true) {
                    return result;
                }
            }
            
            return true;
        });
    }

    /**
     * Configurer les propriÃ©tÃ©s calculÃ©es
     */
    setupComputedProperties() {
        // Calculer si du contenu est sÃ©lectionnÃ©
        this.addComputed('hasSelection', (data) => {
            return !!(data.currentFile || data.currentPlaylist);
        });
        
        // Calculer l'Ã©tat de lecture complet
        this.addComputed('playbackState', (data) => {
            return {
                isActive: data.isPlaying || data.isPaused,
                canPlay: !!(data.currentFile || (data.currentPlaylist && data.currentPlaylist.files.length > 0)),
                progressPercent: data.duration > 0 ? (data.progress / data.duration) * 100 : 0,
                remainingTime: Math.max(0, data.duration - data.progress),
                formattedProgress: this.formatTime(data.progress),
                formattedDuration: this.formatTime(data.duration),
                formattedRemaining: this.formatTime(Math.max(0, data.duration - data.progress))
            };
        });
        
        // Calculer les informations de session
        this.addComputed('sessionInfo', (data) => {
            const sessionDuration = Date.now() - new Date(data.sessionStartTime).getTime();
            return {
                duration: sessionDuration,
                formattedDuration: this.formatDuration(sessionDuration),
                actionsPerMinute: data.stats.actionsCount / (sessionDuration / 60000),
                errorRate: data.stats.errorsCount / Math.max(1, data.stats.actionsCount),
                efficiency: data.stats.filesPlayed / Math.max(1, data.stats.actionsCount)
            };
        });
        
        // Calculer l'Ã©tat de l'interface
        this.addComputed('uiState', (data) => {
            return {
                theme: data.settings.darkMode ? 'dark' : 'light',
                density: data.settings.compactMode ? 'compact' : 'normal',
                debugVisible: data.debugMode && !data.settings.hideDebugButton,
                accessibility: {
                    reducedMotion: data.settings.reducedMotion || false,
                    highContrast: data.settings.highContrast || false,
                    largeText: data.settings.largeText || false
                }
            };
        });
        
        // Calculer les performances
        this.addComputed('performance', (data) => {
            const sessionDuration = Date.now() - new Date(data.sessionStartTime).getTime();
            return {
                memoryUsage: this.estimateMemoryUsage(),
                renderingLoad: this.calculateRenderingLoad(data),
                midiLoad: this.calculateMidiLoad(data),
                recommendations: this.generatePerformanceRecommendations(data)
            };
        });
    }

    /**
     * Configurer les observateurs de changement
     */
    setupWatchers() {
        // Observer les changements de page
        this.watch('currentPage', (newPage, oldPage) => {
            if (oldPage) {
                this.set('previousPage', oldPage, { silent: true });
            }
            this.emitEvent('page:changed', { from: oldPage, to: newPage });
        });
        
        // Observer les changements de fichier/playlist
        this.watch('currentFile', (newFile, oldFile) => {
            if (newFile) {
                this.addToRecentFiles(newFile);
                this.set('currentPlaylist', null, { silent: true });
            }
            this.updatePlaybackCapabilities();
        });
        
        this.watch('currentPlaylist', (newPlaylist, oldPlaylist) => {
            if (newPlaylist) {
                this.addToRecentPlaylists(newPlaylist);
                this.set('currentFile', null, { silent: true });
            }
            this.updatePlaybackCapabilities();
        });
        
        // Observer les changements d'Ã©tat de lecture
        this.watch('isPlaying', (isPlaying) => {
            if (isPlaying) {
                this.incrementStat('filesPlayed');
                this.set('isPaused', false, { silent: true });
            }
            this.emitEvent('playback:state:changed', { isPlaying, isPaused: this.get('isPaused') });
        });
        
        // Observer les changements de paramÃ¨tres
        this.watch('settings', (newSettings, oldSettings) => {
            this.handleSettingsChange(newSettings, oldSettings);
        });
        
        // Observer les changements de zoom
        this.watch('zoomLevel', (newZoom) => {
            this.emitEvent('ui:zoom:changed', { zoomLevel: newZoom });
        });
    }

    // ===== MÃ‰THODES PUBLIQUES =====

    /**
     * SÃ©lectionner un fichier MIDI (dÃ©sÃ©lectionne la playlist)
     * @param {Object} file - Objet fichier MIDI
     */
    setCurrentFile(file) {
        this.set('currentFile', file);
        if (file) {
            this.set('selectorMode', 'file');
            this.incrementStat('actionsCount');
        }
    }

    /**
     * SÃ©lectionner une playlist (dÃ©sÃ©lectionne le fichier)
     * @param {Object} playlist - Objet playlist
     */
    setCurrentPlaylist(playlist) {
        this.set('currentPlaylist', playlist);
        if (playlist) {
            this.set('selectorMode', 'playlist');
            this.incrementStat('actionsCount');
        }
    }

    /**
     * Changer de page
     * @param {string} page - Nom de la page
     */
    setCurrentPage(page) {
        this.set('currentPage', page);
        this.incrementStat('actionsCount');
    }

    /**
     * Mettre Ã  jour l'Ã©tat de lecture
     * @param {Object} playbackState - Nouvel Ã©tat
     */
    setPlaybackState(playbackState) {
        this.update({
            isPlaying: playbackState.isPlaying ?? this.get('isPlaying'),
            isPaused: playbackState.isPaused ?? this.get('isPaused'),
            progress: playbackState.progress ?? this.get('progress'),
            duration: playbackState.duration ?? this.get('duration'),
            currentTrackIndex: playbackState.currentTrackIndex ?? this.get('currentTrackIndex')
        });
    }

    /**
     * Mettre Ã  jour un paramÃ¨tre
     * @param {string} key - ClÃ© du paramÃ¨tre
     * @param {*} value - Nouvelle valeur
     */
    setSetting(key, value) {
        const settings = { ...this.get('settings') };
        settings[key] = value;
        this.set('settings', settings);
    }

    /**
     * Mettre Ã  jour plusieurs paramÃ¨tres
     * @param {Object} newSettings - Nouveaux paramÃ¨tres
     */
    updateSettings(newSettings) {
        const settings = { ...this.get('settings'), ...newSettings };
        this.set('settings', settings);
    }

    /**
     * Incrementer une statistique
     * @param {string} statKey - ClÃ© de la statistique
     * @param {number} amount - Montant Ã  ajouter (dÃ©faut: 1)
     */
    incrementStat(statKey, amount = 1) {
        const stats = { ...this.get('stats') };
        stats[statKey] = (stats[statKey] || 0) + amount;
        this.set('stats', stats);
    }

    /**
     * RÃ©initialiser les statistiques de session
     */
    resetSessionStats() {
        this.set('stats', {
            filesPlayed: 0,
            totalPlayTime: 0,
            sessionsCount: this.get('stats').sessionsCount + 1,
            errorsCount: 0,
            actionsCount: 0
        });
        this.set('sessionStartTime', new Date().toISOString());
    }

    /**
     * Obtenir l'Ã©tat complet de lecture
     * @returns {Object} Ã‰tat de lecture calculÃ©
     */
    getPlaybackState() {
        return this.get('_computed_playbackState');
    }

    /**
     * Obtenir les informations de session
     * @returns {Object} Informations de session calculÃ©es
     */
    getSessionInfo() {
        return this.get('_computed_sessionInfo');
    }

    /**
     * Obtenir l'Ã©tat de l'interface
     * @returns {Object} Ã‰tat UI calculÃ©
     */
    getUIState() {
        return this.get('_computed_uiState');
    }

    /**
     * Obtenir les mÃ©triques de performance
     * @returns {Object} MÃ©triques de performance
     */
    getPerformanceMetrics() {
        return this.get('_computed_performance');
    }

    /**
     * Exporter l'Ã©tat pour sauvegarde
     * @returns {Object} Ã‰tat sÃ©rialisÃ©
     */
    exportState() {
        return this.serialize(['settings', 'recentFiles', 'recentPlaylists', 'stats']);
    }

    /**
     * Importer un Ã©tat sauvegardÃ©
     * @param {Object} savedState - Ã‰tat Ã  importer
     */
    importState(savedState) {
        if (savedState.data) {
            this.update(savedState.data);
        }
    }

    // ===== MÃ‰THODES PRIVÃ‰ES =====

    /**
     * Ajouter un fichier aux rÃ©cents
     * @param {Object} file - Fichier Ã  ajouter
     */
    addToRecentFiles(file) {
        const recentFiles = [...this.get('recentFiles')];
        
        // Supprimer s'il existe dÃ©jÃ 
        const existingIndex = recentFiles.findIndex(f => f.id === file.id);
        if (existingIndex > -1) {
            recentFiles.splice(existingIndex, 1);
        }
        
        // Ajouter en premiÃ¨re position
        recentFiles.unshift({
            id: file.id,
            name: file.name,
            path: file.path,
            lastAccess: new Date().toISOString()
        });
        
        // Limiter Ã  10 fichiers rÃ©cents
        if (recentFiles.length > 10) {
            recentFiles.splice(10);
        }
        
        this.set('recentFiles', recentFiles, { silent: true });
        this.set('lastFileAccess', new Date().toISOString(), { silent: true });
    }

    /**
     * Ajouter une playlist aux rÃ©centes
     * @param {Object} playlist - Playlist Ã  ajouter
     */
    addToRecentPlaylists(playlist) {
        const recentPlaylists = [...this.get('recentPlaylists')];
        
        // Supprimer s'il existe dÃ©jÃ 
        const existingIndex = recentPlaylists.findIndex(p => p.id === playlist.id);
        if (existingIndex > -1) {
            recentPlaylists.splice(existingIndex, 1);
        }
        
        // Ajouter en premiÃ¨re position
        recentPlaylists.unshift({
            id: playlist.id,
            name: playlist.name,
            lastAccess: new Date().toISOString()
        });
        
        // Limiter Ã  5 playlists rÃ©centes
        if (recentPlaylists.length > 5) {
            recentPlaylists.splice(5);
        }
        
        this.set('recentPlaylists', recentPlaylists, { silent: true });
    }

    /**
     * Mettre Ã  jour les capacitÃ©s de lecture
     */
    updatePlaybackCapabilities() {
        const hasFile = !!this.get('currentFile');
        const hasPlaylist = !!(this.get('currentPlaylist')?.files?.length > 0);
        
        this.emitEvent('playback:capabilities:changed', {
            canPlay: hasFile || hasPlaylist,
            hasContent: hasFile || hasPlaylist,
            mode: this.get('selectorMode')
        });
    }

    /**
     * GÃ©rer les changements de paramÃ¨tres
     * @param {Object} newSettings - Nouveaux paramÃ¨tres
     * @param {Object} oldSettings - Anciens paramÃ¨tres
     */
    handleSettingsChange(newSettings, oldSettings) {
        // DÃ©tecter les changements critiques
        const criticalChanges = [];
        
        if (newSettings.darkMode !== oldSettings?.darkMode) {
            criticalChanges.push('theme');
            this.emitEvent('theme:changed', { darkMode: newSettings.darkMode });
        }
        
        if (newSettings.compactMode !== oldSettings?.compactMode) {
            criticalChanges.push('layout');
            this.emitEvent('layout:changed', { compactMode: newSettings.compactMode });
        }
        
        if (newSettings.visualizerRefreshRate !== oldSettings?.visualizerRefreshRate) {
            criticalChanges.push('visualizer');
            this.emitEvent('visualizer:settings:changed', { 
                refreshRate: newSettings.visualizerRefreshRate 
            });
        }
        
        // Sauvegarder si auto-save activÃ©
        if (newSettings.autoSaveSettings) {
            this.persistState();
        }
        
        // Ã‰mettre Ã©vÃ©nement global
        this.emitEvent('settings:changed', {
            settings: newSettings,
            criticalChanges
        });
    }

    /**
     * Charger l'Ã©tat persistant
     */
    loadPersistedState() {
        try {
            const saved = localStorage.getItem('midimind_state');
            if (saved) {
                const parsedState = JSON.parse(saved);
                this.importState(parsedState);
            }
        } catch (error) {
            console.warn('Erreur lors du chargement de l\'Ã©tat:', error);
        }
    }

    /**
     * Sauvegarder l'Ã©tat persistant
     */
    persistState() {
        try {
            const stateToSave = this.exportState();
            localStorage.setItem('midimind_state', JSON.stringify(stateToSave));
        } catch (error) {
            console.warn('Erreur lors de la sauvegarde de l\'Ã©tat:', error);
        }
    }

    /**
     * Estimer l'usage mÃ©moire
     * @returns {Object} Estimation de la mÃ©moire
     */
    estimateMemoryUsage() {
        const base = 50; // MB de base
        const perFile = 2; // MB par fichier rÃ©cent
        const perPlaylist = 0.5; // MB par playlist
        
        return {
            estimated: base + 
                      (this.get('recentFiles').length * perFile) + 
                      (this.get('recentPlaylists').length * perPlaylist),
            files: this.get('recentFiles').length * perFile,
            playlists: this.get('recentPlaylists').length * perPlaylist
        };
    }

    /**
     * Calculer la charge de rendu
     * @param {Object} data - DonnÃ©es du modÃ¨le
     * @returns {number} Charge de rendu (0-1)
     */
    calculateRenderingLoad(data) {
        let load = 0;
        
        // Facteurs de charge
        load += data.zoomLevel > 150 ? 0.2 : 0;
        load += data.settings.visualizerRefreshRate > 60 ? 0.3 : 0;
        load += data.settings.noteDisplayWindow > 60 ? 0.1 : 0;
        load += data.debugMode ? 0.1 : 0;
        
        return Math.min(1, load);
    }

    /**
     * Calculer la charge MIDI
     * @param {Object} data - DonnÃ©es du modÃ¨le
     * @returns {number} Charge MIDI (0-1)
     */
    calculateMidiLoad(data) {
        let load = 0;
        
        if (data.isPlaying) {
            load += 0.3;
            load += data.settings.maxConcurrentNotes > 256 ? 0.3 : 0.1;
            load += data.playbackRate > 1.5 ? 0.2 : 0;
        }
        
        return Math.min(1, load);
    }

    /**
     * GÃ©nÃ©rer des recommandations de performance
     * @param {Object} data - DonnÃ©es du modÃ¨le
     * @returns {Array} Liste de recommandations
     */
    generatePerformanceRecommendations(data) {
        const recommendations = [];
        
        if (data.zoomLevel > 200) {
            recommendations.push({
                type: 'warning',
                message: 'Niveau de zoom Ã©levÃ© - peut affecter les performances',
                action: 'RÃ©duire le zoom Ã  150% ou moins'
            });
        }
        
        if (data.settings.visualizerRefreshRate > 60) {
            recommendations.push({
                type: 'info',
                message: 'Taux de rafraÃ®chissement Ã©levÃ© dÃ©tectÃ©',
                action: 'RÃ©duire Ã  60fps pour de meilleures performances'
            });
        }
        
        if (data.settings.maxConcurrentNotes > 256) {
            recommendations.push({
                type: 'warning',
                message: 'Limite de notes simultanÃ©es Ã©levÃ©e',
                action: 'RÃ©duire Ã  256 notes maximum'
            });
        }
        
        return recommendations;
    }

    /**
     * Formater un temps en secondes
     * @param {number} seconds - Temps en secondes
     * @returns {string} Temps formatÃ© (MM:SS)
     */
    formatTime(seconds) {
        if (typeof seconds !== 'number' || isNaN(seconds)) {
            return '0:00';
        }
        
        const mins = Math.floor(seconds / 60);
        const secs = Math.floor(seconds % 60);
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    }

    /**
     * Formater une durÃ©e en millisecondes
     * @param {number} ms - DurÃ©e en millisecondes
     * @returns {string} DurÃ©e formatÃ©e
     */
    formatDuration(ms) {
        const seconds = Math.floor(ms / 1000);
        const minutes = Math.floor(seconds / 60);
        const hours = Math.floor(minutes / 60);
        
        if (hours > 0) {
            return `${hours}h ${minutes % 60}min`;
        } else if (minutes > 0) {
            return `${minutes}min ${seconds % 60}s`;
        } else {
            return `${seconds}s`;
        }
    }

    /**
     * Surcharge de persist pour sauvegarder automatiquement
     */
    async persist() {
        this.persistState();
        return Promise.resolve();
    }
}
// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = StateModel;
}

if (typeof window !== 'undefined') {
    window.StateModel = StateModel;
}

// Export par défaut
window.StateModel = StateModel;
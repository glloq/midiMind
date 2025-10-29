// ============================================================================
// Fichier: frontend/js/models/StateModel.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.2 - MINIMAL FIX (Logger + EventBus fallbacks, NO DOWNGRADE)
// Date: 2025-10-29
// ============================================================================
// CORRECTIONS v3.0.1:
//   ✅ CRITIQUE: Fixed super() call - now compatible with BaseModel(initialData, options)
//   ✅ CRITIQUE: Constructor passes initialData directly instead of using this.initialize()
//   ✅ EventBus assigned as property after super() for consistency
// ============================================================================
// Description:
//   Modèle d'état global de l'application (Single Source of Truth).
//   Gère l'état central : page courante, fichier/playlist en cours,
//   état de lecture, paramètres utilisateur, statistiques de session.
//
// Fonctionnalités:
//   - État global application (navigation, sélection, lecture)
//   - Validation automatique des données
//   - Propriétés calculées (computed properties)
//   - Watchers pour réactivité
//   - Persistence automatique (localStorage)
//   - Gestion session et statistiques
//   - Export/Import état complet
//
// Architecture:
//   StateModel extends BaseModel
//   - Validation avec règles personnalisées
//   - Computed properties automatiques
//   - Watchers sur changements d'état
//   - Sérialisation/désérialisation
//
// Auteur: MidiMind Team
// ============================================================================

class StateModel extends BaseModel {
    constructor(eventBus) {
        // ✅ FIX v3.0.1: Correct super() call compatible with BaseModel(initialData, options)
        super({
            // Navigation et interface
            currentPage: 'home',
            previousPage: null,
            
            // Sélection de contenu
            currentFile: null,
            currentPlaylist: null,
            selectorMode: 'file', // 'file' ou 'playlist'
            
            // État de lecture
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
            
            // Paramètres utilisateur
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
            
            // État temporaire/cache
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
        }, {
            persistKey: 'statemodel',
            eventPrefix: 'state',
            autoPersist: true
        });
        
        // ✅ FIX v3.0.1: Assign eventBus as property for consistency
        // ✅ FIX v3.0.2: EventBus avec fallbacks multiples pour robustesse
        this.eventBus = eventBus || window.EventBus || window.eventBus;
        
        // ✅ FIX v3.0.2: Logger utilise l'instance (window.logger) pas la classe
        this.logger = window.logger || console;
        
        // ✅ FIX v3.0.2: Validation des dépendances critiques
        if (!this.eventBus) {
            console.error('[StateModel] EventBus not available!');
        }
        
        // Configuration du modèle
        this.config.autoValidate = true;
        this.config.persistOnChange = true;
        
        // Configurer les règles de validation
        this.setupValidation();
        
        // Configurer les propriétés calculées
        this.setupComputedProperties();
        
        // Configurer les observateurs
        this.setupWatchers();
        
        // Charger les paramètres persistants
        this.loadPersistedState();
    }

    /**
     * Configurer les règles de validation
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
        
        // Validation du mode sélecteur
        this.addValidationRule('selectorMode', (value) => {
            const validModes = ['file', 'playlist'];
            if (!validModes.includes(value)) {
                return 'Mode sélecteur invalide. Modes valides: file, playlist';
            }
            return true;
        });
        
        // Validation du niveau de zoom
        this.addValidationRule('zoomLevel', (value) => {
            if (typeof value !== 'number' || value < 25 || value > 500) {
                return 'Le niveau de zoom doit être entre 25 et 500';
            }
            return true;
        });
        
        // Validation du volume
        this.addValidationRule('volume', (value) => {
            if (typeof value !== 'number' || value < 0 || value > 100) {
                return 'Le volume doit être entre 0 et 100';
            }
            return true;
        });
        
        // Validation du taux de lecture
        this.addValidationRule('playbackRate', (value) => {
            if (typeof value !== 'number' || value < 0.25 || value > 3.0) {
                return 'Le taux de lecture doit être entre 0.25 et 3.0';
            }
            return true;
        });
        
        // Validation des paramètres
        this.addValidationRule('settings', (settings) => {
            if (!settings || typeof settings !== 'object') {
                return 'Les paramètres doivent être un objet';
            }
            
            // Validation des paramètres spécifiques
            const validations = [
                () => settings.pauseBetweenTracks >= 0 && settings.pauseBetweenTracks <= 30 || 'Pause entre pistes: 0-30s',
                () => settings.startDelay >= 0 && settings.startDelay <= 5000 || 'Délai de démarrage: 0-5000ms',
                () => settings.noteDisplayWindow >= 5 && settings.noteDisplayWindow <= 120 || 'Fenêtre d\'affichage: 5-120s',
                () => settings.visualizerRefreshRate >= 1 && settings.visualizerRefreshRate <= 120 || 'Taux de rafraîchissement: 1-120fps',
                () => settings.maxConcurrentNotes >= 16 && settings.maxConcurrentNotes <= 512 || 'Notes simultanées max: 16-512'
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
     * Configurer les propriétés calculées
     */
    setupComputedProperties() {
        // Calculer si du contenu est sélectionné
        this.addComputed('hasSelection', (data) => {
            return !!(data.currentFile || data.currentPlaylist);
        });
        
        // Calculer l'état de lecture complet
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
                startTime: data.sessionStartTime,
                stats: data.stats
            };
        });
        
        // Calculer les statistiques de performance
        this.addComputed('performanceMetrics', (data) => {
            return {
                memoryUsage: this.estimateMemoryUsage(),
                renderingLoad: this.calculateRenderingLoad(data),
                midiLoad: this.calculateMidiLoad(data),
                recommendations: this.generatePerformanceRecommendations(data)
            };
        });
    }

    /**
     * Configurer les observateurs
     */
    setupWatchers() {
        // Observer les changements de page
        this.watch('currentPage', (newPage, oldPage) => {
            this.set('previousPage', oldPage);
            this.emitEvent('page:changed', { from: oldPage, to: newPage });
        });
        
        // Observer les changements de fichier
        this.watch('currentFile', (newFile, oldFile) => {
            if (newFile) {
                this.addRecentFile(newFile);
                this.updatePlaybackCapabilities();
            }
            this.emitEvent('file:changed', { from: oldFile, to: newFile });
        });
        
        // Observer les changements de playlist
        this.watch('currentPlaylist', (newPlaylist, oldPlaylist) => {
            if (newPlaylist) {
                this.addRecentPlaylist(newPlaylist);
                this.updatePlaybackCapabilities();
            }
            this.emitEvent('playlist:changed', { from: oldPlaylist, to: newPlaylist });
        });
        
        // Observer les changements d'état de lecture
        this.watch('isPlaying', (isPlaying) => {
            if (isPlaying) {
                this.incrementStat('filesPlayed');
            }
            this.emitEvent('playback:state:changed', { isPlaying });
        });
        
        // Observer les changements de paramètres
        this.watch('settings', (newSettings, oldSettings) => {
            this.handleSettingsChange(newSettings, oldSettings);
        });
    }

    /**
     * Méthode helper pour émettre des événements
     * @param {string} eventName - Nom de l'événement
     * @param {Object} data - Données de l'événement
     */
    emitEvent(eventName, data) {
        if (this.eventBus && typeof this.eventBus.emit === 'function') {
            this.eventBus.emit(eventName, data);
        }
    }

    /**
     * Méthode helper pour ajouter une règle de validation
     * @param {string} key - Clé à valider
     * @param {Function} validationFn - Fonction de validation
     */
    addValidationRule(key, validationFn) {
        this.validationRules[key] = validationFn;
    }

    /**
     * Méthode helper pour ajouter une propriété calculée
     * @param {string} name - Nom de la propriété
     * @param {Function} computeFn - Fonction de calcul
     */
    addComputed(name, computeFn) {
        if (!this._computed) {
            this._computed = {};
        }
        this._computed[name] = computeFn;
    }

    /**
     * Obtenir une propriété calculée
     * @param {string} name - Nom de la propriété
     * @returns {*} Valeur calculée
     */
    getComputed(name) {
        if (this._computed && this._computed[name]) {
            return this._computed[name](this.data);
        }
        return undefined;
    }

    /**
     * Méthode helper pour observer un changement
     * @param {string} key - Clé à observer
     * @param {Function} callback - Callback appelé lors du changement
     */
    watch(key, callback) {
        if (!this._watchers) {
            this._watchers = {};
        }
        if (!this._watchers[key]) {
            this._watchers[key] = [];
        }
        this._watchers[key].push(callback);
    }

    /**
     * Surcharge de set pour déclencher les watchers
     * @param {string} key - Clé
     * @param {*} value - Valeur
     * @param {boolean} silent - Mode silencieux
     */
    set(key, value, silent = false) {
        const oldValue = this.get(key);
        const result = super.set(key, value, silent);
        
        // Déclencher les watchers
        if (this._watchers && this._watchers[key] && oldValue !== value) {
            this._watchers[key].forEach(callback => {
                try {
                    callback(value, oldValue);
                } catch (error) {
                    console.error(`Error in watcher for ${key}:`, error);
                }
            });
        }
        
        return result;
    }

    /**
     * Incrémenter une statistique
     * @param {string} statName - Nom de la stat
     * @param {number} amount - Montant à ajouter
     */
    incrementStat(statName, amount = 1) {
        const stats = this.get('stats');
        if (stats && typeof stats[statName] === 'number') {
            stats[statName] += amount;
            this.set('stats', stats);
        }
    }

    /**
     * Ajouter un fichier récent
     * @param {Object} file - Fichier à ajouter
     */
    addRecentFile(file) {
        const recentFiles = this.get('recentFiles') || [];
        
        // Supprimer les doublons
        const filtered = recentFiles.filter(f => f.id !== file.id);
        
        // Ajouter en tête
        filtered.unshift({
            id: file.id,
            name: file.name,
            path: file.path,
            accessTime: Date.now()
        });
        
        // Limiter à 20
        if (filtered.length > 20) {
            filtered.pop();
        }
        
        this.set('recentFiles', filtered, { silent: true });
    }

    /**
     * Ajouter une playlist récente
     * @param {Object} playlist - Playlist à ajouter
     */
    addRecentPlaylist(playlist) {
        const recentPlaylists = this.get('recentPlaylists') || [];
        
        // Supprimer les doublons
        const filtered = recentPlaylists.filter(p => p.id !== playlist.id);
        
        // Ajouter en tête
        filtered.unshift({
            id: playlist.id,
            name: playlist.name,
            fileCount: playlist.files?.length || 0,
            accessTime: Date.now()
        });
        
        // Limiter à 10
        if (filtered.length > 10) {
            filtered.pop();
        }
        
        this.set('recentPlaylists', filtered, { silent: true });
    }

    /**
     * Mettre à jour les capacités de lecture
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
     * Gérer les changements de paramètres
     * @param {Object} newSettings - Nouveaux paramètres
     * @param {Object} oldSettings - Anciens paramètres
     */
    handleSettingsChange(newSettings, oldSettings) {
        // Détecter les changements critiques
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
        
        // Sauvegarder si auto-save activé
        if (newSettings.autoSaveSettings) {
            this.persistState();
        }
        
        // Émettre événement global
        this.emitEvent('settings:changed', {
            settings: newSettings,
            criticalChanges
        });
    }

    /**
     * Charger l'état persistant
     */
    loadPersistedState() {
        try {
            const saved = localStorage.getItem('midimind_state');
            if (saved) {
                const parsedState = JSON.parse(saved);
                this.importState(parsedState);
            }
        } catch (error) {
            console.warn('Erreur lors du chargement de l\'état:', error);
        }
    }

    /**
     * Sauvegarder l'état persistant
     */
    persistState() {
        try {
            const stateToSave = this.exportState();
            localStorage.setItem('midimind_state', JSON.stringify(stateToSave));
        } catch (error) {
            console.warn('Erreur lors de la sauvegarde de l\'état:', error);
        }
    }

    /**
     * Exporter l'état
     * @returns {Object} État exporté
     */
    exportState() {
        return {
            data: this.getData(),
            version: '3.0.0',
            exportedAt: Date.now()
        };
    }

    /**
     * Importer l'état
     * @param {Object} state - État à importer
     */
    importState(state) {
        if (state.data) {
            Object.keys(state.data).forEach(key => {
                this.set(key, state.data[key], true);
            });
        }
    }

    /**
     * Estimer l'usage mémoire
     * @returns {Object} Estimation de la mémoire
     */
    estimateMemoryUsage() {
        const base = 50; // MB de base
        const perFile = 2; // MB par fichier récent
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
     * @param {Object} data - Données du modèle
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
     * @param {Object} data - Données du modèle
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
     * Générer des recommandations de performance
     * @param {Object} data - Données du modèle
     * @returns {Array} Liste de recommandations
     */
    generatePerformanceRecommendations(data) {
        const recommendations = [];
        
        if (data.zoomLevel > 200) {
            recommendations.push({
                type: 'warning',
                message: 'Niveau de zoom élevé - peut affecter les performances',
                action: 'Réduire le zoom à 150% ou moins'
            });
        }
        
        if (data.settings.visualizerRefreshRate > 60) {
            recommendations.push({
                type: 'info',
                message: 'Taux de rafraîchissement élevé détecté',
                action: 'Réduire à 60fps pour de meilleures performances'
            });
        }
        
        if (data.settings.maxConcurrentNotes > 256) {
            recommendations.push({
                type: 'warning',
                message: 'Limite de notes simultanées élevée',
                action: 'Réduire à 256 notes maximum'
            });
        }
        
        return recommendations;
    }

    /**
     * Formater un temps en secondes
     * @param {number} seconds - Temps en secondes
     * @returns {string} Temps formaté (MM:SS)
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
     * Formater une durée en millisecondes
     * @param {number} ms - Durée en millisecondes
     * @returns {string} Durée formatée
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
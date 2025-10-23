// ============================================================================
// Fichier: frontend/js/config/AppConfig.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Configuration globale de l'application.
//   URLs, constantes, paramètres par défaut, feature flags.
//
// Configuration:
//   - Backend : URLs WebSocket et REST API
//   - MIDI : Latence, buffer size, canaux
//   - Audio : Sample rate, buffer size
//   - UI : Thèmes, animations, timeouts
//   - Files : Extensions supportées, taille max
//   - Network : Retry, timeout
//   - Debug : Logs level, debug mode
//   - Features : Feature flags (enable/disable)
//
// Architecture:
//   Object freezé (immutable)
//   - Chargement depuis config.json si disponible
//   - Merge avec localStorage (overrides)
//   - Export/Import configuration
//
// Auteur: MidiMind Team
// ============================================================================
const AppConfig = {
    // Informations de l'application
    app: {
        name: 'MIDI Mind',
        version: '1.0.0',
        description: 'Système de contrôle MIDI avec backend C++',
        author: 'MIDI Orchestrion Team'
    },
    
    // Configuration du backend C++
    backend: {
        url: 'ws://localhost:8080',
        reconnectInterval: 3000,      // Intervalle de reconnexion (ms)
        commandTimeout: 5000,          // Timeout pour les commandes (ms)
        maxReconnectAttempts: 10,      // Nombre max de tentatives de reconnexion
        heartbeatInterval: 30000,      // Intervalle de heartbeat (ms)
        enableAutoReconnect: true      // Reconnexion automatique
    },
    
    // Configuration MIDI
    midi: {
        defaultVelocity: 64,           // Vélocité par défaut (0-127)
        defaultTempo: 120,             // Tempo par défaut (BPM)
        ticksPerQuarter: 480,          // Ticks par noire
        maxPolyphony: 128,             // Polyphonie max
        
        // Plage de notes du clavier virtuel
        keyboardRange: {
            start: 48,                 // C3
            end: 84                    // C6
        },
        
        // Configuration des canaux
        channels: {
            min: 0,
            max: 15,
            drumChannel: 9             // Canal 10 (index 9)
        },
        
        // Latence et synchronisation
        sync: {
            enabled: true,
            autoCalibration: true,
            maxLatency: 200,           // Latence max acceptable (ms)
            defaultOffset: 0,          // Offset par défaut (ms)
            jitterTolerance: 5         // Tolérance de jitter (ms)
        }
    },
    
    // Configuration de l'interface utilisateur
    ui: {
        theme: 'light',                // 'light' ou 'dark'
        enableAnimations: true,        // Activer les animations CSS
        notificationDuration: 3000,    // Durée des notifications (ms)
        tooltipDelay: 500,             // Délai d'apparition des tooltips (ms)
        
        // Pagination
        pagination: {
            filesPerPage: 20,
            playlistsPerPage: 10,
            instrumentsPerPage: 15
        },
        
        // Visualiseur MIDI
        visualizer: {
            enabled: true,
            fps: 60,                   // Images par seconde
            noteHeight: 10,            // Hauteur des notes (px)
            pixelsPerSecond: 100,      // Pixels par seconde
            showNoteNames: true,       // Afficher les noms des notes
            showVelocity: true,        // Afficher la vélocité
            colors: {
                background: '#1a1a2e',
                grid: '#16213e',
                note: '#667eea',
                noteSelected: '#f97316',
                notePlaying: '#10b981'
            }
        }
    },
    
    // Configuration du debug
    debug: {
        enabled: true,                 // Activer le mode debug
        console: true,                 // Afficher la console de debug
        logLevel: 'info',              // 'debug', 'info', 'warning', 'error'
        enableMetrics: true,           // Activer les métriques de performance
        maxLogs: 1000,                 // Nombre max de logs à conserver
        
        // Catégories de logs
        categories: {
            system: true,
            midi: true,
            files: true,
            keyboard: true,
            network: true,
            sync: true,
            instruments: true
        }
    },
    
    // Configuration des fichiers
    files: {
        maxFileSize: 50 * 1024 * 1024, // 50 MB
        allowedExtensions: ['.mid', '.midi', '.kar'],
        enableAnalysis: true,           // Analyser les fichiers MIDI
        generateThumbnails: true,       // Générer des miniatures
        autoSave: true,                 // Sauvegarde automatique
        autoSaveInterval: 60000,        // Intervalle de sauvegarde auto (ms)
        
        // Upload
        upload: {
            batchSize: 10,              // Nombre de fichiers traités en parallèle
            showProgress: true,         // Afficher la progression
            validateBeforeUpload: true  // Valider avant upload
        }
    },
    
    // Configuration des instruments
    instruments: {
        autoDiscovery: true,            // Découverte automatique
        discoveryInterval: 30000,       // Intervalle de découverte (ms)
        discoveryTimeout: 5000,         // Timeout de découverte (ms)
        enableSysEx: true,              // Activer les messages SysEx
        enableFallback: true,           // Fallback sur détection classique
        
        // Calibration
        calibration: {
            enabled: true,
            automatic: true,
            pingCount: 10,              // Nombre de pings pour calibration
            measurementDelay: 100       // Délai entre mesures (ms)
        }
    },
    
    // Configuration des playlists
    playlists: {
        maxSize: 100,                   // Nombre max de fichiers par playlist
        autoPlay: false,                // Lecture automatique
        shuffle: false,                 // Lecture aléatoire
        repeat: false,                  // Répétition
        crossfade: 0                    // Crossfade entre fichiers (ms)
    },
    
    // Configuration de la performance
    performance: {
        enableOptimizations: true,      // Activer les optimisations
        lazyLoading: true,              // Chargement différé
        caching: true,                  // Cache des données
        maxCacheSize: 100 * 1024 * 1024, // Taille max du cache (100 MB)
        
        // Throttling/Debouncing
        throttle: {
            scroll: 16,                 // ~60fps
            resize: 100,
            search: 300
        }
    },
    
    // Configuration du stockage
    storage: {
        enabled: true,
        type: 'localStorage',           // 'localStorage' ou 'sessionStorage'
        prefix: 'midiMind_',            // Préfixe des clés
        compress: false,                // Compression des données
        
        // Éléments à persister
        persist: {
            settings: true,
            recentFiles: true,
            playlists: true,
            instruments: true,
            preferences: true
        }
    },
    
    // Raccourcis clavier
    shortcuts: {
        enabled: true,
        
        // Lecture
        playPause: 'Space',
        stop: 'Escape',
        nextTrack: 'ArrowRight',
        prevTrack: 'ArrowLeft',
        
        // Navigation
        gotoFiles: 'f',
        gotoInstruments: 'i',
        gotoKeyboard: 'k',
        gotoSystem: 's',
        
        // Actions
        openDebugConsole: 'd',
        saveSettings: 'Ctrl+s',
        openSearch: 'Ctrl+f'
    },
    
    // URLs et endpoints (si nécessaire)
    urls: {
        documentation: 'https://docs.midi-orchestrion.com',
        support: 'https://support.midi-orchestrion.com',
        github: 'https://github.com/midi-orchestrion'
    }
};

// Fonction utilitaire pour accéder à la config de manière sécurisée
AppConfig.get = function(path, defaultValue = null) {
    const keys = path.split('.');
    let value = this;
    
    for (const key of keys) {
        if (value && typeof value === 'object' && key in value) {
            value = value[key];
        } else {
            return defaultValue;
        }
    }
    
    return value;
};

// Fonction pour mettre à jour la config
AppConfig.set = function(path, newValue) {
    const keys = path.split('.');
    const lastKey = keys.pop();
    let obj = this;
    
    for (const key of keys) {
        if (!(key in obj)) {
            obj[key] = {};
        }
        obj = obj[key];
    }
    
    obj[lastKey] = newValue;
};

// Exporter pour usage global
if (typeof module !== 'undefined' && module.exports) {
    module.exports = AppConfig;
}
window.AppConfig = AppConfig;
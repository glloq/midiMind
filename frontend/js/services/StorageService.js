// ============================================================================
// Fichier: frontend/js/services/StorageService.js
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Service de persistance des données dans le localStorage.
//   Gère la sauvegarde/restauration de l'état, des préférences et du cache.
//
// Auteur: midiMind Team
// Date: 2025-10-04
// Version: 3.0.0
// ============================================================================

class StorageService {
    constructor(eventBus, logger) {
        this.eventBus = eventBus;
        this.logger = logger;
        
        // Préfixe pour toutes les clés
        this.prefix = 'midiMind_';
        
        // Configuration
        this.config = {
            maxStorageSize: 5 * 1024 * 1024, // 5MB
            compressionEnabled: true,
            encryptionEnabled: false,
            autoSave: true,
            autoSaveInterval: 30000, // 30 secondes
            version: '3.0.0'
        };
        
        // Timer d'auto-sauvegarde
        this.autoSaveTimer = null;
        
        // Cache mémoire pour optimisation
        this.memoryCache = new Map();
        
        // Statistiques
        this.stats = {
            reads: 0,
            writes: 0,
            errors: 0,
            storageUsed: 0
        };
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        this.logger.info('StorageService', 'Initializing storage service...');
        
        // Vérifier la disponibilité du localStorage
        if (!this.isStorageAvailable()) {
            this.logger.error('StorageService', 'LocalStorage is not available');
            return;
        }
        
        // Migrer les données si nécessaire
        this.migrateData();
        
        // Calculer l'espace utilisé
        this.calculateStorageUsage();
        
        // Démarrer l'auto-sauvegarde
        if (this.config.autoSave) {
            this.startAutoSave();
        }
        
        // Écouter les événements
        this.bindEvents();
    }
    
    bindEvents() {
        // Sauvegarder avant fermeture de la fenêtre
        window.addEventListener('beforeunload', () => {
            this.saveAll();
        });
        
        // Écouter les changements d'état importants
        this.eventBus.on('state:changed', (data) => {
            if (this.config.autoSave) {
                this.saveState(data);
            }
        });
        
        this.eventBus.on('preferences:changed', (data) => {
            this.savePreferences(data);
        });
    }
    
    // ========================================================================
    // OPÉRATIONS DE BASE
    // ========================================================================
    
    /**
     * Sauvegarder une valeur
     * @param {string} key - Clé de stockage
     * @param {any} value - Valeur à sauvegarder
     * @param {Object} options - Options de sauvegarde
     */
    save(key, value, options = {}) {
        try {
            const fullKey = this.prefix + key;
            
            // Préparer les données
            const data = {
                value: value,
                timestamp: Date.now(),
                version: this.config.version,
                compressed: false
            };
            
            // Compression si activée et données volumineuses
            let serialized = JSON.stringify(data);
            if (this.config.compressionEnabled && serialized.length > 1024) {
                serialized = this.compress(serialized);
                data.compressed = true;
            }
            
            // Vérifier la taille
            if (serialized.length > this.config.maxStorageSize) {
                throw new Error('Data too large for storage');
            }
            
            // Sauvegarder
            localStorage.setItem(fullKey, serialized);
            
            // Mettre à jour le cache mémoire
            this.memoryCache.set(key, value);
            
            this.stats.writes++;
            this.logger.debug('StorageService', `Saved: ${key}`);
            
            // Émettre un événement
            this.eventBus.emit('storage:saved', { key, size: serialized.length });
            
            return true;
            
        } catch (error) {
            this.stats.errors++;
            this.logger.error('StorageService', `Failed to save ${key}:`, error);
            
            // Essayer de libérer de l'espace si quota dépassé
            if (error.name === 'QuotaExceededError') {
                this.cleanupOldData();
                // Réessayer une fois
                try {
                    localStorage.setItem(this.prefix + key, JSON.stringify(value));
                    return true;
                } catch (retryError) {
                    return false;
                }
            }
            
            return false;
        }
    }
    
    /**
     * Charger une valeur
     * @param {string} key - Clé de stockage
     * @param {any} defaultValue - Valeur par défaut si non trouvée
     */
    load(key, defaultValue = null) {
        try {
            // Vérifier le cache mémoire d'abord
            if (this.memoryCache.has(key)) {
                this.stats.reads++;
                return this.memoryCache.get(key);
            }
            
            const fullKey = this.prefix + key;
            const stored = localStorage.getItem(fullKey);
            
            if (!stored) {
                return defaultValue;
            }
            
            // Décompresser si nécessaire
            let parsed;
            try {
                parsed = JSON.parse(stored);
            } catch (e) {
                // Données peut-être compressées
                const decompressed = this.decompress(stored);
                parsed = JSON.parse(decompressed);
            }
            
            // Vérifier la version
            if (parsed.version && parsed.version !== this.config.version) {
                this.logger.warn('StorageService', `Version mismatch for ${key}`);
            }
            
            const value = parsed.value || parsed;
            
            // Mettre en cache
            this.memoryCache.set(key, value);
            
            this.stats.reads++;
            return value;
            
        } catch (error) {
            this.stats.errors++;
            this.logger.error('StorageService', `Failed to load ${key}:`, error);
            return defaultValue;
        }
    }
    
    /**
     * Supprimer une valeur
     * @param {string} key - Clé à supprimer
     */
    remove(key) {
        try {
            const fullKey = this.prefix + key;
            localStorage.removeItem(fullKey);
            this.memoryCache.delete(key);
            
            this.logger.debug('StorageService', `Removed: ${key}`);
            return true;
            
        } catch (error) {
            this.logger.error('StorageService', `Failed to remove ${key}:`, error);
            return false;
        }
    }
    
    /**
     * Vérifier si une clé existe
     * @param {string} key - Clé à vérifier
     */
    exists(key) {
        return localStorage.getItem(this.prefix + key) !== null;
    }
    
    // ========================================================================
    // OPÉRATIONS SPÉCIFIQUES
    // ========================================================================
    
    /**
     * Sauvegarder l'état global de l'application
     */
    saveState(state) {
        return this.save('appState', {
            playback: state.playback,
            routing: state.routing,
            selectedFile: state.selectedFile,
            volume: state.volume,
            tempo: state.tempo,
            transpose: state.transpose
        });
    }
    
    /**
     * Charger l'état global
     */
    loadState() {
        return this.load('appState', {});
    }
    
    /**
     * Sauvegarder les préférences utilisateur
     */
    savePreferences(preferences) {
        return this.save('preferences', preferences);
    }
    
    /**
     * Charger les préférences
     */
    loadPreferences() {
        return this.load('preferences', {
            theme: 'dark',
            language: 'fr',
            autoPlay: false,
            visualizerEnabled: true,
            debugMode: false,
            notifications: true
        });
    }
    
    /**
     * Sauvegarder une playlist
     */
    savePlaylist(name, playlist) {
        const playlists = this.loadPlaylists();
        playlists[name] = {
            ...playlist,
            updatedAt: Date.now()
        };
        return this.save('playlists', playlists);
    }
    
    /**
     * Charger les playlists
     */
    loadPlaylists() {
        return this.load('playlists', {});
    }
    
    /**
     * Sauvegarder la configuration de routage
     */
    saveRouting(routing) {
        return this.save('routing', routing);
    }
    
    /**
     * Charger la configuration de routage
     */
    loadRouting() {
        return this.load('routing', {
            channels: {},
            presets: []
        });
    }
    
    /**
     * Sauvegarder l'historique des fichiers récents
     */
    saveRecentFiles(files) {
        // Garder seulement les 20 derniers
        const recent = files.slice(0, 20);
        return this.save('recentFiles', recent);
    }
    
    /**
     * Charger l'historique des fichiers récents
     */
    loadRecentFiles() {
        return this.load('recentFiles', []);
    }
    
    // ========================================================================
    // AUTO-SAUVEGARDE
    // ========================================================================
    
    startAutoSave() {
        this.stopAutoSave();
        
        this.autoSaveTimer = setInterval(() => {
            this.autoSave();
        }, this.config.autoSaveInterval);
        
        this.logger.info('StorageService', 'Auto-save enabled');
    }
    
    stopAutoSave() {
        if (this.autoSaveTimer) {
            clearInterval(this.autoSaveTimer);
            this.autoSaveTimer = null;
        }
    }
    
    autoSave() {
        // Sauvegarder l'état actuel si disponible
        if (window.app && window.app.getState) {
            const state = window.app.getState();
            this.saveState(state);
        }
        
        this.logger.debug('StorageService', 'Auto-save completed');
    }
    
    /**
     * Sauvegarder toutes les données en mémoire
     */
    saveAll() {
        let saved = 0;
        
        for (const [key, value] of this.memoryCache) {
            if (this.save(key, value)) {
                saved++;
            }
        }
        
        this.logger.info('StorageService', `Saved ${saved} items to storage`);
        return saved;
    }
    
    // ========================================================================
    // COMPRESSION
    // ========================================================================
    
    /**
     * Compresser une chaîne (simple compression LZ)
     */
    compress(str) {
        // Implémentation simple de compression LZ
        // Pour une vraie compression, utiliser une librairie comme pako
        return str; // TODO: Implémenter compression réelle
    }
    
    /**
     * Décompresser une chaîne
     */
    decompress(str) {
        // Implémentation simple de décompression
        return str; // TODO: Implémenter décompression réelle
    }
    
    // ========================================================================
    // GESTION DE L'ESPACE
    // ========================================================================
    
    /**
     * Calculer l'espace utilisé
     */
    calculateStorageUsage() {
        let totalSize = 0;
        
        for (let key in localStorage) {
            if (key.startsWith(this.prefix)) {
                const value = localStorage.getItem(key);
                totalSize += key.length + value.length;
            }
        }
        
        this.stats.storageUsed = totalSize;
        return totalSize;
    }
    
    /**
     * Obtenir l'espace disponible (estimation)
     */
    getAvailableSpace() {
        // Tester en écrivant progressivement
        const testKey = this.prefix + '_test_';
        const chunk = 'x'.repeat(1024); // 1KB
        let size = 0;
        
        try {
            while (size < 10 * 1024 * 1024) { // Max 10MB test
                localStorage.setItem(testKey + size, chunk);
                size += 1024;
            }
        } catch (e) {
            // Quota atteint
        }
        
        // Nettoyer
        for (let i = 0; i < size; i += 1024) {
            localStorage.removeItem(testKey + i);
        }
        
        return size;
    }
    
    /**
     * Nettoyer les anciennes données
     */
    cleanupOldData() {
        const items = [];
        
        // Collecter tous les items avec timestamp
        for (let key in localStorage) {
            if (key.startsWith(this.prefix)) {
                try {
                    const value = localStorage.getItem(key);
                    const parsed = JSON.parse(value);
                    if (parsed.timestamp) {
                        items.push({
                            key: key,
                            timestamp: parsed.timestamp,
                            size: value.length
                        });
                    }
                } catch (e) {
                    // Ignorer les erreurs de parsing
                }
            }
        }
        
        // Trier par ancienneté
        items.sort((a, b) => a.timestamp - b.timestamp);
        
        // Supprimer les 25% plus anciens
        const toRemove = Math.floor(items.length * 0.25);
        for (let i = 0; i < toRemove; i++) {
            localStorage.removeItem(items[i].key);
        }
        
        this.logger.info('StorageService', `Cleaned up ${toRemove} old items`);
    }
    
    // ========================================================================
    // MIGRATION
    // ========================================================================
    
    /**
     * Migrer les données d'anciennes versions
     */
    migrateData() {
        const versionKey = this.prefix + 'version';
        const currentVersion = localStorage.getItem(versionKey);
        
        if (!currentVersion) {
            // Première installation
            localStorage.setItem(versionKey, this.config.version);
            return;
        }
        
        if (currentVersion !== this.config.version) {
            this.logger.info('StorageService', `Migrating from ${currentVersion} to ${this.config.version}`);
            
            // Logique de migration selon les versions
            // ...
            
            localStorage.setItem(versionKey, this.config.version);
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Vérifier si le localStorage est disponible
     */
    isStorageAvailable() {
        try {
            const test = '__storage_test__';
            localStorage.setItem(test, test);
            localStorage.removeItem(test);
            return true;
        } catch (e) {
            return false;
        }
    }
    
    /**
     * Effacer toutes les données de l'application
     */
    clearAll() {
        const keys = [];
        
        for (let key in localStorage) {
            if (key.startsWith(this.prefix)) {
                keys.push(key);
            }
        }
        
        for (const key of keys) {
            localStorage.removeItem(key);
        }
        
        this.memoryCache.clear();
        
        this.logger.info('StorageService', 'All data cleared');
    }
    
    /**
     * Exporter toutes les données
     */
    exportAll() {
        const data = {};
        
        for (let key in localStorage) {
            if (key.startsWith(this.prefix)) {
                const cleanKey = key.substring(this.prefix.length);
                data[cleanKey] = this.load(cleanKey);
            }
        }
        
        return data;
    }
    
    /**
     * Importer des données
     */
    importAll(data) {
        for (const [key, value] of Object.entries(data)) {
            this.save(key, value);
        }
        
        this.logger.info('StorageService', `Imported ${Object.keys(data).length} items`);
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    getStats() {
        return {
            ...this.stats,
            cacheSize: this.memoryCache.size,
            storageUsedMB: (this.stats.storageUsed / (1024 * 1024)).toFixed(2)
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = StorageService;
}

if (typeof window !== 'undefined') {
    window.StorageService = StorageService;
}
window.StorageService = StorageService;
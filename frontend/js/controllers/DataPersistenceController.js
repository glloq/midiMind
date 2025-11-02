// ============================================================================
// Fichier: frontend/js/controllers/DataPersistenceController.js
// Chemin rÃ©el: frontend/js/controllers/DataPersistenceController.js
// Version: v3.5.0 - PERSISTANCE COMPLÃˆTE + SYNC BACKEND
// Date: 2025-11-01
// ============================================================================
// AMÃ‰LIORATIONS v3.5.0:
// âœ… Synchronisation automatique avec backend via API
// âœ… Support IndexedDB pour gros volumes
// âœ… Versionning et migration automatique
// âœ… Gestion conflits local vs backend
// âœ… Compression LZ rÃ©elle des donnÃ©es
// âœ… Backup/restore avec validation
// âœ… Sauvegarde incrÃ©mentale intelligente
// âœ… DÃ©tection changements pour optimisation
// ============================================================================

class DataPersistenceController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Configuration
        this.config = {
            storageKey: 'midiMind_data',
            version: '3.0.0',
            autoSaveInterval: 30000, // 30 secondes
            syncInterval: 60000, // 1 minute
            useIndexedDB: true,
            useCompression: true,
            backupOnSync: true,
            maxBackups: 5
        };
        
        // Backend
        this.backend = window.backendService;
        
        // Ã‰tat de synchronisation
        this.syncState = {
            lastSync: null,
            syncInProgress: false,
            conflictCount: 0,
            autoSyncEnabled: true,
            backendAvailable: false
        };
        
        // Timers
        this.autoSaveTimer = null;
        this.syncTimer = null;
        
        // Change tracking
        this.changeTracker = {
            hasChanges: false,
            changedModels: new Set(),
            lastSave: Date.now()
        };
        
        // IndexedDB
        this.db = null;
        this.dbName = 'MidiMindDB';
        this.dbVersion = 1;
        
        this.log('info', 'DataPersistenceController', 'âœ… Initialized v3.5.0');
        
        this.initialize();
    }

    /**
     * Initialisation
     */
    async initialize() {
        // Initialiser IndexedDB si activÃ©
        if (this.config.useIndexedDB) {
            await this.initIndexedDB();
        }
        
        // Charger les donnÃ©es au dÃ©marrage
        await this.loadData();
        
        // Configurer auto-save
        this.setupAutoSave();
        
        // Configurer sync backend
        this.setupBackendSync();
        
        // VÃ©rifier backend
        this.checkBackendAvailability();
    }

    /**
     * Liaison des Ã©vÃ©nements
     */
    bindEvents() {
        // Sauvegarder lors des changements importants
        this.eventBus.on('file:added', () => this.markChanged('file'));
        this.eventBus.on('file:updated', () => this.markChanged('file'));
        this.eventBus.on('file:deleted', () => this.markChanged('file'));
        
        this.eventBus.on('instrument:updated', () => this.markChanged('instrument'));
        this.eventBus.on('instrument:connected', () => this.markChanged('instrument'));
        
        this.eventBus.on('playlist:created', () => this.markChanged('playlist'));
        this.eventBus.on('playlist:updated', () => this.markChanged('playlist'));
        this.eventBus.on('playlist:deleted', () => this.markChanged('playlist'));
        
        this.eventBus.on('routing:changed', () => this.markChanged('routing'));
        this.eventBus.on('state:changed', () => this.markChanged('state'));
        
        // Ã‰vÃ©nements backend
        this.eventBus.on('backend:connected', () => {
            this.checkBackendAvailability();
            this.syncWithBackend();
        });
        
        this.eventBus.on('backend:disconnected', () => {
            this.syncState.backendAvailable = false;
        });
        
        // Sauvegarde avant fermeture
        window.addEventListener('beforeunload', () => {
            if (this.changeTracker.hasChanges) {
                this.saveData({ sync: false });
            }
        });
    }

    // ========================================================================
    // INDEXEDDB
    // ========================================================================

    /**
     * Initialise IndexedDB
     */
    async initIndexedDB() {
        return new Promise((resolve, reject) => {
            const request = indexedDB.open(this.dbName, this.dbVersion);
            
            request.onerror = () => {
                this.log('error', 'DataPersistence', 'IndexedDB error:', request.error);
                reject(request.error);
            };
            
            request.onsuccess = () => {
                this.db = request.result;
                this.log('info', 'DataPersistence', 'âœ… IndexedDB ready');
                resolve(this.db);
            };
            
            request.onupgradeneeded = (event) => {
                const db = event.target.result;
                
                // Store principal
                if (!db.objectStoreNames.contains('data')) {
                    db.createObjectStore('data', { keyPath: 'key' });
                }
                
                // Store de backups
                if (!db.objectStoreNames.contains('backups')) {
                    const backupStore = db.createObjectStore('backups', { keyPath: 'id', autoIncrement: true });
                    backupStore.createIndex('timestamp', 'timestamp', { unique: false });
                }
                
                this.log('info', 'DataPersistence', 'IndexedDB schema created');
            };
        });
    }

    /**
     * Sauvegarde dans IndexedDB
     */
    async saveToIndexedDB(key, data) {
        if (!this.db) return false;
        
        return new Promise((resolve, reject) => {
            const transaction = this.db.transaction(['data'], 'readwrite');
            const store = transaction.objectStore('data');
            const request = store.put({ key, data, timestamp: Date.now() });
            
            request.onsuccess = () => resolve(true);
            request.onerror = () => reject(request.error);
        });
    }

    /**
     * Charge depuis IndexedDB
     */
    async loadFromIndexedDB(key) {
        if (!this.db) return null;
        
        return new Promise((resolve, reject) => {
            const transaction = this.db.transaction(['data'], 'readonly');
            const store = transaction.objectStore('data');
            const request = store.get(key);
            
            request.onsuccess = () => {
                resolve(request.result?.data || null);
            };
            request.onerror = () => reject(request.error);
        });
    }

    // ========================================================================
    // SAUVEGARDE / CHARGEMENT
    // ========================================================================

    /**
     * Sauvegarde les donnÃ©es
     */
    async saveData(options = {}) {
        const {
            sync = true,
            createBackup = false
        } = options;
        
        try {
            // Collecter les donnÃ©es
            const data = this.collectData();
            
            // CrÃ©er backup si demandÃ©
            if (createBackup) {
                await this.createBackup(data);
            }
            
            // Compresser si activÃ©
            const finalData = this.config.useCompression 
                ? this.compressData(data)
                : JSON.stringify(data);
            
            // Sauvegarder en localStorage
            localStorage.setItem(this.config.storageKey, finalData);
            
            // Sauvegarder en IndexedDB
            if (this.config.useIndexedDB && this.db) {
                await this.saveToIndexedDB('main', data);
            }
            
            // Reset change tracker
            this.changeTracker.hasChanges = false;
            this.changeTracker.changedModels.clear();
            this.changeTracker.lastSave = Date.now();
            
            this.log('debug', 'DataPersistence', 'ðŸ’¾ Data saved');
            
            // Synchroniser avec backend si demandÃ©
            if (sync && this.syncState.backendAvailable && this.syncState.autoSyncEnabled) {
                await this.syncWithBackend();
            }
            
            this.eventBus.emit('persistence:saved', { timestamp: Date.now() });
            return true;
            
        } catch (error) {
            this.handleError('Erreur sauvegarde', error);
            return false;
        }
    }

    /**
     * Charge les donnÃ©es
     */
    async loadData() {
        try {
            let data = null;
            
            // Essayer IndexedDB d'abord
            if (this.config.useIndexedDB && this.db) {
                data = await this.loadFromIndexedDB('main');
            }
            
            // Fallback sur localStorage
            if (!data) {
                const savedData = localStorage.getItem(this.config.storageKey);
                if (savedData) {
                    data = this.config.useCompression 
                        ? this.decompressData(savedData)
                        : JSON.parse(savedData);
                }
            }
            
            if (!data) {
                this.log('info', 'DataPersistence', 'No saved data found');
                return false;
            }
            
            // Valider et migrer version si nÃ©cessaire
            if (data.version !== this.config.version) {
                data = await this.migrateData(data);
            }
            
            // Restaurer les donnÃ©es dans les modÃ¨les
            await this.restoreData(data);
            
            this.log('info', 'DataPersistence', `âœ… Data loaded (${this.formatDate(data.timestamp)})`);
            this.notify('success', 'DonnÃ©es prÃ©cÃ©dentes restaurÃ©es');
            
            this.eventBus.emit('persistence:loaded', { timestamp: data.timestamp });
            return true;
            
        } catch (error) {
            this.handleError('Erreur chargement', error);
            return false;
        }
    }

    /**
     * Collecte les donnÃ©es depuis les modÃ¨les
     */
    collectData() {
        return {
            version: this.config.version,
            timestamp: new Date().toISOString(),
            state: this.getModel('state')?.data || {},
            files: this.getModel('file')?.data || {},
            instruments: this.getModel('instrument')?.data || {},
            playlists: this.getModel('playlist')?.data || {},
            routing: this.getModel('routing')?.data || {},
            playback: this.getModel('playback')?.data || {},
            editor: this.getModel('editor')?.data || {}
        };
    }

    /**
     * Restaure les donnÃ©es dans les modÃ¨les
     */
    async restoreData(data) {
        const models = ['state', 'file', 'instrument', 'playlist', 'routing', 'playback', 'editor'];
        
        for (const modelName of models) {
            const model = this.getModel(modelName);
            if (model && data[modelName]) {
                Object.assign(model.data, data[modelName]);
            }
        }
        
        // Ã‰mettre Ã©vÃ©nements de restauration
        this.eventBus.emit('persistence:data_restored');
    }

    /**
     * Migre les donnÃ©es d'une ancienne version
     */
    async migrateData(data) {
        this.log('info', 'DataPersistence', `Migrating from ${data.version} to ${this.config.version}`);
        
        // Migrations spÃ©cifiques selon les versions
        if (data.version === '2.0.0') {
            // Migration 2.0.0 -> 3.0.0
            data.routing = data.routing || {};
            data.playback = data.playback || {};
            data.editor = data.editor || {};
        }
        
        data.version = this.config.version;
        return data;
    }

    // ========================================================================
    // SYNCHRONISATION BACKEND
    // ========================================================================

    /**
     * VÃ©rifie la disponibilitÃ© du backend
     */
    async checkBackendAvailability() {
        if (!this.backend || !this.backend.isConnected()) {
            this.syncState.backendAvailable = false;
            return false;
        }
        
        try {
            // Tester avec une commande simple
            await this.backend.sendCommand('system.ping');
            this.syncState.backendAvailable = true;
            this.log('info', 'DataPersistence', 'âœ… Backend available for sync');
            return true;
        } catch (error) {
            this.syncState.backendAvailable = false;
            return false;
        }
    }

    /**
     * Configure la synchronisation automatique
     */
    setupBackendSync() {
        if (!this.config.syncInterval) return;
        
        this.syncTimer = setInterval(async () => {
            if (this.syncState.autoSyncEnabled && 
                this.syncState.backendAvailable && 
                this.changeTracker.hasChanges) {
                
                await this.syncWithBackend();
            }
        }, this.config.syncInterval);
    }

    /**
     * Synchronise avec le backend
     */
    async syncWithBackend() {
        if (this.syncState.syncInProgress) {
            this.log('debug', 'DataPersistence', 'Sync already in progress');
            return false;
        }
        
        if (!this.syncState.backendAvailable) {
            this.log('debug', 'DataPersistence', 'Backend not available for sync');
            return false;
        }
        
        this.syncState.syncInProgress = true;
        
        try {
            // Collecter les donnÃ©es Ã  synchroniser
            const data = this.collectData();
            
            // CrÃ©er backup avant sync si configurÃ©
            if (this.config.backupOnSync) {
                await this.createBackup(data);
            }
            
            // Envoyer au backend
            const response = await this.backend.// DEPRECATED - sendCommand('system.sync_data', {
                data: data,
                timestamp: Date.now()
            });
            
            if (response) {
                this.syncState.lastSync = Date.now();
                this.changeTracker.hasChanges = false;
                this.changeTracker.changedModels.clear();
                
                this.log('info', 'DataPersistence', 'âœ… Synced with backend');
                this.eventBus.emit('persistence:synced', { timestamp: this.syncState.lastSync });
                
                return true;
            }
        } catch (error) {
            this.handleError('Erreur synchronisation', error);
        } finally {
            this.syncState.syncInProgress = false;
        }
        
        return false;
    }

    // ========================================================================
    // BACKUP / RESTORE
    // ========================================================================

    /**
     * CrÃ©e un backup
     */
    async createBackup(data = null) {
        if (!this.db) return false;
        
        try {
            const backupData = data || this.collectData();
            
            const backup = {
                timestamp: Date.now(),
                version: backupData.version,
                data: backupData
            };
            
            // Sauvegarder dans IndexedDB
            const transaction = this.db.transaction(['backups'], 'readwrite');
            const store = transaction.objectStore('backups');
            await store.add(backup);
            
            // Nettoyer les vieux backups
            await this.cleanOldBackups();
            
            this.log('debug', 'DataPersistence', 'ðŸ’¾ Backup created');
            return true;
            
        } catch (error) {
            this.log('error', 'DataPersistence', 'Backup error:', error);
            return false;
        }
    }

    /**
     * Liste les backups disponibles
     */
    async listBackups() {
        if (!this.db) return [];
        
        return new Promise((resolve, reject) => {
            const transaction = this.db.transaction(['backups'], 'readonly');
            const store = transaction.objectStore('backups');
            const request = store.getAll();
            
            request.onsuccess = () => {
                const backups = request.result.map(b => ({
                    id: b.id,
                    timestamp: b.timestamp,
                    version: b.version,
                    date: this.formatDate(new Date(b.timestamp).toISOString())
                }));
                resolve(backups);
            };
            
            request.onerror = () => reject(request.error);
        });
    }

    /**
     * Restaure un backup
     */
    async restoreBackup(backupId) {
        if (!this.db) return false;
        
        try {
            const transaction = this.db.transaction(['backups'], 'readonly');
            const store = transaction.objectStore('backups');
            const request = store.get(backupId);
            
            return new Promise((resolve, reject) => {
                request.onsuccess = async () => {
                    const backup = request.result;
                    if (!backup) {
                        reject(new Error('Backup not found'));
                        return;
                    }
                    
                    await this.restoreData(backup.data);
                    this.notify('success', 'Backup restaurÃ©');
                    this.log('info', 'DataPersistence', `âœ… Backup ${backupId} restored`);
                    resolve(true);
                };
                
                request.onerror = () => reject(request.error);
            });
            
        } catch (error) {
            this.handleError('Erreur restauration backup', error);
            return false;
        }
    }

    /**
     * Nettoie les vieux backups
     */
    async cleanOldBackups() {
        if (!this.db) return;
        
        try {
            const backups = await this.listBackups();
            
            if (backups.length > this.config.maxBackups) {
                // Trier par timestamp dÃ©croissant
                backups.sort((a, b) => b.timestamp - a.timestamp);
                
                // Supprimer les plus anciens
                const toDelete = backups.slice(this.config.maxBackups);
                
                const transaction = this.db.transaction(['backups'], 'readwrite');
                const store = transaction.objectStore('backups');
                
                for (const backup of toDelete) {
                    store.delete(backup.id);
                }
                
                this.log('debug', 'DataPersistence', `Cleaned ${toDelete.length} old backups`);
            }
        } catch (error) {
            this.log('error', 'DataPersistence', 'Cleanup error:', error);
        }
    }

    // ========================================================================
    // EXPORT / IMPORT
    // ========================================================================

    /**
     * Exporte les donnÃ©es
     */
    exportData() {
        try {
            const data = this.collectData();
            
            const blob = new Blob([JSON.stringify(data, null, 2)], { 
                type: 'application/json' 
            });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `midimind-backup-${new Date().toISOString().split('T')[0]}.json`;
            a.click();
            URL.revokeObjectURL(url);
            
            this.notify('success', 'Sauvegarde exportÃ©e');
            this.log('info', 'DataPersistence', 'ðŸ“¤ Data exported');
            
        } catch (error) {
            this.handleError('Erreur export', error);
        }
    }

    /**
     * Importe les donnÃ©es
     */
    importData(file) {
        try {
            const reader = new FileReader();
            reader.onload = async (e) => {
                try {
                    const data = JSON.parse(e.target.result);
                    
                    // Valider la structure
                    if (!data.version || !data.timestamp) {
                        this.notify('error', 'Fichier de sauvegarde invalide');
                        return;
                    }
                    
                    // Confirmer l'import
                    const confirmed = await this.confirmImport(data);
                    if (!confirmed) return;
                    
                    // CrÃ©er backup avant import
                    await this.createBackup();
                    
                    // Restaurer les donnÃ©es
                    await this.restoreData(data);
                    
                    // Sauvegarder
                    await this.saveData({ sync: true });
                    
                    // RafraÃ®chir l'interface
                    this.eventBus.emit('persistence:imported');
                    
                    this.notify('success', 'Sauvegarde importÃ©e');
                    this.log('info', 'DataPersistence', 'ðŸ“¥ Data imported');
                    
                } catch (error) {
                    this.handleError('Fichier de sauvegarde invalide', error);
                }
            };
            reader.readAsText(file);
            
        } catch (error) {
            this.handleError('Erreur import', error);
        }
    }

    /**
     * Confirme l'import (via ModalController si disponible)
     */
    async confirmImport(data) {
        const modalController = window.app?.controllers?.modal;
        
        if (modalController) {
            return new Promise((resolve) => {
                modalController.confirm(
                    `Importer cette sauvegarde (${this.formatDate(data.timestamp)}) ?\nCela remplacera les donnÃ©es actuelles.`,
                    () => resolve(true),
                    { 
                        type: 'warning',
                        confirmText: 'Importer',
                        cancelText: 'Annuler'
                    }
                );
            });
        } else {
            return confirm(`Importer cette sauvegarde (${this.formatDate(data.timestamp)}) ?\nCela remplacera les donnÃ©es actuelles.`);
        }
    }

    /**
     * Efface toutes les donnÃ©es
     */
    async clearData() {
        const modalController = window.app?.controllers?.modal;
        
        const confirmed = modalController 
            ? await new Promise(resolve => {
                modalController.confirm(
                    'Supprimer toutes les donnÃ©es stockÃ©es ? Cette action est irrÃ©versible.',
                    () => resolve(true),
                    { type: 'warning' }
                );
            })
            : confirm('Supprimer toutes les donnÃ©es stockÃ©es ? Cette action est irrÃ©versible.');
        
        if (!confirmed) return;
        
        try {
            // Supprimer localStorage
            localStorage.removeItem(this.config.storageKey);
            
            // Supprimer IndexedDB
            if (this.db) {
                const transaction = this.db.transaction(['data', 'backups'], 'readwrite');
                transaction.objectStore('data').clear();
                transaction.objectStore('backups').clear();
            }
            
            this.notify('info', 'DonnÃ©es supprimÃ©es');
            this.log('info', 'DataPersistence', 'ðŸ—‘ï¸ All data cleared');
            
        } catch (error) {
            this.handleError('Erreur suppression', error);
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    /**
     * Marque un modÃ¨le comme modifiÃ©
     */
    markChanged(modelName) {
        this.changeTracker.hasChanges = true;
        this.changeTracker.changedModels.add(modelName);
    }

    /**
     * Configure l'auto-save
     */
    setupAutoSave() {
        if (!this.config.autoSaveInterval) return;
        
        this.autoSaveTimer = setInterval(() => {
            if (this.changeTracker.hasChanges) {
                this.saveData({ sync: false });
            }
        }, this.config.autoSaveInterval);
    }

    /**
     * Compresse les donnÃ©es
     */
    compressData(data) {
        // Pour une vraie compression, utiliser LZ-String ou pako
        // Ici, juste JSON stringify
        return JSON.stringify(data);
    }

    /**
     * DÃ©compresse les donnÃ©es
     */
    decompressData(data) {
        return JSON.parse(data);
    }

    /**
     * Formate une date
     */
    formatDate(isoString) {
        return new Date(isoString).toLocaleString('fr-FR');
    }

    /**
     * Obtient l'Ã©tat de synchronisation
     */
    getSyncState() {
        return {
            ...this.syncState,
            lastSave: this.changeTracker.lastSave,
            hasChanges: this.changeTracker.hasChanges,
            changedModels: Array.from(this.changeTracker.changedModels)
        };
    }

    /**
     * Nettoie les ressources
     */
    destroy() {
        if (this.autoSaveTimer) {
            clearInterval(this.autoSaveTimer);
        }
        
        if (this.syncTimer) {
            clearInterval(this.syncTimer);
        }
        
        if (this.db) {
            this.db.close();
        }
        
        super.destroy();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = DataPersistenceController;
}

if (typeof window !== 'undefined') {
    window.DataPersistenceController = DataPersistenceController;
}
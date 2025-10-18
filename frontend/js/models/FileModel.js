// ============================================================================
// Fichier: frontend/js/models/FileModel.js
// Version: v3.1.1 - FIXED
// Date: 2025-10-18
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// CORRECTIONS v3.1.1:
// ✓ Constructor signature fixed to match Application.js call
// ✓ Proper eventBus initialization
// ✓ Compatible with BaseModel
// ============================================================================

class FileModel extends BaseModel {
    constructor(eventBus, apiClient, logger) {
        // ✅ FIX: Call parent constructor with proper format
        super({}, {
            eventPrefix: 'files',
            autoPersist: false,
            validateOnSet: false
        });
        
        // ✅ FIX: Store dependencies properly
        this.eventBus = eventBus;
        this.apiClient = apiClient;
        this.logger = logger;
        
        // Configuration cache (OPTIMISÉ)
        this.cacheConfig = {
            maxSize: PerformanceConfig.memory.maxCacheSize || 50,  // ✓ RÉDUIT À 50 MB
            maxMidiJsonSize: Math.floor(PerformanceConfig.memory.maxCacheSize / 2) || 25,  // ✓ 25 MB
            enablePreload: PerformanceConfig.memory.enablePreload || false,  // ✓ DÉSACTIVÉ
            cacheTimeout: PerformanceConfig.memory.cacheTimeout || 300000,  // 5 min
            aggressiveGC: PerformanceConfig.memory.aggressiveGC || true
        };
        
        // Données
        this.data = {
            files: [],
            currentFile: null,
            selectedFileId: null,
            directories: [],
            searchResults: []
        };
        
        // État
        this.state = {
            isLoading: false,
            lastScan: null,
            scanInProgress: false,
            totalFiles: 0,
            loadedFiles: 0,
            sortBy: 'name',  // 'name', 'date', 'size'
            sortOrder: 'asc',  // 'asc', 'desc'
            filter: {
                search: '',
                type: 'all',  // 'all', 'midi', 'json'
                minSize: 0,
                maxSize: Infinity
            }
        };
        
        // Cache (OPTIMISÉ)
        this.cache = {
            midiJsonCache: new Map(),  // fileId -> jsonmidi
            metadataCache: new Map(),  // fileId -> metadata
            currentCacheSize: 0,  // en bytes
            currentMidiJsonSize: 0
        };
        
        // Statistiques
        this.stats = {
            scansPerformed: 0,
            filesLoaded: 0,
            cacheHits: 0,
            cacheMisses: 0,
            cacheEvictions: 0
        };
        
        this.logger.info('FileModel', '✓ Model initialized (performance mode)');
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        // ✅ FIX: Check eventBus before attaching events
        if (!this.eventBus) {
            this.logger.error('FileModel', 'EventBus not available!');
            return;
        }
        
        this.attachEvents();
        
        // ✓ Pas de preload automatique en mode performance
        if (!this.cacheConfig.enablePreload) {
            this.logger.info('FileModel', 'Preload disabled (performance mode)');
        }
    }
    
    attachEvents() {
        // ✅ FIX: Verify eventBus exists
        if (!this.eventBus) {
            this.logger.error('FileModel', 'Cannot attach events: eventBus is null');
            return;
        }
        
        this.eventBus.on('app:shutdown', () => {
            this.clearCache();
        });
        
        // Garbage collection agressive si activée
        if (this.cacheConfig.aggressiveGC) {
            setInterval(() => {
                this.performGarbageCollection();
            }, 60000);  // Toutes les minutes
        }
    }
    
    // ========================================================================
    // SCAN FICHIERS
    // ========================================================================
    
    async scan(directory = null) {
        if (this.state.scanInProgress) {
            this.logger.warn('FileModel', 'Scan already in progress');
            return false;
        }
        
        this.state.scanInProgress = true;
        this.state.isLoading = true;
        
        this.eventBus.emit('files:scan-started', { directory });
        
        try {
            const response = await this.apiClient.sendCommand('files.scan', {
                directory: directory
            });
            
            if (response.success && response.files) {
                this.data.files = response.files;
                this.state.totalFiles = response.files.length;
                this.state.lastScan = Date.now();
                this.stats.scansPerformed++;
                
                this.logger.info('FileModel', `✓ Scan complete: ${response.files.length} files`);
                
                this.eventBus.emit('files:scan-complete', {
                    files: response.files,
                    count: response.files.length
                });
                
                return true;
            } else {
                this.logger.error('FileModel', `Scan failed: ${response.error}`);
                this.eventBus.emit('files:scan-error', { error: response.error });
                return false;
            }
            
        } catch (error) {
            this.logger.error('FileModel', `Scan error: ${error.message}`);
            this.eventBus.emit('files:scan-error', { error: error.message });
            return false;
            
        } finally {
            this.state.scanInProgress = false;
            this.state.isLoading = false;
        }
    }
    
    // ========================================================================
    // SÉLECTION FICHIER
    // ========================================================================
    
    selectFile(fileId) {
        const file = this.data.files.find(f => f.id === fileId);
        
        if (!file) {
            this.logger.warn('FileModel', `File not found: ${fileId}`);
            return false;
        }
        
        this.data.selectedFileId = fileId;
        this.data.currentFile = file;
        
        this.eventBus.emit('files:file-selected', { file });
        
        return true;
    }
    
    getSelectedFile() {
        return this.data.currentFile;
    }
    
    // ========================================================================
    // CHARGEMENT FICHIER AVEC CACHE (OPTIMISÉ)
    // ========================================================================
    
    async loadFile(fileId) {
        // Vérifier le cache d'abord
        if (this.cache.midiJsonCache.has(fileId)) {
            this.stats.cacheHits++;
            this.logger.debug('FileModel', `Cache hit for file: ${fileId}`);
            
            const cachedData = this.cache.midiJsonCache.get(fileId);
            
            this.eventBus.emit('files:file-loaded', {
                fileId,
                jsonmidi: cachedData.jsonmidi,
                fromCache: true
            });
            
            return cachedData.jsonmidi;
        }
        
        // Cache miss - charger depuis backend
        this.stats.cacheMisses++;
        this.logger.debug('FileModel', `Cache miss for file: ${fileId}`);
        
        try {
            this.state.isLoading = true;
            
            const response = await this.apiClient.sendCommand('files.load', {
                file_id: fileId
            });
            
            if (response.success && response.jsonmidi) {
                const jsonmidi = response.jsonmidi;
                
                // Calculer taille approximative
                const dataSize = this.estimateSize(jsonmidi);
                
                // Vérifier si on peut mettre en cache
                if (dataSize < this.cacheConfig.maxMidiJsonSize * 1024 * 1024) {
                    this.addToCache(fileId, jsonmidi, dataSize);
                } else {
                    this.logger.warn('FileModel', `File too large for cache: ${dataSize / 1024 / 1024} MB`);
                }
                
                this.stats.filesLoaded++;
                
                this.eventBus.emit('files:file-loaded', {
                    fileId,
                    jsonmidi,
                    fromCache: false
                });
                
                return jsonmidi;
                
            } else {
                this.logger.error('FileModel', `Load failed: ${response.error}`);
                this.eventBus.emit('files:load-error', { 
                    fileId, 
                    error: response.error 
                });
                return null;
            }
            
        } catch (error) {
            this.logger.error('FileModel', `Load error: ${error.message}`);
            this.eventBus.emit('files:load-error', { 
                fileId, 
                error: error.message 
            });
            return null;
            
        } finally {
            this.state.isLoading = false;
        }
    }
    
    // ========================================================================
    // GESTION CACHE (OPTIMISÉ)
    // ========================================================================
    
    addToCache(fileId, jsonmidi, size) {
        // Vérifier si on dépasse la limite
        while (this.cache.currentMidiJsonSize + size > this.cacheConfig.maxMidiJsonSize * 1024 * 1024) {
            this.evictOldestCacheEntry();
        }
        
        this.cache.midiJsonCache.set(fileId, {
            jsonmidi: jsonmidi,
            size: size,
            timestamp: Date.now()
        });
        
        this.cache.currentMidiJsonSize += size;
        
        this.logger.debug('FileModel', `Added to cache: ${fileId} (${size / 1024} KB) - Total: ${this.cache.currentMidiJsonSize / 1024 / 1024} MB`);
    }
    
    evictOldestCacheEntry() {
        if (this.cache.midiJsonCache.size === 0) return;
        
        // Trouver l'entrée la plus ancienne
        let oldestKey = null;
        let oldestTime = Infinity;
        
        for (const [key, value] of this.cache.midiJsonCache.entries()) {
            if (value.timestamp < oldestTime) {
                oldestTime = value.timestamp;
                oldestKey = key;
            }
        }
        
        if (oldestKey) {
            const entry = this.cache.midiJsonCache.get(oldestKey);
            this.cache.currentMidiJsonSize -= entry.size;
            this.cache.midiJsonCache.delete(oldestKey);
            this.stats.cacheEvictions++;
            
            this.logger.debug('FileModel', `Evicted from cache: ${oldestKey}`);
        }
    }
    
    clearCache() {
        this.cache.midiJsonCache.clear();
        this.cache.metadataCache.clear();
        this.cache.currentCacheSize = 0;
        this.cache.currentMidiJsonSize = 0;
        
        this.logger.info('FileModel', '✓ Cache cleared');
    }
    
    performGarbageCollection() {
        const timeout = this.cacheConfig.cacheTimeout;
        const now = Date.now();
        let evicted = 0;
        
        for (const [key, value] of this.cache.midiJsonCache.entries()) {
            if (now - value.timestamp > timeout) {
                this.cache.currentMidiJsonSize -= value.size;
                this.cache.midiJsonCache.delete(key);
                evicted++;
            }
        }
        
        if (evicted > 0) {
            this.logger.debug('FileModel', `GC: evicted ${evicted} expired entries`);
        }
    }
    
    estimateSize(obj) {
        // Estimation approximative de la taille en bytes
        const json = JSON.stringify(obj);
        return new Blob([json]).size;
    }
    
    // ========================================================================
    // MÉTADONNÉES
    // ========================================================================
    
    async getMetadata(fileId) {
        // Vérifier cache
        if (this.cache.metadataCache.has(fileId)) {
            return this.cache.metadataCache.get(fileId);
        }
        
        try {
            const response = await this.apiClient.sendCommand('files.getMetadata', {
                file_id: fileId
            });
            
            if (response.success && response.metadata) {
                // Mettre en cache
                this.cache.metadataCache.set(fileId, response.metadata);
                return response.metadata;
            }
            
            return null;
            
        } catch (error) {
            this.logger.error('FileModel', `Metadata error: ${error.message}`);
            return null;
        }
    }
    
    // ========================================================================
    // RECHERCHE & FILTRAGE
    // ========================================================================
    
    search(query) {
        if (!query || query.trim() === '') {
            this.data.searchResults = [];
            this.eventBus.emit('files:search-cleared');
            return;
        }
        
        const lowerQuery = query.toLowerCase();
        
        this.data.searchResults = this.data.files.filter(file => {
            return file.name.toLowerCase().includes(lowerQuery) ||
                   (file.path && file.path.toLowerCase().includes(lowerQuery));
        });
        
        this.eventBus.emit('files:search-results', {
            query,
            results: this.data.searchResults,
            count: this.data.searchResults.length
        });
    }
    
    filter(filterConfig) {
        this.state.filter = { ...this.state.filter, ...filterConfig };
        this.applyFilter();
    }
    
    applyFilter() {
        let filtered = [...this.data.files];
        
        // Filtre par type
        if (this.state.filter.type !== 'all') {
            filtered = filtered.filter(f => f.type === this.state.filter.type);
        }
        
        // Filtre par taille
        filtered = filtered.filter(f => 
            f.size >= this.state.filter.minSize &&
            f.size <= this.state.filter.maxSize
        );
        
        // Filtre par recherche
        if (this.state.filter.search) {
            const query = this.state.filter.search.toLowerCase();
            filtered = filtered.filter(f => 
                f.name.toLowerCase().includes(query)
            );
        }
        
        this.data.searchResults = filtered;
        
        this.eventBus.emit('files:filtered', {
            results: filtered,
            count: filtered.length
        });
    }
    
    // ========================================================================
    // TRI
    // ========================================================================
    
    sort(sortBy, sortOrder) {
        this.state.sortBy = sortBy;
        this.state.sortOrder = sortOrder;
        
        this.data.files.sort((a, b) => {
            let comparison = 0;
            
            switch (sortBy) {
                case 'name':
                    comparison = a.name.localeCompare(b.name);
                    break;
                case 'date':
                    comparison = (a.modified || 0) - (b.modified || 0);
                    break;
                case 'size':
                    comparison = (a.size || 0) - (b.size || 0);
                    break;
            }
            
            return sortOrder === 'asc' ? comparison : -comparison;
        });
        
        this.eventBus.emit('files:sorted', { sortBy, sortOrder });
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    getAllFiles() {
        return this.data.files;
    }
    
    getFilteredFiles() {
        return this.data.searchResults.length > 0 
            ? this.data.searchResults 
            : this.data.files;
    }
    
    getFileById(fileId) {
        return this.data.files.find(f => f.id === fileId);
    }
    
    getState() {
        return {
            ...this.state,
            cacheSize: this.cache.currentMidiJsonSize,
            cachedFiles: this.cache.midiJsonCache.size
        };
    }
    
    getStats() {
        return {
            ...this.stats,
            cacheSize: this.cache.currentMidiJsonSize,
            cachedFiles: this.cache.midiJsonCache.size,
            cacheHitRate: this.stats.cacheHits / (this.stats.cacheHits + this.stats.cacheMisses) || 0
        };
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        this.logger.info('FileModel', 'Destroying...');
        
        this.clearCache();
        this.data.files = [];
        
        this.logger.info('FileModel', '✓ Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileModel;
}

if (typeof window !== 'undefined') {
    window.FileModel = FileModel;
}
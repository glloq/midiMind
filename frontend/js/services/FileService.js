// ============================================================================
// Fichier: frontend/js/services/FileService.js
// Version: v3.1.1 - LOGGER FIX
// Date: 2025-10-30
// ============================================================================
// CORRECTIONS v3.1.1:
// ✅ Logger avec fallback robuste
// ✅ Méthode log() helper sécurisée
// ============================================================================

class FileService {
    constructor(backendService, eventBus, logger) {
        this.backend = backendService;
        this.eventBus = eventBus;
        this.logger = logger || this.createFallbackLogger();
        
        // Cache des fichiers
        this.fileCache = new Map();
        this.lastScanTimestamp = 0;
        
        // Index de recherche
        this.searchIndex = new Map();
        
        // État du service
        this.state = {
            isScanning: false,
            isUploading: false,
            uploadProgress: 0,
            lastScanDuration: 0,
            totalFiles: 0,
            totalSize: 0
        };
        
        // Configuration
        this.config = {
            cacheExpiration: 60000,        // 1 minute
            autoRefresh: true,
            supportedExtensions: ['.mid', '.midi', '.MID', '.MIDI'],
            maxFileSize: 10 * 1024 * 1024, // 10 MB
            uploadChunkSize: 64 * 1024,     // 64 KB
            allowedMimeTypes: ['audio/midi', 'audio/x-midi', 'application/octet-stream']
        };
        
        // Statistiques
        this.stats = {
            scansPerformed: 0,
            filesLoaded: 0,
            cacheHits: 0,
            cacheMisses: 0,
            uploadsCount: 0,
            uploadsSucceeded: 0,
            uploadsFailed: 0,
            deletesCount: 0
        };
        
        this.log('info', 'FileService', '✓ Service initialized');
        
        this._bindBackendEvents();
    }
    
    /**
     * Crée un logger fallback si aucun logger fourni
     */
    createFallbackLogger() {
        return {
            debug: (...args) => console.log('[DEBUG]', ...args),
            info: (...args) => console.info('[INFO]', ...args),
            warn: (...args) => console.warn('[WARN]', ...args),
            error: (...args) => console.error('[ERROR]', ...args),
            log: (...args) => console.log(...args)
        };
    }
    
    /**
     * Log sécurisé
     */
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        } else {
            console[level]?.(...args) || console.log(...args);
        }
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    _bindBackendEvents() {
        if (!this.eventBus) return;
        
        this.eventBus.on('backend:event:files_list', (data) => {
            this._handleFilesList(data);
        });
        
        this.eventBus.on('backend:event:file_added', (data) => {
            this._handleFileAdded(data);
        });
        
        this.eventBus.on('backend:event:file_removed', (data) => {
            this._handleFileRemoved(data);
        });
        
        this.eventBus.on('backend:connected', async () => {
            this.log('info', 'FileService', 'Backend connected, refreshing files');
            await this.scanFiles();
        });
    }
    
    // ========================================================================
    // SCAN & LISTE
    // ========================================================================
    
    /**
     * Scan tous les fichiers MIDI disponibles
     */
    async scanFiles(forceRefresh = false) {
        // Vérifier si scan déjà en cours
        if (this.state.isScanning) {
            this.log('warn', 'FileService', 'Scan already in progress');
            return this.getFilesFromCache();
        }
        
        // Vérifier cache si pas force refresh
        if (!forceRefresh && this._isCacheValid()) {
            this.stats.cacheHits++;
            this.log('debug', 'FileService', 'Using cached files');
            return this.getFilesFromCache();
        }
        
        this.state.isScanning = true;
        this.stats.cacheMisses++;
        const startTime = Date.now();
        
        try {
            this.log('info', 'FileService', 'Scanning files...');
            
            // Émettre événement scan start
            if (this.eventBus) {
                this.eventBus.emit('files:scan_start');
            }
            
            // Appeler backend
            let files = [];
            if (this.backend && typeof this.backend.listFiles === 'function') {
                files = await this.backend.listFiles();
            } else {
                this.log('warn', 'FileService', 'Backend not available, using mock data');
                files = this._getMockFiles();
            }
            
            // Mettre à jour cache
            this._updateCache(files);
            
            // Mettre à jour index de recherche
            this._updateSearchIndex(files);
            
            // Statistiques
            this.state.totalFiles = files.length;
            this.state.totalSize = files.reduce((sum, f) => sum + (f.size || 0), 0);
            this.state.lastScanDuration = Date.now() - startTime;
            this.stats.scansPerformed++;
            this.stats.filesLoaded += files.length;
            
            this.lastScanTimestamp = Date.now();
            
            this.log('info', 'FileService', `Scan complete: ${files.length} files in ${this.state.lastScanDuration}ms`);
            
            // Émettre événement scan complete
            if (this.eventBus) {
                this.eventBus.emit('files:scan_complete', { 
                    files,
                    count: files.length,
                    duration: this.state.lastScanDuration
                });
            }
            
            return files;
            
        } catch (error) {
            this.log('error', 'FileService', 'Scan failed:', error);
            
            if (this.eventBus) {
                this.eventBus.emit('files:scan_error', { error });
            }
            
            // Retourner cache si disponible
            return this.getFilesFromCache();
            
        } finally {
            this.state.isScanning = false;
        }
    }
    
    /**
     * Récupère les fichiers depuis le cache
     */
    getFilesFromCache() {
        return Array.from(this.fileCache.values());
    }
    
    /**
     * Vérifie si le cache est valide
     */
    _isCacheValid() {
        if (this.fileCache.size === 0) return false;
        const age = Date.now() - this.lastScanTimestamp;
        return age < this.config.cacheExpiration;
    }
    
    /**
     * Met à jour le cache
     */
    _updateCache(files) {
        this.fileCache.clear();
        files.forEach(file => {
            this.fileCache.set(file.id || file.path, file);
        });
    }
    
    /**
     * Met à jour l'index de recherche
     */
    _updateSearchIndex(files) {
        this.searchIndex.clear();
        files.forEach(file => {
            const searchText = [
                file.name || '',
                file.path || '',
                file.tags?.join(' ') || ''
            ].join(' ').toLowerCase();
            
            this.searchIndex.set(file.id || file.path, searchText);
        });
    }
    
    /**
     * Données mock pour développement
     */
    _getMockFiles() {
        return [
            {
                id: 'mock-1',
                name: 'Example Song.mid',
                path: '/midi/Example Song.mid',
                size: 45678,
                duration: 180,
                tracks: 4,
                noteCount: 1234,
                created: Date.now() - 86400000,
                modified: Date.now() - 3600000
            },
            {
                id: 'mock-2',
                name: 'Test Composition.mid',
                path: '/midi/Test Composition.mid',
                size: 23456,
                duration: 120,
                tracks: 2,
                noteCount: 567,
                created: Date.now() - 172800000,
                modified: Date.now() - 7200000
            }
        ];
    }
    
    // ========================================================================
    // GESTION DES ÉVÉNEMENTS BACKEND
    // ========================================================================
    
    _handleFilesList(data) {
        this.log('debug', 'FileService', 'Received files list:', data);
        
        if (data.files) {
            this._updateCache(data.files);
            this._updateSearchIndex(data.files);
            
            if (this.eventBus) {
                this.eventBus.emit('files:loaded', { files: data.files });
            }
        }
    }
    
    _handleFileAdded(data) {
        this.log('info', 'FileService', 'File added:', data);
        
        if (data.file) {
            this.fileCache.set(data.file.id || data.file.path, data.file);
            
            if (this.eventBus) {
                this.eventBus.emit('files:file_added', { file: data.file });
            }
        }
    }
    
    _handleFileRemoved(data) {
        this.log('info', 'FileService', 'File removed:', data);
        
        if (data.fileId || data.path) {
            const key = data.fileId || data.path;
            this.fileCache.delete(key);
            
            if (this.eventBus) {
                this.eventBus.emit('files:file_removed', { fileId: key });
            }
        }
    }
    
    // ========================================================================
    // UPLOAD
    // ========================================================================
    
    /**
     * Upload un fichier MIDI
     */
    async uploadFile(file, metadata = {}) {
        if (this.state.isUploading) {
            throw new Error('Upload already in progress');
        }
        
        this.state.isUploading = true;
        this.state.uploadProgress = 0;
        this.stats.uploadsCount++;
        
        try {
            this.log('info', 'FileService', 'Uploading file:', file.name);
            
            if (this.eventBus) {
                this.eventBus.emit('files:upload_start', { filename: file.name });
            }
            
            // Valider fichier
            this._validateFile(file);
            
            // Lire fichier
            const content = await this._readFile(file);
            
            // Envoyer au backend
            let result;
            if (this.backend && typeof this.backend.uploadFile === 'function') {
                result = await this.backend.uploadFile({
                    filename: file.name,
                    content: content,
                    metadata: metadata
                });
            } else {
                // Mock upload
                result = {
                    success: true,
                    fileId: 'mock-' + Date.now(),
                    message: 'File uploaded (mock)'
                };
            }
            
            this.stats.uploadsSucceeded++;
            this.log('info', 'FileService', 'Upload complete:', result);
            
            if (this.eventBus) {
                this.eventBus.emit('files:upload_complete', { 
                    file: result,
                    filename: file.name
                });
            }
            
            // Rafraîchir liste
            await this.scanFiles(true);
            
            return result;
            
        } catch (error) {
            this.stats.uploadsFailed++;
            this.log('error', 'FileService', 'Upload failed:', error);
            
            if (this.eventBus) {
                this.eventBus.emit('files:upload_error', { 
                    error,
                    filename: file.name
                });
            }
            
            throw error;
            
        } finally {
            this.state.isUploading = false;
            this.state.uploadProgress = 0;
        }
    }
    
    /**
     * Valide un fichier
     */
    _validateFile(file) {
        // Taille
        if (file.size > this.config.maxFileSize) {
            throw new Error(`File too large: ${file.size} bytes (max ${this.config.maxFileSize})`);
        }
        
        // Extension
        const ext = '.' + file.name.split('.').pop();
        if (!this.config.supportedExtensions.includes(ext)) {
            throw new Error(`Unsupported file type: ${ext}`);
        }
        
        return true;
    }
    
    /**
     * Lit un fichier
     */
    _readFile(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            
            reader.onload = (e) => {
                this.state.uploadProgress = 100;
                resolve(e.target.result);
            };
            
            reader.onerror = (e) => {
                reject(new Error('Failed to read file'));
            };
            
            reader.onprogress = (e) => {
                if (e.lengthComputable) {
                    this.state.uploadProgress = (e.loaded / e.total) * 100;
                }
            };
            
            reader.readAsArrayBuffer(file);
        });
    }
    
    // ========================================================================
    // RECHERCHE
    // ========================================================================
    
    /**
     * Recherche de fichiers
     */
    searchFiles(query) {
        if (!query || query.trim() === '') {
            return this.getFilesFromCache();
        }
        
        const searchTerm = query.toLowerCase().trim();
        const results = [];
        
        for (const [fileId, searchText] of this.searchIndex.entries()) {
            if (searchText.includes(searchTerm)) {
                const file = this.fileCache.get(fileId);
                if (file) {
                    results.push(file);
                }
            }
        }
        
        this.log('debug', 'FileService', `Search "${query}" found ${results.length} results`);
        
        return results;
    }
    
    // ========================================================================
    // OPÉRATIONS FICHIER
    // ========================================================================
    
    /**
     * Récupère un fichier par ID
     */
    async getFile(fileId) {
        // Vérifier cache
        const cached = this.fileCache.get(fileId);
        if (cached) {
            return cached;
        }
        
        // Charger depuis backend
        if (this.backend && typeof this.backend.getFile === 'function') {
            const file = await this.backend.getFile(fileId);
            this.fileCache.set(fileId, file);
            return file;
        }
        
        throw new Error(`File not found: ${fileId}`);
    }
    
    /**
     * Supprime un fichier
     */
    async deleteFile(fileId) {
        this.log('info', 'FileService', 'Deleting file:', fileId);
        
        try {
            if (this.backend && typeof this.backend.deleteFile === 'function') {
                await this.backend.deleteFile(fileId);
            }
            
            this.fileCache.delete(fileId);
            this.searchIndex.delete(fileId);
            this.stats.deletesCount++;
            
            if (this.eventBus) {
                this.eventBus.emit('files:file_deleted', { fileId });
            }
            
            this.log('info', 'FileService', 'File deleted:', fileId);
            
        } catch (error) {
            this.log('error', 'FileService', 'Delete failed:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    getStats() {
        return {
            ...this.stats,
            cacheSize: this.fileCache.size,
            cacheAge: Date.now() - this.lastScanTimestamp,
            ...this.state
        };
    }
    
    // ========================================================================
    // CLEANUP
    // ========================================================================
    
    destroy() {
        this.fileCache.clear();
        this.searchIndex.clear();
        this.log('info', 'FileService', 'Service destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileService;
}

if (typeof window !== 'undefined') {
    window.FileService = FileService;
}
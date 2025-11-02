// ============================================================================
// Fichier: frontend/js/services/FileService.js
// Version: v4.0.0 - API COMPATIBLE DOCUMENTATION v4.2.2
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v4.0.0:
// ✅ Utilise files.list, files.read, files.write, files.delete
// ✅ Compatible avec le format API documenté
// ✅ Gestion des réponses { success: true, data: {...} }
//
// CONSERVÉ:
// ✅ Logger avec fallback robuste
// ✅ Cache des fichiers
// ✅ Index de recherche
// ✅ Statistiques
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
        
        this.log('info', 'FileService', '✓ Service initialized (v4.0.0)');
        
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
        
        // Événements backend
        this.eventBus.on('backend:event:file:added', (data) => {
            this._handleFileAdded(data);
        });
        
        this.eventBus.on('backend:event:file:removed', (data) => {
            this._handleFileRemoved(data);
        });
        
        this.eventBus.on('backend:event:file:modified', (data) => {
            this._handleFileModified(data);
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
            
            // ✅ Appeler backend avec nouvelle API
            let filesData = [];
            if (this.backend && typeof this.backend.listFiles === 'function') {
                const response = await this.backend.listFiles('/midi');
                // Le backend renvoie { success: true, data: { files: [...] } }
                filesData = response.files || [];
            } else {
                this.log('warn', 'FileService', 'Backend not available, using mock data');
                filesData = this._getMockFiles();
            }
            
            // Mettre à jour cache
            this._updateCache(filesData);
            
            // Mettre à jour index de recherche
            this._updateSearchIndex(filesData);
            
            // Statistiques
            this.state.totalFiles = filesData.length;
            this.state.totalSize = filesData.reduce((sum, f) => sum + (f.size || 0), 0);
            this.state.lastScanDuration = Date.now() - startTime;
            this.stats.scansPerformed++;
            this.stats.filesLoaded += filesData.length;
            
            this.lastScanTimestamp = Date.now();
            
            this.log('info', 'FileService', `Scan complete: ${filesData.length} files in ${this.state.lastScanDuration}ms`);
            
            // Émettre événement scan complete
            if (this.eventBus) {
                this.eventBus.emit('files:scan_complete', { 
                    files: filesData,
                    count: filesData.length,
                    duration: this.state.lastScanDuration
                });
            }
            
            return filesData;
            
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
            const fileId = file.id || file.path || file.name;
            this.fileCache.set(fileId, file);
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
            
            const fileId = file.id || file.path || file.name;
            this.searchIndex.set(fileId, searchText);
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
                modified: Date.now()
            },
            {
                id: 'mock-2',
                name: 'Test Track.mid',
                path: '/midi/Test Track.mid',
                size: 23456,
                duration: 120,
                tracks: 2,
                noteCount: 567,
                modified: Date.now()
            }
        ];
    }
    
    // ========================================================================
    // GESTION DES ÉVÉNEMENTS BACKEND
    // ========================================================================
    
    /**
     * Gère l'ajout d'un fichier
     */
    _handleFileAdded(data) {
        if (data && data.file) {
            const fileId = data.file.id || data.file.path || data.file.name;
            this.fileCache.set(fileId, data.file);
            
            // Mettre à jour l'index de recherche
            const searchText = [
                data.file.name || '',
                data.file.path || '',
                data.file.tags?.join(' ') || ''
            ].join(' ').toLowerCase();
            this.searchIndex.set(fileId, searchText);
            
            if (this.eventBus) {
                this.eventBus.emit('files:file_added', { file: data.file });
            }
        }
    }
    
    /**
     * Gère la suppression d'un fichier
     */
    _handleFileRemoved(data) {
        if (data && (data.fileId || data.path)) {
            const key = data.fileId || data.path;
            this.fileCache.delete(key);
            this.searchIndex.delete(key);
            
            if (this.eventBus) {
                this.eventBus.emit('files:file_removed', { fileId: key });
            }
        }
    }
    
    /**
     * Gère la modification d'un fichier
     */
    _handleFileModified(data) {
        if (data && data.file) {
            const fileId = data.file.id || data.file.path || data.file.name;
            this.fileCache.set(fileId, data.file);
            
            if (this.eventBus) {
                this.eventBus.emit('files:file_modified', { file: data.file });
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
            
            // Convertir en base64
            const base64Data = btoa(
                new Uint8Array(content).reduce((data, byte) => data + String.fromCharCode(byte), '')
            );
            
            // ✅ Envoyer au backend avec nouvelle API
            let result;
            if (this.backend && typeof this.backend.writeFile === 'function') {
                result = await this.backend.writeFile(
                    `/midi/${file.name}`,
                    base64Data,
                    'base64'
                );
            } else {
                // Mock upload
                result = {
                    success: true,
                    path: `/midi/${file.name}`,
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
        
        // ✅ Charger depuis backend avec nouvelle API
        if (this.backend && typeof this.backend.getFileInfo === 'function') {
            const fileInfo = await this.backend.getFileInfo(fileId);
            this.fileCache.set(fileId, fileInfo);
            return fileInfo;
        }
        
        throw new Error(`File not found: ${fileId}`);
    }
    
    /**
     * Récupère le contenu d'un fichier
     */
    async readFile(path) {
        if (this.backend && typeof this.backend.readFile === 'function') {
            return await this.backend.readFile(path);
        }
        throw new Error('Backend not available');
    }
    
    /**
     * Vérifie si un fichier existe
     */
    async fileExists(path) {
        if (this.backend && typeof this.backend.fileExists === 'function') {
            const response = await this.backend.fileExists(path);
            return response.exists || false;
        }
        return false;
    }
    
    /**
     * Supprime un fichier
     */
    async deleteFile(fileIdOrPath) {
        this.log('info', 'FileService', 'Deleting file:', fileIdOrPath);
        
        try {
            // ✅ Supprimer via backend avec nouvelle API
            if (this.backend && typeof this.backend.deleteFile === 'function') {
                await this.backend.deleteFile(fileIdOrPath);
            }
            
            this.fileCache.delete(fileIdOrPath);
            this.searchIndex.delete(fileIdOrPath);
            this.stats.deletesCount++;
            
            if (this.eventBus) {
                this.eventBus.emit('files:file_deleted', { fileId: fileIdOrPath });
            }
            
            this.log('info', 'FileService', 'File deleted:', fileIdOrPath);
            
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
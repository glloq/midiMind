// ============================================================================
// Fichier: frontend/js/services/FileService.js
// Version: v3.0.2 - COMPLET (CRUD complet)
// Date: 2025-10-08
// Projet: midiMind v3.0 - SystÃƒÂ¨me d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Service de gestion des fichiers MIDI.
//   Centralise toutes les opÃƒÂ©rations sur les fichiers.
//
// CORRECTIONS v3.0.2:
//   Ã¢Å“â€¦ uploadFile() complet avec validation
//   Ã¢Å“â€¦ deleteFile() complet avec confirmation
//   Ã¢Å“â€¦ moveFile() implÃƒÂ©mentÃƒÂ©
//   Ã¢Å“â€¦ renameFile() implÃƒÂ©mentÃƒÂ©
//   Ã¢Å“â€¦ Gestion erreurs robuste
//   Ã¢Å“â€¦ Progress callbacks pour upload
//
// Auteur: midiMind Team
// ============================================================================

class FileService {
    constructor(backendService, eventBus, logger) {
        this.backend = backendService;
        this.eventBus = eventBus;
        this.logger = logger;
        
        // Cache des fichiers
        this.fileCache = new Map();
        this.lastScanTimestamp = 0;
        
        // Index de recherche
        this.searchIndex = new Map();
        
        // Ãƒâ€°tat du service
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
        
        this.logger.info('FileService', 'Ã¢Å“â€œ Service initialized');
        
        // Ãƒâ€°couter les ÃƒÂ©vÃƒÂ©nements backend
        this._bindBackendEvents();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    _bindBackendEvents() {
        // Ãƒâ€°couter la rÃƒÂ©ception de la liste de fichiers
        this.eventBus.on('backend:event:files_list', (data) => {
            this._handleFilesList(data);
        });
        
        // Ãƒâ€°couter les ÃƒÂ©vÃƒÂ©nements de fichier ajoutÃƒÂ©/supprimÃƒÂ©
        this.eventBus.on('backend:event:file_added', (data) => {
            this._handleFileAdded(data);
        });
        
        this.eventBus.on('backend:event:file_removed', (data) => {
            this._handleFileRemoved(data);
        });
        
        // Ãƒâ€°couter la connexion backend
        this.eventBus.on('backend:connected', () => {
            if (this.config.autoRefresh) {
                this.scanFiles();
            }
        });
    }
    
    // ========================================================================
    // SCAN FICHIERS
    // ========================================================================
    
    /**
     * Scanner les fichiers depuis le backend
     */
    async scanFiles(force = false) {
        if (this.state.isScanning) {
            this.logger.warn('FileService', 'Scan already in progress');
            return;
        }
        
        // VÃƒÂ©rifier si cache encore valide
        const now = Date.now();
        if (!force && (now - this.lastScanTimestamp < this.config.cacheExpiration)) {
            this.logger.info('FileService', 'Using cached file list');
            this.stats.cacheHits++;
            return this._getFilesFromCache();
        }
        
        this.state.isScanning = true;
        this.stats.cacheMisses++;
        
        this.eventBus.emit('files:scan-start');
        
        const startTime = Date.now();
        
        try {
            this.logger.info('FileService', 'Ã°Å¸â€Â Scanning files...');
            
            const response = await this.backend.sendCommand('files.scan', {
                recursive: true
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Scan failed');
            }
            
            const files = response.data?.files || response.files || [];
            
            // Vider et remplir le cache
            this.fileCache.clear();
            
            for (const file of files) {
                const normalizedFile = this._normalizeFileData(file);
                this._addToCache(normalizedFile);
            }
            
            // Mettre ÃƒÂ  jour l'ÃƒÂ©tat
            const duration = Date.now() - startTime;
            
            this.state.lastScanDuration = duration;
            this.state.totalFiles = this.fileCache.size;
            this.state.totalSize = this._calculateTotalSize();
            this.lastScanTimestamp = Date.now();
            this.stats.scansPerformed++;
            
            this.logger.info('FileService', 
                `Ã¢Å“â€œ Scan complete: ${files.length} files in ${duration}ms`);
            
            // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
            this.eventBus.emit('files:scan-complete', {
                files: this.getAllFiles(),
                count: this.fileCache.size,
                duration: duration
            });
            
            return this.getAllFiles();
            
        } catch (error) {
            this.logger.error('FileService', 'Scan failed:', error);
            
            this.eventBus.emit('files:scan-error', {
                error: error.message
            });
            
            throw error;
            
        } finally {
            this.state.isScanning = false;
        }
    }
    
    // ========================================================================
    // UPLOAD FICHIER - Ã¢Å“â€¦ COMPLET
    // ========================================================================
    
/**
 * Upload un fichier MIDI
 * @param {File} file - Fichier ÃƒÂ  uploader
 * @param {Function} onProgress - Callback progression
 * @returns {Promise<Object>}
 */
async uploadFile(file, onProgress = null) {
    this.logger.info('FileService', `Uploading: ${file.name}`);
    
    // Validation
    const validation = this._validateFile(file);
    if (!validation.valid) {
        throw new Error(validation.error);
    }
    
    try {
        this.state.isUploading = true;
        this.state.uploadProgress = 0;
        this.eventBus.emit('file:upload:start', { filename: file.name });
        
        // Upload via backend
        const result = await this.backend.uploadFile(file, (progress) => {
            this.state.uploadProgress = progress;
            this.eventBus.emit('file:upload:progress', { 
                filename: file.name, 
                progress 
            });
            if (onProgress) onProgress(progress);
        });
        
        if (!result.success) {
            throw new Error(result.error || 'Upload failed');
        }
        
        // CORRECTION: Invalider cache et rafraÃƒÂ®chir
        this.fileCache.clear();
        this.lastScanTimestamp = 0;
        
        this.logger.info('FileService', `ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Upload successful: ${file.name}`);
        this.eventBus.emit('file:upload:complete', { 
            filename: file.name,
            fileId: result.fileId
        });
        
        // RafraÃƒÂ®chir la liste automatiquement
        await this.scanFiles();
        
        this.stats.uploadsSucceeded++;
        
        return result;
        
    } catch (error) {
        this.logger.error('FileService', 'Upload failed:', error);
        this.eventBus.emit('file:upload:error', { 
            filename: file.name, 
            error: error.message 
        });
        this.stats.uploadsFailed++;
        throw error;
        
    } finally {
        this.state.isUploading = false;
        this.state.uploadProgress = 0;
    }
}

/**
 * Upload un fichier MIDI vers le backend
 * @param {File} file - Fichier ÃƒÂ  uploader
 * @param {Function} onProgress - Callback de progression (0-100)
 * @param {Object} options - Options d'upload
 * @returns {Promise<Object>} RÃƒÂ©sultat de l'upload
 */
async uploadMIDI(file, onProgress, options = {}) {
    this.logger.info('FileService', `Uploading file: ${file.name} (${file.size} bytes)`);
    
    // ========================================================================
    // VALIDATION 1: Extension
    // ========================================================================
    const validExtensions = this.config.supportedExtensions;
    const fileExt = '.' + file.name.split('.').pop().toLowerCase();
    
    if (!validExtensions.includes(fileExt)) {
        throw new Error(
            `Invalid file extension: ${fileExt}. ` +
            `Allowed: ${validExtensions.join(', ')}`
        );
    }
    
    // ========================================================================
    // VALIDATION 2: Taille
    // ========================================================================
    if (file.size > this.config.maxFileSize) {
        const maxMB = (this.config.maxFileSize / (1024 * 1024)).toFixed(1);
        const fileMB = (file.size / (1024 * 1024)).toFixed(1);
        throw new Error(
            `File too large: ${fileMB}MB (max ${maxMB}MB)`
        );
    }
    
    // ========================================================================
    // VALIDATION 3: Type MIME
    // ========================================================================
    if (!this.config.allowedMimeTypes.includes(file.type) && file.type !== '') {
        this.logger.warn('FileService', `Unexpected MIME type: ${file.type}`);
        // Ne pas bloquer, certains OS retournent des MIME types bizarres
    }
    
    try {
        // ====================================================================
        // CONVERSION EN BASE64
        // ====================================================================
        this.state.isUploading = true;
        this.state.uploadProgress = 0;
        
        if (onProgress) onProgress(0);
        
        const base64Content = await this.fileToBase64(file, (progress) => {
            this.state.uploadProgress = Math.floor(progress * 0.5); // 0-50%
            if (onProgress) onProgress(this.state.uploadProgress);
        });
        
        this.logger.debug('FileService', `File converted to base64 (${base64Content.length} chars)`);
        
        if (onProgress) onProgress(50);
        
        // ====================================================================
        // ENVOI AU BACKEND
        // ====================================================================
        const result = await this.backend.sendCommand('files.upload', {
            filename: file.name,
            content: base64Content,
            size: file.size,
            directory: options.directory || '/home/pi/midi-files',
            overwrite: options.overwrite || false
        });
        
        if (onProgress) onProgress(90);
        
        // ====================================================================
        // MISE Ãƒâ‚¬ JOUR CACHE ET STATS
        // ====================================================================
        if (result.success !== false && result.data) {
            const fileData = {
                id: result.data.filepath,
                name: result.data.filename,
                path: result.data.filepath,
                size: result.data.size,
                uploadedAt: Date.now(),
                metadata: result.data.metadata || {}
            };
            
            // Ajouter au cache
            this.fileCache.set(fileData.id, fileData);
            
            // Mettre ÃƒÂ  jour stats
            this.stats.uploadsCount++;
            this.stats.uploadsSucceeded++;
            this.state.totalFiles++;
            this.state.totalSize += fileData.size;
            
            // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
            this.eventBus.emit('file:uploaded', fileData);
            
            this.logger.info('FileService', `Ã¢Å“â€œ File uploaded: ${fileData.name}`);
            
            if (onProgress) onProgress(100);
            
            return fileData;
        } else {
            throw new Error(result.error || 'Upload failed');
        }
        
    } catch (error) {
        this.logger.error('FileService', 'Upload failed:', error);
        
        this.stats.uploadsCount++;
        this.stats.uploadsFailed++;
        
        throw error;
        
    } finally {
        this.state.isUploading = false;
        this.state.uploadProgress = 0;
    }
}

/**
 * Convertit un fichier en base64
 * @param {File} file - Fichier ÃƒÂ  convertir
 * @param {Function} onProgress - Callback de progression
 * @returns {Promise<string>} Contenu base64 (sans prÃƒÂ©fixe data:)
 */
fileToBase64(file, onProgress = null) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        
        reader.onload = () => {
            try {
                // Extraire uniquement la partie base64 (supprimer "data:...;base64,")
                const result = reader.result;
                const base64 = result.split(',')[1];
                
                if (!base64) {
                    reject(new Error('Failed to extract base64 content'));
                    return;
                }
                
                if (onProgress) onProgress(100);
                resolve(base64);
                
            } catch (error) {
                reject(error);
            }
        };
        
        reader.onerror = () => {
            reject(new Error('Failed to read file: ' + reader.error));
        };
        
        reader.onprogress = (event) => {
            if (event.lengthComputable && onProgress) {
                const progress = (event.loaded / event.total) * 100;
                onProgress(progress);
            }
        };
        
        // Lire le fichier en Data URL (base64)
        reader.readAsDataURL(file);
    });
}

/**
 * Upload multiple fichiers
 * @param {FileList|Array<File>} files - Fichiers ÃƒÂ  uploader
 * @param {Function} onProgress - Callback global de progression
 * @returns {Promise<Array>} RÃƒÂ©sultats des uploads
 */
async uploadMultipleFiles(files, onProgress = null) {
    const results = [];
    const total = files.length;
    
    this.logger.info('FileService', `Uploading ${total} files...`);
    
    for (let i = 0; i < total; i++) {
        const file = files[i];
        
        try {
            const result = await this.uploadFile(file, (fileProgress) => {
                // Calculer progression globale
                const globalProgress = ((i / total) * 100) + (fileProgress / total);
                if (onProgress) onProgress(Math.floor(globalProgress));
            });
            
            results.push({
                file: file.name,
                success: true,
                data: result
            });
            
        } catch (error) {
            results.push({
                file: file.name,
                success: false,
                error: error.message
            });
            
            this.logger.error('FileService', `Failed to upload ${file.name}:`, error);
        }
    }
    
    const succeeded = results.filter(r => r.success).length;
    const failed = results.filter(r => !r.success).length;
    
    this.logger.info('FileService', 
        `Upload complete: ${succeeded} succeeded, ${failed} failed`);
    
    if (onProgress) onProgress(100);
    
    return results;
}




    // ========================================================================
    // DELETE FICHIER - Ã¢Å“â€¦ COMPLET
    // ========================================================================
    /**
 * Supprimer un fichier avec confirmation
 * @param {string} fileId - ID du fichier ÃƒÂ  supprimer
 * @param {boolean} skipConfirmation - Passer la confirmation
 * @returns {Promise<void>}
 */
async deleteFile(fileId, skipConfirmation = false) {
    this.logger.info('FileService', `Deleting file: ${fileId}`);
    
    // RÃƒÂ©cupÃƒÂ©rer info fichier
    const file = this.fileCache.get(fileId);
    if (!file) {
        throw new Error('File not found in cache');
    }
    
    // VÃƒÂ©rifier utilisation dans playlists (si model disponible)
    if (!skipConfirmation && window.playlistModel) {
        const playlists = window.playlistModel.get('playlists') || [];
        const usedIn = playlists.filter(p => 
            p.files && p.files.includes(fileId)
        );
        
        if (usedIn.length > 0) {
            const playlistNames = usedIn.map(p => p.name).join(', ');
            const confirmed = confirm(
                `File "${file.name}" is used in ${usedIn.length} playlist(s): ${playlistNames}\n\n` +
                `Delete anyway?`
            );
            
            if (!confirmed) {
                this.logger.info('FileService', 'Delete cancelled by user');
                return;
            }
        }
    }
    
    try {
        // Supprimer cÃƒÂ´tÃƒÂ© backend
        const result = await this.backend.sendCommand('files.delete', {
            file_path: fileId
        });
        
        if (result.success === false) {
            throw new Error(result.error || 'Delete failed');
        }
        
        // Mettre ÃƒÂ  jour cache et stats
        this.fileCache.delete(fileId);
        this.stats.deletesCount++;
        this.state.totalFiles--;
        if (file.size) {
            this.state.totalSize -= file.size;
        }
        
        // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
        this.eventBus.emit('file:deleted', { fileId, file });
        
        this.logger.info('FileService', `Ã¢Å“â€œ File deleted: ${file.name}`);
        
    } catch (error) {
        this.logger.error('FileService', 'Delete failed:', error);
        throw error;
    }
}

    // ========================================================================
    // MOVE FICHIER - Ã¢Å“â€¦ NOUVEAU
    // ========================================================================
    
    /**
     * DÃƒÂ©place un fichier vers un autre dossier
     * @param {string} fileId - ID du fichier
     * @param {string} newPath - Nouveau chemin
     */
    async moveFile(fileId, newPath) {
        const file = this.fileCache.get(fileId);
        
        if (!file) {
            throw new Error(`File not found: ${fileId}`);
        }
        
        this.logger.info('FileService', `Moving: ${file.name} Ã¢â€ â€™ ${newPath}`);
        
        this.eventBus.emit('files:move-start', {
            fileId: fileId,
            oldPath: file.path,
            newPath: newPath
        });
        
        try {
            const response = await this.backend.sendCommand('files.move', {
                file_id: fileId,
                old_path: file.path,
                new_path: newPath
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Move failed');
            }
            
            // Mettre ÃƒÂ  jour le cache
            file.path = newPath;
            file.directory = newPath.substring(0, newPath.lastIndexOf('/')) || '/';
            this._addToCache(file);
            
            this.logger.info('FileService', `Ã¢Å“â€œ Moved: ${file.name}`);
            
            this.eventBus.emit('files:move-complete', {
                fileId: fileId,
                oldPath: file.path,
                newPath: newPath,
                file: file
            });
            
            return file;
            
        } catch (error) {
            this.logger.error('FileService', `Move failed: ${file.name}`, error);
            
            this.eventBus.emit('files:move-error', {
                fileId: fileId,
                error: error.message
            });
            
            throw error;
        }
    }
    
    // ========================================================================
    // RENAME FICHIER - Ã¢Å“â€¦ NOUVEAU
    // ========================================================================


/**
 * Renommer un fichier
 * @param {string} fileId - ID du fichier
 * @param {string} newName - Nouveau nom
 * @returns {Promise<Object>} Fichier renommÃƒÂ©
 */
async renameFile(fileId, newName) {
    this.logger.info('FileService', `Renaming file: ${fileId} -> ${newName}`);
    
    // Validation nom
    if (!newName || newName.trim() === '') {
        throw new Error('New name cannot be empty');
    }
    
    // VÃƒÂ©rifier extension
    if (!newName.endsWith('.mid') && !newName.endsWith('.midi')) {
        newName += '.mid';
    }
    
    try {
        const result = await this.backend.sendCommand('files.rename', {
            file_path: fileId,
            new_name: newName
        });
        
        if (result.success === false) {
            throw new Error(result.error || 'Rename failed');
        }
        
        // Mettre ÃƒÂ  jour cache
        const file = this.fileCache.get(fileId);
        if (file) {
            this.fileCache.delete(fileId);
            
            const updatedFile = {
                ...file,
                id: result.data.new_path,
                name: newName,
                path: result.data.new_path
            };
            
            this.fileCache.set(updatedFile.id, updatedFile);
            
            // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
            this.eventBus.emit('file:renamed', {
                oldId: fileId,
                newId: updatedFile.id,
                file: updatedFile
            });
            
            return updatedFile;
        }
        
        return result.data;
        
    } catch (error) {
        this.logger.error('FileService', 'Rename failed:', error);
        throw error;
    }
}
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    _validateFile(file) {
        // VÃƒÂ©rifier existence
        if (!file) {
            return { valid: false, error: 'No file provided' };
        }
        
        // VÃƒÂ©rifier taille
        if (file.size === 0) {
            return { valid: false, error: 'File is empty' };
        }
        
        if (file.size > this.config.maxFileSize) {
            return { 
                valid: false, 
                error: `File too large (max ${this.config.maxFileSize / 1024 / 1024}MB)` 
            };
        }
        
        // VÃƒÂ©rifier extension
        const extension = '.' + file.name.split('.').pop().toLowerCase();
        if (!this.config.supportedExtensions.includes(extension)) {
            return { 
                valid: false, 
                error: `Unsupported file type (${extension})` 
            };
        }
        
        // VÃƒÂ©rifier MIME type
        if (file.type && !this.config.allowedMimeTypes.includes(file.type)) {
            this.logger.warn('FileService', `Unusual MIME type: ${file.type}`);
            // Ne pas bloquer, juste avertir
        }
        
        return { valid: true };
    }
    
    _isValidFileName(name) {
        if (!name || name.trim() === '') return false;
        
        // Interdire certains caractÃƒÂ¨res
        const invalidChars = /[<>:"/\\|?*\x00-\x1F]/g;
        if (invalidChars.test(name)) return false;
        
        // Interdire ".." pour ÃƒÂ©viter path traversal
        if (name.includes('..')) return false;
        
        return true;
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    _readFileAsBase64(file, onProgress = null) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            
            reader.onprogress = (e) => {
                if (e.lengthComputable && onProgress) {
                    const progress = (e.loaded / e.total) * 100;
                    onProgress(progress);
                }
            };
            
            reader.onload = () => {
                const base64 = reader.result.split(',')[1];
                resolve(base64);
            };
            
            reader.onerror = () => {
                reject(new Error('Failed to read file'));
            };
            
            reader.readAsDataURL(file);
        });
    }
    
    _normalizeFileData(file) {
        return {
            id: file.id || this._generateFileId(file),
            name: file.name || 'Unknown',
            path: file.path || '',
            directory: file.directory || '/',
            size: file.size || 0,
            duration: file.duration || 0,
            trackCount: file.trackCount || file.tracks || 0,
            format: file.format || 1,
            resolution: file.resolution || 480,
            tempo: file.tempo || 120,
            timeSignature: file.timeSignature || '4/4',
            createdAt: file.createdAt || Date.now(),
            modifiedAt: file.modifiedAt || Date.now(),
            metadata: file.metadata || {}
        };
    }
    
    _generateFileId(file) {
        const path = file.path || file.name;
        return 'file_' + btoa(path).replace(/[^a-zA-Z0-9]/g, '');
    }
    
    _addToCache(file) {
        this.fileCache.set(file.id, file);
        this._updateSearchIndex(file);
    }
    
    _removeFromCache(fileId) {
        const file = this.fileCache.get(fileId);
        
        if (file) {
            this.fileCache.delete(fileId);
            this._removeFromSearchIndex(file);
        }
    }
    
    _updateSearchIndex(file) {
        const terms = this._extractSearchTerms(file);
        
        for (const term of terms) {
            if (!this.searchIndex.has(term)) {
                this.searchIndex.set(term, new Set());
            }
            this.searchIndex.get(term).add(file.id);
        }
    }
    
    _removeFromSearchIndex(file) {
        const terms = this._extractSearchTerms(file);
        
        for (const term of terms) {
            const set = this.searchIndex.get(term);
            if (set) {
                set.delete(file.id);
                if (set.size === 0) {
                    this.searchIndex.delete(term);
                }
            }
        }
    }
    
    _extractSearchTerms(file) {
        const terms = [];
        
        // Nom de fichier
        if (file.name) {
            terms.push(...file.name.toLowerCase().split(/\s+/));
        }
        
        // Chemin
        if (file.directory) {
            terms.push(...file.directory.toLowerCase().split('/'));
        }
        
        return terms.filter(t => t.length > 2);
    }
    
    _calculateTotalSize() {
        let total = 0;
        for (const file of this.fileCache.values()) {
            total += file.size || 0;
        }
        return total;
    }
    
    _getFilesFromCache() {
        return Array.from(this.fileCache.values());
    }
    
    getAllFiles() {
        return this._getFilesFromCache();
    }
    
    getFile(fileId) {
        return this.fileCache.get(fileId);
    }
    
    searchFiles(query) {
        if (!query || query.trim() === '') {
            return this.getAllFiles();
        }
        
        const terms = query.toLowerCase().split(/\s+/);
        const matchingIds = new Set();
        
        for (const term of terms) {
            const ids = this.searchIndex.get(term);
            if (ids) {
                for (const id of ids) {
                    matchingIds.add(id);
                }
            }
        }
        
        return Array.from(matchingIds).map(id => this.fileCache.get(id)).filter(Boolean);
    }
    
    getStats() {
        return {
            ...this.stats,
            totalFiles: this.state.totalFiles,
            totalSize: this.state.totalSize,
            cacheSize: this.fileCache.size,
            uploadSuccessRate: this.stats.uploadsCount > 0 
                ? (this.stats.uploadsSucceeded / this.stats.uploadsCount) * 100 
                : 0
        };
    }
    
    // ========================================================================
    // HANDLERS Ãƒâ€°VÃƒâ€°NEMENTS BACKEND
    // ========================================================================
    
    _handleFilesList(data) {
        if (data.files && Array.isArray(data.files)) {
            this.fileCache.clear();
            
            for (const file of data.files) {
                const normalized = this._normalizeFileData(file);
                this._addToCache(normalized);
            }
            
            this.state.totalFiles = this.fileCache.size;
            this.state.totalSize = this._calculateTotalSize();
            
            this.stats.filesLoaded += data.files.length;
        }
    }
    
    _handleFileAdded(data) {
        if (data.file) {
            const file = this._normalizeFileData(data.file);
            this._addToCache(file);
            
            this.state.totalFiles = this.fileCache.size;
            this.state.totalSize = this._calculateTotalSize();
            
            this.eventBus.emit('files:file-added', { file });
            this.logger.info('FileService', `Ã¢Å¾â€¢ File added: ${file.name}`);
        }
    }
    
    _handleFileRemoved(data) {
        if (data.fileId || data.path) {
            const id = data.fileId || this._generateFileId({ path: data.path });
            const file = this.fileCache.get(id);
            
            if (file) {
                this._removeFromCache(id);
                
                this.state.totalFiles = this.fileCache.size;
                this.state.totalSize = this._calculateTotalSize();
                
                this.eventBus.emit('files:file-removed', { file });
                this.logger.info('FileService', `Ã¢Å¾â€“ File removed: ${file.name}`);
            }
        }
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
window.FileService = FileService;
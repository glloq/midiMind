// ============================================================================
// Fichier: frontend/js/models/FileModel.js
// Chemin réel: frontend/js/models/FileModel.js
// Version: v3.3.0 - CONFORMITÉ API BACKEND
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.3.0:
// ✅ CRITIQUE: Commandes API conformes à API_DOCUMENTATION_FRONTEND.md
// ✅ list_files (au lieu de files.list)
// ✅ load_file (au lieu de files.read)
// ✅ delete_file (au lieu de files.delete)
// ✅ get_file_info pour obtenir les métadonnées
// ✅ import_file, export_file pour gestion fichiers
// ============================================================================

class FileModel extends BaseModel {
    constructor(eventBus, backend, logger, initialData = {}, options = {}) {
        super(eventBus, backend, logger, {
            files: [],
            currentPath: '/midi',
            selectedFile: null,
            recentFiles: [],
            ...initialData
        }, {
            persistKey: 'filemodel',
            eventPrefix: 'file',
            autoPersist: true,
            ...options
        });
        
        if (!this.data) {
            this.data = {};
        }
        this.data.files = this.data.files || [];
        this.data.currentPath = this.data.currentPath || '/midi';
        this.data.selectedFile = this.data.selectedFile || null;
        this.data.recentFiles = this.data.recentFiles || [];
        
        this.log('debug', 'FileModel', '✓ FileModel v3.3.0 initialized (API compliant)');
    }
    
    // ========================================================================
    // GESTION FICHIERS - BASE
    // ========================================================================
    
    /**
     * Rafraîchit la liste des fichiers
     * ✅ API: list_files
     */
    async refreshFileList(path = null) {
        const targetPath = path || this.getCurrentPath();
        
        if (!this.backend) {
            const error = 'Backend service not available';
            this.log('error', 'FileModel', error);
            throw new Error(error);
        }
        
        try {
            this.log('info', 'FileModel', `Refreshing file list: ${targetPath}`);
            
            // ✅ CONFORME: list_files
            const response = await this.backend.sendCommand('list_files', {
                path: targetPath
            });
            
            const files = response.files || [];
            
            this.set('files', files);
            this.set('currentPath', targetPath);
            
            if (this.eventBus) {
                this.eventBus.emit('file:list-refreshed', {
                    files,
                    path: targetPath
                });
            }
            
            return files;
            
        } catch (error) {
            this.log('error', 'FileModel', `Refresh failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Charge un fichier MIDI
     * ✅ API: load_file
     */
    async loadFile(filePath) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Loading file: ${filePath}`);
            
            // ✅ CONFORME: load_file avec file_path
            const response = await this.backend.sendCommand('load_file', {
                file_path: filePath
            });
            
            this.set('selectedFile', response);
            this.addToRecent(filePath);
            
            if (this.eventBus) {
                this.eventBus.emit('file:loaded', {
                    filePath,
                    data: response
                });
            }
            
            return response;
            
        } catch (error) {
            this.log('error', 'FileModel', `Load failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Décharge le fichier actuel
     * ✅ API: unload_file
     */
    async unloadFile() {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', 'Unloading current file');
            
            // ✅ CONFORME: unload_file
            await this.backend.sendCommand('unload_file');
            
            this.set('selectedFile', null);
            
            if (this.eventBus) {
                this.eventBus.emit('file:unloaded', {
                    timestamp: Date.now()
                });
            }
            
            return true;
            
        } catch (error) {
            this.log('error', 'FileModel', `Unload failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Obtient les informations d'un fichier
     * ✅ API: get_file_info
     */
    async getFileInfo(filePath) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Getting file info: ${filePath}`);
            
            // ✅ CONFORME: get_file_info
            const response = await this.backend.sendCommand('get_file_info', {
                file_path: filePath
            });
            
            return response;
            
        } catch (error) {
            this.log('error', 'FileModel', `Get file info failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Supprime un fichier
     * ✅ API: delete_file
     */
    async deleteFile(filePath) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Deleting file: ${filePath}`);
            
            // ✅ CONFORME: delete_file
            await this.backend.sendCommand('delete_file', {
                file_path: filePath
            });
            
            const files = this.get('files');
            const updatedFiles = files.filter(f => f.path !== filePath);
            this.set('files', updatedFiles);
            
            this.removeFromRecent(filePath);
            
            if (this.eventBus) {
                this.eventBus.emit('file:deleted', {
                    filePath,
                    timestamp: Date.now()
                });
            }
            
            return true;
            
        } catch (error) {
            this.log('error', 'FileModel', `Delete failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Importe un fichier
     * ✅ API: import_file
     */
    async importFile(fileName, fileData, size, type = 'audio/midi') {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Importing file: ${fileName}`);
            
            // ✅ CONFORME: import_file
            const response = await this.backend.sendCommand('import_file', {
                file_name: fileName,
                file_data: fileData,
                size: size,
                type: type
            });
            
            await this.refreshFileList();
            
            if (this.eventBus) {
                this.eventBus.emit('file:imported', {
                    fileName,
                    timestamp: Date.now()
                });
            }
            
            return response;
            
        } catch (error) {
            this.log('error', 'FileModel', `Import failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Exporte un fichier
     * ✅ API: export_file
     */
    async exportFile(filePath, format = 'midi') {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Exporting file: ${filePath}`);
            
            // ✅ CONFORME: export_file
            const response = await this.backend.sendCommand('export_file', {
                file_path: filePath,
                format: format
            });
            
            if (this.eventBus) {
                this.eventBus.emit('file:exported', {
                    filePath,
                    format,
                    timestamp: Date.now()
                });
            }
            
            return response;
            
        } catch (error) {
            this.log('error', 'FileModel', `Export failed: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // GESTION RÉCENTS
    // ========================================================================
    
    addToRecent(filePath) {
        const recents = this.get('recentFiles') || [];
        const filtered = recents.filter(f => f !== filePath);
        filtered.unshift(filePath);
        const limited = filtered.slice(0, 10);
        this.set('recentFiles', limited);
        this.log('debug', 'FileModel', `Added to recent: ${filePath}`);
    }
    
    removeFromRecent(filePath) {
        const recents = this.get('recentFiles') || [];
        const filtered = recents.filter(f => f !== filePath);
        this.set('recentFiles', filtered);
        this.log('debug', 'FileModel', `Removed from recent: ${filePath}`);
    }
    
    clearRecent() {
        this.set('recentFiles', []);
        this.log('info', 'FileModel', 'Recent files cleared');
    }
    
    // ========================================================================
    // ACCESSEURS
    // ========================================================================
    
    getAll() {
        return this.data.files || [];
    }
    
    getRecentFiles() {
        return this.data.recentFiles || [];
    }
    
    getCurrentPath() {
        return this.data.currentPath || '/midi';
    }
    
    getSelectedFile() {
        return this.data.selectedFile;
    }
    
    setSelectedFile(file) {
        this.set('selectedFile', file);
        if (file && file.path) {
            this.addToRecent(file.path);
        }
    }
    
    // ========================================================================
    // RECHERCHE & FILTRAGE
    // ========================================================================
    
    searchFiles(query) {
        const files = this.getAll();
        if (!query || query.trim() === '') {
            return files;
        }
        
        const lowerQuery = query.toLowerCase();
        return files.filter(file => {
            return (
                file.name?.toLowerCase().includes(lowerQuery) ||
                file.path?.toLowerCase().includes(lowerQuery) ||
                file.artist?.toLowerCase().includes(lowerQuery) ||
                file.title?.toLowerCase().includes(lowerQuery)
            );
        });
    }
    
    filterByType(type) {
        const files = this.getAll();
        return files.filter(file => {
            return file.type === type || file.extension === type;
        });
    }
    
    sortFiles(sortBy = 'name', order = 'asc') {
        const files = [...this.getAll()];
        
        files.sort((a, b) => {
            let valA = a[sortBy];
            let valB = b[sortBy];
            
            if (typeof valA === 'string') {
                valA = valA.toLowerCase();
            }
            if (typeof valB === 'string') {
                valB = valB.toLowerCase();
            }
            
            if (order === 'asc') {
                return valA > valB ? 1 : valA < valB ? -1 : 0;
            } else {
                return valA < valB ? 1 : valA > valB ? -1 : 0;
            }
        });
        
        return files;
    }
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    validateFileName(fileName) {
        if (!fileName || typeof fileName !== 'string') {
            return { valid: false, error: 'Nom de fichier invalide' };
        }
        
        if (fileName.length > 255) {
            return { valid: false, error: 'Nom trop long (max 255 caractères)' };
        }
        
        const invalidChars = /[<>:"/\\|?*]/;
        if (invalidChars.test(fileName)) {
            return { valid: false, error: 'Caractères non autorisés dans le nom' };
        }
        
        return { valid: true };
    }
    
    validateFilePath(filePath) {
        if (!filePath || typeof filePath !== 'string') {
            return { valid: false, error: 'Chemin invalide' };
        }
        
        if (!filePath.startsWith('/')) {
            return { valid: false, error: 'Le chemin doit commencer par /' };
        }
        
        return { valid: true };
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
// ============================================================================
// Fichier: frontend/js/models/FileModel.js
// Chemin rÃ©el: frontend/js/models/FileModel.js
// Version: v4.0.0 - API COMPATIBLE v4.2.2
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v4.0.0:
// âœ… list_files â†’ files.list
// âœ… load_file â†’ playback.load
// âœ… unload_file â†’ playback.stop
// âœ… get_file_info â†’ files.getInfo
// âœ… delete_file â†’ files.delete
// âœ… import_file â†’ files.write
// âœ… export_file â†’ files.read
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
        
        this.log('debug', 'FileModel', 'âœ“ FileModel v4.0.0 initialized (API v4.2.2)');
    }
    
    // ========================================================================
    // GESTION FICHIERS - BASE
    // ========================================================================
    
    /**
     * RafraÃ®chit la liste des fichiers
     * âœ… API v4.0.0: files.list
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
            
            // âœ… Nouvelle commande API v4.0.0
            const response = await this.backend.sendCommand('files.list', {
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
     * Charge un fichier MIDI pour lecture
     * âœ… API v4.0.0: playback.load
     */
    async loadFile(filePath) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Loading file: ${filePath}`);
            
            // âœ… Nouvelle commande API v4.0.0 - charge pour lecture
            const response = await this.backend.sendCommand('playback.load', {
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
     * Lit le contenu d'un fichier
     * âœ… API v4.0.0: files.read
     */
    async readFile(filePath) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Reading file: ${filePath}`);
            
            // âœ… Nouvelle commande API v4.0.0
            const response = await this.backend.sendCommand('files.read', {
                path: filePath
            });
            
            return response;
            
        } catch (error) {
            this.log('error', 'FileModel', `Read failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * DÃ©charge le fichier actuel
     * âœ… API v4.0.0: playback.stop
     */
    async unloadFile() {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', 'Unloading current file');
            
            // âœ… Nouvelle commande API v4.0.0
            await this.backend.sendCommand('playback.stop');
            
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
     * âœ… API v4.0.0: files.getInfo
     */
    async getFileInfo(filePath) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Getting file info: ${filePath}`);
            
            // âœ… Nouvelle commande API v4.0.0
            const response = await this.backend.sendCommand('files.getInfo', {
                path: filePath
            });
            
            return response;
            
        } catch (error) {
            this.log('error', 'FileModel', `Get file info failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Supprime un fichier
     * âœ… API v4.0.0: files.delete
     */
    async deleteFile(filePath) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Deleting file: ${filePath}`);
            
            // âœ… Nouvelle commande API v4.0.0
            await this.backend.sendCommand('files.delete', {
                path: filePath
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
     * Importe/Upload un fichier
     * âœ… API v4.0.0: files.write
     */
    async importFile(fileName, fileData, size, type = 'audio/midi') {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Importing file: ${fileName}`);
            
            // âœ… Nouvelle commande API v4.0.0
            const response = await this.backend.sendCommand('files.write', {
                path: `/midi/${fileName}`,
                content: fileData,
                encoding: 'base64'
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
     * Exporte/TÃ©lÃ©charge un fichier
     * âœ… API v4.0.0: files.read
     */
    async exportFile(filePath, format = 'midi') {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            this.log('info', 'FileModel', `Exporting file: ${filePath}`);
            
            // âœ… Nouvelle commande API v4.0.0
            const response = await this.backend.sendCommand('files.read', {
                path: filePath
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
    // GESTION FICHIERS RÃƒâ€°CENTS
    // ========================================================================
    
    /**
     * Ajoute un fichier aux rÃƒÂ©cents
     */
    addToRecent(filePath) {
        let recentFiles = this.get('recentFiles');
        
        // Retirer le fichier s'il existe dÃ©jÃ 
        recentFiles = recentFiles.filter(f => f !== filePath);
        
        // Ajouter en tÃªte
        recentFiles.unshift(filePath);
        
        // Limiter Ãƒ  10 fichiers
        if (recentFiles.length > 10) {
            recentFiles = recentFiles.slice(0, 10);
        }
        
        this.set('recentFiles', recentFiles);
    }
    
    /**
     * Retire un fichier des rÃ©cents
     */
    removeFromRecent(filePath) {
        let recentFiles = this.get('recentFiles');
        recentFiles = recentFiles.filter(f => f !== filePath);
        this.set('recentFiles', recentFiles);
    }
    
    /**
     * Obtient les fichiers rÃ©cents
     */
    getRecentFiles() {
        return this.get('recentFiles');
    }
    
    /**
     * Efface les fichiers rÃ©cents
     */
    clearRecentFiles() {
        this.set('recentFiles', []);
    }
    
    // ========================================================================
    // GETTERS / SETTERS
    // ========================================================================
    
    /**
     * Obtient la liste des fichiers
     */
    getFiles() {
        return this.get('files');
    }
    
    /**
     * Obtient le chemin actuel
     */
    getCurrentPath() {
        return this.get('currentPath');
    }
    
    /**
     * DÃƒÂ©finit le chemin actuel
     */
    setCurrentPath(path) {
        this.set('currentPath', path);
    }
    
    /**
     * Obtient le fichier sÃƒÂ©lectionnÃƒÂ©
     */
    getSelectedFile() {
        return this.get('selectedFile');
    }
    
    /**
     * DÃƒÂ©finit le fichier sÃƒÂ©lectionnÃƒÂ©
     */
    setSelectedFile(file) {
        this.set('selectedFile', file);
    }
    
    /**
     * Recherche des fichiers par nom
     */
    searchFiles(query) {
        const files = this.getFiles();
        const lowerQuery = query.toLowerCase();
        
        return files.filter(file => 
            file.name.toLowerCase().includes(lowerQuery) ||
            (file.path && file.path.toLowerCase().includes(lowerQuery))
        );
    }
    
    /**
     * Filtre les fichiers par extension
     */
    filterByExtension(extension) {
        const files = this.getFiles();
        return files.filter(file => 
            file.name.toLowerCase().endsWith(extension.toLowerCase())
        );
    }
    
    /**
     * Trie les fichiers
     */
    sortFiles(sortBy = 'name', order = 'asc') {
        const files = [...this.getFiles()];
        
        files.sort((a, b) => {
            let valueA = a[sortBy];
            let valueB = b[sortBy];
            
            if (typeof valueA === 'string') {
                valueA = valueA.toLowerCase();
                valueB = valueB.toLowerCase();
            }
            
            if (order === 'asc') {
                return valueA > valueB ? 1 : -1;
            } else {
                return valueA < valueB ? 1 : -1;
            }
        });
        
        this.set('files', files);
        return files;
    }
}

// ============================================================================
// EXPORT GLOBAL
// ============================================================================
if (typeof window !== 'undefined') {
    window.FileModel = FileModel;
}
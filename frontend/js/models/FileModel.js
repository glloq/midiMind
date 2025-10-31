// ============================================================================
// Fichier: frontend/js/models/FileModel.js
// Version: v3.1.09 - MÉTHODES ROBUSTES
// Date: 2025-10-31
// ============================================================================
// CORRECTIONS v3.1.09:
// ✅ Méthodes getAll, getRecentFiles garanties fonctionnelles
// ✅ Accès direct à this.data pour éviter problèmes BaseModel.get()
// ✅ Compatibilité totale avec NavigationController
// ============================================================================

class FileModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // Appel super() avec BaseModel(initialData, options)
        super({
            files: [],
            currentPath: '/midi',
            selectedFile: null,
            recentFiles: []
        }, {
            persistKey: 'filemodel',
            eventPrefix: 'file',
            autoPersist: true
        });
        
        // Assigner dépendances
        this.eventBus = eventBus || window.EventBus || window.eventBus;
        this.backend = backend || window.backendService || window.app?.services?.backend;
        this.logger = logger || window.logger || console;
        
        // Validation
        if (!this.eventBus) {
            console.error('[FileModel] EventBus not available!');
        }
        if (!this.backend) {
            console.warn('[FileModel] BackendService not available - file operations will fail');
        }
        
        // ✅ CRITIQUE: S'assurer que this.data existe avec valeurs par défaut
        if (!this.data) {
            this.data = {};
        }
        this.data.files = this.data.files || [];
        this.data.currentPath = this.data.currentPath || '/midi';
        this.data.selectedFile = this.data.selectedFile || null;
        this.data.recentFiles = this.data.recentFiles || [];
        
        if (this.logger && typeof this.logger.info === 'function') {
            this.logger.info('FileModel', '✓ FileModel v3.1.09 initialized');
        }
    }
    
    // ========================================================================
    // GESTION FICHIERS - BASE
    // ========================================================================
    
    async refreshFileList(path = null) {
        const targetPath = path || this.getCurrentPath();
        
        if (!this.backend) {
            const error = 'Backend service not available';
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('FileModel', error);
            }
            throw new Error(error);
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('FileModel', `Refreshing file list: ${targetPath}`);
            }
            
            const response = await this.backend.sendCommand('files.list', {
                path: targetPath
            });
            
            if (response.success) {
                const files = response.data.files || [];
                
                this.set('files', files);
                this.set('currentPath', targetPath);
                
                if (this.eventBus) {
                    this.eventBus.emit('file:list-refreshed', {
                        files,
                        path: targetPath
                    });
                }
                
                return files;
            }
            
            throw new Error(response.error || 'Failed to refresh file list');
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('FileModel', `Refresh failed: ${error.message}`);
            }
            throw error;
        }
    }
    
    async loadFile(fileId) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('FileModel', `Loading file: ${fileId}`);
            }
            
            const response = await this.backend.sendCommand('files.read', {
                filename: fileId
            });
            
            if (response.success) {
                const fileData = response.data;
                
                this.set('selectedFile', fileData);
                this.addToRecent(fileId);
                
                if (this.eventBus) {
                    this.eventBus.emit('file:loaded', {
                        fileId,
                        data: fileData
                    });
                }
                
                return fileData;
            }
            
            throw new Error(response.error || 'Failed to load file');
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('FileModel', `Load failed: ${error.message}`);
            }
            throw error;
        }
    }
    
    async saveFile(fileId, midiData) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('FileModel', `Saving file: ${fileId}`);
            }
            
            const response = await this.backend.sendCommand('files.write', {
                filename: fileId,
                content: midiData
            });
            
            if (response.success) {
                if (this.eventBus) {
                    this.eventBus.emit('file:saved', {
                        fileId,
                        timestamp: Date.now()
                    });
                }
                
                return true;
            }
            
            throw new Error(response.error || 'Failed to save file');
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('FileModel', `Save failed: ${error.message}`);
            }
            throw error;
        }
    }
    
    async deleteFile(fileId) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('FileModel', `Deleting file: ${fileId}`);
            }
            
            const response = await this.backend.sendCommand('files.delete', {
                filename: fileId
            });
            
            if (response.success) {
                const files = this.getAll();
                this.set('files', files.filter(f => f.id !== fileId));
                this.removeFromRecent(fileId);
                
                if (this.eventBus) {
                    this.eventBus.emit('file:deleted', {
                        fileId,
                        timestamp: Date.now()
                    });
                }
                
                return true;
            }
            
            throw new Error(response.error || 'Failed to delete file');
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('FileModel', `Delete failed: ${error.message}`);
            }
            throw error;
        }
    }
    
    async renameFile(fileId, newName) {
        // ⚠️ WARNING: files.rename does not exist in backend API v4.2.1
        // Workaround: read + write + delete
        throw new Error('Rename operation not supported by backend API');
        
        /* Original code disabled:
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('FileModel', `Renaming file: ${fileId} to ${newName}`);
            }
            
            const response = await this.backend.sendCommand('files.rename', {
                file_id: fileId,
                new_name: newName
            });
            
            if (response.success) {
                const files = this.getAll();
                const fileIndex = files.findIndex(f => f.id === fileId);
                if (fileIndex !== -1) {
                    files[fileIndex].name = newName;
                    this.set('files', [...files]);
                }
                
                if (this.eventBus) {
                    this.eventBus.emit('file:renamed', {
                        fileId,
                        newName,
                        timestamp: Date.now()
                    });
                }
                
                return true;
            }
            
            throw new Error(response.error || 'Failed to rename file');
            
        } catch (error) {
            if (this.logger && typeof this.logger.error === 'function') {
                this.logger.error('FileModel', `Rename failed: ${error.message}`);
            }
            throw error;
        }
        */
    }
    
    // ========================================================================
    // GESTION FICHIERS RÉCENTS
    // ========================================================================
    
    addToRecent(fileId) {
        let recent = this.getRecentFiles();
        
        // Retirer si déjà présent
        recent = recent.filter(id => id !== fileId);
        
        // Ajouter en premier
        recent.unshift(fileId);
        
        // Limiter à 10
        if (recent.length > 10) {
            recent = recent.slice(0, 10);
        }
        
        this.set('recentFiles', recent);
    }
    
    removeFromRecent(fileId) {
        let recent = this.getRecentFiles();
        recent = recent.filter(id => id !== fileId);
        this.set('recentFiles', recent);
    }
    
    clearRecent() {
        this.set('recentFiles', []);
    }
    
    getRecentFiles() {
        // ✅ ACCÈS DIRECT pour garantir fonctionnement
        if (this.data && Array.isArray(this.data.recentFiles)) {
            return this.data.recentFiles;
        }
        // Fallback via get()
        const recent = this.get('recentFiles');
        return Array.isArray(recent) ? recent : [];
    }
    
    // ========================================================================
    // GETTERS / HELPERS
    // ========================================================================
    
    getAll() {
        // ✅ ACCÈS DIRECT pour garantir fonctionnement
        if (this.data && Array.isArray(this.data.files)) {
            return this.data.files;
        }
        // Fallback via get()
        const files = this.get('files');
        return Array.isArray(files) ? files : [];
    }
    
    getById(fileId) {
        const files = this.getAll();
        return files.find(f => f.id === fileId);
    }
    
    getSelected() {
        return this.get('selectedFile');
    }
    
    setSelected(file) {
        this.set('selectedFile', file);
        
        if (file && this.eventBus) {
            this.eventBus.emit('file:selected', {
                file,
                timestamp: Date.now()
            });
        }
    }
    
    getCurrentPath() {
        // ✅ ACCÈS DIRECT pour garantir fonctionnement
        if (this.data && this.data.currentPath) {
            return this.data.currentPath;
        }
        // Fallback via get()
        return this.get('currentPath') || '/midi';
    }
    
    setCurrentPath(path) {
        this.set('currentPath', path);
    }
    
    filterByName(query) {
        const files = this.getAll();
        const lowerQuery = query.toLowerCase();
        return files.filter(f => 
            f.name.toLowerCase().includes(lowerQuery)
        );
    }
    
    sortFiles(sortBy = 'name', order = 'asc') {
        const files = this.getAll();
        const sorted = [...files].sort((a, b) => {
            let comparison = 0;
            
            switch (sortBy) {
                case 'name':
                    comparison = a.name.localeCompare(b.name);
                    break;
                case 'size':
                    comparison = (a.size || 0) - (b.size || 0);
                    break;
                case 'date':
                    comparison = (a.modified || 0) - (b.modified || 0);
                    break;
                default:
                    comparison = 0;
            }
            
            return order === 'desc' ? -comparison : comparison;
        });
        
        this.set('files', sorted);
        return sorted;
    }
}

// ============================================================================
// EXPORT GLOBAL
// ============================================================================
if (typeof window !== 'undefined') {
    window.FileModel = FileModel;
}
// ============================================================================
// Fichier: frontend/js/models/FileModel.js
// Version: v3.1.08 - COHERENCE MAXIMALE
// Date: 2025-10-29
// ============================================================================
// CORRECTIONS v3.1.08:
// ✅ CRITIQUE: Cohérence totale avec BaseModel(initialData, options)
// ✅ CRITIQUE: Logger utilise window.logger (instance) pas window.logger (classe)
// ✅ CRITIQUE: EventBus et backend acceptés en paramètres OU depuis window
// ✅ Protection contre paramètres null/undefined
// ✅ Toutes les méthodes dans la classe
// ============================================================================

class FileModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // ✅ Appel super() CORRECT avec BaseModel(initialData, options)
        super({}, {
            persistKey: 'filemodel',
            eventPrefix: 'file',
            autoPersist: true
        });
        
        // ✅ CRITIQUE: Assigner IMMÉDIATEMENT après super()
        // Accepter paramètres OU utiliser globaux
        this.eventBus = eventBus || window.EventBus || window.eventBus;
        this.backend = backend || window.backendService || window.app?.services?.backend;
        this.logger = logger || window.logger || console;
        
        // ✅ Validation des dépendances critiques
        if (!this.eventBus) {
            console.error('[FileModel] EventBus not available!');
        }
        if (!this.backend) {
            console.warn('[FileModel] BackendService not available - file operations will fail');
        }
        
        // ✅ Initialiser data directement
        this.data = {
            files: [],
            currentPath: '/midi',
            selectedFile: null,
            recentFiles: []
        };
        
        if (this.logger && typeof this.logger.info === 'function') {
            this.logger.info('FileModel', '✓ FileModel v3.1.08 initialized');
        }
    }
    
    // ========================================================================
    // GESTION FICHIERS - BASE
    // ========================================================================
    
    /**
     * Récupère la liste des fichiers
     */
    async refreshFileList(path = null) {
        const targetPath = path || this.get('currentPath');
        
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
    
    /**
     * Charge un fichier MIDI
     */
    async loadFile(fileId) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('FileModel', `Loading file: ${fileId}`);
            }
            
            const response = await this.backend.sendCommand('files.load', {
                file_id: fileId
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
    
    /**
     * Sauvegarde un fichier MIDI
     */
    async saveFile(fileId, midiData) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('FileModel', `Saving file: ${fileId}`);
            }
            
            const response = await this.backend.sendCommand('files.save', {
                file_id: fileId,
                midi_data: midiData
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
    
    /**
     * Supprime un fichier
     */
    async deleteFile(fileId) {
        if (!this.backend) {
            throw new Error('Backend service not available');
        }
        
        try {
            if (this.logger && typeof this.logger.info === 'function') {
                this.logger.info('FileModel', `Deleting file: ${fileId}`);
            }
            
            const response = await this.backend.sendCommand('files.delete', {
                file_id: fileId
            });
            
            if (response.success) {
                // Retirer des fichiers et des récents
                const files = this.get('files') || [];
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
    
    /**
     * Renomme un fichier
     */
    async renameFile(fileId, newName) {
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
                // Mettre à jour dans la liste
                const files = this.get('files') || [];
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
    }
    
    // ========================================================================
    // GESTION FICHIERS RÉCENTS
    // ========================================================================
    
    /**
     * Ajoute un fichier aux récents
     */
    addToRecent(fileId) {
        let recent = this.get('recentFiles') || [];
        
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
    
    /**
     * Retire un fichier des récents
     */
    removeFromRecent(fileId) {
        let recent = this.get('recentFiles') || [];
        recent = recent.filter(id => id !== fileId);
        this.set('recentFiles', recent);
    }
    
    /**
     * Vide les fichiers récents
     */
    clearRecent() {
        this.set('recentFiles', []);
    }
    
    /**
     * Obtient les fichiers récents
     */
    getRecentFiles() {
        return this.get('recentFiles') || [];
    }
    
    // ========================================================================
    // GETTERS / HELPERS
    // ========================================================================
    
    /**
     * Obtient la liste des fichiers
     */
    getAll() {
        return this.get('files') || [];
    }
    
    /**
     * Obtient un fichier par ID
     */
    getById(fileId) {
        const files = this.get('files') || [];
        return files.find(f => f.id === fileId);
    }
    
    /**
     * Obtient le fichier sélectionné
     */
    getSelected() {
        return this.get('selectedFile');
    }
    
    /**
     * Définit le fichier sélectionné
     */
    setSelected(file) {
        this.set('selectedFile', file);
        
        if (file && this.eventBus) {
            this.eventBus.emit('file:selected', {
                file,
                timestamp: Date.now()
            });
        }
    }
    
    /**
     * Obtient le chemin actuel
     */
    getCurrentPath() {
        return this.get('currentPath') || '/midi';
    }
    
    /**
     * Définit le chemin actuel
     */
    setCurrentPath(path) {
        this.set('currentPath', path);
    }
    
    /**
     * Filtre les fichiers par nom
     */
    filterByName(query) {
        const files = this.get('files') || [];
        const lowerQuery = query.toLowerCase();
        return files.filter(f => 
            f.name.toLowerCase().includes(lowerQuery)
        );
    }
    
    /**
     * Trie les fichiers
     */
    sortFiles(sortBy = 'name', order = 'asc') {
        const files = this.get('files') || [];
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
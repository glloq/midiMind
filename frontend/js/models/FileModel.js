// ============================================================================
// Fichier: frontend/js/models/FileModel.js
// Version: v3.0.6 - MINIMAL (Constructor fixed + basic functions only)
// Date: 2025-10-19
// ============================================================================
// SIMPLIFICATION: Seulement les fonctions de base
// - Lister fichiers
// - Charger un fichier
// - Supprimer un fichier
// - Pas de cache complexe
// - Pas de favoris/tags
// ============================================================================

class FileModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // ✅ FIX: Correct super() call
        super({}, {
            persistKey: 'filemodel',
            eventPrefix: 'file',
            autoPersist: true
        });
        
        // ✅ FIX: Assign immediately
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // ✅ FIX: Initialize data directly
        this.data = {
            files: [],
            currentPath: '/midi',
            selectedFile: null,
            recentFiles: []
        };
        
        this.logger.info('FileModel', '✓ Model initialized (minimal version)');
    }
    
    // ========================================================================
    // GESTION FICHIERS - BASE
    // ========================================================================
    
    /**
     * Récupère la liste des fichiers
     */
    async refreshFileList(path = null) {
        const targetPath = path || this.get('currentPath');
        
        try {
            this.logger.info('FileModel', `Refreshing file list: ${targetPath}`);
            
            const response = await this.backend.sendCommand('files.list', {
                path: targetPath
            });
            
            if (response.success) {
                const files = response.data.files || [];
                
                this.set('files', files);
                this.set('currentPath', targetPath);
                
                this.eventBus.emit('file:list-refreshed', {
                    files,
                    path: targetPath
                });
                
                return files;
            }
            
            throw new Error(response.error || 'Failed to refresh file list');
            
        } catch (error) {
            this.logger.error('FileModel', `Refresh failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Charge un fichier MIDI
     */
    async loadFile(fileId) {
        try {
            this.logger.info('FileModel', `Loading file: ${fileId}`);
            
            const response = await this.backend.sendCommand('files.load', {
                file_id: fileId
            });
            
            if (response.success) {
                const fileData = response.data;
                
                this.set('selectedFile', fileData);
                this.addToRecent(fileId);
                
                this.eventBus.emit('file:loaded', {
                    fileId,
                    data: fileData
                });
                
                return fileData;
            }
            
            throw new Error(response.error || 'Failed to load file');
            
        } catch (error) {
            this.logger.error('FileModel', `Load failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Supprime un fichier
     */
    async deleteFile(fileId) {
        try {
            this.logger.info('FileModel', `Deleting file: ${fileId}`);
            
            const response = await this.backend.sendCommand('files.delete', {
                file_id: fileId
            });
            
            if (response.success) {
                // Mettre à jour la liste locale
                const files = this.get('files').filter(f => f.id !== fileId);
                this.set('files', files);
                
                // Retirer des récents
                this.removeFromRecent(fileId);
                
                this.eventBus.emit('file:deleted', { fileId });
                
                return true;
            }
            
            throw new Error(response.error || 'Failed to delete file');
            
        } catch (error) {
            this.logger.error('FileModel', `Delete failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Renomme un fichier
     */
    async renameFile(fileId, newName) {
        try {
            this.logger.info('FileModel', `Renaming file ${fileId} to ${newName}`);
            
            const response = await this.backend.sendCommand('files.rename', {
                file_id: fileId,
                new_name: newName
            });
            
            if (response.success) {
                // Mettre à jour localement
                const files = this.get('files');
                const file = files.find(f => f.id === fileId);
                
                if (file) {
                    file.name = newName;
                    this.set('files', files);
                }
                
                this.eventBus.emit('file:renamed', {
                    fileId,
                    newName
                });
                
                return true;
            }
            
            throw new Error(response.error || 'Failed to rename file');
            
        } catch (error) {
            this.logger.error('FileModel', `Rename failed: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // FICHIERS RÉCENTS - SIMPLE
    // ========================================================================
    
    addToRecent(fileId) {
        let recent = this.get('recentFiles');
        
        // Retirer si déjà présent
        recent = recent.filter(id => id !== fileId);
        
        // Ajouter en tête
        recent.unshift(fileId);
        
        // Limiter à 10
        if (recent.length > 10) {
            recent = recent.slice(0, 10);
        }
        
        this.set('recentFiles', recent);
    }
    
    removeFromRecent(fileId) {
        const recent = this.get('recentFiles').filter(id => id !== fileId);
        this.set('recentFiles', recent);
    }
    
    getRecentFiles() {
        const recentIds = this.get('recentFiles');
        const files = this.get('files');
        
        return recentIds
            .map(id => files.find(f => f.id === id))
            .filter(f => f !== undefined);
    }
    
    clearRecent() {
        this.set('recentFiles', []);
        this.logger.info('FileModel', 'Recent files cleared');
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    getFilesInCurrentPath() {
        return this.get('files');
    }
    
    getFileById(fileId) {
        return this.get('files').find(f => f.id === fileId) || null;
    }
    
    selectFile(fileId) {
        const file = this.getFileById(fileId);
        
        if (file) {
            this.set('selectedFile', file);
            this.eventBus.emit('file:selected', { file });
        }
    }
    
    deselectFile() {
        this.set('selectedFile', null);
        this.eventBus.emit('file:deselected');
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
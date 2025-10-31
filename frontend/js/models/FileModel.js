// ============================================================================
// Fichier: frontend/js/models/FileModel.js
// Chemin réel: frontend/js/models/FileModel.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE
// Date: 2025-10-31
// ============================================================================

class FileModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, backend, logger, {}, {
            persistKey: 'filemodel',
            eventPrefix: 'file',
            autoPersist: true
        });
        
        // Données spécifiques
        this.data.files = [];
        this.data.currentFile = null;
        this.data.filter = '';
        this.data.sortBy = 'name';
        this.data.sortOrder = 'asc';
        
        this.log('debug', 'FileModel', 'Initialized');
    }
    
    // ========================================================================
    // API
    // ========================================================================
    
    async listFiles() {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'FileModel.listFiles', 'Backend not connected');
            return [];
        }
        
        try {
            const response = await this.backend.send('files.list', {});
            
            if (response.success && response.data.files) {
                this.data.files = response.data.files;
                this.emit('files:updated', { files: this.data.files });
                return this.data.files;
            }
            
        } catch (error) {
            this.log('error', 'FileModel.listFiles', error);
        }
        
        return [];
    }
    
    async readFile(filename) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'FileModel.readFile', 'Backend not connected');
            return null;
        }
        
        try {
            const response = await this.backend.send('files.read', { filename });
            
            if (response.success) {
                return response.data;
            }
            
        } catch (error) {
            this.log('error', 'FileModel.readFile', error);
        }
        
        return null;
    }
    
    async writeFile(filename, content) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'FileModel.writeFile', 'Backend not connected');
            return false;
        }
        
        try {
            const response = await this.backend.send('files.write', { filename, content });
            
            if (response.success) {
                await this.listFiles();
                return true;
            }
            
        } catch (error) {
            this.log('error', 'FileModel.writeFile', error);
        }
        
        return false;
    }
    
    async deleteFile(filename) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'FileModel.deleteFile', 'Backend not connected');
            return false;
        }
        
        try {
            const response = await this.backend.send('files.delete', { filename });
            
            if (response.success) {
                await this.listFiles();
                this.emit('file:deleted', { filename });
                return true;
            }
            
        } catch (error) {
            this.log('error', 'FileModel.deleteFile', error);
        }
        
        return false;
    }
    
    async getFileInfo(filename) {
        if (!this.backend || !this.backend.isConnected()) {
            this.log('warn', 'FileModel.getFileInfo', 'Backend not connected');
            return null;
        }
        
        try {
            const response = await this.backend.send('files.getInfo', { filename });
            
            if (response.success) {
                return response.data;
            }
            
        } catch (error) {
            this.log('error', 'FileModel.getFileInfo', error);
        }
        
        return null;
    }
    
    // ========================================================================
    // LOCAL
    // ========================================================================
    
    setCurrentFile(file) {
        this.data.currentFile = file;
        this.emit('file:selected', { file });
    }
    
    getCurrentFile() {
        return this.data.currentFile;
    }
    
    getFiles() {
        return this.data.files;
    }
    
    getFilteredFiles() {
        let files = [...this.data.files];
        
        if (this.data.filter) {
            const filter = this.data.filter.toLowerCase();
            files = files.filter(f => 
                f.name.toLowerCase().includes(filter)
            );
        }
        
        files.sort((a, b) => {
            const aVal = a[this.data.sortBy];
            const bVal = b[this.data.sortBy];
            
            if (this.data.sortOrder === 'asc') {
                return aVal < bVal ? -1 : 1;
            } else {
                return aVal > bVal ? -1 : 1;
            }
        });
        
        return files;
    }
    
    setFilter(filter) {
        this.data.filter = filter;
        this.emit('filter:changed', { filter });
    }
    
    setSorting(sortBy, sortOrder) {
        this.data.sortBy = sortBy;
        this.data.sortOrder = sortOrder;
        this.emit('sorting:changed', { sortBy, sortOrder });
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
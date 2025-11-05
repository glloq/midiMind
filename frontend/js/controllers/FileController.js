// ============================================================================
// Fichier: frontend/js/controllers/FileController.js
// Chemin réel: frontend/js/controllers/FileController.js
// Version: v4.3.0 - BACKEND NULL SAFETY
// Date: 2025-11-04
// ============================================================================
// CORRECTIONS v4.3.0:
// ✓ CRITIQUE: Ajout vérifications backend avant tous les appels
// ✓ CRITIQUE: Utilisation méthodes withBackend() et isBackendReady()
// ✓ Gestion mode offline avec messages appropriés
// ✓ Protection complète contre backend null/undefined
//
// CORRECTIONS v4.2.3:
// ✓ CRITIQUE: Ajout paramètre backend au constructeur (6ème paramètre)
// ✓ Fix: super() appelle BaseController avec backend
// ✓ this.backend initialisé automatiquement via BaseController
// ============================================================================

class FileController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        this.logger = window.logger || console;
        this.fileModel = models.file;
        this.view = views.file;
        // ✓ this.backend initialisé automatiquement par BaseController
        this.fileService = window.app?.services?.file;
        
        this.state = {
            ...this.state,
            currentPath: '/midi',
            selectedFile: null,
            isLoading: false,
            lastRefresh: null
        };
        
        this.config = {
            ...this.config,
            maxFileSize: 10 * 1024 * 1024,
            allowedExtensions: ['.mid', '.midi'],
            autoRefresh: true,
            confirmDelete: true,
            refreshInterval: 30000
        };
        
        this.refreshTimer = null;
        this._fullyInitialized = true;
        this.bindEvents();
    }
    
    bindEvents() {
        this.eventBus.on('file:select', (data) => this.selectFile(data.fileId));
        this.eventBus.on('file:load', (data) => this.loadFile(data.fileId));
        this.eventBus.on('file:save', (data) => this.saveFile(data.fileId, data.content));
        this.eventBus.on('file:delete', (data) => this.deleteFile(data.fileId));
        this.eventBus.on('file:refresh', () => this.refreshFileList());
        this.eventBus.on('file:upload', (data) => this.handleFileUpload(data.file));
        
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'files') {
                this.onFilesPageActive();
            } else {
                this.onFilesPageInactive();
            }
        });
        
        this.log('info', 'FileController', '✓ Events bound');
    }
    
    async onBackendConnected() {
        this.log('info', 'FileController', '✓ Backend connected');
        
        this.refreshFileList().catch(error => {
            this.log('warn', 'FileController', 'Initial file list failed:', error.message);
        });
        
        if (this.config.autoRefresh) {
            this.startAutoRefresh();
        }
    }
    
    onBackendDisconnected() {
        this.stopAutoRefresh();
        this.log('warn', 'FileController', '⚠️ Backend disconnected');
    }
    
    onFilesPageActive() {
        this.refreshFileList().catch(error => {
            // Silencieux si backend non disponible
            if (!error.offline) {
                this.log('error', 'FileController', 'Failed to refresh on page active:', error);
            }
        });
        
        if (this.config.autoRefresh && this.isBackendReady()) {
            this.startAutoRefresh();
        }
    }
    
    onFilesPageInactive() {
        this.stopAutoRefresh();
    }
    
    /**
     * ✓ Liste fichiers - files.list
     */
    async listFiles(path = null) {
        return this.withBackend(
            async () => {
                const targetPath = path || this.state.currentPath;
                
                this.log('info', 'FileController', `Listing files in: ${targetPath}`);
                this.state.isLoading = true;
                
                const response = await this.backend.listFiles(targetPath);
                
                this.state.isLoading = false;
                
                // ✓ Extraction via response (BackendService fait déjà)
                const files = response.files || [];
                
                if (this.fileModel) {
                    this.fileModel.set('files', files);
                    this.fileModel.set('currentPath', targetPath);
                }
                
                this.state.currentPath = targetPath;
                this.state.lastRefresh = Date.now();
                
                this.log('info', 'FileController', `✓ Found ${files.length} files`);
                this.eventBus.emit('files:list-updated', { files, path: targetPath });
                
                return files;
            },
            'list files',
            [] // Retourner liste vide si offline
        ).catch(error => {
            this.state.isLoading = false;
            if (!error.offline) {
                this.log('error', 'FileController', 'listFiles failed:', error);
            }
            throw error;
        });
    }
    
    /**
     * ✓ Lit fichier - files.read
     */
    async readFile(filename) {
        return this.withBackend(
            async () => {
                this.log('info', 'FileController', `Reading file: ${filename}`);
                
                const content = await this.backend.readFile(filename);
                
                this.log('info', 'FileController', `✓ File read: ${filename}`);
                this.eventBus.emit('file:read-complete', { filename, content });
                
                return content;
            },
            'read file',
            null
        );
    }
    
    /**
     * ✓ Écrit fichier - files.write
     */
    async writeFile(filename, content) {
        return this.withBackend(
            async () => {
                this.log('info', 'FileController', `Writing file: ${filename}`);
                
                await this.backend.writeFile(filename, content);
                
                this.log('info', 'FileController', `✓ File written: ${filename}`);
                this.eventBus.emit('file:write-complete', { filename });
                
                await this.refreshFileList();
                
                return true;
            },
            'write file',
            false
        );
    }
    
    /**
     * ✓ Supprime fichier - files.delete
     */
    async deleteFile(filename) {
        return this.withBackend(
            async () => {
                if (this.config.confirmDelete) {
                    const confirmed = confirm(`Delete file "${filename}"?`);
                    if (!confirmed) {
                        return false;
                    }
                }
                
                this.log('info', 'FileController', `Deleting file: ${filename}`);
                
                await this.backend.deleteFile(filename);
                
                if (this.fileModel) {
                    const files = this.fileModel.get('files') || [];
                    const filtered = files.filter(f => f.name !== filename && f.id !== filename);
                    this.fileModel.set('files', filtered);
                }
                
                this.log('info', 'FileController', `✓ File deleted: ${filename}`);
                this.eventBus.emit('file:delete-complete', { filename });
                
                await this.refreshFileList();
                
                return true;
            },
            'delete file',
            false
        );
    }
    
    async fileExists(filename) {
        return this.withBackend(
            async () => {
                const response = await this.backend.fileExists(filename);
                return response.exists || false;
            },
            'check file exists',
            false
        );
    }
    
    async getFileInfo(filename) {
        return this.withBackend(
            async () => {
                return await this.backend.getFileInfo(filename);
            },
            'get file info',
            null
        );
    }
    
    /**
     * ✓ CORRECTION: Upload via FileService (midi.import)
     */
    async handleFileUpload(file) {
        return this.withBackend(
            async () => {
                if (!file) {
                    throw new Error('No file provided');
                }
                
                this.log('info', 'FileController', `Uploading file: ${file.name}`);
                
                if (this.notifications) {
                    this.notifications.show('Upload', `Uploading ${file.name}...`, 'info', 2000);
                }
                
                // ✓ Utiliser FileService qui gère midi.import
                let result;
                if (this.fileService) {
                    result = await this.fileService.uploadFile(file);
                } else {
                    // Fallback direct via backend
                    result = await this.backend.uploadFile(file);
                }
                
                this.log('info', 'FileController', `✓ File uploaded: ${file.name}`);
                
                if (this.notifications) {
                    this.notifications.show('Upload Complete', `${file.name} uploaded`, 'success', 3000);
                }
                
                this.eventBus.emit('file:upload-complete', { file, result });
                
                await this.refreshFileList();
                
                return result;
            },
            'upload file',
            null
        ).catch(error => {
            this.log('error', 'FileController', 'Upload failed:', error);
            
            if (this.notifications && !error.offline) {
                this.notifications.show('Upload Failed', error.message, 'error', 5000);
            }
            
            throw error;
        });
    }
    
    async refreshFileList() {
        try {
            return await this.listFiles();
        } catch (error) {
            // Ne pas afficher d'erreur si mode offline
            if (!error.offline) {
                this.log('error', 'FileController', 'refreshFileList failed:', error);
                if (this.notifications) {
                    this.notifications.show('Error', 'Failed to refresh file list', 'error', 3000);
                }
            }
            throw error;
        }
    }
    
    async loadFile(fileId) {
        try {
            this.log('info', 'FileController', `Loading file: ${fileId}`);
            
            const content = await this.readFile(fileId);
            
            this.state.selectedFile = fileId;
            if (this.fileModel) {
                this.fileModel.set('selectedFile', { id: fileId, content });
            }
            
            this.eventBus.emit('file:loaded', { fileId, content });
            
            if (this.notifications) {
                this.notifications.show('File Loaded', `${fileId} loaded`, 'success', 2000);
            }
            
            return content;
        } catch (error) {
            if (!error.offline) {
                this.log('error', 'FileController', 'loadFile failed:', error);
                if (this.notifications) {
                    this.notifications.show('Error', `Failed to load: ${error.message}`, 'error', 3000);
                }
            }
            throw error;
        }
    }
    
    async saveFile(fileId, content) {
        try {
            this.log('info', 'FileController', `Saving file: ${fileId}`);
            
            await this.writeFile(fileId, content);
            
            if (this.notifications) {
                this.notifications.show('File Saved', `${fileId} saved`, 'success', 2000);
            }
            
            return true;
        } catch (error) {
            if (!error.offline) {
                this.log('error', 'FileController', 'saveFile failed:', error);
                if (this.notifications) {
                    this.notifications.show('Error', `Failed to save: ${error.message}`, 'error', 3000);
                }
            }
            throw error;
        }
    }
    
    selectFile(fileId) {
        this.state.selectedFile = fileId;
        
        if (this.fileModel) {
            const files = this.fileModel.get('files') || [];
            const file = files.find(f => f.id === fileId || f.name === fileId);
            if (file) {
                this.fileModel.setSelected(file);
            }
        }
        
        this.eventBus.emit('file:selected', { fileId });
    }
    
    startAutoRefresh() {
        if (this.refreshTimer) return;
        
        // Ne démarrer l'auto-refresh que si backend disponible
        if (!this.isBackendReady()) {
            this.log('info', 'FileController', 'Auto-refresh skipped - backend not ready');
            return;
        }
        
        this.log('info', 'FileController', 'Starting auto-refresh...');
        
        this.refreshTimer = setInterval(() => {
            // Vérifier backend avant chaque refresh
            if (!this.isBackendReady()) {
                this.stopAutoRefresh();
                return;
            }
            
            this.refreshFileList().catch(err => {
                // Silencieux si offline
                if (!err.offline) {
                    this.log('error', 'FileController', 'Auto-refresh failed:', err);
                }
            });
        }, this.config.refreshInterval);
    }
    
    stopAutoRefresh() {
        if (this.refreshTimer) {
            clearInterval(this.refreshTimer);
            this.refreshTimer = null;
            this.log('info', 'FileController', 'Auto-refresh stopped');
        }
    }
    
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        }
    }
}

if (typeof window !== 'undefined') {
    window.FileController = FileController;
}
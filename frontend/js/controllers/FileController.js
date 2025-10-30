// ============================================================================
// Fichier: frontend/js/controllers/FileController.js
// Version: v3.0.4-FIXED-MERGE
// Date: 2025-10-30
// ============================================================================
// CORRECTIONS v3.0.4:
// âœ… CRITIQUE: Fusionne state/config au lieu de les Ã©craser
// âœ… Utilise Object.assign pour prÃ©server propriÃ©tÃ©s de BaseController
// âœ… Conserve toutes les fonctionnalitÃ©s existantes
// ============================================================================

class FileController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Services - Initialize BEFORE everything else
        this.backend = window.app?.services?.backend || null;
        this.logger = window.logger || console;
        
        // âœ… FUSIONNER state au lieu d'Ã©craser
        Object.assign(this.state, {
            currentDirectory: '/midi',
            selectedFile: null,
            isLoading: false,
            lastRefresh: null
        });
        
        // âœ… FUSIONNER config au lieu d'Ã©craser
        Object.assign(this.config, {
            maxFileSize: 10 * 1024 * 1024, // 10 MB
            allowedExtensions: ['.mid', '.midi'],
            autoRefresh: true,
            confirmDelete: true
        });
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        // Now initialize
        // ✅ REMOVED: this.initialize() - BaseController calls it via autoInitialize
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        // Only initialize if fully ready
        if (!this._fullyInitialized) {
            return;
        }
        
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', 'ðŸ“ Initializing...');
        }
        
        // VÃ©rifier dÃ©pendances
        if (!this.backend) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'BackendService not available');
            }
            return;
        }
        
        // Setup Ã©vÃ©nements
        this.setupEventListeners();
        
        // Charger liste initiale
        setTimeout(() => {
            this.refreshFileList();
        }, 1000);
        
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', 'âœ“ Initialized');
        }
    }
    
    setupEventListeners() {
        // Ã‰vÃ©nements UI
        this.eventBus.on('file:select', (data) => this.handleFileSelect(data));
        this.eventBus.on('file:load', (data) => this.handleFileLoad(data));
        this.eventBus.on('file:upload', (data) => this.handleFileUpload(data));
        this.eventBus.on('file:delete', (data) => this.handleFileDelete(data));
        this.eventBus.on('file:rename', (data) => this.handleFileRename(data));
        this.eventBus.on('file:refresh', () => this.refreshFileList());
        
        // Ã‰vÃ©nements backend
        this.eventBus.on('backend:event', (event) => this.handleBackendEvent(event));
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
    }
    
    // ========================================================================
    // GESTION LISTE DE FICHIERS
    // ========================================================================
    
    /**
     * RafraÃ®chit la liste des fichiers
     * @returns {Promise<Array>} Liste des fichiers
     */
    async refreshFileList() {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', 'Refreshing file list...');
        }
        
        try {
            this.showLoading(true);
            
            const result = await this.backend.sendCommand('files.list', {
                directory: this.state.currentDirectory
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to fetch file list');
            }
            
            const files = result.data?.files || [];
            
            // Mettre Ã  jour model
            const model = this.getModel('file');
            if (model) {
                model.set('files', files);
                model.set('directory', this.state.currentDirectory);
                model.set('lastRefresh', Date.now());
            }
            
            // Mettre Ã  jour view
            this.updateView('file', {
                files: files,
                directory: this.state.currentDirectory,
                count: files.length
            });
            
            this.state.lastRefresh = Date.now();
            
            if (this.logger && this.logger.info) {
                this.logger.info('FileController', `âœ“ ${files.length} files loaded`);
            }
            
            // Ã‰mettre Ã©vÃ©nement
            this.eventBus.emit('files:refreshed', {
                files: files,
                count: files.length
            });
            
            return files;
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'Failed to refresh file list:', error);
            }
            
            this.showNotification(
                `Failed to load files: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            return [];
            
        } finally {
            this.showLoading(false);
        }
    }
    
    // ========================================================================
    // SÃ‰LECTION FICHIER
    // ========================================================================
    
    /**
     * SÃ©lectionne un fichier
     * @param {string} fileId - ID du fichier
     */
    selectFile(fileId) {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', `Selecting file: ${fileId}`);
        }
        
        const previousFile = this.state.selectedFile;
        this.state.selectedFile = fileId;
        
        // RÃ©cupÃ©rer infos fichier
        const file = this.getFileById(fileId);
        
        // Ã‰mettre Ã©vÃ©nement
        this.eventBus.emit('file:selected', {
            fileId: fileId,
            file: file,
            previousFile: previousFile
        });
        
        // Mettre Ã  jour view
        this.updateView('file', {
            selectedFile: fileId,
            selectedFileData: file
        });
    }
    
    /**
     * Handler pour Ã©vÃ©nement file:select
     * @private
     */
    handleFileSelect(data) {
        const fileId = data.fileId || data.id || data.file_id;
        
        if (!fileId) {
            if (this.logger && this.logger.warn) {
                this.logger.warn('FileController', 'No file ID provided for select');
            }
            return;
        }
        
        this.selectFile(fileId);
    }
    
    /**
     * Charge un fichier pour lecture via GlobalPlaybackController
     * @param {string} fileId - ID du fichier
     * @returns {Promise<void>}
     */
    async loadFile(fileId) {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', `Loading file: ${fileId}`);
        }
        
        try {
            // VÃ©rifier GlobalPlaybackController disponible
            if (!window.globalPlaybackController) {
                throw new Error('GlobalPlaybackController not available');
            }
            
            // Charger via playback controller
            await window.globalPlaybackController.loadFile(fileId);
            
            // RÃ©cupÃ©rer nom fichier
            const file = this.getFileById(fileId);
            const fileName = file?.filename || file?.name || 'File';
            
            // Notification succÃ¨s
            this.showNotification(
                `${fileName} loaded`,
                'success',
                { duration: 2000 }
            );
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'Load file failed:', error);
            }
            
            this.showNotification(
                `Failed to load file: ${error.message}`,
                'error',
                { duration: 5000 }
            );
        }
    }
    
    /**
     * Handler pour Ã©vÃ©nement file:load
     * @private
     */
    async handleFileLoad(data) {
        const fileId = data.fileId || data.id || data.file_id;
        
        if (!fileId) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'No file ID provided for load');
            }
            return;
        }
        
        await this.loadFile(fileId);
    }
    
    // ========================================================================
    // UPLOAD FICHIER
    // ========================================================================
    
    /**
     * Upload un fichier MIDI
     * @param {File} file - Fichier Ã  uploader
     * @returns {Promise<boolean>} SuccÃ¨s
     */
    async uploadFile(file) {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', `Uploading file: ${file.name}`);
        }
        
        try {
            // VÃ©rifier taille
            if (file.size > this.config.maxFileSize) {
                throw new Error(`File too large (max ${this.config.maxFileSize / 1024 / 1024} MB)`);
            }
            
            // VÃ©rifier extension
            const extension = '.' + file.name.split('.').pop().toLowerCase();
            if (!this.config.allowedExtensions.includes(extension)) {
                throw new Error(`Invalid file type (allowed: ${this.config.allowedExtensions.join(', ')})`);
            }
            
            // Afficher loader
            this.showLoading(true);
            
            // Convertir en base64
            const base64Content = await this.fileToBase64(file);
            
            // Uploader via backend
            const result = await this.backend.sendCommand('files.upload', {
                filename: file.name,
                content: base64Content,
                directory: this.state.currentDirectory
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Upload failed');
            }
            
            if (this.logger && this.logger.info) {
                this.logger.info('FileController', `âœ“ File uploaded: ${file.name}`);
            }
            
            // RafraÃ®chir liste
            await this.refreshFileList();
            
            // Notification succÃ¨s
            this.showNotification(
                `File uploaded: ${file.name}`,
                'success',
                { duration: 3000 }
            );
            
            // Ã‰mettre Ã©vÃ©nement
            this.eventBus.emit('file:uploaded', {
                filename: file.name,
                fileId: result.data?.file_id
            });
            
            return true;
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'Upload failed:', error);
            }
            
            this.showNotification(
                `Failed to upload file: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            return false;
            
        } finally {
            this.showLoading(false);
        }
    }
    
    /**
     * Handler pour Ã©vÃ©nement file:upload
     * @private
     */
    async handleFileUpload(data) {
        const file = data.file || data.files?.[0];
        
        if (!file) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'No file provided for upload');
            }
            return;
        }
        
        await this.uploadFile(file);
    }
    
    // ========================================================================
    // SUPPRESSION FICHIER
    // ========================================================================
    
    /**
     * Supprime un fichier avec confirmation
     * @param {string} fileId - ID ou path du fichier
     * @returns {Promise<boolean>} SuccÃ¨s
     */
    async deleteFile(fileId) {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', `Deleting file: ${fileId}`);
        }
        
        try {
            // RÃ©cupÃ©rer info fichier
            const file = this.getFileById(fileId);
            const fileName = file?.filename || file?.name || fileId;
            
            // Confirmation utilisateur (si activÃ©e)
            if (this.config.confirmDelete) {
                const confirmed = confirm(
                    `Delete file "${fileName}"?\n\n` +
                    `This action cannot be undone.`
                );
                
                if (!confirmed) {
                    if (this.logger && this.logger.info) {
                        this.logger.info('FileController', 'Delete cancelled by user');
                    }
                    return false;
                }
            }
            
            // Afficher loader
            this.showLoading(true);
            
            // Supprimer via backend
            const result = await this.backend.sendCommand('files.delete', {
                file_id: fileId,
                file_path: fileId
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Delete failed');
            }
            
            if (this.logger && this.logger.info) {
                this.logger.info('FileController', `âœ“ File deleted: ${fileName}`);
            }
            
            // Si fichier sÃ©lectionnÃ©, dÃ©sÃ©lectionner
            if (this.state.selectedFile === fileId) {
                this.state.selectedFile = null;
                this.eventBus.emit('file:deselected');
            }
            
            // RafraÃ®chir liste
            await this.refreshFileList();
            
            // Notification succÃ¨s
            this.showNotification(
                `File deleted: ${fileName}`,
                'info',
                { duration: 3000 }
            );
            
            // Ã‰mettre Ã©vÃ©nement
            this.eventBus.emit('file:deleted', { 
                fileId,
                file
            });
            
            return true;
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'Delete failed:', error);
            }
            
            this.showNotification(
                `Failed to delete file: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            return false;
            
        } finally {
            this.showLoading(false);
        }
    }
    
    /**
     * Handler pour Ã©vÃ©nement file:delete
     * @private
     */
    async handleFileDelete(data) {
        const fileId = data.fileId || data.id || data.file_id || data.filePath;
        
        if (!fileId) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'No file ID provided for delete');
            }
            return;
        }
        
        await this.deleteFile(fileId);
    }
    
    // ========================================================================
    // RENOMMER FICHIER
    // ========================================================================
    
    /**
     * Renomme un fichier
     * @param {string} fileId - ID du fichier
     * @param {string} newName - Nouveau nom
     * @returns {Promise<boolean>} SuccÃ¨s
     */
    async renameFile(fileId, newName) {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', `Renaming file: ${fileId} â†’ ${newName}`);
        }
        
        try {
            const result = await this.backend.sendCommand('files.rename', {
                file_id: fileId,
                new_name: newName
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Rename failed');
            }
            
            // RafraÃ®chir liste
            await this.refreshFileList();
            
            // Notification
            this.showNotification(
                `File renamed to: ${newName}`,
                'success',
                { duration: 2000 }
            );
            
            return true;
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'Rename failed:', error);
            }
            
            this.showNotification(
                `Failed to rename file: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            return false;
        }
    }
    
    /**
     * Handler pour Ã©vÃ©nement file:rename
     * @private
     */
    async handleFileRename(data) {
        const fileId = data.fileId || data.id || data.file_id;
        const newName = data.newName || data.name;
        
        if (!fileId || !newName) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'Invalid rename parameters');
            }
            return;
        }
        
        await this.renameFile(fileId, newName);
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * RÃ©cupÃ¨re un fichier par ID
     * @param {string} fileId - ID du fichier
     * @returns {Object|null} Fichier
     */
    getFileById(fileId) {
        const model = this.getModel('file');
        const files = model?.get('files') || [];
        return files.find(f => 
            f.id === fileId || 
            f.file_id === fileId || 
            f.path === fileId ||
            f.file_path === fileId
        );
    }
    
    /**
     * Convertit un File en base64
     * @param {File} file - Fichier
     * @returns {Promise<string>} Base64
     */
    fileToBase64(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = () => {
                const base64 = reader.result.split(',')[1];
                resolve(base64);
            };
            reader.onerror = reject;
            reader.readAsDataURL(file);
        });
    }
    
    /**
     * Affiche/masque le loader
     * @param {boolean} show - Afficher
     */
    showLoading(show) {
        this.state.isLoading = show;
        this.eventBus.emit('file:loading', { isLoading: show });
    }
    
    /**
     * Met Ã  jour une vue
     * @param {string} viewName - Nom de la vue
     * @param {Object} data - DonnÃ©es
     */
    updateView(viewName, data) {
        const view = this.getView(viewName);
        if (view && typeof view.render === 'function') {
            view.render(data);
        }
    }
    
    /**
     * GÃ¨re les Ã©vÃ©nements reÃ§us du backend
     * @param {Object} event - Ã‰vÃ©nement backend
     */
    handleBackendEvent(event) {
        if (!event || !event.name) {
            if (this.logger && this.logger.warn) {
                this.logger.warn('FileController', 'Invalid backend event received');
            }
            return;
        }
        
        if (this.logger && this.logger.debug) {
            this.logger.debug('FileController', `Backend event: ${event.name}`);
        }
        
        switch (event.name) {
            case 'files:list':
            case 'files:refreshed':
                // Liste rafraÃ®chie
                break;
            case 'file:added':
                this.refreshFileList();
                break;
            case 'file:deleted':
                this.refreshFileList();
                break;
            case 'file:renamed':
                this.refreshFileList();
                break;
        }
    }
    
    onBackendConnected() {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', 'Backend connected, refreshing file list');
        }
        
        if (this.config.autoRefresh) {
            this.refreshFileList();
        }
    }
    
    onBackendDisconnected() {
        if (this.logger && this.logger.warn) {
            this.logger.warn('FileController', 'Backend disconnected');
        }
    }
    
    /**
     * RÃ©cupÃ¨re l'Ã©tat du contrÃ´leur
     * @returns {Object} Ã‰tat
     */
    getState() {
        return {
            ...this.state,
            filesCount: this.getModel('file')?.get('files')?.length || 0
        };
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileController;
}

if (typeof window !== 'undefined') {
    window.FileController = FileController;
}

// Export par dÃ©faut
window.FileController = FileController;

// ============================================================================
// FIN DU FICHIER FileController.js v3.0.4-FIXED-MERGE
// ============================================================================
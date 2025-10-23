// ============================================================================
// Fichier: frontend/js/controllers/FileController.js
// Version: v3.0.3-FIXED
// Date: 2025-10-20
// ============================================================================
// CORRECTIONS v3.0.3:
// Ã¢Å“â€¦ Fixed initialization order (logger before initialize call)
// Ã¢Å“â€¦ Added _fullyInitialized flag pattern
// Ã¢Å“â€¦ Protected initialize() method
// ============================================================================


class FileController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Services - Initialize BEFORE everything else
        this.backend = window.app?.services?.backend || null;
        this.logger = window.logger || console;
        
        // Ãƒâ€°tat
        this.state = {
            currentDirectory: '/midi',
            selectedFile: null,
            isLoading: false,
            lastRefresh: null
        };
        
        // Configuration
        this.config = {
            maxFileSize: 10 * 1024 * 1024, // 10 MB
            allowedExtensions: ['.mid', '.midi'],
            autoRefresh: true,
            confirmDelete: true
        };
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        // Now initialize
        this.initialize();
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
            this.logger.info('FileController', 'Ã°Å¸â€œÂ Initializing...');
        }
        
        // VÃƒÂ©rifier dÃƒÂ©pendances
        if (!this.backend) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'BackendService not available');
            }
            return;
        }
        
        // Setup ÃƒÂ©vÃƒÂ©nements
        this.setupEventListeners();
        
        // Charger liste initiale
        setTimeout(() => {
            this.refreshFileList();
        }, 1000);
        
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', 'Ã¢Å“â€œ Initialized');
        }
    }
    
    setupEventListeners() {
        // Ãƒâ€°vÃƒÂ©nements UI
        this.eventBus.on('file:select', (data) => this.handleFileSelect(data));
        this.eventBus.on('file:load', (data) => this.handleFileLoad(data));
        this.eventBus.on('file:upload', (data) => this.handleFileUpload(data));
        this.eventBus.on('file:delete', (data) => this.handleFileDelete(data));
        this.eventBus.on('file:rename', (data) => this.handleFileRename(data));
        this.eventBus.on('file:refresh', () => this.refreshFileList());
        
        // Ãƒâ€°vÃƒÂ©nements backend
        this.eventBus.on('backend:event', (event) => this.handleBackendEvent(event));
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
    }
    
    // ========================================================================
    // GESTION LISTE DE FICHIERS
    // ========================================================================
    
    /**
     * RafraÃƒÂ®chit la liste des fichiers
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
            
            // Mettre ÃƒÂ  jour model
            const model = this.getModel('file');
            if (model) {
                model.set('files', files);
                model.set('directory', this.state.currentDirectory);
                model.set('lastRefresh', Date.now());
            }
            
            // Mettre ÃƒÂ  jour view
            this.updateView('file', {
                files: files,
                directory: this.state.currentDirectory,
                count: files.length
            });
            
            this.state.lastRefresh = Date.now();
            
            if (this.logger && this.logger.info) {
                this.logger.info('FileController', `Ã¢Å“â€œ ${files.length} files loaded`);
            }
            
            // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
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
    // SÃƒâ€°LECTION FICHIER
    // ========================================================================
    
    /**
     * SÃƒÂ©lectionne un fichier
     * @param {string} fileId - ID du fichier
     */
    selectFile(fileId) {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', `Selecting file: ${fileId}`);
        }
        
        const previousFile = this.state.selectedFile;
        this.state.selectedFile = fileId;
        
        // RÃƒÂ©cupÃƒÂ©rer infos fichier
        const file = this.getFileById(fileId);
        
        // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
        this.eventBus.emit('file:selected', {
            fileId: fileId,
            file: file,
            previousFile: previousFile
        });
        
        // Mettre ÃƒÂ  jour view
        this.updateView('file', {
            selectedFile: fileId,
            selectedFileData: file
        });
    }
    
    /**
     * Handler pour ÃƒÂ©vÃƒÂ©nement file:select
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
            // VÃƒÂ©rifier GlobalPlaybackController disponible
            if (!window.globalPlaybackController) {
                throw new Error('GlobalPlaybackController not available');
            }
            
            // Charger via playback controller
            await window.globalPlaybackController.loadFile(fileId);
            
            // RÃƒÂ©cupÃƒÂ©rer nom fichier
            const file = this.getFileById(fileId);
            const fileName = file?.filename || file?.name || 'File';
            
            // Notification succÃƒÂ¨s
            this.showNotification(
                `${fileName} loaded`,
                'success',
                { duration: 2000 }
            );
            
            if (this.logger && this.logger.info) {
                this.logger.info('FileController', 'Ã¢Å“â€œ File loaded successfully');
            }
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'Failed to load file:', error);
            }
            
            this.showNotification(
                `Failed to load file: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            throw error;
        }
    }
    
    /**
     * Handler pour ÃƒÂ©vÃƒÂ©nement file:load
     * @private
     */
    async handleFileLoad(data) {
        const fileId = data.fileId || data.id || data.file_id || data.filePath;
        
        if (!fileId) {
            if (this.logger && this.logger.warn) {
                this.logger.warn('FileController', 'No file ID provided for load');
            }
            return;
        }
        
        try {
            await this.loadFile(fileId);
        } catch (error) {
            // Erreur dÃƒÂ©jÃƒÂ  gÃƒÂ©rÃƒÂ©e dans loadFile()
        }
    }
    
    // ========================================================================
    // UPLOAD FICHIER
    // ========================================================================
    
    /**
     * Upload un fichier MIDI
     * @param {File} file - Fichier ÃƒÂ  uploader
     * @param {Function} onProgress - Callback progression
     * @returns {Promise<Object>} Fichier uploadÃƒÂ©
     */
    async uploadFile(file, onProgress = null) {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', `Uploading file: ${file.name}`);
        }
        
        try {
            // Validation
            if (!file) {
                throw new Error('No file provided');
            }
            
            if (file.size > this.config.maxFileSize) {
                throw new Error(`File too large (max ${this.config.maxFileSize / 1024 / 1024}MB)`);
            }
            
            const ext = '.' + file.name.split('.').pop().toLowerCase();
            if (!this.config.allowedExtensions.includes(ext)) {
                throw new Error(`Invalid file type (allowed: ${this.config.allowedExtensions.join(', ')})`);
            }
            
            // Afficher loader
            this.showLoading(true);
            
            // Lire fichier en base64
            const base64 = await this.fileToBase64(file);
            
            // Upload via backend
            const result = await this.backend.sendCommand('files.upload', {
                filename: file.name,
                content: base64,
                directory: this.state.currentDirectory
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Upload failed');
            }
            
            const uploadedFile = result.data;
            
            if (this.logger && this.logger.info) {
                this.logger.info('FileController', `Ã¢Å“â€œ File uploaded: ${file.name}`);
            }
            
            // RafraÃƒÂ®chir liste
            await this.refreshFileList();
            
            // Notification succÃƒÂ¨s
            this.showNotification(
                `File uploaded: ${file.name}`,
                'success',
                { duration: 3000 }
            );
            
            // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
            this.eventBus.emit('file:uploaded', {
                file: uploadedFile,
                originalFile: file
            });
            
            return uploadedFile;
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'Upload failed:', error);
            }
            
            this.showNotification(
                `Upload failed: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            throw error;
            
        } finally {
            this.showLoading(false);
        }
    }
    
    /**
     * Handler pour ÃƒÂ©vÃƒÂ©nement file:upload
     * @private
     */
    async handleFileUpload(data) {
        const { file, onProgress } = data;
        
        if (!file) {
            if (this.logger && this.logger.error) {
                this.logger.error('FileController', 'No file provided for upload');
            }
            return;
        }
        
        try {
            await this.uploadFile(file, onProgress);
        } catch (error) {
            // Erreur dÃƒÂ©jÃƒÂ  gÃƒÂ©rÃƒÂ©e dans uploadFile()
        }
    }
    
    // ========================================================================
    // SUPPRESSION FICHIER
    // ========================================================================
    
    /**
     * Supprime un fichier avec confirmation
     * @param {string} fileId - ID ou path du fichier
     * @returns {Promise<boolean>} SuccÃƒÂ¨s
     */
    async deleteFile(fileId) {
        if (this.logger && this.logger.info) {
            this.logger.info('FileController', `Deleting file: ${fileId}`);
        }
        
        try {
            // RÃƒÂ©cupÃƒÂ©rer info fichier
            const file = this.getFileById(fileId);
            const fileName = file?.filename || file?.name || fileId;
            
            // Confirmation utilisateur (si activÃƒÂ©e)
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
                this.logger.info('FileController', `Ã¢Å“â€œ File deleted: ${fileName}`);
            }
            
            // Si fichier sÃƒÂ©lectionnÃƒÂ©, dÃƒÂ©sÃƒÂ©lectionner
            if (this.state.selectedFile === fileId) {
                this.state.selectedFile = null;
                this.eventBus.emit('file:deselected');
            }
            
            // RafraÃƒÂ®chir liste
            await this.refreshFileList();
            
            // Notification succÃƒÂ¨s
            this.showNotification(
                `File deleted: ${fileName}`,
                'info',
                { duration: 3000 }
            );
            
            // Ãƒâ€°mettre ÃƒÂ©vÃƒÂ©nement
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
     * Handler pour ÃƒÂ©vÃƒÂ©nement file:delete
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
    // HELPERS
    // ========================================================================
    
    /**
     * RÃƒÂ©cupÃƒÂ¨re un fichier par ID
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
     * Met ÃƒÂ  jour une vue
     * @param {string} viewName - Nom de la vue
     * @param {Object} data - DonnÃƒÂ©es
     */
    updateView(viewName, data) {
        const view = this.getView(viewName);
        if (view && typeof view.render === 'function') {
            view.render(data);
        }
    }
    
    /**
     * GÃƒÂ¨re les ÃƒÂ©vÃƒÂ©nements reÃƒÂ§us du backend
     * @param {Object} event - Ãƒâ€°vÃƒÂ©nement backend
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
                // Liste rafraÃƒÂ®chie
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
     * RÃƒÂ©cupÃƒÂ¨re l'ÃƒÂ©tat du contrÃƒÂ´leur
     * @returns {Object} Ãƒâ€°tat
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

// Export par défaut
window.FileController = FileController;

// ============================================================================
// FIN DU FICHIER FileController.js v3.0.3-FIXED
// ============================================================================
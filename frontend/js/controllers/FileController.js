// ============================================================================
// Fichier: frontend/js/controllers/FileController.js
// Version: v3.3.0 - API CONFORMITÉ
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.2.0:
// âœ… Toutes les commandes files.* implÃ©mentÃ©es
// âœ… Gestion upload/download fichiers MIDI
// âœ… Gestion Ã©vÃ©nements backend temps rÃ©el
// âœ… Auto-refresh liste fichiers
// ============================================================================

class FileController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.logger = window.logger || console;
        this.fileModel = models.file;
        this.view = views.file;
        this.backend = window.app?.services?.backend || window.backendService;
        
        // Ã‰tat
        this.state = {
            ...this.state,
            currentPath: '/midi',
            selectedFile: null,
            isLoading: false,
            lastRefresh: null
        };
        
        // Configuration
        this.config = {
            ...this.config,
            maxFileSize: 10 * 1024 * 1024, // 10 MB
            allowedExtensions: ['.mid', '.midi'],
            autoRefresh: true,
            confirmDelete: true,
            refreshInterval: 30000 // 30 secondes
        };
        
        // Timer
        this.refreshTimer = null;
        
        this._fullyInitialized = true;
        this.bindEvents();
    }
    
    // ========================================================================
    // Ã‰VÃ‰NEMENTS
    // ========================================================================
    
    bindEvents() {
        // Actions fichiers
        this.eventBus.on('file:select', (data) => this.selectFile(data.fileId));
        this.eventBus.on('file:load', (data) => this.loadFile(data.fileId));
        this.eventBus.on('file:save', (data) => this.saveFile(data.fileId, data.content));
        this.eventBus.on('file:delete', (data) => this.deleteFile(data.fileId));
        this.eventBus.on('file:refresh', () => this.refreshFileList());
        
        // Backend
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        
        // Navigation
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'files') {
                this.onFilesPageActive();
            } else {
                this.onFilesPageInactive();
            }
        });
        
        this.log('info', 'FileController', 'âœ… Events bound');
    }
    
    async onBackendConnected() {
        this.log('info', 'FileController', 'âœ… Backend connected');
        
        try {
            // Charger liste initiale
            await this.refreshFileList();
            
            // DÃ©marrer auto-refresh si configurÃ©
            if (this.config.autoRefresh) {
                this.startAutoRefresh();
            }
        } catch (error) {
            this.log('error', 'FileController', 'Initialization failed:', error);
        }
    }
    
    onBackendDisconnected() {
        this.stopAutoRefresh();
        this.log('warn', 'FileController', 'âš ï¸ Backend disconnected');
    }
    
    onFilesPageActive() {
        this.refreshFileList();
        if (this.config.autoRefresh) {
            this.startAutoRefresh();
        }
    }
    
    onFilesPageInactive() {
        this.stopAutoRefresh();
    }
    
    // ========================================================================
    // COMMANDES FILES.*
    // ========================================================================
    
    /**
     * Liste tous les fichiers MIDI
     */
    async listFiles(path = null) {
        try {
            const targetPath = path || this.state.currentPath;
            
            this.log('info', 'FileController', \`Listing files in: \${targetPath}\`);
            this.state.isLoading = true;
            
            const response = await this.backend.sendCommand('list_files', {
                path: targetPath
            });
            
            this.state.isLoading = false;
            
            if (response.success !== false) {
                const files = response.data?.files || response.files || [];
                
                // Mettre Ã  jour le model
                if (this.fileModel) {
                    this.fileModel.set('files', files);
                    this.fileModel.set('currentPath', targetPath);
                }
                
                this.state.currentPath = targetPath;
                this.state.lastRefresh = Date.now();
                
                this.log('info', 'FileController', \`âœ… Found \${files.length} files\`);
                this.eventBus.emit('files:list-updated', { files, path: targetPath });
                
                return files;
            }
            throw new Error(response.message || 'Failed to list files');
        } catch (error) {
            this.state.isLoading = false;
            this.log('error', 'FileController', 'listFiles failed:', error);
            throw error;
        }
    }
    
    /**
     * Lit un fichier MIDI
     */
    async readFile(filename) {
        try {
            this.log('info', 'FileController', \`Reading file: \${filename}\`);
            
            const response = await this.backend.sendCommand('load_file', {
                filename: filename
            });
            
            if (response.success !== false) {
                const content = response.data || response;
                
                this.log('info', 'FileController', \`âœ… File read: \${filename}\`);
                this.eventBus.emit('file:read-complete', { filename, content });
                
                return content;
            }
            throw new Error(response.message || 'Failed to read file');
        } catch (error) {
            this.log('error', 'FileController', 'readFile failed:', error);
            throw error;
        }
    }
    
    /**
     * Ã‰crit un fichier MIDI
     */
    async writeFile(filename, content) {
        try {
            this.log('info', 'FileController', \`Writing file: \${filename}\`);
            
            const response = await this.backend.sendCommand('import_file', {
                filename: filename,
                content: content
            });
            
            if (response.success !== false) {
                this.log('info', 'FileController', \`âœ… File written: \${filename}\`);
                this.eventBus.emit('file:write-complete', { filename });
                
                // RafraÃ®chir liste
                await this.refreshFileList();
                
                return true;
            }
            throw new Error(response.message || 'Failed to write file');
        } catch (error) {
            this.log('error', 'FileController', 'writeFile failed:', error);
            throw error;
        }
    }
    
    /**
     * Supprime un fichier MIDI
     */
    async deleteFile(filename) {
        try {
            // Confirmation si configurÃ©
            if (this.config.confirmDelete) {
                const confirmed = confirm(\`Delete file "\${filename}"?\`);
                if (!confirmed) {
                    return false;
                }
            }
            
            this.log('info', 'FileController', \`Deleting file: \${filename}\`);
            
            const response = await this.backend.sendCommand('delete_file', {
                filename: filename
            });
            
            if (response.success !== false) {
                // Mettre Ã  jour le model
                if (this.fileModel) {
                    const files = this.fileModel.get('files') || [];
                    const filtered = files.filter(f => f.name !== filename && f.id !== filename);
                    this.fileModel.set('files', filtered);
                }
                
                this.log('info', 'FileController', \`âœ… File deleted: \${filename}\`);
                this.eventBus.emit('file:delete-complete', { filename });
                
                // RafraÃ®chir liste
                await this.refreshFileList();
                
                return true;
            }
            throw new Error(response.message || 'Failed to delete file');
        } catch (error) {
            this.log('error', 'FileController', 'deleteFile failed:', error);
            throw error;
        }
    }
    
    // ========================================================================
    // OPÃ‰RATIONS FICHIERS
    // ========================================================================
    
    /**
     * RafraÃ®chit la liste des fichiers
     */
    async refreshFileList() {
        try {
            return await this.listFiles();
        } catch (error) {
            this.log('error', 'FileController', 'refreshFileList failed:', error);
            if (this.notifications) {
                this.notifications.show(
                    'Error',
                    'Failed to refresh file list',
                    'error',
                    3000
                );
            }
            throw error;
        }
    }
    
    /**
     * Charge un fichier
     */
    async loadFile(fileId) {
        try {
            this.log('info', 'FileController', \`Loading file: \${fileId}\`);
            
            // Lire le contenu
            const content = await this.readFile(fileId);
            
            // SÃ©lectionner
            this.state.selectedFile = fileId;
            if (this.fileModel) {
                this.fileModel.set('selectedFile', { id: fileId, content });
            }
            
            // Notifier
            this.eventBus.emit('file:loaded', { fileId, content });
            
            if (this.notifications) {
                this.notifications.show(
                    'File Loaded',
                    \`File "\${fileId}" loaded successfully\`,
                    'success',
                    2000
                );
            }
            
            return content;
        } catch (error) {
            this.log('error', 'FileController', 'loadFile failed:', error);
            if (this.notifications) {
                this.notifications.show(
                    'Error',
                    \`Failed to load file: \${error.message}\`,
                    'error',
                    3000
                );
            }
            throw error;
        }
    }
    
    /**
     * Sauvegarde un fichier
     */
    async saveFile(fileId, content) {
        try {
            this.log('info', 'FileController', \`Saving file: \${fileId}\`);
            
            await this.writeFile(fileId, content);
            
            if (this.notifications) {
                this.notifications.show(
                    'File Saved',
                    \`File "\${fileId}" saved successfully\`,
                    'success',
                    2000
                );
            }
            
            return true;
        } catch (error) {
            this.log('error', 'FileController', 'saveFile failed:', error);
            if (this.notifications) {
                this.notifications.show(
                    'Error',
                    \`Failed to save file: \${error.message}\`,
                    'error',
                    3000
                );
            }
            throw error;
        }
    }
    
    /**
     * SÃ©lectionne un fichier
     */
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
    
    // ========================================================================
    // AUTO-REFRESH
    // ========================================================================
    
    startAutoRefresh() {
        if (this.refreshTimer) {
            return; // DÃ©jÃ  dÃ©marrÃ©
        }
        
        this.log('info', 'FileController', 'Starting auto-refresh...');
        
        this.refreshTimer = setInterval(() => {
            this.refreshFileList().catch(err => {
                this.log('error', 'FileController', 'Auto-refresh failed:', err);
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
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        }
    }
}

// ============================================================================
// EXPORT
// ============================================================================
if (typeof window !== 'undefined') {
    window.FileController = FileController;
}
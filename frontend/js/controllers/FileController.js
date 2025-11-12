// ============================================================================
// Fichier: frontend/js/controllers/FileController.js
// Chemin réel: frontend/js/controllers/FileController.js
// Version: v4.5.0 - SUPPORT ROUTAGE + ÉDITION
// Date: 2025-11-12
// ============================================================================
// CORRECTIONS v4.5.0:
// ✅ Support événement file:load_for_routing (routage MIDI)
// ✅ Upload fonctionnel vérifié
// ✅ Tous événements FileView gérés
// ============================================================================

class FileController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        this.logger = window.logger || console;
        this.fileModel = models.file;
        this.view = views.file;
        // ✅ this.backend initialisé automatiquement par BaseController
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
        // Événements standards
        this.eventBus.on('file:select', (data) => this.selectFile(data.fileId));
        this.eventBus.on('file:load', (data) => this.loadFile(data.fileId));
        this.eventBus.on('file:save', (data) => this.saveFile(data.fileId, data.content));
        this.eventBus.on('file:delete', (data) => this.deleteFile(data.fileId));
        this.eventBus.on('file:refresh', () => this.refreshFileList());
        this.eventBus.on('file:upload', (data) => this.handleFileUpload(data.file));
        
        // ✅ ÉVÉNEMENTS depuis FileView
        this.eventBus.on('file:list_requested', (data) => this.handleListRequest(data));
        this.eventBus.on('file:delete_requested', (data) => this.handleDeleteRequest(data));
        this.eventBus.on('file:play_requested', (data) => this.handlePlayRequest(data));
        this.eventBus.on('file:load_in_editor', (data) => this.handleLoadInEditor(data));
        this.eventBus.on('file:load_for_routing', (data) => this.handleLoadForRouting(data));
        
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
        
        this.log('info', 'FileController', '✅ Events bound (v4.5.0 - routing support)');
    }
    
    // ========================================================================
    // HANDLERS POUR ÉVÉNEMENTS FILEVIEW
    // ========================================================================
    
    /**
     * ✅ Handler: Demande liste fichiers
     */
    async handleListRequest(data) {
        const path = data?.path || this.state.currentPath;
        
        try {
            const files = await this.listFiles(path);
            
            // Émettre vers FileView
            this.eventBus.emit('files:list-updated', { 
                files,
                path 
            });
            
            return files;
        } catch (error) {
            this.log('error', 'FileController', 'handleListRequest failed:', error);
            this.eventBus.emit('files:error', { error: error.message });
            throw error;
        }
    }
    
    /**
     * ✅ Handler: Demande suppression fichier
     */
    async handleDeleteRequest(data) {
        const filePath = data?.file_path;
        
        if (!filePath) {
            this.log('error', 'FileController', 'handleDeleteRequest: missing file_path');
            return;
        }
        
        try {
            await this.deleteFile(filePath);
            
            // Émettre succès
            this.eventBus.emit('file:deleted', { 
                file_path: filePath 
            });
            
            if (this.notifications) {
                this.notifications.show('Supprimé', `${filePath} supprimé`, 'success', 2000);
            }
        } catch (error) {
            this.log('error', 'FileController', 'handleDeleteRequest failed:', error);
            this.eventBus.emit('files:error', { error: error.message });
            
            if (this.notifications) {
                this.notifications.show('Erreur', `Échec suppression: ${error.message}`, 'error', 3000);
            }
        }
    }
    
    /**
     * ✅ Handler: Demande lecture fichier
     */
    async handlePlayRequest(data) {
        const filePath = data?.file_path;
        
        if (!filePath) {
            this.log('error', 'FileController', 'handlePlayRequest: missing file_path');
            return;
        }
        
        try {
            // Charger le fichier dans le lecteur
            this.eventBus.emit('playback:load', { 
                file_path: filePath 
            });
            
            // Démarrer la lecture
            setTimeout(() => {
                this.eventBus.emit('playback:play');
            }, 100);
            
            this.log('info', 'FileController', `Playing: ${filePath}`);
            
            if (this.notifications) {
                this.notifications.show('Lecture', `Lecture de ${filePath}`, 'info', 2000);
            }
        } catch (error) {
            this.log('error', 'FileController', 'handlePlayRequest failed:', error);
            
            if (this.notifications) {
                this.notifications.show('Erreur', `Échec lecture: ${error.message}`, 'error', 3000);
            }
        }
    }
    
    /**
     * ✅ Handler: Charger dans l'éditeur
     */
    async handleLoadInEditor(data) {
        const filePath = data?.file_path;
        
        if (!filePath) {
            this.log('error', 'FileController', 'handleLoadInEditor: missing file_path');
            return;
        }
        
        try {
            // Charger le fichier MIDI
            const response = await this.backend.loadMidi(filePath);
            
            // Émettre vers EditorController
            this.eventBus.emit('editor:load_file', { 
                file_path: filePath,
                midi_json: response.midi_json || response.data
            });
            
            this.log('info', 'FileController', `Loading in editor: ${filePath}`);
            
            if (this.notifications) {
                this.notifications.show('Éditeur', `Chargement de ${filePath}`, 'info', 2000);
            }
        } catch (error) {
            this.log('error', 'FileController', 'handleLoadInEditor failed:', error);
            
            if (this.notifications) {
                this.notifications.show('Erreur', `Échec chargement: ${error.message}`, 'error', 3000);
            }
        }
    }
    
    /**
     * ✅ NOUVEAU: Handler: Charger pour routage
     */
    async handleLoadForRouting(data) {
        const filePath = data?.file_path;
        
        if (!filePath) {
            this.log('error', 'FileController', 'handleLoadForRouting: missing file_path');
            return;
        }
        
        try {
            // Charger le fichier MIDI
            const response = await this.backend.loadMidi(filePath);
            
            // Émettre vers RoutingController
            this.eventBus.emit('routing:load_file', { 
                file_path: filePath,
                midi_json: response.midi_json || response.data
            });
            
            this.log('info', 'FileController', `Loading for routing: ${filePath}`);
            
            if (this.notifications) {
                this.notifications.show('Routage', `Configuration du routage pour ${filePath}`, 'info', 2000);
            }
        } catch (error) {
            this.log('error', 'FileController', 'handleLoadForRouting failed:', error);
            
            if (this.notifications) {
                this.notifications.show('Erreur', `Échec chargement: ${error.message}`, 'error', 3000);
            }
        }
    }
    
    // ========================================================================
    // BACKEND EVENTS
    // ========================================================================
    
    async onBackendConnected() {
        this.log('info', 'FileController', '✓ Backend connected');
        
        // NE PAS charger automatiquement les fichiers au démarrage
        // Les fichiers seront chargés uniquement quand la page Files devient active
        this.log('info', 'FileController', 'Waiting for files page activation...');
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
    
    // ========================================================================
    // API FILES
    // ========================================================================
    
    /**
     * ✅ Liste fichiers - files.list
     */
    async listFiles(path = null) {
        return this.withBackend(
            async () => {
                const targetPath = path || this.state.currentPath;
                
                this.log('info', 'FileController', `Listing files in: ${targetPath}`);
                this.state.isLoading = true;
                
                const response = await this.backend.listFiles(targetPath);
                
                this.state.isLoading = false;
                
                // ✅ Extraction via response (BackendService fait déjà)
                const files = response.files || [];
                
                if (this.fileModel) {
                    this.fileModel.set('files', files);
                    this.fileModel.set('currentPath', targetPath);
                }
                
                this.state.currentPath = targetPath;
                this.state.lastRefresh = Date.now();
                
                this.log('info', 'FileController', `✅ Found ${files.length} files`);
                
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
     * ✅ Lit fichier - files.read
     */
    async readFile(filename) {
        return this.withBackend(
            async () => {
                this.log('info', 'FileController', `Reading file: ${filename}`);
                
                const content = await this.backend.readFile(filename);
                
                this.log('info', 'FileController', `✅ File read: ${filename}`);
                this.eventBus.emit('file:read-complete', { filename, content });
                
                return content;
            },
            'read file',
            null
        );
    }
    
    async selectFile(fileId) {
        const file = this.fileModel?.get('files')?.find(f => f.id === fileId || f.name === fileId);
        
        if (file) {
            this.state.selectedFile = file;
            this.eventBus.emit('file:selected', { file });
        }
    }
    
    async loadFile(fileId) {
        const file = this.state.selectedFile || 
                     this.fileModel?.get('files')?.find(f => f.id === fileId || f.name === fileId);
        
        if (file) {
            this.eventBus.emit('file:loaded', { file });
        }
    }
    
    async saveFile(fileId, content) {
        return this.withBackend(
            async () => {
                const filename = fileId;
                
                this.log('info', 'FileController', `Saving file: ${filename}`);
                
                await this.backend.writeFile(filename, content);
                
                this.log('info', 'FileController', `✅ File saved: ${filename}`);
                this.eventBus.emit('file:save-complete', { filename });
                
                await this.refreshFileList();
                
                return true;
            },
            'save file',
            false
        );
    }
    
    async deleteFile(filename) {
        return this.withBackend(
            async () => {
                if (this.config.confirmDelete) {
                    const confirmed = confirm(`Supprimer le fichier "${filename}" ?`);
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
                
                this.log('info', 'FileController', `✅ File deleted: ${filename}`);
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
     * ✅ CORRECTION: Upload via FileService (midi.import)
     */
    async handleFileUpload(file) {
        return this.withBackend(
            async () => {
                if (!file) {
                    throw new Error('No file provided');
                }
                
                this.log('info', 'FileController', `Uploading file: ${file.name}`);
                
                if (this.notifications) {
                    this.notifications.show('Upload', `Upload de ${file.name}...`, 'info', 2000);
                }
                
                // ✅ Utiliser FileService qui gère midi.import
                let result;
                if (this.fileService) {
                    result = await this.fileService.uploadFile(file);
                } else {
                    // Fallback direct via backend
                    result = await this.backend.uploadFile(file);
                }
                
                this.log('info', 'FileController', `✅ File uploaded: ${file.name}`);
                
                if (this.notifications) {
                    this.notifications.show('Upload Terminé', `${file.name} uploadé`, 'success', 3000);
                }
                
                this.eventBus.emit('file:upload-complete', { file, result });
                this.eventBus.emit('file:uploaded', { 
                    file_path: result.filepath || `/midi/${file.name}` 
                });
                
                await this.refreshFileList();
                
                return result;
            },
            'upload file',
            null
        ).catch(error => {
            this.log('error', 'FileController', 'Upload failed:', error);
            
            if (this.notifications && !error.offline) {
                this.notifications.show('Échec Upload', error.message, 'error', 5000);
            }
            
            throw error;
        });
    }
    
    async refreshFileList() {
        try {
            await this.listFiles();
            this.log('debug', 'FileController', 'File list refreshed');
        } catch (error) {
            if (!error.offline) {
                this.log('error', 'FileController', 'Failed to refresh file list:', error);
            }
        }
    }
    
    startAutoRefresh() {
        if (this.refreshTimer) {
            clearInterval(this.refreshTimer);
        }
        
        this.refreshTimer = setInterval(() => {
            this.refreshFileList();
        }, this.config.refreshInterval);
        
        this.log('debug', 'FileController', 'Auto-refresh started');
    }
    
    stopAutoRefresh() {
        if (this.refreshTimer) {
            clearInterval(this.refreshTimer);
            this.refreshTimer = null;
            this.log('debug', 'FileController', 'Auto-refresh stopped');
        }
    }
    
    destroy() {
        this.stopAutoRefresh();
        super.destroy?.();
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileController;
}
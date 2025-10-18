// ============================================================================
// Fichier: frontend/js/controllers/FileController.js
// Version: v3.0.2-COMPLETE
// Date: 2025-10-12
// ============================================================================
// Description:
//   Contrôleur de gestion des fichiers MIDI - Version complète et unifiée
//   Toutes les duplications supprimées, API cohérente
//
// Modifications v3.0.2:
//   ✅ Suppression duplications (refreshFileList, handleFileSelect, etc.)
//   ✅ Unification accès model (toujours via get/set)
//   ✅ Unification accès view (toujours via render)
//   ✅ uploadFile() corrigé pour utiliser files.upload (base64)
//   ✅ Gestion événements backend complète
//   ✅ Méthodes helpers unifiées
//   ✅ Documentation complète
//
// API Publique:
//   - refreshFileList()      : Rafraîchir la liste
//   - uploadFile(file)       : Upload fichier MIDI
//   - deleteFile(fileId)     : Supprimer avec confirmation
//   - renameFile(fileId, name) : Renommer fichier
//   - moveFile(fileId, path) : Déplacer fichier
//   - selectFile(fileId)     : Sélectionner fichier
//   - loadFile(fileId)       : Charger pour lecture
//
// Auteur: MidiMind Team
// ============================================================================

class FileController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Services
        this.backend = window.app?.services?.backend || null;
        this.logger = window.Logger || console;
        
        // État
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
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        this.logger.info('FileController', '📁 Initializing...');
        
        // Vérifier dépendances
        if (!this.backend) {
            this.logger.error('FileController', 'BackendService not available');
            return;
        }
        
        // Setup événements
        this.setupEventListeners();
        
        // Charger liste initiale
        setTimeout(() => {
            this.refreshFileList();
        }, 1000);
        
        this.logger.info('FileController', '✓ Initialized');
    }
    
    setupEventListeners() {
        // Événements UI
        this.eventBus.on('file:select', (data) => this.handleFileSelect(data));
        this.eventBus.on('file:load', (data) => this.handleFileLoad(data));
        this.eventBus.on('file:upload', (data) => this.handleFileUpload(data));
        this.eventBus.on('file:delete', (data) => this.handleFileDelete(data));
        this.eventBus.on('file:rename', (data) => this.handleFileRename(data));
        this.eventBus.on('file:refresh', () => this.refreshFileList());
        
        // Événements backend
        this.eventBus.on('backend:event', (event) => this.handleBackendEvent(event));
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
    }
    
    // ========================================================================
    // GESTION LISTE DE FICHIERS
    // ========================================================================
    
    /**
     * Rafraîchit la liste de fichiers depuis le backend
     * @returns {Promise<Array>} Liste des fichiers
     */
    async refreshFileList() {
        this.logger.info('FileController', 'Refreshing file list...');
        
        if (this.state.isLoading) {
            this.logger.warn('FileController', 'Already loading, skip');
            return;
        }
        
        this.state.isLoading = true;
        
        try {
            // Afficher loader
            this.showLoading(true);
            
            // Demander liste au backend
            const result = await this.backend.sendCommand('files.list', {
                directory: this.state.currentDirectory,
                recursive: true
            });
            
            // Vérifier succès
            if (result.success === false) {
                throw new Error(result.error || 'Failed to load file list');
            }
            
            // Extraire fichiers (compatibilité avec différentes structures)
            const files = result.data?.files || result.files || [];
            
            this.logger.info('FileController', `✓ Loaded ${files.length} files`);
            
            // Mettre à jour modèle
            const model = this.getModel('file');
            if (model) {
                model.set('files', files);
                model.set('directory', this.state.currentDirectory);
                model.set('lastRefresh', Date.now());
            }
            
            // Mettre à jour vue
            this.updateView('file', {
                files: files,
                directory: this.state.currentDirectory,
                count: files.length
            });
            
            // État
            this.state.lastRefresh = Date.now();
            
            // Émettre événement
            this.eventBus.emit('files:loaded', { 
                files, 
                count: files.length,
                directory: this.state.currentDirectory
            });
            
            return files;
            
        } catch (error) {
            this.logger.error('FileController', 'Failed to refresh file list:', error);
            
            this.showNotification(
                `Failed to load files: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            throw error;
            
        } finally {
            this.state.isLoading = false;
            this.showLoading(false);
        }
    }
    
    // ========================================================================
    // UPLOAD FICHIER
    // ========================================================================
    
    /**
     * Upload un fichier MIDI avec validation et progress
     * @param {File} file - Fichier à uploader
     * @param {Function} onProgress - Callback progression (0-100)
     * @returns {Promise<Object>} Fichier uploadé
     */
    async uploadFile(file, onProgress = null) {
        this.logger.info('FileController', `Uploading file: ${file.name}`);
        
        try {
            // ============================================================
            // VALIDATION CLIENT
            // ============================================================
            
            // 1. Extension
            const ext = file.name.toLowerCase().slice(-4);
            if (!this.config.allowedExtensions.some(e => ext.endsWith(e))) {
                throw new Error('Invalid file type. Only .mid or .midi files are allowed.');
            }
            
            // 2. Taille
            if (file.size > this.config.maxFileSize) {
                const maxMB = (this.config.maxFileSize / (1024 * 1024)).toFixed(1);
                throw new Error(`File too large. Maximum size is ${maxMB} MB.`);
            }
            
            // 3. Lecture fichier
            if (onProgress) onProgress(10);
            
            const arrayBuffer = await file.arrayBuffer();
            const uint8Array = new Uint8Array(arrayBuffer);
            
            if (onProgress) onProgress(30);
            
            // 4. Vérifier header MIDI (MThd)
            if (uint8Array.length < 4 || 
                String.fromCharCode(...uint8Array.slice(0, 4)) !== 'MThd') {
                throw new Error('Invalid MIDI file format (missing MThd header)');
            }
            
            // ============================================================
            // CONVERSION BASE64
            // ============================================================
            
            if (onProgress) onProgress(40);
            
            const base64 = btoa(String.fromCharCode.apply(null, uint8Array));
            
            if (onProgress) onProgress(60);
            
            this.logger.debug('FileController', 
                `File converted to base64 (${base64.length} chars)`);
            
            // ============================================================
            // ENVOI BACKEND
            // ============================================================
            
            const result = await this.backend.sendCommand('files.upload', {
                filename: file.name,
                data: base64,
                size: file.size,
                directory: this.state.currentDirectory,
                overwrite: false
            });
            
            if (onProgress) onProgress(90);
            
            // Vérifier succès
            if (result.success === false) {
                throw new Error(result.error || 'Upload failed');
            }
            
            const uploadedFile = result.data || result;
            
            this.logger.info('FileController', 
                `✓ File uploaded: ${file.name} (ID: ${uploadedFile.file_id})`);
            
            // ============================================================
            // MISE À JOUR
            // ============================================================
            
            // Rafraîchir liste
            await this.refreshFileList();
            
            if (onProgress) onProgress(100);
            
            // Notification succès
            this.showNotification(
                `File uploaded: ${file.name}`,
                'success',
                { duration: 3000 }
            );
            
            // Émettre événement
            this.eventBus.emit('file:uploaded', { 
                file: uploadedFile,
                originalFile: file
            });
            
            return uploadedFile;
            
        } catch (error) {
            this.logger.error('FileController', 'Upload failed:', error);
            
            this.showNotification(
                `Upload failed: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            throw error;
        }
    }
    
    /**
     * Handler pour événement file:upload
     * @private
     */
    async handleFileUpload(data) {
        const { file, onProgress } = data;
        
        if (!file) {
            this.logger.error('FileController', 'No file provided for upload');
            return;
        }
        
        try {
            await this.uploadFile(file, onProgress);
        } catch (error) {
            // Erreur déjà gérée dans uploadFile()
        }
    }
    
    // ========================================================================
    // SUPPRESSION FICHIER
    // ========================================================================
    
    /**
     * Supprime un fichier avec confirmation
     * @param {string} fileId - ID ou path du fichier
     * @returns {Promise<boolean>} Succès
     */
    async deleteFile(fileId) {
        this.logger.info('FileController', `Deleting file: ${fileId}`);
        
        try {
            // Récupérer info fichier
            const file = this.getFileById(fileId);
            const fileName = file?.filename || file?.name || fileId;
            
            // Confirmation utilisateur (si activée)
            if (this.config.confirmDelete) {
                const confirmed = confirm(
                    `Delete file "${fileName}"?\n\n` +
                    `This action cannot be undone.`
                );
                
                if (!confirmed) {
                    this.logger.info('FileController', 'Delete cancelled by user');
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
            
            this.logger.info('FileController', `✓ File deleted: ${fileName}`);
            
            // Si fichier sélectionné, désélectionner
            if (this.state.selectedFile === fileId) {
                this.state.selectedFile = null;
                this.eventBus.emit('file:deselected');
            }
            
            // Rafraîchir liste
            await this.refreshFileList();
            
            // Notification succès
            this.showNotification(
                `File deleted: ${fileName}`,
                'info',
                { duration: 3000 }
            );
            
            // Émettre événement
            this.eventBus.emit('file:deleted', { 
                fileId,
                file
            });
            
            return true;
            
        } catch (error) {
            this.logger.error('FileController', 'Delete failed:', error);
            
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
     * Handler pour événement file:delete
     * @private
     */
    async handleFileDelete(data) {
        const fileId = data.fileId || data.id || data.file_id || data.filePath;
        
        if (!fileId) {
            this.logger.error('FileController', 'No file ID provided for delete');
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
     * @returns {Promise<Object>} Fichier renommé
     */
    async renameFile(fileId, newName) {
        this.logger.info('FileController', `Renaming file: ${fileId} -> ${newName}`);
        
        try {
            // Validation
            if (!newName || newName.trim().length === 0) {
                throw new Error('Invalid file name');
            }
            
            // Ajouter extension si absente
            if (!this.config.allowedExtensions.some(ext => 
                newName.toLowerCase().endsWith(ext))) {
                newName += '.mid';
            }
            
            // Renommer via backend
            const result = await this.backend.sendCommand('files.rename', {
                file_id: fileId,
                new_name: newName
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Rename failed');
            }
            
            this.logger.info('FileController', `✓ File renamed to: ${newName}`);
            
            // Rafraîchir liste
            await this.refreshFileList();
            
            // Notification succès
            this.showNotification(
                `File renamed to: ${newName}`,
                'success',
                { duration: 3000 }
            );
            
            // Émettre événement
            this.eventBus.emit('file:renamed', {
                fileId,
                oldName: result.data?.old_name,
                newName: newName,
                newPath: result.data?.new_path
            });
            
            return result.data;
            
        } catch (error) {
            this.logger.error('FileController', 'Rename failed:', error);
            
            this.showNotification(
                `Failed to rename file: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            throw error;
        }
    }
    
    /**
     * Handler pour événement file:rename
     * @private
     */
    async handleFileRename(data) {
        const { fileId, newName } = data;
        
        if (!fileId || !newName) {
            this.logger.error('FileController', 'Missing fileId or newName for rename');
            return;
        }
        
        try {
            await this.renameFile(fileId, newName);
        } catch (error) {
            // Erreur déjà gérée dans renameFile()
        }
    }
    
    // ========================================================================
    // DÉPLACER FICHIER
    // ========================================================================
    
    /**
     * Déplace un fichier vers un nouveau répertoire
     * @param {string} fileId - ID du fichier
     * @param {string} destination - Chemin destination
     * @returns {Promise<Object>} Fichier déplacé
     */
    async moveFile(fileId, destination) {
        this.logger.info('FileController', `Moving file: ${fileId} -> ${destination}`);
        
        try {
            const result = await this.backend.sendCommand('files.move', {
                file_id: fileId,
                destination: destination
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Move failed');
            }
            
            this.logger.info('FileController', '✓ File moved successfully');
            
            // Rafraîchir liste
            await this.refreshFileList();
            
            // Notification succès
            this.showNotification(
                'File moved successfully',
                'success',
                { duration: 3000 }
            );
            
            // Émettre événement
            this.eventBus.emit('file:moved', {
                fileId,
                destination,
                newPath: result.data?.new_path
            });
            
            return result.data;
            
        } catch (error) {
            this.logger.error('FileController', 'Move failed:', error);
            
            this.showNotification(
                `Failed to move file: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            throw error;
        }
    }
    
    // ========================================================================
    // SÉLECTION ET CHARGEMENT
    // ========================================================================
    
    /**
     * Sélectionne un fichier
     * @param {string} fileId - ID du fichier
     */
    selectFile(fileId) {
        this.logger.info('FileController', `Selecting file: ${fileId}`);
        
        this.state.selectedFile = fileId;
        
        // Récupérer info complète
        const file = this.getFileById(fileId);
        
        // Mettre à jour modèle
        const model = this.getModel('file');
        if (model) {
            model.set('selectedFile', fileId);
        }
        
        // Émettre événement
        this.eventBus.emit('file:selected', {
            fileId,
            file
        });
    }
    
    /**
     * Handler pour événement file:select
     * @private
     */
    handleFileSelect(data) {
        const fileId = data.fileId || data.id || data.file_id;
        
        if (!fileId) {
            this.logger.warn('FileController', 'No file ID provided for select');
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
        this.logger.info('FileController', `Loading file: ${fileId}`);
        
        try {
            // Vérifier GlobalPlaybackController disponible
            if (!window.globalPlaybackController) {
                throw new Error('GlobalPlaybackController not available');
            }
            
            // Charger via playback controller
            await window.globalPlaybackController.loadFile(fileId);
            
            // Récupérer nom fichier
            const file = this.getFileById(fileId);
            const fileName = file?.filename || file?.name || 'File';
            
            // Notification succès
            this.showNotification(
                `${fileName} loaded`,
                'success',
                { duration: 2000 }
            );
            
            this.logger.info('FileController', '✓ File loaded successfully');
            
        } catch (error) {
            this.logger.error('FileController', 'Failed to load file:', error);
            
            this.showNotification(
                `Failed to load file: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            throw error;
        }
    }
    
    /**
     * Handler pour événement file:load
     * @private
     */
    async handleFileLoad(data) {
        const fileId = data.fileId || data.id || data.file_id || data.filePath;
        
        if (!fileId) {
            this.logger.warn('FileController', 'No file ID provided for load');
            return;
        }
        
        try {
            await this.loadFile(fileId);
        } catch (error) {
            // Erreur déjà gérée dans loadFile()
        }
    }
    
    // ========================================================================
    // ÉVÉNEMENTS BACKEND
    // ========================================================================
    
    /**
     * Gère les événements reçus du backend
     * @param {Object} event - Événement backend
     */
    handleBackendEvent(event) {
        if (!event || !event.name) {
            this.logger.warn('FileController', 'Invalid backend event received');
            return;
        }
        
        this.logger.debug('FileController', `Backend event: ${event.name}`);
        
        switch (event.name) {
            case 'files:list':
                this.handleFilesListUpdate(event.data);
                break;
                
            case 'files:added':
            case 'file:added':
                this.handleFileAdded(event.data);
                break;
                
            case 'files:deleted':
            case 'file:deleted':
                this.handleFileDeleted(event.data);
                break;
                
            case 'files:renamed':
            case 'file:renamed':
                this.handleFileRenamed(event.data);
                break;
                
            case 'files:updated':
            case 'file:updated':
                this.handleFileUpdated(event.data);
                break;
                
            case 'files:scan:complete':
                this.handleScanComplete(event.data);
                break;
                
            case 'files:scan:progress':
                this.handleScanProgress(event.data);
                break;
                
            default:
                this.logger.debug('FileController', `Unhandled event: ${event.name}`);
        }
    }
    
    handleFilesListUpdate(data) {
        this.logger.info('FileController', `Files list updated (${data.count} files)`);
        
        const model = this.getModel('file');
        if (model) {
            model.set('files', data.files || []);
            model.set('directory', data.directory);
            model.set('lastScan', Date.now());
        }
        
        this.updateView('file', {
            files: data.files || [],
            directory: data.directory,
            count: data.count
        });
        
        this.eventBus.emit('files:refreshed', data);
    }
    
    handleFileAdded(data) {
        this.logger.info('FileController', `File added: ${data.filename || data.name}`);
        
        // Rafraîchir liste complète
        this.refreshFileList();
        
        // Notification
        this.showNotification(
            `File "${data.filename || data.name}" added`,
            'success',
            { duration: 3000 }
        );
    }
    
    handleFileDeleted(data) {
        this.logger.info('FileController', `File deleted: ${data.fileId || data.id}`);
        
        const fileId = data.fileId || data.id;
        
        // Si fichier sélectionné, désélectionner
        if (this.state.selectedFile === fileId) {
            this.state.selectedFile = null;
            this.eventBus.emit('file:deselected');
        }
        
        // Rafraîchir liste
        this.refreshFileList();
    }
    
    handleFileRenamed(data) {
        this.logger.info('FileController', `File renamed: ${data.oldId} -> ${data.newId}`);
        
        // Si fichier sélectionné, mettre à jour ID
        if (this.state.selectedFile === data.oldId) {
            this.state.selectedFile = data.newId;
        }
        
        // Rafraîchir liste
        this.refreshFileList();
        
        // Notification
        this.showNotification(
            'File renamed successfully',
            'success',
            { duration: 3000 }
        );
    }
    
    handleFileUpdated(data) {
        this.logger.info('FileController', `File updated: ${data.id}`);
        
        // Rafraîchir liste
        this.refreshFileList();
    }
    
    handleScanComplete(data) {
        this.logger.info('FileController', 
            `Scan complete: ${data.filesFound} files in ${data.duration}ms`);
        
        // Notification
        this.showNotification(
            `Scan complete: ${data.filesFound} files found`,
            'success',
            { duration: 3000 }
        );
        
        this.eventBus.emit('file:scan:complete', data);
    }
    
    handleScanProgress(data) {
        this.logger.debug('FileController', 
            `Scan progress: ${data.progress}% (${data.filesScanned} files)`);
        
        this.eventBus.emit('file:scan:progress', data);
    }
    
    onBackendConnected() {
        this.logger.info('FileController', 'Backend connected, refreshing file list');
        
        if (this.config.autoRefresh) {
            this.refreshFileList();
        }
    }
    
    onBackendDisconnected() {
        this.logger.warn('FileController', 'Backend disconnected');
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * Récupère un fichier par son ID
     * @param {string} fileId - ID du fichier
     * @returns {Object|null} Fichier ou null
     */
    getFileById(fileId) {
        const model = this.getModel('file');
        if (!model) return null;
        
        const files = model.get('files') || [];
        return files.find(f => 
            f.id === fileId || 
            f.path === fileId || 
            f.filepath === fileId
        ) || null;
    }
    
    /**
     * Affiche/masque le loader
     * @param {boolean} show - Afficher ou masquer
     */
    showLoading(show) {
        const view = this.getView('file');
        if (view && typeof view.showLoading === 'function') {
            view.showLoading(show);
        }
    }
    
    /**
     * Met à jour une vue
     * @param {string} viewName - Nom de la vue
     * @param {Object} data - Données à afficher
     */
    updateView(viewName, data) {
        const view = this.getView(viewName);
        if (view && typeof view.render === 'function') {
            view.render(data);
        }
    }
    
    // ========================================================================
    // API PUBLIQUE
    // ========================================================================
    
    /**
     * Récupère le répertoire courant
     * @returns {string} Chemin du répertoire
     */
    getCurrentDirectory() {
        return this.state.currentDirectory;
    }
    
    /**
     * Récupère le fichier sélectionné
     * @returns {string|null} ID du fichier sélectionné
     */
    getSelectedFile() {
        return this.state.selectedFile;
    }
    
    /**
     * Change de répertoire
     * @param {string} directory - Nouveau répertoire
     * @returns {Promise<void>}
     */
    async changeDirectory(directory) {
        this.logger.info('FileController', `Changing directory: ${directory}`);
        
        this.state.currentDirectory = directory;
        await this.refreshFileList();
    }
    
    /**
     * Scan le répertoire pour nouveaux fichiers
     * @returns {Promise<Object>} Résultat du scan
     */
    async scanDirectory() {
        this.logger.info('FileController', 'Scanning directory...');
        
        try {
            const result = await this.backend.sendCommand('files.scan', {
                directory: this.state.currentDirectory,
                recursive: true
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Scan failed');
            }
            
            this.logger.info('FileController', 
                `✓ Scan complete: ${result.data.files_found} files found`);
            
            // Rafraîchir liste
            await this.refreshFileList();
            
            return result.data;
            
        } catch (error) {
            this.logger.error('FileController', 'Scan failed:', error);
            
            this.showNotification(
                `Scan failed: ${error.message}`,
                'error',
                { duration: 5000 }
            );
            
            throw error;
        }
    }
    
    /**
     * Récupère les métadonnées d'un fichier
     * @param {string} fileId - ID du fichier
     * @returns {Promise<Object>} Métadonnées
     */
    async getFileMetadata(fileId) {
        this.logger.info('FileController', `Getting metadata for: ${fileId}`);
        
        try {
            const result = await this.backend.sendCommand('files.getMetadata', {
                file_id: fileId
            });
            
            if (result.success === false) {
                throw new Error(result.error || 'Failed to get metadata');
            }
            
            return result.data;
            
        } catch (error) {
            this.logger.error('FileController', 'Failed to get metadata:', error);
            throw error;
        }
    }
    
    /**
     * Récupère l'état du contrôleur
     * @returns {Object} État
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

// ============================================================================
// FIN DU FICHIER FileController.js v3.0.2-COMPLETE
// ============================================================================
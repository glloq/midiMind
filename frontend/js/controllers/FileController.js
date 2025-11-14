// ============================================================================
// Fichier: frontend/js/controllers/FileController.js
// Version: v4.5.0 - ENRICHMENT METADATA + DURATION
// Date: 2025-11-13
// ============================================================================
// CORRECTIONS v4.5.0:
// ✅ NOUVEAU: enrichFilesWithMetadata() - Récupère durée/pistes via midi.load
// ✅ listFiles() accepte maintenant enrichMetadata=true par défaut
// ✅ Affiche durée et nombre de pistes pour chaque fichier
// ============================================================================

class FileController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);

        this.logger = window.logger || console;
        this.fileModel = models.file;
        this.view = views.file;
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
            refreshInterval: 30000,
            enrichMetadata: true  // ✅ NOUVEAU: Activer enrichissement par défaut
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

        // Nouveaux événements depuis FileView
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

        this.log('info', 'FileController', '✅ Events bound (v4.5.0)');
    }

    // ========================================================================
    // BACKEND EVENTS
    // ========================================================================

    async onBackendConnected() {
        this.log('info', 'FileController', '✅ Backend connected');
        this.log('info', 'FileController', 'Waiting for files page activation...');
    }

    onBackendDisconnected() {
        this.stopAutoRefresh();
        this.log('warn', 'FileController', '⚠️ Backend disconnected');
    }

    onFilesPageActive() {
        this.refreshFileList().catch(error => {
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
     * @param {string|null} path - Chemin à lister
     * @param {boolean} enrichMetadata - Si true, enrichit avec durée/pistes via midi.load
     */
    async listFiles(path = null, enrichMetadata = null) {
        // Utiliser config si non spécifié
        if (enrichMetadata === null) {
            enrichMetadata = this.config.enrichMetadata;
        }

        return this.withBackend(
            async () => {
                const targetPath = path || this.state.currentPath;

                this.log('info', 'FileController', `Listing files in: ${targetPath}`);
                this.state.isLoading = true;

                const response = await this.backend.listFiles(targetPath);

                this.state.isLoading = false;

                // Extraction via response
                let files = response.files || [];

                // ✅ NOUVEAU: Enrichir avec métadonnées MIDI (durée, pistes)
                if (enrichMetadata && files.length > 0) {
                    this.log('info', 'FileController', `Enriching ${files.length} files with metadata...`);
                    files = await this.enrichFilesWithMetadata(files);
                }

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
            []
        ).catch(error => {
            this.state.isLoading = false;
            if (!error.offline) {
                this.log('error', 'FileController', 'listFiles failed:', error);
            }
            throw error;
        });
    }

    /**
     * ✅ NOUVEAU v4.5.0: Enrichit les fichiers avec métadonnées MIDI
     * ✅ FIX Bug #5: Parallélise le chargement avec batching
     * Appelle midi.load pour chaque fichier .mid/.midi
     * @param {Array} files - Liste des fichiers
     * @returns {Promise<Array>} Fichiers enrichis
     */
    async enrichFilesWithMetadata(files) {
        const BATCH_SIZE = 5; // ✅ FIX Bug #5: Limite de 5 requêtes simultanées

        // Séparer les fichiers MIDI des autres
        const midiFiles = [];
        const enrichedFiles = [];

        files.forEach((file, index) => {
            const isMidiFile = file.name && (
                file.name.toLowerCase().endsWith('.mid') ||
                file.name.toLowerCase().endsWith('.midi')
            );

            if (isMidiFile) {
                midiFiles.push({ ...file, originalIndex: index });
            } else {
                enrichedFiles[index] = { ...file };
            }
        });

        // ✅ FIX Bug #5: Traiter par batches parallèles
        for (let i = 0; i < midiFiles.length; i += BATCH_SIZE) {
            const batch = midiFiles.slice(i, i + BATCH_SIZE);

            const batchResults = await Promise.all(batch.map(async (file) => {
                const enrichedFile = { ...file };

                try {
                    const filePath = file.path || file.name;
                    this.log('debug', 'FileController', `Enriching metadata for: ${filePath}`);
                    const midiData = await this.backend.loadMidi(filePath);

                    if (midiData && midiData.midi_json) {
                        const json = midiData.midi_json;

                        enrichedFile.duration = json.duration || 0;
                        enrichedFile.tracks = json.tracks ? json.tracks.length : 0;
                        enrichedFile.tempo = json.tempo || 120;
                        enrichedFile.timeSignature = json.timeSignature || '4/4';

                        this.log('debug', 'FileController',
                            `Enriched ${file.name}: ${enrichedFile.duration}s, ${enrichedFile.tracks} tracks`);
                    }
                } catch (error) {
                    this.log('warn', 'FileController',
                        `Failed to enrich ${file.name}: ${error.message || error}`);
                }

                return enrichedFile;
            }));

            // Réinsérer les fichiers enrichis aux bons indices
            batchResults.forEach((enrichedFile) => {
                enrichedFiles[enrichedFile.originalIndex] = enrichedFile;
            });
        }

        // Retourner le tableau complet dans l'ordre original
        return enrichedFiles.filter(f => f !== undefined);
    }

    /**
     * Lit fichier - files.read
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

    /**
     * Écrit fichier - files.write
     */
    async writeFile(filename, content) {
        return this.withBackend(
            async () => {
                this.log('info', 'FileController', `Writing file: ${filename}`);

                await this.backend.writeFile(filename, content);

                this.log('info', 'FileController', `✅ File written: ${filename}`);
                this.eventBus.emit('file:write-complete', { filename });

                await this.refreshFileList();

                return true;
            },
            'write file',
            false
        );
    }

    /**
     * Supprime fichier - files.delete
     */
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
     * Upload via FileService (midi.import)
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

                let result;
                if (this.fileService) {
                    result = await this.fileService.uploadFile(file);
                } else {
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

    // ========================================================================
    // NOUVEAUX HANDLERS POUR ÉVÉNEMENTS FILEVIEW
    // ========================================================================

    /**
     * Handler: Demande liste fichiers
     */
    async handleListRequest(data) {
        const path = data?.path || this.state.currentPath;

        try {
            const files = await this.listFiles(path);

            this.eventBus.emit('files:listUpdated', {
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
     * Handler: Demande suppression fichier
     */
    async handleDeleteRequest(data) {
        const filePath = data?.file_path;

        if (!filePath) {
            this.log('error', 'FileController', 'handleDeleteRequest: missing file_path');
            return;
        }

        try {
            await this.deleteFile(filePath);

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
     * Handler: Demande lecture fichier
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
     * Handler: Charger dans l'éditeur
     */
    async handleLoadInEditor(data) {
        const filePath = data?.file_path;

        if (!filePath) {
            this.log('error', 'FileController', 'handleLoadInEditor: missing file_path');
            return;
        }

        this.log('debug', 'FileController', `handleLoadInEditor called with: ${filePath}`);

        try {
            const response = await this.backend.loadMidi(filePath);

            this.eventBus.emit('editor:fileLoaded', {
                file_path: filePath,
                midi_json: response.midi_json || response.data
            });

            this.log('info', 'FileController', `Successfully loaded in editor: ${filePath}`);

            if (this.notifications) {
                this.notifications.show('Éditeur', `Chargement de ${filePath}`, 'info', 2000);
            }
        } catch (error) {
            const errorDetails = {
                message: error?.message || error?.error || 'Unknown error',
                code: error?.code,
                details: error?.details,
                filepath: filePath,
                stack: error?.stack
            };
            this.log('error', 'FileController', 'handleLoadInEditor failed:', errorDetails);

            if (this.notifications) {
                const errorMsg = error.message || 'Unknown error';
                this.notifications.show('Erreur', `Échec chargement: ${errorMsg}`, 'error', 5000);
            }
        }
    }

    /**
     * Handler: Charger pour routage
     */
    async handleLoadForRouting(data) {
        const filePath = data?.file_path;

        if (!filePath) {
            this.log('error', 'FileController', 'handleLoadForRouting: missing file_path');
            return;
        }

        this.log('debug', 'FileController', `handleLoadForRouting called with: ${filePath}`);

        try {
            const response = await this.backend.loadMidi(filePath);

            this.eventBus.emit('routing:fileLoaded', {
                file_path: filePath,
                midi_json: response.midi_json || response.data
            });

            this.log('info', 'FileController', `Successfully loaded for routing: ${filePath}`);

            if (this.notifications) {
                this.notifications.show('Routage', `Configuration du routage pour ${filePath}`, 'info', 2000);
            }
        } catch (error) {
            const errorDetails = {
                message: error?.message || error?.error || 'Unknown error',
                code: error?.code,
                details: error?.details,
                filepath: filePath,
                stack: error?.stack
            };
            this.log('error', 'FileController', 'handleLoadForRouting failed:', errorDetails);

            if (this.notifications) {
                const errorMsg = error.message || 'Unknown error';
                this.notifications.show('Erreur', `Échec chargement: ${errorMsg}`, 'error', 5000);
            }
        }
    }

    async refreshFileList() {
        try {
            return await this.listFiles();
        } catch (error) {
            if (!error.offline) {
                this.log('error', 'FileController', 'refreshFileList failed:', error);
                if (this.notifications) {
                    this.notifications.show('Erreur', 'Échec actualisation liste', 'error', 3000);
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
                this.notifications.show('Fichier Chargé', `${fileId} chargé`, 'success', 2000);
            }

            return content;
        } catch (error) {
            if (!error.offline) {
                this.log('error', 'FileController', 'loadFile failed:', error);
                if (this.notifications) {
                    this.notifications.show('Erreur', `Échec chargement: ${error.message}`, 'error', 3000);
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
                this.notifications.show('Fichier Sauvegardé', `${fileId} sauvegardé`, 'success', 2000);
            }

            return true;
        } catch (error) {
            if (!error.offline) {
                this.log('error', 'FileController', 'saveFile failed:', error);
                if (this.notifications) {
                    this.notifications.show('Erreur', `Échec sauvegarde: ${error.message}`, 'error', 3000);
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

        if (!this.isBackendReady()) {
            this.log('info', 'FileController', 'Auto-refresh skipped - backend not ready');
            return;
        }

        this.log('info', 'FileController', 'Starting auto-refresh...');

        this.refreshTimer = setInterval(() => {
            if (!this.isBackendReady()) {
                this.stopAutoRefresh();
                return;
            }

            this.refreshFileList().catch(err => {
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

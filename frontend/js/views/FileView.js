// ============================================================================
// Fichier: frontend/js/views/FileView.js
// Version: v3.9.0 - CORRECTIONS INTERFACE
// Date: 2025-10-30
// Projet: MidiMind v3.1
// ============================================================================
// CORRECTIONS v3.9.0:
// ✅ Bouton Upload ajouté dans la section fichiers
// ✅ Gestion de l'événement upload
// ============================================================================

class FileView {
    constructor(container, eventBus) {
        // Container
        if (typeof container === 'string') {
            this.container = document.getElementById(container) || document.querySelector(container);
        } else if (container instanceof HTMLElement) {
            this.container = container;
        } else {
            this.container = null;
        }
        
        if (!this.container) {
            console.error('[FileView] Container not found:', container);
        }
        
        this.eventBus = eventBus;
        this.logger = window.logger || console;
        
        // État
        this.state = {
            files: [],
            playlists: [],
            selectedFile: null,
            selectedPlaylist: null
        };
        
        // Éléments DOM
        this.elements = {};
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    init() {
        if (!this.container) {
            this.logger.error('[FileView] Cannot initialize: container not found');
            return;
        }
        
        this.render();
        this.cacheElements();
        this.attachEvents();
        this.loadData();
        
        this.logger.info('[FileView] Initialized');
    }

    render() {
        if (!this.container) return;
        
        this.container.innerHTML = `
            <div class="page-header">
                <h1>📁 Gestion des Fichiers</h1>
            </div>
            
            <div class="files-layout">
                <!-- Section Fichiers -->
                <div class="files-section">
                    <div class="section-header">
                        <h2>Fichiers MIDI JSON</h2>
                        <div class="section-actions">
                            <button class="btn-action btn-primary" id="btnUploadFile">
                                ⬆️ Uploader
                            </button>
                            <button class="btn-action" id="btnRefreshFiles">
                                🔄 Actualiser
                            </button>
                        </div>
                    </div>
                    
                    <div class="files-grid" id="filesGrid">
                        ${this.renderEmptyFiles()}
                    </div>
                </div>
                
                <!-- Section Playlists -->
                <div class="playlists-section">
                    <div class="section-header">
                        <h2>Playlists</h2>
                        <button class="btn-action btn-create" id="btnCreatePlaylist">
                            ➕ Nouvelle Playlist
                        </button>
                    </div>
                    
                    <div class="playlists-grid" id="playlistsGrid">
                        ${this.renderEmptyPlaylists()}
                    </div>
                </div>
            </div>
        `;
    }

    cacheElements() {
        this.elements = {
            filesGrid: document.getElementById('filesGrid'),
            playlistsGrid: document.getElementById('playlistsGrid'),
            btnUploadFile: document.getElementById('btnUploadFile'),
            btnRefreshFiles: document.getElementById('btnRefreshFiles'),
            btnCreatePlaylist: document.getElementById('btnCreatePlaylist')
        };
    }

    attachEvents() {
        // Boutons
        if (this.elements.btnUploadFile) {
            this.elements.btnUploadFile.addEventListener('click', () => this.uploadFile());
        }
        if (this.elements.btnRefreshFiles) {
            this.elements.btnRefreshFiles.addEventListener('click', () => this.refreshFiles());
        }
        if (this.elements.btnCreatePlaylist) {
            this.elements.btnCreatePlaylist.addEventListener('click', () => this.createPlaylist());
        }
        
        // Délégation d'événements
        if (this.elements.filesGrid) {
            this.elements.filesGrid.addEventListener('click', (e) => this.handleFileAction(e));
        }
        if (this.elements.playlistsGrid) {
            this.elements.playlistsGrid.addEventListener('click', (e) => this.handlePlaylistAction(e));
        }
        
        // EventBus
        this.setupEventBusListeners();
    }

    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        this.eventBus.on('files:loaded', (data) => {
            this.state.files = data.files || [];
            this.renderFilesGrid();
        });
        
        this.eventBus.on('playlists:loaded', (data) => {
            this.state.playlists = data.playlists || [];
            this.renderPlaylistsGrid();
        });
        
        this.eventBus.on('file:updated', () => {
            this.refreshFiles();
        });
        
        this.eventBus.on('playlist:updated', () => {
            this.refreshPlaylists();
        });
    }

    // ========================================================================
    // RENDU DES FICHIERS
    // ========================================================================

    renderFilesGrid() {
        if (!this.elements.filesGrid) return;
        
        if (!this.state.files || this.state.files.length === 0) {
            this.elements.filesGrid.innerHTML = this.renderEmptyFiles();
            return;
        }
        
        this.elements.filesGrid.innerHTML = this.state.files
            .map(file => this.renderFileCard(file))
            .join('');
    }

    renderFileCard(file) {
        const duration = this.formatDuration(file.duration || 0);
        const tracks = file.tracks || 0;
        const notes = file.noteCount || 0;
        
        return `
            <div class="file-card" data-file-id="${file.id}">
                <div class="file-card-header">
                    <div class="file-card-icon">🎵</div>
                    <div class="file-card-info">
                        <div class="file-card-name">${file.name || 'Sans nom'}</div>
                        <div class="file-card-meta">
                            <span>⏱️ ${duration}</span>
                            <span>🎹 ${tracks} pistes</span>
                            <span>🎼 ${notes} notes</span>
                        </div>
                    </div>
                </div>
                
                <div class="file-card-actions">
                    <button class="file-card-btn btn-edit" data-action="edit" data-file-id="${file.id}">
                        <span>✏️</span>
                        <span>Éditer</span>
                    </button>
                    <button class="file-card-btn btn-routes" data-action="routes" data-file-id="${file.id}">
                        <span>🔀</span>
                        <span>Routes</span>
                    </button>
                </div>
            </div>
        `;
    }

    renderEmptyFiles() {
        return `
            <div class="empty-state">
                <div class="empty-state-icon">🎵</div>
                <div class="empty-state-text">Aucun fichier MIDI</div>
                <div class="empty-state-hint">Les fichiers MIDI JSON apparaîtront ici</div>
            </div>
        `;
    }

    // ========================================================================
    // RENDU DES PLAYLISTS
    // ========================================================================

    renderPlaylistsGrid() {
        if (!this.elements.playlistsGrid) return;
        
        if (!this.state.playlists || this.state.playlists.length === 0) {
            this.elements.playlistsGrid.innerHTML = this.renderEmptyPlaylists();
            return;
        }
        
        this.elements.playlistsGrid.innerHTML = this.state.playlists
            .map(playlist => this.renderPlaylistCard(playlist))
            .join('');
    }

    renderPlaylistCard(playlist) {
        const fileCount = playlist.files ? playlist.files.length : 0;
        const duration = this.calculatePlaylistDuration(playlist);
        
        return `
            <div class="playlist-card" data-playlist-id="${playlist.id}">
                <div class="playlist-card-header">
                    <div class="playlist-card-icon">📋</div>
                    <div class="playlist-card-info">
                        <div class="playlist-card-name">${playlist.name || 'Sans nom'}</div>
                        <div class="playlist-card-meta">
                            <span>📁 ${fileCount} fichiers</span>
                            <span>⏱️ ${duration}</span>
                        </div>
                    </div>
                </div>
                
                <div class="playlist-card-actions">
                    <button class="playlist-card-btn btn-play" data-action="play-playlist" data-playlist-id="${playlist.id}">
                        <span>▶️</span>
                        <span>Lire</span>
                    </button>
                    <button class="playlist-card-btn btn-edit" data-action="edit-playlist" data-playlist-id="${playlist.id}">
                        <span>✏️</span>
                        <span>Éditer</span>
                    </button>
                    <button class="playlist-card-btn btn-delete" data-action="delete-playlist" data-playlist-id="${playlist.id}">
                        <span>🗑️</span>
                    </button>
                </div>
            </div>
        `;
    }

    renderEmptyPlaylists() {
        return `
            <div class="empty-state">
                <div class="empty-state-icon">📋</div>
                <div class="empty-state-text">Aucune playlist</div>
                <div class="empty-state-hint">Créez une playlist pour organiser vos fichiers</div>
            </div>
        `;
    }

    // ========================================================================
    // ACTIONS FICHIERS
    // ========================================================================

    handleFileAction(e) {
        const button = e.target.closest('[data-action]');
        if (!button) return;
        
        const action = button.dataset.action;
        const fileId = button.dataset.fileId;
        const file = this.state.files.find(f => f.id === fileId);
        
        if (!file) return;
        
        switch (action) {
            case 'edit':
                this.editFile(file);
                break;
            case 'routes':
                this.editRoutes(file);
                break;
        }
    }

    uploadFile() {
        this.logger.info('[FileView] Upload file requested');
        
        if (this.eventBus) {
            this.eventBus.emit('file:upload_requested');
        }
        
        // Créer un input file pour sélectionner le fichier
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.json,.mid,.midi';
        input.onchange = (e) => {
            const file = e.target.files[0];
            if (file) {
                this.handleFileUpload(file);
            }
        };
        input.click();
    }

    handleFileUpload(file) {
        this.logger.info('[FileView] Uploading file:', file.name);
        
        const reader = new FileReader();
        reader.onload = (e) => {
            try {
                const content = e.target.result;
                
                if (this.eventBus) {
                    this.eventBus.emit('file:upload', {
                        filename: file.name,
                        content: content
                    });
                }
                
                this.logger.info('[FileView] File uploaded successfully');
            } catch (error) {
                this.logger.error('[FileView] Error uploading file:', error);
            }
        };
        
        reader.readAsText(file);
    }

    editFile(file) {
        this.logger.info('[FileView] Edit file:', file.name);
        
        if (this.eventBus) {
            this.eventBus.emit('file:edit_requested', { file });
            this.eventBus.emit('navigation:page_request', { page: 'editor', data: { file } });
        }
    }

    editRoutes(file) {
        this.logger.info('[FileView] Edit routes for file:', file.name);
        
        // Modal pour choisir le mode de routing
        if (this.eventBus) {
            this.eventBus.emit('file:routes_requested', { file });
            this.showRoutingModeModal(file);
        }
    }

    showRoutingModeModal(file) {
        // Créer un modal pour choisir entre 1→1 et N→N
        const modalContent = `
            <div class="routing-mode-modal">
                <h2>Mode de routing pour ${file.name}</h2>
                <p>Choisissez le mode de routing :</p>
                <div class="routing-mode-buttons">
                    <button class="btn-routing-mode" data-mode="simple">
                        <span class="mode-icon">→</span>
                        <span class="mode-title">Simple (1→1)</span>
                        <span class="mode-desc">Un canal d'entrée vers un canal de sortie</span>
                    </button>
                    <button class="btn-routing-mode" data-mode="complex">
                        <span class="mode-icon">⚡</span>
                        <span class="mode-title">Complexe (N→N)</span>
                        <span class="mode-desc">Plusieurs canaux avec routage avancé</span>
                    </button>
                </div>
            </div>
        `;
        
        if (this.eventBus) {
            this.eventBus.emit('modal:show', {
                content: modalContent,
                onAction: (mode) => {
                    this.eventBus.emit('navigation:page_request', {
                        page: 'routing',
                        data: { file, mode }
                    });
                }
            });
        }
    }

    refreshFiles() {
        this.logger.info('[FileView] Refreshing files...');
        
        if (this.eventBus) {
            this.eventBus.emit('files:refresh_requested');
        }
    }

    // ========================================================================
    // ACTIONS PLAYLISTS
    // ========================================================================

    handlePlaylistAction(e) {
        const button = e.target.closest('[data-action]');
        if (!button) return;
        
        const action = button.dataset.action;
        const playlistId = button.dataset.playlistId;
        const playlist = this.state.playlists.find(p => p.id === playlistId);
        
        if (!playlist && action !== 'create-playlist') return;
        
        switch (action) {
            case 'edit-playlist':
                this.editPlaylist(playlist);
                break;
            case 'play-playlist':
                this.playPlaylist(playlist);
                break;
            case 'delete-playlist':
                this.deletePlaylist(playlist);
                break;
        }
    }

    createPlaylist() {
        this.logger.info('[FileView] Create new playlist');
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:create_requested');
            this.showPlaylistEditorModal(null);
        }
    }

    editPlaylist(playlist) {
        this.logger.info('[FileView] Edit playlist:', playlist.name);
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:edit_requested', { playlist });
            this.showPlaylistEditorModal(playlist);
        }
    }

    playPlaylist(playlist) {
        this.logger.info('[FileView] Play playlist:', playlist.name);
        
        if (this.eventBus) {
            this.eventBus.emit('playlist:play_requested', { playlist });
            this.eventBus.emit('navigation:page_request', { page: 'home' });
        }
    }

    deletePlaylist(playlist) {
        this.logger.info('[FileView] Delete playlist:', playlist.name);
        
        // Confirmation
        if (confirm(`Supprimer la playlist "${playlist.name}" ?`)) {
            if (this.eventBus) {
                this.eventBus.emit('playlist:delete_requested', { playlist });
            }
        }
    }

    showPlaylistEditorModal(playlist) {
        // Modal pour créer/éditer une playlist
        const isNew = !playlist;
        const title = isNew ? 'Nouvelle Playlist' : `Éditer ${playlist.name}`;
        
        const modalContent = `
            <div class="playlist-editor-modal">
                <h2>${title}</h2>
                <div class="playlist-editor-form">
                    <div class="form-group">
                        <label>Nom de la playlist</label>
                        <input type="text" id="playlistName" value="${playlist ? playlist.name : ''}" />
                    </div>
                    
                    <div class="form-group">
                        <label>Fichiers</label>
                        <div class="playlist-files-selector" id="playlistFilesSelector">
                            <!-- Généré dynamiquement -->
                        </div>
                    </div>
                    
                    <div class="form-actions">
                        <button class="btn-action" id="btnSavePlaylist">💾 Enregistrer</button>
                        <button class="btn-action btn-cancel" id="btnCancelPlaylist">❌ Annuler</button>
                    </div>
                </div>
            </div>
        `;
        
        if (this.eventBus) {
            this.eventBus.emit('modal:show', { content: modalContent });
        }
    }

    refreshPlaylists() {
        this.logger.info('[FileView] Refreshing playlists...');
        
        if (this.eventBus) {
            this.eventBus.emit('playlists:refresh_requested');
        }
    }

    // ========================================================================
    // CHARGEMENT DES DONNÉES
    // ========================================================================

    loadData() {
        if (this.eventBus) {
            this.eventBus.emit('files:load_requested');
            this.eventBus.emit('playlists:load_requested');
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    formatDuration(seconds) {
        const mins = Math.floor(seconds / 60);
        const secs = Math.floor(seconds % 60);
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    }

    calculatePlaylistDuration(playlist) {
        if (!playlist.files || playlist.files.length === 0) {
            return '0:00';
        }
        
        const totalSeconds = playlist.files.reduce((sum, fileId) => {
            const file = this.state.files.find(f => f.id === fileId);
            return sum + (file ? file.duration || 0 : 0);
        }, 0);
        
        return this.formatDuration(totalSeconds);
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    destroy() {
        if (this.eventBus) {
            this.eventBus.off('files:loaded');
            this.eventBus.off('playlists:loaded');
            this.eventBus.off('file:updated');
            this.eventBus.off('playlist:updated');
        }
        
        this.logger.info('[FileView] Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileView;
}

if (typeof window !== 'undefined') {
    window.FileView = FileView;
}
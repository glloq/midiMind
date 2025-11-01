// ============================================================================
// Fichier: frontend/js/views/PlaylistView.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE + API COMPLÈTE
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.2.0:
// ✅ Signature cohérente : constructor(containerId, eventBus, logger = null)
// ✅ Appel super() correct
// ✅ Affichage liste playlists
// ✅ Affichage fichiers dans playlist
// ✅ Contrôles playlist (play, shuffle, repeat)
// ============================================================================

class PlaylistView extends BaseView {
    constructor(containerId, eventBus, logger = null) {
        super(containerId, eventBus);
        
        this.logger = logger || window.logger || console;
        
        // État spécifique à la vue
        this.viewState = {
            playlists: [],
            selectedPlaylist: null,
            currentFile: null,
            isPlaying: false,
            shuffle: false,
            repeat: false
        };
        
        this.log('info', 'PlaylistView', '✅ PlaylistView v3.2.0 initialized');
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
    buildTemplate(data = {}) {
        const state = { ...this.viewState, ...data };
        
        return \`
            <div class="playlist-view-container">
                <div class="page-header">
                    <h1>🎵 Playlists</h1>
                    <div class="header-actions">
                        <button class="btn-primary" data-action="create-playlist">
                            ➕ Nouvelle Playlist
                        </button>
                    </div>
                </div>
                
                <div class="playlist-layout">
                    <!-- Sidebar: Liste playlists -->
                    <div class="playlist-sidebar">
                        <h2>Mes Playlists</h2>
                        \${this.renderPlaylistsList(state)}
                    </div>
                    
                    <!-- Main: Contenu playlist -->
                    <div class="playlist-main">
                        \${this.renderPlaylistContent(state)}
                    </div>
                </div>
            </div>
        \`;
    }
    
    // ========================================================================
    // RENDERING PLAYLISTS
    // ========================================================================
    
    renderPlaylistsList(state) {
        const playlists = state.playlists || [];
        
        if (playlists.length === 0) {
            return \`
                <div class="playlists-empty">
                    <p>Aucune playlist</p>
                    <p class="text-muted">Créez votre première playlist</p>
                </div>
            \`;
        }
        
        return \`
            <div class="playlists-list">
                \${playlists.map(playlist => this.renderPlaylistItem(playlist, state.selectedPlaylist)).join('')}
            </div>
        \`;
    }
    
    renderPlaylistItem(playlist, selectedPlaylist) {
        const isSelected = selectedPlaylist && selectedPlaylist.id === playlist.id;
        const selectedClass = isSelected ? 'selected' : '';
        const fileCount = playlist.files?.length || 0;
        
        return \`
            <div class="playlist-item \${selectedClass}" 
                 data-playlist-id="\${playlist.id}"
                 data-action="select-playlist">
                <div class="playlist-icon">📋</div>
                <div class="playlist-info">
                    <div class="playlist-name">\${playlist.name}</div>
                    <div class="playlist-meta">\${fileCount} fichier(s)</div>
                </div>
                <button class="btn-icon btn-danger" 
                        data-action="delete-playlist" 
                        data-playlist-id="\${playlist.id}"
                        title="Supprimer">
                    🗑️
                </button>
            </div>
        \`;
    }
    
    // ========================================================================
    // RENDERING PLAYLIST CONTENT
    // ========================================================================
    
    renderPlaylistContent(state) {
        if (!state.selectedPlaylist) {
            return \`
                <div class="playlist-empty-state">
                    <div class="empty-icon">🎵</div>
                    <p>Sélectionnez une playlist</p>
                </div>
            \`;
        }
        
        const playlist = state.selectedPlaylist;
        const files = playlist.files || [];
        
        return \`
            <div class="playlist-content">
                <div class="playlist-header">
                    <h2>\${playlist.name}</h2>
                    <div class="playlist-controls">
                        \${this.renderPlaylistControls(state)}
                    </div>
                </div>
                
                <div class="playlist-files">
                    \${files.length === 0 
                        ? this.renderEmptyPlaylist() 
                        : this.renderFilesList(files, state.currentFile)}
                </div>
                
                <div class="playlist-footer">
                    <button class="btn-secondary" data-action="add-files">
                        ➕ Ajouter des fichiers
                    </button>
                </div>
            </div>
        \`;
    }
    
    renderPlaylistControls(state) {
        const playIcon = state.isPlaying ? '⏸️' : '▶️';
        const shuffleClass = state.shuffle ? 'active' : '';
        const repeatClass = state.repeat ? 'active' : '';
        
        return \`
            <div class="controls-group">
                <button class="btn-control" data-action="play-playlist" title="Play/Pause">
                    \${playIcon}
                </button>
                <button class="btn-control \${shuffleClass}" data-action="toggle-shuffle" title="Shuffle">
                    🔀
                </button>
                <button class="btn-control \${repeatClass}" data-action="toggle-repeat" title="Repeat">
                    🔁
                </button>
            </div>
        \`;
    }
    
    renderEmptyPlaylist() {
        return \`
            <div class="files-empty">
                <div class="empty-icon">📂</div>
                <p>Cette playlist est vide</p>
                <p class="text-muted">Ajoutez des fichiers MIDI</p>
            </div>
        \`;
    }
    
    renderFilesList(files, currentFile) {
        return \`
            <div class="files-list">
                \${files.map((file, index) => this.renderFileItem(file, index, currentFile)).join('')}
            </div>
        \`;
    }
    
    renderFileItem(file, index, currentFile) {
        const isCurrent = currentFile && (currentFile.id === file.id || currentFile.name === file.name);
        const currentClass = isCurrent ? 'current' : '';
        
        return \`
            <div class="file-item \${currentClass}" 
                 data-file-id="\${file.id || file.name}"
                 data-file-index="\${index}">
                <div class="file-number">\${index + 1}</div>
                <div class="file-icon">\${isCurrent ? '🎵' : '🎼'}</div>
                <div class="file-info">
                    <div class="file-name">\${file.name || file.id}</div>
                    <div class="file-meta">
                        \${file.duration ? this.formatDuration(file.duration) : 'N/A'}
                    </div>
                </div>
                <div class="file-actions">
                    <button class="btn-icon" 
                            data-action="play-file" 
                            data-file-index="\${index}"
                            title="Lire">
                        ▶️
                    </button>
                    <button class="btn-icon btn-danger" 
                            data-action="remove-file" 
                            data-file-index="\${index}"
                            title="Retirer">
                        ✖️
                    </button>
                </div>
            </div>
        \`;
    }
    
    // ========================================================================
    // FORMATTERS
    // ========================================================================
    
    formatDuration(duration) {
        if (!duration) return '0:00';
        
        const minutes = Math.floor(duration / 60);
        const seconds = Math.floor(duration % 60);
        
        return \`\${minutes}:\${seconds.toString().padStart(2, '0')}\`;
    }
    
    // ========================================================================
    // UPDATE MÉTHODES
    // ========================================================================
    
    updatePlaylists(playlists) {
        this.viewState.playlists = playlists;
        this.render();
    }
    
    updateSelectedPlaylist(playlist) {
        this.viewState.selectedPlaylist = playlist;
        this.render();
    }
    
    updateCurrentFile(file) {
        this.viewState.currentFile = file;
        this.render();
    }
    
    updatePlaybackState(isPlaying) {
        this.viewState.isPlaying = isPlaying;
        this.render();
    }
    
    updateControls(controls) {
        if (controls.shuffle !== undefined) {
            this.viewState.shuffle = controls.shuffle;
        }
        if (controls.repeat !== undefined) {
            this.viewState.repeat = controls.repeat;
        }
        this.render();
    }
    
    // ========================================================================
    // ÉVÉNEMENTS UI
    // ========================================================================
    
    attachEventListeners() {
        if (!this.container) return;
        
        // Délégation événements
        this.container.addEventListener('click', (e) => {
            const target = e.target.closest('[data-action]');
            if (!target) return;
            
            const action = target.dataset.action;
            const playlistId = target.dataset.playlistId;
            const fileIndex = target.dataset.fileIndex;
            
            e.stopPropagation();
            
            switch (action) {
                case 'create-playlist':
                    const name = prompt('Nom de la playlist:');
                    if (name) {
                        this.eventBus.emit('playlist:create', { name });
                    }
                    break;
                    
                case 'select-playlist':
                    if (playlistId) {
                        this.eventBus.emit('playlist:select', { playlistId });
                    }
                    break;
                    
                case 'delete-playlist':
                    if (playlistId) {
                        const confirmed = confirm('Supprimer cette playlist ?');
                        if (confirmed) {
                            this.eventBus.emit('playlist:delete', { playlistId });
                        }
                    }
                    break;
                    
                case 'play-playlist':
                    this.eventBus.emit('playlist:play');
                    break;
                    
                case 'toggle-shuffle':
                    this.eventBus.emit('playlist:toggle-shuffle');
                    break;
                    
                case 'toggle-repeat':
                    this.eventBus.emit('playlist:toggle-repeat');
                    break;
                    
                case 'add-files':
                    this.eventBus.emit('playlist:add-files');
                    break;
                    
                case 'play-file':
                    if (fileIndex !== undefined) {
                        this.eventBus.emit('playlist:play-file', { index: parseInt(fileIndex) });
                    }
                    break;
                    
                case 'remove-file':
                    if (fileIndex !== undefined) {
                        this.eventBus.emit('playlist:remove-file', { index: parseInt(fileIndex) });
                    }
                    break;
            }
        });
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
    window.PlaylistView = PlaylistView;
}
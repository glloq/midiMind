// ============================================================================
// Fichier: frontend/js/views/PlaylistView.js
// Version: v4.0.0 - CONFORMITÃ‰ API DOCUMENTATION
// Date: 2025-11-02
// ============================================================================
// AMÃ‰LIORATIONS v4.0.0:
// âœ… API v4.2.2: playlist.* (create, delete, update, list, get, addItem, removeItem, reorder, setLoop)
// âœ… Drag & drop pour rÃ©organiser
// âœ… Gestion loop
// ============================================================================

class PlaylistView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        this.logger = window.logger || console;
        
        // Ã‰tat
        this.viewState = {
            playlists: [],
            selectedPlaylist: null,
            playlistItems: [],
            isEditing: false,
            draggedItem: null
        };
        
        this.log('info', 'PlaylistView', 'âœ… PlaylistView v4.0.0 initialized');
    }
    
    // ========================================================================
    // TEMPLATE
    // ========================================================================
    
    buildTemplate(data = {}) {
        const state = { ...this.viewState, ...data };
        
        return `
            <div class="playlist-view-container">
                <div class="page-header">
                    <h1>ðŸŽµ Playlists</h1>
                    <div class="header-actions">
                        <button class="btn-create" data-action="create-playlist">
                            âž• Nouvelle Playlist
                        </button>
                    </div>
                </div>
                
                <div class="playlist-layout">
                    <!-- Liste des playlists -->
                    <div class="playlists-sidebar">
                        <h2>Mes playlists</h2>
                        <div id="playlistsList">
                            ${this.renderPlaylistsList(state)}
                        </div>
                    </div>
                    
                    <!-- Contenu playlist -->
                    <div class="playlist-content">
                        ${state.selectedPlaylist ? 
                            this.renderPlaylistContent(state) : 
                            this.renderNoSelection()}
                    </div>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // RENDERING
    // ========================================================================
    
    renderPlaylistsList(state) {
        const playlists = state.playlists;
        
        if (playlists.length === 0) {
            return `
                <div class="playlists-empty">
                    <p>Aucune playlist</p>
                    <button class="btn-create-first" data-action="create-playlist">
                        âž• CrÃ©er
                    </button>
                </div>
            `;
        }
        
        return `
            <div class="playlists-list">
                ${playlists.map(pl => this.renderPlaylistItem(pl, state.selectedPlaylist)).join('')}
            </div>
        `;
    }
    
    renderPlaylistItem(playlist, selectedPlaylist) {
        const isSelected = selectedPlaylist && selectedPlaylist.id === playlist.id;
        const itemCount = playlist.item_count || (playlist.items ? playlist.items.length : 0);
        
        return `
            <div class="playlist-item ${isSelected ? 'selected' : ''}" 
                 data-playlist-id="${playlist.id}">
                <div class="playlist-icon">ðŸ“‹</div>
                <div class="playlist-info">
                    <div class="playlist-name">${playlist.name}</div>
                    <div class="playlist-count">${itemCount} morceaux</div>
                </div>
                <div class="playlist-actions">
                    <button class="btn-icon" data-action="select-playlist" title="Voir">ðŸ‘ï¸</button>
                    <button class="btn-icon" data-action="delete-playlist" title="Supprimer">ðŸ—‘ï¸</button>
                </div>
            </div>
        `;
    }
    
    renderPlaylistContent(state) {
        const pl = state.selectedPlaylist;
        const items = state.playlistItems;
        
        return `
            <div class="playlist-header">
                <div class="playlist-title">
                    <input type="text" class="playlist-name-input" 
                           value="${pl.name}" data-action="rename-playlist" />
                    <span class="playlist-loop ${pl.loop ? 'active' : ''}" 
                          data-action="toggle-loop">
                        ðŸ” Loop ${pl.loop ? 'ON' : 'OFF'}
                    </span>
                </div>
                <div class="playlist-actions">
                    <button class="btn-play-all" data-action="play-playlist">
                        â–¶ Lire tout
                    </button>
                    <button class="btn-add-files" data-action="add-files">
                        âž• Ajouter des fichiers
                    </button>
                </div>
            </div>
            
            <div class="playlist-items" id="playlistItems">
                ${items.length > 0 ? 
                    this.renderPlaylistItems(items) : 
                    this.renderEmptyPlaylist()}
            </div>
        `;
    }
    
    renderPlaylistItems(items) {
        return `
            <div class="items-list">
                ${items.map((item, index) => this.renderPlaylistItemEntry(item, index)).join('')}
            </div>
        `;
    }
    
    renderPlaylistItemEntry(item, index) {
        return `
            <div class="item-entry" 
                 data-item-id="${item.id}" 
                 data-index="${index}"
                 draggable="true">
                <div class="item-drag">â˜°</div>
                <div class="item-order">${index + 1}</div>
                <div class="item-info">
                    <div class="item-name">${item.file_path || item.name || 'â€”'}</div>
                    <div class="item-duration">${item.duration ? this.formatDuration(item.duration) : 'â€”'}</div>
                </div>
                <div class="item-actions">
                    <button class="btn-icon" data-action="play-item" title="Lire">â–¶</button>
                    <button class="btn-icon" data-action="remove-item" title="Retirer">âœ•</button>
                </div>
            </div>
        `;
    }
    
    renderEmptyPlaylist() {
        return `
            <div class="playlist-empty">
                <div class="empty-icon">ðŸŽµ</div>
                <p>Playlist vide</p>
                <button class="btn-add-files" data-action="add-files">
                    âž• Ajouter des fichiers
                </button>
            </div>
        `;
    }
    
    renderNoSelection() {
        return `
            <div class="no-selection">
                <div class="empty-icon">ðŸ“‹</div>
                <p>SÃ©lectionnez une playlist</p>
            </div>
        `;
    }
    
    // ========================================================================
    // EVENTS
    // ========================================================================
    
    attachEvents() {
        super.attachEvents();
        
        if (!this.container) return;
        
        this.container.addEventListener('click', (e) => {
            const action = e.target.closest('[data-action]')?.dataset.action;
            if (!action) return;
            
            const playlistItem = e.target.closest('.playlist-item');
            const itemEntry = e.target.closest('.item-entry');
            
            switch (action) {
                case 'create-playlist':
                    this.createPlaylist();
                    break;
                case 'select-playlist':
                    if (playlistItem) this.selectPlaylist(playlistItem.dataset.playlistId);
                    break;
                case 'delete-playlist':
                    if (playlistItem) this.deletePlaylist(playlistItem.dataset.playlistId);
                    break;
                case 'play-playlist':
                    this.playPlaylist();
                    break;
                case 'add-files':
                    this.addFiles();
                    break;
                case 'toggle-loop':
                    this.toggleLoop();
                    break;
                case 'play-item':
                    if (itemEntry) this.playItem(itemEntry.dataset.itemId);
                    break;
                case 'remove-item':
                    if (itemEntry) this.removeItem(itemEntry.dataset.itemId);
                    break;
            }
        });
        
        // Drag & drop
        this.container.addEventListener('dragstart', (e) => {
            const itemEntry = e.target.closest('.item-entry');
            if (itemEntry) {
                this.viewState.draggedItem = itemEntry.dataset.itemId;
                itemEntry.classList.add('dragging');
            }
        });
        
        this.container.addEventListener('dragend', (e) => {
            const itemEntry = e.target.closest('.item-entry');
            if (itemEntry) {
                itemEntry.classList.remove('dragging');
                this.viewState.draggedItem = null;
            }
        });
        
        this.container.addEventListener('dragover', (e) => {
            e.preventDefault();
            const itemEntry = e.target.closest('.item-entry');
            if (itemEntry && this.viewState.draggedItem) {
                itemEntry.classList.add('drag-over');
            }
        });
        
        this.container.addEventListener('dragleave', (e) => {
            const itemEntry = e.target.closest('.item-entry');
            if (itemEntry) {
                itemEntry.classList.remove('drag-over');
            }
        });
        
        this.container.addEventListener('drop', (e) => {
            e.preventDefault();
            const itemEntry = e.target.closest('.item-entry');
            if (itemEntry && this.viewState.draggedItem) {
                this.reorderItem(this.viewState.draggedItem, itemEntry.dataset.index);
                itemEntry.classList.remove('drag-over');
            }
        });
        
        // Rename
        this.container.addEventListener('blur', (e) => {
            if (e.target.dataset.action === 'rename-playlist') {
                this.renamePlaylist(e.target.value);
            }
        }, true);
        
        this.setupEventBusListeners();
    }
    
    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        // playlist.list response
        this.eventBus.on('playlists:listed', (data) => {
            this.viewState.playlists = data.playlists || [];
            this.render();
        });
        
        // playlist.get response
        this.eventBus.on('playlist:loaded', (data) => {
            this.viewState.selectedPlaylist = data.playlist;
            this.viewState.playlistItems = data.playlist.items || [];
            this.render();
        });
        
        // playlist.create response
        this.eventBus.on('playlist:created', (data) => {
            this.loadPlaylists();
        });
        
        // playlist.delete response
        this.eventBus.on('playlist:deleted', (data) => {
            if (this.viewState.selectedPlaylist && 
                this.viewState.selectedPlaylist.id === data.playlist_id) {
                this.viewState.selectedPlaylist = null;
                this.viewState.playlistItems = [];
            }
            this.loadPlaylists();
        });
        
        // playlist.update response
        this.eventBus.on('playlist:updated', (data) => {
            this.loadPlaylist(this.viewState.selectedPlaylist.id);
        });
    }
    
    // ========================================================================
    // ACTIONS
    // ========================================================================
    
    async createPlaylist() {
        const name = prompt('Nom de la playlist:');
        if (!name) return;
        
        // API: playlist.create
        if (this.eventBus) {
            this.eventBus.emit('playlist:create_requested', {
                name,
                description: ''
            });
        }
    }
    
    async selectPlaylist(playlistId) {
        // API: playlist.get
        if (this.eventBus) {
            this.eventBus.emit('playlist:get_requested', {
                playlist_id: playlistId
            });
        }
    }
    
    async deletePlaylist(playlistId) {
        if (!confirm('Supprimer cette playlist ?')) return;
        
        // API: playlist.delete
        if (this.eventBus) {
            this.eventBus.emit('playlist:delete_requested', {
                playlist_id: playlistId
            });
        }
    }
    
    async renamePlaylist(newName) {
        if (!this.viewState.selectedPlaylist || !newName) return;
        
        // API: playlist.update
        if (this.eventBus) {
            this.eventBus.emit('playlist:update_requested', {
                playlist_id: this.viewState.selectedPlaylist.id,
                name: newName
            });
        }
    }
    
    async toggleLoop() {
        if (!this.viewState.selectedPlaylist) return;
        
        const newLoop = !this.viewState.selectedPlaylist.loop;
        
        // API: playlist.setLoop
        if (this.eventBus) {
            this.eventBus.emit('playlist:set_loop_requested', {
                playlist_id: this.viewState.selectedPlaylist.id,
                loop: newLoop
            });
        }
    }
    
    async addFiles() {
        // Montrer modal de sÃ©lection de fichiers
        if (this.eventBus) {
            this.eventBus.emit('playlist:add_files_modal_requested', {
                playlist_id: this.viewState.selectedPlaylist.id
            });
        }
    }
    
    async removeItem(itemId) {
        if (!this.viewState.selectedPlaylist) return;
        
        // API: playlist.removeItem
        if (this.eventBus) {
            this.eventBus.emit('playlist:remove_item_requested', {
                playlist_id: this.viewState.selectedPlaylist.id,
                item_id: itemId
            });
        }
    }
    
    async reorderItem(itemId, newIndex) {
        if (!this.viewState.selectedPlaylist) return;
        
        // API: playlist.reorder
        if (this.eventBus) {
            this.eventBus.emit('playlist:reorder_requested', {
                playlist_id: this.viewState.selectedPlaylist.id,
                item_id: itemId,
                new_index: parseInt(newIndex)
            });
        }
    }
    
    async playPlaylist() {
        if (!this.viewState.selectedPlaylist) return;
        
        // Ã‰mettre event pour lire la playlist
        if (this.eventBus) {
            this.eventBus.emit('playlist:play_requested', {
                playlist_id: this.viewState.selectedPlaylist.id
            });
        }
    }
    
    async playItem(itemId) {
        // Ã‰mettre event pour lire un item spÃ©cifique
        if (this.eventBus) {
            this.eventBus.emit('playlist:play_item_requested', {
                item_id: itemId
            });
        }
    }
    
    // ========================================================================
    // LOADING
    // ========================================================================
    
    async loadPlaylists() {
        // API: playlist.list
        if (this.eventBus) {
            this.eventBus.emit('playlist:list_requested');
        }
    }
    
    async loadPlaylist(playlistId) {
        // API: playlist.get
        if (this.eventBus) {
            this.eventBus.emit('playlist:get_requested', {
                playlist_id: playlistId
            });
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
    
    // ========================================================================
    // INIT
    // ========================================================================
    
    init() {
        super.init();
        this.loadPlaylists();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PlaylistView;
}

if (typeof window !== 'undefined') {
    window.PlaylistView = PlaylistView;
}
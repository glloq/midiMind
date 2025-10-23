// ============================================================================
// Fichier: frontend/js/views/PlaylistView.js
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Vue dÃ©diÃ©e Ã  l'affichage et la gestion de l'interface des playlists.
//   Interface complÃ¨te avec liste de playlists, fichiers, contrÃ´les
//   (shuffle/repeat/auto-advance) et queue de lecture temporaire.
//
// FonctionnalitÃ©s:
//   - Affichage liste playlists sauvegardÃ©es
//   - Affichage fichiers de la playlist courante
//   - ContrÃ´les lecture (shuffle, repeat, auto-advance)
//   - Queue de lecture temporaire avec drag & drop
//   - Indicateurs visuels (fichier en cours, queue active)
//   - Statistiques (durÃ©e totale, nombre fichiers)
//   - Menu contextuel (Ã©dition, suppression)
//   - Drag & Drop pour rÃ©organisation
//
// Structure HTML:
//   playlist-view-container
//   â”œâ”€â”€ playlist-header (titre + contrÃ´les)
//   â”œâ”€â”€ playlist-layout
//   â”‚   â”œâ”€â”€ playlist-sidebar.left (liste playlists)
//   â”‚   â”œâ”€â”€ playlist-main (fichiers playlist)
//   â”‚   â””â”€â”€ playlist-sidebar.right (queue)
//   â””â”€â”€ playlist-footer (statistiques)
//
// Auteur: MidiMind Team
// ============================================================================


class PlaylistView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        // Ã‰tat de la vue
        this.viewState = {
            currentPlaylist: null,
            currentFile: null,
            playlists: [],
            queueVisible: true,
            editMode: false,
            selectedFiles: [],
            draggedItem: null
        };
        
        // Configuration
        this.config = {
            autoRender: false,
            showDurations: true,
            showMetadata: true,
            enableDragDrop: true,
            queueCollapsible: true
        };
        
        // Cache des Ã©lÃ©ments DOM pour performance
        this.cachedElements = {
            playlistsList: null,
            currentPlaylistContent: null,
            queuePanel: null,
            controls: null,
            stats: null
        };
        
        this.logger = window.logger || console;
        this.logger.info('PlaylistView', 'ðŸŽµ PlaylistView v1.0.0 initialized');
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
    /**
     * Construit le template HTML complet
     */
    buildTemplate(data = {}) {
        // Fusionner data avec viewState
        const state = { ...this.viewState, ...data };
        
        return `
            <div class="playlist-view-container">
                
                <!-- Header avec actions globales -->
                <div class="playlist-header">
                    ${this.renderHeader(state)}
                </div>
                
                <!-- Layout principal 3 colonnes -->
                <div class="playlist-layout">
                    
                    <!-- Sidebar gauche: Liste des playlists -->
                    <div class="playlist-sidebar left">
                        ${this.renderPlaylistsList(state)}
                    </div>
                    
                    <!-- Centre: Contenu playlist courante -->
                    <div class="playlist-main">
                        ${this.renderCurrentPlaylist(state)}
                    </div>
                    
                    <!-- Sidebar droite: Queue (repliable) -->
                    <div class="playlist-sidebar right ${state.queueVisible ? 'visible' : 'collapsed'}">
                        ${this.renderQueuePanel(state)}
                    </div>
                    
                </div>
                
                <!-- Footer avec statistiques -->
                <div class="playlist-footer">
                    ${this.renderStats(state)}
                </div>
                
            </div>
        `;
    }
    
    // ========================================================================
    // HEADER
    // ========================================================================
    
    renderHeader(state) {
        return `
            <div class="playlist-header-content">
                <div class="header-left">
                    <h2 class="playlist-title">
                        <span class="icon">ðŸŽµ</span>
                        Playlists
                    </h2>
                </div>
                
                <div class="header-center">
                    ${this.renderControls(state)}
                </div>
                
                <div class="header-right">
                    <button class="btn btn-primary" 
                            onclick="app.playlistController.openPlaylistEditor()"
                            title="CrÃ©er une nouvelle playlist">
                        <span class="icon">âž•</span>
                        Nouvelle Playlist
                    </button>
                    
                    <button class="btn btn-secondary" 
                            onclick="app.playlistController.toggleQueue()"
                            title="Afficher/Masquer la queue">
                        <span class="icon">ðŸ“‹</span>
                        Queue
                        ${state.queueVisible ? 'â–¼' : 'â–¶'}
                    </button>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // CONTRÃ”LES (Shuffle / Repeat / Auto-advance)
    // ========================================================================
    
    renderControls(state) {
        const shuffleMode = state.shuffleMode || false;
        const repeatMode = state.repeatMode || 'none';
        const autoAdvance = state.autoAdvance !== false;
        
        return `
            <div class="playlist-controls" data-controls>
                
                <!-- Shuffle -->
                <button class="btn-control ${shuffleMode ? 'active' : ''}"
                        data-control="shuffle"
                        onclick="app.playlistController.toggleShuffle()"
                        title="Mode alÃ©atoire">
                    <span class="icon">ðŸ”€</span>
                    <span class="label">Shuffle</span>
                </button>
                
                <!-- Repeat -->
                <button class="btn-control ${repeatMode !== 'none' ? 'active' : ''}"
                        data-control="repeat"
                        onclick="app.playlistController.cycleRepeat()"
                        title="Mode rÃ©pÃ©tition: ${repeatMode}">
                    <span class="icon">${this.getRepeatIcon(repeatMode)}</span>
                    <span class="label">${this.getRepeatLabel(repeatMode)}</span>
                </button>
                
                <!-- Auto-advance -->
                <button class="btn-control ${autoAdvance ? 'active' : ''}"
                        data-control="auto-advance"
                        onclick="app.playlistController.toggleAutoAdvance()"
                        title="Avance automatique">
                    <span class="icon">â­ï¸</span>
                    <span class="label">Auto</span>
                </button>
                
            </div>
        `;
    }
    
    getRepeatIcon(mode) {
        switch(mode) {
            case 'one': return 'ðŸ”‚';
            case 'all': return 'ðŸ”';
            default: return 'â†»';
        }
    }
    
    getRepeatLabel(mode) {
        switch(mode) {
            case 'one': return 'Repeat One';
            case 'all': return 'Repeat All';
            default: return 'Repeat';
        }
    }
    
    // ========================================================================
    // LISTE DES PLAYLISTS (Sidebar gauche)
    // ========================================================================
    
    renderPlaylistsList(state) {
        const playlists = state.playlists || [];
        const currentId = state.currentPlaylist?.id;
        
        return `
            <div class="playlists-list-container">
                <div class="playlists-list-header">
                    <h3>Mes Playlists</h3>
                    <span class="count">${playlists.length}</span>
                </div>
                
                <div class="playlists-list" data-playlists-list>
                    ${playlists.length === 0 
                        ? this.renderEmptyPlaylists()
                        : playlists.map(pl => this.renderPlaylistItem(pl, currentId)).join('')
                    }
                </div>
            </div>
        `;
    }
    
    renderPlaylistItem(playlist, currentId) {
        const isActive = playlist.id === currentId;
        const fileCount = playlist.files?.length || 0;
        const duration = this.formatDuration(playlist.duration || 0);
        
        return `
            <div class="playlist-item ${isActive ? 'active' : ''}"
                 data-playlist-id="${playlist.id}"
                 onclick="app.playlistController.loadPlaylist('${playlist.id}')">
                
                <div class="playlist-item-icon">
                    ${isActive ? 'â–¶ï¸' : 'ðŸŽµ'}
                </div>
                
                <div class="playlist-item-info">
                    <div class="playlist-item-name" title="${this.escapeHtml(playlist.name)}">
                        ${this.escapeHtml(playlist.name)}
                    </div>
                    <div class="playlist-item-meta">
                        <span class="file-count">${fileCount} fichier${fileCount > 1 ? 's' : ''}</span>
                        ${duration ? `<span class="duration">â€¢ ${duration}</span>` : ''}
                    </div>
                </div>
                
                <div class="playlist-item-actions">
                    <button class="btn-icon" 
                            onclick="event.stopPropagation(); app.playlistController.editPlaylist('${playlist.id}')"
                            title="Ã‰diter">
                        âœï¸
                    </button>
                    <button class="btn-icon" 
                            onclick="event.stopPropagation(); app.playlistController.deletePlaylist('${playlist.id}')"
                            title="Supprimer">
                        ðŸ—‘ï¸
                    </button>
                </div>
                
            </div>
        `;
    }
    
    renderEmptyPlaylists() {
        return `
            <div class="empty-state">
                <div class="empty-icon">ðŸ“</div>
                <p class="empty-message">Aucune playlist</p>
                <button class="btn btn-sm btn-primary" 
                        onclick="app.playlistController.openPlaylistEditor()">
                    CrÃ©er une playlist
                </button>
            </div>
        `;
    }
    
    // ========================================================================
    // PLAYLIST COURANTE (Centre)
    // ========================================================================
    
    renderCurrentPlaylist(state) {
        const playlist = state.currentPlaylist;
        
        if (!playlist) {
            return this.renderNoPlaylist();
        }
        
        const files = playlist.files || [];
        const currentFileId = state.currentFile?.id;
        
        return `
            <div class="current-playlist-container">
                
                <!-- En-tÃªte playlist -->
                <div class="current-playlist-header">
                    <h3 class="playlist-name">${this.escapeHtml(playlist.name)}</h3>
                    <div class="playlist-actions">
                        <button class="btn btn-sm" 
                                onclick="app.playlistController.editPlaylist('${playlist.id}')"
                                title="Ã‰diter cette playlist">
                            âœï¸ Ã‰diter
                        </button>
                        <button class="btn btn-sm" 
                                onclick="app.playlistController.clearCurrentPlaylist()"
                                title="Vider la playlist">
                            ðŸ—‘ï¸ Vider
                        </button>
                    </div>
                </div>
                
                <!-- Liste des fichiers -->
                <div class="playlist-files" 
                     data-playlist-files
                     data-playlist-id="${playlist.id}">
                    ${files.length === 0 
                        ? this.renderEmptyPlaylist()
                        : files.map((file, index) => 
                            this.renderFileItem(file, index, currentFileId)
                          ).join('')
                    }
                </div>
                
            </div>
        `;
    }
    
    renderNoPlaylist() {
        return `
            <div class="no-playlist-state">
                <div class="empty-icon">ðŸŽµ</div>
                <h3>Aucune playlist sÃ©lectionnÃ©e</h3>
                <p>CrÃ©ez ou sÃ©lectionnez une playlist pour commencer</p>
                <button class="btn btn-primary" 
                        onclick="app.playlistController.openPlaylistEditor()">
                    âž• CrÃ©er une playlist
                </button>
            </div>
        `;
    }
    
    renderEmptyPlaylist() {
        return `
            <div class="empty-playlist-state">
                <div class="empty-icon">ðŸ“­</div>
                <p>Cette playlist est vide</p>
                <button class="btn btn-sm btn-primary" 
                        onclick="app.playlistController.editPlaylist('${this.viewState.currentPlaylist?.id}')">
                    Ajouter des fichiers
                </button>
            </div>
        `;
    }
    
    renderFileItem(file, index, currentFileId) {
        const isPlaying = file.id === currentFileId;
        const duration = this.formatDuration(file.duration || 0);
        
        return `
            <div class="playlist-file-item ${isPlaying ? 'playing' : ''}"
                 data-file-id="${file.id}"
                 data-index="${index}"
                 draggable="${this.config.enableDragDrop}"
                 ondragstart="app.playlistView?.onDragStart(event, ${index})"
                 ondragover="app.playlistView?.onDragOver(event)"
                 ondrop="app.playlistView?.onDrop(event, ${index})"
                 ondblclick="app.playlistController.playFileAt(${index})">
                
                <!-- Handle drag -->
                <div class="file-drag-handle" title="Glisser pour rÃ©organiser">
                    â‹®â‹®
                </div>
                
                <!-- NumÃ©ro -->
                <div class="file-number">
                    ${isPlaying ? 'â–¶ï¸' : (index + 1)}
                </div>
                
                <!-- Infos -->
                <div class="file-info">
                    <div class="file-name" title="${this.escapeHtml(file.name || file.filename)}">
                        ${this.escapeHtml(file.name || file.filename)}
                    </div>
                    ${this.config.showMetadata && file.metadata ? `
                        <div class="file-metadata">
                            ${file.metadata.trackCount ? `<span>ðŸŽ¹ ${file.metadata.trackCount} pistes</span>` : ''}
                            ${file.metadata.bpm ? `<span>ðŸ¥ ${file.metadata.bpm} BPM</span>` : ''}
                        </div>
                    ` : ''}
                </div>
                
                <!-- DurÃ©e -->
                ${this.config.showDurations && duration ? `
                    <div class="file-duration">${duration}</div>
                ` : ''}
                
                <!-- Actions -->
                <div class="file-actions">
                    <button class="btn-icon" 
                            onclick="event.stopPropagation(); app.playlistController.playFileAt(${index})"
                            title="Lire">
                        â–¶ï¸
                    </button>
                    <button class="btn-icon" 
                            onclick="event.stopPropagation(); app.playlistController.addToQueue('${file.id}')"
                            title="Ajouter Ã  la queue">
                        âž•
                    </button>
                    <button class="btn-icon" 
                            onclick="event.stopPropagation(); app.playlistController.removeFileFromPlaylist('${this.viewState.currentPlaylist?.id}', '${file.id}')"
                            title="Retirer de la playlist">
                        âœ–ï¸
                    </button>
                </div>
                
            </div>
        `;
    }
    
    // ========================================================================
    // QUEUE PANEL (Sidebar droite)
    // ========================================================================
    
    renderQueuePanel(state) {
        const queue = state.queue || [];
        const isPlayingQueue = state.isPlayingQueue || false;
        
        return `
            <div class="queue-panel-container">
                
                <!-- Header queue -->
                <div class="queue-header">
                    <h3>
                        <span class="icon">ðŸ“‹</span>
                        Queue
                        ${isPlayingQueue ? '<span class="playing-badge">En cours</span>' : ''}
                    </h3>
                    <div class="queue-count">${queue.length}</div>
                </div>
                
                <!-- Actions queue -->
                ${queue.length > 0 ? `
                    <div class="queue-actions">
                        <button class="btn btn-sm btn-primary" 
                                onclick="app.playlistController.playQueue()"
                                ${isPlayingQueue ? 'disabled' : ''}>
                            â–¶ï¸ Lire la queue
                        </button>
                        <button class="btn btn-sm btn-danger" 
                                onclick="app.playlistController.clearQueue()">
                            ðŸ—‘ï¸ Vider
                        </button>
                    </div>
                ` : ''}
                
                <!-- Liste queue -->
                <div class="queue-list" data-queue-list>
                    ${queue.length === 0 
                        ? this.renderEmptyQueue()
                        : queue.map((file, index) => 
                            this.renderQueueItem(file, index)
                          ).join('')
                    }
                </div>
                
                <!-- Stats queue -->
                ${queue.length > 0 ? `
                    <div class="queue-stats">
                        <span>DurÃ©e totale: ${this.formatDuration(this.calculateTotalDuration(queue))}</span>
                    </div>
                ` : ''}
                
            </div>
        `;
    }
    
    renderEmptyQueue() {
        return `
            <div class="empty-queue-state">
                <div class="empty-icon">ðŸ“­</div>
                <p>La queue est vide</p>
                <small>Ajoutez des fichiers depuis la playlist</small>
            </div>
        `;
    }
    
    renderQueueItem(file, index) {
        const duration = this.formatDuration(file.duration || 0);
        
        return `
            <div class="queue-item" 
                 data-file-id="${file.id}"
                 data-queue-index="${index}">
                
                <div class="queue-item-number">${index + 1}</div>
                
                <div class="queue-item-info">
                    <div class="queue-item-name" title="${this.escapeHtml(file.name || file.filename)}">
                        ${this.escapeHtml(file.name || file.filename)}
                    </div>
                    ${duration ? `<div class="queue-item-duration">${duration}</div>` : ''}
                </div>
                
                <button class="btn-icon" 
                        onclick="app.playlistController.removeFromQueue(${index})"
                        title="Retirer de la queue">
                    âœ–ï¸
                </button>
                
            </div>
        `;
    }
    
    // ========================================================================
    // STATISTIQUES (Footer)
    // ========================================================================
    
    renderStats(state) {
        const playlist = state.currentPlaylist;
        const fileCount = playlist?.files?.length || 0;
        const totalDuration = playlist?.duration || this.calculateTotalDuration(playlist?.files || []);
        const queueCount = state.queue?.length || 0;
        
        return `
            <div class="playlist-stats">
                
                <div class="stat-item">
                    <span class="stat-label">Playlist courante</span>
                    <span class="stat-value">${playlist?.name || 'Aucune'}</span>
                </div>
                
                <div class="stat-item">
                    <span class="stat-label">Fichiers</span>
                    <span class="stat-value">${fileCount}</span>
                </div>
                
                <div class="stat-item">
                    <span class="stat-label">DurÃ©e totale</span>
                    <span class="stat-value">${this.formatDuration(totalDuration)}</span>
                </div>
                
                <div class="stat-item">
                    <span class="stat-label">Queue</span>
                    <span class="stat-value">${queueCount} fichier${queueCount > 1 ? 's' : ''}</span>
                </div>
                
            </div>
        `;
    }
    
    // ========================================================================
    // MÃ‰THODES DE MISE Ã€ JOUR DYNAMIQUE
    // ========================================================================
    
    /**
     * Met Ã  jour le fichier en cours de lecture
     */
    updateCurrentFile(file, index) {
        this.viewState.currentFile = file;
        
        // Mettre Ã  jour visuellement
        const items = this.container?.querySelectorAll('.playlist-file-item');
        items?.forEach((item, i) => {
            if (i === index) {
                item.classList.add('playing');
                const number = item.querySelector('.file-number');
                if (number) number.textContent = 'â–¶ï¸';
            } else {
                item.classList.remove('playing');
                const number = item.querySelector('.file-number');
                if (number) number.textContent = i + 1;
            }
        });
    }
    
    /**
     * Met Ã  jour l'Ã©tat du bouton shuffle
     */
    updateShuffleButton(enabled) {
        this.viewState.shuffleMode = enabled;
        
        const btn = this.container?.querySelector('[data-control="shuffle"]');
        if (btn) {
            if (enabled) {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
        }
    }
    
    /**
     * Met Ã  jour l'Ã©tat du bouton repeat
     */
    updateRepeatButton(mode) {
        this.viewState.repeatMode = mode;
        
        const btn = this.container?.querySelector('[data-control="repeat"]');
        if (btn) {
            if (mode !== 'none') {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
            
            const icon = btn.querySelector('.icon');
            const label = btn.querySelector('.label');
            if (icon) icon.textContent = this.getRepeatIcon(mode);
            if (label) label.textContent = this.getRepeatLabel(mode);
            
            btn.title = `Mode rÃ©pÃ©tition: ${mode}`;
        }
    }
    
    /**
     * Met Ã  jour l'Ã©tat du bouton auto-advance
     */
    updateAutoAdvanceButton(enabled) {
        this.viewState.autoAdvance = enabled;
        
        const btn = this.container?.querySelector('[data-control="auto-advance"]');
        if (btn) {
            if (enabled) {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
        }
    }
    
    /**
     * Met Ã  jour le statut de la queue
     */
    updateQueueStatus(count) {
        const badge = this.container?.querySelector('.queue-count');
        if (badge) {
            badge.textContent = count;
        }
        
        // Re-render queue si visible
        if (this.viewState.queueVisible) {
            this.refreshQueuePanel();
        }
    }
    
    /**
     * Affiche indicateur queue en cours de lecture
     */
    showQueuePlaying(isPlaying) {
        this.viewState.isPlayingQueue = isPlaying;
        
        const header = this.container?.querySelector('.queue-header h3');
        if (header) {
            const badge = header.querySelector('.playing-badge');
            if (isPlaying && !badge) {
                header.innerHTML += '<span class="playing-badge">En cours</span>';
            } else if (!isPlaying && badge) {
                badge.remove();
            }
        }
    }
    
    /**
     * Notification fin de lecture
     */
    showPlaybackComplete() {
        // Animation ou notification visuelle
        this.logger.info('PlaylistView', 'âœ… Playback complete');
    }
    
    // ========================================================================
    // DRAG & DROP
    // ========================================================================
    
    onDragStart(event, index) {
        this.viewState.draggedItem = index;
        event.dataTransfer.effectAllowed = 'move';
        event.dataTransfer.setData('text/plain', index);
        
        event.target.classList.add('dragging');
    }
    
    onDragOver(event) {
        event.preventDefault();
        event.dataTransfer.dropEffect = 'move';
    }
    
    onDrop(event, targetIndex) {
        event.preventDefault();
        
        const sourceIndex = this.viewState.draggedItem;
        
        if (sourceIndex !== null && sourceIndex !== targetIndex) {
            // Appeler controller pour rÃ©organiser
            if (window.app?.playlistController) {
                window.app.playlistController.reorderFiles(sourceIndex, targetIndex);
            }
        }
        
        // Cleanup
        const dragging = this.container?.querySelector('.dragging');
        if (dragging) {
            dragging.classList.remove('dragging');
        }
        
        this.viewState.draggedItem = null;
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * RafraÃ®chit uniquement le panel queue
     */
    refreshQueuePanel() {
        const queuePanel = this.container?.querySelector('.queue-panel-container');
        if (queuePanel) {
            queuePanel.innerHTML = this.renderQueuePanel(this.viewState).replace(
                /<div class="queue-panel-container">([\s\S]*)<\/div>$/,
                '$1'
            );
        }
    }
    
    /**
     * Calcule durÃ©e totale d'une liste de fichiers
     */
    calculateTotalDuration(files) {
        return files.reduce((sum, file) => sum + (file.duration || 0), 0);
    }
    
    /**
     * Formate une durÃ©e en ms vers HH:MM:SS
     */
    formatDuration(ms) {
        if (!ms || ms === 0) return '00:00';
        
        const seconds = Math.floor(ms / 1000);
        const minutes = Math.floor(seconds / 60);
        const hours = Math.floor(minutes / 60);
        
        const s = seconds % 60;
        const m = minutes % 60;
        
        if (hours > 0) {
            return `${hours}:${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
        }
        
        return `${m}:${s.toString().padStart(2, '0')}`;
    }
    
    /**
     * Ã‰chappe HTML
     */
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    /**
     * Toggle visibilitÃ© queue
     */
    toggleQueueVisibility() {
        this.viewState.queueVisible = !this.viewState.queueVisible;
        
        const sidebar = this.container?.querySelector('.playlist-sidebar.right');
        if (sidebar) {
            if (this.viewState.queueVisible) {
                sidebar.classList.add('visible');
                sidebar.classList.remove('collapsed');
            } else {
                sidebar.classList.remove('visible');
                sidebar.classList.add('collapsed');
            }
        }
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

window.PlaylistView = PlaylistView;
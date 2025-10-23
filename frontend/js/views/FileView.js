// ============================================================================
// Fichier: frontend/js/views/FileView.js
// Version: v1.1.0 - SELECTION & CONTEXT MENU
// Date: 2025-10-10
// Projet: midiMind v3.0 - SystÃƒÆ’Ã‚Â¨me d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Vue amÃƒÆ’Ã‚Â©liorÃƒÆ’Ã‚Â©e pour afficher la liste des fichiers MIDI.
//
// NOUVEAU v1.1.0:
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ SÃƒÆ’Ã‚Â©lection multiple (Ctrl+Click, Shift+Click, Ctrl+A)
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Menu contextuel clic droit
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Drag & drop vers playlist/queue
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Indicateur fichiers sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©s
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Actions bulk sur sÃƒÆ’Ã‚Â©lection
//
// Auteur: midiMind Team
// ============================================================================


class FileView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        // ÃƒÆ’Ã¢â‚¬Â°tat de la vue
        this.viewState = {
            files: [],
            selectedFiles: [],
            lastSelectedIndex: -1,
            contextMenuOpen: false,
            searchQuery: '',
            sortBy: 'name',
            sortOrder: 'asc'
        };
        
        // Configuration
        this.config = {
            autoRender: true,
            enableMultiSelect: true,
            enableContextMenu: true,
            enableDragDrop: true
        };
        
        this.logger = window.logger || console;
        this.init();
    }
    
    init() {
        this.setupGlobalEvents();
        this.logger.info('FileView', 'ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Âµ FileView v1.1.0 initialized with multi-select');
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
    buildTemplate(data = {}) {
        const state = { ...this.viewState, ...data };
        const selectedCount = state.selectedFiles.length;
        
        return `
            <div class="file-view-container">
                
                <!-- Header avec compteur sÃƒÆ’Ã‚Â©lection -->
                <div class="file-view-header">
                    <div class="file-header-left">
                        <h2 class="file-view-title">ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã‚Â Fichiers MIDI</h2>
                        <span class="file-count">${state.files.length} fichier(s)</span>
                        ${selectedCount > 0 ? `
                            <span class="selected-count">${selectedCount} sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©(s)</span>
                        ` : ''}
                    </div>
                    
                    <div class="file-header-right">
                        <!-- Barre de recherche -->
                        <div class="search-box">
                            <input type="text" 
                                   class="search-input" 
                                   placeholder="ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ‚Â Rechercher..."
                                   value="${state.searchQuery}"
                                   onkeyup="app.fileView.onSearch(this.value)">
                        </div>
                        
                        <!-- Actions bulk si sÃƒÆ’Ã‚Â©lection -->
                        ${selectedCount > 0 ? this.renderBulkActions(selectedCount) : ''}
                    </div>
                </div>
                
                <!-- Liste des fichiers -->
                <div class="file-list" 
                     data-file-list
                     oncontextmenu="return app.fileView.onContextMenu(event)">
                    ${state.files.length === 0 
                        ? this.renderEmptyState()
                        : state.files.map((file, index) => 
                            this.renderFileItem(file, index, state)
                          ).join('')
                    }
                </div>
                
                <!-- Menu contextuel -->
                ${this.renderContextMenu()}
                
            </div>
        `;
    }
    
    // ========================================================================
    // ITEMS FICHIERS
    // ========================================================================
    
    renderFileItem(file, index, state) {
        const isSelected = state.selectedFiles.some(f => f.id === file.id);
        const duration = this.formatDuration(file.duration || 0);
        
        return `
            <div class="file-item ${isSelected ? 'selected' : ''}" 
                 data-file-id="${file.id}"
                 data-file-index="${index}"
                 draggable="${this.config.enableDragDrop}"
                 onclick="app.fileView.onFileClick(event, ${index})"
                 ondblclick="app.fileView.onFileDoubleClick('${file.id}')"
                 ondragstart="app.fileView.onFileDragStart(event, ${index})"
                 ondragend="app.fileView.onFileDragEnd(event)">
                
                <!-- Checkbox sÃƒÆ’Ã‚Â©lection -->
                ${this.config.enableMultiSelect ? `
                    <div class="file-checkbox">
                        <input type="checkbox" 
                               ${isSelected ? 'checked' : ''}
                               onclick="event.stopPropagation(); app.fileView.toggleFileSelection(${index})">
                    </div>
                ` : ''}
                
                <!-- IcÃƒÆ’Ã‚Â´ne -->
                <div class="file-icon">ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Âµ</div>
                
                <!-- Infos -->
                <div class="file-info">
                    <div class="file-name" title="${this.escapeHtml(file.name)}">
                        ${this.escapeHtml(file.name)}
                    </div>
                    <div class="file-metadata">
                        ${duration ? `<span>ÃƒÂ¢Ã‚ÂÃ‚Â±ÃƒÂ¯Ã‚Â¸Ã‚Â ${duration}</span>` : ''}
                        ${file.trackCount ? `<span>ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¹ ${file.trackCount} pistes</span>` : ''}
                        ${file.bpm ? `<span>ÃƒÂ°Ã…Â¸Ã‚Â¥Ã‚Â ${file.bpm} BPM</span>` : ''}
                    </div>
                </div>
                
                <!-- Taille -->
                ${file.size ? `
                    <div class="file-size">${this.formatFileSize(file.size)}</div>
                ` : ''}
                
                <!-- Actions rapides -->
                <div class="file-actions">
                    <button class="btn-icon" 
                            onclick="event.stopPropagation(); app.fileView.playFile('${file.id}')"
                            title="Lire">
                        ÃƒÂ¢Ã¢â‚¬â€œÃ‚Â¶ÃƒÂ¯Ã‚Â¸Ã‚Â
                    </button>
                </div>
                
            </div>
        `;
    }
    
    // ========================================================================
    // ACTIONS BULK
    // ========================================================================
    
    renderBulkActions(count) {
        return `
            <div class="bulk-actions">
                <button class="btn btn-sm btn-primary" 
                        onclick="app.fileView.addSelectedToPlaylist()"
                        title="Ajouter ÃƒÆ’Ã‚Â  la playlist">
                    ÃƒÂ¢Ã…Â¾Ã¢â‚¬Â¢ Playlist
                </button>
                <button class="btn btn-sm btn-secondary" 
                        onclick="app.fileView.addSelectedToQueue()"
                        title="Ajouter ÃƒÆ’Ã‚Â  la queue">
                    ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã¢â‚¬Â¹ Queue
                </button>
                <button class="btn btn-sm btn-danger" 
                        onclick="app.fileView.clearSelection()"
                        title="DÃƒÆ’Ã‚Â©sÃƒÆ’Ã‚Â©lectionner tout">
                    ÃƒÂ¢Ã…â€œÃ¢â‚¬â€œÃƒÂ¯Ã‚Â¸Ã‚Â
                </button>
            </div>
        `;
    }
    
    // ========================================================================
    // MENU CONTEXTUEL
    // ========================================================================
    
    renderContextMenu() {
        return `
            <div id="file-context-menu" 
                 class="context-menu" 
                 style="display: none;"
                 data-context-menu>
                <div class="context-menu-item" onclick="app.fileView.contextPlay()">
                    <span class="icon">ÃƒÂ¢Ã¢â‚¬â€œÃ‚Â¶ÃƒÂ¯Ã‚Â¸Ã‚Â</span>
                    Lire
                </div>
                <div class="context-menu-item" onclick="app.fileView.contextAddToPlaylist()">
                    <span class="icon">ÃƒÂ¢Ã…Â¾Ã¢â‚¬Â¢</span>
                    Ajouter ÃƒÆ’Ã‚Â  la playlist
                </div>
                <div class="context-menu-item" onclick="app.fileView.contextAddToQueue()">
                    <span class="icon">ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã¢â‚¬Â¹</span>
                    Ajouter ÃƒÆ’Ã‚Â  la queue
                </div>
                <div class="context-menu-divider"></div>
                <div class="context-menu-item" onclick="app.fileView.contextInfo()">
                    <span class="icon">ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¹ÃƒÂ¯Ã‚Â¸Ã‚Â</span>
                    Informations
                </div>
                <div class="context-menu-item danger" onclick="app.fileView.contextDelete()">
                    <span class="icon">ÃƒÂ°Ã…Â¸Ã¢â‚¬â€Ã¢â‚¬ËœÃƒÂ¯Ã‚Â¸Ã‚Â</span>
                    Supprimer
                </div>
            </div>
        `;
    }
    
    renderEmptyState() {
        return `
            <div class="empty-state">
                <div class="empty-icon">ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã‚Â­</div>
                <p>Aucun fichier</p>
            </div>
        `;
    }
    
    // ========================================================================
    // SÃƒÆ’Ã¢â‚¬Â°LECTION MULTIPLE
    // ========================================================================
    
    /**
     * GÃƒÆ’Ã‚Â¨re le clic sur un fichier avec Ctrl/Shift
     */
    onFileClick(event, index) {
        event.preventDefault();
        
        const file = this.viewState.files[index];
        if (!file) return;
        
        if (event.ctrlKey || event.metaKey) {
            // Ctrl+Click : Toggle individuel
            this.toggleFileSelection(index);
        } else if (event.shiftKey && this.viewState.lastSelectedIndex >= 0) {
            // Shift+Click : SÃƒÆ’Ã‚Â©lection de plage
            this.selectRange(this.viewState.lastSelectedIndex, index);
        } else {
            // Click simple : SÃƒÆ’Ã‚Â©lection unique
            this.selectSingleFile(index);
        }
        
        this.viewState.lastSelectedIndex = index;
    }
    
    /**
     * Toggle sÃƒÆ’Ã‚Â©lection d'un fichier
     */
    toggleFileSelection(index) {
        const file = this.viewState.files[index];
        const isSelected = this.viewState.selectedFiles.some(f => f.id === file.id);
        
        if (isSelected) {
            this.viewState.selectedFiles = this.viewState.selectedFiles.filter(
                f => f.id !== file.id
            );
        } else {
            this.viewState.selectedFiles.push(file);
        }
        
        this.updateFileItemState(index);
        this.updateHeader();
    }
    
    /**
     * SÃƒÆ’Ã‚Â©lectionne un seul fichier
     */
    selectSingleFile(index) {
        const file = this.viewState.files[index];
        this.viewState.selectedFiles = [file];
        this.updateAllFilesState();
        this.updateHeader();
    }
    
    /**
     * SÃƒÆ’Ã‚Â©lectionne une plage de fichiers (Shift+Click)
     */
    selectRange(startIndex, endIndex) {
        const start = Math.min(startIndex, endIndex);
        const end = Math.max(startIndex, endIndex);
        
        this.viewState.selectedFiles = this.viewState.files.slice(start, end + 1);
        this.updateAllFilesState();
        this.updateHeader();
    }
    
    /**
     * SÃƒÆ’Ã‚Â©lectionne tous les fichiers (Ctrl+A)
     */
    selectAll() {
        this.viewState.selectedFiles = [...this.viewState.files];
        this.updateAllFilesState();
        this.updateHeader();
    }
    
    /**
     * DÃƒÆ’Ã‚Â©sÃƒÆ’Ã‚Â©lectionne tout
     */
    clearSelection() {
        this.viewState.selectedFiles = [];
        this.updateAllFilesState();
        this.updateHeader();
    }
    
    // ========================================================================
    // MENU CONTEXTUEL
    // ========================================================================
    
    /**
     * Affiche le menu contextuel
     */
    onContextMenu(event) {
        if (!this.config.enableContextMenu) return true;
        
        event.preventDefault();
        
        // Trouver le fichier cliquÃƒÆ’Ã‚Â©
        const fileItem = event.target.closest('.file-item');
        if (!fileItem) return false;
        
        const fileId = fileItem.getAttribute('data-file-id');
        const fileIndex = parseInt(fileItem.getAttribute('data-file-index'));
        
        // Si le fichier n'est pas dans la sÃƒÆ’Ã‚Â©lection, le sÃƒÆ’Ã‚Â©lectionner
        const isSelected = this.viewState.selectedFiles.some(f => f.id === fileId);
        if (!isSelected) {
            this.selectSingleFile(fileIndex);
        }
        
        // Afficher le menu
        this.showContextMenu(event.clientX, event.clientY);
        
        return false;
    }
    
    /**
     * Affiche le menu aux coordonnÃƒÆ’Ã‚Â©es donnÃƒÆ’Ã‚Â©es
     */
    showContextMenu(x, y) {
        const menu = document.querySelector('[data-context-menu]');
        if (!menu) return;
        
        // Positionner et afficher
        menu.style.left = x + 'px';
        menu.style.top = y + 'px';
        menu.style.display = 'block';
        
        this.viewState.contextMenuOpen = true;
    }
    
    /**
     * Cache le menu contextuel
     */
    hideContextMenu() {
        const menu = document.querySelector('[data-context-menu]');
        if (menu) {
            menu.style.display = 'none';
        }
        this.viewState.contextMenuOpen = false;
    }
    
    // ========================================================================
    // ACTIONS MENU CONTEXTUEL
    // ========================================================================
    
    contextPlay() {
        const selected = this.viewState.selectedFiles;
        if (selected.length > 0) {
            this.playFile(selected[0].id);
        }
        this.hideContextMenu();
    }
    
    contextAddToPlaylist() {
        this.addSelectedToPlaylist();
        this.hideContextMenu();
    }
    
    contextAddToQueue() {
        this.addSelectedToQueue();
        this.hideContextMenu();
    }
    
    contextInfo() {
        const selected = this.viewState.selectedFiles;
        if (selected.length > 0) {
            this.showFileInfo(selected[0].id);
        }
        this.hideContextMenu();
    }
    
    contextDelete() {
        this.deleteSelected();
        this.hideContextMenu();
    }
    
    // ========================================================================
    // DRAG & DROP
    // ========================================================================
    
    onFileDragStart(event, index) {
        const file = this.viewState.files[index];
        
        // Si le fichier n'est pas sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©, le sÃƒÆ’Ã‚Â©lectionner
        const isSelected = this.viewState.selectedFiles.some(f => f.id === file.id);
        if (!isSelected) {
            this.selectSingleFile(index);
        }
        
        // Stocker les IDs des fichiers sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©s
        const fileIds = this.viewState.selectedFiles.map(f => f.id);
        event.dataTransfer.setData('application/json', JSON.stringify(fileIds));
        event.dataTransfer.effectAllowed = 'copy';
        
        // Effet visuel
        event.target.classList.add('dragging');
    }
    
    onFileDragEnd(event) {
        event.target.classList.remove('dragging');
    }
    
    // ========================================================================
    // ACTIONS
    // ========================================================================
    
    onFileDoubleClick(fileId) {
        this.playFile(fileId);
    }
    
    playFile(fileId) {
        if (window.app?.playlistController) {
            window.app.playlistController.playFile(fileId);
        }
    }
    
    addSelectedToPlaylist() {
        const fileIds = this.viewState.selectedFiles.map(f => f.id);
        if (fileIds.length === 0) return;
        
        if (window.app?.playlistController) {
            window.app.playlistController.addMultipleFiles(fileIds);
        }
        
        this.showSuccess(`${fileIds.length} fichier(s) ajoutÃƒÆ’Ã‚Â©(s) ÃƒÆ’Ã‚Â  la playlist`);
    }
    
    addSelectedToQueue() {
        const fileIds = this.viewState.selectedFiles.map(f => f.id);
        if (fileIds.length === 0) return;
        
        if (window.app?.playlistController) {
            window.app.playlistController.addMultipleToQueue(fileIds);
        }
        
        this.showSuccess(`${fileIds.length} fichier(s) ajoutÃƒÆ’Ã‚Â©(s) ÃƒÆ’Ã‚Â  la queue`);
    }
    
    deleteSelected() {
        const count = this.viewState.selectedFiles.length;
        if (count === 0) return;
        
        const confirmed = confirm(
            `Supprimer ${count} fichier(s) sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©(s) ?\n\nCette action est irrÃƒÆ’Ã‚Â©versible.`
        );
        
        if (!confirmed) return;
        
        const fileIds = this.viewState.selectedFiles.map(f => f.id);
        
        if (window.app?.fileController) {
            window.app.fileController.deleteMultiple(fileIds);
        }
        
        this.clearSelection();
    }
    
    showFileInfo(fileId) {
        if (window.app?.fileController) {
            window.app.fileController.showFileInfo(fileId);
        }
    }
    
    onSearch(query) {
        this.viewState.searchQuery = query;
        // Trigger search via controller
        if (window.app?.fileController) {
            window.app.fileController.searchFiles(query);
        }
    }
    
    // ========================================================================
    // MISE ÃƒÆ’Ã¢â€šÂ¬ JOUR UI
    // ========================================================================
    
    updateFileItemState(index) {
        const fileItem = this.container?.querySelector(
            `[data-file-index="${index}"]`
        );
        
        if (!fileItem) return;
        
        const file = this.viewState.files[index];
        const isSelected = this.viewState.selectedFiles.some(f => f.id === file.id);
        
        if (isSelected) {
            fileItem.classList.add('selected');
        } else {
            fileItem.classList.remove('selected');
        }
        
        const checkbox = fileItem.querySelector('input[type="checkbox"]');
        if (checkbox) {
            checkbox.checked = isSelected;
        }
    }
    
    updateAllFilesState() {
        this.viewState.files.forEach((_, index) => {
            this.updateFileItemState(index);
        });
    }
    
    updateHeader() {
        // Re-render juste le header si possible, sinon full render
        if (this.container) {
            const header = this.container.querySelector('.file-view-header');
            if (header) {
                const selectedCount = this.viewState.selectedFiles.length;
                const countEl = header.querySelector('.selected-count');
                const bulkActions = header.querySelector('.bulk-actions');
                
                if (selectedCount > 0) {
                    if (!countEl) {
                        const newCount = document.createElement('span');
                        newCount.className = 'selected-count';
                        newCount.textContent = `${selectedCount} sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©(s)`;
                        header.querySelector('.file-header-left').appendChild(newCount);
                    } else {
                        countEl.textContent = `${selectedCount} sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©(s)`;
                    }
                    
                    if (!bulkActions) {
                        const rightSection = header.querySelector('.file-header-right');
                        const div = document.createElement('div');
                        div.innerHTML = this.renderBulkActions(selectedCount);
                        rightSection.appendChild(div.firstChild);
                    }
                } else {
                    if (countEl) countEl.remove();
                    if (bulkActions) bulkActions.remove();
                }
            }
        }
    }
    
    // ========================================================================
    // ÃƒÆ’Ã¢â‚¬Â°VÃƒÆ’Ã¢â‚¬Â°NEMENTS GLOBAUX
    // ========================================================================
    
    setupGlobalEvents() {
        // Ctrl+A pour sÃƒÆ’Ã‚Â©lectionner tout
        document.addEventListener('keydown', (e) => {
            if ((e.ctrlKey || e.metaKey) && e.key === 'a') {
                // Seulement si focus dans file view
                if (this.container?.contains(document.activeElement)) {
                    e.preventDefault();
                    this.selectAll();
                }
            }
        });
        
        // Clic ailleurs ferme le menu contextuel
        document.addEventListener('click', (e) => {
            if (this.viewState.contextMenuOpen) {
                const menu = document.querySelector('[data-context-menu]');
                if (menu && !menu.contains(e.target)) {
                    this.hideContextMenu();
                }
            }
        });
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    formatDuration(ms) {
        if (!ms) return '00:00';
        const seconds = Math.floor(ms / 1000);
        const minutes = Math.floor(seconds / 60);
        const s = seconds % 60;
        return `${minutes}:${s.toString().padStart(2, '0')}`;
    }
    
    formatFileSize(bytes) {
        if (bytes === 0) return '0 B';
        const units = ['B', 'KB', 'MB', 'GB'];
        const k = 1024;
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return `${(bytes / Math.pow(k, i)).toFixed(1)} ${units[i]}`;
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    showSuccess(message) {
        console.log('ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦', message);
        if (window.app?.notifications) {
            window.app.notifications.success(message);
        }
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
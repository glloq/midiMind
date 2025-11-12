// ============================================================================
// Fichier: frontend/js/views/FileView.js
// Chemin réel: frontend/js/views/FileView.js
// Version: v4.3.1 - FIX DOUBLE LISTENERS
// Date: 2025-11-12
// ============================================================================
// CORRECTIONS v4.3.1:
// ✅ CRITIQUE: Suppression appel dupliqué setupEventBusListeners (ligne 562)
// ✅ Un seul appel dans init() - pas dans attachEvents()
// ✅ Toutes fonctionnalités préservées
// ============================================================================

class FileView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        this.logger = window.logger || console;
        
        // État spécifique à la vue
        this.viewState = {
            files: [],
            selectedFile: null,
            currentPath: '/midi',
            isLoading: false,
            sortBy: 'name',
            sortOrder: 'asc',
            filter: ''
        };
        
        // Flag pour éviter double init
        this._listenersAttached = false;
        
        this.log('debug', 'FileView', '✅ FileView v4.3.1 constructed');
    }
    
    init() {
        if (!this.container) {
            this.log('error', 'FileView', 'Cannot initialize: container not found');
            return;
        }
        
        try {
            this.render();
            this.attachEvents();
            
            // ✅ Attacher listeners EventBus UNE SEULE FOIS
            if (!this._listenersAttached) {
                this.setupEventBusListeners();
                this._listenersAttached = true;
            }
            
            this.state.initialized = true;
            this.log('info', 'FileView', '✅ FileView v4.3.1 initialized (NO DOUBLE LISTENERS)');
            
        } catch (error) {
            this.log('error', 'FileView', 'Initialization failed:', error);
            this.state.error = error.message;
        }
    }
    
    buildTemplate(data = {}) {
        const state = { ...this.viewState, ...data };
        
        return `
            <div class="file-view-container">
                <div class="page-header">
                    <h1>📁 Fichiers MIDI</h1>
                    <div class="header-actions">
                        <button class="btn-upload" data-action="upload-file">
                            📤 Upload
                        </button>
                        <button class="btn-refresh" data-action="refresh-files">
                            🔄 Actualiser
                        </button>
                    </div>
                </div>
                
                <div class="toolbar">
                    ${this.buildToolbar(state)}
                </div>
                
                <div class="content-area">
                    <div class="file-list">
                        ${state.isLoading ? this.buildLoadingState() : this.buildFileList(state)}
                    </div>
                    
                    ${state.selectedFile ? this.buildFileDetails(state.selectedFile) : ''}
                </div>
            </div>
        `;
    }
    
    buildToolbar(state) {
        return `
            <div class="toolbar-section">
                <input 
                    type="text" 
                    class="search-input" 
                    placeholder="🔍 Rechercher..." 
                    data-action="filter-files"
                    value="${this.escapeHtml(state.filter)}"
                />
                
                <select class="sort-select" data-action="change-sort">
                    <option value="name" ${state.sortBy === 'name' ? 'selected' : ''}>Nom</option>
                    <option value="date" ${state.sortBy === 'date' ? 'selected' : ''}>Date</option>
                    <option value="size" ${state.sortBy === 'size' ? 'selected' : ''}>Taille</option>
                </select>
                
                <button 
                    class="btn-icon" 
                    data-action="toggle-sort-order"
                    title="Ordre de tri"
                >
                    ${state.sortOrder === 'asc' ? '⬆️' : '⬇️'}
                </button>
            </div>
        `;
    }
    
    buildFileList(state) {
        if (!state.files || !Array.isArray(state.files)) {
            return this.buildEmptyState();
        }
        
        const filteredFiles = this.filterFiles(state.files, state.filter);
        const sortedFiles = this.sortFiles(filteredFiles, state.sortBy, state.sortOrder);
        
        if (sortedFiles.length === 0) {
            return this.buildEmptyState();
        }
        
        return `
            <div class="file-list-compact">
                ${sortedFiles.map(file => this.buildFileRow(file)).join('')}
            </div>
        `;
    }
    
    buildFileRow(file) {
        const isSelected = this.viewState.selectedFile?.path === file.path;
        
        return `
            <div 
                class="file-row ${isSelected ? 'selected' : ''}" 
                data-file-path="${this.escapeHtml(file.path || file.name)}"
            >
                <div class="file-icon">🎵</div>
                <div class="file-info">
                    <div class="file-name" title="${this.escapeHtml(file.name)}">
                        ${this.escapeHtml(file.name)}
                    </div>
                    <div class="file-meta">
                        ${this.formatFileSize(file.size)} • ${this.formatDate(file.modified)}
                        ${file.tracks ? ` • ${file.tracks} pistes` : ''}
                    </div>
                </div>
                <div class="file-actions">
                    <button class="btn-icon" data-action="select-file" title="Détails">
                        📋
                    </button>
                    <button class="btn-icon" data-action="edit-file" title="Éditer">
                        ✏️
                    </button>
                    <button class="btn-icon" data-action="route-file" title="Router">
                        🔀
                    </button>
                    <button class="btn-icon" data-action="play-file" title="Jouer">
                        ▶️
                    </button>
                    <button class="btn-icon btn-danger" data-action="delete-file" title="Supprimer">
                        🗑️
                    </button>
                </div>
            </div>
        `;
    }
    
    buildFileDetails(file) {
        return `
            <div class="file-details">
                <div class="details-header">
                    <h3>📄 Détails du fichier</h3>
                    <button class="btn-close" data-action="close-details">✕</button>
                </div>
                
                <div class="details-content">
                    <div class="detail-row">
                        <span class="detail-label">Nom:</span>
                        <span class="detail-value">${this.escapeHtml(file.name)}</span>
                    </div>
                    <div class="detail-row">
                        <span class="detail-label">Chemin:</span>
                        <span class="detail-value">${this.escapeHtml(file.path || file.name)}</span>
                    </div>
                    <div class="detail-row">
                        <span class="detail-label">Taille:</span>
                        <span class="detail-value">${this.formatFileSize(file.size)}</span>
                    </div>
                    <div class="detail-row">
                        <span class="detail-label">Modifié:</span>
                        <span class="detail-value">${this.formatDate(file.modified)}</span>
                    </div>
                    ${file.tracks ? `
                    <div class="detail-row">
                        <span class="detail-label">Pistes:</span>
                        <span class="detail-value">${file.tracks}</span>
                    </div>
                    ` : ''}
                    ${file.duration ? `
                    <div class="detail-row">
                        <span class="detail-label">Durée:</span>
                        <span class="detail-value">${this.formatDuration(file.duration)}</span>
                    </div>
                    ` : ''}
                </div>
                
                <div class="details-actions">
                    <button class="btn" data-action="load-file">
                        ✏️ Éditer
                    </button>
                    <button class="btn" data-action="play-file">
                        ▶️ Jouer
                    </button>
                </div>
            </div>
        `;
    }
    
    buildLoadingState() {
        return `
            <div class="empty-state">
                <div class="spinner"></div>
                <p>Chargement...</p>
            </div>
        `;
    }
    
    buildEmptyState() {
        return `
            <div class="empty-state">
                <p>📭 Aucun fichier MIDI</p>
                <button class="btn-upload" data-action="upload-file">
                    📤 Uploader des fichiers
                </button>
            </div>
        `;
    }
    
    filterFiles(files, query) {
        if (!Array.isArray(files)) return [];
        if (!query || query.trim() === '') return files;
        
        const lowerQuery = query.toLowerCase();
        return files.filter(file => 
            file.name?.toLowerCase().includes(lowerQuery) ||
            file.path?.toLowerCase().includes(lowerQuery)
        );
    }
    
    sortFiles(files, sortBy, sortOrder) {
        if (!Array.isArray(files)) return [];
        
        const sorted = [...files];
        
        sorted.sort((a, b) => {
            let comparison = 0;
            
            switch (sortBy) {
                case 'name':
                    comparison = (a.name || '').localeCompare(b.name || '');
                    break;
                case 'date':
                    comparison = (a.modified || 0) - (b.modified || 0);
                    break;
                case 'size':
                    comparison = (a.size || 0) - (b.size || 0);
                    break;
                default:
                    comparison = 0;
            }
            
            return sortOrder === 'asc' ? comparison : -comparison;
        });
        
        return sorted;
    }
    
    formatFileSize(bytes) {
        if (!bytes || bytes === 0) return '0 B';
        
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        
        return Math.round(bytes / Math.pow(k, i) * 10) / 10 + ' ' + sizes[i];
    }
    
    formatDate(timestamp) {
        if (!timestamp) return 'N/A';
        
        let date;
        if (typeof timestamp === 'number') {
            date = new Date(timestamp);
        } else if (typeof timestamp === 'string') {
            date = new Date(timestamp);
        } else {
            return 'N/A';
        }
        
        if (isNaN(date.getTime())) return 'N/A';
        
        return date.toLocaleDateString('fr-FR', {
            day: '2-digit',
            month: '2-digit',
            year: 'numeric'
        });
    }
    
    formatDuration(seconds) {
        if (!seconds) return '0:00';
        
        const mins = Math.floor(seconds / 60);
        const secs = Math.floor(seconds % 60);
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    }
    
    escapeHtml(text) {
        if (!text) return '';
        
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    attachEvents() {
        if (!this.container) return;
        
        this.container.addEventListener('click', (e) => {
            const action = e.target.closest('[data-action]')?.dataset.action;
            if (!action) return;
            
            const fileRow = e.target.closest('.file-row');
            const filePath = fileRow?.dataset.filePath;
            
            switch (action) {
                case 'upload-file':
                    this.handleUpload();
                    break;
                case 'refresh-files':
                    this.handleRefresh();
                    break;
                case 'select-file':
                    if (filePath) this.handleSelectFile(filePath);
                    break;
                case 'edit-file':
                    if (filePath) this.handleEditFile(filePath);
                    break;
                case 'route-file':
                    if (filePath) this.handleRouteFile(filePath);
                    break;
                case 'play-file':
                    if (filePath) this.handlePlayFile(filePath);
                    break;
                case 'delete-file':
                    if (filePath) this.handleDeleteFile(filePath);
                    break;
                case 'load-file':
                    this.handleLoadFile();
                    break;
                case 'close-details':
                    this.handleCloseDetails();
                    break;
                case 'toggle-sort-order':
                    this.viewState.sortOrder = this.viewState.sortOrder === 'asc' ? 'desc' : 'asc';
                    this.render();
                    break;
            }
        });
        
        const searchInput = this.container.querySelector('[data-action="filter-files"]');
        if (searchInput) {
            searchInput.addEventListener('input', (e) => {
                this.viewState.filter = e.target.value;
                this.render();
            });
        }
        
        const sortSelect = this.container.querySelector('[data-action="change-sort"]');
        if (sortSelect) {
            sortSelect.addEventListener('change', (e) => {
                this.viewState.sortBy = e.target.value;
                this.render();
            });
        }
        
        // ✅ PAS d'appel à setupEventBusListeners ici !
        // Appelé uniquement dans init() pour éviter double listeners
    }
    
    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        this.eventBus.on('files:list-updated', (data) => {
            this.log('info', 'FileView', `Received ${data.files?.length || 0} files`);
            this.viewState.files = data.files || [];
            this.viewState.isLoading = false;
            this.render();
        });
        
        this.eventBus.on('file:uploaded', (data) => {
            this.log('info', 'FileView', `File uploaded: ${data.file_path}`);
            this.handleRefresh();
        });
        
        this.eventBus.on('file:deleted', (data) => {
            this.log('info', 'FileView', `File deleted: ${data.file_path}`);
            this.handleRefresh();
        });
        
        this.eventBus.on('files:error', (data) => {
            this.log('error', 'FileView', `Error: ${data.error}`);
            this.viewState.isLoading = false;
            this.render();
        });
    }
    
    handleUpload() {
        let input = document.getElementById('file-upload-input');
        
        if (!input) {
            input = document.createElement('input');
            input.id = 'file-upload-input';
            input.type = 'file';
            input.accept = '.mid,.midi';
            input.multiple = true;
            input.style.display = 'none';
            document.body.appendChild(input);
            
            this.log('debug', 'FileView', 'Created permanent file input');
            
            input.addEventListener('change', (e) => {
                const files = Array.from(e.target.files);
                
                if (files.length === 0) return;
                
                this.log('info', 'FileView', `Selected ${files.length} file(s)`);
                
                for (const file of files) {
                    try {
                        if (this.eventBus) {
                            this.eventBus.emit('file:upload', {
                                file: file
                            });
                            this.log('info', 'FileView', `Upload requested: ${file.name}`);
                        }
                    } catch (error) {
                        this.log('error', 'FileView', `Upload error: ${error.message}`);
                    }
                }
                
                input.value = '';
            });
        }
        
        this.log('debug', 'FileView', 'Triggering file selector');
        input.click();
    }
    
    handleRefresh() {
        this.viewState.isLoading = true;
        this.render();
        
        if (this.eventBus) {
            this.eventBus.emit('file:list_requested', {
                path: this.viewState.currentPath
            });
        }
    }
    
    handleSelectFile(filePath) {
        const file = this.viewState.files.find(f => 
            f.path === filePath || f.name === filePath
        );
        
        if (file) {
            this.viewState.selectedFile = file;
            this.render();
            
            if (this.eventBus) {
                this.eventBus.emit('file:selected', { file });
            }
        }
    }
    
    handleEditFile(filePath) {
        this.log('info', 'FileView', `Edit requested: ${filePath}`);
        
        if (this.eventBus) {
            this.eventBus.emit('file:load_in_editor', {
                file_path: filePath
            });
        }
        
        if (window.app?.router) {
            window.app.router.navigateTo('/editor');
        }
    }
    
    handleRouteFile(filePath) {
        this.log('info', 'FileView', `Routing requested: ${filePath}`);
        
        if (this.eventBus) {
            this.eventBus.emit('routing:configure', {
                file_path: filePath
            });
        }
    }
    
    handlePlayFile(filePath) {
        if (this.eventBus) {
            this.eventBus.emit('file:play_requested', {
                file_path: filePath
            });
        }
    }
    
    handleDeleteFile(filePath) {
        if (!confirm(`Supprimer ${filePath} ?`)) return;
        
        if (this.eventBus) {
            this.eventBus.emit('file:delete_requested', {
                file_path: filePath
            });
        }
    }
    
    handleLoadFile() {
        if (!this.viewState.selectedFile) return;
        
        const filePath = this.viewState.selectedFile.path || this.viewState.selectedFile.name;
        
        if (this.eventBus) {
            this.eventBus.emit('file:load_in_editor', {
                file_path: filePath
            });
        }
        
        if (window.app?.router) {
            window.app.router.navigateTo('/editor');
        }
    }
    
    handleCloseDetails() {
        this.viewState.selectedFile = null;
        this.render();
    }
    
    updateFiles(files) {
        this.viewState.files = files || [];
        this.viewState.isLoading = false;
        this.render();
    }
    
    setLoading(loading) {
        this.viewState.isLoading = loading;
        this.render();
    }
    
    getSelectedFile() {
        return this.viewState.selectedFile;
    }
    
    setSelectedFile(file) {
        this.viewState.selectedFile = file;
        this.render();
    }
    
    destroy() {
        const input = document.getElementById('file-upload-input');
        if (input) {
            input.remove();
        }
        
        if (super.destroy) {
            super.destroy();
        }
    }
    
    log(level, context, ...args) {
        if (!this.logger) return;
        
        const prefix = `[FileView${context ? ':' + context : ''}]`;
        
        if (typeof this.logger[level] === 'function') {
            this.logger[level](prefix, ...args);
        } else {
            console[level](prefix, ...args);
        }
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileView;
}

if (typeof window !== 'undefined') {
    window.FileView = FileView;
}
// ============================================================================
// Fichier: frontend/js/views/FileView.js
// Version: v4.0.0 - CONFORMITÉ API DOCUMENTATION
// Date: 2025-11-02
// ============================================================================
// AMÉLIORATIONS v4.0.0:
// ✅ Conformité API v4.2.2 (files.list, files.read, files.write, files.delete)
// ✅ Gestion réponses {success: true, data: {...}}
// ✅ Upload et gestion fichiers MIDI
// ✅ Prévisualisation et métadonnées
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
            sortBy: 'name', // 'name', 'date', 'size'
            sortOrder: 'asc', // 'asc', 'desc'
            filter: '' // filtre de recherche
        };
        
        this.log('info', 'FileView', '✅ FileView v4.0.0 initialized (API-compliant)');
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
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
                
                <!-- Barre de contrôle -->
                <div class="files-toolbar">
                    <div class="files-path">
                        <span class="path-label">Dossier:</span>
                        <input type="text" class="path-input" value="${state.currentPath}" 
                               data-action="change-path" />
                    </div>
                    
                    <div class="files-search">
                        <input type="text" class="search-input" placeholder="Rechercher..." 
                               value="${state.filter}" data-action="filter-files" />
                    </div>
                    
                    <div class="files-sort">
                        <select class="sort-select" data-action="change-sort">
                            <option value="name" ${state.sortBy === 'name' ? 'selected' : ''}>Nom</option>
                            <option value="date" ${state.sortBy === 'date' ? 'selected' : ''}>Date</option>
                            <option value="size" ${state.sortBy === 'size' ? 'selected' : ''}>Taille</option>
                        </select>
                        <button class="btn-sort-order" data-action="toggle-sort-order">
                            ${state.sortOrder === 'asc' ? '↑' : '↓'}
                        </button>
                    </div>
                    
                    <div class="files-count">
                        <span>${state.files.length} fichier(s)</span>
                    </div>
                </div>
                
                <!-- Contenu des fichiers -->
                <div class="files-section">
                    <div class="files-content">
                        ${state.isLoading ? this.renderLoading() : this.renderFilesList(state)}
                    </div>
                </div>
                
                <!-- Panneau de détails -->
                <div class="file-details" id="fileDetails">
                    ${state.selectedFile ? this.renderFileDetails(state.selectedFile) : this.renderNoSelection()}
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // RENDERING
    // ========================================================================
    
    renderLoading() {
        return `
            <div class="files-loading">
                <div class="spinner"></div>
                <p>Chargement des fichiers...</p>
            </div>
        `;
    }
    
    renderFilesList(state) {
        let files = this.filterFiles(state.files, state.filter);
        files = this.sortFiles(files, state.sortBy, state.sortOrder);
        
        if (files.length === 0) {
            return `
                <div class="files-empty">
                    <div class="empty-icon">📂</div>
                    <p>Aucun fichier MIDI trouvé</p>
                    <p class="text-muted">Uploadez des fichiers ou vérifiez le chemin</p>
                </div>
            `;
        }
        
        return `
            <div class="files-grid">
                ${files.map(file => this.renderFileCard(file, state.selectedFile)).join('')}
            </div>
        `;
    }
    
    renderFileCard(file, selectedFile) {
        const isSelected = selectedFile && 
                          (selectedFile.path === file.path || selectedFile.name === file.name);
        
        const size = file.size ? this.formatFileSize(file.size) : '—';
        const date = file.modified || file.created;
        const dateStr = date ? this.formatDate(date) : '—';
        
        return `
            <div class="file-card ${isSelected ? 'selected' : ''}" 
                 data-file-path="${file.path || file.name}">
                <div class="file-icon">🎵</div>
                <div class="file-info">
                    <div class="file-name" title="${file.name}">${file.name}</div>
                    <div class="file-meta">
                        <span class="file-size">${size}</span>
                        <span>•</span>
                        <span class="file-date">${dateStr}</span>
                    </div>
                </div>
                <div class="file-actions">
                    <button class="btn-icon" data-action="select-file" title="Sélectionner">
                        📋
                    </button>
                    <button class="btn-icon" data-action="play-file" title="Lire">
                        ▶
                    </button>
                    <button class="btn-icon" data-action="delete-file" title="Supprimer">
                        🗑️
                    </button>
                </div>
            </div>
        `;
    }
    
    renderFileDetails(file) {
        const size = file.size ? this.formatFileSize(file.size) : '—';
        const date = file.modified || file.created;
        const dateStr = date ? this.formatDate(date) : '—';
        
        return `
            <div class="details-header">
                <h3>Détails du fichier</h3>
                <button class="btn-close" data-action="close-details">✕</button>
            </div>
            <div class="details-content">
                <div class="detail-icon">🎵</div>
                <div class="detail-name">${file.name}</div>
                
                <div class="details-section">
                    <h4>Informations</h4>
                    <div class="detail-row">
                        <span class="detail-label">Chemin:</span>
                        <span class="detail-value">${file.path || file.name}</span>
                    </div>
                    <div class="detail-row">
                        <span class="detail-label">Taille:</span>
                        <span class="detail-value">${size}</span>
                    </div>
                    <div class="detail-row">
                        <span class="detail-label">Date:</span>
                        <span class="detail-value">${dateStr}</span>
                    </div>
                </div>
                
                ${file.midi_info ? this.renderMidiInfo(file.midi_info) : ''}
                
                <div class="details-actions">
                    <button class="btn-primary" data-action="load-file">
                        📂 Charger dans l'éditeur
                    </button>
                    <button class="btn-secondary" data-action="play-file">
                        ▶ Lire
                    </button>
                    <button class="btn-danger" data-action="delete-file">
                        🗑️ Supprimer
                    </button>
                </div>
            </div>
        `;
    }
    
    renderMidiInfo(midiInfo) {
        return `
            <div class="details-section">
                <h4>Informations MIDI</h4>
                <div class="detail-row">
                    <span class="detail-label">Format:</span>
                    <span class="detail-value">${midiInfo.format || '—'}</span>
                </div>
                <div class="detail-row">
                    <span class="detail-label">Pistes:</span>
                    <span class="detail-value">${midiInfo.tracks || '—'}</span>
                </div>
                <div class="detail-row">
                    <span class="detail-label">Tempo:</span>
                    <span class="detail-value">${midiInfo.tempo || '—'} BPM</span>
                </div>
                <div class="detail-row">
                    <span class="detail-label">Durée:</span>
                    <span class="detail-value">${midiInfo.duration ? this.formatDuration(midiInfo.duration) : '—'}</span>
                </div>
            </div>
        `;
    }
    
    renderNoSelection() {
        return `
            <div class="details-empty">
                <div class="empty-icon">📄</div>
                <p>Sélectionnez un fichier</p>
                <p class="text-muted">Les détails s'afficheront ici</p>
            </div>
        `;
    }
    
    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================
    
    attachEvents() {
        super.attachEvents();
        
        if (!this.container) return;
        
        // Délégation d'événements
        this.container.addEventListener('click', (e) => {
            const action = e.target.closest('[data-action]')?.dataset.action;
            if (!action) return;
            
            const fileCard = e.target.closest('.file-card');
            const filePath = fileCard?.dataset.filePath;
            
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
                case 'play-file':
                    if (filePath) this.handlePlayFile(filePath);
                    break;
                case 'load-file':
                    this.handleLoadFile();
                    break;
                case 'delete-file':
                    if (filePath) this.handleDeleteFile(filePath);
                    else if (this.viewState.selectedFile) {
                        this.handleDeleteFile(this.viewState.selectedFile.path || this.viewState.selectedFile.name);
                    }
                    break;
                case 'close-details':
                    this.handleCloseDetails();
                    break;
                case 'toggle-sort-order':
                    this.toggleSortOrder();
                    break;
            }
        });
        
        // Change events
        this.container.addEventListener('change', (e) => {
            const action = e.target.dataset.action;
            
            if (action === 'change-sort') {
                this.viewState.sortBy = e.target.value;
                this.render();
            }
        });
        
        // Input events
        this.container.addEventListener('input', (e) => {
            const action = e.target.dataset.action;
            
            if (action === 'filter-files') {
                this.viewState.filter = e.target.value;
                this.render();
            } else if (action === 'change-path') {
                this.viewState.currentPath = e.target.value;
            }
        });
        
        // EventBus listeners
        this.setupEventBusListeners();
    }
    
    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        // files:listed - réponse de files.list
        this.eventBus.on('files:listed', (data) => {
            this.viewState.files = data.files || [];
            this.viewState.isLoading = false;
            this.render();
        });
        
        // files:loaded - réponse de files.read
        this.eventBus.on('file:loaded', (data) => {
            // Fichier chargé avec succès
            this.log('info', 'FileView', `File loaded: ${data.path}`);
        });
        
        // files:deleted - réponse de files.delete
        this.eventBus.on('file:deleted', (data) => {
            this.log('info', 'FileView', `File deleted: ${data.path}`);
            this.handleRefresh();
        });
        
        // files:uploaded - réponse de files.write
        this.eventBus.on('file:uploaded', (data) => {
            this.log('info', 'FileView', `File uploaded: ${data.path}`);
            this.handleRefresh();
        });
        
        // Erreurs
        this.eventBus.on('files:error', (data) => {
            this.log('error', 'FileView', `Error: ${data.error}`);
            this.viewState.isLoading = false;
            this.render();
        });
    }
    
    // ========================================================================
    // ACTIONS
    // ========================================================================
    
    handleUpload() {
        // Créer un input file
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.mid,.midi';
        input.multiple = true;
        
        input.onchange = async (e) => {
            const files = Array.from(e.target.files);
            
            for (const file of files) {
                try {
                    const arrayBuffer = await file.arrayBuffer();
                    const base64 = this.arrayBufferToBase64(arrayBuffer);
                    
                    // Émettre event pour upload via files.write API
                    this.eventBus.emit('file:upload_requested', {
                        file_path: `${this.viewState.currentPath}/${file.name}`,
                        content: base64,
                        encoding: 'base64'
                    });
                } catch (error) {
                    this.log('error', 'FileView', `Upload error: ${error.message}`);
                }
            }
        };
        
        input.click();
    }
    
    handleRefresh() {
        this.viewState.isLoading = true;
        this.render();
        
        // Demander la liste via files.list API
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
            
            // Émettre événement
            if (this.eventBus) {
                this.eventBus.emit('file:selected', { file });
            }
        }
    }
    
    handlePlayFile(filePath) {
        // Demander lecture via playback.load + playback.play
        if (this.eventBus) {
            this.eventBus.emit('file:play_requested', {
                file_path: filePath
            });
        }
    }
    
    handleLoadFile() {
        if (!this.viewState.selectedFile) return;
        
        const filePath = this.viewState.selectedFile.path || this.viewState.selectedFile.name;
        
        // Demander chargement dans l'éditeur via files.read puis conversion
        if (this.eventBus) {
            this.eventBus.emit('file:load_editor_requested', {
                file_path: filePath
            });
        }
    }
    
    async handleDeleteFile(filePath) {
        const confirm = window.confirm(`Supprimer le fichier ${filePath} ?`);
        if (!confirm) return;
        
        // Demander suppression via files.delete API
        if (this.eventBus) {
            this.eventBus.emit('file:delete_requested', {
                file_path: filePath
            });
        }
        
        // Clear selection si c'est le fichier sélectionné
        if (this.viewState.selectedFile && 
            (this.viewState.selectedFile.path === filePath || 
             this.viewState.selectedFile.name === filePath)) {
            this.viewState.selectedFile = null;
        }
    }
    
    handleCloseDetails() {
        this.viewState.selectedFile = null;
        this.render();
    }
    
    toggleSortOrder() {
        this.viewState.sortOrder = this.viewState.sortOrder === 'asc' ? 'desc' : 'asc';
        this.render();
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    filterFiles(files, filter) {
        if (!filter) return files;
        
        const lowerFilter = filter.toLowerCase();
        return files.filter(file => 
            file.name.toLowerCase().includes(lowerFilter)
        );
    }
    
    sortFiles(files, sortBy, sortOrder) {
        const sorted = [...files].sort((a, b) => {
            let aVal, bVal;
            
            switch (sortBy) {
                case 'name':
                    aVal = a.name.toLowerCase();
                    bVal = b.name.toLowerCase();
                    break;
                case 'date':
                    aVal = a.modified || a.created || 0;
                    bVal = b.modified || b.created || 0;
                    break;
                case 'size':
                    aVal = a.size || 0;
                    bVal = b.size || 0;
                    break;
                default:
                    return 0;
            }
            
            if (aVal < bVal) return sortOrder === 'asc' ? -1 : 1;
            if (aVal > bVal) return sortOrder === 'asc' ? 1 : -1;
            return 0;
        });
        
        return sorted;
    }
    
    formatFileSize(bytes) {
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
        return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    }
    
    formatDate(timestamp) {
        const date = new Date(timestamp * 1000);
        return date.toLocaleDateString() + ' ' + date.toLocaleTimeString();
    }
    
    formatDuration(seconds) {
        const mins = Math.floor(seconds / 60);
        const secs = Math.floor(seconds % 60);
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    }
    
    arrayBufferToBase64(buffer) {
        const bytes = new Uint8Array(buffer);
        let binary = '';
        for (let i = 0; i < bytes.byteLength; i++) {
            binary += String.fromCharCode(bytes[i]);
        }
        return window.btoa(binary);
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        super.init();
        
        // Charger la liste des fichiers
        this.handleRefresh();
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
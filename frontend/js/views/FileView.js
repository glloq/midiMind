// ============================================================================
// Fichier: frontend/js/views/FileView.js
// Version: v4.0.2 - FIX ERREURS DE RENDU
// Date: 2025-11-11
// ============================================================================
// CORRECTIONS v4.0.2:
// ✅ Fix: Meilleure gestion d'erreurs dans render()
// ✅ Fix: Vérification container avant buildTemplate()
// ✅ Fix: Protection contre erreurs dans renderFilesList()
// ✅ Fix: Utilisation correcte de this.log() au lieu de logger direct
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
        
        this.log('info', '[FileView] ✅ FileView v4.0.2 initialized (Fix Render Errors)');
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
    buildTemplate(data = {}) {
        try {
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
                            <input type="text" class="path-input" value="${this.escapeHtml(state.currentPath)}" 
                                   data-action="change-path" />
                        </div>
                        
                        <div class="files-search">
                            <input type="text" class="search-input" placeholder="Rechercher..." 
                                   value="${this.escapeHtml(state.filter)}" data-action="filter-files" />
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
        } catch (error) {
            this.log('error', '[FileView] buildTemplate failed:', error);
            return '<div class="error-message">Erreur lors de la construction du template</div>';
        }
    }
    
    // ========================================================================
    // RENDERING - MÉTHODES PRINCIPALES
    // ========================================================================
    
    /**
     * Rendre la vue
     * @param {Object} data - Données optionnelles pour le rendu
     */
    render(data = null) {
        if (!this.container) {
            this.log('error', '[FileView] Cannot render: container not found');
            return;
        }
        
        const startTime = performance.now();
        
        try {
            // Générer et insérer le HTML
            const html = this.buildTemplate(data || this.viewState);
            this.container.innerHTML = html;
            
            // Attacher les événements
            this.attachEvents();
            
            // Mettre à jour l'état
            this.state.rendered = true;
            this.state.lastUpdate = Date.now();
            
            // Émettre événement
            if (this.eventBus) {
                this.eventBus.emit('file-view:rendered', {
                    filesCount: this.viewState.files.length
                });
            }
            
            const renderTime = performance.now() - startTime;
            this.log('debug', '[FileView]', `✓ Rendered in ${renderTime.toFixed(2)}ms`);
            
        } catch (error) {
            this.log('error', '[FileView] Render failed:', error);
            // Ne pas appeler handleError qui pourrait créer une boucle infinie
            if (this.container) {
                this.container.innerHTML = '<div class="error-message">Erreur lors du rendu de la vue</div>';
            }
        }
    }

    /**
     * Afficher la vue
     */
    show() {
        if (this.container) {
            this.container.style.display = 'block';
            this.state.visible = true;
            
            // Recharger les données si nécessaire
            if (this.viewState.files.length === 0) {
                this.refreshFiles();
            }
        }
    }

    /**
     * Masquer la vue
     */
    hide() {
        if (this.container) {
            this.container.style.display = 'none';
            this.state.visible = false;
        }
    }

    /**
     * Recharger les fichiers
     */
    refreshFiles() {
        this.handleRefresh();
    }
    
    // ========================================================================
    // RENDERING - SOUS-COMPOSANTS
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
        try {
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
        } catch (error) {
            this.log('error', '[FileView] renderFilesList failed:', error);
            return '<div class="error-message">Erreur lors de l\'affichage des fichiers</div>';
        }
    }
    
    renderFileCard(file, selectedFile) {
        try {
            const isSelected = selectedFile && 
                              (selectedFile.path === file.path || selectedFile.name === file.name);
            
            const size = file.size ? this.formatFileSize(file.size) : '—';
            const date = file.modified || file.created;
            const dateStr = date ? this.formatDate(date) : '—';
            
            return `
                <div class="file-card ${isSelected ? 'selected' : ''}" 
                     data-file-path="${this.escapeHtml(file.path || file.name)}">
                    <div class="file-icon">🎵</div>
                    <div class="file-info">
                        <div class="file-name" title="${this.escapeHtml(file.name)}">${this.escapeHtml(file.name)}</div>
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
        } catch (error) {
            this.log('error', '[FileView] renderFileCard failed:', error);
            return '';
        }
    }
    
    renderFileDetails(file) {
        try {
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
                    <div class="detail-name">${this.escapeHtml(file.name)}</div>
                    
                    <div class="details-section">
                        <h4>Informations</h4>
                        <div class="detail-row">
                            <span class="detail-label">Chemin:</span>
                            <span class="detail-value">${this.escapeHtml(file.path || file.name)}</span>
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
        } catch (error) {
            this.log('error', '[FileView] renderFileDetails failed:', error);
            return '<div class="error-message">Erreur lors de l\'affichage des détails</div>';
        }
    }
    
    renderMidiInfo(midiInfo) {
        try {
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
                        <span class="detail-label">PPQ:</span>
                        <span class="detail-value">${midiInfo.ppq || '—'}</span>
                    </div>
                    <div class="detail-row">
                        <span class="detail-label">Durée:</span>
                        <span class="detail-value">${midiInfo.duration ? this.formatDuration(midiInfo.duration) : '—'}</span>
                    </div>
                </div>
            `;
        } catch (error) {
            this.log('error', '[FileView] renderMidiInfo failed:', error);
            return '';
        }
    }
    
    renderNoSelection() {
        return `
            <div class="details-placeholder">
                <div class="placeholder-icon">📋</div>
                <p>Sélectionnez un fichier pour voir ses détails</p>
            </div>
        `;
    }
    
    // ========================================================================
    // EVENTS
    // ========================================================================
    
    attachEvents() {
        super.attachEvents();
        
        if (!this.container) return;
        
        // Actions des boutons
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
                    this.toggleSortOrder();
                    break;
            }
        });
        
        // Changement de filtre
        this.container.addEventListener('input', (e) => {
            if (e.target.dataset.action === 'filter-files') {
                this.viewState.filter = e.target.value;
                this.render();
            }
        });
        
        // Changement de tri
        this.container.addEventListener('change', (e) => {
            if (e.target.dataset.action === 'change-sort') {
                this.viewState.sortBy = e.target.value;
                this.render();
            }
        });
        
        this.setupEventBusListeners();
    }
    
    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        // files.list response
        this.eventBus.on('files:listed', (data) => {
            this.log('debug', '[FileView]', `Received ${data.files?.length || 0} files`);
            this.viewState.files = data.files || [];
            this.viewState.isLoading = false;
            this.render();
        });
        
        // files.write response (upload)
        this.eventBus.on('file:uploaded', (data) => {
            this.log('info', '[FileView]', `File uploaded: ${data.file_path}`);
            this.handleRefresh(); // Recharger la liste
        });
        
        // files.delete response
        this.eventBus.on('file:deleted', (data) => {
            this.log('info', '[FileView]', `File deleted: ${data.file_path}`);
            this.handleRefresh(); // Recharger la liste
        });
        
        // Erreurs
        this.eventBus.on('files:error', (data) => {
            this.log('error', '[FileView]', `Error: ${data.error}`);
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
        
        input.addEventListener('change', async (e) => {
            const files = Array.from(e.target.files);
            
            for (const file of files) {
                try {
                    // Lire le fichier
                    const arrayBuffer = await file.arrayBuffer();
                    const base64Data = this.arrayBufferToBase64(arrayBuffer);
                    
                    // Émettre événement d'upload
                    if (this.eventBus) {
                        this.eventBus.emit('file:upload_requested', {
                            file_name: file.name,
                            file_data: base64Data,
                            file_path: `${this.viewState.currentPath}/${file.name}`
                        });
                    }
                    
                    this.log('info', '[FileView]', `Uploading file: ${file.name}`);
                } catch (error) {
                    this.log('error', '[FileView]', `Failed to upload ${file.name}:`, error);
                }
            }
        });
        
        input.click();
    }
    
    handleRefresh() {
        this.viewState.isLoading = true;
        this.render();
        
        // Demander liste des fichiers via files.list API
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
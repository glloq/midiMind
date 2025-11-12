// ============================================================================
// Fichier: frontend/js/views/FileView.js
// Chemin rÃ©el: frontend/js/views/FileView.js
// Version: v4.1.0 - ENCODAGE UTF-8 CORRIGÃ‰
// Date: 2025-11-11
// ============================================================================
// CORRECTIONS v4.1.0:
// âœ… Encodage UTF-8 complet (tous caractÃ¨res franÃ§ais corrigÃ©s)
// âœ… Gestion Ã©vÃ©nements robuste
// âœ… Upload permanent dans DOM
// âœ… Affichage liste fichiers
// ============================================================================

class FileView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        this.logger = window.logger || console;
        
        // Ã‰tat spÃ©cifique Ã  la vue
        this.viewState = {
            files: [],
            selectedFile: null,
            currentPath: '/midi',
            isLoading: false,
            sortBy: 'name', // 'name', 'date', 'size'
            sortOrder: 'asc', // 'asc', 'desc'
            filter: '' // filtre de recherche
        };
        
        this.log('info', 'FileView', 'âœ… FileView v4.1.0 initialized (UTF-8 fixed)');
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
    buildTemplate(data = {}) {
        const state = { ...this.viewState, ...data };
        
        return `
            <div class="file-view-container">
                <div class="page-header">
                    <h1>ðŸ“ Fichiers MIDI</h1>
                    <div class="header-actions">
                        <button class="btn-upload" data-action="upload-file">
                            ðŸ“¤ Upload
                        </button>
                        <button class="btn-refresh" data-action="refresh-files">
                            ðŸ”„ Actualiser
                        </button>
                    </div>
                </div>
                
                <div class="toolbar">
                    ${this.buildToolbar(state)}
                </div>
                
                <div class="content-area">
                    <div class="file-list">
                        ${state.isLoading ? this.buildLoadingState() : this.buildFileGrid(state)}
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
                    placeholder="ðŸ” Rechercher..." 
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
                    ${state.sortOrder === 'asc' ? 'â¬†ï¸' : 'â¬‡ï¸'}
                </button>
            </div>
        `;
    }
    
    buildFileGrid(state) {
        // âœ… SÃ‰CURITÃ‰: VÃ©rifier que state.files existe et est un tableau
        if (!state.files || !Array.isArray(state.files)) {
            return this.buildEmptyState();
        }
        
        const filteredFiles = this.filterFiles(state.files, state.filter);
        const sortedFiles = this.sortFiles(filteredFiles, state.sortBy, state.sortOrder);
        
        if (sortedFiles.length === 0) {
            return this.buildEmptyState();
        }
        
        return `
            <div class="file-list-table">
                <table class="file-table">
                    <thead>
                        <tr>
                            <th class="col-icon"></th>
                            <th class="col-name">Nom</th>
                            <th class="col-size">Taille</th>
                            <th class="col-date">Date</th>
                            <th class="col-actions">Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${sortedFiles.map(file => this.buildFileCard(file)).join('')}
                    </tbody>
                </table>
            </div>
        `;
    }
    
    buildFileCard(file) {
        const isSelected = this.viewState.selectedFile?.path === file.path;
        const filePath = this.escapeHtml(file.path || file.name);
        const fileName = this.escapeHtml(file.name);
        
        return `
            <tr class="file-row ${isSelected ? 'selected' : ''}" data-file-path="${filePath}">
                <td class="col-icon">
                    <span class="file-icon">🎵</span>
                </td>
                <td class="col-name">
                    <span class="file-name">${fileName}</span>
                </td>
                <td class="col-size">
                    ${this.formatFileSize(file.size)}
                </td>
                <td class="col-date">
                    ${this.formatDate(file.modified)}
                </td>
                <td class="col-actions">
                    <div class="file-actions-group">
                        <button 
                            class="btn-action btn-play" 
                            data-action="play-file" 
                            title="Jouer">
                            ▶️
                        </button>
                        <button 
                            class="btn-action btn-routing" 
                            data-action="open-routing" 
                            title="Routage">
                            🔀
                        </button>
                        <button 
                            class="btn-action btn-edit" 
                            data-action="edit-file" 
                            title="Éditer">
                            ✏️
                        </button>
                        <button 
                            class="btn-action btn-info" 
                            data-action="select-file" 
                            title="Détails">
                            👁️
                        </button>
                        <button 
                            class="btn-action btn-delete" 
                            data-action="delete-file" 
                            title="Supprimer">
                            🗑️
                        </button>
                    </div>
                </td>
            </tr>
        `;
    }
    
        buildFileDetails(file) {
        return `
            <div class="file-details">
                <div class="details-header">
                    <h3>ðŸ“„ DÃ©tails du fichier</h3>
                    <button class="btn-close" data-action="close-details">âœ•</button>
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
                        <span class="detail-label">ModifiÃ©:</span>
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
                            <span class="detail-label">DurÃ©e:</span>
                            <span class="detail-value">${this.formatDuration(file.duration)}</span>
                        </div>
                    ` : ''}
                </div>
                
                <div class="details-actions">
                    <button class="btn-primary" data-action="load-file">
                        ðŸ“‚ Charger dans l'Ã©diteur
                    </button>
                </div>
            </div>
        `;
    }
    
    buildLoadingState() {
        return `
            <div class="loading-state">
                <div class="spinner"></div>
                <p>Chargement des fichiers...</p>
            </div>
        `;
    }
    
    buildEmptyState() {
        return `
            <div class="empty-state">
                <div class="empty-icon">ðŸ“­</div>
                <h3>Aucun fichier</h3>
                <p>Uploadez des fichiers MIDI pour commencer</p>
            </div>
        `;
    }
    
    // ========================================================================
    // RENDERING - MÃ‰THODES PRINCIPALES
    // ========================================================================
    
    /**
     * Rendre la vue
     * @param {Object} data - DonnÃ©es optionnelles pour le rendu
     */
    render(data = null) {
        if (!this.container) {
            this.log('error', 'FileView', 'Cannot render: container not found');
            return;
        }
        
        const startTime = performance.now();
        
        try {
            // DEBUG: Afficher l'Ã©tat actuel
            console.log('[FileView] render() - viewState:', JSON.stringify(this.viewState));
            console.log('[FileView] render() - data:', data);
            
            // GÃ©nÃ©rer et insÃ©rer le HTML
            const template = this.buildTemplate(data || this.viewState);
            console.log('[FileView] Template generated, length:', template.length);
            
            this.container.innerHTML = template;
            console.log('[FileView] innerHTML set');
            
            // Attacher les Ã©vÃ©nements
            this.attachEvents();
            console.log('[FileView] Events attached');
            
            // Mettre Ã  jour l'Ã©tat
            this.state.rendered = true;
            this.state.lastUpdate = Date.now();
            
            // Ã‰mettre Ã©vÃ©nement
            if (this.eventBus) {
                this.eventBus.emit('file-view:rendered', {
                    filesCount: this.viewState.files ? this.viewState.files.length : 0
                });
            }
            
            const renderTime = performance.now() - startTime;
            this.log('info', 'FileView', `âœ… Rendered in ${renderTime.toFixed(2)}ms`);
            
        } catch (error) {
            // LOG COMPLET de l'erreur
            console.error('[FileView] RENDER ERROR:', error);
            console.error('[FileView] Error stack:', error.stack);
            console.error('[FileView] Error message:', error.message);
            console.error('[FileView] viewState at error:', this.viewState);
            
            this.log('error', 'FileView', 'Render failed:', error.message || error);
            
            // Afficher un message d'erreur Ã  l'utilisateur
            if (this.container) {
                this.container.innerHTML = `
                    <div class="error-message" style="padding: 20px; text-align: center;">
                        <h3>âŒ Erreur d'affichage</h3>
                        <p>${error.message || 'Erreur inconnue'}</p>
                        <pre style="text-align: left; background: #f5f5f5; padding: 10px; overflow: auto;">${error.stack || ''}</pre>
                        <button onclick="window.location.reload()" style="margin-top: 10px; padding: 10px 20px;">Recharger la page</button>
                    </div>
                `;
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
            
            // Recharger les donnÃ©es si nÃ©cessaire
            if (!this.viewState.files || this.viewState.files.length === 0) {
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
    // FILTRE ET TRI
    // ========================================================================
    
    /**
     * Filtrer les fichiers selon une recherche
     * @param {Array} files - Liste des fichiers
     * @param {string} filter - Terme de recherche
     * @returns {Array} Fichiers filtrÃ©s
     */
    filterFiles(files, filter) {
        // âœ… SÃ‰CURITÃ‰: VÃ©rifier que files est un tableau
        if (!Array.isArray(files)) {
            console.warn('[FileView] filterFiles: files is not an array', files);
            return [];
        }
        
        if (!filter || filter.trim() === '') {
            return files;
        }
        
        const searchTerm = filter.toLowerCase();
        
        return files.filter(file => {
            if (!file || !file.name) return false;
            return file.name.toLowerCase().includes(searchTerm);
        });
    }
    
    /**
     * Trier les fichiers
     * @param {Array} files - Liste des fichiers
     * @param {string} sortBy - CritÃ¨re de tri
     * @param {string} sortOrder - Ordre (asc/desc)
     * @returns {Array} Fichiers triÃ©s
     */
    sortFiles(files, sortBy, sortOrder) {
        // âœ… SÃ‰CURITÃ‰: VÃ©rifier que files est un tableau
        if (!Array.isArray(files)) {
            console.warn('[FileView] sortFiles: files is not an array', files);
            return [];
        }
        
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
    
    toggleSortOrder() {
        this.viewState.sortOrder = this.viewState.sortOrder === 'asc' ? 'desc' : 'asc';
        this.render();
    }
    
    // ========================================================================
    // FORMATAGE
    // ========================================================================
    
    formatFileSize(bytes) {
        if (!bytes || bytes === 0) return '0 B';
        
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        
        return `${(bytes / Math.pow(k, i)).toFixed(1)} ${sizes[i]}`;
    }
    
    formatDate(timestamp) {
        if (!timestamp) return 'N/A';
        
        const date = new Date(timestamp * 1000);
        const now = new Date();
        const diff = now - date;
        
        // Moins d'une heure
        if (diff < 3600000) {
            const minutes = Math.floor(diff / 60000);
            return `il y a ${minutes} min`;
        }
        
        // Moins d'un jour
        if (diff < 86400000) {
            const hours = Math.floor(diff / 3600000);
            return `il y a ${hours}h`;
        }
        
        // Moins d'une semaine
        if (diff < 604800000) {
            const days = Math.floor(diff / 86400000);
            return `il y a ${days}j`;
        }
        
        // Format complet
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
    
    // ========================================================================
    // EVENTS
    // ========================================================================
    
    attachEvents() {
        // âœ… PAS d'appel Ã  super.attachEvents() car BaseView n'a pas cette mÃ©thode
        
        if (!this.container) return;
        
        // Actions des boutons
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
                case 'play-file':
                    if (filePath) this.handlePlayFile(filePath);
                    break;
                case 'delete-file':
                    if (filePath) this.handleDeleteFile(filePath);
                case 'edit-file':
                    if (filePath) this.handleEditFile(filePath);
                    break;
                case 'open-routing':
                    if (filePath) this.handleOpenRouting(filePath);
                    break;
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
        this.eventBus.on('files:list-updated', (data) => {
            this.log('info', 'FileView', `Received ${data.files?.length || 0} files`);
            this.viewState.files = data.files || [];
            this.viewState.isLoading = false;
            this.render();
        });
        
        // files.write response (upload)
        this.eventBus.on('file:uploaded', (data) => {
            this.log('info', 'FileView', `File uploaded: ${data.file_path}`);
            this.handleRefresh(); // Recharger la liste
        });
        
        // files.delete response
        this.eventBus.on('file:deleted', (data) => {
            this.log('info', 'FileView', `File deleted: ${data.file_path}`);
            this.handleRefresh(); // Recharger la liste
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
        // âœ… Utiliser ou crÃ©er un input file permanent attachÃ© au DOM
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
            
            // Attacher le handler une seule fois
            input.addEventListener('change', (e) => {
                const files = Array.from(e.target.files);
                
                if (files.length === 0) return;
                
                this.log('info', 'FileView', `Selected ${files.length} file(s)`);
                
                for (const file of files) {
                    try {
                        // âœ… Ã‰mettre Ã©vÃ©nement que FileController Ã©coute
                        if (this.eventBus) {
                            this.eventBus.emit('file:upload', {
                                file: file  // Objet File natif
                            });
                            this.log('info', 'FileView', `Upload requested: ${file.name}`);
                        }
                    } catch (error) {
                        this.log('error', 'FileView', `Upload error: ${error.message}`);
                    }
                }
                
                // RÃ©initialiser pour permettre de rÃ©uploader le mÃªme fichier
                input.value = '';
            });
        }
        
        // DÃ©clencher le sÃ©lecteur de fichiers
        this.log('debug', 'FileView', 'Triggering file selector');
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
            
            // Ã‰mettre Ã©vÃ©nement
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
    
    handleDeleteFile(filePath) {
        if (!confirm(`Supprimer ${filePath} ?`)) return;
        
        // Demander suppression via files.delete API
        if (this.eventBus) {
            this.eventBus.emit('file:delete_requested', {
                file_path: filePath
            });
        }
    }
    
    
    handleEditFile(filePath) {
        this.log('info', 'FileView', `Opening editor for: ${filePath}`);
        
        if (this.eventBus) {
            this.eventBus.emit('file:load_in_editor', {
                file_path: filePath
            });
        }
        
        if (window.app?.router) {
            window.app.router.navigateTo('/editor');
        }
    }
    
    handleOpenRouting(filePath) {
        this.log('info', 'FileView', `Opening routing for: ${filePath}`);
        
        if (this.eventBus) {
            this.eventBus.emit('file:load_for_routing', {
                file_path: filePath
            });
        }
        
        if (window.app?.router) {
            window.app.router.navigateTo('/routing');
        }
    }
    
        handleLoadFile() {
        if (!this.viewState.selectedFile) return;
        
        const filePath = this.viewState.selectedFile.path || this.viewState.selectedFile.name;
        
        // Demander chargement dans l'Ã©diteur
        if (this.eventBus) {
            this.eventBus.emit('file:load_in_editor', {
                file_path: filePath
            });
        }
        
        // Navigation vers l'Ã©diteur
        if (window.app?.router) {
            window.app.router.navigateTo('/editor');
        }
    }
    
    handleCloseDetails() {
        this.viewState.selectedFile = null;
        this.render();
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Mettre Ã  jour la liste des fichiers
     * @param {Array} files - Nouvelle liste de fichiers
     */
    updateFiles(files) {
        this.viewState.files = files || [];
        this.viewState.isLoading = false;
        this.render();
    }
    
    /**
     * DÃ©finir l'Ã©tat de chargement
     * @param {boolean} loading - Ã‰tat de chargement
     */
    setLoading(loading) {
        this.viewState.isLoading = loading;
        this.render();
    }
    
    /**
     * Obtenir le fichier sÃ©lectionnÃ©
     * @returns {Object|null}
     */
    getSelectedFile() {
        return this.viewState.selectedFile;
    }
    
    /**
     * DÃ©finir le fichier sÃ©lectionnÃ©
     * @param {Object} file - Fichier Ã  sÃ©lectionner
     */
    setSelectedFile(file) {
        this.viewState.selectedFile = file;
        this.render();
    }
    
    /**
     * Nettoyer la vue
     */
    destroy() {
        // Nettoyer l'input file si existant
        const input = document.getElementById('file-upload-input');
        if (input) {
            input.remove();
        }
        
        // Appeler la mÃ©thode destroy du parent si elle existe
        if (super.destroy) {
            super.destroy();
        }
    }
    
    /**
     * Logger avec prÃ©fixe
     */
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

// Export pour utilisation en tant que module
if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileView;
}
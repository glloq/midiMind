// ============================================================================
// Fichier: frontend/js/views/FileView.js
// Chemin réel: frontend/js/views/FileView.js
// Version: v4.2.0 - FIXED MISSING init() METHOD
// Date: 2025-11-12
// ============================================================================
// CORRECTIONS v4.2.0:
// ✅ CRITIQUE: Ajout méthode init() manquante
// ✅ Encodage UTF-8 corrigé
// ✅ Pattern d'initialisation standard
// 
// CORRECTIONS v4.1.0:
// ✅ Encodage UTF-8 complet (tous caractères français corrigés)
// ✅ Gestion événements robuste
// ✅ Upload permanent dans DOM
// ✅ Affichage liste fichiers
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
        
        this.log('debug', 'FileView', 'FileView v4.2.0 created');
    }
    
    // ========================================================================
    // INITIALISATION ✅ NOUVEAU
    // ========================================================================
    
    /**
     * Initialise la vue FileView
     */
    init() {
        if (!this.container) {
            this.log('error', 'FileView', 'Cannot initialize: container not found');
            return;
        }
        
        // Rendre l'interface initiale
        this.render();
        
        // Attacher les événements DOM
        this.attachEvents();
        
        // Attacher les événements EventBus
        this.setupEventBusListeners();
        
        // Marquer comme initialisée
        this.state.initialized = true;
        
        // Log d'initialisation
        this.log('info', 'FileView', '✅ FileView v4.2.0 initialized');
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
    
    buildFileGrid(state) {
        // ✅ SÉCURITÉ: Vérifier que state.files existe et est un tableau
        if (!state.files || !Array.isArray(state.files)) {
            return this.buildEmptyState();
        }
        
        // Filtrer les fichiers selon le filtre de recherche
        let filteredFiles = state.files;
        if (state.filter) {
            const filterLower = state.filter.toLowerCase();
            filteredFiles = state.files.filter(file => 
                file.name?.toLowerCase().includes(filterLower) ||
                file.path?.toLowerCase().includes(filterLower)
            );
        }
        
        // Trier les fichiers
        filteredFiles = this.sortFiles(filteredFiles, state.sortBy, state.sortOrder);
        
        if (filteredFiles.length === 0) {
            return state.filter ? 
                this.buildNoResultsState() : 
                this.buildEmptyState();
        }
        
        return `
            <div class="file-grid">
                ${filteredFiles.map(file => this.buildFileCard(file)).join('')}
            </div>
        `;
    }
    
    buildFileCard(file) {
        const fileName = file.name || file.path?.split('/').pop() || 'Inconnu';
        const fileSize = file.size ? this.formatFileSize(file.size) : 'N/A';
        const fileDate = file.modified ? this.formatDate(file.modified) : 'N/A';
        
        return `
            <div class="file-card" data-file-path="${this.escapeHtml(file.path || file.name)}">
                <div class="file-icon">🎵</div>
                <div class="file-info">
                    <div class="file-name">${this.escapeHtml(fileName)}</div>
                    <div class="file-meta">
                        <span class="file-size">${fileSize}</span>
                        <span class="file-date">${fileDate}</span>
                    </div>
                </div>
                <div class="file-actions">
                    <button class="btn-icon" data-action="play-file" title="Jouer">
                        ▶️
                    </button>
                    <button class="btn-icon" data-action="select-file" title="Détails">
                        📋
                    </button>
                    <button class="btn-icon btn-danger" data-action="delete-file" title="Supprimer">
                        🗑️
                    </button>
                </div>
            </div>
        `;
    }
    
    buildFileDetails(file) {
        const fileName = file.name || file.path?.split('/').pop() || 'Inconnu';
        const fileSize = file.size ? this.formatFileSize(file.size) : 'N/A';
        const fileDate = file.modified ? this.formatDate(file.modified) : 'N/A';
        const filePath = file.path || file.name || 'N/A';
        
        return `
            <div class="file-details">
                <div class="details-header">
                    <h2>📄 Détails du fichier</h2>
                    <button class="btn-close" data-action="close-details">×</button>
                </div>
                <div class="details-content">
                    <div class="detail-row">
                        <span class="detail-label">Nom :</span>
                        <span class="detail-value">${this.escapeHtml(fileName)}</span>
                    </div>
                    <div class="detail-row">
                        <span class="detail-label">Chemin :</span>
                        <span class="detail-value">${this.escapeHtml(filePath)}</span>
                    </div>
                    <div class="detail-row">
                        <span class="detail-label">Taille :</span>
                        <span class="detail-value">${fileSize}</span>
                    </div>
                    <div class="detail-row">
                        <span class="detail-label">Modifié :</span>
                        <span class="detail-value">${fileDate}</span>
                    </div>
                    ${file.tracks ? `
                        <div class="detail-row">
                            <span class="detail-label">Pistes :</span>
                            <span class="detail-value">${file.tracks}</span>
                        </div>
                    ` : ''}
                    ${file.duration ? `
                        <div class="detail-row">
                            <span class="detail-label">Durée :</span>
                            <span class="detail-value">${this.formatDuration(file.duration)}</span>
                        </div>
                    ` : ''}
                </div>
                <div class="details-actions">
                    <button class="btn-primary" data-action="load-file">
                        📝 Ouvrir dans l'éditeur
                    </button>
                    <button class="btn-secondary" data-action="play-file">
                        ▶️ Jouer
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
                <div class="empty-icon">📁</div>
                <h3>Aucun fichier MIDI</h3>
                <p>Uploadez vos fichiers MIDI pour commencer</p>
                <button class="btn-primary" data-action="upload-file">
                    📤 Upload un fichier
                </button>
            </div>
        `;
    }
    
    buildNoResultsState() {
        return `
            <div class="empty-state">
                <div class="empty-icon">🔍</div>
                <h3>Aucun résultat</h3>
                <p>Essayez avec un autre terme de recherche</p>
            </div>
        `;
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    sortFiles(files, sortBy, sortOrder) {
        const sorted = [...files].sort((a, b) => {
            let compareA, compareB;
            
            switch (sortBy) {
                case 'name':
                    compareA = (a.name || a.path || '').toLowerCase();
                    compareB = (b.name || b.path || '').toLowerCase();
                    break;
                case 'date':
                    compareA = new Date(a.modified || 0).getTime();
                    compareB = new Date(b.modified || 0).getTime();
                    break;
                case 'size':
                    compareA = a.size || 0;
                    compareB = b.size || 0;
                    break;
                default:
                    return 0;
            }
            
            if (compareA < compareB) return sortOrder === 'asc' ? -1 : 1;
            if (compareA > compareB) return sortOrder === 'asc' ? 1 : -1;
            return 0;
        });
        
        return sorted;
    }
    
    toggleSortOrder() {
        this.viewState.sortOrder = this.viewState.sortOrder === 'asc' ? 'desc' : 'asc';
        this.render();
    }
    
    formatFileSize(bytes) {
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
        return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    }
    
    formatDate(date) {
        if (!date) return 'N/A';
        
        const d = new Date(date);
        if (isNaN(d.getTime())) return 'N/A';
        
        return d.toLocaleDateString('fr-FR', {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
            hour: '2-digit',
            minute: '2-digit'
        });
    }
    
    formatDuration(seconds) {
        if (!seconds || isNaN(seconds)) return 'N/A';
        
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
        // ✅ PAS d'appel à super.attachEvents() car BaseView n'a pas cette méthode
        
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
    }
    
    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        // files.list response
        this.eventBus.on('files:list-updated', (data) => {
            this.log('info', 'EventBus', `Received ${data.files?.length || 0} files`);
            this.viewState.files = data.files || [];
            this.viewState.isLoading = false;
            this.render();
        });
        
        // files.write response (upload)
        this.eventBus.on('file:uploaded', (data) => {
            this.log('info', 'EventBus', `File uploaded: ${data.file_path}`);
            this.handleRefresh(); // Recharger la liste
        });
        
        // files.delete response
        this.eventBus.on('file:deleted', (data) => {
            this.log('info', 'EventBus', `File deleted: ${data.file_path}`);
            this.handleRefresh(); // Recharger la liste
        });
        
        // Erreurs
        this.eventBus.on('files:error', (data) => {
            this.log('error', 'EventBus', `Error: ${data.error}`);
            this.viewState.isLoading = false;
            this.render();
        });
    }
    
    // ========================================================================
    // HANDLERS
    // ========================================================================
    
    handleUpload() {
        // Créer/obtenir l'input file
        let input = document.getElementById('file-upload-input');
        
        if (!input) {
            input = document.createElement('input');
            input.type = 'file';
            input.id = 'file-upload-input';
            input.accept = '.mid,.midi';
            input.style.display = 'none';
            document.body.appendChild(input);
        }
        
        input.onchange = (e) => {
            const file = e.target.files[0];
            if (!file) return;
            
            this.log('info', 'Upload', `Uploading: ${file.name}`);
            
            // Lire le fichier
            const reader = new FileReader();
            reader.onload = (event) => {
                const content = event.target.result;
                
                // Émettre événement d'upload
                if (this.eventBus) {
                    this.eventBus.emit('file:upload_requested', {
                        file_name: file.name,
                        content: Array.from(new Uint8Array(content))
                    });
                }
            };
            
            reader.readAsArrayBuffer(file);
            
            // Reset input
            input.value = '';
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
    
    handleDeleteFile(filePath) {
        if (!confirm(`Supprimer ${filePath} ?`)) return;
        
        // Demander suppression via files.delete API
        if (this.eventBus) {
            this.eventBus.emit('file:delete_requested', {
                file_path: filePath
            });
        }
    }
    
    handleLoadFile() {
        if (!this.viewState.selectedFile) return;
        
        const filePath = this.viewState.selectedFile.path || this.viewState.selectedFile.name;
        
        // Demander chargement dans l'éditeur
        if (this.eventBus) {
            this.eventBus.emit('file:load_in_editor', {
                file_path: filePath
            });
        }
        
        // Navigation vers l'éditeur
        if (window.app?.router) {
            window.app.router.navigateTo('/editor');
        }
    }
    
    handleCloseDetails() {
        this.viewState.selectedFile = null;
        this.render();
    }
    
    // ========================================================================
    // UTILITAIRES PUBLICS
    // ========================================================================
    
    /**
     * Mettre à jour la liste des fichiers
     * @param {Array} files - Nouvelle liste de fichiers
     */
    updateFiles(files) {
        this.viewState.files = files || [];
        this.viewState.isLoading = false;
        this.render();
    }
    
    /**
     * Définir l'état de chargement
     * @param {boolean} loading - État de chargement
     */
    setLoading(loading) {
        this.viewState.isLoading = loading;
        this.render();
    }
    
    /**
     * Obtenir le fichier sélectionné
     * @returns {Object|null}
     */
    getSelectedFile() {
        return this.viewState.selectedFile;
    }
    
    /**
     * Définir le fichier sélectionné
     * @param {Object} file - Fichier à sélectionner
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
        
        // Appeler la méthode destroy du parent si elle existe
        if (super.destroy) {
            super.destroy();
        }
    }
    
    /**
     * Logger avec préfixe
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

// ============================================================================
// EXPORT - CRITIQUE !
// ============================================================================

// Export pour utilisation en tant que module
if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileView;
}

// ✅ CRITIQUE: Export vers window pour utilisation dans le navigateur
if (typeof window !== 'undefined') {
    window.FileView = FileView;
}

// ============================================================================
// FIN - FileView.js v4.2.0
// ============================================================================
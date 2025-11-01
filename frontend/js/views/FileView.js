// ============================================================================
// Fichier: frontend/js/views/FileView.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE + API COMPLÈTE
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.2.0:
// ✅ Signature cohérente : constructor(containerId, eventBus, logger = null)
// ✅ Héritage de BaseView
// ✅ Affichage liste fichiers MIDI
// ✅ Boutons load/delete/refresh
// ✅ Sélection fichier
// ============================================================================

class FileView extends BaseView {
    constructor(containerId, eventBus, logger = null) {
        super(containerId, eventBus);
        
        this.logger = logger || window.logger || console;
        
        // État spécifique à la vue
        this.viewState = {
            files: [],
            selectedFile: null,
            currentPath: '/midi',
            isLoading: false
        };
        
        this.log('info', 'FileView', '✅ FileView v3.2.0 initialized');
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
    buildTemplate(data = {}) {
        const state = { ...this.viewState, ...data };
        
        return \`
            <div class="file-view-container">
                <div class="page-header">
                    <h1>📁 Fichiers MIDI</h1>
                    <div class="header-actions">
                        <button class="btn-refresh" data-action="refresh-files">
                            🔄 Actualiser
                        </button>
                    </div>
                </div>
                
                <div class="files-section">
                    <div class="files-header">
                        <div class="files-path">
                            <span class="path-label">Dossier:</span>
                            <span class="path-value">\${state.currentPath}</span>
                        </div>
                        <div class="files-count">
                            <span>\${state.files.length} fichier(s)</span>
                        </div>
                    </div>
                    
                    <div class="files-content">
                        \${state.isLoading ? this.renderLoading() : this.renderFilesList(state)}
                    </div>
                </div>
            </div>
        \`;
    }
    
    // ========================================================================
    // RENDERING
    // ========================================================================
    
    renderLoading() {
        return \`
            <div class="files-loading">
                <div class="spinner"></div>
                <p>Chargement des fichiers...</p>
            </div>
        \`;
    }
    
    renderFilesList(state) {
        const files = state.files || [];
        
        if (files.length === 0) {
            return \`
                <div class="files-empty">
                    <div class="empty-icon">📂</div>
                    <p>Aucun fichier MIDI trouvé</p>
                    <p class="text-muted">Les fichiers MIDI seront affichés ici</p>
                </div>
            \`;
        }
        
        return \`
            <div class="files-grid">
                \${files.map(file => this.renderFileCard(file, state.selectedFile)).join('')}
            </div>
        \`;
    }
    
    renderFileCard(file, selectedFile) {
        const isSelected = selectedFile && (selectedFile.id === file.id || selectedFile.name === file.name);
        const selectedClass = isSelected ? 'selected' : '';
        
        // Extraire nom et extension
        const fileName = file.name || file.id || 'Unnamed';
        const fileSize = this.formatFileSize(file.size);
        const fileDate = this.formatDate(file.modified || file.created);
        
        return \`
            <div class="file-card \${selectedClass}" 
                 data-file-id="\${file.id || file.name}"
                 data-action="select-file">
                <div class="file-icon">🎵</div>
                <div class="file-info">
                    <div class="file-name" title="\${fileName}">\${fileName}</div>
                    <div class="file-meta">
                        <span class="file-size">\${fileSize}</span>
                        <span class="file-date">\${fileDate}</span>
                    </div>
                </div>
                <div class="file-actions">
                    <button class="btn-icon" 
                            data-action="load-file" 
                            data-file-id="\${file.id || file.name}"
                            title="Charger">
                        ▶️
                    </button>
                    <button class="btn-icon btn-danger" 
                            data-action="delete-file" 
                            data-file-id="\${file.id || file.name}"
                            title="Supprimer">
                        🗑️
                    </button>
                </div>
            </div>
        \`;
    }
    
    // ========================================================================
    // FORMATTERS
    // ========================================================================
    
    formatFileSize(size) {
        if (!size) return 'N/A';
        
        if (size < 1024) {
            return \`\${size} B\`;
        } else if (size < 1024 * 1024) {
            return \`\${(size / 1024).toFixed(1)} KB\`;
        } else {
            return \`\${(size / (1024 * 1024)).toFixed(1)} MB\`;
        }
    }
    
    formatDate(timestamp) {
        if (!timestamp) return 'N/A';
        
        const date = new Date(timestamp);
        if (isNaN(date.getTime())) return 'N/A';
        
        const now = new Date();
        const diffMs = now - date;
        const diffMins = Math.floor(diffMs / 60000);
        const diffHours = Math.floor(diffMs / 3600000);
        const diffDays = Math.floor(diffMs / 86400000);
        
        if (diffMins < 1) {
            return 'À l\'instant';
        } else if (diffMins < 60) {
            return \`Il y a \${diffMins} min\`;
        } else if (diffHours < 24) {
            return \`Il y a \${diffHours}h\`;
        } else if (diffDays < 7) {
            return \`Il y a \${diffDays}j\`;
        } else {
            return date.toLocaleDateString('fr-FR');
        }
    }
    
    // ========================================================================
    // UPDATE MÉTHODES
    // ========================================================================
    
    updateFiles(files) {
        this.viewState.files = files;
        this.viewState.isLoading = false;
        this.render();
    }
    
    updateSelectedFile(file) {
        this.viewState.selectedFile = file;
        this.render();
    }
    
    setLoading(loading) {
        this.viewState.isLoading = loading;
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
            const fileId = target.dataset.fileId;
            
            e.stopPropagation();
            
            switch (action) {
                case 'refresh-files':
                    this.eventBus.emit('file:refresh');
                    break;
                    
                case 'select-file':
                    if (fileId) {
                        this.eventBus.emit('file:select', { fileId });
                    }
                    break;
                    
                case 'load-file':
                    if (fileId) {
                        this.eventBus.emit('file:load', { fileId });
                    }
                    break;
                    
                case 'delete-file':
                    if (fileId) {
                        const confirmed = confirm(\`Supprimer le fichier "\${fileId}" ?\`);
                        if (confirmed) {
                            this.eventBus.emit('file:delete', { fileId });
                        }
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
    window.FileView = FileView;
}
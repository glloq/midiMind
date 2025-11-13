// ============================================================================
// Fichier: frontend/js/views/components/FileSelectionModal.js
// Version: v4.2.2
// Description: Modal de sélection de fichiers MIDI pour ajout aux playlists
// ============================================================================

class FileSelectionModal {
    constructor(eventBus, fileModel, logger) {
        this.eventBus = eventBus || window.eventBus || null;
        this.fileModel = fileModel;
        this.logger = logger;
        this.container = null;
        this.isOpen = false;
        this.selectedFiles = new Set();
        this.midiFiles = [];
        this.onConfirm = null;
    }

    /**
     * Affiche le modal de sélection de fichiers
     * @param {Function} onConfirm - Callback appelé avec les fichiers sélectionnés
     * @param {Object} options - Options du modal (multiSelect, title, etc.)
     */
    async show(onConfirm, options = {}) {
        const {
            multiSelect = true,
            title = 'Sélectionner des fichiers MIDI',
            confirmText = 'Ajouter',
            cancelText = 'Annuler'
        } = options;

        this.onConfirm = onConfirm;
        this.selectedFiles.clear();
        this.multiSelect = multiSelect;

        // Charger les fichiers MIDI de la base de données
        try {
            this.midiFiles = await this.fileModel.getMidiFiles();
        } catch (error) {
            this.log('error', 'FileSelectionModal', `Failed to load MIDI files: ${error.message}`);
            this.showError('Erreur de chargement des fichiers MIDI');
            return;
        }

        this.close();

        this.container = document.createElement('div');
        this.container.className = 'modal-overlay file-selection-modal';
        this.container.innerHTML = `
            <div class="modal-dialog modal-lg">
                <div class="modal-header">
                    <h3>${title}</h3>
                    <button class="modal-close">&times;</button>
                </div>
                <div class="modal-body">
                    <div class="file-selection-search">
                        <input type="text"
                               class="form-control search-input"
                               placeholder="Rechercher un fichier..."
                               id="file-search">
                    </div>
                    <div class="file-selection-list" id="file-list">
                        ${this.renderFileList()}
                    </div>
                    <div class="file-selection-info">
                        <span id="selection-count">0 fichier(s) sélectionné(s)</span>
                    </div>
                </div>
                <div class="modal-footer">
                    <button class="btn btn-secondary" data-action="cancel">
                        ${cancelText}
                    </button>
                    <button class="btn btn-primary" data-action="confirm" disabled>
                        ${confirmText}
                    </button>
                </div>
            </div>
        `;

        document.body.appendChild(this.container);
        this.isOpen = true;

        this.attachEventListeners();
    }

    /**
     * Génère le HTML de la liste des fichiers
     */
    renderFileList(filter = '') {
        if (this.midiFiles.length === 0) {
            return '<div class="file-list-empty">Aucun fichier MIDI disponible</div>';
        }

        const filteredFiles = filter
            ? this.midiFiles.filter(file =>
                file.filename.toLowerCase().includes(filter.toLowerCase()))
            : this.midiFiles;

        if (filteredFiles.length === 0) {
            return '<div class="file-list-empty">Aucun fichier trouvé</div>';
        }

        return filteredFiles.map(file => `
            <div class="file-item" data-file-id="${file.id}">
                <div class="file-checkbox">
                    <input type="${this.multiSelect ? 'checkbox' : 'radio'}"
                           id="file-${file.id}"
                           name="file-selection"
                           value="${file.id}">
                </div>
                <div class="file-info">
                    <div class="file-name">${this.escapeHtml(file.filename)}</div>
                    <div class="file-meta">
                        <span class="file-duration">${this.formatDuration(file.duration_ms)}</span>
                        <span class="file-tracks">${file.track_count} pistes</span>
                        <span class="file-events">${file.event_count} événements</span>
                    </div>
                </div>
            </div>
        `).join('');
    }

    /**
     * Attache les événements au modal
     */
    attachEventListeners() {
        // Fermer le modal
        this.container.querySelector('.modal-close').addEventListener('click', () => this.close());
        this.container.addEventListener('click', (e) => {
            if (e.target === this.container) this.close();
        });

        // Bouton annuler
        this.container.querySelector('[data-action="cancel"]').addEventListener('click', () => {
            this.close();
        });

        // Bouton confirmer
        const confirmBtn = this.container.querySelector('[data-action="confirm"]');
        confirmBtn.addEventListener('click', () => {
            this.handleConfirm();
        });

        // Recherche
        const searchInput = this.container.querySelector('#file-search');
        searchInput.addEventListener('input', (e) => {
            this.handleSearch(e.target.value);
        });

        // Sélection de fichiers
        this.container.querySelectorAll('input[type="checkbox"], input[type="radio"]').forEach(input => {
            input.addEventListener('change', (e) => {
                this.handleFileSelection(e.target);
            });
        });
    }

    /**
     * Gère la recherche de fichiers
     */
    handleSearch(query) {
        const fileList = this.container.querySelector('#file-list');
        fileList.innerHTML = this.renderFileList(query);

        // Ré-attacher les événements de sélection
        this.container.querySelectorAll('input[type="checkbox"], input[type="radio"]').forEach(input => {
            input.addEventListener('change', (e) => {
                this.handleFileSelection(e.target);
            });

            // Restaurer l'état de sélection
            if (this.selectedFiles.has(parseInt(input.value))) {
                input.checked = true;
            }
        });
    }

    /**
     * Gère la sélection/désélection d'un fichier
     */
    handleFileSelection(input) {
        const fileId = parseInt(input.value);

        if (this.multiSelect) {
            if (input.checked) {
                this.selectedFiles.add(fileId);
            } else {
                this.selectedFiles.delete(fileId);
            }
        } else {
            this.selectedFiles.clear();
            if (input.checked) {
                this.selectedFiles.add(fileId);
            }
        }

        this.updateSelectionCount();
        this.updateConfirmButton();
    }

    /**
     * Met à jour le compteur de fichiers sélectionnés
     */
    updateSelectionCount() {
        const countElement = this.container.querySelector('#selection-count');
        if (countElement) {
            countElement.textContent = `${this.selectedFiles.size} fichier(s) sélectionné(s)`;
        }
    }

    /**
     * Active/désactive le bouton de confirmation
     */
    updateConfirmButton() {
        const confirmBtn = this.container.querySelector('[data-action="confirm"]');
        if (confirmBtn) {
            confirmBtn.disabled = this.selectedFiles.size === 0;
        }
    }

    /**
     * Gère la confirmation de la sélection
     */
    handleConfirm() {
        if (this.selectedFiles.size === 0) {
            return;
        }

        // Récupérer les détails des fichiers sélectionnés
        const selectedFileIds = Array.from(this.selectedFiles);
        const selectedFilesData = this.midiFiles.filter(file =>
            selectedFileIds.includes(file.id)
        );

        if (this.onConfirm) {
            this.onConfirm(selectedFilesData);
        }

        this.close();
    }

    /**
     * Ferme le modal
     */
    close() {
        if (this.container) {
            this.container.remove();
            this.container = null;
        }
        this.isOpen = false;
        this.selectedFiles.clear();
    }

    /**
     * Affiche une erreur
     */
    showError(message) {
        if (this.eventBus) {
            this.eventBus.emit('notification:show', {
                type: 'error',
                message: message
            });
        }
    }

    /**
     * Formate la durée en minutes:secondes
     */
    formatDuration(durationMs) {
        if (!durationMs || durationMs === 0) {
            return '0:00';
        }

        const totalSeconds = Math.floor(durationMs / 1000);
        const minutes = Math.floor(totalSeconds / 60);
        const seconds = totalSeconds % 60;

        return `${minutes}:${seconds.toString().padStart(2, '0')}`;
    }

    /**
     * Échappe le HTML pour éviter les injections XSS
     */
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    /**
     * Log helper
     */
    log(level, component, message) {
        if (this.logger) {
            this.logger[level](component, message);
        } else {
            console[level === 'error' ? 'error' : 'log'](`[${component}] ${message}`);
        }
    }
}

// ============================================================================
// EXPORT GLOBAL
// ============================================================================
if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileSelectionModal;
}
if (typeof window !== 'undefined') {
    window.FileSelectionModal = FileSelectionModal;
}

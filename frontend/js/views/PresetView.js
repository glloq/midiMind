// ============================================================================
// Fichier: frontend/js/views/PresetView.js
// Version: v1.0.0
// Date: 2025-10-28
// ============================================================================
// Description:
//   Vue pour l'interface de gestion des presets
//   - Liste des presets disponibles
//   - Prévisualisation du contenu
//   - Contrôles de chargement/sauvegarde/suppression
//   - Formulaire de création
// ============================================================================

class PresetView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        this.config.name = 'PresetView';
        
        // Données de la vue
        this.data = {
            presets: [],
            selectedPreset: null,
            showCreateForm: false
        };
    }
    
    /**
     * Initialisation de la vue
     */
    onInitialize() {
        this.logDebug('info', 'PresetView initialized');
    }
    
    /**
     * Construit le template HTML
     */
    buildTemplate() {
        return `
            <div class="preset-container">
                ${this.buildHeader()}
                ${this.buildPresetsList()}
                ${this.buildPresetDetails()}
                ${this.buildCreateForm()}
            </div>
        `;
    }
    
    /**
     * Construit l'en-tête
     */
    buildHeader() {
        return `
            <div class="preset-header">
                <h2>
                    <i class="icon-list"></i>
                    Gestion des Presets
                </h2>
                <div class="header-actions">
                    <button class="btn btn-primary" id="create-preset">
                        <i class="icon-plus"></i>
                        Nouveau Preset
                    </button>
                    <button class="btn btn-secondary" id="refresh-presets">
                        <i class="icon-refresh"></i>
                        Actualiser
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Construit la liste des presets
     */
    buildPresetsList() {
        return `
            <div class="presets-list card">
                <h3>Presets Disponibles (${this.data.presets.length})</h3>
                
                <div class="presets-grid">
                    ${this.data.presets.length === 0
                        ? '<p class="no-presets">Aucun preset disponible. Créez-en un nouveau.</p>'
                        : this.data.presets.map(preset => this.buildPresetCard(preset)).join('')
                    }
                </div>
            </div>
        `;
    }
    
    /**
     * Construit une carte de preset
     */
    buildPresetCard(preset) {
        const isSelected = this.data.selectedPreset?.id === preset.id;
        const selectedClass = isSelected ? 'preset-selected' : '';
        const isCurrent = preset.is_current || false;
        
        const metadata = preset.metadata || {};
        const name = metadata.name || `Preset ${preset.id}`;
        const description = metadata.description || 'Pas de description';
        const created = metadata.created ? this.formatDate(metadata.created) : 'Date inconnue';
        const routesCount = (preset.routes || []).length;
        const devicesCount = Object.keys(preset.deviceSettings || {}).length;
        
        return `
            <div class="preset-card ${selectedClass}" data-preset-id="${preset.id}">
                ${isCurrent ? '<div class="preset-badge current">Actuel</div>' : ''}
                
                <div class="preset-card-header">
                    <h4>${this.escapeHTML(name)}</h4>
                    ${metadata.version 
                        ? `<span class="preset-version">v${metadata.version}</span>`
                        : ''
                    }
                </div>
                
                <div class="preset-card-body">
                    <p class="preset-description">${this.escapeHTML(description)}</p>
                    
                    <div class="preset-info">
                        <div class="info-item">
                            <i class="icon-route"></i>
                            <span>${routesCount} route(s)</span>
                        </div>
                        <div class="info-item">
                            <i class="icon-device"></i>
                            <span>${devicesCount} périphérique(s)</span>
                        </div>
                        <div class="info-item">
                            <i class="icon-calendar"></i>
                            <span>${created}</span>
                        </div>
                    </div>
                </div>
                
                <div class="preset-card-actions">
                    <button class="btn btn-sm btn-primary btn-load-preset" 
                            data-preset-id="${preset.id}"
                            ${isCurrent ? 'disabled' : ''}>
                        <i class="icon-download"></i>
                        Charger
                    </button>
                    <button class="btn btn-sm btn-info btn-view-preset" 
                            data-preset-id="${preset.id}">
                        <i class="icon-eye"></i>
                        Voir
                    </button>
                    <button class="btn btn-sm btn-secondary btn-export-preset" 
                            data-preset-id="${preset.id}">
                        <i class="icon-export"></i>
                        Exporter
                    </button>
                    <button class="btn btn-sm btn-danger btn-delete-preset" 
                            data-preset-id="${preset.id}">
                        <i class="icon-trash"></i>
                        Supprimer
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Construit les détails du preset sélectionné
     */
    buildPresetDetails() {
        if (!this.data.selectedPreset) {
            return `
                <div class="preset-details card" style="display: none;">
                    <p class="no-selection">Sélectionnez un preset pour voir les détails.</p>
                </div>
            `;
        }
        
        const preset = this.data.selectedPreset;
        const metadata = preset.metadata || {};
        
        return `
            <div class="preset-details card">
                <h3>Détails du Preset: ${this.escapeHTML(metadata.name || 'Sans nom')}</h3>
                
                <div class="details-section">
                    <h4>Métadonnées</h4>
                    <dl class="details-list">
                        <dt>Nom:</dt>
                        <dd>${this.escapeHTML(metadata.name || 'N/A')}</dd>
                        
                        <dt>Description:</dt>
                        <dd>${this.escapeHTML(metadata.description || 'N/A')}</dd>
                        
                        <dt>Version:</dt>
                        <dd>${this.escapeHTML(metadata.version || 'N/A')}</dd>
                        
                        <dt>Créé le:</dt>
                        <dd>${metadata.created ? this.formatDate(metadata.created) : 'N/A'}</dd>
                        
                        ${metadata.author 
                            ? `<dt>Auteur:</dt><dd>${this.escapeHTML(metadata.author)}</dd>`
                            : ''
                        }
                    </dl>
                </div>
                
                <div class="details-section">
                    <h4>Routes (${(preset.routes || []).length})</h4>
                    ${this.buildRoutesList(preset.routes || [])}
                </div>
                
                <div class="details-section">
                    <h4>Paramètres des Périphériques</h4>
                    ${this.buildDeviceSettings(preset.deviceSettings || {})}
                </div>
            </div>
        `;
    }
    
    /**
     * Construit la liste des routes
     */
    buildRoutesList(routes) {
        if (routes.length === 0) {
            return '<p class="no-data">Aucune route définie.</p>';
        }
        
        return `
            <table class="routes-table">
                <thead>
                    <tr>
                        <th>Source</th>
                        <th>Destination</th>
                        <th>Activé</th>
                    </tr>
                </thead>
                <tbody>
                    ${routes.map(route => `
                        <tr>
                            <td>${this.escapeHTML(route.source_id || 'N/A')}</td>
                            <td>${this.escapeHTML(route.destination_id || 'N/A')}</td>
                            <td>
                                <span class="status-badge ${route.enabled ? 'enabled' : 'disabled'}">
                                    ${route.enabled ? 'Oui' : 'Non'}
                                </span>
                            </td>
                        </tr>
                    `).join('')}
                </tbody>
            </table>
        `;
    }
    
    /**
     * Construit les paramètres des périphériques
     */
    buildDeviceSettings(deviceSettings) {
        const devices = Object.keys(deviceSettings);
        
        if (devices.length === 0) {
            return '<p class="no-data">Aucun paramètre de périphérique.</p>';
        }
        
        return `
            <div class="device-settings">
                ${devices.map(deviceId => {
                    const settings = deviceSettings[deviceId];
                    return `
                        <div class="device-setting-item">
                            <h5>${this.escapeHTML(deviceId)}</h5>
                            <pre class="settings-json">${JSON.stringify(settings, null, 2)}</pre>
                        </div>
                    `;
                }).join('')}
            </div>
        `;
    }
    
    /**
     * Construit le formulaire de création
     */
    buildCreateForm() {
        if (!this.data.showCreateForm) {
            return '';
        }
        
        return `
            <div class="preset-create-form card">
                <div class="form-header">
                    <h3>Créer un Nouveau Preset</h3>
                    <button class="btn btn-sm btn-close" id="close-create-form">
                        <i class="icon-close"></i>
                    </button>
                </div>
                
                <form id="create-preset-form">
                    <div class="form-group">
                        <label for="preset-name">Nom du Preset *</label>
                        <input type="text" 
                               id="preset-name" 
                               class="form-control" 
                               required
                               placeholder="Mon Preset">
                    </div>
                    
                    <div class="form-group">
                        <label for="preset-description">Description</label>
                        <textarea id="preset-description" 
                                  class="form-control" 
                                  rows="3"
                                  placeholder="Description du preset..."></textarea>
                    </div>
                    
                    <div class="form-group">
                        <label for="preset-version">Version</label>
                        <input type="text" 
                               id="preset-version" 
                               class="form-control" 
                               value="1.0.0"
                               placeholder="1.0.0">
                    </div>
                    
                    <div class="form-actions">
                        <button type="submit" class="btn btn-primary">
                            <i class="icon-save"></i>
                            Sauvegarder
                        </button>
                        <button type="button" class="btn btn-secondary" id="cancel-create-form">
                            Annuler
                        </button>
                    </div>
                </form>
            </div>
        `;
    }
    
    /**
     * Attache les événements DOM
     */
    attachEvents() {
        // Actualiser la liste
        this.addDOMEventListener('#refresh-presets', 'click', () => {
            this.emit('preset:list');
        });
        
        // Créer un nouveau preset
        this.addDOMEventListener('#create-preset', 'click', () => {
            this.data.showCreateForm = true;
            this.render();
        });
        
        // Fermer le formulaire de création
        this.addDOMEventListener('#close-create-form', 'click', () => {
            this.data.showCreateForm = false;
            this.render();
        });
        
        this.addDOMEventListener('#cancel-create-form', 'click', () => {
            this.data.showCreateForm = false;
            this.render();
        });
        
        // Soumettre le formulaire de création
        this.addDOMEventListener('#create-preset-form', 'submit', (e) => {
            e.preventDefault();
            
            const name = document.getElementById('preset-name').value.trim();
            const description = document.getElementById('preset-description').value.trim();
            const version = document.getElementById('preset-version').value.trim();
            
            if (!name) {
                alert('Le nom du preset est requis');
                return;
            }
            
            this.emit('preset:create-new', {
                name,
                description,
                version
            });
            
            this.data.showCreateForm = false;
            this.render();
        });
        
        // Charger un preset
        this.addDOMEventListener('.btn-load-preset', 'click', (e) => {
            const presetId = parseInt(e.target.closest('.btn-load-preset').dataset.presetId);
            this.emit('preset:load', { id: presetId });
        }, true);
        
        // Voir les détails d'un preset
        this.addDOMEventListener('.btn-view-preset', 'click', (e) => {
            const presetId = parseInt(e.target.closest('.btn-view-preset').dataset.presetId);
            this.emit('preset:select', { id: presetId });
        }, true);
        
        // Exporter un preset
        this.addDOMEventListener('.btn-export-preset', 'click', (e) => {
            const presetId = parseInt(e.target.closest('.btn-export-preset').dataset.presetId);
            const filepath = prompt('Chemin d\'export:', `/tmp/preset_${presetId}.json`);
            
            if (filepath) {
                this.emit('preset:export', { id: presetId, filepath });
            }
        }, true);
        
        // Supprimer un preset
        this.addDOMEventListener('.btn-delete-preset', 'click', (e) => {
            const presetId = parseInt(e.target.closest('.btn-delete-preset').dataset.presetId);
            this.emit('preset:delete', { id: presetId });
        }, true);
        
        // Sélectionner une carte de preset
        this.addDOMEventListener('.preset-card', 'click', (e) => {
            if (!e.target.closest('button')) {
                const presetId = parseInt(e.target.closest('.preset-card').dataset.presetId);
                this.emit('preset:select', { id: presetId });
            }
        }, true);
    }
    
    /**
     * Cache les éléments DOM
     */
    cacheElements() {
        this.elements = {
            presetsGrid: this.container.querySelector('.presets-grid'),
            presetDetails: this.container.querySelector('.preset-details'),
            createForm: this.container.querySelector('.preset-create-form')
        };
    }
    
    /**
     * Affiche un message d'erreur
     */
    showError(message) {
        const errorDiv = document.createElement('div');
        errorDiv.className = 'preset-error alert alert-danger';
        errorDiv.textContent = message;
        
        if (this.container) {
            this.container.insertBefore(errorDiv, this.container.firstChild);
            
            setTimeout(() => {
                errorDiv.remove();
            }, 5000);
        }
    }
    
    /**
     * Obtient l'état de la vue
     */
    getViewState() {
        return {
            ...this.getState(),
            presetsCount: this.data.presets.length,
            hasSelection: !!this.data.selectedPreset,
            showingForm: this.data.showCreateForm
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PresetView;
}

if (typeof window !== 'undefined') {
    window.PresetView = PresetView;
}
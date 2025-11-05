// ============================================================================
// Fichier: frontend/js/views/PresetView.js
// Version: v4.0.0
// ============================================================================

class PresetView {
    constructor(containerId, eventBus) {
        this.container = typeof containerId === 'string' ? 
            document.getElementById(containerId) : containerId;
        this.eventBus = eventBus;
        
        this.state = {
            presets: [],
            selectedPreset: null
        };
    }
    
    init() {
        if (!this.container) return;
        this.render();
        this.attachEvents();
        this.loadPresets();
    }
    
    render() {
        this.container.innerHTML = `
            <div class="preset-view">
                <div class="preset-header">
                    <h2>💾 Presets</h2>
                    <button data-action="new">Nouveau preset</button>
                </div>
                
                <div class="preset-list">
                    ${this.state.presets.length > 0 ?
                        this.state.presets.map(p => this.renderPreset(p)).join('') :
                        '<p class="empty">Aucun preset</p>'}
                </div>
            </div>
        `;
    }
    
    renderPreset(preset) {
        const isSelected = this.state.selectedPreset?.id === preset.id;
        return `
            <div class="preset-item ${isSelected ? 'selected' : ''}" data-id="${preset.id}">
                <div class="preset-info">
                    <div class="preset-name">${preset.name}</div>
                    <div class="preset-desc">${preset.description || ''}</div>
                </div>
                <div class="preset-actions">
                    <button data-action="load">Charger</button>
                    <button data-action="save">Sauver</button>
                    <button data-action="delete">Supprimer</button>
                    <button data-action="export">Exporter</button>
                </div>
            </div>
        `;
    }
    
    attachEvents() {
        this.container.addEventListener('click', (e) => {
            const action = e.target.dataset.action;
            const presetEl = e.target.closest('.preset-item');
            const presetId = presetEl?.dataset.id;
            
            switch(action) {
                case 'new': this.createPreset(); break;
                case 'load': if (presetId) this.loadPreset(presetId); break;
                case 'save': if (presetId) this.savePreset(presetId); break;
                case 'delete': if (presetId) this.deletePreset(presetId); break;
                case 'export': if (presetId) this.exportPreset(presetId); break;
            }
        });
        
        if (!this.eventBus) return;
        
        this.eventBus.on('preset:list', (data) => {
            this.state.presets = data.presets || [];
            this.render();
        });
        
        this.eventBus.on('preset:loaded', (data) => {
            this.state.selectedPreset = data.preset;
            this.render();
        });
    }
    
    createPreset() {
        const name = prompt('Nom du preset:');
        if (!name) return;
        
        this.eventBus?.emit('preset:save_requested', {
            name,
            description: ''
        });
    }
    
    loadPreset(presetId) {
        this.eventBus?.emit('preset:load_requested', { preset_id: presetId });
    }
    
    savePreset(presetId) {
        this.eventBus?.emit('preset:save_requested', { preset_id: presetId });
    }
    
    deletePreset(presetId) {
        if (!confirm('Supprimer ce preset ?')) return;
        this.eventBus?.emit('preset:delete_requested', { preset_id: presetId });
    }
    
    exportPreset(presetId) {
        this.eventBus?.emit('preset:export_requested', { preset_id: presetId });
    }
    
    loadPresets() {
        this.eventBus?.emit('preset:list_requested');
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PresetView;
}
if (typeof window !== 'undefined') {
    window.PresetView = PresetView;
}
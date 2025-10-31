// ============================================================================
// Fichier: frontend/js/models/EditorModel.js
// Chemin réel: frontend/js/models/EditorModel.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE
// Date: 2025-10-31
// ============================================================================

class EditorModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, backend, logger, {}, {
            persistKey: 'editormodel',
            eventPrefix: 'editor',
            autoPersist: false
        });
        
        this.data.midiData = null;
        this.data.selectedNotes = [];
        this.data.zoom = 1.0;
        this.data.snapToGrid = true;
        this.data.gridSize = 16;
        
        this.log('debug', 'EditorModel', 'Initialized');
    }
    
    setMidiData(midiData) {
        this.data.midiData = midiData;
        this.emit('editor:midiData:changed', { midiData });
    }
    
    getMidiData() {
        return this.data.midiData;
    }
    
    selectNotes(notes) {
        this.data.selectedNotes = notes;
        this.emit('editor:selection:changed', { notes });
    }
    
    getSelectedNotes() {
        return this.data.selectedNotes;
    }
    
    setZoom(zoom) {
        this.data.zoom = Math.max(0.1, Math.min(10, zoom));
        this.emit('editor:zoom:changed', { zoom: this.data.zoom });
    }
    
    getZoom() {
        return this.data.zoom;
    }
    
    setSnapToGrid(enabled) {
        this.data.snapToGrid = enabled;
        this.emit('editor:snap:changed', { enabled });
    }
    
    setGridSize(size) {
        this.data.gridSize = size;
        this.emit('editor:grid:changed', { size });
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = EditorModel;
}

if (typeof window !== 'undefined') {
    window.EditorModel = EditorModel;
}
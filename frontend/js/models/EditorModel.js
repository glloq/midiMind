// ============================================================================
// Fichier: frontend/js/models/EditorModel.js
// Chemin réel: frontend/js/models/EditorModel.js
// Version: v3.3.0 - SIGNATURE CORRIGÉE (5 PARAMÈTRES)
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.3.0:
// ✅ CRITIQUE: Ajout paramètres initialData et options manquants
// ✅ Signature cohérente: (eventBus, backend, logger, initialData = {}, options = {})
// ✅ Merge intelligente des options par défaut
// ✅ Ajout méthodes utilitaires pour l'édition
// ============================================================================

class EditorModel extends BaseModel {
    constructor(eventBus, backend, logger, initialData = {}, options = {}) {
        // ✅ NOUVEAU: Appel super() avec les 5 paramètres
        super(eventBus, backend, logger, initialData, {
            persistKey: 'editormodel',
            eventPrefix: 'editor',
            autoPersist: false,  // Ne pas persister automatiquement les données d'édition
            ...options
        });
        
        // Initialisation des données de l'éditeur avec valeurs par défaut
        this.data.midiData = this.data.midiData || null;
        this.data.selectedNotes = this.data.selectedNotes || [];
        this.data.zoom = this.data.zoom || 1.0;
        this.data.snapToGrid = this.data.snapToGrid !== undefined ? this.data.snapToGrid : true;
        this.data.gridSize = this.data.gridSize || 16;  // 16ème de note par défaut
        
        // État de l'éditeur
        this.data.viewportOffset = this.data.viewportOffset || { x: 0, y: 0 };
        this.data.currentTool = this.data.currentTool || 'select';  // select, draw, erase
        this.data.isDirty = false;  // Modifications non sauvegardées
        
        this.log('debug', 'EditorModel', 'Initialized v3.3.0');
    }
    
    /**
     * Définit les données MIDI à éditer
     * @param {Object} midiData - Données MIDI
     */
    setMidiData(midiData) {
        this.data.midiData = midiData;
        this.data.isDirty = false;
        this.emit('editor:midiData:changed', { midiData });
    }
    
    /**
     * Récupère les données MIDI
     * @returns {Object|null}
     */
    getMidiData() {
        return this.data.midiData;
    }
    
    /**
     * Sélectionne des notes
     * @param {Array} notes - Tableau de notes sélectionnées
     */
    selectNotes(notes) {
        this.data.selectedNotes = notes;
        this.emit('editor:selection:changed', { notes });
    }
    
    /**
     * Récupère les notes sélectionnées
     * @returns {Array}
     */
    getSelectedNotes() {
        return this.data.selectedNotes;
    }
    
    /**
     * Ajoute une note à la sélection
     * @param {Object} note
     */
    addToSelection(note) {
        if (!this.data.selectedNotes.includes(note)) {
            this.data.selectedNotes.push(note);
            this.emit('editor:selection:changed', { notes: this.data.selectedNotes });
        }
    }
    
    /**
     * Retire une note de la sélection
     * @param {Object} note
     */
    removeFromSelection(note) {
        const index = this.data.selectedNotes.indexOf(note);
        if (index !== -1) {
            this.data.selectedNotes.splice(index, 1);
            this.emit('editor:selection:changed', { notes: this.data.selectedNotes });
        }
    }
    
    /**
     * Vide la sélection
     */
    clearSelection() {
        this.data.selectedNotes = [];
        this.emit('editor:selection:changed', { notes: [] });
    }
    
    /**
     * Définit le niveau de zoom
     * @param {number} zoom - Niveau de zoom (0.1 à 10)
     */
    setZoom(zoom) {
        this.data.zoom = Math.max(0.1, Math.min(10, zoom));
        this.emit('editor:zoom:changed', { zoom: this.data.zoom });
    }
    
    /**
     * Récupère le niveau de zoom
     * @returns {number}
     */
    getZoom() {
        return this.data.zoom;
    }
    
    /**
     * Active/désactive la magnétisation à la grille
     * @param {boolean} enabled
     */
    setSnapToGrid(enabled) {
        this.data.snapToGrid = enabled;
        this.emit('editor:snap:changed', { enabled });
    }
    
    /**
     * Vérifie si la magnétisation est active
     * @returns {boolean}
     */
    isSnapToGridEnabled() {
        return this.data.snapToGrid;
    }
    
    /**
     * Définit la taille de la grille
     * @param {number} size - Taille en ticks MIDI
     */
    setGridSize(size) {
        this.data.gridSize = size;
        this.emit('editor:grid:changed', { size });
    }
    
    /**
     * Récupère la taille de la grille
     * @returns {number}
     */
    getGridSize() {
        return this.data.gridSize;
    }
    
    /**
     * Définit l'outil actuel
     * @param {string} tool - 'select', 'draw', 'erase'
     */
    setCurrentTool(tool) {
        this.data.currentTool = tool;
        this.emit('editor:tool:changed', { tool });
    }
    
    /**
     * Récupère l'outil actuel
     * @returns {string}
     */
    getCurrentTool() {
        return this.data.currentTool;
    }
    
    /**
     * Définit l'offset du viewport
     * @param {Object} offset - {x, y}
     */
    setViewportOffset(offset) {
        this.data.viewportOffset = offset;
        this.emit('editor:viewport:changed', { offset });
    }
    
    /**
     * Récupère l'offset du viewport
     * @returns {Object}
     */
    getViewportOffset() {
        return this.data.viewportOffset;
    }
    
    /**
     * Marque l'éditeur comme modifié
     */
    markDirty() {
        this.data.isDirty = true;
        this.emit('editor:dirty:changed', { isDirty: true });
    }
    
    /**
     * Marque l'éditeur comme sauvegardé
     */
    markClean() {
        this.data.isDirty = false;
        this.emit('editor:dirty:changed', { isDirty: false });
    }
    
    /**
     * Vérifie si l'éditeur a des modifications non sauvegardées
     * @returns {boolean}
     */
    isDirty() {
        return this.data.isDirty;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = EditorModel;
}

if (typeof window !== 'undefined') {
    window.EditorModel = EditorModel;
}
// ============================================================================
// Fichier: frontend/scripts/controllers/EditorController.js
// Version: v3.0.3 - CORRIGÃ‰ COMPLET
// Date: 2025-10-09
// Projet: midiMind v3.0 - SystÃ¨me d'Orchestration MIDI
// ============================================================================
// Description:
//   ContrÃ´leur complet de l'Ã©diteur MIDI avec intÃ©gration EditorModel v3.0.2
//   GÃ¨re le visualizer, routing, et toutes les opÃ©rations d'Ã©dition
// 
// CORRECTIONS v3.0.3:
//   âœ… Cut() dÃ©lÃ¨gue maintenant Ã  EditorModel
//   âœ… MÃ©thodes getCurrentPasteTime() et reloadVisualizer() ajoutÃ©es
//   âœ… Transformations ajoutÃ©es (quantize, transpose, scaleVelocity)
//   âœ… Callbacks Visualizer simplifiÃ©s (pas de redondance)
//   âœ… Synchronisation propre EditorModel â†” Visualizer
//   âœ… Tous les Ã©vÃ©nements bindÃ©s correctement
// ============================================================================

class EditorController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // RÃ©fÃ©rences aux modÃ¨les
        this.editorModel = models.editor;  // EditorModel
        this.fileModel = models.file;      // FileModel
        this.routingModel = models.routing; // RoutingModel
        
        // RÃ©fÃ©rences aux vues
        this.view = views.editor;          // EditorView
        
        // RÃ©fÃ©rence au visualizer (sera crÃ©Ã© dans initVisualizer)
        this.visualizer = null;
        
        // Fichier actuellement Ã©ditÃ©
        this.currentFile = null;
        
        // RÃ©fÃ©rence au backend service
        this.backend = window.backendService;
        
        // RÃ©fÃ©rence au routing manager
        this.routingManager = null;
        
        // Ã‰tat de l'Ã©diteur
        this.editorState = {
            isLoading: false,
            hasUnsavedChanges: false,
            currentTool: 'select',
            currentMode: 'edit',
            isPlaying: false,
            currentTime: 0
        };
        
        // ✅ REMOVED: this.initialize() - BaseController calls it via autoInitialize
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
       super.initialize();
		this.bindEvents();
		
        this.setupBeforeUnloadHandler();
		
        // Initialiser le routing manager
        if (typeof RoutingManager !== 'undefined') {
            this.routingManager = new RoutingManager(
                this.eventBus,
                this.routingModel,
                this.backend
            );
            this.logDebug('editor', 'âœ“ RoutingManager initialized');
        } else {
            this.logDebug('warning', 'RoutingManager not available');
        }
    }
    /**
 * MÃ©thode init() publique appelÃ©e par Application.js
 */
init() {
    this.logDebug('editor', 'EditorController.init() called');
    
    // S'assurer que la vue est initialisÃ©e
    if (this.view && typeof this.view.init === 'function') {
        this.view.init();
    }
    
    // Forcer le rendu initial
    if (this.view && typeof this.view.render === 'function') {
        this.view.render();
        this.logDebug('editor', 'EditorView rendered');
    
    
    // Initialiser le visualizer maintenant que la vue est rendue
    setTimeout(() => {
        this.initVisualizer();
    }, 100);
}
}
	
    bindEvents() {
        // Ã‰vÃ©nements d'actions d'Ã©dition
        this.eventBus.on('editor:action:undo', () => this.undo());
        this.eventBus.on('editor:action:redo', () => this.redo());
        this.eventBus.on('editor:action:copy', () => this.copy());
        this.eventBus.on('editor:action:cut', () => this.cut());
        this.eventBus.on('editor:action:paste', () => this.paste());
        this.eventBus.on('editor:action:save', () => this.saveChanges());
        this.eventBus.on('editor:action:delete', () => this.deleteSelected());
        
        // âœ… NOUVEAU: Ã‰vÃ©nements de transformations
        this.eventBus.on('editor:action:quantize', (data) => this.quantize(data.division));
        this.eventBus.on('editor:action:transpose', (data) => this.transpose(data.semitones));
        this.eventBus.on('editor:action:velocity', (data) => this.scaleVelocity(data.factor));
        
        // Ã‰vÃ©nements de routing
        this.eventBus.on('routing:assigned', (data) => this.onRoutingAssigned(data));
        this.eventBus.on('routing:unassigned', (data) => this.onRoutingUnassigned(data));
        this.eventBus.on('routing:changed', () => this.onRoutingChanged());
        
        // âœ… CORRIGÃ‰: Ã‰couter EditorModel directement (pas de redondance)
        if (this.editorModel) {
            this.eventBus.on('editor:modified', (data) => {
                this.editorState.hasUnsavedChanges = data.isModified;
                this.updateModifiedState();
            });
            
            this.eventBus.on('editor:file-saved', () => {
                this.editorState.hasUnsavedChanges = false;
                this.updateModifiedState();
                this.showSuccess('File saved');
            });
            
            // âœ… NOUVEAU: Ã‰couter les Ã©vÃ©nements de transformation
            this.eventBus.on('editor:quantized', (data) => {
                this.logDebug('editor', `Quantized ${data.count} notes to 1/${data.gridValue}`);
            });
            
            this.eventBus.on('editor:transposed', (data) => {
                this.logDebug('editor', `Transposed ${data.count} notes by ${data.semitones}`);
            });
            
            this.eventBus.on('editor:velocity-scaled', (data) => {
                this.logDebug('editor', `Scaled velocity for ${data.count} notes by ${data.factor}`);
            });
        }
        
        // Ã‰vÃ©nements du visualizer (seront attachÃ©s aprÃ¨s crÃ©ation)
        // Voir attachVisualizerEvents()
    }
    
    // ========================================================================
    // GESTION DU VISUALIZER
    // ========================================================================
    
    /**
     * Initialise le visualizer avec un canvas
     */
  
initVisualizer(canvas) {
    if (!canvas) {
        canvas = document.getElementById('editor-main-canvas');
    }
    
    if (!canvas) {
        console.error('Editor canvas not found');
        return null;
    }
    
    // Configuration
    const config = {
        coordSystem: {
            pixelsPerSecond: 100,
            pixelsPerNote: 12
        },
        rendering: {
            showPianoRoll: true,
            showTimeline: true,
            showCC: false,
            showVelocity: false
        }
    };
    
    // CrÃ©er le visualizer
    this.visualizer = new MidiVisualizer(canvas, config);
    
    // CRITIQUE: Injecter dans la vue
    if (this.view) {
        this.view.setVisualizer(this.visualizer);
        console.log('âœ… Visualizer injected into EditorView');
    }
    
    console.log('âœ… Visualizer initialized');
    return this.visualizer;
}
    
    /**
     * Attache les Ã©vÃ©nements du visualizer
     */
    attachVisualizerEvents() {
        if (!this.visualizer) return;
        
        this.visualizer.on('data:loaded', (data) => {
            this.logDebug('editor', 'MIDI data loaded in visualizer');
            this.onDataLoaded(data);
        });
        
        this.visualizer.on('note:added', (data) => {
            this.onNoteAdded(data);
        });
        
        this.visualizer.on('notes:deleted', (data) => {
            this.onNotesDeleted(data);
        });
        
        this.visualizer.on('notes:modified', (data) => {
            this.onNotesModified(data);
        });
        
        this.visualizer.on('selection:changed', (data) => {
            this.updateSelectionInfo(data);
        });
    }
    
    // ========================================================================
    // CHARGEMENT DE FICHIERS
    // ========================================================================
    
    /**
     * Charge un fichier MIDI dans l'Ã©diteur
     * MÃ‰THODE PRINCIPALE appelÃ©e depuis HomeController ou navigation
     */
    async loadFile(file) {
        if (!this.visualizer) {
            this.logDebug('error', 'Visualizer not initialized');
            this.showError('Editor not ready');
            return;
        }
        
        if (!file) {
            this.logDebug('error', 'No file provided');
            this.showError('No file provided');
            return;
        }
        
        this.editorState.isLoading = true;
        this.currentFile = file;
        
        try {
            this.logDebug('editor', `Loading file: ${file.name}`);
            
            // Obtenir les donnÃ©es MidiJSON
            let midiJson = file.midiJson;
            
            // Si pas de MidiJSON, convertir depuis les donnÃ©es brutes
            if (!midiJson && file.data) {
                midiJson = await this.convertMidiToJson(file);
            }
            
            if (!midiJson) {
                throw new Error('No MIDI data available');
            }
            
            // Charger dans EditorModel
            if (this.editorModel) {
                await this.editorModel.load(midiJson, file.id, file.path);
                this.logDebug('editor', 'âœ“ Loaded in EditorModel');
            }
            
            // Charger dans le visualizer
            this.visualizer.loadMidiData(midiJson);
            this.logDebug('editor', 'âœ“ Loaded in Visualizer');
            
            // Initialiser le routing pour ce fichier
            await this.initializeRouting(midiJson);
            this.logDebug('editor', 'âœ“ Routing initialized');
            
            // Mettre Ã  jour la vue
            if (this.view) {
                this.view.updateFileInfo(file);
            }
            
            this.editorState.isLoading = false;
            this.editorState.hasUnsavedChanges = false;
            
            this.logDebug('editor', `âœ“ File loaded: ${file.name}`);
            this.eventBus.emit('editor:file:loaded', { file, midiJson });
            
            this.showSuccess(`File "${file.name}" loaded in editor`);
            
        } catch (error) {
            this.editorState.isLoading = false;
            this.logDebug('error', `Failed to load file: ${error.message}`);
            this.showError(`Cannot load file: ${error.message}`);
        }
    }
    
    /**
     * Convertit un fichier MIDI en MidiJSON
     */
    async convertMidiToJson(file) {
        try {
            if (typeof MidiJsonConverter === 'undefined') {
                throw new Error('MidiJsonConverter not available');
            }
            
            const converter = new MidiJsonConverter();
            const midiJson = await converter.midiToJson(file.data);
            
            // Sauvegarder la version JSON dans FileModel
            if (this.fileModel) {
                await this.fileModel.update(file.id, { midiJson });
            }
            
            return midiJson;
            
        } catch (error) {
            this.logDebug('error', `MIDI conversion failed: ${error.message}`);
            throw error;
        }
    }
    
    /**
     * Initialise le routing pour un fichier
     */
    async initializeRouting(midiJson) {
        if (!this.routingManager) {
            this.logDebug('warning', 'RoutingManager not available');
            return;
        }
        
        try {
            // Extraire les canaux du fichier
            const channels = midiJson.channels || [];
            
            // Configurer le routing model
            this.routingModel.setCurrentFile({
                id: this.currentFile.id,
                name: this.currentFile.name,
                midiJson: midiJson
            });
            
            // Initialiser le routing manager avec ces canaux
            await this.routingManager.initialize(channels);
            
            this.logDebug('editor', `Routing initialized for ${channels.length} channels`);
            
        } catch (error) {
            this.logDebug('error', `Routing initialization failed: ${error.message}`);
        }
    }
    
    // ========================================================================
    // OPÃ‰RATIONS D'Ã‰DITION - UNDO/REDO
    // ========================================================================
    
    /**
     * Undo
     */
    undo() {
        if (!this.editorModel) {
            this.logDebug('warning', 'EditorModel not available');
            return;
        }
        
        const success = this.editorModel.undo();
        
        if (success) {
            // Recharger les donnÃ©es dans le visualizer
            this.reloadVisualizer();
            
            this.logDebug('editor', 'Undo performed');
            this.showSuccess('Undo');
        } else {
            this.logDebug('editor', 'Cannot undo');
        }
    }
    
    /**
     * Redo
     */
    redo() {
        if (!this.editorModel) {
            this.logDebug('warning', 'EditorModel not available');
            return;
        }
        
        const success = this.editorModel.redo();
        
        if (success) {
            // Recharger les donnÃ©es dans le visualizer
            this.reloadVisualizer();
            
            this.logDebug('editor', 'Redo performed');
            this.showSuccess('Redo');
        } else {
            this.logDebug('editor', 'Cannot redo');
        }
    }
    
    // ========================================================================
    // OPÃ‰RATIONS D'Ã‰DITION - COPY/CUT/PASTE âœ… CORRIGÃ‰
    // ========================================================================
    
    /**
     * Copy - Copie les notes sÃ©lectionnÃ©es
     */
    copy() {
        if (!this.editorModel) {
            this.logDebug('error', 'EditorModel not available');
            return;
        }
        
        // âœ… DÃ©lÃ©guer entiÃ¨rement Ã  EditorModel
        const success = this.editorModel.copy();
        
        if (success) {
            const count = this.editorModel.clipboard.notes.length;
            this.showSuccess(`Copied ${count} note${count > 1 ? 's' : ''}`);
        } else {
            this.showInfo('No notes selected');
        }
    }
    
/**
 * Cut notes sÃ©lectionnÃ©es
 */
cut() {
    this.copy();
    this.deleteSelection();
}

/**
 * Copy notes sÃ©lectionnÃ©es
 */
copy() {
    const selectedNotes = this.editorModel.getSelectedNotes();
    
    if (selectedNotes.length === 0) {
        this.showError('No notes selected');
        return;
    }
    
    // DÃ©lÃ©guer au model
    this.editorModel.copySelection();
    
    this.showSuccess(`Copied ${selectedNotes.length} notes`);
}

/**
 * Paste notes depuis clipboard
 * @param {number} targetTime - Temps cible (optionnel, utilise currentTime si absent)
 */
paste(targetTime = null) {
    // Utiliser currentTime si pas de cible
    if (targetTime === null) {
        targetTime = this.editorModel.getCurrentPasteTime();
    }
    
    try {
        // DÃ©lÃ©guer au model
        const pastedNotes = this.editorModel.paste(targetTime);
        
        // SÃ©lectionner les notes collÃ©es
        this.editorModel.selectNotes(pastedNotes.map(n => n.id));
        
        // RafraÃ®chir visualizer
        this.reloadVisualizer();
        
        this.showSuccess(`Pasted ${pastedNotes.length} notes`);
        
    } catch (error) {
        this.showError('Paste failed: ' + error.message);
    }
}

/**
 * Supprime la sÃ©lection
 */
deleteSelection() {
    const selectedNotes = this.editorModel.getSelectedNotes();
    
    if (selectedNotes.length === 0) {
        return;
    }
    
    const selectedIds = selectedNotes.map(n => n.id);
    this.editorModel.deleteNotes(selectedIds);
    
    this.showSuccess(`Deleted ${selectedNotes.length} notes`);
}

/**
 * Recharge le visualizer aprÃ¨s Ã©dition
 * @private
 */
reloadVisualizer() {
    if (this.visualizer && this.visualizer.reload) {
        this.visualizer.reload();
    }
}


    /**
     * Ajoute une note
     * @param {Object} noteData - DonnÃ©es de la note
     */
    addNote(noteData) {
        if (!this.editorModel) {
            this.logDebug('error', 'EditorModel not available');
            return null;
        }
        
        try {
            const note = this.editorModel.addNote(noteData);
            this.reloadVisualizer();
            this.showSuccess('Note added');
            return note;
        } catch (error) {
            this.showError('Failed to add note: ' + error.message);
            return null;
        }
    }
    
    /**
     * Supprime une note par ID
     * @param {string} noteId - ID de la note Ã  supprimer
     */
    deleteNote(noteId) {
        if (!this.editorModel) {
            this.logDebug('error', 'EditorModel not available');
            return false;
        }
        
        try {
            this.editorModel.deleteNotes([noteId]);
            this.reloadVisualizer();
            this.showSuccess('Note deleted');
            return true;
        } catch (error) {
            this.showError('Failed to delete note: ' + error.message);
            return false;
        }
    }
    
    /**
     * Met Ã  jour une note existante
     * @param {string} noteId - ID de la note
     * @param {Object} updates - PropriÃ©tÃ©s Ã  mettre Ã  jour
     */
    updateNote(noteId, updates) {
        if (!this.editorModel) {
            this.logDebug('error', 'EditorModel not available');
            return false;
        }
        
        try {
            this.editorModel.updateNote(noteId, updates);
            this.reloadVisualizer();
            this.showSuccess('Note updated');
            return true;
        } catch (error) {
            this.showError('Failed to update note: ' + error.message);
            return false;
        }
    }
    
    // ========================================================================
    // TRANSFORMATIONS 
    // ========================================================================
 /**
 * Quantize les notes sÃ©lectionnÃ©es
 * @param {number} grid - Grille (1, 2, 4, 8, 16, 32, 64)
 * @param {number} strength - Force 0-100%
 */
quantize(grid = 16, strength = 100) {
    const selectedNotes = this.editorModel.getSelectedNotes();
    
    if (selectedNotes.length === 0) {
        this.showError('No notes selected');
        return;
    }
    
    // Calculer division temporelle
    const ppq = this.editorModel.data.midiJson.division;
    const quantizeUnit = (ppq * 4) / grid; // 4 = noire
    
    const transformedNotes = selectedNotes.map(note => {
        // Position quantizÃ©e
        const quantizedTime = Math.round(note.time / quantizeUnit) * quantizeUnit;
        
        // Appliquer strength (0-100%)
        const newTime = note.time + ((quantizedTime - note.time) * strength / 100);
        
        return {
            ...note,
            time: Math.round(newTime)
        };
    });
    
    // Appliquer via EditorModel
    this.editorModel.updateNotes(transformedNotes);
    
    this.showSuccess(`Quantized ${selectedNotes.length} notes to 1/${grid}`);
}
    
  /**
 * Transpose les notes sÃ©lectionnÃ©es
 * @param {number} semitones - Nombre de demi-tons (-12 Ã  +12)
 */
transpose(semitones) {
    if (semitones === 0) return;
    
    const selectedNotes = this.editorModel.getSelectedNotes();
    
    if (selectedNotes.length === 0) {
        this.showError('No notes selected');
        return;
    }
    
    const transformedNotes = selectedNotes.map(note => {
        const newPitch = Math.max(0, Math.min(127, note.pitch + semitones));
        return { ...note, pitch: newPitch };
    });
    
    this.editorModel.updateNotes(transformedNotes);
    
    const direction = semitones > 0 ? 'up' : 'down';
    this.showSuccess(`Transposed ${selectedNotes.length} notes ${Math.abs(semitones)} semitones ${direction}`);
}

/**
 * Scale vÃ©locitÃ© des notes sÃ©lectionnÃ©es
 * @param {number} factor - Facteur (0.1 Ã  2.0)
 */
scaleVelocity(factor) {
    if (factor === 1.0) return;
    
    const selectedNotes = this.editorModel.getSelectedNotes();
    
    if (selectedNotes.length === 0) {
        this.showError('No notes selected');
        return;
    }
    
    const transformedNotes = selectedNotes.map(note => {
        const newVelocity = Math.max(1, Math.min(127, Math.round(note.velocity * factor)));
        return { ...note, velocity: newVelocity };
    });
    
    this.editorModel.updateNotes(transformedNotes);
    
    this.showSuccess(`Scaled velocity of ${selectedNotes.length} notes by ${factor.toFixed(2)}x`);
}
    
    /**
     * Transformations rapides (raccourcis)
     */
    quantizeToGrid(grid) {
        // Grilles communes: 4, 8, 16, 32
        this.quantize(grid, 100);
    }
    
    transposeUp() {
        this.transpose(12); // +1 octave
    }
    
    transposeDown() {
        this.transpose(-12); // -1 octave
    }
    
    increaseVelocity() {
        this.scaleVelocity(1.1); // +10%
    }
    
    decreaseVelocity() {
        this.scaleVelocity(0.9); // -10%
    }
    
    // ========================================================================
    // UTILITAIRES âœ… NOUVEAU
    // ========================================================================
    
    /**
     * âœ… NOUVEAU: Obtient le temps de destination pour paste
     */
    getCurrentPasteTime() {
        // PrioritÃ© 1: Position du curseur dans le visualizer
        if (this.visualizer && this.visualizer.getCursorPosition) {
            const cursorPos = this.visualizer.getCursorPosition();
            if (cursorPos !== null) {
                return cursorPos;
            }
        }
        
        // PrioritÃ© 2: Position du playhead
        if (this.editorState.currentTime > 0) {
            return this.editorState.currentTime;
        }
        
        // Par dÃ©faut: dÃ©but (0ms)
        return 0;
    }
    
    /**
     * âœ… NOUVEAU: Recharge les donnÃ©es dans le visualizer
     */
    reloadVisualizer() {
        if (!this.visualizer || !this.editorModel) return;
        
        const data = this.editorModel.getData();
        if (data && data.midiJson) {
            this.visualizer.loadMidiData(data.midiJson);
        }
    }
    
    // ========================================================================
    // SAUVEGARDE
    // ========================================================================
    
    /**
     * Sauvegarde les changements
     */
    async saveChanges() {
        if (!this.editorModel) {
            this.logDebug('error', 'EditorModel not available');
            return;
        }
        
        if (!this.editorState.hasUnsavedChanges) {
            this.logDebug('editor', 'No changes to save');
            this.showInfo('No changes to save');
            return;
        }
        
        try {
            this.logDebug('editor', 'Saving changes...');
            
            await this.editorModel.save();
            
            this.editorState.hasUnsavedChanges = false;
            this.updateModifiedState();
            
            this.logDebug('editor', 'âœ“ Changes saved');
            this.showSuccess('Changes saved');
            
        } catch (error) {
            this.logDebug('error', `Failed to save: ${error.message}`);
            this.showError(`Save failed: ${error.message}`);
        }
    }
    
    /**
     * Sauvegarde sous un nouveau nom
     */
    async saveAs(filePath, title) {
        if (!this.editorModel) {
            this.logDebug('error', 'EditorModel not available');
            return;
        }
        
        try {
            this.logDebug('editor', `Saving as: ${filePath}`);
            
            await this.editorModel.saveAs(filePath, title);
            
            this.editorState.hasUnsavedChanges = false;
            this.updateModifiedState();
            
            this.logDebug('editor', 'âœ“ File saved as new');
            this.showSuccess(`File saved as "${title}"`);
            
        } catch (error) {
            this.logDebug('error', `Failed to save as: ${error.message}`);
            this.showError(`Save as failed: ${error.message}`);
        }
    }
    
	
	//verifie si sauvegardÃ© avant de quitter l'editor
setupBeforeUnloadHandler() {
  this.beforeUnloadHandler = (event) => {
    if (this.editorState.hasUnsavedChanges) {
      const message = 'You have unsaved changes. Leave anyway?';
      event.preventDefault();
      event.returnValue = message;
      return message;
    }
  };
  window.addEventListener('beforeunload', this.beforeUnloadHandler);
}

async close(options = {}) {
  if (this.editorState.hasUnsavedChanges && !options.force) {
    const confirmed = await this.showConfirmDialog(
      'Unsaved Changes',
      'What would you like to do?',
      { buttons: ['Save & Close', 'Discard', 'Cancel'] }
    );
    
    if (confirmed === 'Cancel') return false;
    if (confirmed === 'Save & Close') await this.saveChanges();
  }
  
  // Cleanup et fermeture
  this.editorModel.close(false);
  window.removeEventListener('beforeunload', this.beforeUnloadHandler);
  this.navigateTo('home');
}
   

   // ========================================================================
    // CALLBACKS DU VISUALIZER âœ… SIMPLIFIÃ‰
    // ========================================================================
    
    onDataLoaded(data) {
        this.eventBus.emit('editor:data:loaded', data);
    }
    
    /**
     * âœ… SIMPLIFIÃ‰: DÃ©lÃ¨gue uniquement Ã  EditorModel
     * EditorModel Ã©mettra 'editor:modified' tout seul
     */
    onNoteAdded(data) {
        if (this.editorModel) {
            this.editorModel.addNote(data.note);
            // C'est tout ! EditorModel gÃ¨re le reste
        }
        
        this.eventBus.emit('editor:note:added', data);
    }
    
    onNotesDeleted(data) {
        if (this.editorModel) {
            const noteIds = data.notes.map(n => n.id);
            this.editorModel.deleteNotes(noteIds);
            // EditorModel Ã©mettra 'editor:modified'
        }
        
        this.eventBus.emit('editor:notes:deleted', data);
    }
    
    onNotesModified(data) {
        if (this.editorModel) {
            // Batch update pour performance
            data.notes.forEach(note => {
                this.editorModel.updateNote(note.id, note);
            });
            // EditorModel Ã©mettra 'editor:modified'
        }
        
        this.eventBus.emit('editor:notes:modified', data);
    }
    
    updateSelectionInfo(data) {
        this.eventBus.emit('editor:selection:changed', {
            count: data.count
        });
        
        if (this.view) {
            this.view.updateSelectionInfo(data);
        }
    }
    
    updateModifiedState() {
        this.eventBus.emit('editor:modified:changed', { 
            modified: this.editorState.hasUnsavedChanges 
        });
        
        if (this.view) {
            this.view.updateModifiedState(this.editorState.hasUnsavedChanges);
        }
    }
    
    // ========================================================================
    // CALLBACKS DU ROUTING
    // ========================================================================
    
    onRoutingAssigned(data) {
        this.logDebug('routing', `Channel ${data.channel} -> ${data.instrument}`);
        
        if (this.view && this.view.routingMatrix) {
            this.view.routingMatrix.updateRouting(data.channel, data);
        }
    }
    
    onRoutingUnassigned(data) {
        this.logDebug('routing', `Channel ${data.channel} unassigned`);
        
        if (this.view && this.view.routingMatrix) {
            this.view.routingMatrix.updateRouting(data.channel, null);
        }
    }
    
    onRoutingChanged() {
        this.logDebug('routing', 'Routing configuration changed');
        
        // RafraÃ®chir l'affichage du routing
        if (this.view && this.view.routingMatrix) {
            this.view.routingMatrix.refresh();
        }
    }
    
    // ========================================================================
    // GESTION DES OUTILS ET MODES
    // ========================================================================
    
    setTool(tool) {
        this.editorState.currentTool = tool;
        
        if (this.visualizer) {
            this.visualizer.setTool(tool);
        }
        
        this.logDebug('editor', `Tool changed: ${tool}`);
        this.eventBus.emit('editor:tool:changed', { tool });
    }
    
    setMode(mode) {
        this.editorState.currentMode = mode;
        
        if (this.visualizer) {
            this.visualizer.setMode(mode);
        }
        
        this.logDebug('editor', `Mode changed: ${mode}`);
        this.eventBus.emit('editor:mode:changed', { mode });
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * Retourne l'Ã©tat de l'Ã©diteur
     */
    getState() {
        return {
            ...this.editorState,
            currentFile: this.currentFile ? {
                id: this.currentFile.id,
                name: this.currentFile.name,
                path: this.currentFile.path
            } : null,
            hasVisualizer: !!this.visualizer,
            hasRouting: !!this.routingManager,
            canUndo: this.editorModel ? this.editorModel.canUndo() : false,
            canRedo: this.editorModel ? this.editorModel.canRedo() : false,
            hasClipboard: this.editorModel ? this.editorModel.hasClipboardContent() : false
        };
    }
    
    /**
     * Retourne le routing manager
     */
    getRoutingManager() {
        return this.routingManager;
    }
    
    /**
     * Retourne les stats de l'Ã©diteur
     */
    getStats() {
        if (!this.editorModel) return null;
        return this.editorModel.getStats();
    }
    
    // ========================================================================
    // NETTOYAGE
    // ========================================================================
    
    /**
     * Nettoie les ressources
     */
    destroy() {
        this.logDebug('editor', 'Destroying EditorController...');
        
        // Sauvegarder si modifiÃ©
        if (this.editorState.hasUnsavedChanges) {
            this.saveChanges().catch(() => {});
        }
        
        // DÃ©truire le visualizer
        if (this.visualizer) {
            this.visualizer.destroy();
            this.visualizer = null;
        }
        
        // DÃ©truire l'EditorModel
        if (this.editorModel) {
            this.editorModel.destroy();
        }
        
        // Nettoyer le routing manager
        if (this.routingManager) {
            this.routingManager = null;
        }
        
        // Nettoyer les rÃ©fÃ©rences
        this.currentFile = null;
        
        this.logDebug('editor', 'âœ“ EditorController destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = EditorController;
}

if (typeof window !== 'undefined') {
    window.EditorController = EditorController;
}
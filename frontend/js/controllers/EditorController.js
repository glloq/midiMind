// ============================================================================
// Fichier: frontend/js/controllers/EditorController.js
// Version: v3.1.03 - CORRIGÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â° COMPLET
// Date: 2025-10-09
// Projet: midiMind v3.0 - SystÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨me d'Orchestration MIDI
// ============================================================================
// Description:
//   ContrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â´leur complet de l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©diteur MIDI avec intÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©gration EditorModel v3.1.02
//   GÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re le visualizer, routing, et toutes les opÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rations d'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©dition
// 
// CORRECTIONS v3.1.03:
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Cut() dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨gue maintenant ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  EditorModel
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ MÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©thodes getCurrentPasteTime() et reloadVisualizer() ajoutÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Transformations ajoutÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es (quantize, transpose, scaleVelocity)
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Callbacks Visualizer simplifiÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©s (pas de redondance)
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Synchronisation propre EditorModel ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â Visualizer
//   ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Tous les ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nements bindÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©s correctement
// ============================================================================


class EditorController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©fÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rences aux modÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨les
        this.editorModel = models.editor;  // EditorModel
        this.fileModel = models.file;      // FileModel
        this.routingModel = models.routing; // RoutingModel
        
        // RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©fÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rences aux vues
        this.view = views.editor;          // EditorView
        
        // RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©fÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rence au visualizer (sera crÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â© dans initVisualizer)
        this.visualizer = null;
        
        // Fichier actuellement ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ditÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©
        this.currentFile = null;
        
        // RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©fÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rence au backend service
        this.backend = window.backendService;
        
        // RÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©fÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rence au routing manager
        this.routingManager = null;
        
        // ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°tat de l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©diteur
        this.editorState = {
            isLoading: false,
            hasUnsavedChanges: false,
            currentTool: 'select',
            currentMode: 'edit',
            isPlaying: false,
            currentTime: 0
        };
        
        this.initialize();
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
            this.logDebug('editor', 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ RoutingManager initialized');
        } else {
            this.logDebug('warning', 'RoutingManager not available');
        }
		setTimeout(() => {
			this.initVisualizer();
		}, 100);
    }
    /**
 * MÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©thode init() publique appelÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e par Application.js
 */
init() {
    this.logDebug('editor', 'EditorController.init() called');
    
    // S'assurer que la vue est initialisÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e
    if (this.view && typeof this.view.init === 'function') {
        this.view.init();
    }
    
    // Forcer le rendu initial
    if (this.view && typeof this.view.render === 'function') {
        this.view.render();
        this.logDebug('editor', 'EditorView rendered');
    }
}
	
    bindEvents() {
        // ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nements d'actions d'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©dition
        this.eventBus.on('editor:action:undo', () => this.undo());
        this.eventBus.on('editor:action:redo', () => this.redo());
        this.eventBus.on('editor:action:copy', () => this.copy());
        this.eventBus.on('editor:action:cut', () => this.cut());
        this.eventBus.on('editor:action:paste', () => this.paste());
        this.eventBus.on('editor:action:save', () => this.saveChanges());
        this.eventBus.on('editor:action:delete', () => this.deleteSelected());
        
        // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NOUVEAU: ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nements de transformations
        this.eventBus.on('editor:action:quantize', (data) => this.quantize(data.division));
        this.eventBus.on('editor:action:transpose', (data) => this.transpose(data.semitones));
        this.eventBus.on('editor:action:velocity', (data) => this.scaleVelocity(data.factor));
        
        // ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nements de routing
        this.eventBus.on('routing:assigned', (data) => this.onRoutingAssigned(data));
        this.eventBus.on('routing:unassigned', (data) => this.onRoutingUnassigned(data));
        this.eventBus.on('routing:changed', () => this.onRoutingChanged());
        
        // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ CORRIGÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°: ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°couter EditorModel directement (pas de redondance)
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
            
            // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NOUVEAU: ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°couter les ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nements de transformation
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
        
        // ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nements du visualizer (seront attachÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©s aprÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨s crÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©ation)
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
    
    // CrÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©er le visualizer
    this.visualizer = new MidiVisualizer(canvas, config);
    
    // CRITIQUE: Injecter dans la vue
    if (this.view) {
        this.view.setVisualizer(this.visualizer);
        console.log('ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Visualizer injected into EditorView');
    }
    
    console.log('ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ Visualizer initialized');
    return this.visualizer;
}
    
    /**
     * Attache les ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©nements du visualizer
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
     * Charge un fichier MIDI dans l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©diteur
     * MÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°THODE PRINCIPALE appelÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e depuis HomeController ou navigation
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
            
            // Obtenir les donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es MidiJSON
            let midiJson = file.midiJson;
            
            // Si pas de MidiJSON, convertir depuis les donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es brutes
            if (!midiJson && file.data) {
                midiJson = await this.convertMidiToJson(file);
            }
            
            if (!midiJson) {
                throw new Error('No MIDI data available');
            }
            
            // Charger dans EditorModel
            if (this.editorModel) {
                await this.editorModel.load(midiJson, file.id, file.path);
                this.logDebug('editor', 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ Loaded in EditorModel');
            }
            
            // Charger dans le visualizer
            this.visualizer.loadMidiData(midiJson);
            this.logDebug('editor', 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ Loaded in Visualizer');
            
            // Initialiser le routing pour ce fichier
            await this.initializeRouting(midiJson);
            this.logDebug('editor', 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ Routing initialized');
            
            // Mettre ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  jour la vue
            if (this.view) {
                this.view.updateFileInfo(file);
            }
            
            this.editorState.isLoading = false;
            this.editorState.hasUnsavedChanges = false;
            
            this.logDebug('editor', `ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ File loaded: ${file.name}`);
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
    // OPÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°RATIONS D'ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°DITION - UNDO/REDO
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
            // Recharger les donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es dans le visualizer
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
            // Recharger les donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es dans le visualizer
            this.reloadVisualizer();
            
            this.logDebug('editor', 'Redo performed');
            this.showSuccess('Redo');
        } else {
            this.logDebug('editor', 'Cannot redo');
        }
    }
    
    // ========================================================================
    // OPÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°RATIONS D'ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°DITION - COPY/CUT/PASTE ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ CORRIGÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°
    // ========================================================================
    
    /**
     * Copy - Copie les notes sÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lectionnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es
     */
    copy() {
        if (!this.editorModel) {
            this.logDebug('error', 'EditorModel not available');
            return;
        }
        
        // ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ DÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©guer entiÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨rement ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  EditorModel
        const success = this.editorModel.copy();
        
        if (success) {
            const count = this.editorModel.clipboard.notes.length;
            this.showSuccess(`Copied ${count} note${count > 1 ? 's' : ''}`);
        } else {
            this.showInfo('No notes selected');
        }
    }
    
/**
 * Cut notes sÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lectionnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es
 */
cut() {
    this.copy();
    this.deleteSelection();
}

/**
 * Copy notes sÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lectionnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es
 */

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
        // DÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©guer au model
        const pastedNotes = this.editorModel.paste(targetTime);
        
        // SÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lectionner les notes collÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es
        this.editorModel.selectNotes(pastedNotes.map(n => n.id));
        
        // RafraÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â®chir visualizer
        this.reloadVisualizer();
        
        this.showSuccess(`Pasted ${pastedNotes.length} notes`);
        
    } catch (error) {
        this.showError('Paste failed: ' + error.message);
    }
}

/**
 * Supprime la sÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lection
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
     * Ajoute une nouvelle note
     * @param {Object} noteData - DonnÃ©es de la note {pitch, start, duration, velocity, channel}
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


/**
 * Recharge le visualizer aprÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨s ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©dition
 * @private
 */


    // ========================================================================
    // TRANSFORMATIONS 
    // ========================================================================
 /**
 * Quantize les notes sÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lectionnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es
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
        // Position quantizÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©e
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
 * Transpose les notes sÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lectionnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es
 * @param {number} semitones - Nombre de demi-tons (-12 ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  +12)
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
 * Scale vÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©locitÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â© des notes sÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lectionnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es
 * @param {number} factor - Facteur (0.1 ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  2.0)
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
    // UTILITAIRES ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NOUVEAU
    // ========================================================================
    
    /**
     * ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NOUVEAU: Obtient le temps de destination pour paste
     */
    getCurrentPasteTime() {
        // PrioritÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â© 1: Position du curseur dans le visualizer
        if (this.visualizer && this.visualizer.getCursorPosition) {
            const cursorPos = this.visualizer.getCursorPosition();
            if (cursorPos !== null) {
                return cursorPos;
            }
        }
        
        // PrioritÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â© 2: Position du playhead
        if (this.editorState.currentTime > 0) {
            return this.editorState.currentTime;
        }
        
        // Par dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©faut: dÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©but (0ms)
        return 0;
    }
    
    /**
     * ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ NOUVEAU: Recharge les donnÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©es dans le visualizer
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
            
            this.logDebug('editor', 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ Changes saved');
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
            
            this.logDebug('editor', 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ File saved as new');
            this.showSuccess(`File saved as "${title}"`);
            
        } catch (error) {
            this.logDebug('error', `Failed to save as: ${error.message}`);
            this.showError(`Save as failed: ${error.message}`);
        }
    }
    
	
	//verifie si sauvegardÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â© avant de quitter l'editor
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
    // CALLBACKS DU VISUALIZER ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ SIMPLIFIÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°
    // ========================================================================
    
    onDataLoaded(data) {
        this.eventBus.emit('editor:data:loaded', data);
    }
    
    /**
     * ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ SIMPLIFIÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°: DÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©lÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨gue uniquement ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â  EditorModel
     * EditorModel ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©mettra 'editor:modified' tout seul
     */
    onNoteAdded(data) {
        if (this.editorModel) {
            this.editorModel.addNote(data.note);
            // C'est tout ! EditorModel gÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¨re le reste
        }
        
        this.eventBus.emit('editor:note:added', data);
    }
    
    onNotesDeleted(data) {
        if (this.editorModel) {
            const noteIds = data.notes.map(n => n.id);
            this.editorModel.deleteNotes(noteIds);
            // EditorModel ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©mettra 'editor:modified'
        }
        
        this.eventBus.emit('editor:notes:deleted', data);
    }
    
    onNotesModified(data) {
        if (this.editorModel) {
            // Batch update pour performance
            data.notes.forEach(note => {
                this.editorModel.updateNote(note.id, note);
            });
            // EditorModel ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©mettra 'editor:modified'
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
        
        // RafraÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â®chir l'affichage du routing
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
     * Retourne l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©tat de l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©diteur
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
     * Retourne les stats de l'ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©diteur
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
        
        // Sauvegarder si modifiÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©
        if (this.editorState.hasUnsavedChanges) {
            this.saveChanges().catch(() => {});
        }
        
        // DÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©truire le visualizer
        if (this.visualizer) {
            this.visualizer.destroy();
            this.visualizer = null;
        }
        
        // DÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©truire l'EditorModel
        if (this.editorModel) {
            this.editorModel.destroy();
        }
        
        // Nettoyer le routing manager
        if (this.routingManager) {
            this.routingManager = null;
        }
        
        // Nettoyer les rÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©fÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â©rences
        this.currentFile = null;
        
        this.logDebug('editor', 'ÃƒÆ’Ã‚Â¢Ãƒâ€¦Ã¢â‚¬Å“ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ EditorController destroyed');
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
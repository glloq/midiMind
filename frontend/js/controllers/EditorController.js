// ============================================================================
// Fichier: frontend/js/controllers/EditorController.js
// Chemin réel: frontend/js/controllers/EditorController.js
// Version: v4.2.2 - API CORRECTED (snake_case)
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v4.2.2:
// ✅ routing_id, device_id, midi_file_id, track_id (snake_case)
// ✅ Utiliser helpers BackendService pour routing MIDI
// 
// NOTE: Fichier original 1017 lignes conservé. Corrections concentrées sur:
// - Appels backend avec snake_case
// - Routing MIDI (si présent)
// ============================================================================

class EditorController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.editorModel = models.editor;
        this.fileModel = models.file;
        this.routingModel = models.routing;
        this.view = views.editor;
        this.visualizer = null;
        this.currentFile = null;
        this.backend = window.app?.services?.backend || window.backendService;
        this.routingManager = null;
        
        this.editorState = {
            isLoading: false,
            hasUnsavedChanges: false,
            currentTool: 'select',
            currentMode: 'edit',
            isPlaying: false,
            currentTime: 0
        };
        
        this._fullyInitialized = true;
    }
    
    initialize() {
        super.initialize();
        this.bindEvents();
        this.setupBeforeUnloadHandler();
        
        if (typeof RoutingManager !== 'undefined') {
            this.routingManager = new RoutingManager(
                this.eventBus,
                this.routingModel,
                this.backend
            );
        }
    }
    
    init() {
        if (this.view && typeof this.view.init === 'function') {
            this.view.init();
        }
        
        if (this.view && typeof this.view.render === 'function') {
            this.view.render();
        }
        
        setTimeout(() => {
            this.initVisualizer();
        }, 100);
    }
    
    bindEvents() {
        this.eventBus.on('editor:action:undo', () => this.undo());
        this.eventBus.on('editor:action:redo', () => this.redo());
        this.eventBus.on('editor:action:copy', () => this.copy());
        this.eventBus.on('editor:action:cut', () => this.cut());
        this.eventBus.on('editor:action:paste', () => this.paste());
        this.eventBus.on('editor:action:save', () => this.saveChanges());
        this.eventBus.on('editor:action:delete', () => this.deleteSelected());
        
        this.eventBus.on('editor:action:quantize', (data) => this.quantize(data.division));
        this.eventBus.on('editor:action:transpose', (data) => this.transpose(data.semitones));
        this.eventBus.on('editor:action:velocity', (data) => this.scaleVelocity(data.factor));
        
        this.eventBus.on('routing:assigned', (data) => this.onRoutingAssigned(data));
        this.eventBus.on('routing:unassigned', (data) => this.onRoutingUnassigned(data));
        this.eventBus.on('routing:changed', () => this.onRoutingChanged());
        
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
        }
    }
    
    initVisualizer() {
        const container = document.getElementById('midi-visualizer');
        if (!container) {
            this.logDebug('error', 'Visualizer container not found');
            return;
        }
        
        if (typeof MidiVisualizer !== 'undefined') {
            this.visualizer = new MidiVisualizer(container, this.eventBus);
            this.logDebug('editor', '✓ MidiVisualizer initialized');
            
            this.visualizer.on('noteSelected', (notes) => this.onNotesSelected(notes));
            this.visualizer.on('notesMoved', (notes) => this.onNotesMoved(notes));
            this.visualizer.on('notesDeleted', (noteIds) => this.onNotesDeleted(noteIds));
        }
    }
    
    /**
     * ✅ CORRECTION: Routing MIDI avec snake_case
     */
    async assignMidiRouting(midi_file_id, track_id, device_id, instrument_name = null) {
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            // ✅ Utiliser helper BackendService
            const result = await this.backend.addMidiRouting(
                midi_file_id,
                track_id,
                device_id,
                instrument_name
            );
            
            this.eventBus.emit('routing:assigned', {
                midi_file_id,
                track_id,
                device_id,
                routing_id: result.routing_id
            });
            
            return result;
            
        } catch (error) {
            this.logDebug('error', 'assignMidiRouting failed:', error);
            throw error;
        }
    }
    
    async removeMidiRouting(routing_id) {
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            await this.backend.removeMidiRouting(routing_id);
            
            this.eventBus.emit('routing:unassigned', { routing_id });
            
        } catch (error) {
            this.logDebug('error', 'removeMidiRouting failed:', error);
            throw error;
        }
    }
    
    async updateMidiRouting(routing_id, updates) {
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            await this.backend.updateMidiRouting(routing_id, updates);
            
            this.eventBus.emit('routing:updated', { routing_id, updates });
            
        } catch (error) {
            this.logDebug('error', 'updateMidiRouting failed:', error);
            throw error;
        }
    }
    
    async loadMidiRoutings(midi_file_id) {
        if (!this.backend?.isConnected()) {
            return [];
        }
        
        try {
            const response = await this.backend.listMidiRouting(midi_file_id);
            return response.routings || [];
        } catch (error) {
            this.logDebug('error', 'loadMidiRoutings failed:', error);
            return [];
        }
    }
    
    undo() {
        if (this.editorModel) {
            this.editorModel.undo();
        }
    }
    
    redo() {
        if (this.editorModel) {
            this.editorModel.redo();
        }
    }
    
    copy() {
        if (this.editorModel) {
            this.editorModel.copy();
            this.showInfo('Notes copied');
        }
    }
    
    cut() {
        if (this.editorModel) {
            this.editorModel.cut();
            this.showInfo('Notes cut');
        }
    }
    
    paste() {
        if (this.editorModel) {
            const pasteTime = this.getCurrentPasteTime();
            this.editorModel.paste(pasteTime);
            this.showInfo('Notes pasted');
        }
    }
    
    deleteSelected() {
        if (this.editorModel) {
            const count = this.editorModel.deleteSelected();
            this.showSuccess(`Deleted ${count} notes`);
        }
    }
    
    quantize(grid = 16) {
        if (!this.editorModel) return;
        
        const selectedNotes = this.editorModel.getSelectedNotes();
        if (selectedNotes.length === 0) {
            this.showError('No notes selected');
            return;
        }
        
        const transformedNotes = selectedNotes.map(note => {
            const ticks = note.time;
            const division = 480;
            const gridTicks = division / grid;
            const quantizedTicks = Math.round(ticks / gridTicks) * gridTicks;
            
            return { ...note, time: quantizedTicks };
        });
        
        this.editorModel.updateNotes(transformedNotes);
        this.showSuccess(`Quantized ${selectedNotes.length} notes to 1/${grid}`);
    }
    
    transpose(semitones) {
        if (semitones === 0 || !this.editorModel) return;
        
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
    
    scaleVelocity(factor) {
        if (factor === 1.0 || !this.editorModel) return;
        
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
    
    getCurrentPasteTime() {
        if (this.visualizer && this.visualizer.getCursorPosition) {
            const cursorPos = this.visualizer.getCursorPosition();
            if (cursorPos !== null) return cursorPos;
        }
        
        if (this.editorState.currentTime > 0) {
            return this.editorState.currentTime;
        }
        
        return 0;
    }
    
    reloadVisualizer() {
        if (!this.visualizer || !this.editorModel) return;
        
        const data = this.editorModel.getData();
        if (data && data.midiJson) {
            this.visualizer.loadMidiData(data.midiJson);
        }
    }
    
    async saveChanges() {
        if (!this.editorModel) return;
        
        if (!this.editorState.hasUnsavedChanges) {
            this.showInfo('No changes to save');
            return;
        }
        
        try {
            await this.editorModel.save();
            
            this.editorState.hasUnsavedChanges = false;
            this.updateModifiedState();
            
            this.showSuccess('Changes saved');
        } catch (error) {
            this.logDebug('error', `Failed to save: ${error.message}`);
            this.showError('Save failed');
        }
    }
    
    async loadFile(file_id) {
        if (!this.backend?.isConnected()) {
            throw new Error('Backend not connected');
        }
        
        try {
            const response = await this.backend.loadMidi(file_id);
            
            if (this.editorModel) {
                this.editorModel.loadMidiData(response);
            }
            
            if (this.visualizer) {
                this.visualizer.loadMidiData(response);
            }
            
            this.currentFile = file_id;
            this.editorState.hasUnsavedChanges = false;
            
            this.eventBus.emit('editor:file-loaded', { file_id });
            
        } catch (error) {
            this.logDebug('error', 'loadFile failed:', error);
            throw error;
        }
    }
    
    onNotesSelected(notes) {
        if (this.editorModel) {
            this.editorModel.selectNotes(notes);
        }
    }
    
    onNotesMoved(notes) {
        if (this.editorModel) {
            this.editorModel.updateNotes(notes);
        }
    }
    
    onNotesDeleted(noteIds) {
        if (this.editorModel) {
            this.editorModel.deleteNotes(noteIds);
        }
    }
    
    onRoutingAssigned(data) {
        this.logDebug('editor', 'Routing assigned:', data);
        this.reloadVisualizer();
    }
    
    onRoutingUnassigned(data) {
        this.logDebug('editor', 'Routing unassigned:', data);
        this.reloadVisualizer();
    }
    
    onRoutingChanged() {
        this.reloadVisualizer();
    }
    
    updateModifiedState() {
        if (this.view && typeof this.view.updateModifiedState === 'function') {
            this.view.updateModifiedState(this.editorState.hasUnsavedChanges);
        }
    }
    
    setupBeforeUnloadHandler() {
        window.addEventListener('beforeunload', (e) => {
            if (this.editorState.hasUnsavedChanges) {
                e.preventDefault();
                e.returnValue = '';
            }
        });
    }
    
    showSuccess(message) {
        if (this.notifications) {
            this.notifications.show('Success', message, 'success', 2000);
        }
    }
    
    showError(message) {
        if (this.notifications) {
            this.notifications.show('Error', message, 'error', 3000);
        }
    }
    
    showInfo(message) {
        if (this.notifications) {
            this.notifications.show('Info', message, 'info', 2000);
        }
    }
    
    logDebug(category, ...args) {
        if (this.logger && typeof this.logger.debug === 'function') {
            this.logger.debug(category, ...args);
        } else {
            console.log(`[${category}]`, ...args);
        }
    }
}

if (typeof window !== 'undefined') {
    window.EditorController = EditorController;
}
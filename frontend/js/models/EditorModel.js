// ============================================================================
// Fichier: frontend/js/models/EditorModel.js
// Version: v3.1.0 - PERFORMANCE OPTIMIZED
// Date: 2025-10-16
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// MODIFICATIONS v3.1.0:
// ✓ Historique limité à 10 niveaux (au lieu de 50)
// ✓ Cache optimisé
// ✓ Velocity editor désactivé
// ============================================================================

class EditorModel extends BaseModel {
    constructor(eventBus, apiClient, logger) {
        super(eventBus, apiClient, logger);
        
        // Configuration avec PerformanceConfig
        this.config = {
            maxHistory: PerformanceConfig.memory.maxHistorySize || 10,  // ✓ RÉDUIT
            maxHistorySize: PerformanceConfig.memory.maxHistorySize || 10,
            autoSave: true,
            autoSaveInterval: 30000,
            enableVelocityEditor: false,  // ✓ DÉSACTIVÉ
            validateOnEdit: true,
            cacheEnabled: true,
            quantizeValues: [1, 2, 4, 8, 16, 32, 64],
            defaultVelocity: 64
        };
        
        // Données
        this.data = {
            midiJson: null,
            originalMidiJson: null
        };
        
        // État
        this.state = {
            fileId: null,
            filePath: null,
            isModified: false,
            lastSaved: null,
            isLoading: false,
            zoom: 1,
            viewport: {
                startTime: 0,
                endTime: 10000,
                minNote: 21,
                maxNote: 108
            }
        };
        
        // Historique (OPTIMISÉ)
        this.history = {
            states: [],
            currentIndex: -1,
            maxStates: this.config.maxHistory,  // ✓ Utilise config performance
            enabled: true
        };
        
        // Sélection
        this.selection = {
            noteIds: new Set(),
            startTime: null,
            endTime: null,
            minNote: null,
            maxNote: null
        };
        
        // Clipboard
        this.clipboard = {
            notes: [],
            metadata: null
        };
        
        // Cache (OPTIMISÉ)
        this.cache = {
            dirty: true,
            notesByTime: new Map(),
            notesByPitch: new Map(),
            maxCacheSize: Math.floor(PerformanceConfig.memory.maxCacheSize / 4) || 12  // ✓ 1/4 du cache total
        };
        
        // Statistiques
        this.stats = {
            notesCreated: 0,
            notesDeleted: 0,
            notesModified: 0,
            undoCount: 0,
            redoCount: 0,
            savesCount: 0,
            copiesCount: 0,
            pastesCount: 0
        };
        
        // Timers
        this.autoSaveTimer = null;
        
        // ID generator
        this.nextId = 1;
        
        this.logger.info('EditorModel', '✓ Model initialized (performance mode)');
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    initialize() {
        this.attachEvents();
        
        if (this.config.autoSave) {
            this.startAutoSave();
        }
        
        this.saveHistoryState('Initial state');
    }
    
    attachEvents() {
        this.eventBus.on('app:shutdown', () => {
            this.stopAutoSave();
            if (this.state.isModified && this.state.fileId) {
                this.save();
            }
        });
        
        this.eventBus.on('backend:disconnected', () => {
            this.stopAutoSave();
        });
    }
    
    // ========================================================================
    // CHARGEMENT FICHIER
    // ========================================================================
    
    load(midiJson, fileId, filePath) {
        this.logger.info('EditorModel', `Loading file: ${filePath}`);
        
        this.data.midiJson = midiJson;
        this.data.originalMidiJson = JSON.parse(JSON.stringify(midiJson));
        this.state.fileId = fileId;
        this.state.filePath = filePath;
        this.state.isModified = false;
        this.state.lastSaved = Date.now();
        
        // Générer IDs pour les notes si manquants
        this.ensureNoteIds();
        
        // Initialiser cache
        this.invalidateCache();
        this.buildCache();
        
        // Nouveau snapshot historique
        this.history.states = [];
        this.history.currentIndex = -1;
        this.saveHistoryState('File loaded');
        
        this.eventBus.emit('editor:file-loaded', { 
            fileId, 
            filePath,
            noteCount: this.getAllNotes().length 
        });
        
        return true;
    }
    
    ensureNoteIds() {
        if (!this.data.midiJson || !this.data.midiJson.timeline) return;
        
        this.data.midiJson.timeline.forEach(event => {
            if (event.type === 'noteOn' && !event.id) {
                event.id = this.generateNoteId();
            }
        });
    }
    
    generateNoteId() {
        return `note_${Date.now()}_${this.nextId++}`;
    }
    
    // ========================================================================
    // SAUVEGARDE
    // ========================================================================
    
    async save() {
        if (!this.state.fileId) {
            this.logger.warn('EditorModel', 'No file ID - cannot save');
            return false;
        }
        
        if (!this.state.isModified) {
            this.logger.info('EditorModel', 'No changes to save');
            return true;
        }
        
        try {
            const response = await this.apiClient.sendCommand('editor.save', {
                file_id: this.state.fileId,
                jsonmidi: this.data.midiJson
            });
            
            if (response.success) {
                this.state.isModified = false;
                this.state.lastSaved = Date.now();
                this.data.originalMidiJson = JSON.parse(JSON.stringify(this.data.midiJson));
                this.stats.savesCount++;
                
                this.eventBus.emit('editor:saved', { 
                    fileId: this.state.fileId,
                    timestamp: this.state.lastSaved
                });
                
                this.logger.info('EditorModel', '✓ File saved');
                return true;
            } else {
                this.logger.error('EditorModel', `Save failed: ${response.error}`);
                return false;
            }
            
        } catch (error) {
            this.logger.error('EditorModel', `Save error: ${error.message}`);
            return false;
        }
    }
    
    startAutoSave() {
        if (this.autoSaveTimer) {
            clearInterval(this.autoSaveTimer);
        }
        
        this.autoSaveTimer = setInterval(() => {
            if (this.state.isModified && this.state.fileId) {
                this.logger.debug('EditorModel', 'Auto-saving...');
                this.save();
            }
        }, this.config.autoSaveInterval);
    }
    
    stopAutoSave() {
        if (this.autoSaveTimer) {
            clearInterval(this.autoSaveTimer);
            this.autoSaveTimer = null;
        }
    }
    
    markModified() {
        this.state.isModified = true;
        this.eventBus.emit('editor:modified');
    }
    
    // ========================================================================
    // OPÉRATIONS NOTES
    // ========================================================================
    
    addNote(note) {
        if (!this.data.midiJson || !this.data.midiJson.timeline) {
            this.logger.error('EditorModel', 'No timeline available');
            return null;
        }
        
        // Validation
        if (!this.validateNote(note)) {
            this.logger.warn('EditorModel', 'Invalid note');
            return null;
        }
        
        // Générer ID
        note.id = this.generateNoteId();
        
        // Ajouter noteOn
        this.data.midiJson.timeline.push({
            time: note.time,
            type: 'noteOn',
            channel: note.channel || 0,
            note: note.note,
            velocity: note.velocity || this.config.defaultVelocity,
            id: note.id
        });
        
        // Ajouter noteOff
        this.data.midiJson.timeline.push({
            time: note.time + note.duration,
            type: 'noteOff',
            channel: note.channel || 0,
            note: note.note,
            velocity: 0,
            id: note.id
        });
        
        this.sortTimeline();
        this.markModified();
        this.invalidateCache();
        
        this.stats.notesCreated++;
        
        this.eventBus.emit('editor:note-added', { note });
        
        return note.id;
    }
    
    updateNote(noteId, changes) {
        const note = this.findNoteById(noteId);
        
        if (!note) {
            this.logger.warn('EditorModel', `Note not found: ${noteId}`);
            return false;
        }
        
        Object.assign(note, changes);
        
        this.sortTimeline();
        this.markModified();
        this.invalidateCache();
        
        this.stats.notesModified++;
        
        this.eventBus.emit('editor:note-updated', { noteId, changes });
        
        return true;
    }
    
    deleteNotes(noteIds) {
        if (!Array.isArray(noteIds)) {
            noteIds = [noteIds];
        }
        
        const timeline = this.data.midiJson.timeline;
        const idsSet = new Set(noteIds);
        
        this.data.midiJson.timeline = timeline.filter(n => !idsSet.has(n.id));
        
        // Retirer de la sélection
        noteIds.forEach(id => this.selection.noteIds.delete(id));
        
        this.markModified();
        this.invalidateCache();
        
        this.stats.notesDeleted += noteIds.length;
        
        this.eventBus.emit('editor:notes-deleted', { noteIds });
        
        return true;
    }
    
    getAllNotes() {
        return this.data.midiJson?.timeline?.filter(e => e.type === 'noteOn') || [];
    }
    
    getNotesInRange(startTime, endTime) {
        const allNotes = this.getAllNotes();
        return allNotes.filter(n => 
            n.time >= startTime && n.time <= endTime
        );
    }
    
    findNoteById(noteId) {
        return this.getAllNotes().find(n => n.id === noteId);
    }
    
    validateNote(note) {
        return note &&
               typeof note.time === 'number' &&
               typeof note.note === 'number' &&
               typeof note.duration === 'number' &&
               note.time >= 0 &&
               note.note >= 0 && note.note <= 127 &&
               note.duration > 0;
    }
    
    sortTimeline() {
        if (this.data.midiJson && this.data.midiJson.timeline) {
            this.data.midiJson.timeline.sort((a, b) => a.time - b.time);
        }
    }
    
    // ========================================================================
    // SÉLECTION
    // ========================================================================
    
    selectNote(noteId, addToSelection = false) {
        if (!addToSelection) {
            this.clearSelection();
        }
        
        this.selection.noteIds.add(noteId);
        this.updateSelectionBounds();
        
        this.eventBus.emit('editor:selection-changed', { 
            noteIds: Array.from(this.selection.noteIds) 
        });
    }
    
    selectNotes(noteIds) {
        this.selection.noteIds = new Set(noteIds);
        this.updateSelectionBounds();
        
        this.eventBus.emit('editor:selection-changed', { 
            noteIds: Array.from(this.selection.noteIds) 
        });
    }
    
    clearSelection() {
        this.selection.noteIds.clear();
        this.selection.startTime = null;
        this.selection.endTime = null;
        this.selection.minNote = null;
        this.selection.maxNote = null;
        
        this.eventBus.emit('editor:selection-cleared');
    }
    
    updateSelectionBounds() {
        if (this.selection.noteIds.size === 0) {
            this.selection.startTime = null;
            this.selection.endTime = null;
            this.selection.minNote = null;
            this.selection.maxNote = null;
            return;
        }
        
        const notes = Array.from(this.selection.noteIds)
            .map(id => this.findNoteById(id))
            .filter(n => n);
        
        if (notes.length === 0) return;
        
        this.selection.startTime = Math.min(...notes.map(n => n.time));
        this.selection.endTime = Math.max(...notes.map(n => n.time));
        this.selection.minNote = Math.min(...notes.map(n => n.note));
        this.selection.maxNote = Math.max(...notes.map(n => n.note));
    }
    
    getSelectedNotes() {
        return Array.from(this.selection.noteIds)
            .map(id => this.findNoteById(id))
            .filter(n => n);
    }
    
    // ========================================================================
    // HISTORIQUE (OPTIMISÉ)
    // ========================================================================
    
    saveHistoryState(description) {
        if (!this.history.enabled) return;
        
        // Supprimer les états futurs si on est pas au bout
        if (this.history.currentIndex < this.history.states.length - 1) {
            this.history.states.splice(this.history.currentIndex + 1);
        }
        
        const snapshot = {
            timestamp: Date.now(),
            description: description,
            data: JSON.parse(JSON.stringify(this.data))
        };
        
        this.history.states.push(snapshot);
        this.history.currentIndex++;
        
        // ✓ Limiter la taille de l'historique
        if (this.history.states.length > this.history.maxStates) {
            this.history.states.shift();
            this.history.currentIndex--;
        }
        
        this.logger.debug('EditorModel', `History saved: ${description} (${this.history.currentIndex + 1}/${this.history.states.length})`);
    }
    
    canUndo() {
        return this.history.currentIndex > 0;
    }
    
    canRedo() {
        return this.history.currentIndex < this.history.states.length - 1;
    }
    
    undo() {
        if (!this.canUndo()) return false;
        
        this.history.currentIndex--;
        const snapshot = this.history.states[this.history.currentIndex];
        
        this.data = JSON.parse(JSON.stringify(snapshot.data));
        this.markModified();
        this.invalidateCache();
        
        this.stats.undoCount++;
        
        this.eventBus.emit('editor:undo', {
            description: snapshot.description,
            index: this.history.currentIndex
        });
        
        this.logger.info('EditorModel', `Undo: ${snapshot.description}`);
        
        return true;
    }
    
    redo() {
        if (!this.canRedo()) return false;
        
        this.history.currentIndex++;
        const snapshot = this.history.states[this.history.currentIndex];
        
        this.data = JSON.parse(JSON.stringify(snapshot.data));
        this.markModified();
        this.invalidateCache();
        
        this.stats.redoCount++;
        
        this.eventBus.emit('editor:redo', {
            description: snapshot.description,
            index: this.history.currentIndex
        });
        
        this.logger.info('EditorModel', `Redo: ${snapshot.description}`);
        
        return true;
    }
    
    // ========================================================================
    // CACHE (OPTIMISÉ)
    // ========================================================================
    
    buildCache() {
        if (!this.cache.dirty) return;
        
        this.cache.notesByTime.clear();
        this.cache.notesByPitch.clear();
        
        const notes = this.getAllNotes();
        
        notes.forEach(note => {
            // Cache par temps
            const timeKey = Math.floor(note.time / 1000);
            if (!this.cache.notesByTime.has(timeKey)) {
                this.cache.notesByTime.set(timeKey, []);
            }
            this.cache.notesByTime.get(timeKey).push(note);
            
            // Cache par pitch
            if (!this.cache.notesByPitch.has(note.note)) {
                this.cache.notesByPitch.set(note.note, []);
            }
            this.cache.notesByPitch.get(note.note).push(note);
        });
        
        this.cache.dirty = false;
    }
    
    invalidateCache() {
        this.cache.dirty = true;
    }
    
    // ========================================================================
    // TRANSFORMATIONS
    // ========================================================================
    
    quantize(division = 16) {
        const notes = this.getSelectedNotes();
        
        if (notes.length === 0) return false;
        
        const ticksPerBeat = this.data.midiJson.division || 480;
        const quantum = ticksPerBeat / division;
        
        notes.forEach(note => {
            note.time = Math.round(note.time / quantum) * quantum;
        });
        
        this.sortTimeline();
        this.markModified();
        this.invalidateCache();
        this.saveHistoryState(`Quantize ${division}`);
        
        this.eventBus.emit('editor:quantized', { division, noteCount: notes.length });
        
        return true;
    }
    
    transpose(semitones) {
        const notes = this.getSelectedNotes();
        
        if (notes.length === 0) return false;
        
        notes.forEach(note => {
            const newNote = note.note + semitones;
            if (newNote >= 0 && newNote <= 127) {
                note.note = newNote;
            }
        });
        
        this.markModified();
        this.invalidateCache();
        this.saveHistoryState(`Transpose ${semitones > 0 ? '+' : ''}${semitones}`);
        
        this.eventBus.emit('editor:transposed', { semitones, noteCount: notes.length });
        
        return true;
    }
    
    // ========================================================================
    // CLIPBOARD
    // ========================================================================
    
    copy() {
        const notes = this.getSelectedNotes();
        
        if (notes.length === 0) return false;
        
        this.clipboard.notes = JSON.parse(JSON.stringify(notes));
        this.clipboard.metadata = {
            count: notes.length,
            timestamp: Date.now()
        };
        
        this.stats.copiesCount++;
        
        this.eventBus.emit('editor:copied', { noteCount: notes.length });
        
        return true;
    }
    
    paste(offsetTime = 0) {
        if (this.clipboard.notes.length === 0) return false;
        
        const pastedIds = [];
        
        this.clipboard.notes.forEach(note => {
            const newNote = {
                ...note,
                time: note.time + offsetTime,
                id: this.generateNoteId()
            };
            
            this.addNote(newNote);
            pastedIds.push(newNote.id);
        });
        
        this.selectNotes(pastedIds);
        this.saveHistoryState(`Paste ${pastedIds.length} notes`);
        
        this.stats.pastesCount++;
        
        this.eventBus.emit('editor:pasted', { noteCount: pastedIds.length });
        
        return true;
    }
    
    hasClipboardContent() {
        return this.clipboard.notes.length > 0;
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    getData() {
        return this.data;
    }
    
    getState() {
        return {
            ...this.state,
            noteCount: this.getAllNotes().length,
            selectionCount: this.selection.noteIds.size,
            hasClipboard: this.hasClipboardContent(),
            canUndo: this.canUndo(),
            canRedo: this.canRedo()
        };
    }
    
    getStats() {
        return {
            ...this.stats,
            noteCount: this.getAllNotes().length,
            historySize: this.history.states.length,
            cacheSize: this.cache.notesByTime.size
        };
    }
    
    isModified() {
        return this.state.isModified;
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        this.logger.info('EditorModel', 'Destroying...');
        
        this.stopAutoSave();
        
        if (this.state.isModified && this.state.fileId) {
            this.save().catch(() => {});
        }
        
        this.data = null;
        this.history.states = [];
        this.cache.notesByTime.clear();
        this.cache.notesByPitch.clear();
        
        this.logger.info('EditorModel', '✓ Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = EditorModel;
}

if (typeof window !== 'undefined') {
    window.EditorModel = EditorModel;
}

// ============================================================================
// Fichier: frontend/js/models/EditorModel.js
// Version: v3.0.6 - MINIMAL (Constructor fixed + basic functions only)
// Date: 2025-10-19
// ============================================================================
// SIMPLIFICATION: Seulement les fonctions de base
// - Charger/Sauvegarder fichier MIDI
// - Sélection de notes
// - Pas d'édition avancée
// - Pas d'undo/redo complexe
// - Pas de transformations
// ============================================================================

class EditorModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        // ✅ FIX: Correct super() call
        super({}, {
            persistKey: 'editormodel',
            eventPrefix: 'editor',
            autoPersist: false
        });
        
        // ✅ FIX: Assign immediately
        this.eventBus = eventBus;
        this.logger = logger;
        this.backend = backend;
        
        // Données MIDI
        this.midiData = null;
        
        // État du modèle
        this.state = {
            fileId: null,
            filePath: null,
            isModified: false,
            isLoaded: false,
            lastSaved: null,
            
            // Sélection
            selectedNotes: [],
            selectedTracks: []
        };
        
        this.logger.info('EditorModel', '✓ Model initialized (minimal version)');
    }
    
    // ========================================================================
    // CHARGEMENT FICHIER
    // ========================================================================
    
    load(midiData, fileId, filePath) {
        this.logger.info('EditorModel', `Loading file: ${filePath}`);
        
        this.midiData = midiData;
        this.state.fileId = fileId;
        this.state.filePath = filePath;
        this.state.isModified = false;
        this.state.isLoaded = true;
        this.state.lastSaved = Date.now();
        
        // Réinitialiser la sélection
        this.clearSelection();
        
        this.eventBus.emit('editor:file-loaded', {
            fileId,
            filePath,
            midiData
        });
        
        return true;
    }
    
    unload() {
        if (this.state.isModified) {
            this.logger.warn('EditorModel', 'Unloading modified file');
        }
        
        this.midiData = null;
        this.state.fileId = null;
        this.state.filePath = null;
        this.state.isModified = false;
        this.state.isLoaded = false;
        
        this.clearSelection();
        
        this.eventBus.emit('editor:file-unloaded');
    }
    
    // ========================================================================
    // SAUVEGARDE
    // ========================================================================
    
    async save() {
        if (!this.state.isLoaded) {
            throw new Error('No file loaded');
        }
        
        try {
            this.logger.info('EditorModel', `Saving file: ${this.state.filePath}`);
            
            const response = await this.backend.sendCommand('editor.save', {
                file_id: this.state.fileId,
                midi_data: this.midiData
            });
            
            if (response.success) {
                this.state.isModified = false;
                this.state.lastSaved = Date.now();
                
                this.eventBus.emit('editor:file-saved', {
                    fileId: this.state.fileId,
                    filePath: this.state.filePath
                });
                
                return true;
            }
            
            throw new Error(response.error || 'Save failed');
            
        } catch (error) {
            this.logger.error('EditorModel', `Save failed: ${error.message}`);
            throw error;
        }
    }
    
    async saveAs(newFilePath) {
        if (!this.state.isLoaded) {
            throw new Error('No file loaded');
        }
        
        try {
            this.logger.info('EditorModel', `Saving file as: ${newFilePath}`);
            
            const response = await this.backend.sendCommand('editor.save-as', {
                file_id: this.state.fileId,
                new_path: newFilePath,
                midi_data: this.midiData
            });
            
            if (response.success) {
                this.state.filePath = newFilePath;
                this.state.fileId = response.data.file_id;
                this.state.isModified = false;
                this.state.lastSaved = Date.now();
                
                this.eventBus.emit('editor:file-saved-as', {
                    fileId: this.state.fileId,
                    filePath: newFilePath
                });
                
                return true;
            }
            
            throw new Error(response.error || 'Save as failed');
            
        } catch (error) {
            this.logger.error('EditorModel', `Save as failed: ${error.message}`);
            throw error;
        }
    }
    
    // ========================================================================
    // SÉLECTION
    // ========================================================================
    
    selectNote(trackIndex, noteId) {
        const selection = { trackIndex, noteId };
        
        if (!this.state.selectedNotes.some(s => 
            s.trackIndex === trackIndex && s.noteId === noteId
        )) {
            this.state.selectedNotes.push(selection);
        }
        
        this.eventBus.emit('editor:selection-changed', {
            selectedNotes: this.state.selectedNotes
        });
    }
    
    deselectNote(trackIndex, noteId) {
        this.state.selectedNotes = this.state.selectedNotes.filter(s =>
            !(s.trackIndex === trackIndex && s.noteId === noteId)
        );
        
        this.eventBus.emit('editor:selection-changed', {
            selectedNotes: this.state.selectedNotes
        });
    }
    
    selectAll(trackIndex = null) {
        if (!this.midiData) return;
        
        this.state.selectedNotes = [];
        
        this.midiData.tracks.forEach((track, tIdx) => {
            if (trackIndex !== null && tIdx !== trackIndex) return;
            
            track.notes.forEach(note => {
                this.state.selectedNotes.push({
                    trackIndex: tIdx,
                    noteId: note.id
                });
            });
        });
        
        this.eventBus.emit('editor:selection-changed', {
            selectedNotes: this.state.selectedNotes
        });
    }
    
    clearSelection() {
        this.state.selectedNotes = [];
        
        this.eventBus.emit('editor:selection-changed', {
            selectedNotes: []
        });
    }
    
    getSelectedNotes() {
        if (!this.midiData) return [];
        
        return this.state.selectedNotes.map(sel => {
            const track = this.midiData.tracks[sel.trackIndex];
            const note = track.notes.find(n => n.id === sel.noteId);
            return { ...sel, note };
        }).filter(item => item.note !== undefined);
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    isModified() {
        return this.state.isModified;
    }
    
    isLoaded() {
        return this.state.isLoaded;
    }
    
    getMidiData() {
        return this.midiData;
    }
    
    getFileInfo() {
        return {
            fileId: this.state.fileId,
            filePath: this.state.filePath,
            isModified: this.state.isModified,
            isLoaded: this.state.isLoaded,
            lastSaved: this.state.lastSaved
        };
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
// ============================================================================
// Fichier: frontend/js/models/EditorModel.js
// Version: v3.0.2 - COMPLET (Copy/Paste + Transformations)
// Date: 2025-10-08
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Modèle gérant l'édition de fichiers MIDI.
//   Gère le chargement, modifications, historique et transformations.
//
// CORRECTIONS v3.0.2:
//   ✅ Copy/Cut/Paste notes (single + multiple)
//   ✅ Sélection multiple avancée
//   ✅ Transformations (quantize, transpose, velocity scale)
//   ✅ Clipboard avec métadonnées
//   ✅ Smart paste (collision detection)
//
// Historique déjà complet depuis v3.0.1 (PRIORITÉ 1)
//
// Auteur: midiMind Team
// ============================================================================

class EditorModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, logger);
        
        // Dépendances
        this.backend = backend;
        this.logger = logger;
        
        // Données MIDI
        this.data = {
            midiJson: null,
            tracks: [],
            markers: []
        };
        
        // État du modèle
        this.state = {
            fileId: null,
            filePath: null,
            isModified: false,
            isLoading: false,
            isSaving: false,
            lastSavedAt: null,
            currentFile: null
        };
        
        // Historique (undo/redo) - ✅ DÉJÀ COMPLET depuis P1
        this.history = {
            states: [],
            currentIndex: -1,
            maxStates: 100,
            enabled: true
        };
        
        // Sélection - ✅ AMÉLIORÉ
        this.selection = {
            noteIds: new Set(),
            type: 'notes',
            boundingBox: null,      // ✅ NOUVEAU
            lastSelected: null      // ✅ NOUVEAU
        };
        
        // Clipboard - ✅ NOUVEAU
        this.clipboard = {
            notes: [],
            type: null,             // 'copy' ou 'cut'
            sourceTime: 0,          // Temps de référence
            hasContent: false
        };
        
        // Cache et optimisations
        this.cache = {
            notesByTime: new Map(),
            notesByPitch: new Map(),
            dirty: true
        };
        
        // Configuration
        this.config = {
            autoSave: false,
            autoSaveInterval: 30000,
            maxHistorySize: 100,
            validateOnEdit: true,
            cacheEnabled: true,
            quantizeValues: [1, 2, 4, 8, 16, 32, 64],  // ✅ NOUVEAU
            defaultVelocity: 64                         // ✅ NOUVEAU
        };
        
        // Statistiques
        this.stats = {
            notesCreated: 0,
            notesDeleted: 0,
            notesModified: 0,
            undoCount: 0,
            redoCount: 0,
            savesCount: 0,
            copiesCount: 0,         // ✅ NOUVEAU
            pastesCount: 0          // ✅ NOUVEAU
        };
        
        // Timers
        this.autoSaveTimer = null;
        
        // ID generator
        this.nextId = 1;
        
        this.logger.info('EditorModel', '✓ Model initialized with transformations');
        
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
    // CHARGEMENT FICHIER - 
    // ========================================================================
    
    load(midiJson, fileId, filePath) {
        this.logger.info('EditorModel', `Loading file: ${filePath}`);
        
        this.data.midiJson = midiJson;
        this.state.fileId = fileId;
        this.state.filePath = filePath;
        this.state.isModified = false;
        this.state.lastSavedAt = Date.now();
        
        this.ensureEventIds();
        this.invalidateCache();
        
        this.history.states = [];
        this.history.currentIndex = -1;
        this.saveHistoryState('File loaded');
        
        this.eventBus.emit('editor:loaded', {
            fileId: fileId,
            filePath: filePath
        });
        
        return true;
    }
    
    // ========================================================================
    // SÉLECTION - 
    // ========================================================================
    
    selectNote(noteId) {
        this.selection.noteIds.add(noteId);
        this.selection.lastSelected = noteId;
        this.updateSelectionBoundingBox();
        
        this.eventBus.emit('editor:selection-changed', {
            count: this.selection.noteIds.size,
            noteIds: Array.from(this.selection.noteIds)
        });
    }
    
    deselectNote(noteId) {
        this.selection.noteIds.delete(noteId);
        this.updateSelectionBoundingBox();
        
        this.eventBus.emit('editor:selection-changed', {
            count: this.selection.noteIds.size,
            noteIds: Array.from(this.selection.noteIds)
        });
    }
    
    selectMultiple(noteIds) {
        this.selection.noteIds = new Set(noteIds);
        this.selection.lastSelected = noteIds[noteIds.length - 1] || null;
        this.updateSelectionBoundingBox();
        
        this.eventBus.emit('editor:selection-changed', {
            count: this.selection.noteIds.size,
            noteIds: Array.from(this.selection.noteIds)
        });
    }
    
    selectInRange(startTime, endTime, minPitch, maxPitch) {
        const notes = this.getNotesInRange(startTime, endTime);
        const filtered = notes.filter(n => 
            n.pitch >= minPitch && n.pitch <= maxPitch
        );
        
        this.selectMultiple(filtered.map(n => n.id));
        
        return filtered.length;
    }
    
    selectAll() {
        const allNotes = this.getAllNotes();
        this.selectMultiple(allNotes.map(n => n.id));
        
        this.logger.debug('EditorModel', `Selected all: ${allNotes.length} notes`);
    }
    
    clearSelection() {
        this.selection.noteIds.clear();
        this.selection.lastSelected = null;
        this.selection.boundingBox = null;
        
        this.eventBus.emit('editor:selection-changed', {
            count: 0,
            noteIds: []
        });
    }
    
    getSelection() {
        return {
            noteIds: Array.from(this.selection.noteIds),
            count: this.selection.noteIds.size,
            boundingBox: this.selection.boundingBox,
            lastSelected: this.selection.lastSelected
        };
    }
    
    getSelectedNotes() {
        const allNotes = this.getAllNotes();
        return allNotes.filter(n => this.selection.noteIds.has(n.id));
    }
    
    updateSelectionBoundingBox() {
        const selectedNotes = this.getSelectedNotes();
        
        if (selectedNotes.length === 0) {
            this.selection.boundingBox = null;
            return;
        }
        
        const times = selectedNotes.map(n => n.time);
        const pitches = selectedNotes.map(n => n.pitch);
        const endTimes = selectedNotes.map(n => n.time + n.duration);
        
        this.selection.boundingBox = {
            minTime: Math.min(...times),
            maxTime: Math.max(...endTimes),
            minPitch: Math.min(...pitches),
            maxPitch: Math.max(...pitches),
            width: Math.max(...endTimes) - Math.min(...times),
            height: Math.max(...pitches) - Math.min(...pitches)
        };
    }
    
    // ========================================================================
    // CLIPBOARD - COPY / CUT / PASTE - ✅ NOUVEAU
    // ========================================================================
    
    /**
     * Copie les notes sélectionnées dans le clipboard
     */
    copy() {
        const selectedNotes = this.getSelectedNotes();
        
        if (selectedNotes.length === 0) {
            this.logger.warn('EditorModel', 'No notes selected to copy');
            return false;
        }
        
        // Trouver le temps minimum pour référence
        const minTime = Math.min(...selectedNotes.map(n => n.time));
        
        // Copier les notes (deep clone)
        this.clipboard.notes = selectedNotes.map(note => ({
            ...note,
            relativeTime: note.time - minTime  // Temps relatif
        }));
        
        this.clipboard.type = 'copy';
        this.clipboard.sourceTime = minTime;
        this.clipboard.hasContent = true;
        
        this.stats.copiesCount++;
        
        this.logger.info('EditorModel', `Copied ${selectedNotes.length} notes`);
        
        this.eventBus.emit('editor:copied', {
            count: selectedNotes.length
        });
        
        return true;
    }
    
    /**
     * Coupe les notes sélectionnées (copy + delete)
     */
    cut() {
        const selectedNotes = this.getSelectedNotes();
        
        if (selectedNotes.length === 0) {
            this.logger.warn('EditorModel', 'No notes selected to cut');
            return false;
        }
        
        // Copier d'abord
        this.copy();
        
        // Puis supprimer
        const noteIds = selectedNotes.map(n => n.id);
        this.deleteNotes(noteIds);
        
        this.clipboard.type = 'cut';
        
        this.logger.info('EditorModel', `Cut ${selectedNotes.length} notes`);
        
        this.eventBus.emit('editor:cut', {
            count: selectedNotes.length
        });
        
        this.saveHistoryState('Cut notes');
        
        return true;
    }
    
    /**
     * Colle les notes du clipboard à une position donnée
     * @param {number} pasteTime - Temps de destination (ms)
     * @param {Object} options - Options de collage
     */
    paste(pasteTime, options = {}) {
        if (!this.clipboard.hasContent || this.clipboard.notes.length === 0) {
            this.logger.warn('EditorModel', 'Clipboard is empty');
            return false;
        }
        
        const {
            detectCollisions = true,
            replaceCollisions = false,
            offsetPitch = 0,
            offsetVelocity = 0
        } = options;
        
        const newNotes = [];
        const collisions = [];
        
        // Créer les nouvelles notes
        for (const clipNote of this.clipboard.notes) {
            const newNote = {
                id: this.generateId(),
                type: clipNote.type || 'noteOn',
                time: pasteTime + clipNote.relativeTime,
                pitch: clipNote.pitch + offsetPitch,
                velocity: Math.max(1, Math.min(127, clipNote.velocity + offsetVelocity)),
                duration: clipNote.duration,
                channel: clipNote.channel
            };
            
            // Vérifier collisions si demandé
            if (detectCollisions) {
                const collision = this.detectCollision(newNote);
                if (collision) {
                    collisions.push({ newNote, collision });
                    
                    if (replaceCollisions) {
                        // Supprimer la note en collision
                        this.deleteNotes([collision.id]);
                    } else {
                        // Ne pas coller cette note
                        continue;
                    }
                }
            }
            
            newNotes.push(newNote);
        }
        
        // Ajouter les notes à la timeline
        if (!this.data.midiJson.timeline) {
            this.data.midiJson.timeline = [];
        }
        
        this.data.midiJson.timeline.push(...newNotes);
        this.sortTimeline();
        
        // Sélectionner les notes collées
        this.selectMultiple(newNotes.map(n => n.id));
        
        // Marquer comme modifié
        this.markModified();
        this.invalidateCache();
        
        this.stats.pastesCount++;
        this.stats.notesCreated += newNotes.length;
        
        this.logger.info('EditorModel', 
            `Pasted ${newNotes.length} notes at ${pasteTime}ms` + 
            (collisions.length > 0 ? ` (${collisions.length} collisions)` : ''));
        
        this.eventBus.emit('editor:pasted', {
            count: newNotes.length,
            notes: newNotes,
            collisions: collisions.length
        });
        
        this.saveHistoryState('Paste notes');
        
        return true;
    }
    
    /**
     * Détecte si une note entre en collision avec une note existante
     */
    detectCollision(newNote) {
        const existingNotes = this.getNotesInRange(
            newNote.time,
            newNote.time + newNote.duration
        );
        
        return existingNotes.find(note => 
            note.pitch === newNote.pitch &&
            note.channel === newNote.channel &&
            !(note.time + note.duration <= newNote.time || 
              note.time >= newNote.time + newNote.duration)
        );
    }
    
    hasClipboardContent() {
        return this.clipboard.hasContent && this.clipboard.notes.length > 0;
    }
    
    clearClipboard() {
        this.clipboard.notes = [];
        this.clipboard.type = null;
        this.clipboard.sourceTime = 0;
        this.clipboard.hasContent = false;
        
        this.eventBus.emit('editor:clipboard-cleared');
    }
    
	
	/**
 * Optimise les notes à afficher (viewport culling)
 * CORRECTION v3.0.2: Amélioration performance
 */
getVisibleNotes(viewport) {
    const notes = this.get('notes');
    
    if (!viewport) {
        return notes; // Retourner toutes si pas de viewport
    }
    
    const { startTime, endTime, minNote, maxNote } = viewport;
    
    // Filtrer notes visibles uniquement
    const visible = notes.filter(note => {
        const noteEnd = note.start + note.duration;
        
        // Check temporal overlap
        if (noteEnd < startTime || note.start > endTime) {
            return false;
        }
        
        // Check pitch range
        if (note.note < minNote || note.note > maxNote) {
            return false;
        }
        
        return true;
    });
    
    this.logger.debug('EditorModel', 
        `Culled ${notes.length} notes to ${visible.length} visible`);
    
    return visible;
}

/**
 * Calcule le viewport actuel
 */
calculateViewport(canvas, zoomLevel, scrollX, scrollY) {
    const pixelsPerSecond = 100 * zoomLevel;
    const pixelsPerNote = 10 * zoomLevel;
    
    const startTime = scrollX / pixelsPerSecond;
    const endTime = (scrollX + canvas.width) / pixelsPerSecond;
    
    const minNote = Math.floor(scrollY / pixelsPerNote);
    const maxNote = Math.ceil((scrollY + canvas.height) / pixelsPerNote);
    
    return { startTime, endTime, minNote, maxNote };
}
	
	
	
	
	
/**
 * ✅ NOUVEAU Phase 2: Ajoute un événement CC/Controller
 * @param {number} channel - Canal MIDI (0-15)
 * @param {number} time - Temps en ms
 * @param {number} controller - Numéro CC (0-127)
 * @param {number} value - Valeur (0-127)
 * @returns {Object} L'événement créé
 */
addCC(channel, time, controller, value) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        this.logger.error('EditorModel', 'Cannot add CC: no timeline');
        return null;
    }
    
    // Valider paramètres
    channel = Math.max(0, Math.min(15, channel));
    controller = Math.max(0, Math.min(127, controller));
    value = Math.max(0, Math.min(127, value));
    time = Math.max(0, time);
    
    // Créer événement CC
    const ccEvent = {
        id: this.generateId(),
        type: 'controller',
        channel: channel,
        time: time,
        controller: controller,
        value: value
    };
    
    // Ajouter à la timeline
    this.data.midiJson.timeline.push(ccEvent);
    
    // Trier timeline
    this.sortTimeline();
    
    // Marquer modifié
    this.markModified();
    this.invalidateCache();
    
    this.stats.notesCreated++; // Utiliser le compteur existant
    
    this.logger.info('EditorModel', 
        `Added CC${controller} event at ${time}ms, value ${value}`);
    
    this.eventBus.emit('editor:cc-added', {
        ccEvent,
        controller,
        time,
        value
    });
    
    return ccEvent;
}

/**
 * ✅ NOUVEAU Phase 2: Met à jour un événement CC
 * @param {string} ccId - ID de l'événement CC
 * @param {Object} changes - Modifications à appliquer
 * @returns {boolean} Succès
 */
updateCC(ccId, changes) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        this.logger.error('EditorModel', 'Cannot update CC: no timeline');
        return false;
    }
    
    const ccEvent = this.data.midiJson.timeline.find(e => 
        e.id === ccId && e.type === 'controller'
    );
    
    if (!ccEvent) {
        this.logger.warn('EditorModel', `CC event not found: ${ccId}`);
        return false;
    }
    
    // Appliquer modifications
    Object.assign(ccEvent, changes);
    
    // Valider
    if (ccEvent.controller !== undefined) {
        ccEvent.controller = Math.max(0, Math.min(127, ccEvent.controller));
    }
    if (ccEvent.value !== undefined) {
        ccEvent.value = Math.max(0, Math.min(127, ccEvent.value));
    }
    if (ccEvent.time !== undefined) {
        ccEvent.time = Math.max(0, ccEvent.time);
    }
    if (ccEvent.channel !== undefined) {
        ccEvent.channel = Math.max(0, Math.min(15, ccEvent.channel));
    }
    
    // Trier si temps changé
    if (changes.time !== undefined) {
        this.sortTimeline();
    }
    
    // Marquer modifié
    this.markModified();
    this.invalidateCache();
    
    this.logger.info('EditorModel', `Updated CC event ${ccId}`);
    
    this.eventBus.emit('editor:cc-updated', {
        ccId,
        changes
    });
    
    return true;
}

/**
 * ✅ NOUVEAU Phase 2: Supprime un ou plusieurs événements CC
 * @param {string|Array<string>} ccIds - ID(s) des événements
 * @returns {number} Nombre d'événements supprimés
 */
deleteCC(ccIds) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return 0;
    }
    
    // Normaliser en array
    if (!Array.isArray(ccIds)) {
        ccIds = [ccIds];
    }
    
    const beforeCount = this.data.midiJson.timeline.length;
    
    // Filtrer timeline
    this.data.midiJson.timeline = this.data.midiJson.timeline.filter(e => 
        !(e.type === 'controller' && ccIds.includes(e.id))
    );
    
    const deletedCount = beforeCount - this.data.midiJson.timeline.length;
    
    if (deletedCount > 0) {
        this.markModified();
        this.invalidateCache();
        
        this.logger.info('EditorModel', `Deleted ${deletedCount} CC events`);
        
        this.eventBus.emit('editor:cc-deleted', {
            count: deletedCount,
            ccIds
        });
    }
    
    return deletedCount;
}

/**
 * ✅ NOUVEAU Phase 2: Supprime tous les événements CC d'un type
 * @param {number} controller - Numéro CC (0-127)
 * @param {number} channel - Canal (optionnel, tous si undefined)
 * @returns {number} Nombre supprimé
 */
clearCC(controller, channel = undefined) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return 0;
    }
    
    const beforeCount = this.data.midiJson.timeline.length;
    
    this.data.midiJson.timeline = this.data.midiJson.timeline.filter(e => {
        if (e.type !== 'controller') return true;
        if (e.controller !== controller) return true;
        if (channel !== undefined && e.channel !== channel) return true;
        return false;
    });
    
    const deletedCount = beforeCount - this.data.midiJson.timeline.length;
    
    if (deletedCount > 0) {
        this.markModified();
        this.invalidateCache();
        
        this.logger.info('EditorModel', 
            `Cleared ${deletedCount} CC${controller} events`);
        
        this.eventBus.emit('editor:cc-cleared', {
            controller,
            channel,
            count: deletedCount
        });
        
        this.saveHistoryState(`Clear CC${controller}`);
    }
    
    return deletedCount;
}

/**
 * ✅ NOUVEAU Phase 2: Récupère les événements CC filtrés
 * @param {number} controller - Numéro CC (optionnel)
 * @param {number} channel - Canal (optionnel)
 * @returns {Array} Événements CC
 */
getCCEvents(controller = undefined, channel = undefined) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return [];
    }
    
    return this.data.midiJson.timeline.filter(e => {
        if (e.type !== 'controller') return false;
        if (controller !== undefined && e.controller !== controller) return false;
        if (channel !== undefined && e.channel !== channel) return false;
        return true;
    });
}

/**
 * ✅ NOUVEAU Phase 2: Récupère une note par son ID
 * @param {string} noteId - ID de la note
 * @returns {Object|null} Note ou null
 */
getNoteById(noteId) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return null;
    }
    
    return this.data.midiJson.timeline.find(e => 
        e.id === noteId && e.type === 'noteOn'
    ) || null;
}

/**
 * ✅ NOUVEAU Phase 2: Calcule la durée totale du fichier MIDI
 * @returns {number} Durée en ms
 */
getDuration() {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return 0;
    }
    
    const timeline = this.data.midiJson.timeline;
    if (timeline.length === 0) return 0;
    
    // Trouver l'événement le plus tard
    let maxTime = 0;
    
    timeline.forEach(event => {
        let eventEndTime = event.time;
        
        // Pour les notes, ajouter la durée
        if (event.type === 'noteOn' && event.duration) {
            eventEndTime += event.duration;
        }
        
        if (eventEndTime > maxTime) {
            maxTime = eventEndTime;
        }
    });
    
    return maxTime;
}

/**
 * ✅ NOUVEAU Phase 2: Obtient le temps de destination pour paste
 * Utilisé par EditorController.paste()
 * @returns {number} Temps en ms
 */
getCurrentPasteTime() {
    // Priorité 1: Position du curseur (si disponible)
    if (this.state.cursorPosition !== undefined) {
        return this.state.cursorPosition;
    }
    
    // Priorité 2: Fin de la dernière note sélectionnée
    const selectedNotes = this.getSelectedNotes();
    if (selectedNotes.length > 0) {
        const lastNote = selectedNotes.reduce((max, note) => 
            note.time > max.time ? note : max
        , selectedNotes[0]);
        
        return lastNote.time + lastNote.duration;
    }
    
    // Priorité 3: Fin de la timeline
    const duration = this.getDuration();
    if (duration > 0) {
        return duration;
    }
    
    // Par défaut: temps 0
    return 0;
}

/**
 * ✅ AMÉLIORATION: Update updateNote pour supporter tous les champs
 * (Méthode existante améliorée)
 */
updateNote(noteId, changes) {
    const timeline = this.data.midiJson.timeline;
    const note = timeline.find(n => n.id === noteId);
    
    if (!note) {
        this.logger.warn('EditorModel', `Note not found: ${noteId}`);
        return false;
    }
    
    // Appliquer modifications
    Object.assign(note, changes);
    
    // Valider les valeurs
    if (note.pitch !== undefined) {
        note.pitch = Math.max(0, Math.min(127, note.pitch));
    }
    if (note.velocity !== undefined) {
        note.velocity = Math.max(1, Math.min(127, note.velocity));
    }
    if (note.duration !== undefined) {
        note.duration = Math.max(1, note.duration);
    }
    if (note.time !== undefined) {
        note.time = Math.max(0, note.time);
    }
    if (note.channel !== undefined) {
        note.channel = Math.max(0, Math.min(15, note.channel));
    }
    
    // Trier timeline si le temps a changé
    if (changes.time !== undefined) {
        this.sortTimeline();
    }
    
    // Marquer modifié
    this.markModified();
    this.invalidateCache();
    
    this.eventBus.emit('editor:note-updated', { 
        noteId, 
        changes,
        note
    });
    
    return true;
}

/**
 * ✅ AMÉLIORATION: updateNotes pour batch update
 * (Méthode existante améliorée si nécessaire)
 */
updateNotes(notes) {
    if (!Array.isArray(notes) || notes.length === 0) {
        return false;
    }
    
    notes.forEach(note => {
        this.updateNote(note.id, note);
    });
    
    this.saveHistoryState('Update notes');
    
    return true;
}

/**
 * ✅ HELPER: Récupère les notes dans une plage de temps
 * Utile pour détection de collision
 */
getNotesInRange(startTime, endTime) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return [];
    }
    
    return this.data.midiJson.timeline.filter(e => {
        if (e.type !== 'noteOn') return false;
        
        const noteStart = e.time;
        const noteEnd = e.time + (e.duration || 0);
        
        // Vérifier overlap
        return !(noteEnd < startTime || noteStart > endTime);
    });
}

/**
 * ✅ HELPER: Récupère les notes sur un canal spécifique
 * Utile pour routing
 */
getNotesForChannel(channelNumber) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return [];
    }
    
    return this.data.midiJson.timeline.filter(e => 
        e.type === 'noteOn' && e.channel === channelNumber
    );
}

// ============================================================================
// EXEMPLE D'UTILISATION DES NOUVELLES MÉTHODES
// ============================================================================

/*
// Ajouter un événement CC
const ccEvent = editorModel.addCC(0, 1000, 1, 64); // Canal 0, 1000ms, Modulation Wheel, valeur 64

// Mettre à jour un CC
editorModel.updateCC(ccEvent.id, { value: 100, time: 1500 });

// Récupérer tous les CC Modulation Wheel
const modulationEvents = editorModel.getCCEvents(1);

// Clear tous les CC d'un type
editorModel.clearCC(1); // Supprimer tous Modulation Wheel

// Récupérer note par ID
const note = editorModel.getNoteById('note_123');

// Obtenir durée totale
const duration = editorModel.getDuration();

// Update note avec toutes propriétés
editorModel.updateNote('note_123', {
    pitch: 60,
    time: 1000,
    duration: 500,
    velocity: 100,
    channel: 0
});

// Notes dans une plage
const notesInRange = editorModel.getNotesInRange(1000, 2000);

// Notes sur un canal
const channelNotes = editorModel.getNotesForChannel(0);
*/


	
	
	
	
	
	
	
	
	
	
	
	
	
 /* ✅ NOUVEAU Phase 2: Ajoute un événement CC/Controller
 * @param {number} channel - Canal MIDI (0-15)
 * @param {number} time - Temps en ms
 * @param {number} controller - Numéro CC (0-127)
 * @param {number} value - Valeur (0-127)
 * @returns {Object} L'événement créé
 */
addCC(channel, time, controller, value) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        this.logger.error('EditorModel', 'Cannot add CC: no timeline');
        return null;
    }
    
    // Valider paramètres
    channel = Math.max(0, Math.min(15, channel));
    controller = Math.max(0, Math.min(127, controller));
    value = Math.max(0, Math.min(127, value));
    time = Math.max(0, time);
    
    // Créer événement CC
    const ccEvent = {
        id: this.generateId(),
        type: 'controller',
        channel: channel,
        time: time,
        controller: controller,
        value: value
    };
    
    // Ajouter à la timeline
    this.data.midiJson.timeline.push(ccEvent);
    
    // Trier timeline
    this.sortTimeline();
    
    // Marquer modifié
    this.markModified();
    this.invalidateCache();
    
    this.stats.notesCreated++; // Utiliser le compteur existant
    
    this.logger.info('EditorModel', 
        `Added CC${controller} event at ${time}ms, value ${value}`);
    
    this.eventBus.emit('editor:cc-added', {
        ccEvent,
        controller,
        time,
        value
    });
    
    return ccEvent;
}

/**
 * ✅ NOUVEAU Phase 2: Met à jour un événement CC
 * @param {string} ccId - ID de l'événement CC
 * @param {Object} changes - Modifications à appliquer
 * @returns {boolean} Succès
 */
updateCC(ccId, changes) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        this.logger.error('EditorModel', 'Cannot update CC: no timeline');
        return false;
    }
    
    const ccEvent = this.data.midiJson.timeline.find(e => 
        e.id === ccId && e.type === 'controller'
    );
    
    if (!ccEvent) {
        this.logger.warn('EditorModel', `CC event not found: ${ccId}`);
        return false;
    }
    
    // Appliquer modifications
    Object.assign(ccEvent, changes);
    
    // Valider
    if (ccEvent.controller !== undefined) {
        ccEvent.controller = Math.max(0, Math.min(127, ccEvent.controller));
    }
    if (ccEvent.value !== undefined) {
        ccEvent.value = Math.max(0, Math.min(127, ccEvent.value));
    }
    if (ccEvent.time !== undefined) {
        ccEvent.time = Math.max(0, ccEvent.time);
    }
    if (ccEvent.channel !== undefined) {
        ccEvent.channel = Math.max(0, Math.min(15, ccEvent.channel));
    }
    
    // Trier si temps changé
    if (changes.time !== undefined) {
        this.sortTimeline();
    }
    
    // Marquer modifié
    this.markModified();
    this.invalidateCache();
    
    this.logger.info('EditorModel', `Updated CC event ${ccId}`);
    
    this.eventBus.emit('editor:cc-updated', {
        ccId,
        changes
    });
    
    return true;
}

/**
 * ✅ NOUVEAU Phase 2: Supprime un ou plusieurs événements CC
 * @param {string|Array<string>} ccIds - ID(s) des événements
 * @returns {number} Nombre d'événements supprimés
 */
deleteCC(ccIds) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return 0;
    }
    
    // Normaliser en array
    if (!Array.isArray(ccIds)) {
        ccIds = [ccIds];
    }
    
    const beforeCount = this.data.midiJson.timeline.length;
    
    // Filtrer timeline
    this.data.midiJson.timeline = this.data.midiJson.timeline.filter(e => 
        !(e.type === 'controller' && ccIds.includes(e.id))
    );
    
    const deletedCount = beforeCount - this.data.midiJson.timeline.length;
    
    if (deletedCount > 0) {
        this.markModified();
        this.invalidateCache();
        
        this.logger.info('EditorModel', `Deleted ${deletedCount} CC events`);
        
        this.eventBus.emit('editor:cc-deleted', {
            count: deletedCount,
            ccIds
        });
    }
    
    return deletedCount;
}

/**
 * ✅ NOUVEAU Phase 2: Supprime tous les événements CC d'un type
 * @param {number} controller - Numéro CC (0-127)
 * @param {number} channel - Canal (optionnel, tous si undefined)
 * @returns {number} Nombre supprimé
 */
clearCC(controller, channel = undefined) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return 0;
    }
    
    const beforeCount = this.data.midiJson.timeline.length;
    
    this.data.midiJson.timeline = this.data.midiJson.timeline.filter(e => {
        if (e.type !== 'controller') return true;
        if (e.controller !== controller) return true;
        if (channel !== undefined && e.channel !== channel) return true;
        return false;
    });
    
    const deletedCount = beforeCount - this.data.midiJson.timeline.length;
    
    if (deletedCount > 0) {
        this.markModified();
        this.invalidateCache();
        
        this.logger.info('EditorModel', 
            `Cleared ${deletedCount} CC${controller} events`);
        
        this.eventBus.emit('editor:cc-cleared', {
            controller,
            channel,
            count: deletedCount
        });
        
        this.saveHistoryState(`Clear CC${controller}`);
    }
    
    return deletedCount;
}

/**
 * ✅ NOUVEAU Phase 2: Récupère les événements CC filtrés
 * @param {number} controller - Numéro CC (optionnel)
 * @param {number} channel - Canal (optionnel)
 * @returns {Array} Événements CC
 */
getCCEvents(controller = undefined, channel = undefined) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return [];
    }
    
    return this.data.midiJson.timeline.filter(e => {
        if (e.type !== 'controller') return false;
        if (controller !== undefined && e.controller !== controller) return false;
        if (channel !== undefined && e.channel !== channel) return false;
        return true;
    });
}

/**
 * ✅ NOUVEAU Phase 2: Récupère une note par son ID
 * @param {string} noteId - ID de la note
 * @returns {Object|null} Note ou null
 */
getNoteById(noteId) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return null;
    }
    
    return this.data.midiJson.timeline.find(e => 
        e.id === noteId && e.type === 'noteOn'
    ) || null;
}

/**
 * ✅ NOUVEAU Phase 2: Calcule la durée totale du fichier MIDI
 * @returns {number} Durée en ms
 */
getDuration() {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return 0;
    }
    
    const timeline = this.data.midiJson.timeline;
    if (timeline.length === 0) return 0;
    
    // Trouver l'événement le plus tard
    let maxTime = 0;
    
    timeline.forEach(event => {
        let eventEndTime = event.time;
        
        // Pour les notes, ajouter la durée
        if (event.type === 'noteOn' && event.duration) {
            eventEndTime += event.duration;
        }
        
        if (eventEndTime > maxTime) {
            maxTime = eventEndTime;
        }
    });
    
    return maxTime;
}

/**
 * ✅ NOUVEAU Phase 2: Obtient le temps de destination pour paste
 * Utilisé par EditorController.paste()
 * @returns {number} Temps en ms
 */
getCurrentPasteTime() {
    // Priorité 1: Position du curseur (si disponible)
    if (this.state.cursorPosition !== undefined) {
        return this.state.cursorPosition;
    }
    
    // Priorité 2: Fin de la dernière note sélectionnée
    const selectedNotes = this.getSelectedNotes();
    if (selectedNotes.length > 0) {
        const lastNote = selectedNotes.reduce((max, note) => 
            note.time > max.time ? note : max
        , selectedNotes[0]);
        
        return lastNote.time + lastNote.duration;
    }
    
    // Priorité 3: Fin de la timeline
    const duration = this.getDuration();
    if (duration > 0) {
        return duration;
    }
    
    // Par défaut: temps 0
    return 0;
}

/**
 * ✅ AMÉLIORATION: Update updateNote pour supporter tous les champs
 * (Méthode existante améliorée)
 */
updateNote(noteId, changes) {
    const timeline = this.data.midiJson.timeline;
    const note = timeline.find(n => n.id === noteId);
    
    if (!note) {
        this.logger.warn('EditorModel', `Note not found: ${noteId}`);
        return false;
    }
    
    // Appliquer modifications
    Object.assign(note, changes);
    
    // Valider les valeurs
    if (note.pitch !== undefined) {
        note.pitch = Math.max(0, Math.min(127, note.pitch));
    }
    if (note.velocity !== undefined) {
        note.velocity = Math.max(1, Math.min(127, note.velocity));
    }
    if (note.duration !== undefined) {
        note.duration = Math.max(1, note.duration);
    }
    if (note.time !== undefined) {
        note.time = Math.max(0, note.time);
    }
    if (note.channel !== undefined) {
        note.channel = Math.max(0, Math.min(15, note.channel));
    }
    
    // Trier timeline si le temps a changé
    if (changes.time !== undefined) {
        this.sortTimeline();
    }
    
    // Marquer modifié
    this.markModified();
    this.invalidateCache();
    
    this.eventBus.emit('editor:note-updated', { 
        noteId, 
        changes,
        note
    });
    
    return true;
}

/**
 * ✅ AMÉLIORATION: updateNotes pour batch update
 * (Méthode existante améliorée si nécessaire)
 */
updateNotes(notes) {
    if (!Array.isArray(notes) || notes.length === 0) {
        return false;
    }
    
    notes.forEach(note => {
        this.updateNote(note.id, note);
    });
    
    this.saveHistoryState('Update notes');
    
    return true;
}

/**
 * ✅ HELPER: Récupère les notes dans une plage de temps
 * Utile pour détection de collision
 */
getNotesInRange(startTime, endTime) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return [];
    }
    
    return this.data.midiJson.timeline.filter(e => {
        if (e.type !== 'noteOn') return false;
        
        const noteStart = e.time;
        const noteEnd = e.time + (e.duration || 0);
        
        // Vérifier overlap
        return !(noteEnd < startTime || noteStart > endTime);
    });
}

/**
 * ✅ HELPER: Récupère les notes sur un canal spécifique
 * Utile pour routing
 */
getNotesForChannel(channelNumber) {
    if (!this.data.midiJson || !this.data.midiJson.timeline) {
        return [];
    }
    
    return this.data.midiJson.timeline.filter(e => 
        e.type === 'noteOn' && e.channel === channelNumber
    );
}

// ============================================================================
// EXEMPLE D'UTILISATION DES NOUVELLES MÉTHODES
// ============================================================================

/*
// Ajouter un événement CC
const ccEvent = editorModel.addCC(0, 1000, 1, 64); // Canal 0, 1000ms, Modulation Wheel, valeur 64

// Mettre à jour un CC
editorModel.updateCC(ccEvent.id, { value: 100, time: 1500 });

// Récupérer tous les CC Modulation Wheel
const modulationEvents = editorModel.getCCEvents(1);

// Clear tous les CC d'un type
editorModel.clearCC(1); // Supprimer tous Modulation Wheel

// Récupérer note par ID
const note = editorModel.getNoteById('note_123');

// Obtenir durée totale
const duration = editorModel.getDuration();

// Update note avec toutes propriétés
editorModel.updateNote('note_123', {
    pitch: 60,
    time: 1000,
    duration: 500,
    velocity: 100,
    channel: 0
});

// Notes dans une plage
const notesInRange = editorModel.getNotesInRange(1000, 2000);

// Notes sur un canal
const channelNotes = editorModel.getNotesForChannel(0);
*/

	
	
	
	
	
	
	
	
	
	
    // ========================================================================
    // TRANSFORMATIONS - ✅ NOUVEAU
    // ========================================================================
    
    /**
     * Quantize les notes sélectionnées
     * @param {number} gridValue - Valeur de grille (1, 2, 4, 8, 16, 32, 64)
     * @param {number} strength - Force du quantize (0-100%)
     */
    quantize(gridValue, strength = 100) {
        const selectedNotes = this.getSelectedNotes();
        
        if (selectedNotes.length === 0) {
            this.logger.warn('EditorModel', 'No notes selected to quantize');
            return false;
        }
        
        if (!this.config.quantizeValues.includes(gridValue)) {
            this.logger.warn('EditorModel', `Invalid quantize value: ${gridValue}`);
            return false;
        }
        
        // Calculer la durée d'une grille en ms
        const bpm = this.data.midiJson?.metadata?.tempo || 120;
        const beatDuration = (60000 / bpm); // ms par beat
        const gridDuration = beatDuration * (4 / gridValue); // ms par grille
        
        const strengthFactor = strength / 100;
        
        for (const note of selectedNotes) {
            // Trouver la grille la plus proche
            const nearestGrid = Math.round(note.time / gridDuration) * gridDuration;
            
            // Appliquer avec strength
            const offset = (nearestGrid - note.time) * strengthFactor;
            note.time = Math.max(0, note.time + offset);
        }
        
        this.sortTimeline();
        this.markModified();
        this.invalidateCache();
        
        this.logger.info('EditorModel', 
            `Quantized ${selectedNotes.length} notes to 1/${gridValue} (${strength}%)`);
        
        this.eventBus.emit('editor:quantized', {
            count: selectedNotes.length,
            gridValue: gridValue,
            strength: strength
        });
        
        this.saveHistoryState('Quantize notes');
        
        return true;
    }
    
    /**
     * Transpose les notes sélectionnées
     * @param {number} semitones - Nombre de demi-tons (+/-)
     */
    transpose(semitones) {
        const selectedNotes = this.getSelectedNotes();
        
        if (selectedNotes.length === 0) {
            this.logger.warn('EditorModel', 'No notes selected to transpose');
            return false;
        }
        
        for (const note of selectedNotes) {
            const newPitch = note.pitch + semitones;
            
            // Limiter à la plage MIDI valide (0-127)
            note.pitch = Math.max(0, Math.min(127, newPitch));
        }
        
        this.markModified();
        this.invalidateCache();
        
        this.logger.info('EditorModel', 
            `Transposed ${selectedNotes.length} notes by ${semitones > 0 ? '+' : ''}${semitones}`);
        
        this.eventBus.emit('editor:transposed', {
            count: selectedNotes.length,
            semitones: semitones
        });
        
        this.saveHistoryState('Transpose notes');
        
        return true;
    }
    
    /**
     * Scale les vélocités des notes sélectionnées
     * @param {number} factor - Facteur de multiplication (0.5 = 50%, 2.0 = 200%)
     */
    scaleVelocity(factor) {
        const selectedNotes = this.getSelectedNotes();
        
        if (selectedNotes.length === 0) {
            this.logger.warn('EditorModel', 'No notes selected to scale velocity');
            return false;
        }
        
        for (const note of selectedNotes) {
            const newVelocity = Math.round(note.velocity * factor);
            note.velocity = Math.max(1, Math.min(127, newVelocity));
        }
        
        this.markModified();
        
        this.logger.info('EditorModel', 
            `Scaled velocity of ${selectedNotes.length} notes by ${factor}x`);
        
        this.eventBus.emit('editor:velocity-scaled', {
            count: selectedNotes.length,
            factor: factor
        });
        
        this.saveHistoryState('Scale velocity');
        
        return true;
    }
    
    /**
     * Change toutes les vélocités à une valeur fixe
     * @param {number} velocity - Nouvelle vélocité (1-127)
     */
    setVelocity(velocity) {
        const selectedNotes = this.getSelectedNotes();
        
        if (selectedNotes.length === 0) {
            this.logger.warn('EditorModel', 'No notes selected to set velocity');
            return false;
        }
        
        velocity = Math.max(1, Math.min(127, velocity));
        
        for (const note of selectedNotes) {
            note.velocity = velocity;
        }
        
        this.markModified();
        
        this.logger.info('EditorModel', 
            `Set velocity to ${velocity} for ${selectedNotes.length} notes`);
        
        this.eventBus.emit('editor:velocity-set', {
            count: selectedNotes.length,
            velocity: velocity
        });
        
        this.saveHistoryState('Set velocity');
        
        return true;
    }
    
    /**
     * Change la durée des notes sélectionnées
     * @param {number} factor - Facteur de multiplication
     */
    scaleDuration(factor) {
        const selectedNotes = this.getSelectedNotes();
        
        if (selectedNotes.length === 0) {
            this.logger.warn('EditorModel', 'No notes selected to scale duration');
            return false;
        }
        
        for (const note of selectedNotes) {
            note.duration = Math.max(10, Math.round(note.duration * factor));
        }
        
        this.markModified();
        
        this.logger.info('EditorModel', 
            `Scaled duration of ${selectedNotes.length} notes by ${factor}x`);
        
        this.eventBus.emit('editor:duration-scaled', {
            count: selectedNotes.length,
            factor: factor
        });
        
        this.saveHistoryState('Scale duration');
        
        return true;
    }
    
    // ========================================================================
    // ÉDITION NOTES - (DÉJÀ EXISTANT + AMÉLIORATIONS)
    // ========================================================================
    
    addNote(note) {
        if (!note.id) {
            note.id = this.generateId();
        }
        
        if (!this.data.midiJson.timeline) {
            this.data.midiJson.timeline = [];
        }
        
        this.data.midiJson.timeline.push(note);
        this.sortTimeline();
        
        this.markModified();
        this.invalidateCache();
        
        this.stats.notesCreated++;
        
        this.eventBus.emit('editor:note-added', { note });
        
        return note;
    }
    
    updateNote(noteId, changes) {
        const timeline = this.data.midiJson.timeline;
        const note = timeline.find(n => n.id === noteId);
        
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
    
    // ========================================================================
    // HISTORIQUE - 
    // ========================================================================
    
    saveHistoryState(description) {
        if (!this.history.enabled) return;
        
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
        
        if (this.history.states.length > this.history.maxStates) {
            this.history.states.shift();
            this.history.currentIndex--;
        }
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
        
        return true;
    }
    // Ajouter au Controller
quantize(division = 16) {
    if (!this.editorModel) return;
    
    const success = this.editorModel.quantize(division);
    
    if (success) {
        this.reloadVisualizer();
        this.showSuccess('Notes quantized');
    }
}

transpose(semitones) {
    if (!this.editorModel) return;
    
    const success = this.editorModel.transpose(semitones);
    
    if (success) {
        this.reloadVisualizer();
        this.showSuccess(`Transposed ${semitones > 0 ? '+' : ''}${semitones}`);
    }
}

scaleVelocity(factor) {
    if (!this.editorModel) return;
    
    const success = this.editorModel.scaleVelocity(factor);
    
    if (success) {
        this.reloadVisualizer();
        this.showSuccess('Velocity scaled');
    }
}
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    sortTimeline() {
        if (this.data.midiJson && this.data.midiJson.timeline) {
            this.data.midiJson.timeline.sort((a, b) => {
                if (a.time !== b.time) return a.time - b.time;
                if (a.type === 'noteOff' && b.type === 'noteOn') return -1;
                if (a.type === 'noteOn' && b.type === 'noteOff') return 1;
                return 0;
            });
        }
    }
    
    generateId() {
        return `note_${Date.now()}_${this.nextId++}`;
    }
    
    ensureEventIds() {
        if (!this.data.midiJson || !this.data.midiJson.timeline) return;
        
        this.data.midiJson.timeline.forEach(event => {
            if (!event.id) {
                event.id = this.generateId();
            }
        });
    }
    
    markModified() {
        this.state.isModified = true;
        this.eventBus.emit('editor:modified', { isModified: true });
    }
    bindEvents() {
    // Écouter EditorModel directement
    this.eventBus.on('editor:modified', (data) => {
        this.editorState.hasUnsavedChanges = data.isModified;
        this.updateModifiedState();
    });
    
    // Les callbacks visualizer délèguent UNIQUEMENT
    this.onNoteAdded = (data) => {
        this.editorModel.addNote(data.note);
        // C'est tout ! EditorModel émettra 'editor:modified'
    };
}
    invalidateCache() {
        this.cache.dirty = true;
        this.cache.notesByTime.clear();
        this.cache.notesByPitch.clear();
    }
    
    // Sauvegarde et auto-save (déjà existant)
    async save() {
        // ... (code existant)
    }
    
    startAutoSave() {
        // ... (code existant)
    }
    
    stopAutoSave() {
        if (this.autoSaveTimer) {
            clearInterval(this.autoSaveTimer);
            this.autoSaveTimer = null;
        }
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
            historySize: this.history.states.length
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
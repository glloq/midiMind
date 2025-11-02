// ============================================================================
// Fichier: frontend/js/controllers/KeyboardController.js
// Chemin: frontend/js/controllers/KeyboardController.js
// Version: v3.1.3 - LOOP RECORDER REMOVED
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v3.1.3:
// ❌ Suppression: this.enableLoopRecorder (ligne 23)
// ============================================================================
// CORRECTIONS v3.1.2:
// ✅ CRITIQUE: Removed duplicate this.initialize() call from constructor
// ✅ BaseController already calls initialize() via autoInitialize
// ✅ Fixes "this.eventBus.once is not a function" error
// ============================================================================

class KeyboardController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // ✅ Backend
        this.backend = window.backendService;
        
        // Configuration
        this.mode = PerformanceConfig.keyboard.mode || 'monitor';
        this.enableRecording = false;
        this.enablePlayback = PerformanceConfig.keyboard.enablePlayback || true;
        this.showIncomingNotes = false;
        
        // Instrument sélectionné
        this.selectedInstrument = null;
        this.instrumentProfile = null;
        this.noteRange = { min: 21, max: 108 };
        this.noteMapping = null;
        
        // Notes actives
        this.activeNotes = new Map();
        this.pressedKeys = new Set();
        
        // Devices disponibles
        this.availableDevices = [];
        
        // Configuration velocity
        this.currentVelocity = 100;
        
        // Statistiques
        this.stats = {
            notesPlayed: 0,
            totalDuration: 0,
            errors: 0
        };
        
        this.logDebug('keyboard', `✓ KeyboardController initialized (mode: ${this.mode})`);
        
        // ✅ REMOVED: this.initialize() - BaseController calls it automatically via autoInitialize
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    async initialize() {
        this.attachEvents();
        
        // Charger devices disponibles (seulement si backend connecté)
        if (this.backend && this.backend.isConnected()) {
            await this.loadAvailableDevices();
        } else {
            this.logDebug('keyboard', 'Backend not connected - devices will load later');
            // Écouter connexion backend
            this.eventBus.once('backend:connected', async () => {
                await this.loadAvailableDevices();
            });
        }
    }
    
    attachEvents() {
        // Sélection instrument
        this.eventBus.on('keyboard:select-instrument', async (data) => {
            await this.selectInstrument(data.instrumentId);
        });
        
        // Changement velocity
        this.eventBus.on('keyboard:velocity-changed', (data) => {
            this.currentVelocity = data.velocity;
        });
    }
    
    attachKeyboardEvents() {
        // Mapping clavier PC → notes MIDI
        this.keyboardMap = this.createKeyboardMap();
        
        document.addEventListener('keydown', (e) => {
            if (e.repeat) return;
            this.handleKeyDown(e);
        });
        
        document.addEventListener('keyup', (e) => {
            this.handleKeyUp(e);
        });
    }
    
    createKeyboardMap() {
        return {
            'a': 60, 'w': 61, 's': 62, 'e': 63, 'd': 64, 'f': 65,
            't': 66, 'g': 67, 'y': 68, 'h': 69, 'u': 70, 'j': 71,
            'k': 72, 'o': 73, 'l': 74, 'p': 75, ';': 76, '\'': 77,
            'z': 48, 'x': 50, 'c': 52, 'v': 53, 'b': 55, 'n': 57,
            'm': 59, ',': 60, '.': 62, '/': 64
        };
    }
    
    handleKeyDown(e) {
        const key = e.key.toLowerCase();
        const noteNumber = this.keyboardMap[key];
        
        if (noteNumber === undefined || this.pressedKeys.has(key)) {
            return;
        }
        
        this.pressedKeys.add(key);
        this.sendNoteOn(noteNumber);
    }
    
    handleKeyUp(e) {
        const key = e.key.toLowerCase();
        const noteNumber = this.keyboardMap[key];
        
        if (noteNumber === undefined) {
            return;
        }
        
        this.pressedKeys.delete(key);
        this.sendNoteOff(noteNumber);
    }
    
    // ========================================================================
    // GESTION DEVICES
    // ========================================================================
    
    async loadAvailableDevices() {
        if (!this.backend || !this.backend.isConnected()) {
            this.logDebug('keyboard', 'Backend not ready for device scan');
            return;
        }
        
        try {
            const response = await this.backend.sendCommand('devices.scan');
            this.availableDevices = response.devices || [];
            this.logDebug('keyboard', `${this.availableDevices.length} devices available`);
            
            // Notifier la vue
            this.eventBus.emit('keyboard:devices-loaded', {
                devices: this.availableDevices
            });
            
        } catch (error) {
            this.logDebug('keyboard', `Device scan failed: ${error.message}`, 'error');
            this.availableDevices = [];
        }
    }
    
    // ========================================================================
    // SÉLECTION INSTRUMENT
    // ========================================================================
    
    async selectInstrument(instrumentId) {
        this.logDebug('keyboard', `Selecting instrument: ${instrumentId}`);
        
        try {
            // Récupérer profil instrument
            this.selectedInstrument = instrumentId;
            
            // Charger profile depuis InstrumentModel si disponible
            const instrumentModel = this.getModel('instrument');
            if (instrumentModel && instrumentModel.getInstrument) {
                this.instrumentProfile = instrumentModel.getInstrument(instrumentId);
                
                // Mettre à jour note range si profile disponible
                if (this.instrumentProfile && this.instrumentProfile.noteRange) {
                    this.noteRange = this.instrumentProfile.noteRange;
                }
                
                // Charger note mapping si disponible
                if (this.instrumentProfile && this.instrumentProfile.noteMapping) {
                    this.noteMapping = this.instrumentProfile.noteMapping;
                }
            }
            
            this.logDebug('keyboard', `Instrument selected: ${instrumentId}`);
            
            // Notifier vue
            this.eventBus.emit('keyboard:instrument-selected', {
                instrumentId,
                profile: this.instrumentProfile,
                noteRange: this.noteRange
            });
            
        } catch (error) {
            this.handleError(error, `Failed to select instrument ${instrumentId}`);
        }
    }
    
    // ========================================================================
    // ENVOI NOTES MIDI
    // ========================================================================
    
    sendNoteOn(noteNumber, velocity = null) {
        if (!this.selectedInstrument) {
            this.logDebug('keyboard', 'No instrument selected', 'warn');
            return;
        }
        
        // Appliquer mapping si disponible
        const mappedNote = this.noteMapping ? 
            (this.noteMapping[noteNumber] || noteNumber) : 
            noteNumber;
        
        // Vérifier range
        if (mappedNote < this.noteRange.min || mappedNote > this.noteRange.max) {
            this.logDebug('keyboard', `Note ${mappedNote} outside range ${this.noteRange.min}-${this.noteRange.max}`, 'warn');
            return;
        }
        
        const finalVelocity = velocity || this.currentVelocity;
        
        // Enregistrer note active
        this.activeNotes.set(noteNumber, {
            note: mappedNote,
            velocity: finalVelocity,
            startTime: Date.now()
        });
        
        // Envoyer au backend
        if (this.enablePlayback && this.backend && this.backend.isConnected()) {
            this.backend.sendCommand('midi.sendNoteOn', {
                instrument: this.selectedInstrument,
                note: mappedNote,
                velocity: finalVelocity
            }).catch(err => {
                this.logDebug('keyboard', `Note-on failed: ${err.message}`, 'error');
            });
        }
        
        // Notifier vue
        this.eventBus.emit('keyboard:note-on', {
            note: mappedNote,
            velocity: finalVelocity,
            originalNote: noteNumber
        });
        
        this.stats.notesPlayed++;
    }
    
    sendNoteOff(noteNumber) {
        const noteInfo = this.activeNotes.get(noteNumber);
        if (!noteInfo) return;
        
        const duration = Date.now() - noteInfo.startTime;
        this.stats.totalDuration += duration;
        
        // Envoyer au backend
        if (this.enablePlayback && this.backend && this.backend.isConnected()) {
            this.backend.sendCommand('midi.sendNoteOff', {
                instrument: this.selectedInstrument,
                note: noteInfo.note
            }).catch(err => {
                this.logDebug('keyboard', `Note-off failed: ${err.message}`, 'error');
            });
        }
        
        // Notifier vue
        this.eventBus.emit('keyboard:note-off', {
            note: noteInfo.note,
            duration,
            originalNote: noteNumber
        });
        
        this.activeNotes.delete(noteNumber);
    }
    
    // ========================================================================
    // STATS
    // ========================================================================
    
    getStats() {
        const avgDuration = this.stats.notesPlayed > 0 ? 
            Math.round(this.stats.totalDuration / this.stats.notesPlayed) : 
            0;
        
        return {
            notesPlayed: this.stats.notesPlayed,
            avgDuration,
            activeNotes: this.activeNotes.size,
            errors: this.stats.errors
        };
    }
    
    resetStats() {
        this.stats = {
            notesPlayed: 0,
            totalDuration: 0,
            errors: 0
        };
        
        this.eventBus.emit('keyboard:stats-reset');
    }
    
    // ========================================================================
    // CLEANUP
    // ========================================================================
    
    cleanup() {
        // Arrêter toutes les notes actives
        for (const [noteNumber] of this.activeNotes) {
            this.sendNoteOff(noteNumber);
        }
        
        this.activeNotes.clear();
        this.pressedKeys.clear();
        
        super.cleanup();
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = KeyboardController;
}

window.KeyboardController = KeyboardController;

// ============================================================================
// FIN DU FICHIER KeyboardController.js v3.1.3
// ============================================================================
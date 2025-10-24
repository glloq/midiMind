// ============================================================================
// Fichier: frontend/js/controllers/KeyboardController.js
// Version: v3.1.0 - ADAPTED FOR MIDI.SEND
// Date: 2025-10-16
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// MODIFICATIONS v3.1.0:
// ✓ Utilisation de midi.send pour playback direct
// ✓ Récupération instruments via devices.list / latency.list
// ✓ Support note_mappings personnalisés
// ✓ SUPPRESSION COMPLÈTE : enregistrement + loops
// ✓ Affichage notes entrantes DÉSACTIVÉ
// ============================================================================

class KeyboardController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Configuration
        this.mode = PerformanceConfig.keyboard.mode || 'monitor';
        this.enableRecording = false;  // ✓ DÉSACTIVÉ
        this.enableLoopRecorder = false;  // ✓ DÉSACTIVÉ
        this.enablePlayback = PerformanceConfig.keyboard.enablePlayback || true;
        this.showIncomingNotes = false;  // ✓ DÉSACTIVÉ
        
        // Instrument sélectionné
        this.selectedInstrument = null;
        this.instrumentProfile = null;
        this.noteRange = { min: 21, max: 108 };  // 88 touches par défaut
        this.noteMapping = null;  // Mapping personnalisé si existe
        
        // Notes actives
        this.activeNotes = new Map();  // note → { velocity, timestamp }
        this.pressedKeys = new Set();  // Touches clavier PC enfoncées
        
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
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    async initialize() {
        this.attachEvents();
        
        // Charger devices disponibles
        await this.loadAvailableDevices();
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
        
        // ✓ SUPPRIMÉ : événements d'enregistrement
        // ✓ SUPPRIMÉ : événements de loop recorder
        
        // ✓ DÉSACTIVÉ : Affichage notes entrantes
        // this.eventBus.on('midi:noteOn', ...) 
        // this.eventBus.on('midi:noteOff', ...)
        
        // Clavier PC
        this.attachKeyboardEvents();
    }
    
    attachKeyboardEvents() {
        // Mapping clavier PC → notes MIDI
        this.keyboardMap = this.createKeyboardMap();
        
        document.addEventListener('keydown', (e) => {
            if (e.repeat) return;  // Ignorer auto-repeat
            this.handleKeyDown(e);
        });
        
        document.addEventListener('keyup', (e) => {
            this.handleKeyUp(e);
        });
    }
    
    createKeyboardMap() {
        // Mapping touches PC → notes MIDI (2 octaves)
        return {
            // Octave basse (touches QWERTY)
            'a': 60,  // C4
            'w': 61,  // C#4
            's': 62,  // D4
            'e': 63,  // D#4
            'd': 64,  // E4
            'f': 65,  // F4
            't': 66,  // F#4
            'g': 67,  // G4
            'y': 68,  // G#4
            'h': 69,  // A4
            'u': 70,  // A#4
            'j': 71,  // B4
            
            // Octave haute
            'k': 72,  // C5
            'o': 73,  // C#5
            'l': 74,  // D5
            'p': 75,  // D#5
            ';': 76,  // E5
            '\'': 77, // F5
            ']': 78,  // F#5
        };
    }
    
    // ========================================================================
    // CHARGEMENT DEVICES (devices.list ou latency.list)
    // ========================================================================
    
    async loadAvailableDevices() {
        try {
            // Essayer latency.list d'abord (instruments configurés)
            let response = await this.backend.sendCommand('latency.list');
            
            if (response.success && response.devices) {
                this.availableDevices = response.devices;
                this.logDebug('keyboard', `✓ Loaded ${response.devices.length} devices from latency.list`);
            } else {
                // Fallback sur devices.list
                response = await this.backend.sendCommand('devices.list');
                
                if (response.success && response.devices) {
                    this.availableDevices = response.devices;
                    this.logDebug('keyboard', `✓ Loaded ${response.devices.length} devices from devices.list`);
                }
            }
            
            // Émettre événement
            this.eventBus.emit('keyboard:devices-loaded', {
                devices: this.availableDevices
            });
            
        } catch (error) {
            this.logDebug('keyboard', `Failed to load devices: ${error.message}`, 'error');
            this.showNotification('Erreur chargement instruments', 'error');
        }
    }
    
    // ========================================================================
    // SÉLECTION INSTRUMENT
    // ========================================================================
    
    async selectInstrument(instrumentId) {
        if (!instrumentId) {
            // Désélection
            this.selectedInstrument = null;
            this.instrumentProfile = null;
            this.noteRange = { min: 21, max: 108 };
            this.noteMapping = null;
            
            this.eventBus.emit('keyboard:instrument-deselected');
            this.logDebug('keyboard', 'Instrument deselected');
            return;
        }
        
        this.selectedInstrument = instrumentId;
        
        try {
            // Récupérer profil instrument
            const response = await this.backend.sendCommand('instruments.getProfile', {
                instrument_id: instrumentId
            });
            
            if (response.success && response.profile) {
                this.instrumentProfile = response.profile;
                
                // Extraire note range
                if (response.profile.min_note !== undefined && response.profile.max_note !== undefined) {
                    this.noteRange = {
                        min: response.profile.min_note,
                        max: response.profile.max_note
                    };
                    
                    this.logDebug('keyboard', `Note range: ${this.noteRange.min}-${this.noteRange.max}`);
                }
                
                // Extraire note mapping si existe
                if (response.profile.note_mappings && Array.isArray(response.profile.note_mappings)) {
                    this.noteMapping = new Map();
                    
                    response.profile.note_mappings.forEach(mapping => {
                        this.noteMapping.set(mapping.midi_note, {
                            name: mapping.name,
                            velocity: mapping.velocity || 100
                        });
                    });
                    
                    this.logDebug('keyboard', `Custom mapping: ${this.noteMapping.size} notes`);
                } else {
                    this.noteMapping = null;
                }
                
                // Émettre événement
                this.eventBus.emit('keyboard:instrument-selected', {
                    instrumentId,
                    profile: this.instrumentProfile,
                    noteRange: this.noteRange,
                    hasCustomMapping: this.noteMapping !== null
                });
                
                this.showNotification(`Instrument sélectionné: ${response.profile.name}`, 'success');
                
            } else {
                throw new Error(response.error || 'Failed to get profile');
            }
            
        } catch (error) {
            this.logDebug('keyboard', `Error loading instrument: ${error.message}`, 'error');
            this.showNotification('Erreur chargement instrument', 'error');
            
            // Réinitialiser
            this.selectedInstrument = null;
            this.instrumentProfile = null;
        }
    }
    
    // ========================================================================
    // PLAYBACK NOTES (midi.send)
    // ========================================================================
    
    async playNote(note, velocity = null) {
        if (!this.enablePlayback) {
            this.logDebug('keyboard', 'Playback disabled', 'warn');
            return;
        }
        
        if (!this.selectedInstrument) {
            this.showNotification('Sélectionnez un instrument', 'warning');
            return;
        }
        
        // Vérifier si note jouable
        if (!this.isNotePlayable(note)) {
            this.logDebug('keyboard', `Note ${note} not playable (out of range or not mapped)`, 'warn');
            return;
        }
        
        // Velocity
        const finalVelocity = velocity !== null ? velocity : this.currentVelocity;
        
        try {
            // ✓ UTILISER midi.send pour noteOn
            const response = await this.backend.sendCommand('midi.send', {
                device_id: this.selectedInstrument,
                message: {
                    type: 'note_on',
                    channel: 0,
                    note: note,
                    velocity: finalVelocity
                }
            });
            
            if (response.success) {
                // Enregistrer note active
                this.activeNotes.set(note, {
                    velocity: finalVelocity,
                    timestamp: Date.now()
                });
                
                this.stats.notesPlayed++;
                
                // Émettre événement pour mise à jour UI
                this.eventBus.emit('keyboard:note-on', {
                    note,
                    velocity: finalVelocity
                });
                
                this.logDebug('keyboard', `Note ON: ${note} (vel: ${finalVelocity})`);
                
            } else {
                throw new Error(response.error || 'Failed to send note');
            }
            
        } catch (error) {
            this.logDebug('keyboard', `Play note error: ${error.message}`, 'error');
            this.stats.errors++;
        }
    }
    
    async stopNote(note) {
        if (!this.enablePlayback || !this.selectedInstrument) {
            return;
        }
        
        // Vérifier si note active
        if (!this.activeNotes.has(note)) {
            return;
        }
        
        try {
            // ✓ UTILISER midi.send pour noteOff
            const response = await this.backend.sendCommand('midi.send', {
                device_id: this.selectedInstrument,
                message: {
                    type: 'note_off',
                    channel: 0,
                    note: note,
                    velocity: 0
                }
            });
            
            if (response.success) {
                // Calculer durée
                const noteInfo = this.activeNotes.get(note);
                const duration = Date.now() - noteInfo.timestamp;
                this.stats.totalDuration += duration;
                
                // Retirer des notes actives
                this.activeNotes.delete(note);
                
                // Émettre événement
                this.eventBus.emit('keyboard:note-off', {
                    note,
                    duration
                });
                
                this.logDebug('keyboard', `Note OFF: ${note} (duration: ${duration}ms)`);
                
            } else {
                throw new Error(response.error || 'Failed to send note off');
            }
            
        } catch (error) {
            this.logDebug('keyboard', `Stop note error: ${error.message}`, 'error');
            this.stats.errors++;
        }
    }
    
    // ========================================================================
    // VALIDATION NOTES
    // ========================================================================
    
    isNotePlayable(note) {
        // Si mapping personnalisé, vérifier si note existe dans mapping
        if (this.noteMapping) {
            return this.noteMapping.has(note);
        }
        
        // Sinon, vérifier range
        return note >= this.noteRange.min && note <= this.noteRange.max;
    }
    
    getPlayableNotes() {
        // Si mapping personnalisé, retourner liste des notes mappées
        if (this.noteMapping) {
            return Array.from(this.noteMapping.keys()).sort((a, b) => a - b);
        }
        
        // Sinon, retourner toutes les notes du range
        const notes = [];
        for (let n = this.noteRange.min; n <= this.noteRange.max; n++) {
            notes.push(n);
        }
        return notes;
    }
    
    getNoteInfo(note) {
        if (this.noteMapping && this.noteMapping.has(note)) {
            return this.noteMapping.get(note);
        }
        
        return null;
    }
    
    // ========================================================================
    // CLAVIER PC HANDLERS
    // ========================================================================
    
    handleKeyDown(e) {
        // Ignorer si focus dans input
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
            return;
        }
        
        const key = e.key.toLowerCase();
        
        // Vérifier si touche mappée
        if (!this.keyboardMap[key]) {
            return;
        }
        
        // Éviter répétition
        if (this.pressedKeys.has(key)) {
            return;
        }
        
        this.pressedKeys.add(key);
        
        const note = this.keyboardMap[key];
        
        // Jouer note
        this.playNote(note);
        
        e.preventDefault();
    }
    
    handleKeyUp(e) {
        const key = e.key.toLowerCase();
        
        if (!this.keyboardMap[key]) {
            return;
        }
        
        this.pressedKeys.delete(key);
        
        const note = this.keyboardMap[key];
        
        // Arrêter note
        this.stopNote(note);
        
        e.preventDefault();
    }
    
    // ========================================================================
    // PANIC (arrêter toutes les notes)
    // ========================================================================
    
    async panic() {
        this.logDebug('keyboard', 'PANIC: stopping all notes');
        
        // Arrêter toutes les notes actives
        const notes = Array.from(this.activeNotes.keys());
        
        for (const note of notes) {
            await this.stopNote(note);
        }
        
        this.activeNotes.clear();
        this.pressedKeys.clear();
        
        this.showNotification('Toutes les notes arrêtées', 'info');
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    getState() {
        return {
            mode: this.mode,
            selectedInstrument: this.selectedInstrument,
            noteRange: this.noteRange,
            hasCustomMapping: this.noteMapping !== null,
            playableNotes: this.getPlayableNotes().length,
            activeNotes: this.activeNotes.size,
            currentVelocity: this.currentVelocity,
            enableRecording: false,  // ✓ Toujours false
            enablePlayback: this.enablePlayback
        };
    }
    
    getStats() {
        return {
            ...this.stats,
            activeNotes: this.activeNotes.size,
            avgDuration: this.stats.notesPlayed > 0 
                ? this.stats.totalDuration / this.stats.notesPlayed 
                : 0
        };
    }
    
    getAvailableDevices() {
        return this.availableDevices;
    }
    
    // ========================================================================
    // MÉTHODES SUPPRIMÉES (enregistrement + loops)
    // ========================================================================
    
    // ✓ SUPPRIMÉ : startRecording()
    // ✓ SUPPRIMÉ : stopRecording()
    // ✓ SUPPRIMÉ : saveRecording()
    // ✓ SUPPRIMÉ : startLoop()
    // ✓ SUPPRIMÉ : stopLoop()
    // ✓ SUPPRIMÉ : clearLoop()
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        // Arrêter toutes les notes
        this.panic();
        
        // Nettoyer
        this.activeNotes.clear();
        this.pressedKeys.clear();
        
        this.logDebug('keyboard', '✓ KeyboardController destroyed');
        
        super.destroy();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = KeyboardController;
}

if (typeof window !== 'undefined') {
    window.KeyboardController = KeyboardController;
}

// Export par défaut
window.KeyboardController = KeyboardController;
// ============================================================================
// Fichier: frontend/js/controllers/KeyboardController.js
// Version: v3.1.1 - FIXED BACKEND
// Date: 2025-01-24
// ============================================================================
// CORRECTIONS v3.1.1:
// ✅ Ajout initialisation this.backend = window.backendService
// ✅ Vérification backend avant sendCommand
// ============================================================================

class KeyboardController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // ✅ Backend
        this.backend = window.backendService;
        
        // Configuration
        this.mode = PerformanceConfig.keyboard.mode || 'monitor';
        this.enableRecording = false;
        this.enableLoopRecorder = false;
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
        
        this.initialize();
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
            ']': 78
        };
    }
    
    // ========================================================================
    // CHARGEMENT DEVICES
    // ========================================================================
    
    async loadAvailableDevices() {
        // ✅ Vérifier backend
        if (!this.backend || !this.backend.isConnected()) {
            this.logDebug('keyboard', 'Backend not available for loading devices', 'warn');
            return;
        }
        
        try {
            // Essayer latency.list d'abord
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
            this.showError('Erreur chargement instruments');
        }
    }
    
    // ========================================================================
    // SÉLECTION INSTRUMENT
    // ========================================================================
    
    async selectInstrument(instrumentId) {
        if (!this.backend || !this.backend.isConnected()) {
            this.showError('Backend non connecté');
            return;
        }
        
        try {
            const response = await this.backend.sendCommand('latency.get', {
                device_id: instrumentId
            });
            
            if (response.success) {
                this.selectedInstrument = instrumentId;
                this.instrumentProfile = response.profile || {};
                this.noteMapping = response.note_mappings || null;
                
                // Mettre à jour note range si disponible
                if (this.instrumentProfile.note_range) {
                    this.noteRange = this.instrumentProfile.note_range;
                }
                
                this.logDebug('keyboard', `✓ Selected instrument: ${instrumentId}`);
                this.eventBus.emit('keyboard:instrument-selected', {
                    instrumentId,
                    profile: this.instrumentProfile
                });
            }
        } catch (error) {
            this.logDebug('keyboard', `Failed to select instrument: ${error.message}`, 'error');
            this.showError('Erreur sélection instrument');
        }
    }
    
    // ========================================================================
    // PLAYBACK
    // ========================================================================
    
    handleKeyDown(event) {
        const key = event.key.toLowerCase();
        
        if (!this.keyboardMap[key] || this.pressedKeys.has(key)) {
            return;
        }
        
        this.pressedKeys.add(key);
        const note = this.keyboardMap[key];
        
        this.playNote(note, this.currentVelocity);
    }
    
    handleKeyUp(event) {
        const key = event.key.toLowerCase();
        
        if (!this.keyboardMap[key]) {
            return;
        }
        
        this.pressedKeys.delete(key);
        const note = this.keyboardMap[key];
        
        this.stopNote(note);
    }
    
    async playNote(note, velocity = 100) {
        if (!this.backend || !this.backend.isConnected()) {
            return;
        }
        
        if (!this.selectedInstrument) {
            this.showWarning('Aucun instrument sélectionné');
            return;
        }
        
        // Appliquer note mapping si existe
        const mappedNote = this.noteMapping 
            ? (this.noteMapping[note] || note)
            : note;
        
        try {
            await this.backend.sendCommand('midi.send', {
                device_id: this.selectedInstrument,
                message: {
                    type: 'note_on',
                    note: mappedNote,
                    velocity: velocity
                }
            });
            
            this.activeNotes.set(note, {
                velocity,
                timestamp: Date.now()
            });
            
            this.stats.notesPlayed++;
            
            this.eventBus.emit('keyboard:note-on', {
                note: mappedNote,
                velocity
            });
            
        } catch (error) {
            this.logDebug('keyboard', `Failed to play note: ${error.message}`, 'error');
            this.stats.errors++;
        }
    }
    
    async stopNote(note) {
        if (!this.backend || !this.backend.isConnected()) {
            return;
        }
        
        if (!this.selectedInstrument || !this.activeNotes.has(note)) {
            return;
        }
        
        const mappedNote = this.noteMapping 
            ? (this.noteMapping[note] || note)
            : note;
        
        try {
            await this.backend.sendCommand('midi.send', {
                device_id: this.selectedInstrument,
                message: {
                    type: 'note_off',
                    note: mappedNote,
                    velocity: 0
                }
            });
            
            const noteInfo = this.activeNotes.get(note);
            if (noteInfo) {
                const duration = Date.now() - noteInfo.timestamp;
                this.stats.totalDuration += duration;
            }
            
            this.activeNotes.delete(note);
            
            this.eventBus.emit('keyboard:note-off', {
                note: mappedNote
            });
            
        } catch (error) {
            this.logDebug('keyboard', `Failed to stop note: ${error.message}`, 'error');
            this.stats.errors++;
        }
    }
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    getStats() {
        return {
            ...this.stats,
            activeNotes: this.activeNotes.size,
            pressedKeys: this.pressedKeys.size,
            selectedInstrument: this.selectedInstrument,
            availableDevices: this.availableDevices.length
        };
    }
    
    reset() {
        // Arrêter toutes les notes actives
        for (const note of this.activeNotes.keys()) {
            this.stopNote(note);
        }
        
        this.activeNotes.clear();
        this.pressedKeys.clear();
        
        this.stats = {
            notesPlayed: 0,
            totalDuration: 0,
            errors: 0
        };
        
        this.logDebug('keyboard', '✓ Controller reset');
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
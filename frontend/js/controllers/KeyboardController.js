// ============================================================================
// Fichier: frontend/js/controllers/KeyboardController.js
// Chemin réel: frontend/js/controllers/KeyboardController.js
// Version: v4.2.2 - API CORRECTED
// Date: 2025-11-02
// ============================================================================
// CORRECTIONS v4.2.2:
// ✅ device_id (pas instrument) pour midi.sendNoteOn/Off
// ✅ Utiliser helpers BackendService
// ============================================================================

class KeyboardController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.backend = window.app?.services?.backend || window.backendService;
        
        this.mode = PerformanceConfig?.keyboard?.mode || 'monitor';
        this.enableRecording = false;
        this.enablePlayback = true;
        this.showIncomingNotes = false;
        
        this.selectedDevice = null;
        this.instrumentProfile = null;
        this.noteRange = { min: 21, max: 108 };
        this.noteMapping = null;
        
        this.activeNotes = new Map();
        this.pressedKeys = new Set();
        this.availableDevices = [];
        this.currentVelocity = 100;
        
        this.stats = {
            notesPlayed: 0,
            totalDuration: 0,
            errors: 0
        };
        
        this.logDebug('keyboard', `✓ KeyboardController v4.2.2 (mode: ${this.mode})`);
    }
    
    async initialize() {
        this.attachEvents();
        
        if (this.backend?.isConnected()) {
            await this.loadAvailableDevices();
        } else {
            this.eventBus.once('backend:connected', async () => {
                await this.loadAvailableDevices();
            });
        }
    }
    
    attachEvents() {
        this.eventBus.on('keyboard:select-device', async (data) => {
            await this.selectDevice(data.device_id);
        });
        
        this.eventBus.on('keyboard:velocity-changed', (data) => {
            this.currentVelocity = data.velocity;
        });
    }
    
    attachKeyboardEvents() {
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
            'k': 72, 'o': 73, 'l': 74, 'p': 75, ';': 76, '\'': 77
        };
    }
    
    handleKeyDown(e) {
        const key = e.key.toLowerCase();
        const noteNumber = this.keyboardMap[key];
        
        if (noteNumber === undefined || this.pressedKeys.has(key)) return;
        
        this.pressedKeys.add(key);
        this.sendNoteOn(noteNumber);
    }
    
    handleKeyUp(e) {
        const key = e.key.toLowerCase();
        const noteNumber = this.keyboardMap[key];
        
        if (noteNumber === undefined) return;
        
        this.pressedKeys.delete(key);
        this.sendNoteOff(noteNumber);
    }
    
    async loadAvailableDevices() {
        if (!this.backend?.isConnected()) {
            this.logDebug('keyboard', 'Backend not ready');
            return;
        }
        
        try {
            const response = await this.backend.scanDevices();
            this.availableDevices = response.devices || [];
            this.logDebug('keyboard', `${this.availableDevices.length} devices available`);
            
            this.eventBus.emit('keyboard:devices-loaded', {
                devices: this.availableDevices
            });
        } catch (error) {
            this.handleError(error, 'Failed to load devices');
        }
    }
    
    async selectDevice(device_id) {
        try {
            this.selectedDevice = device_id;
            
            this.logDebug('keyboard', `Device selected: ${device_id}`);
            
            this.eventBus.emit('keyboard:device-selected', {
                device_id,
                noteRange: this.noteRange
            });
            
        } catch (error) {
            this.handleError(error, `Failed to select device ${device_id}`);
        }
    }
    
    /**
     * ✅ CORRECTION: device_id, note, velocity, channel
     */
    sendNoteOn(noteNumber, velocity = null, channel = 0) {
        if (!this.selectedDevice) {
            this.logDebug('keyboard', 'No device selected', 'warn');
            return;
        }
        
        const mappedNote = this.noteMapping ? 
            (this.noteMapping[noteNumber] || noteNumber) : 
            noteNumber;
        
        if (mappedNote < this.noteRange.min || mappedNote > this.noteRange.max) {
            this.logDebug('keyboard', `Note ${mappedNote} outside range`, 'warn');
            return;
        }
        
        const finalVelocity = velocity || this.currentVelocity;
        
        this.activeNotes.set(noteNumber, {
            note: mappedNote,
            velocity: finalVelocity,
            startTime: Date.now()
        });
        
        if (this.enablePlayback && this.backend?.isConnected()) {
            // ✅ Utiliser helper BackendService
            this.backend.sendNoteOn(
                this.selectedDevice,
                mappedNote,
                finalVelocity,
                channel
            ).catch(err => {
                this.logDebug('keyboard', `Note-on failed: ${err.message}`, 'error');
            });
        }
        
        this.eventBus.emit('keyboard:note-on', {
            note: mappedNote,
            velocity: finalVelocity,
            originalNote: noteNumber
        });
        
        this.stats.notesPlayed++;
    }
    
    sendNoteOff(noteNumber, channel = 0) {
        if (!this.selectedDevice) return;
        
        const activeNote = this.activeNotes.get(noteNumber);
        if (!activeNote) return;
        
        const mappedNote = activeNote.note;
        const duration = Date.now() - activeNote.startTime;
        
        this.activeNotes.delete(noteNumber);
        
        if (this.enablePlayback && this.backend?.isConnected()) {
            // ✅ Utiliser helper BackendService
            this.backend.sendNoteOff(
                this.selectedDevice,
                mappedNote,
                channel
            ).catch(err => {
                this.logDebug('keyboard', `Note-off failed: ${err.message}`, 'error');
            });
        }
        
        this.eventBus.emit('keyboard:note-off', {
            note: mappedNote,
            duration,
            originalNote: noteNumber
        });
        
        this.stats.totalDuration += duration;
    }
    
    panicAllNotesOff() {
        if (!this.selectedDevice || !this.backend?.isConnected()) return;
        
        for (let note = 0; note <= 127; note++) {
            this.backend.sendNoteOff(this.selectedDevice, note, 0);
        }
        
        this.activeNotes.clear();
        this.pressedKeys.clear();
        
        this.logDebug('keyboard', 'PANIC: All notes off');
    }
    
    getStats() {
        return { ...this.stats };
    }
    
    handleError(error, context) {
        this.stats.errors++;
        this.logDebug('keyboard', `${context}: ${error.message}`, 'error');
    }
    
    logDebug(category, message, level = 'info') {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](category, message);
        } else {
            console.log(`[${category}] ${message}`);
        }
    }
}

if (typeof window !== 'undefined') {
    window.KeyboardController = KeyboardController;
}
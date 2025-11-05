// ============================================================================
// Fichier: frontend/js/views/KeyboardView.js
// Version: v4.0.0 - CONFORMITÉ API DOCUMENTATION
// Date: 2025-11-02
// ============================================================================
// AMÉLIORATIONS v4.0.0:
// ✓ API v4.2.2: midi.sendNoteOn, midi.sendNoteOff
// ✓ Clavier interactif 88 touches
// ✓ Sélection device de sortie
// ============================================================================

class KeyboardView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        this.config.autoRender = false;
        this.config.name = 'KeyboardView';
        
        // État
        this.viewState = {
            selectedDevice: null,
            devices: [],
            noteRange: { min: 21, max: 108 }, // 88 touches
            velocity: 80,
            activeNotes: new Set(),
            octaveOffset: 0
        };
        
        // Canvas
        this.canvas = null;
        this.ctx = null;
        this.keyWidth = 24;
        this.whiteKeyHeight = 140;
        this.blackKeyHeight = 90;
        
        // Keyboard mapping
        this.keyMap = {
            'KeyZ': 60, 'KeyS': 61, 'KeyX': 62, 'KeyD': 63, 'KeyC': 64,
            'KeyV': 65, 'KeyG': 66, 'KeyB': 67, 'KeyH': 68, 'KeyN': 69,
            'KeyJ': 70, 'KeyM': 71, 'Comma': 72, 'KeyL': 73, 'Period': 74
        };
    }
    
    buildTemplate(data = {}) {
        const state = { ...this.viewState, ...data };
        
        return `
            <div class="keyboard-view">
                <div class="keyboard-header">
                    <h2>🎹 Clavier MIDI</h2>
                    <div class="keyboard-controls">
                        <select class="device-select" data-action="select-device">
                            <option value="">-- Sélectionner device --</option>
                            ${state.devices.map(d => `
                                <option value="${d.id}" ${state.selectedDevice?.id === d.id ? 'selected' : ''}>
                                    ${d.name}
                                </option>
                            `).join('')}
                        </select>
                        
                        <label>Vélocité: <input type="range" min="1" max="127" 
                               value="${state.velocity}" data-action="set-velocity" />
                               <span>${state.velocity}</span></label>
                        
                        <div class="octave-controls">
                            <button data-action="octave-down">◄</button>
                            <span>Octave: ${state.octaveOffset}</span>
                            <button data-action="octave-up">►</button>
                        </div>
                    </div>
                </div>
                
                <div class="keyboard-canvas-container">
                    <canvas id="keyboardCanvas" class="keyboard-canvas"></canvas>
                </div>
                
                <div class="keyboard-info">
                    <span>${state.activeNotes.size} notes actives</span>
                    <span>Utiliser touches ZXCVBNM pour jouer</span>
                </div>
            </div>
        `;
    }
    
    attachEvents() {
        super.attachEvents();
        
        if (!this.container) return;
        
        // Controls
        this.container.addEventListener('change', (e) => {
            const action = e.target.dataset.action;
            
            if (action === 'select-device') {
                const deviceId = e.target.value;
                this.viewState.selectedDevice = this.viewState.devices.find(d => d.id === deviceId);
            } else if (action === 'set-velocity') {
                this.viewState.velocity = parseInt(e.target.value);
                e.target.nextElementSibling.textContent = this.viewState.velocity;
            }
        });
        
        this.container.addEventListener('click', (e) => {
            const action = e.target.dataset.action;
            
            if (action === 'octave-up') {
                this.viewState.octaveOffset++;
                this.render();
            } else if (action === 'octave-down') {
                this.viewState.octaveOffset--;
                this.render();
            }
        });
        
        // Canvas events
        this.canvas = document.getElementById('keyboardCanvas');
        if (this.canvas) {
            this.ctx = this.canvas.getContext('2d');
            this.resizeCanvas();
            this.drawKeyboard();
            
            this.canvas.addEventListener('mousedown', (e) => this.handleMouseDown(e));
            this.canvas.addEventListener('mouseup', (e) => this.handleMouseUp(e));
            this.canvas.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        }
        
        // Computer keyboard
        window.addEventListener('keydown', (e) => this.handleKeyDown(e));
        window.addEventListener('keyup', (e) => this.handleKeyUp(e));
        
        this.setupEventBusListeners();
    }
    
    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        this.eventBus.on('devices:listed', (data) => {
            this.viewState.devices = (data.devices || []).filter(d => d.status === 2);
            this.render();
        });
        
        this.eventBus.on('midi:note_on', (data) => {
            this.viewState.activeNotes.add(data.note);
            this.drawKeyboard();
        });
        
        this.eventBus.on('midi:note_off', (data) => {
            this.viewState.activeNotes.delete(data.note);
            this.drawKeyboard();
        });
    }
    
    // ========================================================================
    // CANVAS
    // ========================================================================
    
    resizeCanvas() {
        if (!this.canvas) return;
        
        const container = this.canvas.parentElement;
        this.canvas.width = container.clientWidth;
        this.canvas.height = this.whiteKeyHeight;
    }
    
    drawKeyboard() {
        if (!this.ctx) return;
        
        const ctx = this.ctx;
        const width = this.canvas.width;
        const height = this.canvas.height;
        
        ctx.clearRect(0, 0, width, height);
        
        const startNote = Math.max(21, 60 + this.viewState.octaveOffset * 12 - 12);
        const endNote = Math.min(108, startNote + 24);
        
        // Dessiner touches blanches
        let x = 0;
        for (let note = startNote; note <= endNote; note++) {
            if (this.isWhiteKey(note)) {
                this.drawWhiteKey(x, note);
                x += this.keyWidth;
            }
        }
        
        // Dessiner touches noires
        x = 0;
        for (let note = startNote; note <= endNote; note++) {
            if (this.isWhiteKey(note)) {
                if (this.hasBlackKey(note)) {
                    this.drawBlackKey(x + this.keyWidth * 0.7, note + 1);
                }
                x += this.keyWidth;
            }
        }
    }
    
    drawWhiteKey(x, note) {
        const ctx = this.ctx;
        const isActive = this.viewState.activeNotes.has(note);
        
        ctx.fillStyle = isActive ? '#4a9eff' : '#ffffff';
        ctx.fillRect(x, 0, this.keyWidth - 1, this.whiteKeyHeight);
        ctx.strokeStyle = '#333';
        ctx.strokeRect(x, 0, this.keyWidth - 1, this.whiteKeyHeight);
    }
    
    drawBlackKey(x, note) {
        const ctx = this.ctx;
        const isActive = this.viewState.activeNotes.has(note);
        
        ctx.fillStyle = isActive ? '#4a9eff' : '#000000';
        ctx.fillRect(x, 0, this.keyWidth * 0.6, this.blackKeyHeight);
    }
    
    isWhiteKey(note) {
        const noteInOctave = note % 12;
        return [0, 2, 4, 5, 7, 9, 11].includes(noteInOctave);
    }
    
    hasBlackKey(note) {
        const noteInOctave = note % 12;
        return [0, 2, 5, 7, 9].includes(noteInOctave);
    }
    
    getNoteAtPosition(x, y) {
        const startNote = 60 + this.viewState.octaveOffset * 12 - 12;
        
        // Check black keys first
        if (y < this.blackKeyHeight) {
            let keyX = 0;
            for (let note = startNote; note <= startNote + 24; note++) {
                if (this.isWhiteKey(note)) {
                    if (this.hasBlackKey(note)) {
                        const blackX = keyX + this.keyWidth * 0.7;
                        if (x >= blackX && x < blackX + this.keyWidth * 0.6) {
                            return note + 1;
                        }
                    }
                    keyX += this.keyWidth;
                }
            }
        }
        
        // White keys
        const keyIndex = Math.floor(x / this.keyWidth);
        let whiteKeyCount = 0;
        for (let note = startNote; note <= startNote + 24; note++) {
            if (this.isWhiteKey(note)) {
                if (whiteKeyCount === keyIndex) return note;
                whiteKeyCount++;
            }
        }
        
        return null;
    }
    
    // ========================================================================
    // INTERACTION
    // ========================================================================
    
    handleMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        const note = this.getNoteAtPosition(x, y);
        
        if (note !== null) {
            this.playNote(note, this.viewState.velocity);
        }
    }
    
    handleMouseUp(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        const note = this.getNoteAtPosition(x, y);
        
        if (note !== null) {
            this.stopNote(note);
        }
    }
    
    handleMouseMove(e) {
        // Optionnel: highlight key
    }
    
    handleKeyDown(e) {
        if (e.repeat) return;
        
        const note = this.keyMap[e.code];
        if (note !== undefined) {
            e.preventDefault();
            const adjustedNote = note + this.viewState.octaveOffset * 12;
            this.playNote(adjustedNote, this.viewState.velocity);
        }
    }
    
    handleKeyUp(e) {
        const note = this.keyMap[e.code];
        if (note !== undefined) {
            e.preventDefault();
            const adjustedNote = note + this.viewState.octaveOffset * 12;
            this.stopNote(adjustedNote);
        }
    }
    
    // ========================================================================
    // MIDI
    // ========================================================================
    
    playNote(note, velocity) {
        if (!this.viewState.selectedDevice) return;
        
        // API: midi.sendNoteOn
        if (this.eventBus) {
            this.eventBus.emit('midi:send_note_on', {
                device_id: this.viewState.selectedDevice.id,
                note: note,
                velocity: velocity,
                channel: 0
            });
        }
        
        this.viewState.activeNotes.add(note);
        this.drawKeyboard();
    }
    
    stopNote(note) {
        if (!this.viewState.selectedDevice) return;
        
        // API: midi.sendNoteOff
        if (this.eventBus) {
            this.eventBus.emit('midi:send_note_off', {
                device_id: this.viewState.selectedDevice.id,
                note: note,
                velocity: 0,
                channel: 0
            });
        }
        
        this.viewState.activeNotes.delete(note);
        this.drawKeyboard();
    }
    
    // ========================================================================
    // INIT
    // ========================================================================
    
    init() {
        super.init();
        
        // Charger devices
        if (this.eventBus) {
            this.eventBus.emit('devices:list_requested');
        }
    }
    
    destroy() {
        window.removeEventListener('keydown', this.handleKeyDown);
        window.removeEventListener('keyup', this.handleKeyUp);
        super.destroy();
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = KeyboardView;
}
if (typeof window !== 'undefined') {
    window.KeyboardView = KeyboardView;
}
// ============================================================================
// Fichier: frontend/js/utils/MetronomeEngine.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Moteur de métronome audio pour l'éditeur et la lecture.
//   Click précis avec Web Audio API, tempo variable.
//
// Fonctionnalités:
//   - Click métronome (temps forts/faibles)
//   - Tempo variable (BPM)
//   - Time signature configurable (4/4, 3/4, etc.)
//   - Volume réglable
//   - Subdivision (1/4, 1/8, 1/16)
//   - Sons personnalisables (beep, wood, clap)
//   - Synchronisation avec playback
//
// Architecture:
//   MetronomeEngine (classe)
//   - Web Audio API (OscillatorNode)
//   - Scheduling précis (lookahead)
//   - Compensation latence
//
// Auteur: MidiMind Team
// ============================================================================

class KeyboardView {
    constructor(container) {
        this.container = container;
        this.currentLayout = 'chromatic';
        this.currentInstrument = null;
        this.pads = [];
        this.activePads = new Set();
        
        // Configuration
        this.scale = 'major';
        this.tonic = 60; // C4
        this.octaveOffset = 0;
    }

    /**
     * Initialise la vue
     */
    init(instrument) {
        this.currentInstrument = instrument;
        this.currentLayout = this.detectOptimalLayout(instrument);
        this.render();
        this.attachEvents();
    }

    /**
     * Détecte le layout optimal selon l'instrument
     */
    detectOptimalLayout(instrument) {
        if (!instrument) return 'chromatic';

        switch (instrument.type) {
            case 'percussion':
                return 'drum-grid';
            case 'melodic':
                return 'scale-linear';
            case 'chromatic':
                return 'piano-roll';
            default:
                return 'custom';
        }
    }

    /**
     * Rend le clavier
     */
    render() {
        let layoutHtml = '';

        switch (this.currentLayout) {
            case 'drum-grid':
                layoutHtml = this.buildDrumGrid();
                break;
            case 'scale-linear':
                layoutHtml = this.buildScaleLinear();
                break;
            case 'piano-roll':
                layoutHtml = this.buildPianoRoll();
                break;
            case 'custom':
                layoutHtml = this.buildCustomLayout();
                break;
        }

        this.container.innerHTML = `
            <div class="keyboard-container">
                <div class="keyboard-header">
                    <div class="layout-selector">
                        <label>Layout:</label>
                        <select id="layoutSelect" onchange="keyboardView.changeLayout(this.value)">
                            <option value="drum-grid" ${this.currentLayout === 'drum-grid' ? 'selected' : ''}>Drum Grid</option>
                            <option value="scale-linear" ${this.currentLayout === 'scale-linear' ? 'selected' : ''}>Scale</option>
                            <option value="piano-roll" ${this.currentLayout === 'piano-roll' ? 'selected' : ''}>Piano</option>
                            <option value="custom" ${this.currentLayout === 'custom' ? 'selected' : ''}>Custom</option>
                        </select>
                    </div>
                    
                    <div class="keyboard-controls">
                        <label>
                            <input type="checkbox" id="velocitySensitive" checked>
                            Velocity Sensitive
                        </label>
                        <label>
                            Fixed Vel:
                            <input type="range" id="fixedVelocity" min="1" max="127" value="64" disabled>
                            <span id="velocityValue">64</span>
                        </label>
                    </div>
                </div>
                
                ${layoutHtml}
            </div>
        `;
    }

    /**
     * Construit le layout drum grid
     */
    buildDrumGrid() {
        if (!this.currentInstrument) return '<div class="empty-keyboard">No instrument selected</div>';

        const notes = this.currentInstrument.notes;
        const gridSize = this.calculateGridSize(notes.length);

        return `
            <div class="keyboard-drum-grid" 
                 style="grid-template-columns: repeat(${gridSize.cols}, 1fr);
                        grid-template-rows: repeat(${gridSize.rows}, 1fr);">
                ${notes.map((note, index) => `
                    <div class="drum-pad" 
                         data-note="${note}"
                         data-index="${index}"
                         style="background: ${this.getPadColor(note)}">
                        <span class="pad-label">
                            ${this.currentInstrument.noteNames?.[note] || this.getNoteName(note)}
                        </span>
                        <span class="pad-note">MIDI ${note}</span>
                    </div>
                `).join('')}
            </div>
        `;
    }

    /**
     * Calcule la taille de grille optimale
     */
    calculateGridSize(count) {
        if (count <= 8) return { cols: 4, rows: 2 };
        if (count <= 16) return { cols: 4, rows: 4 };
        if (count <= 32) return { cols: 8, rows: 4 };
        return { cols: 8, rows: 8 };
    }

    /**
     * Construit le layout mélodique
     */
    buildScaleLinear() {
        const scalePattern = this.getScalePattern(this.scale);
        const notes = this.generateScaleNotes(this.tonic + (this.octaveOffset * 12), scalePattern, 2);

        return `
            <div class="keyboard-scale-linear">
                <div class="scale-controls">
                    <select id="scaleSelect" onchange="keyboardView.changeScale(this.value)">
                        <option value="major" ${this.scale === 'major' ? 'selected' : ''}>Major</option>
                        <option value="minor" ${this.scale === 'minor' ? 'selected' : ''}>Minor</option>
                        <option value="pentatonic" ${this.scale === 'pentatonic' ? 'selected' : ''}>Pentatonic</option>
                        <option value="blues" ${this.scale === 'blues' ? 'selected' : ''}>Blues</option>
                        <option value="dorian" ${this.scale === 'dorian' ? 'selected' : ''}>Dorian</option>
                        <option value="mixolydian" ${this.scale === 'mixolydian' ? 'selected' : ''}>Mixolydian</option>
                    </select>
                    
                    <select id="tonicSelect" onchange="keyboardView.changeTonic(parseInt(this.value))">
                        ${this.buildTonicOptions()}
                    </select>
                    
                    <button onclick="keyboardView.transposeOctave(-1)">Oct ↓</button>
                    <button onclick="keyboardView.transposeOctave(1)">Oct ↑</button>
                </div>
                
                <div class="scale-pads">
                    ${notes.map((note, index) => {
                        const baseTonic = this.tonic % 12;
                        const isTonic = (note % 12) === baseTonic;
                        return `
                            <div class="scale-pad ${isTonic ? 'tonic' : ''}"
                                 data-note="${note}"
                                 data-degree="${index % scalePattern.length}">
                                <span class="note-name">${this.getNoteName(note)}</span>
                                <span class="degree">${this.getScaleDegree(index % scalePattern.length)}</span>
                            </div>
                        `;
                    }).join('')}
                </div>
            </div>
        `;
    }

    /**
     * Construit le layout piano roll
     */
    buildPianoRoll() {
        const startNote = 48 + (this.octaveOffset * 12); // C3 par défaut
        const octaves = 2;
        const notes = [];

        for (let i = 0; i < octaves * 12; i++) {
            notes.push(startNote + i);
        }

        return `
            <div class="keyboard-piano-roll">
                <div class="piano-controls">
                    <button onclick="keyboardView.transposeOctave(-1)">Octave ↓</button>
                    <span>Octave: ${Math.floor(startNote / 12) - 1}</span>
                    <button onclick="keyboardView.transposeOctave(1)">Octave ↑</button>
                </div>
                
                <div class="piano-keys">
                    ${notes.map(note => {
                        const noteName = this.getNoteName(note);
                        const isBlack = noteName.includes('#');
                        return `
                            <div class="piano-key ${isBlack ? 'black' : 'white'}"
                                 data-note="${note}">
                                <span class="key-label">${noteName}</span>
                            </div>
                        `;
                    }).join('')}
                </div>
            </div>
        `;
    }

    /**
     * Construit le layout custom
     */
    buildCustomLayout() {
        return `
            <div class="keyboard-custom">
                <div class="custom-info">
                    <h3>Custom Layout</h3>
                    <p>Create your own pad layout</p>
                    <button onclick="keyboardView.openCustomEditor()">Edit Layout</button>
                </div>
            </div>
        `;
    }

    /**
     * Patterns de gammes
     */
    getScalePattern(scale) {
        const patterns = {
            'major': [0, 2, 4, 5, 7, 9, 11],
            'minor': [0, 2, 3, 5, 7, 8, 10],
            'pentatonic': [0, 2, 4, 7, 9],
            'blues': [0, 3, 5, 6, 7, 10],
            'dorian': [0, 2, 3, 5, 7, 9, 10],
            'mixolydian': [0, 2, 4, 5, 7, 9, 10]
        };
        return patterns[scale] || patterns.major;
    }

    /**
     * Génère les notes d'une gamme
     */
    generateScaleNotes(tonic, pattern, octaves) {
        const notes = [];
        
        for (let octave = 0; octave < octaves; octave++) {
            pattern.forEach(interval => {
                const note = tonic + interval + (octave * 12);
                if (note >= 0 && note <= 127) {
                    notes.push(note);
                }
            });
        }
        
        return notes;
    }

    /**
     * Options de tonique
     */
    buildTonicOptions() {
        const notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        return notes.map((name, index) => {
            const note = 60 + index; // C4 = 60
            return `<option value="${note}" ${this.tonic === note ? 'selected' : ''}>${name}</option>`;
        }).join('');
    }

    /**
     * Degré de gamme
     */
    getScaleDegree(index) {
        const degrees = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII'];
        return degrees[index] || '';
    }

    /**
     * Attache les événements
     */
    attachEvents() {
        // Pads/touches
        this.container.querySelectorAll('[data-note]').forEach(element => {
            // Mouse/Touch events
            element.addEventListener('mousedown', (e) => this.handlePadDown(e));
            element.addEventListener('touchstart', (e) => this.handlePadDown(e));
            
            element.addEventListener('mouseup', (e) => this.handlePadUp(e));
            element.addEventListener('touchend', (e) => this.handlePadUp(e));
            
            element.addEventListener('mouseleave', (e) => this.handlePadUp(e));
        });

        // Velocity sensitivity
        document.getElementById('velocitySensitive')?.addEventListener('change', (e) => {
            document.getElementById('fixedVelocity').disabled = e.target.checked;
        });

        document.getElementById('fixedVelocity')?.addEventListener('input', (e) => {
            document.getElementById('velocityValue').textContent = e.target.value;
        });

        // Clavier QWERTY mapping (optionnel)
        document.addEventListener('keydown', (e) => this.handleKeyDown(e));
        document.addEventListener('keyup', (e) => this.handleKeyUp(e));
    }

    /**
     * Gestion pad down
     */
    handlePadDown(e) {
        e.preventDefault();
        
        const pad = e.currentTarget;
        const note = parseInt(pad.dataset.note);
        
        if (this.activePads.has(note)) return;

        const velocity = this.calculateVelocity(e);
        
        pad.classList.add('active');
        this.activePads.add(note);

        // Émettre l'événement
        this.emitNoteOn(note, velocity);
    }

    /**
     * Gestion pad up
     */
    handlePadUp(e) {
        const pad = e.currentTarget;
        const note = parseInt(pad.dataset.note);

        if (!this.activePads.has(note)) return;

        pad.classList.remove('active');
        this.activePads.delete(note);

        // Émettre l'événement
        this.emitNoteOff(note);
    }

    /**
     * Calcule la vélocité
     */
    calculateVelocity(e) {
        const velocitySensitive = document.getElementById('velocitySensitive')?.checked;

        if (!velocitySensitive) {
            return parseInt(document.getElementById('fixedVelocity')?.value || 64);
        }

        // Calculer selon la position du touch/clic
        const rect = e.currentTarget.getBoundingClientRect();
        const y = (e.clientY || e.touches?.[0]?.clientY || 0) - rect.top;
        const percentage = 1 - (y / rect.height);
        
        return Math.round(Math.max(1, Math.min(127, percentage * 127)));
    }

    /**
     * Émet un Note On
     */
    emitNoteOn(note, velocity) {
        const event = {
            type: 'noteOn',
            note: note,
            velocity: velocity,
            time: Date.now()
        };

        window.eventBus?.emit('keyboard:note', event);
    }

    /**
     * Émet un Note Off
     */
    emitNoteOff(note) {
        const event = {
            type: 'noteOff',
            note: note,
            velocity: 0,
            time: Date.now()
        };

        window.eventBus?.emit('keyboard:note', event);
    }

    /**
     * Change de layout
     */
    changeLayout(layout) {
        this.currentLayout = layout;
        this.render();
        this.attachEvents();
    }

    /**
     * Change de gamme
     */
    changeScale(scale) {
        this.scale = scale;
        this.render();
        this.attachEvents();
    }

    /**
     * Change de tonique
     */
    changeTonic(tonic) {
        this.tonic = tonic;
        this.render();
        this.attachEvents();
    }

    /**
     * Transpose l'octave
     */
    transposeOctave(direction) {
        this.octaveOffset += direction;
        this.render();
        this.attachEvents();
    }

    /**
     * Gestion touche clavier down
     */
    handleKeyDown(e) {
        // Mapping QWERTY basique
        const keyMap = {
            'a': 60, 'w': 61, 's': 62, 'e': 63, 'd': 64,
            'f': 65, 't': 66, 'g': 67, 'y': 68, 'h': 69,
            'u': 70, 'j': 71, 'k': 72, 'o': 73, 'l': 74
        };

        const note = keyMap[e.key.toLowerCase()];
        if (note && !this.activePads.has(note)) {
            this.activePads.add(note);
            this.emitNoteOn(note, 64);
            
            // Visual feedback
            const pad = this.container.querySelector(`[data-note="${note}"]`);
            if (pad) pad.classList.add('active');
        }
    }

    /**
     * Gestion touche clavier up
     */
    handleKeyUp(e) {
        const keyMap = {
            'a': 60, 'w': 61, 's': 62, 'e': 63, 'd': 64,
            'f': 65, 't': 66, 'g': 67, 'y': 68, 'h': 69,
            'u': 70, 'j': 71, 'k': 72, 'o': 73, 'l': 74
        };

        const note = keyMap[e.key.toLowerCase()];
        if (note && this.activePads.has(note)) {
            this.activePads.delete(note);
            this.emitNoteOff(note);
            
            // Visual feedback
            const pad = this.container.querySelector(`[data-note="${note}"]`);
            if (pad) pad.classList.remove('active');
        }
    }

    /**
     * Couleur de pad
     */
    getPadColor(note) {
        const hue = (note * 137.5) % 360;
        return `hsl(${hue}, 60%, 50%)`;
    }

    /**
     * Nom de note
     */
    getNoteName(midiNote) {
        const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        return names[midiNote % 12];
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = KeyboardView;
}
window.MetronomeEngine = MetronomeEngine;
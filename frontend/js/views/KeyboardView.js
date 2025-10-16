// ============================================================================
// Fichier: frontend/js/views/KeyboardView.js
// Version: v3.1.0 - MONITOR MODE
// Date: 2025-10-16
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// MODIFICATIONS v3.1.0:
// ✓ Mode monitor activé (affichage + playback uniquement)
// ✓ Warning si enregistrement désactivé
// ✓ Support note mapping personnalisé
// ✓ Affichage clavier selon disponibilité notes
// ✓ SUPPRESSION : UI enregistrement + loops
// ============================================================================

class KeyboardView extends BaseView {
    constructor(container, eventBus, debugConsole) {
        super(container, eventBus, debugConsole, {
            name: 'KeyboardView',
            autoRender: false
        });
        
        // Canvas clavier
        this.canvas = null;
        this.ctx = null;
        
        // État
        this.selectedInstrument = null;
        this.noteRange = { min: 21, max: 108 };
        this.customNoteMapping = null;
        this.activeNotes = new Set();
        this.currentVelocity = 100;
        this.devices = [];
        
        // Configuration affichage
        this.keyWidth = 20;
        this.whiteKeyHeight = 120;
        this.blackKeyHeight = 80;
        
        this.logDebug('keyboard', '✓ KeyboardView initialized (monitor mode)');
    }
    
    // ========================================================================
    // RENDERING PRINCIPAL
    // ========================================================================
    
    render() {
        if (!this.container) return;
        
        const html = `
            <div class="keyboard-page">
                <!-- Header -->
                <div class="page-header">
                    <h2>🎹 Clavier MIDI</h2>
                    <p class="page-description">
                        Jouez des notes MIDI avec votre clavier ou souris
                    </p>
                </div>
                
                ${this.renderWarningBanner()}
                
                <!-- Controls -->
                <div class="keyboard-controls">
                    ${this.renderInstrumentSelector()}
                    ${this.renderNoteRangeDisplay()}
                    ${this.renderVelocityControl()}
                    ${this.renderActions()}
                </div>
                
                <!-- Canvas clavier -->
                <div class="keyboard-canvas-wrapper">
                    <canvas id="keyboard-canvas" class="keyboard-canvas"></canvas>
                </div>
                
                ${this.renderLegend()}
                
                ${this.renderKeyboardMapping()}
                
                ${this.renderStats()}
            </div>
        `;
        
        this.container.innerHTML = html;
        
        // Initialiser canvas
        this.initCanvas();
        
        // Attacher listeners
        this.attachEventListeners();
        
        // Premier rendu
        this.drawKeyboard();
    }
    
    // ========================================================================
    // COMPOSANTS UI
    // ========================================================================
    
    renderWarningBanner() {
        if (PerformanceConfig.keyboard.enableRecording) {
            return '';  // Pas de warning si enregistrement activé
        }
        
        return `
            <div class="info-banner warning">
                ℹ️ Mode Monitor : Affichage et lecture uniquement
                (Enregistrement désactivé en mode performance)
            </div>
        `;
    }
    
    renderInstrumentSelector() {
        return `
            <div class="control-group">
                <label>Instrument</label>
                <select id="instrument-select" class="instrument-select">
                    <option value="">-- Sélectionner instrument --</option>
                    ${this.devices.map(device => `
                        <option value="${device.id}" 
                                ${device.id === this.selectedInstrument ? 'selected' : ''}>
                            ${device.name || device.id}
                        </option>
                    `).join('')}
                </select>
            </div>
        `;
    }
    
    renderNoteRangeDisplay() {
        let rangeText = `${this.noteRange.min}-${this.noteRange.max}`;
        
        if (this.customNoteMapping) {
            rangeText += ` (${this.customNoteMapping.size} notes mappées)`;
        }
        
        return `
            <div class="control-group">
                <label>Notes disponibles</label>
                <div class="note-range-display">
                    <span class="note-range-value">${rangeText}</span>
                </div>
            </div>
        `;
    }
    
    renderVelocityControl() {
        if (!PerformanceConfig.keyboard.enablePlayback) {
            return '';
        }
        
        return `
            <div class="control-group">
                <label>Vélocité : <span id="velocity-value">${this.currentVelocity}</span></label>
                <input type="range" 
                       id="velocity-slider" 
                       class="velocity-slider"
                       min="1" 
                       max="127" 
                       value="${this.currentVelocity}">
                <div class="velocity-presets">
                    <button class="btn-preset" data-velocity="32">pp</button>
                    <button class="btn-preset" data-velocity="64">mf</button>
                    <button class="btn-preset" data-velocity="96">f</button>
                    <button class="btn-preset" data-velocity="127">ff</button>
                </div>
            </div>
        `;
    }
    
    renderActions() {
        return `
            <div class="control-group actions">
                <button class="btn btn-secondary" id="btn-panic">
                    🛑 Panic (Stop All)
                </button>
                <button class="btn btn-secondary" id="btn-refresh-devices">
                    🔄 Rafraîchir instruments
                </button>
            </div>
        `;
    }
    
    renderLegend() {
        return `
            <div class="keyboard-legend">
                <div class="legend-item">
                    <div class="legend-color" style="background: #4ECDC4;"></div>
                    <span>Note disponible</span>
                </div>
                <div class="legend-item">
                    <div class="legend-color" style="background: #FF6B6B;"></div>
                    <span>Note active</span>
                </div>
                <div class="legend-item">
                    <div class="legend-color" style="background: #95A5A6;"></div>
                    <span>Hors range instrument</span>
                </div>
                ${this.customNoteMapping ? `
                    <div class="legend-item">
                        <div class="legend-color" style="background: #FFD93D;"></div>
                        <span>Note mappée personnalisée</span>
                    </div>
                ` : ''}
            </div>
        `;
    }
    
    renderKeyboardMapping() {
        return `
            <div class="keyboard-mapping-info">
                <h4>💡 Raccourcis clavier</h4>
                <p>Utilisez les touches <kbd>A</kbd> à <kbd>]</kbd> pour jouer (2 octaves)</p>
                <div class="key-hints">
                    <span><kbd>A</kbd>=C4</span>
                    <span><kbd>S</kbd>=D4</span>
                    <span><kbd>D</kbd>=E4</span>
                    <span><kbd>F</kbd>=F4</span>
                    <span><kbd>G</kbd>=G4</span>
                    <span>...</span>
                    <span><kbd>K</kbd>=C5</span>
                </div>
            </div>
        `;
    }
    
    renderStats() {
        return `
            <div class="keyboard-stats">
                <div class="stat-item">
                    <span class="stat-label">Notes jouées</span>
                    <span class="stat-value" id="stat-notes-played">0</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Notes actives</span>
                    <span class="stat-value" id="stat-active-notes">0</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Erreurs</span>
                    <span class="stat-value" id="stat-errors">0</span>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // CANVAS INITIALIZATION
    // ========================================================================
    
    initCanvas() {
        this.canvas = this.container.querySelector('#keyboard-canvas');
        
        if (!this.canvas) {
            this.logDebug('keyboard', 'Canvas not found', 'error');
            return;
        }
        
        this.ctx = this.canvas.getContext('2d');
        
        // Taille canvas
        this.resizeCanvas();
        
        // Listeners canvas
        this.attachCanvasListeners();
        
        // Resize observer
        const resizeObserver = new ResizeObserver(() => {
            this.resizeCanvas();
            this.drawKeyboard();
        });
        
        resizeObserver.observe(this.canvas.parentElement);
    }
    
    resizeCanvas() {
        const wrapper = this.canvas.parentElement;
        const rect = wrapper.getBoundingClientRect();
        
        this.canvas.width = rect.width;
        this.canvas.height = this.whiteKeyHeight + 40;  // +40 pour labels
        
        this.logDebug('keyboard', `Canvas resized: ${this.canvas.width}x${this.canvas.height}`);
    }
    
    attachCanvasListeners() {
        let mouseDown = false;
        let lastNote = null;
        
        this.canvas.addEventListener('mousedown', (e) => {
            mouseDown = true;
            const note = this.getNoteAtPosition(e.offsetX, e.offsetY);
            
            if (note !== null) {
                lastNote = note;
                this.eventBus.emit('keyboard:play-note', { note });
            }
        });
        
        this.canvas.addEventListener('mousemove', (e) => {
            if (!mouseDown) return;
            
            const note = this.getNoteAtPosition(e.offsetX, e.offsetY);
            
            if (note !== null && note !== lastNote) {
                // Arrêter note précédente
                if (lastNote !== null) {
                    this.eventBus.emit('keyboard:stop-note', { note: lastNote });
                }
                
                // Jouer nouvelle note
                this.eventBus.emit('keyboard:play-note', { note });
                lastNote = note;
            }
        });
        
        this.canvas.addEventListener('mouseup', (e) => {
            if (lastNote !== null) {
                this.eventBus.emit('keyboard:stop-note', { note: lastNote });
                lastNote = null;
            }
            mouseDown = false;
        });
        
        this.canvas.addEventListener('mouseleave', (e) => {
            if (mouseDown && lastNote !== null) {
                this.eventBus.emit('keyboard:stop-note', { note: lastNote });
                lastNote = null;
            }
            mouseDown = false;
        });
    }
    
    // ========================================================================
    // EVENT LISTENERS
    // ========================================================================
    
    attachEventListeners() {
        // Instrument selector
        const instrumentSelect = this.container.querySelector('#instrument-select');
        if (instrumentSelect) {
            instrumentSelect.addEventListener('change', (e) => {
                this.eventBus.emit('keyboard:select-instrument', {
                    instrumentId: e.target.value
                });
            });
        }
        
        // Velocity slider
        const velocitySlider = this.container.querySelector('#velocity-slider');
        const velocityValue = this.container.querySelector('#velocity-value');
        
        if (velocitySlider && velocityValue) {
            velocitySlider.addEventListener('input', (e) => {
                const velocity = parseInt(e.target.value);
                velocityValue.textContent = velocity;
                this.currentVelocity = velocity;
                
                this.eventBus.emit('keyboard:velocity-changed', { velocity });
            });
        }
        
        // Velocity presets
        this.container.querySelectorAll('.btn-preset').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const velocity = parseInt(e.target.dataset.velocity);
                
                if (velocitySlider) velocitySlider.value = velocity;
                if (velocityValue) velocityValue.textContent = velocity;
                
                this.currentVelocity = velocity;
                this.eventBus.emit('keyboard:velocity-changed', { velocity });
            });
        });
        
        // Panic button
        const panicBtn = this.container.querySelector('#btn-panic');
        if (panicBtn) {
            panicBtn.addEventListener('click', () => {
                this.eventBus.emit('keyboard:panic');
            });
        }
        
        // Refresh devices
        const refreshBtn = this.container.querySelector('#btn-refresh-devices');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.eventBus.emit('keyboard:refresh-devices');
            });
        }
    }
    
    // ========================================================================
    // DRAWING KEYBOARD
    // ========================================================================
    
    drawKeyboard() {
        if (!this.ctx) return;
        
        // Clear
        this.ctx.fillStyle = '#1a1a1a';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        
        // Calculer nombre de touches visibles
        const numKeys = this.noteRange.max - this.noteRange.min + 1;
        const totalWidth = this.canvas.width;
        const whiteKeyWidth = totalWidth / (numKeys * 0.6);  // Approximation
        
        this.keyWidth = Math.max(15, Math.min(30, whiteKeyWidth));
        
        // Dessiner touches blanches d'abord
        for (let note = this.noteRange.min; note <= this.noteRange.max; note++) {
            if (!this.isBlackKey(note)) {
                this.drawKey(note);
            }
        }
        
        // Puis touches noires (par-dessus)
        for (let note = this.noteRange.min; note <= this.noteRange.max; note++) {
            if (this.isBlackKey(note)) {
                this.drawKey(note);
            }
        }
    }
    
    drawKey(note) {
        const isBlack = this.isBlackKey(note);
        const x = this.getNoteX(note);
        const y = 0;
        const width = this.keyWidth;
        const height = isBlack ? this.blackKeyHeight : this.whiteKeyHeight;
        
        // Couleur selon état
        let color;
        
        if (this.activeNotes.has(note)) {
            color = '#FF6B6B';  // Rouge = actif
        } else if (this.customNoteMapping && this.customNoteMapping.has(note)) {
            color = '#FFD93D';  // Jaune = mappé
        } else if (this.isNotePlayable(note)) {
            color = isBlack ? '#2c3e50' : '#ecf0f1';  // Disponible
        } else {
            color = '#95A5A6';  // Gris = hors range
        }
        
        // Dessiner touche
        this.ctx.fillStyle = color;
        this.ctx.fillRect(x, y, width, height);
        
        // Bordure
        this.ctx.strokeStyle = '#34495e';
        this.ctx.lineWidth = 1;
        this.ctx.strokeRect(x, y, width, height);
        
        // Label (seulement Do)
        if (!isBlack && note % 12 === 0) {
            this.ctx.fillStyle = '#2c3e50';
            this.ctx.font = '10px sans-serif';
            this.ctx.textAlign = 'center';
            this.ctx.fillText(this.getNoteLabel(note), x + width / 2, height + 15);
        }
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    isBlackKey(note) {
        const noteInOctave = note % 12;
        return [1, 3, 6, 8, 10].includes(noteInOctave);  // C#, D#, F#, G#, A#
    }
    
    getNoteX(note) {
        // Calculer position X selon note
        let x = 0;
        const whiteKeyCount = this.countWhiteKeysBefore(note);
        
        x = whiteKeyCount * this.keyWidth;
        
        // Offset pour touches noires
        if (this.isBlackKey(note)) {
            x -= this.keyWidth / 2;
        }
        
        return x;
    }
    
    countWhiteKeysBefore(note) {
        let count = 0;
        
        for (let n = this.noteRange.min; n < note; n++) {
            if (!this.isBlackKey(n)) {
                count++;
            }
        }
        
        return count;
    }
    
    getNoteAtPosition(x, y) {
        // Trouver note sous le curseur
        
        // Tester touches noires d'abord (plus petites, par-dessus)
        for (let note = this.noteRange.min; note <= this.noteRange.max; note++) {
            if (!this.isBlackKey(note)) continue;
            
            const noteX = this.getNoteX(note);
            const width = this.keyWidth;
            const height = this.blackKeyHeight;
            
            if (x >= noteX && x <= noteX + width && y >= 0 && y <= height) {
                return note;
            }
        }
        
        // Puis touches blanches
        for (let note = this.noteRange.min; note <= this.noteRange.max; note++) {
            if (this.isBlackKey(note)) continue;
            
            const noteX = this.getNoteX(note);
            const width = this.keyWidth;
            const height = this.whiteKeyHeight;
            
            if (x >= noteX && x <= noteX + width && y >= 0 && y <= height) {
                return note;
            }
        }
        
        return null;
    }
    
    getNoteLabel(note) {
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(note / 12) - 1;
        const noteName = noteNames[note % 12];
        return `${noteName}${octave}`;
    }
    
    isNotePlayable(note) {
        // Si mapping custom, vérifier si mappé
        if (this.customNoteMapping) {
            return this.customNoteMapping.has(note);
        }
        
        // Sinon, vérifier range
        return note >= this.noteRange.min && note <= this.noteRange.max;
    }
    
    // ========================================================================
    // UPDATE DATA
    // ========================================================================
    
    setInstrument(instrumentId, profile) {
        this.selectedInstrument = instrumentId;
        
        if (profile) {
            if (profile.min_note !== undefined && profile.max_note !== undefined) {
                this.noteRange = {
                    min: profile.min_note,
                    max: profile.max_note
                };
            }
            
            if (profile.note_mappings) {
                this.customNoteMapping = new Map();
                profile.note_mappings.forEach(mapping => {
                    this.customNoteMapping.set(mapping.midi_note, mapping);
                });
            } else {
                this.customNoteMapping = null;
            }
        }
        
        this.render();
    }
    
    setDevices(devices) {
        this.devices = devices || [];
        
        // Mettre à jour dropdown
        const select = this.container.querySelector('#instrument-select');
        if (select) {
            select.innerHTML = '<option value="">-- Sélectionner instrument --</option>';
            this.devices.forEach(device => {
                const option = document.createElement('option');
                option.value = device.id;
                option.textContent = device.name || device.id;
                option.selected = device.id === this.selectedInstrument;
                select.appendChild(option);
            });
        }
    }
    
    setActiveNote(note, active) {
        if (active) {
            this.activeNotes.add(note);
        } else {
            this.activeNotes.delete(note);
        }
        
        this.drawKeyboard();
        this.updateStats();
    }
    
    updateStats(stats) {
        if (!stats) return;
        
        this.updateStatValue('stat-notes-played', stats.notesPlayed);
        this.updateStatValue('stat-active-notes', stats.activeNotes);
        this.updateStatValue('stat-errors', stats.errors);
    }
    
    updateStatValue(id, value) {
        const elem = this.container.querySelector(`#${id}`);
        if (elem) {
            elem.textContent = value;
        }
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        this.activeNotes.clear();
        
        super.destroy();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = KeyboardView;
}

if (typeof window !== 'undefined') {
    window.KeyboardView = KeyboardView;
}

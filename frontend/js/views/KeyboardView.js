// ============================================================================
// Fichier: frontend/js/views/KeyboardView.js
// Version: v3.1.3 - FIXED INITIALIZE TIMING
// Date: 2025-10-24
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// CORRECTIONS v3.1.3:
// ✅ CRITIQUE: Override initialize() to init properties BEFORE render
// ✅ CRITIQUE: Fixed race condition - BaseView calls initialize() in constructor
// ✅ Properties initialized in initialize() if not already set
// ✅ Ensures stats, noteRange, devices exist before any render
// ============================================================================
// CORRECTIONS v3.1.2:
// ✅ CRITIQUE: Fixed constructor parameter order (BaseView only takes 2 params)
// ✅ CRITIQUE: Initialize all properties BEFORE super.initialize() is called
// ✅ CRITIQUE: Properly disable autoRender to prevent premature rendering
// ✅ Fixed initialization race condition causing "this.noteRange is undefined"
// ============================================================================
// CORRECTIONS v3.1.1:
// ✅ Fixed renderInstrumentSelector: added fallback for this.devices
// ✅ Prevents "can't access property 'map', this.devices is undefined" error
// ✅ Robust initialization with devices fallback to empty array
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
        // CRITIQUE: BaseView n'accepte que 2 paramètres (containerId, eventBus)
        // On ne peut PAS passer debugConsole ou config ici
        super(container, eventBus);
        
        // IMPORTANT: Désactiver autoRender immédiatement après super()
        // pour éviter que BaseView.initialize() n'appelle render() trop tôt
        this.config.autoRender = false;
        this.config.name = 'KeyboardView';
        this.config.preserveState = false;
        
        // Sauvegarder debugConsole si fourni
        this.debugConsole = debugConsole || null;
        
        // Canvas clavier
        this.canvas = null;
        this.ctx = null;
        
        // État - INITIALISER TOUTES LES PROPRIÉTÉS CRITIQUES ICI
        // pour éviter "undefined" lors du render
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
        
        // Stats pour tracking
        this.stats = {
            notesPlayed: 0,
            activeNotes: 0,
            errors: 0
        };
        
        // Log de débogage si disponible
        if (this.debugConsole && typeof this.debugConsole.log === 'function') {
            this.debugConsole.log('keyboard', '✓ KeyboardView initialized (monitor mode)');
        } else if (console && console.log) {
            console.log('[KeyboardView] ✓ Initialized (monitor mode)');
        }
    }
    
    // ========================================================================
    // LIFECYCLE - Surcharge pour contrôle précis de l'initialisation
    // ========================================================================
    
    /**
     * OVERRIDE de BaseView.initialize()
     * Cette méthode est appelée par BaseView constructor AVANT que notre
     * constructor ait fini. On doit donc s'assurer que tout est initialisé.
     */
    initialize() {
        // CRITIQUE: Initialiser les propriétés ICI si elles n'existent pas encore
        // car cette méthode est appelée PENDANT super() avant que notre constructor
        // ait pu les initialiser
        
        if (!this.stats) {
            this.stats = {
                notesPlayed: 0,
                activeNotes: 0,
                errors: 0
            };
        }
        
        if (!this.noteRange) {
            this.noteRange = { min: 21, max: 108 };
        }
        
        if (!this.devices) {
            this.devices = [];
        }
        
        if (!this.activeNotes) {
            this.activeNotes = new Set();
        }
        
        // S'assurer que autoRender est désactivé
        this.config.autoRender = false;
        
        // Appeler la méthode parent, mais maintenant tout est prêt
        // Note: on ne peut pas appeler super.initialize() car BaseView
        // va vérifier autoRender qui est maintenant false
        if (!this.container) {
            console.error(`[${this.config.name}] Container not found`);
            return;
        }
        
        // Hook personnalisé
        if (typeof this.onInitialize === 'function') {
            this.onInitialize();
        }
        
        // Ne PAS rendre automatiquement - le controller le fera
    }
    
    /**
     * Hook appelé par initialize() APRÈS que tout soit prêt
     */
    onInitialize() {
        // À ce stade, toutes les propriétés sont initialisées
        // On peut maintenant rendre en toute sécurité
        // (mais on attend que le controller appelle render() manuellement)
    }
    
    // ========================================================================
    // RENDERING PRINCIPAL
    // ========================================================================
    
    buildTemplate(data = {}) {
        return `
            <div class="keyboard-page">
                <div class="page-header">
                    <h2>🎹 Clavier MIDI</h2>
                    <p class="page-description">Jouez des notes MIDI avec votre clavier ou souris</p>
                </div>
                ${this.renderWarningBanner()}
                <div class="keyboard-controls">
                    ${this.renderInstrumentSelector()}
                    ${this.renderNoteRangeDisplay()}
                    ${this.renderVelocityControl()}
                    ${this.renderActions()}
                </div>
                <div class="keyboard-canvas-wrapper">
                    <canvas id="keyboard-canvas" class="keyboard-canvas"></canvas>
                </div>
                ${this.renderLegend()}
                ${this.renderKeyboardMapping()}
                ${this.renderStats()}
            </div>
        `;
    }

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
        // Vérifier que PerformanceConfig existe
        if (typeof PerformanceConfig === 'undefined' || 
            !PerformanceConfig.keyboard || 
            PerformanceConfig.keyboard.enableRecording) {
            return '';  // Pas de warning si enregistrement activé ou config absente
        }
        
        return `
            <div class="info-banner warning">
                ℹ️ Mode Monitor : Affichage et lecture uniquement
                (Enregistrement désactivé en mode performance)
            </div>
        `;
    }
    
    renderInstrumentSelector() {
        // Fallback robuste pour devices
        const devices = this.devices || [];
        
        return `
            <div class="control-group">
                <label>Instrument</label>
                <select id="instrument-select" class="instrument-select">
                    <option value="">-- Sélectionner instrument --</option>
                    ${devices.map(device => `
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
        // Vérifier que noteRange est défini (protection supplémentaire)
        if (!this.noteRange || typeof this.noteRange.min === 'undefined') {
            this.noteRange = { min: 21, max: 108 }; // Fallback sécurisé
        }
        
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
        // Vérifier PerformanceConfig
        if (typeof PerformanceConfig === 'undefined' || 
            !PerformanceConfig.keyboard || 
            !PerformanceConfig.keyboard.enablePlayback) {
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
                    <span class="stat-value" id="stat-notes-played">${this.stats.notesPlayed}</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Notes actives</span>
                    <span class="stat-value" id="stat-active-notes">${this.stats.activeNotes}</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Erreurs</span>
                    <span class="stat-value" id="stat-errors">${this.stats.errors}</span>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // CANVAS INITIALIZATION
    // ========================================================================
    
    initCanvas() {
        this.canvas = this.container.querySelector('#keyboard-canvas');
        if (!this.canvas) return;
        
        this.ctx = this.canvas.getContext('2d');
        
        // Dimensionner canvas selon clavier
        const whiteKeyCount = this.countWhiteKeys();
        this.canvas.width = whiteKeyCount * this.keyWidth;
        this.canvas.height = this.whiteKeyHeight + 20; // +20 pour labels
        
        // Style canvas
        this.canvas.style.border = '1px solid #ccc';
        this.canvas.style.borderRadius = '4px';
    }
    
    countWhiteKeys() {
        let count = 0;
        for (let note = this.noteRange.min; note <= this.noteRange.max; note++) {
            if (!this.isBlackKey(note)) {
                count++;
            }
        }
        return count;
    }
    
    // ========================================================================
    // EVENT LISTENERS
    // ========================================================================
    
    attachEventListeners() {
        // Instrument selector
        const instrumentSelect = this.container.querySelector('#instrument-select');
        if (instrumentSelect) {
            instrumentSelect.addEventListener('change', (e) => {
                this.emit('keyboard:instrument:changed', {
                    instrumentId: e.target.value
                });
            });
        }
        
        // Velocity slider
        const velocitySlider = this.container.querySelector('#velocity-slider');
        if (velocitySlider) {
            velocitySlider.addEventListener('input', (e) => {
                this.currentVelocity = parseInt(e.target.value);
                const valueDisplay = this.container.querySelector('#velocity-value');
                if (valueDisplay) {
                    valueDisplay.textContent = this.currentVelocity;
                }
            });
        }
        
        // Velocity presets
        const presetButtons = this.container.querySelectorAll('.btn-preset');
        presetButtons.forEach(btn => {
            btn.addEventListener('click', (e) => {
                const velocity = parseInt(e.target.dataset.velocity);
                this.currentVelocity = velocity;
                const valueDisplay = this.container.querySelector('#velocity-value');
                const slider = this.container.querySelector('#velocity-slider');
                if (valueDisplay) valueDisplay.textContent = velocity;
                if (slider) slider.value = velocity;
            });
        });
        
        // Panic button
        const panicBtn = this.container.querySelector('#btn-panic');
        if (panicBtn) {
            panicBtn.addEventListener('click', () => {
                this.emit('keyboard:panic');
                this.activeNotes.clear();
                this.drawKeyboard();
            });
        }
        
        // Refresh devices button
        const refreshBtn = this.container.querySelector('#btn-refresh-devices');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.emit('keyboard:refresh:devices');
            });
        }
        
        // Canvas mouse events
        if (this.canvas) {
            this.canvas.addEventListener('mousedown', (e) => this.handleCanvasMouseDown(e));
            this.canvas.addEventListener('mouseup', (e) => this.handleCanvasMouseUp(e));
            this.canvas.addEventListener('mousemove', (e) => this.handleCanvasMouseMove(e));
            this.canvas.addEventListener('mouseleave', (e) => this.handleCanvasMouseLeave(e));
        }
    }
    
    // ========================================================================
    // CANVAS INTERACTION
    // ========================================================================
    
    handleCanvasMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        const note = this.getNoteAtPosition(x, y);
        if (note !== null && this.isNotePlayable(note)) {
            this.activeNotes.add(note);
            this.drawKeyboard();
            
            this.emit('keyboard:note:on', {
                note: note,
                velocity: this.currentVelocity,
                instrument: this.selectedInstrument
            });
            
            this.stats.notesPlayed++;
            this.updateStats();
        }
    }
    
    handleCanvasMouseUp(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        const note = this.getNoteAtPosition(x, y);
        if (note !== null) {
            this.activeNotes.delete(note);
            this.drawKeyboard();
            
            this.emit('keyboard:note:off', {
                note: note,
                instrument: this.selectedInstrument
            });
        }
    }
    
    handleCanvasMouseMove(e) {
        // Optionnel: hover effects
    }
    
    handleCanvasMouseLeave(e) {
        // Arrêter toutes les notes actives
        this.activeNotes.forEach(note => {
            this.emit('keyboard:note:off', {
                note: note,
                instrument: this.selectedInstrument
            });
        });
        
        this.activeNotes.clear();
        this.drawKeyboard();
    }
    
    // ========================================================================
    // CANVAS DRAWING
    // ========================================================================
    
    drawKeyboard() {
        if (!this.ctx || !this.canvas) return;
        
        // Clear canvas
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        
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
            this.stats.activeNotes = this.activeNotes.size;
        } else {
            this.activeNotes.delete(note);
            this.stats.activeNotes = this.activeNotes.size;
        }
        
        this.drawKeyboard();
        this.updateStats();
    }
    
    updateStats(stats) {
        if (stats) {
            this.stats = { ...this.stats, ...stats };
        }
        
        this.updateStatValue('stat-notes-played', this.stats.notesPlayed);
        this.updateStatValue('stat-active-notes', this.stats.activeNotes);
        this.updateStatValue('stat-errors', this.stats.errors);
    }
    
    updateStatValue(id, value) {
        const elem = this.container.querySelector(`#${id}`);
        if (elem) {
            elem.textContent = value;
        }
    }
    
    // ========================================================================
    // UTILITY METHOD FOR DEBUG LOGGING
    // ========================================================================
    
    logDebug(category, message) {
        if (this.debugConsole && typeof this.debugConsole.log === 'function') {
            this.debugConsole.log(category, message);
        } else if (console && console.log) {
            console.log(`[KeyboardView][${category}] ${message}`);
        }
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        // Nettoyer état
        this.activeNotes.clear();
        
        // Nettoyer canvas
        if (this.ctx && this.canvas) {
            this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        }
        this.canvas = null;
        this.ctx = null;
        
        // Appeler destroy parent
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
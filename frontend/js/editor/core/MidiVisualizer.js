// ============================================================================
// Fichier: frontend/js/editor/core/MidiVisualizer.js
// Projet: MidiMind v3.2.1 - SystÃƒÆ’Ã‚Â¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.2.1 (ComplÃƒÆ’Ã‚Â©tÃƒÆ’Ã‚Â©e selon audit 2025-10-14)
// Date: 2025-10-14
// ============================================================================
// Description:
//   Classe principale orchestrant tous les composants de l'ÃƒÆ’Ã‚Â©diteur MIDI.
//   Coordonne renderers, interaction, viewport, et gestion donnÃƒÆ’Ã‚Â©es.
//
// FonctionnalitÃƒÆ’Ã‚Â©s:
//   - Orchestration complÃƒÆ’Ã‚Â¨te ÃƒÆ’Ã‚Â©diteur
//   - Gestion modes (Edit, Playback, Notation)
//   - Coordination renderers
//   - Gestion ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
//   - Statistiques performance
//   - Historique undo/redo
//
// Corrections v3.2.1:
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ updateStats() - Calcul complet stats (noteCount, duration, channels)
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ updateFPS() - Calcul FPS temps rÃƒÆ’Ã‚Â©el
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ destroy() - Nettoyage complet ressources
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ handleResize() - Gestion resize robuste
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ onViewChanged() - Callback viewport complet
//
// Architecture:
//   MidiVisualizer (classe orchestrateur)
//   - CoordinateSystem, Viewport, RenderEngine
//   - EditorMode, PlaybackMode
//   - SelectionManager, HistoryManager
//
// Auteur: MidiMind Team
// ============================================================================


class MidiVisualizer {
    constructor(canvas, config = {}, eventBus = null, debugConsole = null) {
        this.canvas = canvas;
        
        // EventBus et DebugConsole (avec fallback)
        // Si eventBus n'est pas fourni, crÃ©er une nouvelle instance
        if (eventBus) {
            this.eventBus = eventBus;
        } else if (window.EventBus && typeof window.EventBus === 'function') {
            // window.EventBus est la classe, il faut l'instancier
            this.eventBus = new window.EventBus();
        } else if (window.eventBus) {
            // Peut-Ãªtre qu'il y a une instance globale
            this.eventBus = window.eventBus;
        } else {
            this.eventBus = this.createDummyEventBus();
        }
        this.debugConsole = debugConsole || window.DebugConsole || null;
        
        // DonnÃƒÆ’Ã‚Â©es MIDI
        this.midiData = null;
        
        // SystÃƒÆ’Ã‚Â¨mes de base
        this.coordSystem = new CoordinateSystem(config.coordSystem);
        this.viewport = new Viewport(canvas, this.coordSystem);
        this.renderEngine = new RenderEngine(canvas, this.eventBus, this.debugConsole);
        
        // Interaction
        this.selection = new SelectionManager(this);
        
        // Historique (si disponible)
        this.history = config.enableHistory !== false ? new HistoryManager(this) : null;
        
        // Modes
        this.modes = {
            edit: null,
            playback: null
        };
        
        this.state = {
            mode: 'edit',
            tool: 'select',
            modified: false,
            playing: false,
            recording: false
        };
        
        // Playback
        this.playhead = 0;
        this.activeNotes = [];
        
        // Configuration
        this.config = {
            enableGrid: true,
            snapToGrid: true,
            gridSize: 100, // ms
            showVelocity: false,
            showCC: false,
            showPianoRoll: true,
            showTimeline: true,
            autoScroll: false,
            ...config
        };
        
        // Performance
        this.stats = {
            fps: 0,
            renderTime: 0,
            noteCount: 0,
            duration: 0,
            channels: 0,
            lastFrameTime: 0,
            frameCount: 0,
            fpsUpdateInterval: 1000
        };
        
        this.lastFrameTime = performance.now();
        this.fpsStartTime = performance.now();
        
        // Event emitter
        this.listeners = new Map();
        
        // Resize handler
        this.resizeObserver = null;
        
        this.initialize();
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    initialize() {
        console.log('[MidiVisualizer] Initializing...');
        
        // Lier le visualizer au renderEngine
        this.renderEngine.setVisualizer(this);
        
        // CrÃƒÆ’Ã‚Â©er les modes
        this.modes.edit = new EditorMode(this);
        this.modes.playback = new PlaybackMode(this);
        
        // Activer le mode par dÃƒÆ’Ã‚Â©faut
        this.setMode(this.state.mode);
        
        // Configurer le viewport
        this.viewport.onViewChanged = (view) => this.onViewChanged(view);
        
        // DÃƒÆ’Ã‚Â©marrer la boucle de rendu
        this.renderEngine.startRenderLoop(() => this.render());
        
        // GÃƒÆ’Ã‚Â©rer le resize avec ResizeObserver si disponible
        if (typeof ResizeObserver !== 'undefined') {
            this.resizeObserver = new ResizeObserver(() => this.handleResize());
            this.resizeObserver.observe(this.canvas.parentElement);
        } else {
            // Fallback sur window resize
            window.addEventListener('resize', () => this.handleResize());
        }
        
        // Resize initial
        this.handleResize();
        
        console.log('[MidiVisualizer] Initialized');
    }

    // ========================================================================
    // CHARGEMENT DONNÃƒÆ’Ã¢â‚¬Â°ES
    // ========================================================================

    /**
     * Charge des donnÃƒÆ’Ã‚Â©es MIDI
     * @param {Object} midiJson - DonnÃƒÆ’Ã‚Â©es MIDI au format JSON
     */
    loadMidiData(midiJson) {
        console.log('[MidiVisualizer] Loading MIDI data...');
        
        this.midiData = midiJson;
        
        // Mettre ÃƒÆ’Ã‚Â  jour les stats
        this.updateStats();
        
        // Fit to content
        if (midiJson.timeline && midiJson.timeline.length > 0) {
            const notes = midiJson.timeline.filter(e => e.type === 'noteOn');
            if (notes.length > 0) {
                this.viewport.fitToNotes(notes);
            }
        }
        
        // RafraÃƒÆ’Ã‚Â®chir
        this.renderEngine.requestRedraw();
        
        this.emit('data:loaded', { midiData: midiJson });
        
        console.log('[MidiVisualizer] MIDI data loaded:', {
            events: midiJson.timeline.length,
            noteCount: this.stats.noteCount,
            duration: this.stats.duration
        });
    }

    /**
     * Efface les donnÃƒÆ’Ã‚Â©es
     */
    clearData() {
        this.midiData = null;
        this.selection.clear();
        this.activeNotes = [];
        this.playhead = 0;
        
        this.updateStats();
        this.renderEngine.requestRedraw();
        
        this.emit('data:cleared');
    }

    // ========================================================================
    // MODES
    // ========================================================================

    /**
     * Change le mode
     * @param {string} mode - 'edit' ou 'playback'
     */
    setMode(mode) {
        if (!this.modes[mode]) {
            console.warn(`[MidiVisualizer] Unknown mode: ${mode}`);
            return;
        }
        
        // DÃƒÆ’Ã‚Â©sactiver le mode actuel
        if (this.state.mode && this.modes[this.state.mode]) {
            this.modes[this.state.mode].deactivate();
        }
        
        // Activer le nouveau mode
        this.state.mode = mode;
        this.modes[mode].activate();
        
        console.log(`[MidiVisualizer] Mode: ${mode}`);
        this.emit('mode:changed', { mode });
    }

    /**
     * Obtient le mode actuel
     * @returns {string}
     */
    getMode() {
        return this.state.mode;
    }

    // ========================================================================
    // OUTILS
    // ========================================================================

    /**
     * Change l'outil
     * @param {string} tool - 'select', 'pencil', 'eraser'
     */
    setTool(tool) {
        if (!['select', 'pencil', 'eraser'].includes(tool)) {
            console.warn(`[MidiVisualizer] Unknown tool: ${tool}`);
            return;
        }
        
        this.state.tool = tool;
        
        // Transmettre au mode d'ÃƒÆ’Ã‚Â©dition
        if (this.modes.edit && this.modes.edit.setTool) {
            this.modes.edit.setTool(tool);
        }
        
        this.emit('tool:changed', { tool });
    }

    /**
     * Obtient l'outil actuel
     * @returns {string}
     */
    getTool() {
        return this.state.tool;
    }

    // ========================================================================
    // RENDU
    // ========================================================================

    /**
     * Rendu principal
     */
    render() {
        if (!this.midiData) {
            this.renderEngine.clearCanvas();
            return;
        }
        
        // Rendu de base
        this.renderEngine.render(
            this.midiData,
            this.viewport,
            this.coordSystem,
            this.selection,
            this.activeNotes
        );
        
        // Rendu additionnel du mode
        const mode = this.modes[this.state.mode];
        if (mode && mode.render) {
            const ctx = this.canvas.getContext('2d');
            mode.render(ctx);
        }
        
        // Playhead
        if (this.state.playing) {
            this.renderPlayhead();
        }
        
        // Mettre ÃƒÆ’Ã‚Â  jour FPS
        this.updateFPS();
    }

    /**
     * Rendu du playhead
     */
    renderPlayhead() {
        const ctx = this.canvas.getContext('2d');
        const x = this.coordSystem.timeToX(this.playhead);
        
        const offsets = this.renderEngine.getUIOffsets();
        
        ctx.save();
        ctx.strokeStyle = '#e74c3c';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(x, offsets.top);
        ctx.lineTo(x, this.canvas.height - offsets.bottom);
        ctx.stroke();
        
        // Triangle en haut
        ctx.fillStyle = '#e74c3c';
        ctx.beginPath();
        ctx.moveTo(x, offsets.top);
        ctx.lineTo(x - 6, offsets.top + 10);
        ctx.lineTo(x + 6, offsets.top + 10);
        ctx.closePath();
        ctx.fill();
        
        ctx.restore();
    }

    // ========================================================================
    // PLAYBACK
    // ========================================================================

    /**
     * Met ÃƒÆ’Ã‚Â  jour la position de lecture
     * @param {number} timeMs - Temps en millisecondes
     */
    updatePlayhead(timeMs) {
        this.playhead = timeMs;
        
        // Auto-scroll si activÃƒÆ’Ã‚Â©
        if (this.config.autoScroll) {
            this.viewport.followPlayhead(timeMs);
        }
        
        this.renderEngine.requestRedraw();
    }

    /**
     * DÃƒÆ’Ã‚Â©marre la lecture
     */
    play() {
        this.state.playing = true;
        this.emit('playback:started');
    }

    /**
     * Met en pause la lecture
     */
    pause() {
        this.state.playing = false;
        this.emit('playback:paused');
    }

    /**
     * ArrÃƒÆ’Ã‚Âªte la lecture
     */
    stop() {
        this.state.playing = false;
        this.playhead = 0;
        this.activeNotes = [];
        this.emit('playback:stopped');
    }

    /**
     * DÃƒÆ’Ã‚Â©finit les notes actives (pendant playback)
     * @param {Array} notes - Notes actives
     */
    setActiveNotes(notes) {
        this.activeNotes = notes || [];
        this.renderEngine.requestRedraw();
    }

    // ========================================================================
    // ÃƒÆ’Ã¢â‚¬Â°DITION NOTES
    // ========================================================================

    /**
     * Ajoute une note
     * @param {Object} note - Note ÃƒÆ’Ã‚Â  ajouter
     */
    addNote(note) {
        if (!this.midiData) return;
        
        // GÃƒÆ’Ã‚Â©nÃƒÆ’Ã‚Â©rer ID si absent
        if (!note.id) {
            note.id = this.generateNoteId();
        }
        
        // Ajouter type si absent
        if (!note.type) {
            note.type = 'noteOn';
        }
        
        this.midiData.timeline.push(note);
        this.midiData.timeline.sort((a, b) => a.time - b.time);
        
        this.state.modified = true;
        this.updateStats();
        this.renderEngine.requestRedraw();
        
        this.emit('note:added', { note });
    }

    /**
     * Supprime des notes
     * @param {Array<string>} noteIds - IDs des notes
     */
    deleteNotes(noteIds) {
        if (!this.midiData) return;
        
        const deletedNotes = [];
        
        this.midiData.timeline = this.midiData.timeline.filter(event => {
            if (noteIds.includes(event.id)) {
                deletedNotes.push(event);
                return false;
            }
            return true;
        });
        
        this.state.modified = true;
        this.updateStats();
        this.renderEngine.requestRedraw();
        
        this.emit('note:deleted', { noteIds, notes: deletedNotes });
    }

    // ========================================================================
    // STATISTIQUES
    // ========================================================================

    /**
     * ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ COMPLET: Met ÃƒÆ’Ã‚Â  jour les statistiques
     */
    updateStats() {
        if (!this.midiData || !this.midiData.timeline) {
            this.stats.noteCount = 0;
            this.stats.duration = 0;
            this.stats.channels = 0;
            return;
        }
        
        // Compter les notes
        const notes = this.midiData.timeline.filter(e => e.type === 'noteOn');
        this.stats.noteCount = notes.length;
        
        // Calculer durÃƒÆ’Ã‚Â©e totale
        let maxTime = 0;
        notes.forEach(note => {
            const endTime = note.time + (note.duration || 0);
            if (endTime > maxTime) {
                maxTime = endTime;
            }
        });
        this.stats.duration = maxTime;
        
        // Compter canaux uniques
        const channels = new Set();
        notes.forEach(note => {
            if (note.channel !== undefined) {
                channels.add(note.channel);
            }
        });
        this.stats.channels = channels.size;
        
        // ÃƒÆ’Ã¢â‚¬Â°mettre ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
        this.emit('stats:updated', { stats: this.getStats() });
    }

    /**
     * ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ COMPLET: Met ÃƒÆ’Ã‚Â  jour le FPS
     */
    updateFPS() {
        const now = performance.now();
        
        // IncrÃƒÆ’Ã‚Â©menter compteur frames
        this.stats.frameCount++;
        
        // Calculer FPS toutes les secondes
        const elapsed = now - this.fpsStartTime;
        if (elapsed >= this.stats.fpsUpdateInterval) {
            this.stats.fps = Math.round((this.stats.frameCount * 1000) / elapsed);
            this.stats.frameCount = 0;
            this.fpsStartTime = now;
        }
        
        // Temps de frame actuel
        if (this.lastFrameTime) {
            const frameDelta = now - this.lastFrameTime;
            this.stats.renderTime = Math.round(frameDelta * 100) / 100;
        }
        
        this.lastFrameTime = now;
    }

    /**
     * Obtient les statistiques
     * @returns {Object}
     */
    getStats() {
        return {
            ...this.stats,
            zoom: {
                x: this.coordSystem.zoomX,
                y: this.coordSystem.zoomY
            },
            viewport: this.viewport.getVisibleRect(),
            selection: this.selection.getCount(),
            modified: this.state.modified
        };
    }

    // ========================================================================
    // RESIZE
    // ========================================================================

    /**
     * ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ AMÃƒÆ’Ã¢â‚¬Â°LIORÃƒÆ’Ã¢â‚¬Â°: GÃƒÆ’Ã‚Â¨re le resize du canvas
     */
    handleResize() {
        if (!this.canvas.parentElement) return;
        
        const container = this.canvas.parentElement;
        const rect = container.getBoundingClientRect();
        
        // VÃƒÆ’Ã‚Â©rifier si changement rÃƒÆ’Ã‚Â©el
        if (rect.width === this.viewport.width && rect.height === this.viewport.height) {
            return;
        }
        
        // Redimensionner viewport
        this.viewport.resize(rect.width, rect.height);
        
        // Redimensionner render engine
        this.renderEngine.resize(rect.width, rect.height);
        
        // Redraw
        this.renderEngine.requestRedraw();
        
        this.emit('resize', { width: rect.width, height: rect.height });
    }

    // ========================================================================
    // CALLBACKS
    // ========================================================================

    /**
     * ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ COMPLET: Callback changement viewport
     * @param {Object} view - Informations viewport
     */
    onViewChanged(view) {
        // Mettre ÃƒÆ’Ã‚Â  jour config auto-scroll
        this.viewport.setAutoScroll(this.config.autoScroll);
        
        // ÃƒÆ’Ã¢â‚¬Â°mettre ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
        this.emit('viewport:changed', view);
        
        // Redraw si pas en animation
        if (!this.viewport.isAnimating) {
            this.renderEngine.requestRedraw();
        }
    }

    // ========================================================================
    // EVENT EMITTER
    // ========================================================================

    /**
     * ÃƒÆ’Ã¢â‚¬Â°met un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {string} event - Nom ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {*} data - DonnÃƒÆ’Ã‚Â©es
     */
    emit(event, data) {
        const listeners = this.listeners.get(event);
        if (listeners) {
            listeners.forEach(callback => callback(data));
        }
    }

    /**
     * ÃƒÆ’Ã¢â‚¬Â°coute un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {string} event - Nom ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {Function} callback - Callback
     */
    on(event, callback) {
        if (!this.listeners.has(event)) {
            this.listeners.set(event, []);
        }
        this.listeners.get(event).push(callback);
    }

    /**
     * Retire un ÃƒÆ’Ã‚Â©couteur
     * @param {string} event - Nom ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     * @param {Function} callback - Callback
     */
    off(event, callback) {
        const listeners = this.listeners.get(event);
        if (listeners) {
            const index = listeners.indexOf(callback);
            if (index > -1) {
                listeners.splice(index, 1);
            }
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    /**
     * GÃƒÆ’Ã‚Â©nÃƒÆ’Ã‚Â¨re un ID unique pour une note
     * @returns {string}
     */
    generateNoteId() {
        return `note_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    }

    /**
     * ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ COMPLET: Nettoie les ressources
     */
    /**
     * CrÃƒÂ©e un EventBus factice si aucun n'est fourni
     * @returns {Object}
     */
    createDummyEventBus() {
        return {
            on: () => {},
            off: () => {},
            emit: () => {}
        };
    }

    destroy() {
        console.log('[MidiVisualizer] Destroying...');
        
        // ArrÃƒÆ’Ã‚Âªter la boucle de rendu
        this.renderEngine.stopRenderLoop();
        
        // DÃƒÆ’Ã‚Â©sactiver les modes
        Object.values(this.modes).forEach(mode => {
            if (mode && mode.deactivate) {
                mode.deactivate();
            }
        });
        
        // Nettoyer viewport
        if (this.viewport && this.viewport.destroy) {
            this.viewport.destroy();
        }
        
        // Nettoyer ResizeObserver
        if (this.resizeObserver) {
            this.resizeObserver.disconnect();
            this.resizeObserver = null;
        }
        
        // Nettoyer event listeners
        this.listeners.clear();
        
        // Nettoyer rÃƒÆ’Ã‚Â©fÃƒÆ’Ã‚Â©rences
        this.midiData = null;
        this.activeNotes = [];
        
        console.log('[MidiVisualizer] Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = MidiVisualizer;
}
if (typeof window !== 'undefined') {
    window.MidiVisualizer = MidiVisualizer;
}

// Export par défaut
window.MidiVisualizer = MidiVisualizer;
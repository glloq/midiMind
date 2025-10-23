// ============================================================================
// Fichier: frontend/js/editor/core/RenderEngine.js
// Version: v3.1.1 - PERFORMANCE OPTIMIZED + FIXED
// Date: 2025-10-23
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI
// ============================================================================
// MODIFICATIONS v3.1.1:
// ✓ Ajout de setVisualizer() pour compatibilité MidiVisualizer
// ✓ Ajout de startRenderLoop() et stopRenderLoop() (alias)
// ✓ Ajout de callback personnalisé pour render loop
// ============================================================================
// MODIFICATIONS v3.1.0:
// ✓ FPS limité à 10 (au lieu de 60)
// ✓ Anti-aliasing désactivé
// ✓ Animations désactivées
// ✓ Rendering optimisé (batch processing)
// ============================================================================

class RenderEngine {
    constructor(container, eventBus, debugConsole) {
        this.container = container;
        this.eventBus = eventBus;
        this.debugConsole = debugConsole;
        
        // Référence au visualizer (ajouté pour compatibilité)
        this.visualizer = null;
        this.renderCallback = null;
        
        // Configuration (OPTIMISÉ)
        this.config = {
            targetFPS: PerformanceConfig.rendering.targetFPS || 10,  // ✓ RÉDUIT À 10 fps
            enableAntiAliasing: PerformanceConfig.rendering.enableAntiAliasing || false,  // ✓ DÉSACTIVÉ
            maxVisibleNotes: PerformanceConfig.rendering.maxVisibleNotes || 500,
            updateInterval: PerformanceConfig.rendering.updateInterval || 100,
            enableAnimations: PerformanceConfig.rendering.enableAnimations || false,  // ✓ DÉSACTIVÉ
            renderBatchSize: PerformanceConfig.editor.renderBatchSize || 100
        };
        
        // Canvas
        this.canvas = null;
        this.ctx = null;
        this.width = 0;
        this.height = 0;
        this.dpr = 1;  // Device Pixel Ratio (fixe à 1 pour performance)
        
        // Animation loop
        this.animationFrameId = null;
        this.isRendering = false;
        this.lastFrameTime = 0;
        this.frameInterval = 1000 / this.config.targetFPS;  // ✓ ~100ms entre frames
        this.frameCount = 0;
        this.fps = 0;
        
        // État
        this.needsRedraw = true;
        this.viewport = {
            startTime: 0,
            endTime: 10000,
            minNote: 21,
            maxNote: 108,
            scrollX: 0,
            scrollY: 0,
            zoom: 1
        };
        
        // Renderers (sous-composants)
        this.renderers = {
            grid: null,
            timeline: null,
            pianoRoll: null
        };
        
        // Données à rendre
        this.data = {
            notes: [],
            selection: new Set(),
            metadata: null
        };
        
        // Performance tracking
        this.perfStats = {
            frameTime: 0,
            renderTime: 0,
            droppedFrames: 0,
            totalFrames: 0
        };
        
        this.logDebug('render', '✓ RenderEngine initialized (performance mode)');
        
        this.init();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        this.createCanvas();
        this.setupContext();
        this.attachEvents();
        
        this.logDebug('render', `Canvas: ${this.width}x${this.height}, FPS: ${this.config.targetFPS}`);
    }
    
    createCanvas() {
        // Récupérer ou créer canvas
        this.canvas = this.container.querySelector('canvas');
        
        if (!this.canvas) {
            this.canvas = document.createElement('canvas');
            this.container.appendChild(this.canvas);
        }
        
        // Taille initiale
        this.resize();
    }
    
    setupContext() {
        this.ctx = this.canvas.getContext('2d', {
            alpha: false,  // Pas de transparence = plus rapide
            desynchronized: true  // Meilleure perf pour animations
        });
        
        // ✓ DÉSACTIVER ANTI-ALIASING pour performance
        if (!this.config.enableAntiAliasing) {
            this.ctx.imageSmoothingEnabled = false;
            
            // Compatibilité navigateurs
            if (this.ctx.webkitImageSmoothingEnabled !== undefined) {
                this.ctx.webkitImageSmoothingEnabled = false;
            }
            if (this.ctx.mozImageSmoothingEnabled !== undefined) {
                this.ctx.mozImageSmoothingEnabled = false;
            }
            if (this.ctx.msImageSmoothingEnabled !== undefined) {
                this.ctx.msImageSmoothingEnabled = false;
            }
            
            this.logDebug('render', '✓ Anti-aliasing disabled');
        }
    }
    
    resize(width, height) {
        // Taille du container si non fournie
        if (width === undefined || height === undefined) {
            const rect = this.container.getBoundingClientRect();
            width = rect.width;
            height = rect.height;
        }
        
        // ✓ Fixer DPR à 1 pour éviter suréchantillonnage
        this.dpr = 1;
        
        this.width = width;
        this.height = height;
        
        // Appliquer au canvas
        this.canvas.width = this.width * this.dpr;
        this.canvas.height = this.height * this.dpr;
        this.canvas.style.width = `${this.width}px`;
        this.canvas.style.height = `${this.height}px`;
        
        // Rescaler le contexte si DPR > 1
        if (this.dpr > 1) {
            this.ctx.scale(this.dpr, this.dpr);
        }
        
        this.needsRedraw = true;
        
        this.logDebug('render', `Resized: ${this.width}x${this.height} (DPR: ${this.dpr})`);
    }
    
    attachEvents() {
        // Resize window
        window.addEventListener('resize', () => {
            this.resize();
        });
        
        // Redraw sur demande
        this.eventBus.on('render:redraw', () => {
            this.needsRedraw = true;
        });
        
        // Update viewport
        this.eventBus.on('render:viewport-changed', (viewport) => {
            this.setViewport(viewport);
        });
    }
    
    // ========================================================================
    // VISUALIZER (AJOUTÉ pour compatibilité)
    // ========================================================================
    
    /**
     * Définit le visualizer parent
     * @param {MidiVisualizer} visualizer - Instance du visualizer
     */
    setVisualizer(visualizer) {
        this.visualizer = visualizer;
        this.logDebug('render', '✓ Visualizer set');
    }
    
    // ========================================================================
    // RENDERERS (sous-composants)
    // ========================================================================
    
    setRenderer(type, renderer) {
        if (['grid', 'timeline', 'pianoRoll'].includes(type)) {
            this.renderers[type] = renderer;
            this.logDebug('render', `Renderer set: ${type}`);
        }
    }
    
    // ========================================================================
    // ANIMATION LOOP (OPTIMISÉ)
    // ========================================================================
    
    /**
     * Démarre la boucle de rendu
     * @param {Function} callback - Callback optionnel à appeler à chaque frame
     */
    startRenderLoop(callback) {
        if (callback) {
            this.renderCallback = callback;
        }
        this.start();
    }
    
    /**
     * Arrête la boucle de rendu
     */
    stopRenderLoop() {
        this.stop();
    }
    
    start() {
        if (this.isRendering) return;
        
        this.isRendering = true;
        this.lastFrameTime = performance.now();
        
        this.animate();
        
        this.logDebug('render', `Rendering started (${this.config.targetFPS} fps)`);
    }
    
    stop() {
        if (!this.isRendering) return;
        
        this.isRendering = false;
        
        if (this.animationFrameId) {
            cancelAnimationFrame(this.animationFrameId);
            this.animationFrameId = null;
        }
        
        this.logDebug('render', 'Rendering stopped');
    }
    
    animate() {
        if (!this.isRendering) return;
        
        const now = performance.now();
        const elapsed = now - this.lastFrameTime;
        
        // ✓ LIMITER FPS en sautant des frames
        if (elapsed >= this.frameInterval) {
            // Enregistrer frame time
            const frameStart = now;
            
            // Appeler le callback personnalisé si défini
            if (this.renderCallback) {
                this.renderCallback();
            } else if (this.needsRedraw) {
                // Sinon, render par défaut
                this.render();
                this.needsRedraw = false;
            }
            
            // Stats performance
            this.perfStats.renderTime = performance.now() - frameStart;
            this.perfStats.frameTime = elapsed;
            this.perfStats.totalFrames++;
            
            // FPS actuel
            this.fps = 1000 / elapsed;
            
            // Frame droppée si trop lent
            if (this.perfStats.renderTime > this.frameInterval) {
                this.perfStats.droppedFrames++;
            }
            
            this.lastFrameTime = now - (elapsed % this.frameInterval);
            this.frameCount++;
        }
        
        // Prochaine frame
        this.animationFrameId = requestAnimationFrame(() => this.animate());
    }
    
    // ========================================================================
    // RENDERING PRINCIPAL
    // ========================================================================
    
    render() {
        const renderStart = performance.now();
        
        // Clear canvas
        this.clear();
        
        // Render layers (dans l'ordre)
        this.renderGrid();
        this.renderNotes();
        this.renderTimeline();
        this.renderOverlay();
        
        const renderTime = performance.now() - renderStart;
        
        if (renderTime > 50) {  // Warning si > 50ms
            this.logDebug('render', `Slow render: ${renderTime.toFixed(2)}ms`, 'warn');
        }
    }
    
    clear() {
        this.ctx.fillStyle = '#1a1a1a';
        this.ctx.fillRect(0, 0, this.width, this.height);
    }
    
    renderGrid() {
        if (this.renderers.grid) {
            this.renderers.grid.render(this.ctx, this.viewport);
        }
    }
    
    renderNotes() {
        if (this.renderers.pianoRoll) {
            // ✓ LIMITER nombre de notes visibles
            const visibleNotes = this.getVisibleNotes();
            const limitedNotes = visibleNotes.slice(0, this.config.maxVisibleNotes);
            
            if (visibleNotes.length > this.config.maxVisibleNotes) {
                this.logDebug('render', 
                    `⚠️ ${visibleNotes.length} notes (showing ${this.config.maxVisibleNotes})`, 
                    'warn'
                );
            }
            
            // Render par batch pour performance
            this.renderNotesBatched(limitedNotes);
        }
    }
    
    renderNotesBatched(notes) {
        const batchSize = this.config.renderBatchSize;
        
        for (let i = 0; i < notes.length; i += batchSize) {
            const batch = notes.slice(i, i + batchSize);
            this.renderers.pianoRoll.renderNotes(this.ctx, batch, this.viewport, this.data.selection);
        }
    }
    
    renderTimeline() {
        if (this.renderers.timeline) {
            this.renderers.timeline.render(this.ctx, this.viewport, this.data.metadata);
        }
    }
    
    renderOverlay() {
        // Debug info si activé
        if (PerformanceConfig.debug.enableFPSCounter) {
            this.renderDebugInfo();
        }
    }
    
    renderDebugInfo() {
        this.ctx.save();
        
        this.ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
        this.ctx.fillRect(10, 10, 200, 80);
        
        this.ctx.fillStyle = '#00ff00';
        this.ctx.font = '12px monospace';
        this.ctx.fillText(`FPS: ${this.fps.toFixed(1)}`, 20, 30);
        this.ctx.fillText(`Render: ${this.perfStats.renderTime.toFixed(2)}ms`, 20, 50);
        this.ctx.fillText(`Notes: ${this.data.notes.length}`, 20, 70);
        
        this.ctx.restore();
    }
    
    // ========================================================================
    // DONNÉES
    // ========================================================================
    
    setNotes(notes) {
        this.data.notes = notes || [];
        this.needsRedraw = true;
    }
    
    setSelection(selection) {
        this.data.selection = selection || new Set();
        this.needsRedraw = true;
    }
    
    setMetadata(metadata) {
        this.data.metadata = metadata;
        this.needsRedraw = true;
    }
    
    // ========================================================================
    // VIEWPORT
    // ========================================================================
    
    setViewport(viewport) {
        this.viewport = { ...this.viewport, ...viewport };
        this.needsRedraw = true;
    }
    
    getViewport() {
        return { ...this.viewport };
    }
    
    getVisibleNotes() {
        if (!this.data.notes) return [];
        
        return this.data.notes.filter(note => {
            return note.time >= this.viewport.startTime &&
                   note.time <= this.viewport.endTime &&
                   note.note >= this.viewport.minNote &&
                   note.note <= this.viewport.maxNote;
        });
    }
    
    // ========================================================================
    // COORDONNÉES
    // ========================================================================
    
    timeToX(time) {
        const range = this.viewport.endTime - this.viewport.startTime;
        return ((time - this.viewport.startTime) / range) * this.width;
    }
    
    noteToY(note) {
        const range = this.viewport.maxNote - this.viewport.minNote;
        return this.height - (((note - this.viewport.minNote) / range) * this.height);
    }
    
    xToTime(x) {
        const range = this.viewport.endTime - this.viewport.startTime;
        return this.viewport.startTime + (x / this.width) * range;
    }
    
    yToNote(y) {
        const range = this.viewport.maxNote - this.viewport.minNote;
        return Math.round(this.viewport.minNote + ((this.height - y) / this.height) * range);
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    getCanvas() {
        return this.canvas;
    }
    
    getContext() {
        return this.ctx;
    }
    
    getStats() {
        return {
            ...this.perfStats,
            fps: this.fps,
            isRendering: this.isRendering,
            noteCount: this.data.notes.length,
            visibleNotes: this.getVisibleNotes().length
        };
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    requestRedraw() {
        this.needsRedraw = true;
    }
    
    logDebug(category, message, level = 'info') {
        if (this.debugConsole) {
            this.debugConsole.log(category, message, level);
        }
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        this.stop();
        
        if (this.canvas && this.canvas.parentNode) {
            this.canvas.parentNode.removeChild(this.canvas);
        }
        
        this.canvas = null;
        this.ctx = null;
        
        this.logDebug('render', '✓ RenderEngine destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RenderEngine;
}

if (typeof window !== 'undefined') {
    window.RenderEngine = RenderEngine;
}
window.RenderEngine = RenderEngine;
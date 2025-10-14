// ============================================================================
// Fichier: frontend/js/editor/core/RenderEngine.js
// Projet: MidiMind v3.2.1 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.2.1 (Complétée selon audit 2025-10-14)
// Date: 2025-10-14
// ============================================================================
// Description:
//   Moteur de rendu orchestrant tous les renderers (Grid, Notes, Timeline, etc).
//   Gère l'ordre de rendu, les clipping zones, et la performance.
//
// Fonctionnalités:
//   - Orchestration complète renderers
//   - Ordre de rendu (layers)
//   - Clipping zones
//   - Gestion qualité (LOD)
//   - Performance monitoring
//   - Boucle de rendu optimisée
//
// Corrections v3.2.1:
//   ✅ clearCanvas() - Couleur fond configurable
//   ✅ getUIOffsets() - Calculs robustes avec fallbacks
//   ✅ getNoteRenderZone() - Calculs complets et précis
//   ✅ adjustQualityForPerformance() - LOD adaptatif selon FPS
//
// Architecture:
//   RenderEngine (classe orchestrateur)
//   - GridRenderer, NoteRenderer, TimelineRenderer, PianoRollRenderer
//   - Gestion layers et z-index
//   - Optimisation performance
//
// Auteur: MidiMind Team
// ============================================================================

class RenderEngine {
    constructor(canvas, config = {}) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        
        // Renderers spécialisés
        this.gridRenderer = new GridRenderer(config.grid);
        this.noteRenderer = new NoteRenderer(config.notes);
        this.timelineRenderer = new TimelineRenderer(config.timeline);
        this.pianoRollRenderer = new PianoRollRenderer(config.pianoRoll);
        this.ccRenderer = new CCRenderer(config.cc);
        this.velocityEditor = new VelocityEditorRenderer(config.velocity);
        
        // Configuration
        this.config = {
            clearColor: config.clearColor || '#0a0a0a',
            showPianoRoll: config.showPianoRoll !== false,
            showTimeline: config.showTimeline !== false,
            showCC: config.showCC || false,
            showVelocity: config.showVelocity || false,
            showTrackHeaders: config.showTrackHeaders || false,
            enablePerformanceMonitoring: config.enablePerformanceMonitoring !== false,
            targetFPS: config.targetFPS || 60,
            autoAdjustQuality: config.autoAdjustQuality !== false,
            ...config
        };
        
        // Qualité de rendu
        this.quality = {
            antialiasing: true,
            shadows: false,
            details: true,
            level: 'medium' // 'low', 'medium', 'high'
        };
        
        // Performance
        this.lastRenderTime = 0;
        this.fpsHistory = [];
        this.maxFPSHistory = 10;
        this.lowFPSThreshold = 30;
        this.highFPSThreshold = 55;
        
        // Boucle de rendu
        this.needsRedraw = false;
        this.isRendering = false;
        this.animationFrameId = null;
        
        // Référence au visualizer
        this.visualizer = null;
        
        // Renderer additionnel (pour modes)
        this.modeRenderer = null;
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    /**
     * Définit la référence au visualizer
     * @param {Object} visualizer - Instance MidiVisualizer
     */
    setVisualizer(visualizer) {
        this.visualizer = visualizer;
    }

    // ========================================================================
    // RENDU PRINCIPAL
    // ========================================================================

    /**
     * Rendu complet de la scène
     * @param {Object} midiData - Données MIDI
     * @param {Object} viewport - Viewport actuel
     * @param {Object} coordSystem - Système de coordonnées
     * @param {Object} selection - Gestionnaire sélection
     * @param {Array} activeNotes - Notes actives (playback)
     */
    render(midiData, viewport, coordSystem, selection = null, activeNotes = null) {
        if (!midiData || !midiData.timeline) {
            this.clearCanvas();
            return;
        }
        
        const startTime = performance.now();
        
        // Nettoyer le canvas
        this.clearCanvas();
        
        // Activer antialiasing
        this.ctx.imageSmoothingEnabled = this.quality.antialiasing;
        
        // Calculer les dimensions des zones
        const pianoRollWidth = this.config.showPianoRoll ? 
            this.pianoRollRenderer.getWidth() : 0;
        const timelineHeight = this.config.showTimeline ? 
            this.timelineRenderer.getHeight() : 0;
        const velocityHeight = this.config.showVelocity ? 
            this.velocityEditor.getHeight() : 0;
        const ccHeight = this.config.showCC ? 
            this.ccRenderer.getHeight() : 0;
        
        // Zone de rendu des notes
        const noteZoneHeight = this.canvas.height - timelineHeight - velocityHeight - ccHeight;
        
        // ====================================================================
        // 1. ZONE PRINCIPALE (Notes + Grille)
        // ====================================================================
        
        this.ctx.save();
        this.ctx.translate(pianoRollWidth, timelineHeight);
        
        // Clipping pour la zone de notes
        this.ctx.beginPath();
        this.ctx.rect(0, 0, this.canvas.width - pianoRollWidth, noteZoneHeight);
        this.ctx.clip();
        
        // 1a. Grille
        this.gridRenderer.render(
            this.ctx,
            viewport,
            coordSystem,
            midiData.metadata
        );
        
        // 1b. Notes normales
        const notes = midiData.timeline.filter(e => e.type === 'noteOn');
        this.noteRenderer.render(
            this.ctx,
            notes,
            viewport,
            coordSystem,
            selection
        );
        
        // 1c. Notes actives (pendant lecture)
        if (activeNotes && activeNotes.length > 0) {
            this.noteRenderer.renderActiveNotes(
                this.ctx,
                activeNotes,
                viewport,
                coordSystem
            );
        }
        
        // 1d. Rendu additionnel du mode
        if (this.modeRenderer) {
            this.modeRenderer(this.ctx);
        }
        
        this.ctx.restore();
        
        // ====================================================================
        // 2. CONTROL CHANGES (sous la zone de notes)
        // ====================================================================
        
        if (this.config.showCC) {
            const ccEvents = midiData.timeline.filter(e => e.type === 'cc');
            if (ccEvents.length > 0) {
                this.ctx.save();
                this.ctx.translate(pianoRollWidth, timelineHeight + noteZoneHeight);
                
                this.ccRenderer.render(
                    this.ctx,
                    ccEvents,
                    viewport,
                    coordSystem,
                    ccHeight
                );
                
                this.ctx.restore();
            }
        }
        
        // ====================================================================
        // 3. VELOCITY EDITOR (bas du canvas)
        // ====================================================================
        
        if (this.config.showVelocity) {
            this.ctx.save();
            this.ctx.translate(pianoRollWidth, this.canvas.height - velocityHeight);
            
            this.velocityEditor.render(
                this.ctx,
                notes,
                viewport,
                coordSystem,
                selection,
                this.canvas.height
            );
            
            this.ctx.restore();
        }
        
        // ====================================================================
        // 4. PIANO ROLL (gauche)
        // ====================================================================
        
        if (this.config.showPianoRoll) {
            this.ctx.save();
            this.ctx.translate(0, timelineHeight);
            
            this.pianoRollRenderer.render(
                this.ctx,
                viewport,
                coordSystem,
                noteZoneHeight
            );
            
            // Mettre à jour les notes actives
            if (activeNotes) {
                this.pianoRollRenderer.setActiveNotes(activeNotes);
            }
            
            this.ctx.restore();
        }
        
        // ====================================================================
        // 5. TIMELINE (haut)
        // ====================================================================
        
        if (this.config.showTimeline) {
            this.timelineRenderer.renderWithOffset(
                this.ctx,
                viewport,
                coordSystem,
                midiData.metadata,
                pianoRollWidth
            );
        }
        
        // Mesurer le temps de rendu
        const renderTime = performance.now() - startTime;
        this.lastRenderTime = renderTime;
        
        // Ajuster qualité si nécessaire
        if (this.config.autoAdjustQuality && this.config.enablePerformanceMonitoring) {
            this.adjustQualityForPerformance(renderTime);
        }
        
        this.needsRedraw = false;
    }

    // ====================================================================
    // BOUCLE DE RENDU
    // ====================================================================

    /**
     * Démarre la boucle de rendu
     * @param {Function} renderCallback - Callback de rendu
     */
    startRenderLoop(renderCallback) {
        if (this.isRendering) return;
        
        this.isRendering = true;
        
        const loop = () => {
            if (!this.isRendering) return;
            
            if (this.needsRedraw) {
                renderCallback();
            }
            
            this.animationFrameId = requestAnimationFrame(loop);
        };
        
        loop();
    }

    /**
     * Arrête la boucle de rendu
     */
    stopRenderLoop() {
        this.isRendering = false;
        if (this.animationFrameId) {
            cancelAnimationFrame(this.animationFrameId);
            this.animationFrameId = null;
        }
    }

    /**
     * Demande un nouveau rendu
     */
    requestRedraw() {
        this.needsRedraw = true;
    }

    // ====================================================================
    // CANVAS
    // ====================================================================

    /**
     * ✅ AMÉLIORÉ: Nettoie le canvas avec couleur configurable
     */
    clearCanvas() {
        // Remplir avec couleur de fond
        this.ctx.fillStyle = this.config.clearColor || '#0a0a0a';
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        
        // Reset transformations
        this.ctx.setTransform(1, 0, 0, 1, 0, 0);
        
        // Reset compositing
        this.ctx.globalAlpha = 1.0;
        this.ctx.globalCompositeOperation = 'source-over';
    }

    /**
     * Redimensionne le canvas
     * @param {number} width - Largeur
     * @param {number} height - Hauteur
     */
    resize(width, height) {
        // Utiliser device pixel ratio pour netteté sur écrans haute résolution
        const dpr = window.devicePixelRatio || 1;
        
        this.canvas.width = width * dpr;
        this.canvas.height = height * dpr;
        this.canvas.style.width = `${width}px`;
        this.canvas.style.height = `${height}px`;
        
        // Réappliquer scale
        this.ctx.scale(dpr, dpr);
        
        this.requestRedraw();
    }

    // ====================================================================
    // QUALITÉ
    // ====================================================================

    /**
     * Définit le niveau de qualité
     * @param {string} level - 'low', 'medium', 'high'
     */
    setQuality(level) {
        this.quality.level = level;
        
        switch (level) {
            case 'high':
                this.quality.antialiasing = true;
                this.quality.shadows = true;
                this.quality.details = true;
                this.noteRenderer.config.enableLOD = false;
                this.noteRenderer.config.showVelocityGradient = true;
                break;
                
            case 'medium':
                this.quality.antialiasing = true;
                this.quality.shadows = false;
                this.quality.details = true;
                this.noteRenderer.config.enableLOD = true;
                this.noteRenderer.config.showVelocityGradient = true;
                break;
                
            case 'low':
                this.quality.antialiasing = false;
                this.quality.shadows = false;
                this.quality.details = false;
                this.noteRenderer.config.enableLOD = true;
                this.noteRenderer.config.showVelocityGradient = false;
                break;
        }
        
        this.requestRedraw();
    }

    /**
     * ✅ NOUVEAU: Ajuste automatiquement la qualité selon les performances
     * @param {number} renderTime - Temps de rendu en ms
     */
    adjustQualityForPerformance(renderTime) {
        // Calculer FPS actuel
        const currentFPS = renderTime > 0 ? 1000 / renderTime : 60;
        
        // Ajouter à l'historique
        this.fpsHistory.push(currentFPS);
        if (this.fpsHistory.length > this.maxFPSHistory) {
            this.fpsHistory.shift();
        }
        
        // Ne rien faire si pas assez d'historique
        if (this.fpsHistory.length < this.maxFPSHistory) return;
        
        // Calculer FPS moyen
        const avgFPS = this.fpsHistory.reduce((a, b) => a + b, 0) / this.fpsHistory.length;
        
        // Ajuster qualité selon FPS moyen
        if (avgFPS < this.lowFPSThreshold && this.quality.level !== 'low') {
            console.log(`[RenderEngine] Low FPS detected (${Math.round(avgFPS)}), reducing quality`);
            this.setQuality('low');
        } else if (avgFPS > this.highFPSThreshold && avgFPS < this.config.targetFPS && this.quality.level === 'high') {
            console.log(`[RenderEngine] Medium FPS detected (${Math.round(avgFPS)}), using medium quality`);
            this.setQuality('medium');
        } else if (avgFPS >= this.config.targetFPS && this.quality.level !== 'high') {
            console.log(`[RenderEngine] High FPS detected (${Math.round(avgFPS)}), increasing quality`);
            this.setQuality('high');
        }
    }

    // ====================================================================
    // MODE RENDERER
    // ====================================================================

    /**
     * Définit un renderer additionnel pour le mode actif
     * @param {Function} renderer - Fonction de rendu
     */
    setModeRenderer(renderer) {
        this.modeRenderer = renderer;
    }

    // ====================================================================
    // AFFICHAGE DES COMPOSANTS
    // ====================================================================

    setShowPianoRoll(show) {
        this.config.showPianoRoll = show;
        this.requestRedraw();
    }

    setShowTimeline(show) {
        this.config.showTimeline = show;
        this.requestRedraw();
    }

    setShowCC(show) {
        this.config.showCC = show;
        this.ccRenderer.setVisible(show);
        this.requestRedraw();
    }

    setShowVelocity(show) {
        this.config.showVelocity = show;
        this.velocityEditor.setVisible(show);
        this.requestRedraw();
    }

    setShowTrackHeaders(show) {
        this.config.showTrackHeaders = show;
        this.requestRedraw();
    }

    // ====================================================================
    // GETTERS
    // ====================================================================

    /**
     * ✅ ROBUSTIFIÉ: Obtient les offsets des zones UI
     * @returns {Object} {left, top, bottom, right}
     */
    getUIOffsets() {
        const left = this.config.showPianoRoll ? 
            (this.pianoRollRenderer.getWidth ? this.pianoRollRenderer.getWidth() : 80) : 0;
        
        const top = this.config.showTimeline ? 
            (this.timelineRenderer.getHeight ? this.timelineRenderer.getHeight() : 60) : 0;
        
        const ccHeight = this.config.showCC ? 
            (this.ccRenderer.getHeight ? this.ccRenderer.getHeight() : 100) : 0;
        
        const velocityHeight = this.config.showVelocity ? 
            (this.velocityEditor.getHeight ? this.velocityEditor.getHeight() : 80) : 0;
        
        const bottom = ccHeight + velocityHeight;
        
        return {
            left: left,
            top: top,
            bottom: bottom,
            right: 0
        };
    }

    /**
     * ✅ COMPLET: Obtient la zone de rendu des notes
     * @returns {Object} {x, y, width, height}
     */
    getNoteRenderZone() {
        const offsets = this.getUIOffsets();
        
        const zone = {
            x: offsets.left,
            y: offsets.top,
            width: Math.max(0, this.canvas.width - offsets.left - offsets.right),
            height: Math.max(0, this.canvas.height - offsets.top - offsets.bottom)
        };
        
        return zone;
    }

    /**
     * Obtient le dernier temps de rendu
     * @returns {number} Temps en ms
     */
    getLastRenderTime() {
        return this.lastRenderTime;
    }

    /**
     * Obtient la qualité actuelle
     * @returns {Object} Informations qualité
     */
    getQualityInfo() {
        return {
            ...this.quality,
            avgFPS: this.fpsHistory.length > 0 ?
                Math.round(this.fpsHistory.reduce((a, b) => a + b, 0) / this.fpsHistory.length) :
                0
        };
    }

    /**
     * Nettoie le cache
     */
    clearCache() {
        if (this.noteRenderer.clearCache) {
            this.noteRenderer.clearCache();
        }
        if (this.gridRenderer.invalidateCache) {
            this.gridRenderer.invalidateCache();
        }
    }

    /**
     * Nettoie les ressources
     */
    destroy() {
        this.stopRenderLoop();
        this.clearCache();
        
        // Nettoyer références
        this.visualizer = null;
        this.modeRenderer = null;
        this.fpsHistory = [];
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RenderEngine;
}
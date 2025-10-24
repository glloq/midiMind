// ============================================================================
// 📄 Fichier: frontend/js/views/BaseCanvasView.js
// 🎹 Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// 📝 Description:
//   Classe de base pour toutes les vues utilisant un canvas HTML5.
//   Gère le redimensionnement automatique, le contexte 2D, le render loop,
//   et les interactions basiques (souris).
//
// ✨ Fonctionnalités:
//   - Gestion pixel ratio (support écrans Retina)
//   - Render loop avec requestAnimationFrame
//   - Redimensionnement automatique responsive
//   - Gestion événements souris (down, move, up, leave)
//   - Invalidation optimisée (render on demand)
//   - Nettoyage automatique (destroy)
//
// 🏗️ Architecture:
//   Template Method Pattern - Méthodes à surcharger :
//   - render() : Rendu personnalisé
//   - handleMouseDown/Move/Up/Leave() : Interactions
//
// 👤 Auteur: MidiMind Team
// ============================================================================

class BaseCanvasView {
    constructor(canvas) {
        if (!canvas) {
            throw new Error('Canvas element is required');
        }
        
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        
        // Configuration par défaut
        this.pixelRatio = window.devicePixelRatio || 1;
        this.needsRedraw = true;
        
        // État d'interaction
        this.isMouseDown = false;
        this.mousePos = { x: 0, y: 0 };
        
        this.init();
    }

    // ========================================================================
    // 🚀 INITIALISATION
    // ========================================================================

    init() {
        this.resize();
        this.attachEvents();
        this.startRenderLoop();
    }

    attachEvents() {
        // Redimensionnement
        window.addEventListener('resize', () => this.resize());
        
        // Souris
        this.canvas.addEventListener('mousedown', (e) => this.handleMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.handleMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.handleMouseUp(e));
        this.canvas.addEventListener('mouseleave', () => this.handleMouseLeave());
    }

    // ========================================================================
    // 📐 REDIMENSIONNEMENT
    // ========================================================================

    resize() {
        const rect = this.canvas.getBoundingClientRect();
        
        // Ajuster pour device pixel ratio (Retina, etc.)
        this.canvas.width = rect.width * this.pixelRatio;
        this.canvas.height = rect.height * this.pixelRatio;
        
        this.canvas.style.width = rect.width + 'px';
        this.canvas.style.height = rect.height + 'px';
        
        // Ajuster le contexte
        this.ctx.scale(this.pixelRatio, this.pixelRatio);
        
        this.invalidate();
    }

    // ========================================================================
    // 🎨 RENDER LOOP
    // ========================================================================

    startRenderLoop() {
        const renderFrame = () => {
            if (this.needsRedraw) {
                this.render();
                this.needsRedraw = false;
            }
            
            this.animationFrameId = requestAnimationFrame(renderFrame);
        };
        
        renderFrame();
    }

    stopRenderLoop() {
        if (this.animationFrameId) {
            cancelAnimationFrame(this.animationFrameId);
        }
    }

    invalidate() {
        this.needsRedraw = true;
    }

    // ========================================================================
    // 🔧 MÉTHODES À SURCHARGER
    // ========================================================================

    render() {
        // À implémenter dans les classes dérivées
        this.clearCanvas();
    }

    handleMouseDown(e) {
        this.isMouseDown = true;
        this.mousePos = this.getCanvasPoint(e);
    }

    handleMouseMove(e) {
        this.mousePos = this.getCanvasPoint(e);
    }

    handleMouseUp(e) {
        this.isMouseDown = false;
    }

    handleMouseLeave() {
        this.isMouseDown = false;
    }

    // ========================================================================
    // 🛠️ UTILITAIRES
    // ========================================================================

    getCanvasPoint(e) {
        const rect = this.canvas.getBoundingClientRect();
        return {
            x: (e.clientX - rect.left) * this.pixelRatio,
            y: (e.clientY - rect.top) * this.pixelRatio
        };
    }

    clearCanvas() {
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    }

    destroy() {
        this.stopRenderLoop();
        window.removeEventListener('resize', () => this.resize());
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BaseCanvasView;
}

if (typeof window !== 'undefined') {
    window.BaseCanvasView = BaseCanvasView;  // ← AJOUTÉ
}
window.BaseCanvasView = BaseCanvasView;
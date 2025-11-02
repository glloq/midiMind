// ============================================================================
// Fichier: frontend/js/core/BaseCanvasView.js
// Version: v4.0.0
// ============================================================================

class BaseCanvasView {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas ? canvas.getContext('2d') : null;
        this.state = {};
        this.animationId = null;
    }
    
    init() {
        if (!this.canvas) return;
        this.resizeCanvas();
        window.addEventListener('resize', () => this.resizeCanvas());
    }
    
    resizeCanvas() {
        if (!this.canvas) return;
        const parent = this.canvas.parentElement;
        if (!parent) return;
        this.canvas.width = parent.clientWidth;
        this.canvas.height = parent.clientHeight;
        this.render();
    }
    
    render() {
        // Override in subclass
    }
    
    clearCanvas() {
        if (!this.ctx) return;
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    }
    
    destroy() {
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
        }
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BaseCanvasView;
}
if (typeof window !== 'undefined') {
    window.BaseCanvasView = BaseCanvasView;
}
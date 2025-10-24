// ============================================================================
// 📄 Fichier: frontend/js/views/CCEditorView.js
// 🎹 Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// 📝 Description:
//   Vue éditeur Control Changes (CC) MIDI pour l'automation.
//   Affichage courbes CC avec édition graphique (points, lignes, courbes).
//
// ✨ Fonctionnalités:
//   - Affichage courbes CC (0-127)
//   - Sélection type CC (Volume, Pan, Modulation, etc.)
//   - Édition points de contrôle
//   - Interpolation linéaire/courbe
//   - Zoom et pan synchronisés avec piano roll
//   - Outils : Point, Line, Curve, Erase
//   - Snap temporel configurable
//   - Copier/coller sections CC
//
// 🏗️ Architecture:
//   CCEditorView extends BaseCanvasView
//   - CCRenderer : Rendu courbes optimisé
//   - DragHandler : Édition interactive
//   - Synchronisation avec PianoRollView
//
// 👤 Auteur: MidiMind Team
// ============================================================================

class CCEditorView {
    constructor(canvas, model, ccNumber) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.model = model;
        this.ccNumber = ccNumber;
        
        // Configuration
        this.pixelsPerMs = 0.1;
        this.scrollX = 0;
        this.height = 100;
        
        // Points CC
        this.points = [];
        this.selectedPoints = new Set();
        this.hoveredPoint = null;
        
        // Interaction
        this.isDragging = false;
        this.dragStart = null;
        this.interpolation = 'linear'; // linear, step, smooth
        
        this.init();
    }

    /**
     * Initialise l'éditeur CC
     */
    init() {
        this.resize();
        this.extractPoints();
        this.attachEvents();
        
        // Observer les changements
        this.model.on('editor:note:updated', () => {
            this.extractPoints();
            this.render();
        });
        
        this.model.on('editor:zoom:changed', (zoom) => {
            this.pixelsPerMs = 0.1 * zoom.x;
            this.render();
        });
    }

    /**
     * Redimensionne le canvas
     */
    resize() {
        const container = this.canvas.parentElement;
        const dpr = window.devicePixelRatio || 1;
        
        this.canvas.width = container.clientWidth * dpr;
        this.canvas.height = this.height * dpr;
        this.canvas.style.width = `${container.clientWidth}px`;
        this.canvas.style.height = `${this.height}px`;
        
        this.ctx.scale(dpr, dpr);
        
        this.width = container.clientWidth;
    }

    /**
     * Extrait les points CC de la timeline
     */
    extractPoints() {
        const timeline = this.model.getFilteredTimeline();
        
        this.points = timeline
            .filter(e => e.type === 'cc' && e.controller === this.ccNumber)
            .map(e => ({
                time: e.time,
                value: e.value,
                channel: e.channel,
                id: e.id
            }))
            .sort((a, b) => a.time - b.time);
    }

    /**
     * Rend l'éditeur CC
     */
    render() {
        // Clear
        this.ctx.fillStyle = '#1a1a1a';
        this.ctx.fillRect(0, 0, this.width, this.height);
        
        // Grille
        this.drawGrid();
        
        // Ligne de base (0)
        this.drawBaseline();
        
        // Courbe CC
        if (this.points.length > 0) {
            this.drawCurve();
            this.drawPoints();
        }
        
        // Point survolé
        if (this.hoveredPoint) {
            this.drawHoverHighlight(this.hoveredPoint);
        }
        
        // Zone de sélection
        if (this.selectionRect) {
            this.drawSelectionRect();
        }
    }

    /**
     * Dessine la grille
     */
    drawGrid() {
        const { start, end } = this.model.state.viewport;
        
        // Lignes verticales
        this.ctx.strokeStyle = '#2a2a2a';
        this.ctx.lineWidth = 1;
        
        const gridInterval = this.model.state.snapResolution;
        const startTime = Math.floor(start / gridInterval) * gridInterval;
        
        for (let time = startTime; time <= end; time += gridInterval) {
            const x = this.timeToX(time);
            
            if (x >= 0 && x <= this.width) {
                this.ctx.beginPath();
                this.ctx.moveTo(x, 0);
                this.ctx.lineTo(x, this.height);
                this.ctx.stroke();
            }
        }
        
        // Lignes horizontales (valeurs)
        const valueSteps = [0, 32, 64, 96, 127];
        
        valueSteps.forEach(value => {
            const y = this.valueToY(value);
            
            this.ctx.strokeStyle = value === 64 ? '#3a3a3a' : '#2a2a2a';
            this.ctx.beginPath();
            this.ctx.moveTo(0, y);
            this.ctx.lineTo(this.width, y);
            this.ctx.stroke();
            
            // Label de valeur
            this.ctx.fillStyle = '#666';
            this.ctx.font = '10px sans-serif';
            this.ctx.textAlign = 'right';
            this.ctx.textBaseline = 'middle';
            this.ctx.fillText(value, this.width - 5, y);
        });
    }

    /**
     * Dessine la ligne de base
     */
    drawBaseline() {
        const y = this.valueToY(64);
        
        this.ctx.strokeStyle = '#444';
        this.ctx.lineWidth = 1;
        this.ctx.setLineDash([5, 5]);
        this.ctx.beginPath();
        this.ctx.moveTo(0, y);
        this.ctx.lineTo(this.width, y);
        this.ctx.stroke();
        this.ctx.setLineDash([]);
    }

    /**
     * Dessine la courbe CC
     */
    drawCurve() {
        if (this.points.length === 0) return;
        
        this.ctx.strokeStyle = '#667eea';
        this.ctx.lineWidth = 2;
        this.ctx.beginPath();
        
        // Premier point
        const firstPoint = this.points[0];
        let x = this.timeToX(firstPoint.time);
        let y = this.valueToY(firstPoint.value);
        this.ctx.moveTo(x, y);
        
        // Dessiner selon le type d'interpolation
        for (let i = 1; i < this.points.length; i++) {
            const point = this.points[i];
            x = this.timeToX(point.time);
            y = this.valueToY(point.value);
            
            switch (this.interpolation) {
                case 'linear':
                    this.ctx.lineTo(x, y);
                    break;
                    
                case 'step':
                    const prevPoint = this.points[i - 1];
                    const prevX = this.timeToX(prevPoint.time);
                    const prevY = this.valueToY(prevPoint.value);
                    this.ctx.lineTo(x, prevY);
                    this.ctx.lineTo(x, y);
                    break;
                    
                case 'smooth':
                    // Interpolation avec courbes de Bézier
                    const prev = this.points[i - 1];
                    const prevX2 = this.timeToX(prev.time);
                    const prevY2 = this.valueToY(prev.value);
                    
                    const cpX1 = prevX2 + (x - prevX2) / 3;
                    const cpY1 = prevY2;
                    const cpX2 = prevX2 + (x - prevX2) * 2 / 3;
                    const cpY2 = y;
                    
                    this.ctx.bezierCurveTo(cpX1, cpY1, cpX2, cpY2, x, y);
                    break;
            }
        }
        
        this.ctx.stroke();
        
        // Zone sous la courbe (remplissage)
        this.ctx.globalAlpha = 0.2;
        this.ctx.fillStyle = '#667eea';
        this.ctx.lineTo(x, this.height);
        this.ctx.lineTo(this.timeToX(this.points[0].time), this.height);
        this.ctx.closePath();
        this.ctx.fill();
        this.ctx.globalAlpha = 1.0;
    }

    /**
     * Dessine les points de contrôle
     */
    drawPoints() {
        this.points.forEach(point => {
            const x = this.timeToX(point.time);
            const y = this.valueToY(point.value);
            
            // Vérifier si visible
            if (x < -10 || x > this.width + 10) return;
            
            const isSelected = this.selectedPoints.has(point.id);
            const radius = isSelected ? 6 : 4;
            
            // Ombre
            this.ctx.shadowColor = 'rgba(0, 0, 0, 0.3)';
            this.ctx.shadowBlur = 4;
            this.ctx.shadowOffsetX = 0;
            this.ctx.shadowOffsetY = 2;
            
            // Point
            this.ctx.fillStyle = isSelected ? '#e74c3c' : '#667eea';
            this.ctx.beginPath();
            this.ctx.arc(x, y, radius, 0, Math.PI * 2);
            this.ctx.fill();
            
            // Bordure
            if (isSelected) {
                this.ctx.strokeStyle = '#fff';
                this.ctx.lineWidth = 2;
                this.ctx.stroke();
            }
            
            // Reset shadow
            this.ctx.shadowColor = 'transparent';
            this.ctx.shadowBlur = 0;
        });
    }

    /**
     * Dessine le highlight du point survolé
     */
    drawHoverHighlight(point) {
        const x = this.timeToX(point.time);
        const y = this.valueToY(point.value);
        
        // Cercle de highlight
        this.ctx.strokeStyle = '#fff';
        this.ctx.lineWidth = 2;
        this.ctx.beginPath();
        this.ctx.arc(x, y, 8, 0, Math.PI * 2);
        this.ctx.stroke();
        
        // Tooltip
        this.showTooltip(point, x, y);
    }

    /**
     * Dessine le rectangle de sélection
     */
    drawSelectionRect() {
        if (!this.selectionRect) return;
        
        const { x, y, width, height } = this.selectionRect;
        
        this.ctx.fillStyle = 'rgba(102, 126, 234, 0.2)';
        this.ctx.fillRect(x, y, width, height);
        
        this.ctx.strokeStyle = '#667eea';
        this.ctx.lineWidth = 2;
        this.ctx.strokeRect(x, y, width, height);
    }

    /**
     * Affiche un tooltip
     */
    showTooltip(point, x, y) {
        const text = `CC${this.ccNumber}: ${point.value} @ ${Math.round(point.time)}ms`;
        
        this.ctx.font = '11px sans-serif';
        const metrics = this.ctx.measureText(text);
        const padding = 6;
        const tooltipWidth = metrics.width + padding * 2;
        const tooltipHeight = 18;
        
        let tooltipX = x - tooltipWidth / 2;
        let tooltipY = y - tooltipHeight - 10;
        
        // Ajuster si hors écran
        if (tooltipX < 0) tooltipX = 0;
        if (tooltipX + tooltipWidth > this.width) tooltipX = this.width - tooltipWidth;
        if (tooltipY < 0) tooltipY = y + 10;
        
        // Background
        this.ctx.fillStyle = 'rgba(0, 0, 0, 0.85)';
        this.ctx.fillRect(tooltipX, tooltipY, tooltipWidth, tooltipHeight);
        
        // Text
        this.ctx.fillStyle = '#fff';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'middle';
        this.ctx.fillText(text, tooltipX + tooltipWidth / 2, tooltipY + tooltipHeight / 2);
    }

    /**
     * Attache les événements
     */
    attachEvents() {
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
        this.canvas.addEventListener('mouseleave', () => this.onMouseLeave());
        this.canvas.addEventListener('dblclick', (e) => this.onDoubleClick(e));
        
        window.addEventListener('resize', () => this.resize());
    }

    /**
     * Gestion du clic
     */
    onMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        const clickedPoint = this.findPointAt(x, y);
        
        if (clickedPoint) {
            // Sélection de point
            if (e.shiftKey) {
                if (this.selectedPoints.has(clickedPoint.id)) {
                    this.selectedPoints.delete(clickedPoint.id);
                } else {
                    this.selectedPoints.add(clickedPoint.id);
                }
            } else {
                this.selectedPoints.clear();
                this.selectedPoints.add(clickedPoint.id);
            }
            
            this.isDragging = true;
            this.dragStart = { x, y };
        } else if (e.altKey) {
            // Créer un nouveau point (Alt + Clic)
            this.createPoint(x, y);
        } else {
            // Sélection rectangle
            this.isSelecting = true;
            this.selectionRect = { x, y, width: 0, height: 0 };
            
            if (!e.shiftKey) {
                this.selectedPoints.clear();
            }
        }
        
        this.render();
    }

    /**
     * Gestion du mouvement
     */
    onMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        // Hover
        const hoveredPoint = this.findPointAt(x, y);
        if (hoveredPoint !== this.hoveredPoint) {
            this.hoveredPoint = hoveredPoint;
            this.render();
        }
        
        // Drag
        if (this.isDragging && this.dragStart && this.selectedPoints.size > 0) {
            const deltaX = x - this.dragStart.x;
            const deltaY = y - this.dragStart.y;
            
            const deltaTime = Math.round(deltaX / this.pixelsPerMs);
            const deltaValue = -Math.round((deltaY / this.height) * 127);
            
            if (Math.abs(deltaTime) >= 10 || Math.abs(deltaValue) >= 1) {
                this.moveSelectedPoints(deltaTime, deltaValue);
                this.dragStart = { x, y };
            }
        }
        
        // Sélection rectangle
        if (this.isSelecting && this.selectionRect) {
            this.selectionRect.width = x - this.selectionRect.x;
            this.selectionRect.height = y - this.selectionRect.y;
            
            this.selectPointsInRect();
            this.render();
        }
        
        // Curseur
        this.updateCursor(hoveredPoint);
    }

    /**
     * Gestion du relâchement
     */
    onMouseUp(e) {
        this.isDragging = false;
        this.isSelecting = false;
        this.selectionRect = null;
        this.dragStart = null;
        
        this.render();
    }

    /**
     * Gestion de la sortie
     */
    onMouseLeave() {
        this.hoveredPoint = null;
        this.render();
    }

    /**
     * Gestion du double-clic
     */
    onDoubleClick(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        const clickedPoint = this.findPointAt(x, y);
        
        if (clickedPoint) {
            // Supprimer le point
            this.deletePoint(clickedPoint.id);
        }
    }

    /**
     * Crée un nouveau point CC
     */
    createPoint(x, y) {
        const time = Math.round(this.xToTime(x));
        const value = Math.round(this.yToValue(y));
        
        // Vérifier qu'il n'y a pas déjà un point à ce moment
        const existing = this.points.find(p => Math.abs(p.time - time) < 10);
        if (existing) return;
        
        const newEvent = {
            type: 'cc',
            time: time,
            controller: this.ccNumber,
            value: Math.max(0, Math.min(127, value)),
            channel: 0, // Canal par défaut
            id: `cc_${Date.now()}_${Math.random()}`
        };
        
        // Ajouter à la timeline
        this.model.midiJson.timeline.push(newEvent);
        this.model.midiJson.timeline.sort((a, b) => a.time - b.time);
        
        // Sauvegarder dans l'historique
        this.model.saveToHistory(`Add CC${this.ccNumber} point`);
        
        // Rafraîchir
        this.extractPoints();
        this.render();
        
        this.model.emit('editor:cc:added', newEvent);
    }

    /**
     * Supprime un point CC
     */
    deletePoint(pointId) {
        const index = this.model.midiJson.timeline.findIndex(e => e.id === pointId);
        
        if (index !== -1) {
            this.model.midiJson.timeline.splice(index, 1);
            this.model.saveToHistory(`Delete CC${this.ccNumber} point`);
            
            this.selectedPoints.delete(pointId);
            this.extractPoints();
            this.render();
            
            this.model.emit('editor:cc:deleted', pointId);
        }
    }

    /**
     * Déplace les points sélectionnés
     */
    moveSelectedPoints(deltaTime, deltaValue) {
        const selectedIds = Array.from(this.selectedPoints);
        
        selectedIds.forEach(id => {
            const event = this.model.midiJson.timeline.find(e => e.id === id);
            
            if (event) {
                event.time = Math.max(0, event.time + deltaTime);
                event.value = Math.max(0, Math.min(127, event.value + deltaValue));
            }
        });
        
        // Retrier la timeline
        this.model.midiJson.timeline.sort((a, b) => a.time - b.time);
        
        this.extractPoints();
        this.render();
    }

    /**
     * Trouve un point aux coordonnées données
     */
    findPointAt(x, y) {
        const threshold = 10;
        
        for (const point of this.points) {
            const pointX = this.timeToX(point.time);
            const pointY = this.valueToY(point.value);
            
            const distance = Math.sqrt(
                Math.pow(x - pointX, 2) + Math.pow(y - pointY, 2)
            );
            
            if (distance <= threshold) {
                return point;
            }
        }
        
        return null;
    }

    /**
     * Sélectionne les points dans le rectangle
     */
    selectPointsInRect() {
        if (!this.selectionRect) return;
        
        const rect = this.normalizeRect(this.selectionRect);
        
        this.points.forEach(point => {
            const x = this.timeToX(point.time);
            const y = this.valueToY(point.value);
            
            if (x >= rect.x && x <= rect.x + rect.width &&
                y >= rect.y && y <= rect.y + rect.height) {
                this.selectedPoints.add(point.id);
            }
        });
    }

    /**
     * Normalise un rectangle
     */
    normalizeRect(rect) {
        const x = rect.width < 0 ? rect.x + rect.width : rect.x;
        const y = rect.height < 0 ? rect.y + rect.height : rect.y;
        const width = Math.abs(rect.width);
        const height = Math.abs(rect.height);
        
        return { x, y, width, height };
    }

    /**
     * Met à jour le curseur
     */
    updateCursor(hoveredPoint) {
        this.canvas.style.cursor = hoveredPoint ? 'pointer' : 'crosshair';
    }

    /**
     * Définit le type d'interpolation
     */
    setInterpolation(type) {
        if (['linear', 'step', 'smooth'].includes(type)) {
            this.interpolation = type;
            this.render();
        }
    }

    /**
     * Supprime les points sélectionnés
     */
    deleteSelected() {
        const selectedIds = Array.from(this.selectedPoints);
        
        if (selectedIds.length === 0) return 0;
        
        this.model.midiJson.timeline = this.model.midiJson.timeline.filter(e =>
            !selectedIds.includes(e.id)
        );
        
        this.model.saveToHistory(`Delete ${selectedIds.length} CC${this.ccNumber} point(s)`);
        
        this.selectedPoints.clear();
        this.extractPoints();
        this.render();
        
        return selectedIds.length;
    }

    // ========================================================================
    // CONVERSIONS
    // ========================================================================

    timeToX(time) {
        return (time - this.scrollX) * this.pixelsPerMs;
    }

    xToTime(x) {
        return (x / this.pixelsPerMs) + this.scrollX;
    }

    valueToY(value) {
        return this.height - (value / 127) * this.height;
    }

    yToValue(y) {
        return 127 - (y / this.height) * 127;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = CCEditorView;
}

if (typeof window !== 'undefined') {
    window.CCEditorView = CCEditorView;  // ← AJOUTÉ
}
window.CCEditorView = CCEditorView;
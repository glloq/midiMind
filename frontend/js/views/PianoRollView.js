// ============================================================================
// Fichier: frontend/js/views/PianoRollView.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Vue piano roll pour l'édition graphique des notes MIDI.
//   Affichage en grille avec timeline, notes rectangulaires,
//   clavier piano vertical, et outils d'édition.
//
// Fonctionnalités:
//   - Affichage notes en grille temporelle
//   - Clavier piano vertical (88 touches)
//   - Timeline avec mesures et temps
//   - Zoom horizontal et vertical
//   - Pan (défilement) fluide
//   - Sélection multiple notes (rectangle)
//   - Édition notes (déplacer, redimensionner)
//   - Outils : Select, Pencil, Eraser
//   - Snap to grid configurable
//   - Affichage vélocité (couleur)
//
// Architecture:
//   PianoRollView extends BaseCanvasView
//   - CoordinateSystem : Conversions pixels ↔ temps/pitch
//   - Viewport : Gestion zoom/pan
//   - NoteRenderer : Rendu notes optimisé
//   - GridRenderer : Grille et mesures
//
// Auteur: MidiMind Team
// ============================================================================


class PianoRollView extends BaseCanvasView {
    constructor(canvas, model) {
        super(canvas);
        this.model = model;
        
        // Configuration spécifique
        this.noteHeight = 12;
        this.pixelsPerMs = 0.1;
    }
    /**
     * Rendu optimisé avec JsonMidi timeline
     */
    render() {
        if (!this.editorModel || !this.editorModel.data) return;
        
        // Accès direct à la timeline JsonMidi
        const notes = this.editorModel.data.timeline.filter(e => e.type === 'noteOn');
        
        this.clearCanvas();
        this.drawGrid();
        this.drawTimeRuler();
        
        // Dessiner chaque note
        notes.forEach(note => {
            // Vérifier si note visible dans viewport
            if (!this.isNoteVisible(note)) return;
            
            const x = this.timeToX(note.time);
            const y = this.noteToY(note.note);
            const width = this.timeToX(note.time + note.duration) - x;
            const height = this.noteHeight;
            
            // Style basé sur vélocité
            this.ctx.fillStyle = this.getNoteColor(note.velocity);
            this.ctx.fillRect(x, y, width, height);
            
            // Border
            if (this.selectedNotes.has(note.id)) {
                this.ctx.strokeStyle = '#fff';
                this.ctx.lineWidth = 2;
            } else {
                this.ctx.strokeStyle = '#667eea';
                this.ctx.lineWidth = 1;
            }
            this.ctx.strokeRect(x, y, width, height);
        });
        
        // Dessiner playhead
        this.drawPlayhead();
    }
    
    /**
     * Gestion clic - Ajouter note
     */
    handleClick(e) {
        const point = this.getCanvasPoint(e);
        const time = this.xToTime(point.x);
        const note = this.yToNote(point.y);
        
        // Quantifier si activé
        const quantizedTime = this.editorModel.state.snap 
            ? this.quantizeTime(time) 
            : time;
        
        // Ajouter via EditorModel (qui gère JsonMidi)
        this.editorModel.addNote(
            quantizedTime,
            this.currentChannel,
            note,
            100,  // velocity
            480   // duration (1 beat)
        );
    }
    /**
     * Invalide le cache et force le redessinage
     */
    invalidate() {
        this.needsRedraw = true;
        this.renderCache.clear();
    }

    /**
     * Boucle de rendu
     */
    startRenderLoop() {
        const render = () => {
            if (this.needsRedraw) {
                this.render();
                this.needsRedraw = false;
            }
            requestAnimationFrame(render);
        };
        requestAnimationFrame(render);
    }


    /**
     * Dessine la grille
     */
    drawGrid() {
        const { start, end } = this.model.state.viewport;
        
        // Lignes verticales (temps)
        this.ctx.strokeStyle = '#2a2a2a';
        this.ctx.lineWidth = 1;
        
        const gridInterval = this.snapResolution;
        const startTime = Math.floor(start / gridInterval) * gridInterval;
        
        for (let time = startTime; time <= end; time += gridInterval) {
            const x = this.timeToX(time);
            
            if (x >= 0 && x <= this.width) {
                // Ligne plus épaisse tous les 4 temps
                if ((time / gridInterval) % 4 === 0) {
                    this.ctx.strokeStyle = '#3a3a3a';
                    this.ctx.lineWidth = 1.5;
                } else {
                    this.ctx.strokeStyle = '#2a2a2a';
                    this.ctx.lineWidth = 1;
                }
                
                this.ctx.beginPath();
                this.ctx.moveTo(x, 0);
                this.ctx.lineTo(x, this.height);
                this.ctx.stroke();
            }
        }
        
        // Lignes horizontales (notes)
        this.ctx.strokeStyle = '#2a2a2a';
        this.ctx.lineWidth = 1;
        
        for (let note = 0; note <= 127; note++) {
            const y = this.noteToY(note);
            
            // Ligne plus visible pour les C
            if (note % 12 === 0) {
                this.ctx.strokeStyle = '#3a3a3a';
            } else {
                this.ctx.strokeStyle = '#2a2a2a';
            }
            
            this.ctx.beginPath();
            this.ctx.moveTo(0, y);
            this.ctx.lineTo(this.width, y);
            this.ctx.stroke();
        }
    }

    /**
     * Dessine les notes
     */
    drawNotes() {
        const timeline = this.model.getFilteredTimeline();
        const notes = timeline.filter(e => e.type === 'noteOn');
        const selectedNotes = this.model.state.selectedNotes;
        
        notes.forEach(note => {
            const x = this.timeToX(note.time);
            const y = this.noteToY(note.note);
            const width = note.duration * this.pixelsPerMs;
            const height = this.noteHeight - 2;
            
            // Vérifier si visible
            if (x + width < 0 || x > this.width || 
                y + height < 0 || y > this.height) {
                return;
            }
            
            const isSelected = selectedNotes.has(note.id);
            
            // Couleur selon le canal
            const channelColor = this.getChannelColor(note.channel);
            
            // Dessiner la note
            this.ctx.fillStyle = isSelected ? '#667eea' : channelColor;
            this.ctx.fillRect(x, y, width, height);
            
            // Border pour les notes sélectionnées
            if (isSelected) {
                this.ctx.strokeStyle = '#fff';
                this.ctx.lineWidth = 2;
                this.ctx.strokeRect(x, y, width, height);
            }
            
            // Barre de vélocité
            if (this.model.state.velocityView) {
                const velocityHeight = (note.velocity / 127) * height;
                this.ctx.fillStyle = 'rgba(255,255,255,0.3)';
                this.ctx.fillRect(x, y + height - velocityHeight, 3, velocityHeight);
            }
            
            // Nom de la note si assez large
            if (width > 40) {
                this.ctx.fillStyle = '#fff';
                this.ctx.font = '10px sans-serif';
                this.ctx.textAlign = 'left';
                this.ctx.textBaseline = 'middle';
                
                const noteName = this.getNoteName(note.note);
                this.ctx.fillText(noteName, x + 4, y + height / 2);
            }
            
            // Poignée de redimensionnement (si sélectionnée)
            if (isSelected && width > 10) {
                this.ctx.fillStyle = '#fff';
                this.ctx.fillRect(x + width - 4, y + height / 2 - 2, 4, 4);
            }
        });
    }

    /**
     * Dessine le rectangle de sélection
     */
    drawSelectionRect() {
        if (!this.selectionRect) return;
        
        const { x, y, width, height } = this.selectionRect;
        
        // Rectangle avec transparence
        this.ctx.fillStyle = 'rgba(102, 126, 234, 0.2)';
        this.ctx.fillRect(x, y, width, height);
        
        // Bordure
        this.ctx.strokeStyle = '#667eea';
        this.ctx.lineWidth = 2;
        this.ctx.strokeRect(x, y, width, height);
    }

    /**
     * Dessine le highlight de la note survolée
     */
    drawHoverHighlight(note) {
        const x = this.timeToX(note.time);
        const y = this.noteToY(note.note);
        const width = note.duration * this.pixelsPerMs;
        const height = this.noteHeight - 2;
        
        this.ctx.strokeStyle = '#fff';
        this.ctx.lineWidth = 2;
        this.ctx.strokeRect(x - 1, y - 1, width + 2, height + 2);
        
        // Tooltip
        this.showTooltip(note, x, y);
    }

    /**
     * Dessine le playhead
     */
    drawPlayhead(time) {
        const x = this.timeToX(time);
        
        if (x >= 0 && x <= this.width) {
            this.ctx.strokeStyle = '#e74c3c';
            this.ctx.lineWidth = 2;
            this.ctx.beginPath();
            this.ctx.moveTo(x, 0);
            this.ctx.lineTo(x, this.height);
            this.ctx.stroke();
        }
    }

    /**
     * Affiche un tooltip pour une note
     */
    showTooltip(note, x, y) {
        const text = `${this.getNoteName(note.note)} | Vel: ${note.velocity} | Dur: ${note.duration}ms`;
        
        this.ctx.font = '12px sans-serif';
        const metrics = this.ctx.measureText(text);
        const padding = 6;
        const tooltipWidth = metrics.width + padding * 2;
        const tooltipHeight = 20;
        
        // Position du tooltip
        let tooltipX = x;
        let tooltipY = y - tooltipHeight - 5;
        
        // Ajuster si hors écran
        if (tooltipX + tooltipWidth > this.width) {
            tooltipX = this.width - tooltipWidth;
        }
        if (tooltipY < 0) {
            tooltipY = y + this.noteHeight + 5;
        }
        
        // Background
        this.ctx.fillStyle = 'rgba(0, 0, 0, 0.8)';
        this.ctx.fillRect(tooltipX, tooltipY, tooltipWidth, tooltipHeight);
        
        // Text
        this.ctx.fillStyle = '#fff';
        this.ctx.textAlign = 'left';
        this.ctx.textBaseline = 'middle';
        this.ctx.fillText(text, tooltipX + padding, tooltipY + tooltipHeight / 2);
    }

    /**
     * Attache les événements
     */
    attachEvents() {
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
        this.canvas.addEventListener('wheel', (e) => this.onWheel(e));
        this.canvas.addEventListener('mouseleave', () => this.onMouseLeave());
        
        window.addEventListener('resize', () => this.resize());
    }

    /**
     * Gestion du clic
     */
    onMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        const clickedNote = this.findNoteAt(x, y);
        const tool = this.model.state.currentTool;
        
        switch (tool) {
            case 'select':
                if (clickedNote) {
                    // Vérifier si on clique sur la poignée de redimensionnement
                    if (this.isResizeHandle(clickedNote, x, y)) {
                        this.resizingNote = clickedNote;
                    } else {
                        // Sélection de note
                        if (e.shiftKey) {
                            // Ajouter à la sélection
                            this.model.selectNotes([clickedNote.id], true);
                        } else if (!this.model.state.selectedNotes.has(clickedNote.id)) {
                            // Nouvelle sélection
                            this.model.selectNotes([clickedNote.id]);
                        }
                        
                        this.isDragging = true;
                        this.dragStart = { x, y };
                    }
                } else {
                    // Démarrer sélection rectangle
                    this.isSelecting = true;
                    this.selectionRect = { x, y, width: 0, height: 0 };
                    
                    if (!e.shiftKey) {
                        this.model.clearSelection();
                    }
                }
                break;
                
            case 'pencil':
                if (!clickedNote) {
                    // Créer une nouvelle note
                    const time = this.xToTime(x);
                    const note = this.yToNote(y);
                    const duration = this.snapEnabled ? this.snapResolution : 500;
                    
                    this.model.addNote(time, note, 64, duration, 0);
                }
                break;
                
            case 'eraser':
                if (clickedNote) {
                    this.model.deleteNote(clickedNote.id);
                }
                break;
        }
        
        this.invalidate();
    }

    /**
     * Gestion du mouvement de souris
     */
    onMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        // Mise à jour de la position du curseur dans la status bar
        const time = Math.round(this.xToTime(x));
        const note = this.yToNote(y);
        const noteName = this.getNoteName(note);
        
        document.getElementById('cursorPosition').textContent = 
            `Time: ${time}ms | Note: ${noteName}`;
        
        // Hover
        const hoveredNote = this.findNoteAt(x, y);
        if (hoveredNote !== this.hoveredNote) {
            this.hoveredNote = hoveredNote;
            this.invalidate();
        }
        
        // Drag
        if (this.isDragging && this.dragStart) {
            const deltaX = x - this.dragStart.x;
            const deltaY = y - this.dragStart.y;
            
            const deltaTime = Math.round(deltaX / this.pixelsPerMs);
            const deltaNotes = -Math.round(deltaY / this.noteHeight);
            
            if (Math.abs(deltaTime) >= 10 || Math.abs(deltaNotes) >= 1) {
                // Déplacer les notes sélectionnées
                this.moveSelectedNotes(deltaTime, deltaNotes);
                this.dragStart = { x, y };
            }
        }
        
        // Resize
        if (this.resizingNote) {
            const noteX = this.timeToX(this.resizingNote.time);
            const newDuration = Math.round((x - noteX) / this.pixelsPerMs);
            
            if (newDuration > 10) {
                this.model.updateNote(this.resizingNote.id, {
                    duration: this.snapEnabled 
                        ? Math.round(newDuration / this.snapResolution) * this.snapResolution
                        : newDuration
                });
            }
        }
        
        // Sélection rectangle
        if (this.isSelecting && this.selectionRect) {
            this.selectionRect.width = x - this.selectionRect.x;
            this.selectionRect.height = y - this.selectionRect.y;
            
            // Sélectionner les notes dans le rectangle
            this.selectNotesInRect();
            this.invalidate();
        }
        
        // Curseur
        this.updateCursor(x, y, hoveredNote);
    }

    /**
     * Gestion du relâchement de souris
     */
    onMouseUp(e) {
        this.isDragging = false;
        this.isSelecting = false;
        this.resizingNote = null;
        this.selectionRect = null;
        this.dragStart = null;
        
        this.invalidate();
    }

    /**
     * Gestion du scroll
     */
    onWheel(e) {
        e.preventDefault();
        
        if (e.ctrlKey) {
            // Zoom
            const delta = e.deltaY > 0 ? 0.9 : 1.1;
            const newZoom = this.model.state.zoom.x * delta;
            
            editorController.setZoom(newZoom, null);
        } else {
            // Scroll horizontal
            const delta = e.deltaY;
            this.scrollX += delta;
            
            // Limiter le scroll
            this.scrollX = Math.max(0, this.scrollX);
            
            this.invalidate();
        }
    }

    /**
     * Gestion de la sortie de souris
     */
    onMouseLeave() {
        this.hoveredNote = null;
        this.invalidate();
    }

    /**
     * Trouve une note aux coordonnées données
     */
    findNoteAt(x, y) {
        const timeline = this.model.getFilteredTimeline();
        const notes = timeline.filter(e => e.type === 'noteOn');
        
        for (let i = notes.length - 1; i >= 0; i--) {
            const note = notes[i];
            const noteX = this.timeToX(note.time);
            const noteY = this.noteToY(note.note);
            const noteWidth = note.duration * this.pixelsPerMs;
            const noteHeight = this.noteHeight - 2;
            
            if (x >= noteX && x <= noteX + noteWidth &&
                y >= noteY && y <= noteY + noteHeight) {
                return note;
            }
        }
        
        return null;
    }

    /**
     * Vérifie si on clique sur la poignée de redimensionnement
     */
    isResizeHandle(note, x, y) {
        const noteX = this.timeToX(note.time);
        const noteY = this.noteToY(note.note);
        const noteWidth = note.duration * this.pixelsPerMs;
        const noteHeight = this.noteHeight - 2;
        
        const handleX = noteX + noteWidth - 4;
        const handleY = noteY + noteHeight / 2 - 2;
        
        return x >= handleX - 4 && x <= handleX + 8 &&
               y >= handleY - 4 && y <= handleY + 8;
    }

    /**
     * Déplace les notes sélectionnées
     */
    moveSelectedNotes(deltaTime, deltaNotes) {
        const selectedIds = Array.from(this.model.state.selectedNotes);
        
        selectedIds.forEach(id => {
            const note = this.model.midiJson.timeline.find(e => e.id === id);
            
            if (note) {
                let newTime = note.time + deltaTime;
                let newNote = note.note + deltaNotes;
                
                // Snap
                if (this.snapEnabled) {
                    newTime = Math.round(newTime / this.snapResolution) * this.snapResolution;
                }
                
                // Limites
                newTime = Math.max(0, newTime);
                newNote = Math.max(0, Math.min(127, newNote));
                
                this.model.updateNote(id, {
                    time: newTime,
                    note: newNote
                });
            }
        });
    }

    /**
     * Sélectionne les notes dans le rectangle de sélection
     */
    selectNotesInRect() {
        if (!this.selectionRect) return;
        
        const rect = this.normalizeRect(this.selectionRect);
        const timeline = this.model.getFilteredTimeline();
        const notes = timeline.filter(e => e.type === 'noteOn');
        
        const selectedIds = [];
        
        notes.forEach(note => {
            const noteX = this.timeToX(note.time);
            const noteY = this.noteToY(note.note);
            const noteWidth = note.duration * this.pixelsPerMs;
            const noteHeight = this.noteHeight - 2;
            
            // Vérifier intersection
            if (this.rectsIntersect(
                rect,
                { x: noteX, y: noteY, width: noteWidth, height: noteHeight }
            )) {
                selectedIds.push(note.id);
            }
        });
        
        this.model.selectNotes(selectedIds, true);
    }

    /**
     * Normalise un rectangle (gère width/height négatifs)
     */
    normalizeRect(rect) {
        const x = rect.width < 0 ? rect.x + rect.width : rect.x;
        const y = rect.height < 0 ? rect.y + rect.height : rect.y;
        const width = Math.abs(rect.width);
        const height = Math.abs(rect.height);
        
        return { x, y, width, height };
    }

    /**
     * Vérifie l'intersection de deux rectangles
     */
    rectsIntersect(r1, r2) {
        return !(r1.x + r1.width < r2.x || 
                 r2.x + r2.width < r1.x ||
                 r1.y + r1.height < r2.y ||
                 r2.y + r2.height < r1.y);
    }

    /**
     * Met à jour le curseur selon le contexte
     */
    updateCursor(x, y, hoveredNote) {
        let cursor = 'default';
        
        const tool = this.model.state.currentTool;
        
        switch (tool) {
            case 'select':
                if (hoveredNote) {
                    if (this.isResizeHandle(hoveredNote, x, y)) {
                        cursor = 'ew-resize';
                    } else {
                        cursor = 'move';
                    }
                } else {
                    cursor = 'crosshair';
                }
                break;
            case 'pencil':
                cursor = 'crosshair';
                break;
            case 'eraser':
                cursor = 'not-allowed';
                break;
        }
        
        this.canvas.style.cursor = cursor;
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

    noteToY(note) {
        return (127 - note) * this.noteHeight - this.scrollY;
    }

    yToNote(y) {
        return 127 - Math.floor((y + this.scrollY) / this.noteHeight);
    }

    getChannelColor(channel) {
        const colors = [
            '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A',
            '#98D8C8', '#F7DC6F', '#BB8FCE', '#85C1E2',
            '#F8B739', '#52C7B8', '#FF8C94', '#A8E6CF',
            '#FFD93D', '#BCB3E5', '#FAB1A0', '#81C784'
        ];
        return colors[channel % colors.length];
    }

    getNoteName(midiNote) {
        const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(midiNote / 12) - 1;
        return `${names[midiNote % 12]}${octave}`;
    }
}
if (typeof module !== 'undefined' && module.exports) {
    module.exports = PianoRollView;
}

if (typeof window !== 'undefined') {
    window.PianoRollView = PianoRollView;  // ← AJOUTÉ
}

// Export par défaut
window.PianoRollView = PianoRollView;
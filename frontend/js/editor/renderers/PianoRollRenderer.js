// ============================================================================
// Fichier: frontend/js/editor/renderers/PianoRollRenderer.js
// Projet: MidiMind v3.2.1 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.2.1 (Complétée selon audit 2025-10-14)
// Date: 2025-10-14
// ============================================================================
// Description:
//   Renderer du clavier piano vertical (88 touches).
//   Affiche les touches blanches et noires avec highlight notes actives.
//
// Fonctionnalités:
//   - Clavier piano vertical (88 touches standard)
//   - Touches blanches et noires (proportion correcte)
//   - Highlight notes actives (playback)
//   - Hover effect
//   - Labels octaves (C0, C1, C2...)
//   - Labels notes (optionnel)
//   - Click detection
//
// Corrections v3.2.1:
//   ✅ renderKey() - Rendu complet avec proportions correctes
//   ✅ isBlackKey() - Détection touches noires
//   ✅ getWidth() - Retourne largeur piano roll
//   ✅ setActiveNotes() - Mise à jour notes actives pour playback
//   ✅ Amélioration renderLabel() - Labels octaves
//
// Architecture:
//   PianoRollRenderer (classe)
//   - Rendu optimisé par batching (blanches puis noires)
//   - Gestion état notes actives
//   - Interaction hover/click
//
// Auteur: MidiMind Team
// ============================================================================

class PianoRollRenderer {
    constructor(config = {}) {
        this.config = {
            width: config.width || 80,
            
            // Couleurs
            whiteKeyColor: '#ecf0f1',
            blackKeyColor: '#34495e',
            activeKeyColor: '#667eea',
            hoverKeyColor: '#bdc3c7',
            borderColor: '#333',
            borderColorActive: '#667eea',
            textColor: '#2c3e50',
            textColorBlack: '#ecf0f1',
            
            // Affichage
            showOctaveLabels: true,
            showNoteNames: true,
            showAllNoteNames: false, // Si false, seulement les C
            
            // Proportions
            blackKeyWidthRatio: 0.6, // Touches noires = 60% de la largeur
            blackKeyOverlap: 5 // pixels d'overlap sur les touches blanches
        };
        
        // État
        this.activeNotes = new Set();
        this.hoveredNote = null;
    }

    // ========================================================================
    // RENDU PRINCIPAL
    // ========================================================================

    /**
     * Rendu du piano roll complet
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     * @param {Object} viewport - Viewport actuel
     * @param {Object} coordSystem - Système de coordonnées
     * @param {number} height - Hauteur totale
     */
    render(ctx, viewport, coordSystem, height) {
        const visibleRect = viewport.getVisibleRect();
        const noteRange = visibleRect.noteRange;
        
        if (!noteRange) return;
        
        ctx.save();
        
        // Fond
        ctx.fillStyle = '#1a1a1a';
        ctx.fillRect(0, 0, this.config.width, height);
        
        // Dessiner touches blanches d'abord (background)
        for (let note = noteRange.min; note <= noteRange.max; note++) {
            if (!this.isBlackKey(note)) {
                this.renderKey(ctx, note, coordSystem, false);
            }
        }
        
        // Dessiner touches noires par dessus (foreground)
        for (let note = noteRange.min; note <= noteRange.max; note++) {
            if (this.isBlackKey(note)) {
                this.renderKey(ctx, note, coordSystem, true);
            }
        }
        
        // Bordure droite
        ctx.strokeStyle = this.config.borderColor;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(this.config.width - 0.5, 0);
        ctx.lineTo(this.config.width - 0.5, height);
        ctx.stroke();
        
        ctx.restore();
    }

    /**
     * ✅ COMPLET: Rendu d'une touche de piano
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     * @param {number} note - Numéro MIDI de la note (0-127)
     * @param {Object} coordSystem - Système de coordonnées
     * @param {boolean} isBlack - Est une touche noire
     */
    renderKey(ctx, note, coordSystem, isBlack) {
        const y = coordSystem.noteToY(note);
        const height = coordSystem.noteHeight();
        
        // États
        const isActive = this.activeNotes.has(note);
        const isHovered = this.hoveredNote === note;
        
        // Déterminer la couleur
        let color;
        if (isActive) {
            color = this.config.activeKeyColor;
        } else if (isHovered) {
            color = this.config.hoverKeyColor;
        } else if (isBlack) {
            color = this.config.blackKeyColor;
        } else {
            color = this.config.whiteKeyColor;
        }
        
        // Largeur selon type de touche
        const width = isBlack ? 
            this.config.width * this.config.blackKeyWidthRatio : 
            this.config.width;
        
        // Dessiner la touche
        ctx.fillStyle = color;
        ctx.fillRect(0, y, width, height);
        
        // Bordure
        const borderColor = isActive ? 
            this.config.borderColorActive : 
            this.config.borderColor;
        const borderWidth = isActive ? 2 : 1;
        
        ctx.strokeStyle = borderColor;
        ctx.lineWidth = borderWidth;
        ctx.strokeRect(0, y, width, height);
        
        // Label (selon configuration)
        const noteName = this.getNoteName(note);
        const shouldShowLabel = this.config.showAllNoteNames || 
                               (this.config.showNoteNames && noteName.startsWith('C')) ||
                               isHovered;
        
        if (shouldShowLabel && height > 12) {
            this.renderLabel(ctx, note, y, height, width, isBlack);
        }
    }

    /**
     * ✅ AMÉLIORÉ: Rendu du label de note
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     * @param {number} note - Numéro MIDI
     * @param {number} y - Position Y
     * @param {number} height - Hauteur touche
     * @param {number} width - Largeur touche
     * @param {boolean} isBlack - Est une touche noire
     */
    renderLabel(ctx, note, y, height, width, isBlack) {
        const noteName = this.getNoteName(note);
        
        ctx.save();
        ctx.fillStyle = isBlack ? this.config.textColorBlack : this.config.textColor;
        ctx.font = height > 20 ? '11px monospace' : '9px monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        
        // Afficher nom complet avec octave pour les C
        if (this.config.showOctaveLabels && noteName.startsWith('C')) {
            const octave = Math.floor(note / 12) - 1;
            ctx.fillText(`C${octave}`, width / 2, y + height / 2);
        } else {
            // Juste le nom de note
            ctx.fillText(noteName.replace(/[0-9]/g, ''), width / 2, y + height / 2);
        }
        
        ctx.restore();
    }

    // ========================================================================
    // NOTES ACTIVES (PLAYBACK)
    // ========================================================================

    /**
     * ✅ COMPLET: Définit les notes actives pour le playback
     * @param {Array} notes - Liste des notes actives [{note, velocity, ...}]
     */
    setActiveNotes(notes) {
        this.activeNotes.clear();
        
        if (notes && Array.isArray(notes)) {
            notes.forEach(noteObj => {
                // Gérer objet note ou numéro direct
                const noteNumber = typeof noteObj === 'object' ? noteObj.note : noteObj;
                if (noteNumber !== undefined && noteNumber !== null) {
                    this.activeNotes.add(noteNumber);
                }
            });
        }
    }

    /**
     * ✅ NOUVEAU: Ajoute une note active
     * @param {number} note - Numéro MIDI
     */
    addActiveNote(note) {
        this.activeNotes.add(note);
    }

    /**
     * ✅ NOUVEAU: Retire une note active
     * @param {number} note - Numéro MIDI
     */
    removeActiveNote(note) {
        this.activeNotes.delete(note);
    }

    /**
     * ✅ NOUVEAU: Efface toutes les notes actives
     */
    clearActiveNotes() {
        this.activeNotes.clear();
    }

    /**
     * Définit la note survolée
     * @param {number|null} note - Numéro MIDI ou null
     */
    setHoveredNote(note) {
        this.hoveredNote = note;
    }

    // ========================================================================
    // INTERACTION
    // ========================================================================

    /**
     * Trouve la note à une position donnée
     * @param {number} x - Position X
     * @param {number} y - Position Y
     * @param {Object} coordSystem - Système de coordonnées
     * @returns {number|null} Numéro MIDI ou null
     */
    findNoteAt(x, y, coordSystem) {
        // Vérifier si dans la zone du piano roll
        if (x < 0 || x > this.config.width) return null;
        
        const note = coordSystem.yToNote(y);
        
        // Vérifier si note valide
        if (note < 0 || note > 127) return null;
        
        // Pour les touches noires, vérifier si X dans la zone plus étroite
        if (this.isBlackKey(note)) {
            const blackKeyWidth = this.config.width * this.config.blackKeyWidthRatio;
            if (x > blackKeyWidth) {
                // Clic sur zone blanche à droite d'une noire
                // Trouver la touche blanche en dessous
                return this.findWhiteKeyBelow(note, y, coordSystem);
            }
        }
        
        return note;
    }

    /**
     * ✅ NOUVEAU: Trouve la touche blanche sous une position
     * @param {number} blackNote - Note noire actuelle
     * @param {number} y - Position Y
     * @param {Object} coordSystem - Système de coordonnées
     * @returns {number} Note blanche
     */
    findWhiteKeyBelow(blackNote, y, coordSystem) {
        // Chercher la touche blanche la plus proche
        for (let offset = 0; offset <= 1; offset++) {
            const testNote = blackNote - offset;
            if (testNote >= 0 && !this.isBlackKey(testNote)) {
                const noteY = coordSystem.noteToY(testNote);
                const noteHeight = coordSystem.noteHeight();
                if (y >= noteY && y < noteY + noteHeight) {
                    return testNote;
                }
            }
        }
        return blackNote; // Fallback
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    /**
     * ✅ COMPLET: Vérifie si une note est une touche noire
     * @param {number} note - Numéro MIDI (0-127)
     * @returns {boolean} true si touche noire
     */
    isBlackKey(note) {
        const pitchClass = note % 12;
        // C#=1, D#=3, F#=6, G#=8, A#=10
        return [1, 3, 6, 8, 10].includes(pitchClass);
    }

    /**
     * Obtient le nom d'une note MIDI
     * @param {number} note - Numéro MIDI (0-127)
     * @returns {string} Nom complet (ex: "C4", "G#5")
     */
    getNoteName(note) {
        const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(note / 12) - 1;
        const noteName = noteNames[note % 12];
        return `${noteName}${octave}`;
    }

    /**
     * Ajuste la luminosité d'une couleur
     * @param {string} color - Couleur hex ou rgb
     * @param {number} factor - Facteur de luminosité (>1 = plus clair)
     * @returns {string} Couleur ajustée
     */
    adjustBrightness(color, factor) {
        // Pour couleurs hex
        if (color.startsWith('#')) {
            const hex = color.replace('#', '');
            const r = Math.min(255, Math.floor(parseInt(hex.substr(0, 2), 16) * factor));
            const g = Math.min(255, Math.floor(parseInt(hex.substr(2, 2), 16) * factor));
            const b = Math.min(255, Math.floor(parseInt(hex.substr(4, 2), 16) * factor));
            return `rgb(${r}, ${g}, ${b})`;
        }
        
        // Pour couleurs rgb
        const match = color.match(/rgb\((\d+),\s*(\d+),\s*(\d+)\)/);
        if (match) {
            const r = Math.min(255, Math.floor(parseInt(match[1]) * factor));
            const g = Math.min(255, Math.floor(parseInt(match[2]) * factor));
            const b = Math.min(255, Math.floor(parseInt(match[3]) * factor));
            return `rgb(${r}, ${g}, ${b})`;
        }
        
        return color;
    }

    /**
     * ✅ COMPLET: Obtient la largeur du piano roll
     * @returns {number} Largeur en pixels
     */
    getWidth() {
        return this.config.width;
    }

    /**
     * Définit la largeur du piano roll
     * @param {number} width - Nouvelle largeur
     */
    setWidth(width) {
        this.config.width = width;
    }

    /**
     * Définit la visibilité
     * @param {boolean} visible - Visibilité
     */
    setVisible(visible) {
        // Le piano roll est toujours visible si activé
    }

    /**
     * Configure les options
     * @param {Object} options - Options à modifier
     */
    setConfig(options) {
        Object.assign(this.config, options);
    }

    /**
     * ✅ NOUVEAU: Obtient l'état actuel
     * @returns {Object} État {activeNotes, hoveredNote}
     */
    getState() {
        return {
            activeNotes: Array.from(this.activeNotes),
            hoveredNote: this.hoveredNote
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PianoRollRenderer;
}
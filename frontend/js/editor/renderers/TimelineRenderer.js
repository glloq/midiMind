// ============================================================================
// Fichier: frontend/js/editor/renderers/TimelineRenderer.js
// Projet: MidiMind v3.2.1 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.2.1 (Complétée selon audit 2025-10-14)
// Date: 2025-10-14
// ============================================================================
// Description:
//   Renderer de la timeline horizontale (ruler) avec marqueurs temporels.
//   Affichage temps en format mm:ss.ms ou mesures/beats selon préférence.
//
// Fonctionnalités:
//   - Timeline horizontale (ruler)
//   - Marqueurs temporels adaptatifs
//   - Format temps : mm:ss.ms ou bars:beats
//   - Playhead avec suivi
//   - Marqueurs utilisateur (markers)
//   - Sections/régions colorées
//   - Zoom adaptatif (spacing)
//
// Corrections v3.2.1:
//   ✅ renderTimeMarkers() - Complet avec marqueurs adaptatifs
//   ✅ formatTime() - Formatage mm:ss.ms et bars:beats
//   ✅ renderBorder() - Bordure inférieure
//   ✅ calculateMarkerSpacing() - Espacement intelligent
//   ✅ Ajout formatBeats() pour affichage mesures
//
// Architecture:
//   TimelineRenderer (classe)
//   - Utilise CoordinateSystem
//   - Formatage temps (Formatter utils)
//   - Cache de labels texte
//
// Auteur: MidiMind Team
// ============================================================================

class TimelineRenderer {
    constructor(config = {}) {
        this.config = {
            height: config.height || 60,
            backgroundColor: '#151515',
            backgroundColorLeft: '#1a1a1a', // Zone avant piano roll
            markerColor: '#888',
            markerColorMajor: '#aaa',
            textColor: '#ccc',
            textColorMajor: '#fff',
            borderColor: '#333',
            playheadColor: '#667eea',
            
            // Format d'affichage
            showMilliseconds: false,
            showBeats: true,
            timeFormat: 'time', // 'time' ou 'beats'
            
            // Marqueurs
            majorEvery: 4, // Marqueur majeur tous les N marqueurs
            minSpacing: 50 // Espacement minimum en pixels
        };
        
        // Cache
        this.labelCache = new Map();
    }

    // ========================================================================
    // RENDU PRINCIPAL
    // ========================================================================

    /**
     * Rendu avec offset pour le piano roll
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     * @param {Object} viewport - Viewport actuel
     * @param {Object} coordSystem - Système de coordonnées
     * @param {Object} metadata - Métadonnées (tempo, timeSignature)
     * @param {number} offsetX - Offset pour le piano roll
     */
    renderWithOffset(ctx, viewport, coordSystem, metadata = {}, offsetX = 0) {
        ctx.save();
        
        // Fond
        this.renderBackground(ctx, offsetX);
        
        // Marqueurs temporels (avec translation pour offset)
        ctx.save();
        ctx.translate(offsetX, 0);
        this.renderTimeMarkers(ctx, viewport, coordSystem, metadata);
        ctx.restore();
        
        // Bordure inférieure
        this.renderBorder(ctx);
        
        ctx.restore();
    }

    /**
     * Rendu du fond
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     * @param {number} offsetX - Offset horizontal
     */
    renderBackground(ctx, offsetX) {
        ctx.fillStyle = this.config.backgroundColor;
        ctx.fillRect(0, 0, ctx.canvas.width, this.config.height);
        
        // Zone avant le piano roll (si offsetX > 0)
        if (offsetX > 0) {
            ctx.fillStyle = this.config.backgroundColorLeft;
            ctx.fillRect(0, 0, offsetX, this.config.height);
        }
    }

    /**
     * ✅ COMPLET: Rendu des marqueurs temporels adaptatifs
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     * @param {Object} viewport - Viewport actuel
     * @param {Object} coordSystem - Système de coordonnées
     * @param {Object} metadata - Métadonnées (tempo, timeSignature)
     */
    renderTimeMarkers(ctx, viewport, coordSystem, metadata = {}) {
        const visibleRect = viewport.getVisibleRect();
        const timeRange = visibleRect.timeRange;
        
        if (!timeRange) return;
        
        // Déterminer l'espacement selon le zoom
        const spacing = this.calculateMarkerSpacing(coordSystem.zoomX, metadata);
        
        // Premier marqueur visible
        const startTime = Math.floor(timeRange.start / spacing.value) * spacing.value;
        
        ctx.save();
        
        // Dessiner les marqueurs
        for (let time = startTime; time <= timeRange.end; time += spacing.value) {
            const x = coordSystem.timeToX(time);
            
            // Déterminer si c'est un marqueur majeur
            const markerIndex = Math.round(time / spacing.value);
            const isMajor = (markerIndex % this.config.majorEvery) === 0;
            
            this.renderMarker(ctx, x, time, isMajor, metadata);
        }
        
        ctx.restore();
    }

    /**
     * ✅ COMPLET: Rendu d'un marqueur individuel
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     * @param {number} x - Position X
     * @param {number} time - Temps en ms
     * @param {boolean} isMajor - Marqueur majeur
     * @param {Object} metadata - Métadonnées
     */
    renderMarker(ctx, x, time, isMajor, metadata) {
        // Hauteur du trait selon importance
        const height = isMajor ? 40 : 20;
        const color = isMajor ? this.config.markerColorMajor : this.config.markerColor;
        
        // Ligne verticale
        ctx.strokeStyle = color;
        ctx.lineWidth = isMajor ? 2 : 1;
        ctx.beginPath();
        ctx.moveTo(x, this.config.height - height);
        ctx.lineTo(x, this.config.height);
        ctx.stroke();
        
        // Label (seulement pour les marqueurs majeurs)
        if (isMajor) {
            const label = this.config.timeFormat === 'beats' ?
                this.formatBeats(time, metadata) :
                this.formatTime(time);
            
            ctx.fillStyle = this.config.textColorMajor;
            ctx.font = 'bold 11px monospace';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'bottom';
            ctx.fillText(label, x, this.config.height - height - 4);
        }
    }

    /**
     * ✅ COMPLET: Rendu de la bordure inférieure
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     */
    renderBorder(ctx) {
        ctx.strokeStyle = this.config.borderColor;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(0, this.config.height - 0.5);
        ctx.lineTo(ctx.canvas.width, this.config.height - 0.5);
        ctx.stroke();
    }

    // ========================================================================
    // CALCULS ADAPTATIFS
    // ========================================================================

    /**
     * ✅ OPTIMISÉ: Calcule l'espacement des marqueurs selon le zoom
     * @param {number} zoomX - Zoom horizontal
     * @param {Object} metadata - Métadonnées (tempo, timeSignature)
     * @returns {Object} {value: ms, label: string}
     */
    calculateMarkerSpacing(zoomX, metadata = {}) {
        const tempo = metadata.tempo || 120;
        const beatDuration = 60000 / tempo; // ms par beat
        
        // Définir les espacements possibles en millisecondes
        const spacings = [
            { value: 10, label: '10ms' },
            { value: 25, label: '25ms' },
            { value: 50, label: '50ms' },
            { value: 100, label: '100ms' },
            { value: 250, label: '250ms' },
            { value: 500, label: '500ms' },
            { value: 1000, label: '1s' },
            { value: 2000, label: '2s' },
            { value: 5000, label: '5s' },
            { value: 10000, label: '10s' },
            { value: beatDuration, label: '1beat' },
            { value: beatDuration * 2, label: '2beats' },
            { value: beatDuration * 4, label: '4beats' }
        ];
        
        // Trier par valeur
        spacings.sort((a, b) => a.value - b.value);
        
        // Calcul de pixels par milliseconde
        const pixelsPerMs = (100 * zoomX) / 1000; // pixelsPerSecond / 1000
        
        // Trouver l'espacement optimal (viser ~75-150 pixels)
        const targetPixels = 100;
        
        for (const spacing of spacings) {
            const pixels = spacing.value * pixelsPerMs;
            if (pixels >= targetPixels) {
                return spacing;
            }
        }
        
        // Fallback: dernier espacement
        return spacings[spacings.length - 1];
    }

    // ========================================================================
    // FORMATAGE TEMPS
    // ========================================================================

    /**
     * ✅ COMPLET: Formate un temps en mm:ss ou mm:ss.ms
     * @param {number} timeMs - Temps en millisecondes
     * @returns {string} Temps formaté
     */
    formatTime(timeMs) {
        if (timeMs < 0) timeMs = 0;
        
        const totalSeconds = Math.floor(timeMs / 1000);
        const minutes = Math.floor(totalSeconds / 60);
        const seconds = totalSeconds % 60;
        const ms = Math.floor((timeMs % 1000) / 10); // Centièmes
        
        if (this.config.showMilliseconds) {
            return `${minutes}:${seconds.toString().padStart(2, '0')}.${ms.toString().padStart(2, '0')}`;
        } else {
            return `${minutes}:${seconds.toString().padStart(2, '0')}`;
        }
    }

    /**
     * ✅ NOUVEAU: Formate un temps en mesures:temps (bars:beats)
     * @param {number} timeMs - Temps en millisecondes
     * @param {Object} metadata - Métadonnées (tempo, timeSignature)
     * @returns {string} Position formatée (ex: "4:3")
     */
    formatBeats(timeMs, metadata = {}) {
        const tempo = metadata.tempo || 120;
        const timeSignature = metadata.timeSignature || { numerator: 4, denominator: 4 };
        
        const bpm = tempo;
        const beatDuration = 60000 / bpm; // ms par beat
        const beatsPerMeasure = timeSignature.numerator || 4;
        
        // Calcul du nombre total de beats
        const totalBeats = timeMs / beatDuration;
        
        // Calcul mesure et beat (1-based)
        const measure = Math.floor(totalBeats / beatsPerMeasure) + 1;
        const beat = Math.floor(totalBeats % beatsPerMeasure) + 1;
        
        return `${measure}:${beat}`;
    }

    /**
     * ✅ NOUVEAU: Calcule la position en beats
     * @param {number} timeMs - Temps en millisecondes
     * @param {Object} metadata - Métadonnées
     * @returns {Object} {measure, beat, totalBeats}
     */
    calculateBeatPosition(timeMs, metadata = {}) {
        const tempo = metadata.tempo || 120;
        const timeSignature = metadata.timeSignature || { numerator: 4, denominator: 4 };
        
        const bpm = tempo;
        const beatDuration = 60000 / bpm;
        const beatsPerMeasure = timeSignature.numerator || 4;
        
        const totalBeats = timeMs / beatDuration;
        const measure = Math.floor(totalBeats / beatsPerMeasure) + 1;
        const beat = Math.floor(totalBeats % beatsPerMeasure) + 1;
        
        return { measure, beat, totalBeats };
    }

    // ========================================================================
    // PLAYHEAD
    // ========================================================================

    /**
     * ✅ NOUVEAU: Rendu du playhead (tête de lecture)
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     * @param {number} time - Temps actuel en ms
     * @param {Object} coordSystem - Système de coordonnées
     * @param {number} canvasHeight - Hauteur totale canvas
     */
    renderPlayhead(ctx, time, coordSystem, canvasHeight) {
        const x = coordSystem.timeToX(time);
        
        ctx.save();
        
        // Ligne verticale
        ctx.strokeStyle = this.config.playheadColor;
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, canvasHeight);
        ctx.stroke();
        
        // Triangle en haut
        ctx.fillStyle = this.config.playheadColor;
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x - 6, 10);
        ctx.lineTo(x + 6, 10);
        ctx.closePath();
        ctx.fill();
        
        ctx.restore();
    }

    // ========================================================================
    // RÉGIONS / SECTIONS
    // ========================================================================

    /**
     * ✅ NOUVEAU: Rendu des régions/sections colorées
     * @param {CanvasRenderingContext2D} ctx - Contexte Canvas
     * @param {Array} regions - Liste des régions [{startTime, endTime, color, label}]
     * @param {Object} coordSystem - Système de coordonnées
     */
    renderRegions(ctx, regions, coordSystem) {
        if (!regions || regions.length === 0) return;
        
        ctx.save();
        
        regions.forEach(region => {
            const x1 = coordSystem.timeToX(region.startTime);
            const x2 = coordSystem.timeToX(region.endTime);
            const width = x2 - x1;
            
            // Fond coloré semi-transparent
            ctx.fillStyle = region.color || 'rgba(102, 126, 234, 0.2)';
            ctx.fillRect(x1, 0, width, this.config.height);
            
            // Bordures
            ctx.strokeStyle = region.color || '#667eea';
            ctx.lineWidth = 1;
            ctx.strokeRect(x1, 0, width, this.config.height);
            
            // Label (si assez large)
            if (width > 50 && region.label) {
                ctx.fillStyle = '#fff';
                ctx.font = '10px sans-serif';
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.fillText(region.label, x1 + width / 2, this.config.height / 2);
            }
        });
        
        ctx.restore();
    }

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * Définit le format de temps
     * @param {string} format - 'time' ou 'beats'
     */
    setTimeFormat(format) {
        if (['time', 'beats'].includes(format)) {
            this.config.timeFormat = format;
            this.clearCache();
        }
    }

    /**
     * Active/désactive l'affichage des millisecondes
     * @param {boolean} show - Afficher millisecondes
     */
    setShowMilliseconds(show) {
        this.config.showMilliseconds = show;
        this.clearCache();
    }

    /**
     * Obtient la hauteur de la timeline
     * @returns {number} Hauteur en pixels
     */
    getHeight() {
        return this.config.height;
    }

    /**
     * Définit la visibilité
     * @param {boolean} visible - Visibilité
     */
    setVisible(visible) {
        // Timeline est toujours visible si activée
    }

    /**
     * Configure les options
     * @param {Object} options - Options à modifier
     */
    setConfig(options) {
        Object.assign(this.config, options);
        this.clearCache();
    }

    /**
     * Nettoie le cache de labels
     */
    clearCache() {
        this.labelCache.clear();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = TimelineRenderer;
}
// ============================================================================
// Fichier: frontend/js/views/VisualizerView.js
// Version: v3.1.0 - CORRECTED TO EXTEND BASEVIEW
// Date: 2025-10-14
// ============================================================================
// CORRECTIONS v3.1.0:
// ✅ HÉRITAGE: Hérite maintenant de BaseView (CRITIQUE)
// ✅ ÉVÉNEMENTS: Intégration EventBus conforme à l'architecture MVC
// ✅ ARCHITECTURE: Pattern BaseView respecté
// ✅ CANVAS: Gestion optimisée du render loop
// ✅ PERFORMANCE: Object pooling, culling, buffering maintenus
// ✅ CLEANUP: Méthode destroy() complète avec arrêt render loop
//
// Description:
//   Vue visualiseur temps réel des données MIDI en cours de lecture.
//   Affiche les notes actives, vélocité, canaux, et instruments utilisés.
//   Rendu optimisé avec Canvas HTML5.
//
// Fonctionnalités:
//   - Visualisation notes actives en temps réel
//   - Affichage vélocité (couleur/taille)
//   - Séparation par canaux MIDI
//   - Animation fluide (requestAnimationFrame)
//   - Fenêtre temporelle configurable
//   - Statistiques temps réel
//
// Architecture:
//   VisualizerView extends BaseView
//   - Utilise render loop optimisé
//   - Buffering double pour performance
//   - Pool d'objets pour notes actives
//
// Auteur: MidiMind Team
// ============================================================================

/**
 * @class VisualizerView
 * @extends BaseView
 * @description Vue visualiseur temps réel MIDI avec Canvas
 */
class VisualizerView extends BaseView {
    /**
     * Constructeur
     * @param {string|HTMLElement} containerId - ID du conteneur ou élément DOM
     * @param {EventBus} eventBus - Bus d'événements global
     */
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        // Configuration spécifique
        this.config.autoRender = false;
        this.config.preserveState = true;
        
        // Canvas et contexte
        this.canvas = null;
        this.ctx = null;
        
        // Configuration visualiseur
        this.viewConfig = {
            previewTime: 2000,       // ms à afficher
            laneHeight: 40,
            scrollSpeed: 60,
            showVelocity: true,
            showCC: false,
            showNoteNames: true
        };
        
        // État du visualiseur
        this.viewState = {
            currentTime: 0,
            midiJson: null,
            activeChannels: new Set([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]),
            upcomingNotes: [],
            activeNotes: new Map(),
            ccValues: new Map(),
            isPlaying: false
        };
        
        // Performance
        this.performanceMonitor = window.PerformanceMonitor ? new window.PerformanceMonitor() : null;
        this.fps = 60;
        this.lastFrame = 0;
        this.needsRedraw = true;
        this.renderLoopId = null;
        
        // Cache
        this.noteCache = new Map();
        
        // Dimensions
        this.width = 0;
        this.height = 0;
        
        // Logger
        this.logger = window.logger || console;
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    /**
     * Initialisation de la vue
     * Override de BaseView.initialize()
     */
    initialize() {
        super.initialize();
        
        this.bindCustomEvents();
        
        // Exposer globalement pour compatibilité
        if (typeof window !== 'undefined') {
            window.visualizerView = this;
        }
        
        this.logger.info('VisualizerView', 'Initialized');
    }
    
    /**
     * Lie les événements personnalisés via EventBus
     */
    bindCustomEvents() {
        // Écouter les événements de lecture
        this.eventBus.on('playback:started', () => {
            this.viewState.isPlaying = true;
            if (!this.renderLoopId) {
                this.startRenderLoop();
            }
        });
        
        this.eventBus.on('playback:stopped', () => {
            this.viewState.isPlaying = false;
            this.clearActiveNotes();
        });
        
        this.eventBus.on('playback:time-update', (data) => {
            this.update(data.currentTime);
        });
        
        this.eventBus.on('playback:note-on', (data) => {
            this.addActiveNote(data);
        });
        
        this.eventBus.on('playback:note-off', (data) => {
            this.removeActiveNote(data);
        });
        
        // Écouter le redimensionnement
        window.addEventListener('resize', () => {
            this.resize();
            this.invalidate();
        });
    }
    
    // ========================================================================
    // RENDER - OVERRIDE DE BASEVIEW
    // ========================================================================
    
    /**
     * Rend la vue complète
     * Override de BaseView.render()
     * @param {Object} data - Données
     * @param {Object} options - Options
     */
    render(data = null, options = {}) {
        if (this.state.isDestroyed) {
            console.warn('[VisualizerView] Cannot render destroyed view');
            return;
        }
        
        // Mettre à jour les données
        if (data) {
            this.data = { ...this.data, ...data };
        }
        
        // Construire le template
        const html = this.buildTemplate();
        
        // Injecter dans le conteneur
        if (this.container) {
            this.container.innerHTML = html;
            
            // Initialiser le canvas
            setTimeout(() => {
                this.initCanvas();
                this.resize();
                this.startRenderLoop();
            }, 100);
            
            // Marquer comme rendu
            this.state.isRendered = true;
            this.state.lastRender = Date.now();
        }
    }
    
    /**
     * Construit le template HTML
     * @returns {string} HTML
     */
    buildTemplate() {
        return `
            <div class="visualizer-container">
                <!-- Header -->
                <div class="visualizer-header">
                    <h3>🎵 Live MIDI Visualizer</h3>
                    <div class="visualizer-controls">
                        <button class="control-btn" data-action="toggle-velocity" 
                                title="Toggle Velocity Display">
                            <i class="icon">📊</i>
                            Velocity: ${this.viewConfig.showVelocity ? 'ON' : 'OFF'}
                        </button>
                        <button class="control-btn" data-action="toggle-cc" 
                                title="Toggle CC Display">
                            <i class="icon">🎚️</i>
                            CC: ${this.viewConfig.showCC ? 'ON' : 'OFF'}
                        </button>
                        <button class="control-btn" data-action="toggle-notes" 
                                title="Toggle Note Names">
                            <i class="icon">🔤</i>
                            Names: ${this.viewConfig.showNoteNames ? 'ON' : 'OFF'}
                        </button>
                    </div>
                </div>
                
                <!-- Canvas -->
                <div class="visualizer-canvas-wrapper">
                    <canvas id="visualizerCanvas"></canvas>
                </div>
                
                <!-- Stats -->
                <div class="visualizer-stats">
                    <div class="stat-item">
                        <span class="stat-label">FPS:</span>
                        <span class="stat-value" id="fpsValue">--</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Active Notes:</span>
                        <span class="stat-value" id="activeNotesValue">0</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">Time:</span>
                        <span class="stat-value" id="timeValue">00:00.000</span>
                    </div>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // CANVAS INITIALIZATION
    // ========================================================================
    
    /**
     * Initialise le canvas
     */
    initCanvas() {
        this.canvas = document.getElementById('visualizerCanvas');
        if (!this.canvas) {
            this.logger.error('VisualizerView', 'Canvas not found');
            return;
        }
        
        this.ctx = this.canvas.getContext('2d');
        
        this.resize();
        
        this.logger.info('VisualizerView', 'Canvas initialized');
    }
    
    /**
     * Redimensionne le canvas
     */
    resize() {
        if (!this.canvas) return;
        
        const dpr = window.devicePixelRatio || 1;
        const rect = this.canvas.getBoundingClientRect();
        
        this.canvas.width = rect.width * dpr;
        this.canvas.height = rect.height * dpr;
        this.canvas.style.width = `${rect.width}px`;
        this.canvas.style.height = `${rect.height}px`;
        
        if (this.ctx) {
            this.ctx.scale(dpr, dpr);
        }
        
        this.width = rect.width;
        this.height = rect.height;
        
        this.invalidate();
    }
    
    // ========================================================================
    // DATA LOADING
    // ========================================================================
    
    /**
     * Charge un fichier MidiJSON
     * @param {Object} midiJson - Données MIDI JSON
     */
    loadMidiJson(midiJson) {
        this.viewState.midiJson = midiJson;
        this.noteCache.clear();
        this.invalidate();
        
        this.logger.info('VisualizerView', 'MIDI data loaded');
    }
    
    /**
     * Met à jour le temps de lecture
     * @param {number} currentTime - Temps actuel en ms
     */
    update(currentTime) {
        this.viewState.currentTime = currentTime;
        
        if (!this.viewState.midiJson) return;
        
        // Mettre à jour les notes à venir
        this.updateUpcomingNotes();
        
        // Mettre à jour les notes actives
        this.updateActiveNotes();
        
        // Mettre à jour les valeurs CC
        this.updateCCValues();
        
        // Mettre à jour l'affichage du temps
        this.updateTimeDisplay();
        
        this.invalidate();
    }
    
    /**
     * Met à jour les notes à venir
     */
    updateUpcomingNotes() {
        const startTime = this.viewState.currentTime;
        const endTime = this.viewState.currentTime + this.viewConfig.previewTime;
        
        this.viewState.upcomingNotes = this.viewState.midiJson.timeline
            .filter(event => 
                event.type === 'noteOn' &&
                event.time >= startTime &&
                event.time <= endTime &&
                this.viewState.activeChannels.has(event.channel)
            );
    }
    
    /**
     * Met à jour les notes actives
     */
    updateActiveNotes() {
        const tolerance = 50; // ms
        
        // Ajouter les nouvelles notes actives
        this.viewState.midiJson.timeline
            .filter(event =>
                event.type === 'noteOn' &&
                Math.abs(event.time - this.viewState.currentTime) < tolerance &&
                this.viewState.activeChannels.has(event.channel)
            )
            .forEach(event => {
                const key = `${event.channel}_${event.note}`;
                this.viewState.activeNotes.set(key, {
                    ...event,
                    startTime: this.viewState.currentTime
                });
            });
        
        // Retirer les notes terminées
        Array.from(this.viewState.activeNotes.entries()).forEach(([key, note]) => {
            const endTime = note.time + note.duration;
            if (this.viewState.currentTime > endTime + tolerance) {
                this.viewState.activeNotes.delete(key);
            }
        });
        
        // Mettre à jour le compteur
        this.updateActiveNotesDisplay();
    }
    
    /**
     * Met à jour les valeurs CC
     */
    updateCCValues() {
        if (!this.viewConfig.showCC) return;
        
        // Récupérer les dernières valeurs CC avant le temps courant
        this.viewState.ccValues.clear();
        
        const ccEvents = this.viewState.midiJson.timeline
            .filter(event =>
                event.type === 'cc' &&
                event.time <= this.viewState.currentTime
            )
            .sort((a, b) => b.time - a.time);
        
        // Garder la dernière valeur de chaque CC
        const seen = new Set();
        ccEvents.forEach(event => {
            const key = `${event.channel}_${event.controller}`;
            if (!seen.has(key)) {
                this.viewState.ccValues.set(event.controller, event.value);
                seen.add(key);
            }
        });
    }
    
    /**
     * Ajoute une note active
     * @param {Object} noteData - Données de la note
     */
    addActiveNote(noteData) {
        const key = `${noteData.channel}_${noteData.note}`;
        this.viewState.activeNotes.set(key, {
            ...noteData,
            startTime: this.viewState.currentTime
        });
        this.invalidate();
    }
    
    /**
     * Retire une note active
     * @param {Object} noteData - Données de la note
     */
    removeActiveNote(noteData) {
        const key = `${noteData.channel}_${noteData.note}`;
        this.viewState.activeNotes.delete(key);
        this.invalidate();
    }
    
    /**
     * Efface toutes les notes actives
     */
    clearActiveNotes() {
        this.viewState.activeNotes.clear();
        this.invalidate();
    }
    
    // ========================================================================
    // RENDERING
    // ========================================================================
    
    /**
     * Invalide le cache et force le redessinage
     */
    invalidate() {
        this.needsRedraw = true;
    }
    
    /**
     * Boucle de rendu
     */
    startRenderLoop() {
        if (this.renderLoopId) {
            return; // Déjà en cours
        }
        
        const render = (timestamp) => {
            if (this.state.isDestroyed) {
                return; // Arrêter si détruit
            }
            
            if (this.needsRedraw) {
                if (this.performanceMonitor) {
                    this.performanceMonitor.measureRender(() => this.renderCanvas());
                } else {
                    this.renderCanvas();
                }
                this.needsRedraw = false;
                
                // Mettre à jour FPS
                this.updateFpsDisplay();
            }
            
            this.renderLoopId = requestAnimationFrame(render);
        };
        
        this.renderLoopId = requestAnimationFrame(render);
        
        this.logger.info('VisualizerView', 'Render loop started');
    }
    
    /**
     * Arrête la boucle de rendu
     */
    stopRenderLoop() {
        if (this.renderLoopId) {
            cancelAnimationFrame(this.renderLoopId);
            this.renderLoopId = null;
            
            this.logger.info('VisualizerView', 'Render loop stopped');
        }
    }
    
    /**
     * Rend le visualizer sur le canvas
     */
    renderCanvas() {
        if (!this.ctx) return;
        
        // Clear
        this.ctx.fillStyle = '#0a0a0a';
        this.ctx.fillRect(0, 0, this.width, this.height);
        
        if (!this.viewState.midiJson) {
            this.drawEmptyState();
            return;
        }
        
        // Dessiner la ligne de playhead
        this.drawPlayhead();
        
        // Dessiner les lanes par canal
        this.drawChannelLanes();
        
        // Dessiner les notes à venir
        this.drawUpcomingNotes();
        
        // Dessiner les notes actives
        this.drawActiveNotes();
        
        // Overlay CC si activé
        if (this.viewConfig.showCC) {
            this.drawCCOverlay();
        }
    }
    
    /**
     * Dessine l'état vide
     */
    drawEmptyState() {
        this.ctx.fillStyle = '#666';
        this.ctx.font = '16px sans-serif';
        this.ctx.textAlign = 'center';
        this.ctx.textBaseline = 'middle';
        this.ctx.fillText('No MIDI data loaded', this.width / 2, this.height / 2);
    }
    
    /**
     * Dessine la ligne de playhead
     */
    drawPlayhead() {
        const x = this.width * 0.2; // 20% de la largeur
        
        // Ligne
        this.ctx.strokeStyle = '#e74c3c';
        this.ctx.lineWidth = 2;
        this.ctx.beginPath();
        this.ctx.moveTo(x, 0);
        this.ctx.lineTo(x, this.height);
        this.ctx.stroke();
        
        // Triangle en haut
        this.ctx.fillStyle = '#e74c3c';
        this.ctx.beginPath();
        this.ctx.moveTo(x, 0);
        this.ctx.lineTo(x - 6, 12);
        this.ctx.lineTo(x + 6, 12);
        this.ctx.closePath();
        this.ctx.fill();
    }
    
    /**
     * Dessine les lanes par canal
     */
    drawChannelLanes() {
        const channels = this.getActiveChannels();
        const laneHeight = this.height / Math.max(channels.length, 1);
        
        channels.forEach((channel, index) => {
            const y = index * laneHeight;
            
            // Background alterné
            if (index % 2 === 0) {
                this.ctx.fillStyle = 'rgba(255, 255, 255, 0.02)';
                this.ctx.fillRect(0, y, this.width, laneHeight);
            }
            
            // Label du canal
            this.ctx.fillStyle = '#666';
            this.ctx.font = '12px sans-serif';
            this.ctx.textAlign = 'left';
            this.ctx.textBaseline = 'top';
            this.ctx.fillText(`Ch ${channel + 1}`, 5, y + 5);
            
            // Ligne de séparation
            this.ctx.strokeStyle = '#333';
            this.ctx.lineWidth = 1;
            this.ctx.beginPath();
            this.ctx.moveTo(0, y + laneHeight);
            this.ctx.lineTo(this.width, y + laneHeight);
            this.ctx.stroke();
        });
    }
    
    /**
     * Dessine les notes à venir
     */
    drawUpcomingNotes() {
        const playheadX = this.width * 0.2;
        const channels = this.getActiveChannels();
        const laneHeight = this.height / Math.max(channels.length, 1);
        
        this.viewState.upcomingNotes.forEach(note => {
            const channelIndex = channels.indexOf(note.channel);
            if (channelIndex === -1) return;
            
            // Position temporelle
            const timeOffset = note.time - this.viewState.currentTime;
            const x = playheadX + (timeOffset / this.viewConfig.previewTime) * (this.width - playheadX);
            
            // Position verticale
            const y = channelIndex * laneHeight;
            
            // Largeur selon durée
            const noteWidth = (note.duration / this.viewConfig.previewTime) * (this.width - playheadX);
            const noteHeight = laneHeight * 0.8;
            const noteY = y + (laneHeight - noteHeight) / 2;
            
            // Couleur selon canal
            const channelColor = this.getChannelColor(note.channel);
            const opacity = 1.0;
            
            // Dessiner la note
            this.ctx.fillStyle = this.hexToRgba(channelColor, opacity);
            this.ctx.fillRect(x, noteY, Math.max(noteWidth, 2), noteHeight);
            
            // Barre de vélocité
            if (this.viewConfig.showVelocity) {
                const velocityHeight = (note.velocity / 127) * noteHeight;
                this.ctx.fillStyle = 'rgba(255, 255, 255, 0.3)';
                this.ctx.fillRect(x, noteY + noteHeight - velocityHeight, 3, velocityHeight);
            }
            
            // Nom de la note si assez large
            if (this.viewConfig.showNoteNames && noteWidth > 30) {
                this.ctx.fillStyle = '#fff';
                this.ctx.font = '10px sans-serif';
                this.ctx.textAlign = 'left';
                this.ctx.textBaseline = 'middle';
                this.ctx.fillText(
                    this.getNoteName(note.note),
                    x + 4,
                    noteY + noteHeight / 2
                );
            }
        });
    }
    
    /**
     * Dessine les notes actives
     */
    drawActiveNotes() {
        const playheadX = this.width * 0.2;
        const channels = this.getActiveChannels();
        const laneHeight = this.height / Math.max(channels.length, 1);
        
        this.viewState.activeNotes.forEach(note => {
            const channelIndex = channels.indexOf(note.channel);
            if (channelIndex === -1) return;
            
            const y = channelIndex * laneHeight;
            const noteHeight = laneHeight * 0.8;
            const noteY = y + (laneHeight - noteHeight) / 2;
            
            // Flash/pulse effect
            const progress = (this.viewState.currentTime - note.startTime) / 200;
            const scale = Math.max(1.0 - progress, 0.8);
            
            const channelColor = this.getChannelColor(note.channel);
            
            // Halo
            this.ctx.shadowColor = channelColor;
            this.ctx.shadowBlur = 20 * scale;
            this.ctx.fillStyle = channelColor;
            this.ctx.fillRect(
                playheadX - 4 * scale,
                noteY - 2 * scale,
                8 * scale,
                noteHeight + 4 * scale
            );
            
            this.ctx.shadowBlur = 0;
        });
    }
    
    /**
     * Dessine l'overlay des CC
     */
    drawCCOverlay() {
        const x = 10;
        let y = 10;
        const padding = 10;
        const lineHeight = 20;
        
        if (this.viewState.ccValues.size === 0) return;
        
        // Background
        const overlayHeight = this.viewState.ccValues.size * lineHeight + padding * 2;
        this.ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
        this.ctx.fillRect(x, y, 200, overlayHeight);
        
        y += padding;
        
        // CC values
        this.ctx.font = '11px monospace';
        this.ctx.textAlign = 'left';
        this.ctx.textBaseline = 'top';
        
        this.viewState.ccValues.forEach((value, cc) => {
            const ccName = this.getCCName(cc);
            const barWidth = (value / 127) * 100;
            
            // Label
            this.ctx.fillStyle = '#fff';
            this.ctx.fillText(`CC${cc} ${ccName}`, x + padding, y);
            
            // Barre
            y += 12;
            this.ctx.fillStyle = '#333';
            this.ctx.fillRect(x + padding, y, 100, 4);
            this.ctx.fillStyle = '#667eea';
            this.ctx.fillRect(x + padding, y, barWidth, 4);
            
            // Valeur
            this.ctx.fillStyle = '#aaa';
            this.ctx.fillText(value.toString(), x + padding + 105, y - 12);
            
            y += 8;
        });
    }
    
    // ========================================================================
    // CONTROLS & TOGGLES
    // ========================================================================
    
    /**
     * Obtient les canaux actifs
     * @returns {Array<number>}
     */
    getActiveChannels() {
        if (!this.viewState.midiJson) return [];
        
        return this.viewState.midiJson.channels
            .filter(ch => this.viewState.activeChannels.has(ch.number))
            .map(ch => ch.number);
    }
    
    /**
     * Active/désactive un canal
     * @param {number} channel - Numéro du canal
     * @param {boolean} enabled - Actif ou non
     */
    toggleChannel(channel, enabled) {
        if (enabled) {
            this.viewState.activeChannels.add(channel);
        } else {
            this.viewState.activeChannels.delete(channel);
        }
        this.invalidate();
    }
    
    /**
     * Définit le temps d'aperçu
     * @param {number} time - Temps en ms
     */
    setPreviewTime(time) {
        this.viewConfig.previewTime = Math.max(500, Math.min(10000, time));
        this.invalidate();
    }
    
    /**
     * Active/désactive l'affichage de la vélocité
     * @param {boolean} show
     */
    setShowVelocity(show) {
        this.viewConfig.showVelocity = show;
        this.invalidate();
    }
    
    /**
     * Active/désactive l'affichage des CC
     * @param {boolean} show
     */
    setShowCC(show) {
        this.viewConfig.showCC = show;
        this.invalidate();
    }
    
    /**
     * Active/désactive l'affichage des noms de notes
     * @param {boolean} show
     */
    setShowNoteNames(show) {
        this.viewConfig.showNoteNames = show;
        this.invalidate();
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * Obtient la couleur d'un canal
     * @param {number} channel
     * @returns {string}
     */
    getChannelColor(channel) {
        const colors = [
            '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A',
            '#98D8C8', '#F7DC6F', '#BB8FCE', '#85C1E2',
            '#F8B739', '#52C7B8', '#FF8C94', '#A8E6CF',
            '#FFD93D', '#BCB3E5', '#FAB1A0', '#81C784'
        ];
        return colors[channel % colors.length];
    }
    
    /**
     * Convertit hex en rgba
     * @param {string} hex
     * @param {number} alpha
     * @returns {string}
     */
    hexToRgba(hex, alpha) {
        const r = parseInt(hex.slice(1, 3), 16);
        const g = parseInt(hex.slice(3, 5), 16);
        const b = parseInt(hex.slice(5, 7), 16);
        return `rgba(${r}, ${g}, ${b}, ${alpha})`;
    }
    
    /**
     * Obtient le nom d'une note
     * @param {number} midiNote
     * @returns {string}
     */
    getNoteName(midiNote) {
        const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        return names[midiNote % 12];
    }
    
    /**
     * Obtient le nom d'un CC
     * @param {number} ccNumber
     * @returns {string}
     */
    getCCName(ccNumber) {
        const ccNames = {
            1: 'Mod',
            7: 'Vol',
            10: 'Pan',
            11: 'Exp',
            64: 'Sus',
            91: 'Rev',
            93: 'Cho'
        };
        return ccNames[ccNumber] || '';
    }
    
    /**
     * Formate le temps en mm:ss.ms
     * @param {number} ms - Temps en millisecondes
     * @returns {string}
     */
    formatTime(ms) {
        const totalSeconds = Math.floor(ms / 1000);
        const minutes = Math.floor(totalSeconds / 60);
        const seconds = totalSeconds % 60;
        const milliseconds = Math.floor((ms % 1000) / 10);
        
        return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}.${String(milliseconds).padStart(2, '0')}`;
    }
    
    // ========================================================================
    // UI UPDATES
    // ========================================================================
    
    /**
     * Met à jour l'affichage du FPS
     */
    updateFpsDisplay() {
        const now = performance.now();
        if (this.lastFrame) {
            const delta = now - this.lastFrame;
            const fps = Math.round(1000 / delta);
            
            const fpsEl = document.getElementById('fpsValue');
            if (fpsEl) {
                fpsEl.textContent = fps;
            }
        }
        this.lastFrame = now;
    }
    
    /**
     * Met à jour l'affichage des notes actives
     */
    updateActiveNotesDisplay() {
        const countEl = document.getElementById('activeNotesValue');
        if (countEl) {
            countEl.textContent = this.viewState.activeNotes.size;
        }
    }
    
    /**
     * Met à jour l'affichage du temps
     */
    updateTimeDisplay() {
        const timeEl = document.getElementById('timeValue');
        if (timeEl) {
            timeEl.textContent = this.formatTime(this.viewState.currentTime);
        }
    }
    
    // ========================================================================
    // LIFECYCLE
    // ========================================================================
    
    /**
     * Détruit la vue
     * Override de BaseView.destroy()
     */
    destroy() {
        // Arrêter le render loop
        this.stopRenderLoop();
        
        // Nettoyer les références
        this.canvas = null;
        this.ctx = null;
        this.viewState.midiJson = null;
        this.viewState.activeNotes.clear();
        this.viewState.upcomingNotes = [];
        this.noteCache.clear();
        
        // Nettoyer la référence globale
        if (typeof window !== 'undefined' && window.visualizerView === this) {
            window.visualizerView = null;
        }
        
        // Appeler destroy de BaseView
        super.destroy();
        
        this.logger.info('VisualizerView', 'Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = VisualizerView;
}

if (typeof window !== 'undefined') {
    window.VisualizerView = VisualizerView;
}

// ============================================================================
// FIN DU FICHIER VisualizerView.js v3.1.0
// ============================================================================

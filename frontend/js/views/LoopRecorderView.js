// ============================================================================
// Fichier: frontend/js/views/LoopRecorderView.js
// Version: 3.0.0
// Date: 2025-10-10
// ============================================================================
// Description:
//   Vue du Loop Recorder avec timeline interactive avancée.
//   Gestion du zoom, grille rythmique, et interaction souris.
//
// Changelog v3.0.0:
//   - Timeline interactive avec zoom (molette)
//   - Grille rythmique détaillée (1/4, 1/8, 1/16, 1/32)
//   - Interaction souris (click = seek, hover = preview)
//   - Visualisation notes améliorée (hauteur = vélocité)
//   - Drag pour sélection de région (préparation future)
//   - Double-click pour zoom sur mesure
//   - Meilleure gestion des couleurs par canal
// ============================================================================

/**
 * @class LoopRecorderView
 * @description Vue avancée du Loop Recorder
 */
class LoopRecorderView {
    constructor(container) {
        this.container = container;
        this.model = null;
        this.canvas = null;
        this.ctx = null;
        this.canvasWidth = 0;
        this.canvasHeight = 0;
        
        // Zoom et pan
        this.zoomLevel = 1.0;       // 1.0 = normal, 2.0 = 2x zoom
        this.minZoom = 0.5;
        this.maxZoom = 8.0;
        this.panOffset = 0;         // Offset horizontal en pixels
        
        // Grille rythmique
        this.gridResolution = 16;   // 1/16 notes par défaut
        this.showGrid = true;
        
        // Interaction souris
        this.isDragging = false;
        this.dragStartX = 0;
        this.mouseX = 0;
        this.mouseY = 0;
        this.hoverTime = null;
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    /**
     * Initialise la vue
     */
    init(model) {
        this.model = model;
        this.render();
        this.attachEvents();
    }

    /**
     * Rend l'interface
     */
    render() {
        this.container.innerHTML = `
            <div class="loop-recorder">
                <!-- Count-in Display -->
                <div id="countInDisplay" class="count-in-display hidden">
                    <span id="countInNumber">4</span>
                </div>

                <!-- Contrôles principaux -->
                <div class="loop-controls-main">
                    <button class="btn-record" id="btnRecord" title="Record (R)">
                        <span class="icon">⏺️</span>
                        <span class="label">REC</span>
                    </button>
                    
                    <button class="btn-play" id="btnPlayLoop" title="Play (Space)">
                        <span class="icon">▶️</span>
                        <span class="label">PLAY</span>
                    </button>
                    
                    <button class="btn-stop" id="btnStopLoop" title="Stop (S)">
                        <span class="icon">⏹️</span>
                        <span class="label">STOP</span>
                    </button>
                    
                    <button class="btn-clear" id="btnClearLoop" title="Clear All (C)">
                        <span class="icon">🗑️</span>
                        <span class="label">CLEAR</span>
                    </button>
                </div>

                <!-- Paramètres de boucle -->
                <div class="loop-settings">
                    <div class="setting-group">
                        <label>Bars:</label>
                        <input type="number" id="loopBars" min="1" max="16" value="4" step="1">
                    </div>
                    
                    <div class="setting-group">
                        <label>Tempo:</label>
                        <input type="number" id="loopTempo" min="20" max="300" value="120" step="1">
                        <span class="unit">BPM</span>
                    </div>
                    
                    <div class="setting-group">
                        <label>Time Signature:</label>
                        <select id="loopTimeSignature">
                            <option value="2/4">2/4</option>
                            <option value="3/4">3/4</option>
                            <option value="4/4" selected>4/4</option>
                            <option value="5/4">5/4</option>
                            <option value="6/8">6/8</option>
                            <option value="7/8">7/8</option>
                        </select>
                    </div>
                    
                    <div class="setting-group">
                        <label>Record Mode:</label>
                        <select id="recordMode">
                            <option value="overdub">Overdub</option>
                            <option value="replace">Replace</option>
                            <option value="merge">Merge</option>
                        </select>
                    </div>
                    
                    <button class="btn-secondary" onclick="loopController.createNewLoop()">
                        New Loop
                    </button>
                </div>

                <!-- Métronome -->
                <div class="metronome-controls">
                    <div class="setting-group">
                        <label>
                            <input type="checkbox" id="metronomeEnabled">
                            Metronome (M)
                        </label>
                    </div>
                    
                    <div class="setting-group">
                        <label>Volume:</label>
                        <input type="range" id="metronomeVolume" min="0" max="100" value="50" 
                               oninput="loopController.setMetronomeVolume(this.value)">
                        <span id="metronomeVolumeValue">50%</span>
                    </div>
                    
                    <div class="setting-group">
                        <label>
                            <input type="checkbox" id="countInEnabled" checked>
                            Count-in
                        </label>
                        <input type="number" id="countInBars" min="0" max="4" value="1" step="1">
                        <span class="unit">bars</span>
                    </div>
                    
                    <div class="setting-group">
                        <label>
                            <input type="checkbox" id="quantizeEnabled">
                            Quantize
                        </label>
                        <select id="quantizeResolution">
                            <option value="960">1/4</option>
                            <option value="480" selected>1/8</option>
                            <option value="240">1/16</option>
                            <option value="120">1/32</option>
                        </select>
                    </div>
                </div>

                <!-- Layers -->
                <div class="loop-layers">
                    <div class="layers-header">
                        <h3>Layers</h3>
                    </div>
                    <div class="layers-list" id="layersList">
                        <div class="no-layers">No layers yet. Start recording!</div>
                    </div>
                </div>

                <!-- Visualiseur Timeline -->
                <div class="loop-visualizer-timeline">
                    <div class="timeline-header">
                        <span class="position-label" id="loopPosition">0.0 / 4.0</span>
                        <div class="timeline-controls">
                            <span class="zoom-label">Zoom: <span id="zoomValue">100%</span></span>
                            <button class="btn-zoom" onclick="loopRecorderView.resetZoom()" title="Reset Zoom">
                                🔍 Reset
                            </button>
                            <label>
                                <input type="checkbox" id="showGrid" checked 
                                       onchange="loopRecorderView.toggleGrid(this.checked)">
                                Grid
                            </label>
                            <select id="gridResolution" onchange="loopRecorderView.setGridResolution(this.value)">
                                <option value="4">1/4</option>
                                <option value="8">1/8</option>
                                <option value="16" selected>1/16</option>
                                <option value="32">1/32</option>
                            </select>
                        </div>
                    </div>
                    <canvas id="loopTimelineCanvas"></canvas>
                    <div class="timeline-info" id="timelineInfo"></div>
                </div>

                <!-- Actions -->
                <div class="loop-actions">
                    <button class="btn-secondary" onclick="loopController.exportMidi()">
                        📥 Export MIDI
                    </button>
                    <button class="btn-secondary" onclick="loopController.saveLoop()">
                        💾 Save Loop
                    </button>
                    <button class="btn-secondary" onclick="loopController.loadLoop()">
                        📂 Load Loop
                    </button>
                </div>
            </div>
        `;

        // Initialiser le canvas de timeline
        this.initTimelineCanvas();
    }

    /**
     * Initialise le canvas de timeline
     */
    initTimelineCanvas() {
        const canvas = document.getElementById('loopTimelineCanvas');
        if (!canvas) return;

        const dpr = window.devicePixelRatio || 1;
        const rect = canvas.parentElement.getBoundingClientRect();

        canvas.width = rect.width * dpr;
        canvas.height = 150 * dpr;
        canvas.style.width = `${rect.width}px`;
        canvas.style.height = '150px';

        const ctx = canvas.getContext('2d');
        ctx.scale(dpr, dpr);

        this.canvas = canvas;
        this.ctx = ctx;
        this.canvasWidth = rect.width;
        this.canvasHeight = 150;
    }

    /**
     * Attache les événements
     */
    attachEvents() {
        // Boutons principaux
        document.getElementById('btnRecord')?.addEventListener('click', () => {
            loopController.toggleRecord();
        });

        document.getElementById('btnPlayLoop')?.addEventListener('click', () => {
            loopController.togglePlay();
        });

        document.getElementById('btnStopLoop')?.addEventListener('click', () => {
            loopController.stop();
        });

        document.getElementById('btnClearLoop')?.addEventListener('click', () => {
            if (confirm('Clear all layers?')) {
                loopController.clearLoop();
            }
        });

        // Paramètres de loop
        document.getElementById('loopBars')?.addEventListener('change', (e) => {
            loopController.updateLoopSettings({ bars: parseInt(e.target.value) });
        });

        document.getElementById('loopTempo')?.addEventListener('change', (e) => {
            loopController.updateLoopSettings({ tempo: parseInt(e.target.value) });
        });

        document.getElementById('loopTimeSignature')?.addEventListener('change', (e) => {
            loopController.updateLoopSettings({ timeSignature: e.target.value });
        });

        document.getElementById('recordMode')?.addEventListener('change', (e) => {
            loopController.setRecordMode(e.target.value);
        });

        // Métronome
        document.getElementById('metronomeEnabled')?.addEventListener('change', (e) => {
            loopController.setMetronome(e.target.checked);
        });

        document.getElementById('metronomeVolume')?.addEventListener('input', (e) => {
            document.getElementById('metronomeVolumeValue').textContent = `${e.target.value}%`;
        });

        document.getElementById('countInBars')?.addEventListener('change', (e) => {
            loopController.countInBars = parseInt(e.target.value);
        });

        // Quantize
        document.getElementById('quantizeEnabled')?.addEventListener('change', (e) => {
            const resolution = parseInt(document.getElementById('quantizeResolution')?.value || 480);
            loopController.setQuantize(e.target.checked, resolution);
        });

        document.getElementById('quantizeResolution')?.addEventListener('change', (e) => {
            const enabled = document.getElementById('quantizeEnabled')?.checked || false;
            loopController.setQuantize(enabled, parseInt(e.target.value));
        });

        // Timeline interactions
        this.attachTimelineEvents();

        // Resize
        window.addEventListener('resize', () => {
            this.initTimelineCanvas();
        });
    }

    /**
     * Attache les événements de la timeline
     */
    attachTimelineEvents() {
        if (!this.canvas) return;

        // Zoom avec la molette
        this.canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            
            const delta = e.deltaY > 0 ? 0.9 : 1.1;
            const newZoom = this.zoomLevel * delta;
            
            if (newZoom >= this.minZoom && newZoom <= this.maxZoom) {
                this.zoomLevel = newZoom;
                this.updateZoomLabel();
            }
        });

        // Click pour seek
        this.canvas.addEventListener('click', (e) => {
            if (this.isDragging) return;
            
            const rect = this.canvas.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const time = this.pixelToTime(x);
            
            if (this.model.currentLoop && this.model.isPlaying) {
                this.model.loopPosition = time;
                this.model.loopStartTime = Date.now() - time;
            }
        });

        // Double-click pour zoom sur mesure
        this.canvas.addEventListener('dblclick', (e) => {
            const rect = this.canvas.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const bar = Math.floor((this.pixelToTime(x) / this.model.currentLoop?.duration || 1) * 
                                   (this.model.currentLoop?.bars || 4));
            
            // Zoom et centrer sur la mesure
            this.zoomLevel = 2.0;
            this.updateZoomLabel();
        });

        // Hover pour preview position
        this.canvas.addEventListener('mousemove', (e) => {
            const rect = this.canvas.getBoundingClientRect();
            this.mouseX = e.clientX - rect.left;
            this.mouseY = e.clientY - rect.top;
            this.hoverTime = this.pixelToTime(this.mouseX);
            
            this.updateTimelineInfo();
        });

        this.canvas.addEventListener('mouseleave', () => {
            this.hoverTime = null;
            this.updateTimelineInfo();
        });
    }

    // ========================================================================
    // TIMELINE AVANCÉE
    // ========================================================================

    /**
     * Convertit pixel en temps
     */
    pixelToTime(x) {
        if (!this.model.currentLoop) return 0;
        
        const visibleWidth = this.canvasWidth / this.zoomLevel;
        const scrolledX = x + this.panOffset;
        const normalizedX = scrolledX / this.canvasWidth;
        
        return normalizedX * this.model.currentLoop.duration;
    }

    /**
     * Convertit temps en pixel
     */
    timeToPixel(time) {
        if (!this.model.currentLoop) return 0;
        
        const normalizedTime = time / this.model.currentLoop.duration;
        const x = normalizedTime * this.canvasWidth * this.zoomLevel;
        
        return x - this.panOffset;
    }

    /**
     * Dessine la timeline améliorée
     */
    drawTimeline(loop, position) {
        if (!this.ctx || !loop) return;

        const ctx = this.ctx;
        const width = this.canvasWidth;
        const height = this.canvasHeight;

        // Clear
        ctx.fillStyle = '#0a0a0a';
        ctx.fillRect(0, 0, width, height);

        // Largeur effective avec zoom
        const effectiveWidth = width * this.zoomLevel;

        // Grille rythmique
        if (this.showGrid) {
            this.drawGrid(ctx, loop, effectiveWidth, height);
        }

        // Grille de mesures (toujours affichée)
        this.drawBars(ctx, loop, effectiveWidth, height);

        // Dessiner les événements des layers
        this.drawLayers(ctx, loop, effectiveWidth, height);

        // Hover line
        if (this.hoverTime !== null) {
            this.drawHoverLine(ctx, height);
        }

        // Playhead
        this.drawPlayhead(ctx, loop, position, height);
    }

    /**
     * Dessine la grille rythmique
     */
    drawGrid(ctx, loop, width, height) {
        const [numerator] = loop.timeSignature.split('/').map(Number);
        const beatDuration = 60000 / loop.tempo;
        const noteDuration = beatDuration / (this.gridResolution / 4); // Durée d'une subdivision
        
        const totalNotes = loop.duration / noteDuration;
        
        for (let i = 0; i <= totalNotes; i++) {
            const time = i * noteDuration;
            const x = this.timeToPixel(time);
            
            if (x < 0 || x > this.canvasWidth) continue;
            
            // Opacité selon la subdivision
            const isQuarter = i % this.gridResolution === 0;
            const isEighth = i % (this.gridResolution / 2) === 0;
            
            ctx.strokeStyle = isQuarter ? 'rgba(255, 255, 255, 0.1)' : 
                             isEighth ? 'rgba(255, 255, 255, 0.05)' : 
                             'rgba(255, 255, 255, 0.02)';
            ctx.lineWidth = isQuarter ? 1 : 0.5;
            
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
            ctx.stroke();
        }
    }

    /**
     * Dessine les lignes de mesures
     */
    drawBars(ctx, loop, width, height) {
        const barDuration = loop.duration / loop.bars;
        
        for (let i = 0; i <= loop.bars; i++) {
            const time = i * barDuration;
            const x = this.timeToPixel(time);
            
            if (x < -10 || x > this.canvasWidth + 10) continue;
            
            ctx.strokeStyle = i === 0 || i === loop.bars ? '#555' : '#333';
            ctx.lineWidth = i === 0 || i === loop.bars ? 2 : 1;
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
            ctx.stroke();

            // Numéro de mesure
            if (i < loop.bars && x > 0 && x < this.canvasWidth - 20) {
                ctx.fillStyle = '#666';
                ctx.font = '10px sans-serif';
                ctx.textAlign = 'left';
                ctx.fillText(`${i + 1}`, x + 4, 12);
            }
        }
    }

    /**
     * Dessine les layers avec notes améliorées
     */
    drawLayers(ctx, loop, width, height) {
        const layerHeight = 20;
        const layerSpacing = 4;
        const totalLayers = loop.layers.filter(l => !l.muted).length;
        
        if (totalLayers === 0) return;
        
        const startY = 25;
        let layerIndex = 0;

        loop.layers.forEach((layer) => {
            if (layer.muted) return;

            const y = startY + (layerIndex * (layerHeight + layerSpacing));
            const color = this.getChannelColor(layer.channel);

            // Background layer
            ctx.fillStyle = 'rgba(255, 255, 255, 0.03)';
            ctx.fillRect(0, y, this.canvasWidth, layerHeight);

            // Événements avec hauteur proportionnelle à vélocité
            layer.events.forEach(event => {
                if (event.type !== 'noteOn') return;

                const x = this.timeToPixel(event.time);
                
                // Ne dessiner que si visible
                if (x < -10 || x > this.canvasWidth + 10) return;
                
                const noteWidth = event.duration ? 
                    Math.max(1, this.timeToPixel(event.time + event.duration) - x) : 2;
                
                // Hauteur proportionnelle à la vélocité
                const velocityRatio = event.velocity / 127;
                const noteHeight = (layerHeight - 4) * velocityRatio;
                const noteY = y + (layerHeight - noteHeight) / 2;

                // Couleur avec alpha selon vélocité
                const alpha = 0.5 + (velocityRatio * 0.5);
                ctx.fillStyle = this.hexToRgba(color, alpha);
                ctx.fillRect(x, noteY, Math.max(noteWidth, 1), noteHeight);
            });

            layerIndex++;
        });
    }

    /**
     * Dessine la ligne de hover
     */
    drawHoverLine(ctx, height) {
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.3)';
        ctx.lineWidth = 1;
        ctx.setLineDash([2, 2]);
        ctx.beginPath();
        ctx.moveTo(this.mouseX, 0);
        ctx.lineTo(this.mouseX, height);
        ctx.stroke();
        ctx.setLineDash([]);
    }

    /**
     * Dessine le playhead
     */
    drawPlayhead(ctx, loop, position, height) {
        const playheadX = this.timeToPixel(position);
        
        // Ne dessiner que si visible
        if (playheadX < 0 || playheadX > this.canvasWidth) return;
        
        ctx.strokeStyle = '#e74c3c';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(playheadX, 0);
        ctx.lineTo(playheadX, height);
        ctx.stroke();

        // Triangle playhead
        ctx.fillStyle = '#e74c3c';
        ctx.beginPath();
        ctx.moveTo(playheadX, 0);
        ctx.lineTo(playheadX - 5, 10);
        ctx.lineTo(playheadX + 5, 10);
        ctx.closePath();
        ctx.fill();
    }

    /**
     * Met à jour l'info de la timeline
     */
    updateTimelineInfo() {
        const info = document.getElementById('timelineInfo');
        if (!info || !this.model.currentLoop) return;

        if (this.hoverTime !== null) {
            const bars = (this.hoverTime / this.model.currentLoop.duration) * 
                        this.model.currentLoop.bars;
            info.textContent = `Bar: ${bars.toFixed(2)} | Time: ${(this.hoverTime / 1000).toFixed(2)}s`;
            info.style.display = 'block';
        } else {
            info.style.display = 'none';
        }
    }

    /**
     * Reset zoom
     */
    resetZoom() {
        this.zoomLevel = 1.0;
        this.panOffset = 0;
        this.updateZoomLabel();
    }

    /**
     * Toggle grille
     */
    toggleGrid(show) {
        this.showGrid = show;
    }

    /**
     * Définit la résolution de la grille
     */
    setGridResolution(value) {
        this.gridResolution = parseInt(value);
    }

    /**
     * Met à jour le label de zoom
     */
    updateZoomLabel() {
        const label = document.getElementById('zoomValue');
        if (label) {
            label.textContent = `${Math.round(this.zoomLevel * 100)}%`;
        }
    }

    // ========================================================================
    // COUNT-IN
    // ========================================================================

    /**
     * Affiche le count-in
     */
    showCountIn(number) {
        const display = document.getElementById('countInDisplay');
        const numberSpan = document.getElementById('countInNumber');
        
        if (display && numberSpan) {
            numberSpan.textContent = number;
            display.classList.remove('hidden');
            display.classList.add('pulse');
            
            setTimeout(() => {
                display.classList.remove('pulse');
            }, 300);
        }
    }

    /**
     * Cache le count-in
     */
    hideCountIn() {
        const display = document.getElementById('countInDisplay');
        if (display) {
            display.classList.add('hidden');
        }
    }

    // ========================================================================
    // LAYERS
    // ========================================================================

    /**
     * Met à jour l'affichage des layers
     */
    updateLayers(layers) {
        const list = document.getElementById('layersList');
        if (!list) return;

        if (!layers || layers.length === 0) {
            list.innerHTML = '<div class="no-layers">No layers yet. Start recording!</div>';
            return;
        }

        list.innerHTML = layers.map(layer => `
            <div class="layer-item">
                <div class="layer-info">
                    <span class="layer-name">${this.getInstrumentName(layer.instrument)}</span>
                    <span class="layer-channel">Ch ${layer.channel}</span>
                    <span class="layer-events">${layer.events.length} events</span>
                </div>
                
                <button class="btn-layer ${layer.solo ? 'active' : ''}" 
                        onclick="loopController.toggleSolo('${layer.id}')"
                        title="Solo">
                    S
                </button>
                
                <button class="btn-layer ${layer.muted ? 'active' : ''}" 
                        onclick="loopController.toggleMute('${layer.id}')"
                        title="Mute">
                    M
                </button>
                
                <input type="range" 
                       class="layer-volume" 
                       min="0" max="127" 
                       value="${layer.volume}"
                       oninput="loopController.setLayerVolume('${layer.id}', this.value)">
                
                <span class="layer-volume-value">${layer.volume}</span>
                
                <button class="btn-layer-delete" 
                        onclick="loopController.clearLayer('${layer.id}')"
                        title="Delete layer">
                    ×
                </button>
            </div>
        `).join('');
    }

    // ========================================================================
    // BOUTONS
    // ========================================================================

    /**
     * Met à jour l'état des boutons
     */
    updateButtonStates(state) {
        const btnRecord = document.getElementById('btnRecord');
        const btnPlay = document.getElementById('btnPlayLoop');

        if (state.isRecording) {
            btnRecord?.classList.add('active', 'recording');
        } else {
            btnRecord?.classList.remove('active', 'recording');
        }

        if (state.isPlaying) {
            btnPlay?.classList.add('active');
            const icon = btnPlay.querySelector('.icon');
            const label = btnPlay.querySelector('.label');
            if (icon) icon.textContent = '⏸️';
            if (label) label.textContent = 'PAUSE';
        } else {
            btnPlay?.classList.remove('active');
            const icon = btnPlay.querySelector('.icon');
            const label = btnPlay.querySelector('.label');
            if (icon) icon.textContent = '▶️';
            if (label) label.textContent = 'PLAY';
        }
    }

    /**
     * Met à jour la position affichée
     */
    updatePosition(position, duration) {
        const bars = duration / (60000 / 120 * 4);
        const currentBar = (position / duration) * bars;
        
        const positionElement = document.getElementById('loopPosition');
        if (positionElement) {
            positionElement.textContent = 
                `${currentBar.toFixed(1)} / ${bars.toFixed(1)}`;
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    /**
     * Obtient le nom de l'instrument
     */
    getInstrumentName(instrumentId) {
        const instrument = window.instrumentModel?.get(instrumentId);
        return instrument?.name || `Instrument ${instrumentId || 'Unknown'}`;
    }

    /**
     * Obtient la couleur d'un canal
     */
    getChannelColor(channel) {
        const colors = [
            '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A',
            '#98D8C8', '#F7DC6F', '#BB8FCE', '#85C1E2',
            '#F8B195', '#F67280', '#C06C84', '#6C5B7B',
            '#355C7D', '#99B898', '#FECEAB', '#FF847C'
        ];
        return colors[channel % colors.length];
    }

    /**
     * Convertit hex en rgba
     */
    hexToRgba(hex, alpha) {
        const r = parseInt(hex.slice(1, 3), 16);
        const g = parseInt(hex.slice(3, 5), 16);
        const b = parseInt(hex.slice(5, 7), 16);
        return `rgba(${r}, ${g}, ${b}, ${alpha})`;
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = LoopRecorderView;
}

// Rendre accessible globalement
if (typeof window !== 'undefined') {
    window.LoopRecorderView = LoopRecorderView;
}
window.LoopRecorderView = LoopRecorderView;
// ============================================================================
// FIN DU FICHIER LoopRecorderView.js v3.0.0
// ============================================================================

// ============================================================================
// Fichier: frontend/js/views/HomeView.js
// Version: v3.2 - FINAL OPTIMIZED
// Date: 2025-10-22
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI
// ============================================================================
// CORRECTIONS v3.2:
// âœ… Constructeur corrigÃ©: accepte eventBus comme 2Ã¨me paramÃ¨tre (optionnel)
// âœ… EventBus initialisÃ© avec fallback sur window.EventBus
// âœ… Logging d'initialisation ajoutÃ©
// âœ… Compatible avec Application.js (qui passe 2 paramÃ¨tres)
// âœ… RÃ©trocompatible (1 seul paramÃ¨tre fonctionne toujours)
// âœ… Encodage UTF-8 propre, fins de ligne Unix LF
// ============================================================================
// Description:
//   Vue de la page d'accueil avec player et visualizer live.
//   Affiche les contrÃ´les de lecture, la sÃ©lection de fichiers,
//   le routing rapide et la visualisation MIDI en temps rÃ©el.
//
// FonctionnalitÃ©s:
//   - Player avec contrÃ´les Play/Pause/Stop
//   - Timeline interactive
//   - SÃ©lection de fichiers MIDI
//   - Gestion de playlist
//   - Routing rapide
//   - Visualiseur MIDI temps rÃ©el
//   - ContrÃ´le du tempo
//
// Architecture:
//   - Utilise eventBus pour communication inter-composants
//   - S'intÃ¨gre avec MidiVisualizer pour visualisation
//   - Compatible avec le systÃ¨me de routing
//
// Auteur: MidiMind Team
// ============================================================================

/**
 * @class HomeView
 * @description Vue principale de la page d'accueil avec player et visualizer
 */

class HomeView {
    constructor(container, eventBus = null) {
        // Resolve container properly
        if (typeof container === 'string') {
            this.container = document.getElementById(container) || document.querySelector(container);
        } else if (container instanceof HTMLElement) {
            this.container = container;
        } else {
            this.container = null;
        }
        
        // Log if container not found
        if (!this.container) {
            console.error('[HomeView] Container not found:', container);
        }
        
        // EventBus (use global if not provided)
        this.eventBus = eventBus || window.EventBus || null;
        
        this.visualizer = null;
        this.currentFile = null;
        this.logger = window.logger || console;
        
        // Log initialization
        if (this.logger && this.logger.debug) {
            this.logger.debug('HomeView', 'Constructor initialized with eventBus:', !!this.eventBus);
        }
    }

    /**
     * Initialise la vue
     */
    init() {
        if (!this.container) {
            if (this.logger && this.logger.error) {
                this.logger.error('HomeView', 'Cannot initialize: container not found');
            }
            return;
        }
        
        this.render();
        this.attachEvents();
    }

    /**
     * Construit le layout de la page d'accueil
     */
    render() {
        if (!this.container) {
            if (this.logger && this.logger.error) {
                this.logger.error('HomeView', 'Cannot render: container not found');
            }
            return;
        }
		
        this.container.innerHTML = `
            <div class="home-container">
                <!-- Barre de contrÃƒÆ’Ã‚Â´le supÃƒÆ’Ã‚Â©rieure -->
                <div class="top-bar">
                    <div class="file-info">
                        <span class="file-icon">ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Âµ</span>
                        <span class="file-name" id="currentFileName">No file loaded</span>
                        <span class="file-duration" id="fileDuration">--:--</span>
                    </div>
                    
                    <div class="playback-controls">
                        <button class="btn-control" id="btnPrevious" title="Previous">
                            ÃƒÂ¢Ã‚ÂÃ‚Â®ÃƒÂ¯Ã‚Â¸Ã‚Â
                        </button>
                        <button class="btn-control btn-play" id="btnPlay" title="Play">
                            ÃƒÂ¢Ã¢â‚¬â€œÃ‚Â¶ÃƒÂ¯Ã‚Â¸Ã‚Â
                        </button>
                        <button class="btn-control" id="btnPause" title="Pause" style="display: none;">
                            ÃƒÂ¢Ã‚ÂÃ‚Â¸ÃƒÂ¯Ã‚Â¸Ã‚Â
                        </button>
                        <button class="btn-control" id="btnStop" title="Stop">
                            ÃƒÂ¢Ã‚ÂÃ‚Â¹ÃƒÂ¯Ã‚Â¸Ã‚Â
                        </button>
                        <button class="btn-control" id="btnNext" title="Next">
                            ÃƒÂ¢Ã‚ÂÃ‚Â­ÃƒÂ¯Ã‚Â¸Ã‚Â
                        </button>
                    </div>
                    
                    <div class="tempo-control">
                        <label>Tempo:</label>
                        <input type="range" id="tempoSlider" min="50" max="200" value="100" step="1">
                        <span id="tempoValue">100%</span>
                    </div>
                    
                    <div class="top-bar-actions">
                        <button class="btn-secondary" onclick="homeController.openEditor()" title="Open Editor">
                            ÃƒÂ¢Ã…â€œÃ‚ÂÃƒÂ¯Ã‚Â¸Ã‚Â Editor
                        </button>
                        <button class="btn-secondary" onclick="homeController.openSettings()" title="Settings">
                            ÃƒÂ¢Ã…Â¡Ã¢â€žÂ¢ÃƒÂ¯Ã‚Â¸Ã‚Â
                        </button>
                    </div>
                </div>

                <!-- Timeline mini -->
                <div class="timeline-mini">
                    <div class="progress-bar" id="progressBar">
                        <div class="progress-fill" id="progressFill"></div>
                        <div class="playhead" id="playhead"></div>
                    </div>
                    <div class="time-display">
                        <span id="currentTime">0:00</span>
                        <span class="separator">/</span>
                        <span id="totalTime">0:00</span>
                    </div>
                </div>

                <!-- Layout principal -->
                <div class="home-layout">
                    <!-- Section gauche - SÃƒÆ’Ã‚Â©lection et Routing (25%) -->
                    <aside class="left-panel">
                        <div class="panel-section file-section">
                            <h3>File Selection</h3>
                            <div class="file-selector">
                                <select id="fileSelect" class="file-dropdown">
                                    <option value="">-- Select a file --</option>
                                </select>
                                <button class="btn-icon" onclick="homeController.refreshFiles()" title="Refresh">
                                    ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ¢â‚¬Å¾
                                </button>
                                <button class="btn-icon" onclick="homeController.uploadFile()" title="Upload">
                                    ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã‚Â
                                </button>
                            </div>
                            
                            <div class="playlist-section">
                                <div class="section-header">
                                    <span>Playlist</span>
                                    <button class="btn-icon" onclick="homeController.managePlaylist()">
                                        ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã¢â‚¬Â¹
                                    </button>
                                </div>
                                <div class="playlist-info" id="playlistInfo">
                                    <p class="empty-state">No playlist active</p>
                                </div>
                            </div>
                        </div>

                        <div class="panel-section routing-section">
                            <div class="section-header">
                                <h3>Quick Routing</h3>
                                <button class="btn-small" onclick="homeController.autoRoute()">
                                    ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¯ Auto
                                </button>
                                <button class="btn-small" onclick="homeController.clearRouting()">
                                    ÃƒÂ°Ã…Â¸Ã¢â‚¬â€Ã¢â‚¬ËœÃƒÂ¯Ã‚Â¸Ã‚Â Clear
                                </button>
                            </div>
                            
                            <div class="routing-grid" id="routingGrid">
                                <!-- GÃƒÆ’Ã‚Â©nÃƒÆ’Ã‚Â©rÃƒÆ’Ã‚Â© dynamiquement -->
                            </div>
                            
                            <div class="routing-presets">
                                <label>Presets:</label>
                                <select id="routingPresetSelect">
                                    <option value="">-- Select preset --</option>
                                </select>
                                <button class="btn-icon" onclick="homeController.saveRoutingPreset()" title="Save preset">
                                    ÃƒÂ°Ã…Â¸Ã¢â‚¬â„¢Ã‚Â¾
                                </button>
                            </div>
                            
                            <div class="routing-stats" id="routingStats">
                                <!-- Statistiques de compatibilitÃƒÆ’Ã‚Â© -->
                            </div>
                        </div>
                    </aside>

                    <!-- Section principale - Visualizer Live (75%) -->
                    <main class="main-panel">
                        <div class="visualizer-header">
                            <div class="visualizer-controls">
                                <label>Preview:</label>
                                <input type="range" id="previewTimeSlider" 
                                       min="500" max="5000" value="2000" step="100">
                                <span id="previewTimeValue">2.0s</span>
                            </div>
                            
                            <div class="channel-filter-toggles" id="channelToggles">
                                <!-- Toggles par canal gÃƒÆ’Ã‚Â©nÃƒÆ’Ã‚Â©rÃƒÆ’Ã‚Â©s dynamiquement -->
                            </div>
                            
                            <div class="visualizer-view-options">
                                <label>
                                    <input type="checkbox" id="showVelocity" checked>
                                    Velocity
                                </label>
                                <label>
                                    <input type="checkbox" id="showCC">
                                    CC Values
                                </label>
                                <label>
                                    <input type="checkbox" id="showNoteNames" checked>
                                    Note Names
                                </label>
                            </div>
                        </div>
                        
                        <div class="visualizer-container">
                            <canvas id="visualizerCanvas"></canvas>
                            
                            <!-- Overlay pour informations -->
                            <div class="visualizer-overlay">
                                <div class="note-preview" id="notePreview">
                                    <!-- Notes ÃƒÆ’Ã‚Â  venir dans les prochaines secondes -->
                                </div>
                                
                                <div class="cc-monitor" id="ccMonitor" style="display: none;">
                                    <!-- Valeurs CC en temps rÃƒÆ’Ã‚Â©el -->
                                </div>
                                
                                <div class="channel-activity" id="channelActivity">
                                    <!-- Indicateurs d'activitÃƒÆ’Ã‚Â© par canal -->
                                </div>
                            </div>
                            
                            <!-- Message quand pas de fichier -->
                            <div class="empty-visualizer" id="emptyVisualizer">
                                <div class="empty-state-large">
                                    <span class="icon">ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¹</span>
                                    <h2>No MIDI file loaded</h2>
                                    <p>Select a file from the left panel to start</p>
                                    <button class="btn-primary" onclick="homeController.selectFirstFile()">
                                        Load First File
                                    </button>
                                </div>
                            </div>
                        </div>
                    </main>
                </div>
            </div>
        `;

        // Initialiser le visualizer
        this.initVisualizer();
    }

    /**
     * Initialise le visualizer live
     */
    initVisualizer() {
        const canvas = document.getElementById('visualizerCanvas');
        
        if (canvas) {
            this.visualizer = new MidiVisualizer(canvas, {
                previewTime: 2000,
                showVelocity: true,
                showCC: false,
                showNoteNames: true
            }, this.eventBus);
        }
    }

    /**
     * Attache les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
     */
    attachEvents() {
        // ContrÃƒÆ’Ã‚Â´les de lecture
        document.getElementById('btnPlay')?.addEventListener('click', () => {
            homeController.play();
        });
        
        document.getElementById('btnPause')?.addEventListener('click', () => {
            homeController.pause();
        });
        
        document.getElementById('btnStop')?.addEventListener('click', () => {
            homeController.stop();
        });
        
        document.getElementById('btnPrevious')?.addEventListener('click', () => {
            homeController.previous();
        });
        
        document.getElementById('btnNext')?.addEventListener('click', () => {
            homeController.next();
        });

        // SÃƒÆ’Ã‚Â©lection de fichier
        document.getElementById('fileSelect')?.addEventListener('change', (e) => {
            homeController.loadFile(e.target.value);
        });

        // Tempo
        document.getElementById('tempoSlider')?.addEventListener('input', (e) => {
            const tempo = parseInt(e.target.value);
            document.getElementById('tempoValue').textContent = `${tempo}%`;
            homeController.setTempo(tempo);
        });

        // Preview time
        document.getElementById('previewTimeSlider')?.addEventListener('input', (e) => {
            const time = parseInt(e.target.value);
            document.getElementById('previewTimeValue').textContent = `${(time / 1000).toFixed(1)}s`;
            
            if (this.visualizer) {
                this.visualizer.setPreviewTime(time);
            }
        });

        // Options visualizer
        document.getElementById('showVelocity')?.addEventListener('change', (e) => {
            if (this.visualizer) {
                this.visualizer.setShowVelocity(e.target.checked);
            }
        });

        document.getElementById('showCC')?.addEventListener('change', (e) => {
            if (this.visualizer) {
                this.visualizer.setShowCC(e.target.checked);
            }
            document.getElementById('ccMonitor').style.display = e.target.checked ? 'block' : 'none';
        });

        document.getElementById('showNoteNames')?.addEventListener('change', (e) => {
            if (this.visualizer) {
                this.visualizer.setShowNoteNames(e.target.checked);
            }
        });

        // Progress bar
        document.getElementById('progressBar')?.addEventListener('click', (e) => {
            const rect = e.currentTarget.getBoundingClientRect();
            const percent = (e.clientX - rect.left) / rect.width;
            homeController.seek(percent);
        });

        // Preset routing
        document.getElementById('routingPresetSelect')?.addEventListener('change', (e) => {
            if (e.target.value) {
                homeController.loadRoutingPreset(e.target.value);
            }
        });
    }


/**
 * Attache les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements DOM
 */
attachDOMEvents() {
    // Player controls
    this.on('click', '[data-view-action="togglePlayback"]', () => {
        this.emit('playback:toggle');
    });
    
    this.on('click', '[data-view-action="stop"]', () => {
        this.emit('playback:stop');
    });
    
    this.on('click', '[data-view-action="previous"]', () => {
        this.emit('playlist:previous');
    });
    
    this.on('click', '[data-view-action="next"]', () => {
        this.emit('playlist:next');
    });
    
    // Progress bar seek
    this.on('click', '[data-view-action="seek"]', (e) => {
        const bar = e.currentTarget;
        const rect = bar.getBoundingClientRect();
        const percent = (e.clientX - rect.left) / rect.width;
        this.emit('playback:seek', { percent });
    });
    
    // Volume
    this.on('input', '[data-view-action="setVolume"]', (e) => {
        const volume = parseInt(e.target.value);
        this.emit('playback:volume', { volume });
    });
    
    // Shuffle/Repeat
    this.on('click', '[data-view-action="toggleShuffle"]', () => {
        this.emit('playlist:toggle-shuffle');
    });
    
    this.on('click', '[data-view-action="toggleRepeat"]', () => {
        this.emit('playlist:toggle-repeat');
    });
    
    // File actions
    this.on('click', '[data-view-action="playFile"]', (e) => {
        const fileId = e.currentTarget.dataset.fileId;
        this.emit('file:play', { fileId });
    });
    
    this.on('click', '[data-view-action="addToQueue"]', (e) => {
        const fileId = e.currentTarget.dataset.fileId;
        this.emit('playlist:add-to-queue', { fileId });
    });
    
    // Navigation
    this.on('click', '[data-view-action="openFileExplorer"]', () => {
        this.emit('navigation:goto', { page: 'files' });
    });
    
    this.on('click', '[data-view-action="openEditor"]', () => {
        this.emit('navigation:goto', { page: 'editor' });
    });
    
    this.on('click', '[data-view-action="openRouting"]', () => {
        this.emit('navigation:goto', { page: 'routing' });
    });
    
    this.on('click', '[data-view-action="openSystem"]', () => {
        this.emit('navigation:goto', { page: 'system' });
    });
    
    // Tabs
    this.on('click', '.tab-btn', (e) => {
        this.switchTab(e.currentTarget.dataset.tab);
    });
}

/**
 * Change de tab
 */
switchTab(tabName) {
    if (!this.container) {
        console.warn('[HomeView] Cannot switch tab: container not found');
        return;
    }
    
    // DÃƒÆ’Ã‚Â©sactiver tous les tabs
    this.container.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    
    this.container.querySelectorAll('.tab-content').forEach(content => {
        content.classList.remove('active');
    });
    
    // Activer le tab sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©
    this.container.querySelector(`[data-tab="${tabName}"]`)?.classList.add('active');
    this.container.querySelector(`[data-tab-content="${tabName}"]`)?.classList.add('active');
}





    /**
     * Met ÃƒÆ’Ã‚Â  jour la liste des fichiers
     */
    updateFileList(files) {
        const select = document.getElementById('fileSelect');
        
        if (!select) return;

        select.innerHTML = '<option value="">-- Select a file --</option>';
        
        files.forEach(file => {
            const option = document.createElement('option');
            option.value = file.id;
            option.textContent = file.name;
            select.appendChild(option);
        });
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour les informations du fichier courant
     */
    updateCurrentFile(file) {
        this.currentFile = file;

        if (file) {
            document.getElementById('currentFileName').textContent = file.name;
            document.getElementById('fileDuration').textContent = this.formatTime(file.duration);
            document.getElementById('totalTime').textContent = this.formatTime(file.duration);
            document.getElementById('emptyVisualizer').style.display = 'none';
            
            // Charger dans le visualizer
            if (this.visualizer && file.midiJson) {
                this.visualizer.loadMidiJson(file.midiJson);
            }
        } else {
            document.getElementById('currentFileName').textContent = 'No file loaded';
            document.getElementById('fileDuration').textContent = '--:--';
            document.getElementById('emptyVisualizer').style.display = 'flex';
        }
    }
/**
 * Met ÃƒÆ’Ã‚Â  jour la position de lecture
 */
updatePlaybackPosition(position, duration) {
    if (!this.container) {
        return;
    }
    
    const progress = duration > 0 ? (position / duration) * 100 : 0;
    
    // Mise ÃƒÆ’Ã‚Â  jour barre de progression
    const fill = this.container.querySelector('.progress-fill');
    const handle = this.container.querySelector('.progress-handle');
    
    if (fill) fill.style.width = `${progress}%`;
    if (handle) handle.style.left = `${progress}%`;
    
    // Mise ÃƒÆ’Ã‚Â  jour temps
    const currentTimeEl = this.container.querySelector('.current-time');
    if (currentTimeEl) {
        currentTimeEl.textContent = Formatter.formatDuration(position);
    }
}

/**
 * Met ÃƒÆ’Ã‚Â  jour l'ÃƒÆ’Ã‚Â©tat de lecture
 */
updatePlaybackState(isPlaying) {
    const playPauseBtn = this.container.querySelector('.btn-play-pause .icon');
    const status = this.container.querySelector('.player-status');
    
    if (playPauseBtn) {
        playPauseBtn.textContent = isPlaying ? 'ÃƒÂ¢Ã‚ÂÃ‚Â¸ÃƒÂ¯Ã‚Â¸Ã‚Â' : 'ÃƒÂ¢Ã¢â‚¬â€œÃ‚Â¶ÃƒÂ¯Ã‚Â¸Ã‚Â';
    }
    
    if (status) {
        status.className = `player-status ${isPlaying ? 'playing' : 'paused'}`;
        status.textContent = isPlaying ? 'ÃƒÂ¢Ã¢â‚¬â€œÃ‚Â¶ÃƒÂ¯Ã‚Â¸Ã‚Â Lecture' : 'ÃƒÂ¢Ã‚ÂÃ‚Â¸ÃƒÂ¯Ã‚Â¸Ã‚Â Pause';
    }
}

/**
 * Met ÃƒÆ’Ã‚Â  jour le fichier courant
 */
updateCurrentFile(file) {
    const fileName = this.container.querySelector('.file-name');
    const fileInfo = this.container.querySelector('.file-metadata');
    
    if (fileName) {
        fileName.textContent = file ? file.name : 'Aucun fichier sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©';
    }
    
    if (fileInfo && file) {
        fileInfo.innerHTML = this.buildFileInfo(file);
    }
}
    /**
     * Met ÃƒÆ’Ã‚Â  jour la grille de routing
     */
    updateRoutingGrid(channels, instruments) {
        const grid = document.getElementById('routingGrid');
        
        if (!grid) return;

        grid.innerHTML = channels.map(channel => {
            const routing = routingModel.getRouting(channel.number);
            const compatibility = routing ? routing.compatibility : null;
            
            return `
                <div class="routing-row" data-channel="${channel.number}">
                    <div class="channel-indicator" style="background: ${channel.color}">
                        ${channel.number}
                    </div>
                    
                    <select class="instrument-select" 
                            data-channel="${channel.number}"
                            onchange="homeController.assignInstrument(${channel.number}, this.value)">
                        <option value="">-- None --</option>
                        ${instruments.map(inst => `
                            <option value="${inst.id}" 
                                    ${routing?.instrumentId === inst.id ? 'selected' : ''}>
                                ${inst.name}
                            </option>
                        `).join('')}
                    </select>
                    
                    <div class="compatibility-badge" id="compat-${channel.number}">
                        ${this.getCompatibilityBadge(compatibility)}
                    </div>
                </div>
            `;
        }).join('');
    }

    /**
     * Obtient le badge de compatibilitÃƒÆ’Ã‚Â©
     */
    getCompatibilityBadge(compatibility) {
        if (!compatibility) {
            return '<span class="badge badge-neutral">-</span>';
        }

        const percent = compatibility.percentage;
        
        if (percent === 100) {
            return '<span class="badge badge-success" title="Perfect compatibility">ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦</span>';
        } else if (percent >= 80) {
            return `<span class="badge badge-warning" title="${percent}% compatible">ÃƒÂ¢Ã…Â¡Ã‚Â ÃƒÂ¯Ã‚Â¸Ã‚Â</span>`;
        } else if (percent >= 50) {
            return `<span class="badge badge-warning" title="${percent}% compatible">ÃƒÂ¢Ã…Â¡Ã‚Â ÃƒÂ¯Ã‚Â¸Ã‚Â</span>`;
        } else {
            return `<span class="badge badge-error" title="${percent}% compatible">ÃƒÂ¢Ã‚ÂÃ…â€™</span>`;
        }
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour les statistiques de routing
     */
    updateRoutingStats(stats) {
        const container = document.getElementById('routingStats');
        
        if (!container) return;

        if (!stats || stats.perfectChannels === 0 && stats.goodChannels === 0) {
            container.innerHTML = '<p class="empty-state">No routing configured</p>';
            return;
        }

        container.innerHTML = `
            <div class="stats-grid">
                <div class="stat-item">
                    <span class="stat-label">Average:</span>
                    <span class="stat-value">${stats.averageCompatibility}%</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Perfect:</span>
                    <span class="stat-value badge-success">${stats.perfectChannels}</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Good:</span>
                    <span class="stat-value badge-warning">${stats.goodChannels}</span>
                </div>
                <div class="stat-item">
                    <span class="stat-label">Issues:</span>
                    <span class="stat-value badge-error">${stats.incompatibleChannels}</span>
                </div>
            </div>
        `;
    }
// frontend/js/views/HomeView.js - MÃƒÆ’Ã‚Â©thode buildTemplate()

/**
 * Construit le template HTML de la page d'accueil
 */
buildTemplate(data = {}) {
    const {
        currentFile = null,
        recentFiles = [],
        connectedInstruments = [],
        playlists = [],
        isPlaying = false,
        position = 0,
        duration = 0
    } = data;
    
    return `
        <div class="home-container">
            ${this.buildPlayerSection(currentFile, isPlaying, position, duration)}
            ${this.buildFileSelectorSection(recentFiles, playlists)}
            ${this.buildInstrumentStatusSection(connectedInstruments)}
            ${this.buildQuickActionsSection()}
        </div>
    `;
}

/**
 * Section Player principal
 */
buildPlayerSection(file, isPlaying, position, duration) {
    const fileName = file ? file.name : 'Aucun fichier sÃƒÆ’Ã‚Â©lectionnÃƒÆ’Ã‚Â©';
    const fileInfo = file ? this.buildFileInfo(file) : '';
    const progress = duration > 0 ? (position / duration) * 100 : 0;
    
    return `
        <section class="player-section">
            <div class="player-header">
                <h2>ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Âµ Lecteur MIDI</h2>
                <div class="player-status ${isPlaying ? 'playing' : 'paused'}">
                    ${isPlaying ? 'ÃƒÂ¢Ã¢â‚¬â€œÃ‚Â¶ÃƒÂ¯Ã‚Â¸Ã‚Â Lecture' : 'ÃƒÂ¢Ã‚ÂÃ‚Â¸ÃƒÂ¯Ã‚Â¸Ã‚Â Pause'}
                </div>
            </div>
            
            <div class="current-file-display">
                <div class="file-icon">ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¹</div>
                <div class="file-details">
                    <div class="file-name">${fileName}</div>
                    ${fileInfo}
                </div>
            </div>
            
            <div class="playback-controls">
                <div class="progress-bar" data-view-action="seek">
                    <div class="progress-fill" style="width: ${progress}%"></div>
                    <div class="progress-handle" style="left: ${progress}%"></div>
                </div>
                
                <div class="time-display">
                    <span class="current-time">${Formatter.formatDuration(position)}</span>
                    <span class="separator">/</span>
                    <span class="total-time">${Formatter.formatDuration(duration)}</span>
                </div>
                
                <div class="control-buttons">
                    <button class="btn-control" data-view-action="previous" title="PrÃƒÆ’Ã‚Â©cÃƒÆ’Ã‚Â©dent">
                        <span class="icon">ÃƒÂ¢Ã‚ÂÃ‚Â®ÃƒÂ¯Ã‚Â¸Ã‚Â</span>
                    </button>
                    <button class="btn-control btn-play-pause" data-view-action="togglePlayback">
                        <span class="icon">${isPlaying ? 'ÃƒÂ¢Ã‚ÂÃ‚Â¸ÃƒÂ¯Ã‚Â¸Ã‚Â' : 'ÃƒÂ¢Ã¢â‚¬â€œÃ‚Â¶ÃƒÂ¯Ã‚Â¸Ã‚Â'}</span>
                    </button>
                    <button class="btn-control" data-view-action="stop" title="Stop">
                        <span class="icon">ÃƒÂ¢Ã‚ÂÃ‚Â¹ÃƒÂ¯Ã‚Â¸Ã‚Â</span>
                    </button>
                    <button class="btn-control" data-view-action="next" title="Suivant">
                        <span class="icon">ÃƒÂ¢Ã‚ÂÃ‚Â­ÃƒÂ¯Ã‚Â¸Ã‚Â</span>
                    </button>
                </div>
                
                <div class="secondary-controls">
                    <button class="btn-icon" data-view-action="toggleShuffle" title="Lecture alÃƒÆ’Ã‚Â©atoire">
                        ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ¢â€šÂ¬
                    </button>
                    <button class="btn-icon" data-view-action="toggleRepeat" title="RÃƒÆ’Ã‚Â©pÃƒÆ’Ã‚Â©ter">
                        ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ‚Â
                    </button>
                    <div class="volume-control">
                        <span class="icon">ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ…Â </span>
                        <input type="range" 
                               class="volume-slider" 
                               min="0" max="100" 
                               value="100"
                               data-view-action="setVolume">
                    </div>
                </div>
            </div>
        </section>
    `;
}

/**
 * Section sÃƒÆ’Ã‚Â©lecteur fichiers
 */
buildFileSelectorSection(recentFiles, playlists) {
    return `
        <section class="file-selector-section">
            <div class="selector-tabs">
                <button class="tab-btn active" data-tab="recent">
                    ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã‚Â RÃƒÆ’Ã‚Â©cents
                </button>
                <button class="tab-btn" data-tab="playlists">
                    ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã¢â‚¬Â¹ Playlists
                </button>
                <button class="tab-btn" data-tab="browse">
                    ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ‚Â Parcourir
                </button>
            </div>
            
            <div class="tab-content active" data-tab-content="recent">
                ${this.buildRecentFilesList(recentFiles)}
            </div>
            
            <div class="tab-content" data-tab-content="playlists">
                ${this.buildPlaylistsList(playlists)}
            </div>
            
            <div class="tab-content" data-tab-content="browse">
                <div class="browse-placeholder">
                    <p>Cliquez pour ouvrir l'explorateur de fichiers</p>
                    <button class="btn-primary" data-view-action="openFileExplorer">
                        ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã¢â‚¬Å¡ Parcourir les fichiers
                    </button>
                </div>
            </div>
        </section>
    `;
}

/**
 * Liste fichiers rÃƒÆ’Ã‚Â©cents
 */
buildRecentFilesList(files) {
    if (files.length === 0) {
        return `
            <div class="empty-state">
                <div class="empty-icon">ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¼</div>
                <p>Aucun fichier rÃƒÆ’Ã‚Â©cent</p>
                <button class="btn-secondary" data-view-action="openFileExplorer">
                    Ajouter des fichiers
                </button>
            </div>
        `;
    }
    
    return `
        <div class="file-list">
            ${files.map(file => this.buildFileCard(file)).join('')}
        </div>
    `;
}

/**
 * Carte fichier
 */
buildFileCard(file) {
    return `
        <div class="file-card" data-file-id="${file.id}">
            <div class="file-card-icon">ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¹</div>
            <div class="file-card-info">
                <div class="file-card-name">${file.name}</div>
                <div class="file-card-meta">
                    <span class="duration">${Formatter.formatDuration(file.duration)}</span>
                    <span class="separator">ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¢</span>
                    <span class="size">${Formatter.formatFileSize(file.size)}</span>
                </div>
            </div>
            <div class="file-card-actions">
                <button class="btn-icon-small" 
                        data-view-action="playFile" 
                        data-file-id="${file.id}"
                        title="Lire">
                    ÃƒÂ¢Ã¢â‚¬â€œÃ‚Â¶ÃƒÂ¯Ã‚Â¸Ã‚Â
                </button>
                <button class="btn-icon-small" 
                        data-view-action="addToQueue" 
                        data-file-id="${file.id}"
                        title="Ajouter ÃƒÆ’Ã‚Â  la file">
                    ÃƒÂ¢Ã…Â¾Ã¢â‚¬Â¢
                </button>
            </div>
        </div>
    `;
}

/**
 * Section status instruments
 */
buildInstrumentStatusSection(instruments) {
    const connectedCount = instruments.length;
    const statusClass = connectedCount > 0 ? 'connected' : 'disconnected';
    
    return `
        <section class="instrument-status-section">
            <div class="status-header">
                <h3>ÃƒÂ°Ã…Â¸Ã…Â½Ã¢â‚¬ÂºÃƒÂ¯Ã‚Â¸Ã‚Â Instruments</h3>
                <span class="status-badge ${statusClass}">
                    ${connectedCount} connectÃƒÆ’Ã‚Â©${connectedCount > 1 ? 's' : ''}
                </span>
            </div>
            
            ${connectedCount > 0 ? this.buildInstrumentList(instruments) : this.buildNoInstruments()}
        </section>
    `;
}

buildInstrumentList(instruments) {
    return `
        <div class="instrument-list-compact">
            ${instruments.map(inst => `
                <div class="instrument-item">
                    <span class="instrument-icon">${this.getInstrumentIcon(inst.type)}</span>
                    <span class="instrument-name">${inst.name}</span>
                    <span class="instrument-status online"></span>
                </div>
            `).join('')}
        </div>
    `;
}

buildNoInstruments() {
    return `
        <div class="no-instruments">
            <p>Aucun instrument connectÃƒÆ’Ã‚Â©</p>
            <button class="btn-secondary" data-view-action="scanInstruments">
                ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ‚Â Rechercher des instruments
            </button>
        </div>
    `;
}

/**
 * Actions rapides
 */
buildQuickActionsSection() {
    return `
        <section class="quick-actions-section">
            <h3>ÃƒÂ¢Ã…Â¡Ã‚Â¡ Actions Rapides</h3>
            <div class="quick-actions-grid">
                <button class="action-card" data-view-action="openEditor">
                    <span class="action-icon">ÃƒÂ¢Ã…â€œÃ‚ÂÃƒÂ¯Ã‚Â¸Ã‚Â</span>
                    <span class="action-label">ÃƒÆ’Ã¢â‚¬Â°diteur MIDI</span>
                </button>
                
                <button class="action-card" data-view-action="openRouting">
                    <span class="action-icon">ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ¢â€šÂ¬</span>
                    <span class="action-label">Configuration Routing</span>
                </button>
                
                <button class="action-card" data-view-action="createPlaylist">
                    <span class="action-icon">ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã¢â‚¬Â¹</span>
                    <span class="action-label">Nouvelle Playlist</span>
                </button>
                
                <button class="action-card" data-view-action="openSystem">
                    <span class="action-icon">ÃƒÂ¢Ã…Â¡Ã¢â€žÂ¢ÃƒÂ¯Ã‚Â¸Ã‚Â</span>
                    <span class="action-label">ParamÃƒÆ’Ã‚Â¨tres</span>
                </button>
            </div>
        </section>
    `;
}

/**
 * Helpers
 */
getInstrumentIcon(type) {
    const icons = {
        'usb': 'ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ…â€™',
        'wifi': 'ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã‚Â¶',
        'bluetooth': 'ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã‚Â¶',
        'virtual': 'ÃƒÂ°Ã…Â¸Ã¢â‚¬â„¢Ã‚Â»'
    };
    return icons[type] || 'ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Â¹';
}

buildFileInfo(file) {
    return `
        <div class="file-metadata">
            ${file.tempo ? `<span class="meta-item">ÃƒÂ¢Ã¢â€žÂ¢Ã‚Â© ${file.tempo} BPM</span>` : ''}
            ${file.timeSignature ? `<span class="meta-item">ÃƒÂ¢Ã‚ÂÃ‚Â±ÃƒÂ¯Ã‚Â¸Ã‚Â ${file.timeSignature}</span>` : ''}
            ${file.trackCount ? `<span class="meta-item">ÃƒÂ°Ã…Â¸Ã…Â½Ã‚Âµ ${file.trackCount} pistes</span>` : ''}
        </div>
    `;
}
    /**
     * Met ÃƒÆ’Ã‚Â  jour les toggles de canaux
     */
    updateChannelToggles(channels) {
        const container = document.getElementById('channelToggles');
        
        if (!container) return;

        container.innerHTML = channels.map(channel => `
            <label class="channel-toggle" title="Channel ${channel.number}">
                <input type="checkbox" checked 
                       data-channel="${channel.number}"
                       onchange="homeController.toggleChannel(${channel.number}, this.checked)">
                <span class="toggle-indicator" style="background: ${channel.color}">
                    ${channel.number}
                </span>
            </label>
        `).join('');
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour l'ÃƒÆ’Ã‚Â©tat de lecture
     */
    updatePlaybackState(state) {
        const btnPlay = document.getElementById('btnPlay');
        const btnPause = document.getElementById('btnPause');

        if (state === 'playing') {
            btnPlay.style.display = 'none';
            btnPause.style.display = 'inline-block';
        } else {
            btnPlay.style.display = 'inline-block';
            btnPause.style.display = 'none';
        }
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour la barre de progression
     */
    updateProgress(currentTime, totalTime) {
        const percent = (currentTime / totalTime) * 100;
        
        document.getElementById('progressFill').style.width = `${percent}%`;
        document.getElementById('playhead').style.left = `${percent}%`;
        document.getElementById('currentTime').textContent = this.formatTime(currentTime);
        
        // Mettre ÃƒÆ’Ã‚Â  jour le visualizer
        if (this.visualizer) {
            this.visualizer.update(currentTime);
        }
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour l'overlay de notes ÃƒÆ’Ã‚Â  venir
     */
    updateNotePreview(upcomingNotes) {
        const container = document.getElementById('notePreview');
        
        if (!container || upcomingNotes.length === 0) {
            if (container) container.innerHTML = '';
            return;
        }

        // Afficher les 5 prochaines notes
        const preview = upcomingNotes.slice(0, 5);
        
        container.innerHTML = `
            <div class="preview-header">Upcoming Notes:</div>
            ${preview.map(note => `
                <div class="preview-note" style="border-left: 3px solid ${this.getChannelColor(note.channel)}">
                    <span class="note-name">${this.getNoteName(note.note)}</span>
                    <span class="note-time">in ${Math.round(note.timeOffset)}ms</span>
                    <span class="note-channel">Ch${note.channel + 1}</span>
                </div>
            `).join('')}
        `;
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour le moniteur CC
     */
    updateCCMonitor(ccValues) {
        const container = document.getElementById('ccMonitor');
        
        if (!container || ccValues.size === 0) return;

        container.innerHTML = Array.from(ccValues.entries()).map(([cc, value]) => `
            <div class="cc-value">
                <span class="cc-label">CC${cc}</span>
                <div class="cc-bar">
                    <div class="cc-fill" style="width: ${(value / 127) * 100}%"></div>
                </div>
                <span class="cc-number">${value}</span>
            </div>
        `).join('');
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour l'activitÃƒÆ’Ã‚Â© des canaux
     */
    updateChannelActivity(channels) {
        const container = document.getElementById('channelActivity');
        
        if (!container) return;

        container.innerHTML = channels
            .filter(ch => ch.active)
            .map(ch => `
                <div class="channel-activity-indicator" style="background: ${ch.color}">
                    ${ch.number}
                </div>
            `).join('');
    }

    /**
     * Formate un temps en ms en MM:SS
     */
    formatTime(ms) {
        const seconds = Math.floor(ms / 1000);
        const minutes = Math.floor(seconds / 60);
        const secs = seconds % 60;
        
        return `${minutes}:${secs.toString().padStart(2, '0')}`;
    }

    /**
     * Obtient le nom d'une note MIDI
     */
    getNoteName(midiNote) {
        const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
        const octave = Math.floor(midiNote / 12) - 1;
        return `${names[midiNote % 12]}${octave}`;
    }

    /**
     * Obtient la couleur d'un canal
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
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = HomeView;
}

if (typeof window !== 'undefined') {
    window.HomeView = HomeView; 
}
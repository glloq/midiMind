/**
 * HomeView.js
 * Vue de la page d'accueil avec player et visualizer live
 */

class HomeView {
    constructor(container) {
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
        
        this.visualizer = null;
        this.currentFile = null;
        this.logger = window.logger || console;
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
                <!-- Barre de contrÃ´le supÃ©rieure -->
                <div class="top-bar">
                    <div class="file-info">
                        <span class="file-icon">ðŸŽµ</span>
                        <span class="file-name" id="currentFileName">No file loaded</span>
                        <span class="file-duration" id="fileDuration">--:--</span>
                    </div>
                    
                    <div class="playback-controls">
                        <button class="btn-control" id="btnPrevious" title="Previous">
                            â®ï¸
                        </button>
                        <button class="btn-control btn-play" id="btnPlay" title="Play">
                            â–¶ï¸
                        </button>
                        <button class="btn-control" id="btnPause" title="Pause" style="display: none;">
                            â¸ï¸
                        </button>
                        <button class="btn-control" id="btnStop" title="Stop">
                            â¹ï¸
                        </button>
                        <button class="btn-control" id="btnNext" title="Next">
                            â­ï¸
                        </button>
                    </div>
                    
                    <div class="tempo-control">
                        <label>Tempo:</label>
                        <input type="range" id="tempoSlider" min="50" max="200" value="100" step="1">
                        <span id="tempoValue">100%</span>
                    </div>
                    
                    <div class="top-bar-actions">
                        <button class="btn-secondary" onclick="homeController.openEditor()" title="Open Editor">
                            âœï¸ Editor
                        </button>
                        <button class="btn-secondary" onclick="homeController.openSettings()" title="Settings">
                            âš™ï¸
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
                    <!-- Section gauche - SÃ©lection et Routing (25%) -->
                    <aside class="left-panel">
                        <div class="panel-section file-section">
                            <h3>File Selection</h3>
                            <div class="file-selector">
                                <select id="fileSelect" class="file-dropdown">
                                    <option value="">-- Select a file --</option>
                                </select>
                                <button class="btn-icon" onclick="homeController.refreshFiles()" title="Refresh">
                                    ðŸ”„
                                </button>
                                <button class="btn-icon" onclick="homeController.uploadFile()" title="Upload">
                                    ðŸ“
                                </button>
                            </div>
                            
                            <div class="playlist-section">
                                <div class="section-header">
                                    <span>Playlist</span>
                                    <button class="btn-icon" onclick="homeController.managePlaylist()">
                                        ðŸ“‹
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
                                    ðŸŽ¯ Auto
                                </button>
                                <button class="btn-small" onclick="homeController.clearRouting()">
                                    ðŸ—‘ï¸ Clear
                                </button>
                            </div>
                            
                            <div class="routing-grid" id="routingGrid">
                                <!-- GÃ©nÃ©rÃ© dynamiquement -->
                            </div>
                            
                            <div class="routing-presets">
                                <label>Presets:</label>
                                <select id="routingPresetSelect">
                                    <option value="">-- Select preset --</option>
                                </select>
                                <button class="btn-icon" onclick="homeController.saveRoutingPreset()" title="Save preset">
                                    ðŸ’¾
                                </button>
                            </div>
                            
                            <div class="routing-stats" id="routingStats">
                                <!-- Statistiques de compatibilitÃ© -->
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
                                <!-- Toggles par canal gÃ©nÃ©rÃ©s dynamiquement -->
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
                                    <!-- Notes Ã  venir dans les prochaines secondes -->
                                </div>
                                
                                <div class="cc-monitor" id="ccMonitor" style="display: none;">
                                    <!-- Valeurs CC en temps rÃ©el -->
                                </div>
                                
                                <div class="channel-activity" id="channelActivity">
                                    <!-- Indicateurs d'activitÃ© par canal -->
                                </div>
                            </div>
                            
                            <!-- Message quand pas de fichier -->
                            <div class="empty-visualizer" id="emptyVisualizer">
                                <div class="empty-state-large">
                                    <span class="icon">ðŸŽ¹</span>
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
            this.visualizer = new LiveVisualizer(canvas, {
                previewTime: 2000,
                showVelocity: true,
                showCC: false,
                showNoteNames: true
            });
        }
    }

    /**
     * Attache les Ã©vÃ©nements
     */
    attachEvents() {
        // ContrÃ´les de lecture
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

        // SÃ©lection de fichier
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
 * Attache les Ã©vÃ©nements DOM
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
    
    // DÃ©sactiver tous les tabs
    this.container.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    
    this.container.querySelectorAll('.tab-content').forEach(content => {
        content.classList.remove('active');
    });
    
    // Activer le tab sÃ©lectionnÃ©
    this.container.querySelector(`[data-tab="${tabName}"]`)?.classList.add('active');
    this.container.querySelector(`[data-tab-content="${tabName}"]`)?.classList.add('active');
}





    /**
     * Met Ã  jour la liste des fichiers
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
     * Met Ã  jour les informations du fichier courant
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
 * Met Ã  jour la position de lecture
 */
updatePlaybackPosition(position, duration) {
    if (!this.container) {
        return;
    }
    
    const progress = duration > 0 ? (position / duration) * 100 : 0;
    
    // Mise Ã  jour barre de progression
    const fill = this.container.querySelector('.progress-fill');
    const handle = this.container.querySelector('.progress-handle');
    
    if (fill) fill.style.width = `${progress}%`;
    if (handle) handle.style.left = `${progress}%`;
    
    // Mise Ã  jour temps
    const currentTimeEl = this.container.querySelector('.current-time');
    if (currentTimeEl) {
        currentTimeEl.textContent = Formatter.formatDuration(position);
    }
}

/**
 * Met Ã  jour l'Ã©tat de lecture
 */
updatePlaybackState(isPlaying) {
    const playPauseBtn = this.container.querySelector('.btn-play-pause .icon');
    const status = this.container.querySelector('.player-status');
    
    if (playPauseBtn) {
        playPauseBtn.textContent = isPlaying ? 'â¸ï¸' : 'â–¶ï¸';
    }
    
    if (status) {
        status.className = `player-status ${isPlaying ? 'playing' : 'paused'}`;
        status.textContent = isPlaying ? 'â–¶ï¸ Lecture' : 'â¸ï¸ Pause';
    }
}

/**
 * Met Ã  jour le fichier courant
 */
updateCurrentFile(file) {
    const fileName = this.container.querySelector('.file-name');
    const fileInfo = this.container.querySelector('.file-metadata');
    
    if (fileName) {
        fileName.textContent = file ? file.name : 'Aucun fichier sÃ©lectionnÃ©';
    }
    
    if (fileInfo && file) {
        fileInfo.innerHTML = this.buildFileInfo(file);
    }
}
    /**
     * Met Ã  jour la grille de routing
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
     * Obtient le badge de compatibilitÃ©
     */
    getCompatibilityBadge(compatibility) {
        if (!compatibility) {
            return '<span class="badge badge-neutral">-</span>';
        }

        const percent = compatibility.percentage;
        
        if (percent === 100) {
            return '<span class="badge badge-success" title="Perfect compatibility">âœ…</span>';
        } else if (percent >= 80) {
            return `<span class="badge badge-warning" title="${percent}% compatible">âš ï¸</span>`;
        } else if (percent >= 50) {
            return `<span class="badge badge-warning" title="${percent}% compatible">âš ï¸</span>`;
        } else {
            return `<span class="badge badge-error" title="${percent}% compatible">âŒ</span>`;
        }
    }

    /**
     * Met Ã  jour les statistiques de routing
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
// frontend/js/views/HomeView.js - MÃ©thode buildTemplate()

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
    const fileName = file ? file.name : 'Aucun fichier sÃ©lectionnÃ©';
    const fileInfo = file ? this.buildFileInfo(file) : '';
    const progress = duration > 0 ? (position / duration) * 100 : 0;
    
    return `
        <section class="player-section">
            <div class="player-header">
                <h2>ðŸŽµ Lecteur MIDI</h2>
                <div class="player-status ${isPlaying ? 'playing' : 'paused'}">
                    ${isPlaying ? 'â–¶ï¸ Lecture' : 'â¸ï¸ Pause'}
                </div>
            </div>
            
            <div class="current-file-display">
                <div class="file-icon">ðŸŽ¹</div>
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
                    <button class="btn-control" data-view-action="previous" title="PrÃ©cÃ©dent">
                        <span class="icon">â®ï¸</span>
                    </button>
                    <button class="btn-control btn-play-pause" data-view-action="togglePlayback">
                        <span class="icon">${isPlaying ? 'â¸ï¸' : 'â–¶ï¸'}</span>
                    </button>
                    <button class="btn-control" data-view-action="stop" title="Stop">
                        <span class="icon">â¹ï¸</span>
                    </button>
                    <button class="btn-control" data-view-action="next" title="Suivant">
                        <span class="icon">â­ï¸</span>
                    </button>
                </div>
                
                <div class="secondary-controls">
                    <button class="btn-icon" data-view-action="toggleShuffle" title="Lecture alÃ©atoire">
                        ðŸ”€
                    </button>
                    <button class="btn-icon" data-view-action="toggleRepeat" title="RÃ©pÃ©ter">
                        ðŸ”
                    </button>
                    <div class="volume-control">
                        <span class="icon">ðŸ”Š</span>
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
 * Section sÃ©lecteur fichiers
 */
buildFileSelectorSection(recentFiles, playlists) {
    return `
        <section class="file-selector-section">
            <div class="selector-tabs">
                <button class="tab-btn active" data-tab="recent">
                    ðŸ“ RÃ©cents
                </button>
                <button class="tab-btn" data-tab="playlists">
                    ðŸ“‹ Playlists
                </button>
                <button class="tab-btn" data-tab="browse">
                    ðŸ” Parcourir
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
                        ðŸ“‚ Parcourir les fichiers
                    </button>
                </div>
            </div>
        </section>
    `;
}

/**
 * Liste fichiers rÃ©cents
 */
buildRecentFilesList(files) {
    if (files.length === 0) {
        return `
            <div class="empty-state">
                <div class="empty-icon">ðŸŽ¼</div>
                <p>Aucun fichier rÃ©cent</p>
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
            <div class="file-card-icon">ðŸŽ¹</div>
            <div class="file-card-info">
                <div class="file-card-name">${file.name}</div>
                <div class="file-card-meta">
                    <span class="duration">${Formatter.formatDuration(file.duration)}</span>
                    <span class="separator">â€¢</span>
                    <span class="size">${Formatter.formatFileSize(file.size)}</span>
                </div>
            </div>
            <div class="file-card-actions">
                <button class="btn-icon-small" 
                        data-view-action="playFile" 
                        data-file-id="${file.id}"
                        title="Lire">
                    â–¶ï¸
                </button>
                <button class="btn-icon-small" 
                        data-view-action="addToQueue" 
                        data-file-id="${file.id}"
                        title="Ajouter Ã  la file">
                    âž•
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
                <h3>ðŸŽ›ï¸ Instruments</h3>
                <span class="status-badge ${statusClass}">
                    ${connectedCount} connectÃ©${connectedCount > 1 ? 's' : ''}
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
            <p>Aucun instrument connectÃ©</p>
            <button class="btn-secondary" data-view-action="scanInstruments">
                ðŸ” Rechercher des instruments
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
            <h3>âš¡ Actions Rapides</h3>
            <div class="quick-actions-grid">
                <button class="action-card" data-view-action="openEditor">
                    <span class="action-icon">âœï¸</span>
                    <span class="action-label">Ã‰diteur MIDI</span>
                </button>
                
                <button class="action-card" data-view-action="openRouting">
                    <span class="action-icon">ðŸ”€</span>
                    <span class="action-label">Configuration Routing</span>
                </button>
                
                <button class="action-card" data-view-action="createPlaylist">
                    <span class="action-icon">ðŸ“‹</span>
                    <span class="action-label">Nouvelle Playlist</span>
                </button>
                
                <button class="action-card" data-view-action="openSystem">
                    <span class="action-icon">âš™ï¸</span>
                    <span class="action-label">ParamÃ¨tres</span>
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
        'usb': 'ðŸ”Œ',
        'wifi': 'ðŸ“¶',
        'bluetooth': 'ðŸ“¶',
        'virtual': 'ðŸ’»'
    };
    return icons[type] || 'ðŸŽ¹';
}

buildFileInfo(file) {
    return `
        <div class="file-metadata">
            ${file.tempo ? `<span class="meta-item">â™© ${file.tempo} BPM</span>` : ''}
            ${file.timeSignature ? `<span class="meta-item">â±ï¸ ${file.timeSignature}</span>` : ''}
            ${file.trackCount ? `<span class="meta-item">ðŸŽµ ${file.trackCount} pistes</span>` : ''}
        </div>
    `;
}
    /**
     * Met Ã  jour les toggles de canaux
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
     * Met Ã  jour l'Ã©tat de lecture
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
     * Met Ã  jour la barre de progression
     */
    updateProgress(currentTime, totalTime) {
        const percent = (currentTime / totalTime) * 100;
        
        document.getElementById('progressFill').style.width = `${percent}%`;
        document.getElementById('playhead').style.left = `${percent}%`;
        document.getElementById('currentTime').textContent = this.formatTime(currentTime);
        
        // Mettre Ã  jour le visualizer
        if (this.visualizer) {
            this.visualizer.update(currentTime);
        }
    }

    /**
     * Met Ã  jour l'overlay de notes Ã  venir
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
     * Met Ã  jour le moniteur CC
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
     * Met Ã  jour l'activitÃ© des canaux
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

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = HomeView;
}
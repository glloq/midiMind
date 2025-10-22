// ===== SYSTEM VIEW - Vue de configuration systÃƒÂ¨me =====
// =====================================================
// GÃƒÂ¨re l'affichage de toutes les configurations systÃƒÂ¨me :
// - ParamÃƒÂ¨tres audio/MIDI (latence, buffer, sample rate)
// - Calibration automatique des instruments
// - PrÃƒÂ©sets de visualiseur (performance, qualitÃƒÂ©, ÃƒÂ©quilibrÃƒÂ©)
// - Statistiques en temps rÃƒÂ©el du systÃƒÂ¨me
// - Configuration des thÃƒÂ¨mes et interface
// - Monitoring des performances
// =====================================================

class SystemView extends BaseView {
    constructor(eventBus) {
        super('system-page', eventBus);
        
        // Initialize logger first
        this.logger = window.logger || console;
        
        // Ãƒâ€°tat de la vue
        this.calibrationInProgress = false;
        this.statsUpdateInterval = null;
        this.currentTheme = 'light';
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        // Log initialization
        if (this.logger && this.logger.info) {
            this.logger.info('SystemView', 'Ã¢Å“â€œ SystemView initialized');
        }
    }
    
    // Override initialize to prevent premature calls
    initialize() {
        if (!this._fullyInitialized) {
            return;
        }
        
        // Call parent initialize if needed
        if (super.initialize) {
            super.initialize();
        }
    }

    /**
     * Construit le template principal de la page systÃƒÂ¨me
     * @param {Object} data - Configuration systÃƒÂ¨me complÃƒÂ¨te
     * @returns {string} - HTML de la page systÃƒÂ¨me
     */
    buildTemplate(data) {
        return `
            <div class="system-layout">
                <!-- En-tÃƒÂªte avec statut systÃƒÂ¨me -->
                <div class="system-header">
                    <div class="system-status">
                        <div class="status-indicator ${data.systemHealth === 'good' ? 'status-good' : 'status-warning'}"></div>
                        <div class="status-info">
                            <h1 class="system-title">Configuration SystÃƒÂ¨me</h1>
                            <p class="system-subtitle">SantÃƒÂ©: ${data.systemHealth || 'En cours de vÃƒÂ©rification...'}</p>
                        </div>
                    </div>
                    <div class="system-actions">
                        <button class="btn btn-primary" onclick="app.systemController.exportSettings()">
                            Ã°Å¸â€œâ€ž Exporter Config
                        </button>
                        <button class="btn btn-secondary" onclick="app.systemController.importSettings()">
                            Ã°Å¸â€œÂ Importer Config
                        </button>
                    </div>
                </div>
<!-- Section Statut Backend C++ -->
<div class="system-section">
    <div class="section-header">
        <h3 class="section-title">Ã°Å¸â€Å’ Backend MIDI C++</h3>
        <div class="backend-status-indicator ${data.backendConnected ? 'connected' : 'disconnected'}">
            <span class="status-dot"></span>
            ${data.backendConnected ? 'ConnectÃƒÂ©' : 'DÃƒÂ©connectÃƒÂ©'}
        </div>
    </div>
    <div class="section-content">
        ${this.buildBackendStatus(data.backend)}
    </div>
</div>
                <!-- Grille principale des sections -->
                <div class="system-grid">
                    <!-- Section Audio/MIDI -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">Ã°Å¸Å½Âµ Configuration Audio/MIDI</h3>
                            <button class="btn btn-small btn-success" onclick="app.systemController.autoCalibrate()">
                                Ã¢Å¡Â¡ Auto-Calibrer
                            </button>
                        </div>
                        <div class="section-content">
                            ${this.buildAudioMidiConfig(data.audioConfig)}
                        </div>
                    </div>

                    <!-- Section Instruments & Latence -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">Ã°Å¸Å½Â¼ Instruments & Latence</h3>
                            <span class="latency-status ${data.maxLatency > 50 ? 'high-latency' : 'good-latency'}">
                                Max: ${data.maxLatency || 0}ms
                            </span>
                        </div>
                        <div class="section-content">
                            ${this.buildInstrumentLatencyList(data.instruments)}
                        </div>
                    </div>

                    <!-- Section Visualiseur -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">Ã°Å¸Å½Â¨ Configuration Visualiseur</h3>
                            <select class="preset-select" onchange="app.systemController.applyVisualizerPreset(this.value)">
                                <option value="">SÃƒÂ©lectionner un preset</option>
                                <option value="performance" ${data.visualizerPreset === 'performance' ? 'selected' : ''}>Ã¢Å¡Â¡ Performance</option>
                                <option value="balanced" ${data.visualizerPreset === 'balanced' ? 'selected' : ''}>Ã¢Å¡â€“Ã¯Â¸Â Ãƒâ€°quilibrÃƒÂ©</option>
                                <option value="quality" ${data.visualizerPreset === 'quality' ? 'selected' : ''}>Ã¢Å“Â¨ QualitÃƒÂ©</option>
                                <option value="custom" ${data.visualizerPreset === 'custom' ? 'selected' : ''}>Ã°Å¸â€Â§ PersonnalisÃƒÂ©</option>
                            </select>
                        </div>
                        <div class="section-content">
                            ${this.buildVisualizerConfig(data.visualizerConfig)}
                        </div>
                    </div>

                    <!-- Section Interface & ThÃƒÂ¨me -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">Ã°Å¸Å½Â¨ Interface & ThÃƒÂ¨me</h3>
                            <div class="theme-toggle">
                                <button class="theme-btn ${data.theme === 'light' ? 'active' : ''}" 
                                        onclick="app.systemController.setTheme('light')">Ã¢Ëœâ‚¬Ã¯Â¸Â</button>
                                <button class="theme-btn ${data.theme === 'dark' ? 'active' : ''}" 
                                        onclick="app.systemController.setTheme('dark')">Ã°Å¸Å’â„¢</button>
                            </div>
                        </div>
                        <div class="section-content">
                            ${this.buildInterfaceConfig(data.interfaceConfig)}
                        </div>
                    </div>

                    <!-- Section Statistiques Performance -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">Ã°Å¸â€œÅ  Performance & Statistiques</h3>
                            <button class="btn btn-small" onclick="app.systemController.resetStats()">
                                Ã°Å¸â€â€ž Reset Stats
                            </button>
                        </div>
                        <div class="section-content">
                            <div id="systemStats">
                                ${this.buildSystemStats(data.stats)}
                            </div>
                        </div>
                    </div>

                    <!-- Section AvancÃƒÂ©e -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">Ã¢Å¡â„¢Ã¯Â¸Â Configuration AvancÃƒÂ©e</h3>
                            <div class="advanced-toggle">
                                <label class="toggle-switch">
                                    <input type="checkbox" id="showAdvanced" 
                                           ${data.showAdvanced ? 'checked' : ''}
                                           onchange="app.systemController.toggleAdvancedMode(this.checked)">
                                    <span class="toggle-slider"></span>
                                </label>
                                <span>Mode Expert</span>
                            </div>
                        </div>
                        <div class="section-content ${data.showAdvanced ? 'show' : 'hide'}" id="advancedConfig">
                            ${this.buildAdvancedConfig(data.advancedConfig)}
                        </div>
                    </div>
                </div>
            </div>
        `;
    }

    /**
     * Construit la section de configuration audio/MIDI
     * @param {Object} config - Configuration audio actuelle
     * @returns {string} - HTML de la configuration audio
     */
    buildAudioMidiConfig(config = {}) {
        return `
            <div class="audio-config-grid">
                <!-- ParamÃƒÂ¨tres de base -->
                <div class="config-group">
                    <label class="config-label">Buffer Audio:</label>
                    <select class="config-select" 
                            onchange="app.systemController.updateAudioConfig('bufferSize', parseInt(this.value))">
                        <option value="128" ${config.bufferSize === 128 ? 'selected' : ''}>128 samples (2.9ms)</option>
                        <option value="256" ${config.bufferSize === 256 ? 'selected' : ''}>256 samples (5.8ms)</option>
                        <option value="512" ${config.bufferSize === 512 ? 'selected' : ''}>512 samples (11.6ms)</option>
                        <option value="1024" ${config.bufferSize === 1024 ? 'selected' : ''}>1024 samples (23.2ms)</option>
                    </select>
                    <small class="config-hint">Plus petit = moins de latence, plus de CPU</small>
                </div>

                <div class="config-group">
                    <label class="config-label">FrÃƒÂ©quence d'ÃƒÂ©chantillonnage:</label>
                    <select class="config-select"
                            onchange="app.systemController.updateAudioConfig('sampleRate', parseInt(this.value))">
                        <option value="44100" ${config.sampleRate === 44100 ? 'selected' : ''}>44.1 kHz</option>
                        <option value="48000" ${config.sampleRate === 48000 ? 'selected' : ''}>48 kHz</option>
                        <option value="96000" ${config.sampleRate === 96000 ? 'selected' : ''}>96 kHz</option>
                    </select>
                </div>

                <div class="config-group">
                    <label class="config-label">Latence cible (ms):</label>
                    <input type="range" 
                           min="1" max="50" 
                           value="${config.targetLatency || 10}"
                           oninput="app.systemController.updateAudioConfig('targetLatency', parseInt(this.value)); this.nextElementSibling.textContent = this.value + 'ms'">
                    <span class="config-value">${config.targetLatency || 10}ms</span>
                </div>

                <div class="config-group">
                    <label class="config-label">Compensation automatique:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.autoCompensation ? 'checked' : ''}
                               onchange="app.systemController.updateAudioConfig('autoCompensation', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
            </div>
        `;
    }

    /**
     * Construit la liste des instruments avec leurs latences
     * @param {Array} instruments - Liste des instruments connectÃƒÂ©s
     * @returns {string} - HTML de la liste des instruments
     */
    buildInstrumentLatencyList(instruments = []) {
        if (instruments.length === 0) {
            return `
                <div class="empty-state">
                    <p>Ã°Å¸Å½Â¼ Aucun instrument dÃƒÂ©tectÃƒÂ©</p>
                    <button class="btn btn-primary" onclick="app.instrumentController.detectInstruments()">
                        Ã°Å¸â€Â DÃƒÂ©tecter Instruments
                    </button>
                </div>
            `;
        }

        return `
            <div class="instrument-list">
                ${instruments.map(instrument => `
                    <div class="instrument-item ${instrument.connected ? 'connected' : 'disconnected'}">
                        <div class="instrument-info">
                            <div class="instrument-name">${instrument.name}</div>
                            <div class="instrument-type">${instrument.type}</div>
                            <div class="instrument-status">
                                <span class="status-dot ${instrument.connected ? 'online' : 'offline'}"></span>
                                ${instrument.connected ? 'ConnectÃƒÂ©' : 'DÃƒÂ©connectÃƒÂ©'}
                            </div>
                        </div>
                        <div class="latency-info">
                            <div class="latency-value ${instrument.latency > 20 ? 'high' : 'good'}">
                                ${instrument.latency ? instrument.latency.toFixed(1) : 'Ã¢â‚¬â€œ'}ms
                            </div>
                            <div class="jitter-value">
                                Jitter: ${instrument.jitter ? instrument.jitter.toFixed(1) : 'Ã¢â‚¬â€œ'}ms
                            </div>
                        </div>
                        <div class="instrument-actions">
                            ${instrument.connected ? `
                                <button class="btn btn-small" 
                                        onclick="app.systemController.calibrateInstrument('${instrument.id}')">
                                    Ã°Å¸â€œÅ  Calibrer
                                </button>
                            ` : ''}
                        </div>
                    </div>
                `).join('')}
            </div>
        `;
    }

    /**
     * Construit la configuration du visualiseur
     * @param {Object} config - Configuration visualiseur
     * @returns {string} - HTML de la configuration visualiseur
     */
    buildVisualizerConfig(config = {}) {
        return `
            <div class="visualizer-config">
                <div class="config-row">
                    <label>FPS Cible:</label>
                    <input type="range" min="15" max="120" value="${config.targetFPS || 60}"
                           oninput="app.systemController.updateVisualizerConfig('targetFPS', parseInt(this.value)); this.nextElementSibling.textContent = this.value">
                    <span class="config-value">${config.targetFPS || 60}</span>
                </div>
                
                <div class="config-row">
                    <label>FenÃƒÂªtre temps (s):</label>
                    <input type="range" min="1" max="30" value="${config.timeWindow || 10}"
                           oninput="app.systemController.updateVisualizerConfig('timeWindow', parseInt(this.value)); this.nextElementSibling.textContent = this.value">
                    <span class="config-value">${config.timeWindow || 10}</span>
                </div>
                
                <div class="config-row">
                    <label>Hauteur touches piano:</label>
                    <input type="range" min="10" max="50" value="${config.pianoKeyHeight || 20}"
                           oninput="app.systemController.updateVisualizerConfig('pianoKeyHeight', parseInt(this.value)); this.nextElementSibling.textContent = this.value">
                    <span class="config-value">${config.pianoKeyHeight || 20}</span>
                </div>

                <div class="config-row">
                    <label>Anti-aliasing:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.antiAliasing ? 'checked' : ''}
                               onchange="app.systemController.updateVisualizerConfig('antiAliasing', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                </div>

                <div class="config-row">
                    <label>Effets visuels:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.visualEffects ? 'checked' : ''}
                               onchange="app.systemController.updateVisualizerConfig('visualEffects', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
            </div>
        `;
    }

    /**
     * Construit la configuration de l'interface
     * @param {Object} config - Configuration interface
     * @returns {string} - HTML de la configuration interface
     */
    buildInterfaceConfig(config = {}) {
        return `
            <div class="interface-config">
                <div class="config-row">
                    <label>Animations Interface:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.animations ? 'checked' : ''}
                               onchange="app.systemController.updateInterfaceConfig('animations', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                </div>

                <div class="config-row">
                    <label>Notifications sonores:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.soundNotifications ? 'checked' : ''}
                               onchange="app.systemController.updateInterfaceConfig('soundNotifications', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                </div>

                <div class="config-row">
                    <label>Conseils d'aide:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.showTooltips ? 'checked' : ''}
                               onchange="app.systemController.updateInterfaceConfig('showTooltips', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                </div>

                <div class="config-row">
                    <label>Raccourcis clavier:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.keyboardShortcuts ? 'checked' : ''}
                               onchange="app.systemController.updateInterfaceConfig('keyboardShortcuts', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
            </div>
        `;
    }

    /**
     * Construit les statistiques systÃƒÂ¨me
     * @param {Object} stats - Statistiques systÃƒÂ¨me
     * @returns {string} - HTML des statistiques
     */
    buildSystemStats(stats = {}) {
        return `
            <div class="stats-grid">
                <div class="stat-item">
                    <div class="stat-label">CPU Usage</div>
                    <div class="stat-value">${stats.cpuUsage || 0}%</div>
                    <div class="stat-bar">
                        <div class="stat-fill" style="width: ${stats.cpuUsage || 0}%"></div>
                    </div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">MÃƒÂ©moire</div>
                    <div class="stat-value">${stats.memoryUsage || 0}MB</div>
                    <div class="stat-bar">
                        <div class="stat-fill" style="width: ${(stats.memoryUsage || 0) / 1024 * 100}%"></div>
                    </div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">FPS Moyen</div>
                    <div class="stat-value">${stats.avgFPS || 0}</div>
                    <div class="stat-color ${stats.avgFPS > 30 ? 'good' : 'warning'}"></div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">Messages MIDI/s</div>
                    <div class="stat-value">${stats.midiMessagesPerSecond || 0}</div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">Uptime</div>
                    <div class="stat-value">${this.formatUptime(stats.uptime || 0)}</div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">Fichiers chargÃƒÂ©s</div>
                    <div class="stat-value">${stats.filesLoaded || 0}</div>
                </div>
            </div>
        `;
    }

    /**
     * Construit la configuration avancÃƒÂ©e
     * @param {Object} config - Configuration avancÃƒÂ©e
     * @returns {string} - HTML de la configuration avancÃƒÂ©e
     */
    buildAdvancedConfig(config = {}) {
        return `
            <div class="advanced-config">
                <div class="config-group">
                    <h4>Ã°Å¸â€Â§ Mode Debug</h4>
                    <div class="config-row">
                        <label>Logging dÃƒÂ©taillÃƒÂ©:</label>
                        <label class="toggle-switch">
                            <input type="checkbox" 
                                   ${config.verboseLogging ? 'checked' : ''}
                                   onchange="app.systemController.updateAdvancedConfig('verboseLogging', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                    <div class="config-row">
                        <label>MÃƒÂ©triques temps rÃƒÂ©el:</label>
                        <label class="toggle-switch">
                            <input type="checkbox" 
                                   ${config.realtimeMetrics ? 'checked' : ''}
                                   onchange="app.systemController.updateAdvancedConfig('realtimeMetrics', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                </div>

                <div class="config-group">
                    <h4>Ã¢Å¡Â¡ Optimisations</h4>
                    <div class="config-row">
                        <label>PrÃƒÂ©diction cache:</label>
                        <label class="toggle-switch">
                            <input type="checkbox" 
                                   ${config.predictiveCache ? 'checked' : ''}
                                   onchange="app.systemController.updateAdvancedConfig('predictiveCache', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                    <div class="config-row">
                        <label>Compression donnÃƒÂ©es:</label>
                        <label class="toggle-switch">
                            <input type="checkbox" 
                                   ${config.dataCompression ? 'checked' : ''}
                                   onchange="app.systemController.updateAdvancedConfig('dataCompression', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                </div>

                <div class="config-group">
                    <h4>Ã°Å¸â€ºÂ¡Ã¯Â¸Â SÃƒÂ©curitÃƒÂ©</h4>
                    <div class="config-row">
                        <label>Validation stricte MIDI:</label>
                        <label class="toggle-switch">
                            <input type="checkbox" 
                                   ${config.strictMidiValidation ? 'checked' : ''}
                                   onchange="app.systemController.updateAdvancedConfig('strictMidiValidation', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                </div>
            </div>
        `;
    }

    /**
     * Formate le temps d'uptime en format lisible
     * @param {number} uptime - Uptime en secondes
     * @returns {string} - Uptime formatÃƒÂ©
     */
    formatUptime(uptime) {
        const hours = Math.floor(uptime / 3600);
        const minutes = Math.floor((uptime % 3600) / 60);
        const seconds = Math.floor(uptime % 60);
        
        if (hours > 0) {
            return `${hours}h ${minutes}m`;
        } else if (minutes > 0) {
            return `${minutes}m ${seconds}s`;
        } else {
            return `${seconds}s`;
        }
    }

    /**
     * Met ÃƒÂ  jour l'affichage des statistiques en temps rÃƒÂ©el
     */
    startStatsUpdate() {
        if (this.statsUpdateInterval) {
            clearInterval(this.statsUpdateInterval);
        }

        this.statsUpdateInterval = setInterval(() => {
            const statsContainer = document.getElementById('systemStats');
            if (statsContainer && document.getElementById('system-page').classList.contains('active')) {
                // Ãƒâ€°mettre une demande de mise ÃƒÂ  jour des stats
                this.eventBus.emit('system:request_stats_update');
            }
        }, 1000);
    }

    /**
     * ArrÃƒÂªte la mise ÃƒÂ  jour des statistiques
     */
    stopStatsUpdate() {
        if (this.statsUpdateInterval) {
            clearInterval(this.statsUpdateInterval);
            this.statsUpdateInterval = null;
        }
    }

    /**
     * Nettoie les ressources de la vue
     */
    destroy() {
        this.stopStatsUpdate();
        super.destroy();
    }
	/**
 * Construit l'affichage du statut backend
 * @param {Object} backendData - DonnÃƒÂ©es du backend
 * @returns {string} - HTML du statut backend
 */
function buildBackendStatus(backendData = {}) {
    if (!backendData.connected) {
        return `
            <div class="backend-disconnected">
                <div class="warning-message">
                    Ã¢Å¡Â Ã¯Â¸Â Backend C++ non connectÃƒÂ©
                </div>
                <p class="help-text">
                    Le backend MIDI n'est pas accessible. VÃƒÂ©rifiez que :
                </p>
                <ul class="help-list">
                    <li>Le serveur C++ est dÃƒÂ©marrÃƒÂ© sur le port 8080</li>
                    <li>L'URL WebSocket est correcte (ws://localhost:8080)</li>
                    <li>Aucun pare-feu ne bloque la connexion</li>
                </ul>
                <button class="btn btn-primary" onclick="app.systemController.reconnectBackend()">
                    Ã°Å¸â€â€ž Tenter reconnexion
                </button>
            </div>
        `;
    }
    
    return `
        <div class="backend-connected">
            <div class="success-message">
                Ã¢Å“â€¦ Backend opÃƒÂ©rationnel
            </div>
            
            <div class="backend-info-grid">
                <div class="info-item">
                    <div class="info-label">URL WebSocket</div>
                    <div class="info-value">${backendData.url || 'ws://localhost:8080'}</div>
                </div>
                
                <div class="info-item">
                    <div class="info-label">Ãƒâ€°tat</div>
                    <div class="info-value status-playing">
                        ${backendData.isPlaying ? 'Ã¢â€“Â¶Ã¯Â¸Â En lecture' : 'Ã¢ÂÂ¸Ã¯Â¸Â En pause'}
                    </div>
                </div>
                
                <div class="info-item">
                    <div class="info-label">Position</div>
                    <div class="info-value">${backendData.position?.toFixed(2) || '0.00'}s</div>
                </div>
                
                <div class="info-item">
                    <div class="info-label">Tempo</div>
                    <div class="info-value">${backendData.tempo || 120} BPM</div>
                </div>
                
                <div class="info-item">
                    <div class="info-label">Fichier actuel</div>
                    <div class="info-value">${backendData.currentFile || 'Aucun'}</div>
                </div>
                
                <div class="info-item">
                    <div class="info-label">Messages en attente</div>
                    <div class="info-value">${backendData.queuedCommands || 0}</div>
                </div>
            </div>
            
            <div class="backend-actions">
                <button class="btn btn-secondary" onclick="app.systemController.testBackendConnection()">
                    Ã°Å¸â€Â Tester connexion
                </button>
                <button class="btn btn-secondary" onclick="app.systemController.clearBackendQueue()">
                    Ã°Å¸â€”â€˜Ã¯Â¸Â Vider file d'attente
                </button>
            </div>
        </div>
    `;
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = SystemView;
}

if (typeof window !== 'undefined') {
    window.SystemView = SystemView;  // â† AJOUTÃ‰
}
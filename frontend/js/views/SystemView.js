// ===== SYSTEM VIEW - Vue de configuration système =====
// =====================================================
// Gère l'affichage de toutes les configurations système :
// - Paramètres audio/MIDI (latence, buffer, sample rate)
// - Calibration automatique des instruments
// - Présets de visualiseur (performance, qualité, équilibré)
// - Statistiques en temps réel du système
// - Configuration des thèmes et interface
// - Monitoring des performances
// =====================================================

class SystemView extends BaseView {
    constructor(eventBus) {
        super('system-page', eventBus);
        
        // Initialize logger first
        this.logger = window.logger || console;
        
        // État de la vue
        this.calibrationInProgress = false;
        this.statsUpdateInterval = null;
        this.currentTheme = 'light';
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        // Log initialization
        if (this.logger && this.logger.info) {
            this.logger.info('SystemView', '✓ SystemView initialized');
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
     * Construit le template principal de la page système
     * @param {Object} data - Configuration système complète
     * @returns {string} - HTML de la page système
     */
    buildTemplate(data) {
        return `
            <div class="system-layout">
                <!-- En-tête avec statut système -->
                <div class="system-header">
                    <div class="system-status">
                        <div class="status-indicator ${data.systemHealth === 'good' ? 'status-good' : 'status-warning'}"></div>
                        <div class="status-info">
                            <h1 class="system-title">Configuration Système</h1>
                            <p class="system-subtitle">Santé: ${data.systemHealth || 'En cours de vérification...'}</p>
                        </div>
                    </div>
                    <div class="system-actions">
                        <button class="btn btn-primary" onclick="app.systemController.exportSettings()">
                            📄 Exporter Config
                        </button>
                        <button class="btn btn-secondary" onclick="app.systemController.importSettings()">
                            📁 Importer Config
                        </button>
                    </div>
                </div>
<!-- Section Statut Backend C++ -->
<div class="system-section">
    <div class="section-header">
        <h3 class="section-title">🔌 Backend MIDI C++</h3>
        <div class="backend-status-indicator ${data.backendConnected ? 'connected' : 'disconnected'}">
            <span class="status-dot"></span>
            ${data.backendConnected ? 'Connecté' : 'Déconnecté'}
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
                            <h3 class="section-title">🎵 Configuration Audio/MIDI</h3>
                            <button class="btn btn-small btn-success" onclick="app.systemController.autoCalibrate()">
                                ⚡ Auto-Calibrer
                            </button>
                        </div>
                        <div class="section-content">
                            ${this.buildAudioMidiConfig(data.audioConfig)}
                        </div>
                    </div>

                    <!-- Section Instruments & Latence -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">🎼 Instruments & Latence</h3>
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
                            <h3 class="section-title">🎨 Configuration Visualiseur</h3>
                            <select class="preset-select" onchange="app.systemController.applyVisualizerPreset(this.value)">
                                <option value="">Sélectionner un preset</option>
                                <option value="performance" ${data.visualizerPreset === 'performance' ? 'selected' : ''}>⚡ Performance</option>
                                <option value="balanced" ${data.visualizerPreset === 'balanced' ? 'selected' : ''}>⚖️ Équilibré</option>
                                <option value="quality" ${data.visualizerPreset === 'quality' ? 'selected' : ''}>✨ Qualité</option>
                                <option value="custom" ${data.visualizerPreset === 'custom' ? 'selected' : ''}>🔧 Personnalisé</option>
                            </select>
                        </div>
                        <div class="section-content">
                            ${this.buildVisualizerConfig(data.visualizerConfig)}
                        </div>
                    </div>

                    <!-- Section Interface & Thème -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">🎨 Interface & Thème</h3>
                            <div class="theme-toggle">
                                <button class="theme-btn ${data.theme === 'light' ? 'active' : ''}" 
                                        onclick="app.systemController.setTheme('light')">☀️</button>
                                <button class="theme-btn ${data.theme === 'dark' ? 'active' : ''}" 
                                        onclick="app.systemController.setTheme('dark')">🌙</button>
                            </div>
                        </div>
                        <div class="section-content">
                            ${this.buildInterfaceConfig(data.interfaceConfig)}
                        </div>
                    </div>

                    <!-- Section Statistiques Performance -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">📊 Performance & Statistiques</h3>
                            <button class="btn btn-small" onclick="app.systemController.resetStats()">
                                🔄 Reset Stats
                            </button>
                        </div>
                        <div class="section-content">
                            <div id="systemStats">
                                ${this.buildSystemStats(data.stats)}
                            </div>
                        </div>
                    </div>

                    <!-- Section Avancée -->
                    <div class="system-section">
                        <div class="section-header">
                            <h3 class="section-title">⚙️ Configuration Avancée</h3>
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
     * Construit la configuration audio/MIDI
     * @param {Object} config - Configuration audio
     * @returns {string} - HTML de la configuration
     */
    buildAudioMidiConfig(config = {}) {
        return `
            <div class="config-grid">
                <div class="config-item">
                    <label for="bufferSize">Taille buffer:</label>
                    <select id="bufferSize" 
                            onchange="app.systemController.updateAudioConfig('bufferSize', parseInt(this.value))">
                        <option value="128" ${config.bufferSize === 128 ? 'selected' : ''}>128 samples</option>
                        <option value="256" ${config.bufferSize === 256 ? 'selected' : ''}>256 samples</option>
                        <option value="512" ${config.bufferSize === 512 ? 'selected' : ''}>512 samples</option>
                        <option value="1024" ${config.bufferSize === 1024 ? 'selected' : ''}>1024 samples</option>
                    </select>
                    <span class="config-help">⚡ Plus petit = moins de latence</span>
                </div>

                <div class="config-item">
                    <label for="sampleRate">Sample rate:</label>
                    <select id="sampleRate" 
                            onchange="app.systemController.updateAudioConfig('sampleRate', parseInt(this.value))">
                        <option value="44100" ${config.sampleRate === 44100 ? 'selected' : ''}>44100 Hz</option>
                        <option value="48000" ${config.sampleRate === 48000 ? 'selected' : ''}>48000 Hz</option>
                        <option value="96000" ${config.sampleRate === 96000 ? 'selected' : ''}>96000 Hz</option>
                    </select>
                    <span class="config-help">🎵 Qualité audio</span>
                </div>

                <div class="config-item">
                    <label for="targetLatency">Latence cible:</label>
                    <input type="number" 
                           id="targetLatency" 
                           value="${config.targetLatency || 10}" 
                           min="1" 
                           max="100"
                           onchange="app.systemController.updateAudioConfig('targetLatency', parseInt(this.value))">
                    <span class="config-help">ms</span>
                </div>

                <div class="config-item">
                    <label>Compensation auto:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.autoCompensation ? 'checked' : ''}
                               onchange="app.systemController.updateAudioConfig('autoCompensation', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                    <span class="config-help">🎯 Ajuste automatiquement</span>
                </div>
            </div>
        `;
    }

    /**
     * Construit la liste des instruments avec latences
     * @param {Array} instruments - Liste des instruments
     * @returns {string} - HTML de la liste
     */
    buildInstrumentLatencyList(instruments = []) {
        if (!instruments || instruments.length === 0) {
            return `
                <div class="empty-state">
                    <p>Aucun instrument connecté</p>
                </div>
            `;
        }

        return `
            <div class="instrument-latency-list">
                ${instruments.map(inst => `
                    <div class="instrument-latency-item">
                        <div class="instrument-info">
                            <span class="instrument-name">${inst.name || 'Instrument'}</span>
                            <span class="instrument-type">${inst.type || 'MIDI'}</span>
                        </div>
                        <div class="latency-info">
                            <span class="latency-value ${inst.latency > 50 ? 'high' : 'good'}">
                                ${inst.latency || 0}ms
                            </span>
                            <button class="btn btn-mini" 
                                    onclick="app.systemController.calibrateInstrument('${inst.id}')">
                                ⚡ Calibrer
                            </button>
                        </div>
                    </div>
                `).join('')}
            </div>
        `;
    }

    /**
     * Construit la configuration du visualiseur
     * @param {Object} config - Configuration visualiseur
     * @returns {string} - HTML de la configuration
     */
    buildVisualizerConfig(config = {}) {
        return `
            <div class="config-grid">
                <div class="config-item">
                    <label for="targetFPS">FPS cible:</label>
                    <input type="number" 
                           id="targetFPS" 
                           value="${config.targetFPS || 60}" 
                           min="15" 
                           max="120"
                           onchange="app.systemController.updateVisualizerConfig('targetFPS', parseInt(this.value))">
                    <span class="config-help">images/sec</span>
                </div>

                <div class="config-item">
                    <label for="timeWindow">Fenêtre temps:</label>
                    <input type="number" 
                           id="timeWindow" 
                           value="${config.timeWindow || 10}" 
                           min="1" 
                           max="30"
                           onchange="app.systemController.updateVisualizerConfig('timeWindow', parseInt(this.value))">
                    <span class="config-help">secondes</span>
                </div>

                <div class="config-item">
                    <label for="pianoKeyHeight">Hauteur touches:</label>
                    <input type="number" 
                           id="pianoKeyHeight" 
                           value="${config.pianoKeyHeight || 20}" 
                           min="10" 
                           max="50"
                           onchange="app.systemController.updateVisualizerConfig('pianoKeyHeight', parseInt(this.value))">
                    <span class="config-help">pixels</span>
                </div>

                <div class="config-item">
                    <label>Anti-aliasing:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.antiAliasing ? 'checked' : ''}
                               onchange="app.systemController.updateVisualizerConfig('antiAliasing', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                    <span class="config-help">✨ Lissage</span>
                </div>

                <div class="config-item">
                    <label>Effets visuels:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.visualEffects ? 'checked' : ''}
                               onchange="app.systemController.updateVisualizerConfig('visualEffects', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                    <span class="config-help">🎨 Animations</span>
                </div>
            </div>
        `;
    }

    /**
     * Construit la configuration de l'interface
     * @param {Object} config - Configuration interface
     * @returns {string} - HTML de la configuration
     */
    buildInterfaceConfig(config = {}) {
        return `
            <div class="config-grid">
                <div class="config-item">
                    <label>Animations:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.animations ? 'checked' : ''}
                               onchange="app.systemController.updateInterfaceConfig('animations', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                    <span class="config-help">🎬 Transitions</span>
                </div>

                <div class="config-item">
                    <label>Notifications sonores:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.soundNotifications ? 'checked' : ''}
                               onchange="app.systemController.updateInterfaceConfig('soundNotifications', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                    <span class="config-help">🔔 Sons</span>
                </div>

                <div class="config-item">
                    <label>Info-bulles:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.showTooltips ? 'checked' : ''}
                               onchange="app.systemController.updateInterfaceConfig('showTooltips', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                    <span class="config-help">💬 Aide</span>
                </div>

                <div class="config-item">
                    <label>Raccourcis clavier:</label>
                    <label class="toggle-switch">
                        <input type="checkbox" 
                               ${config.keyboardShortcuts ? 'checked' : ''}
                               onchange="app.systemController.updateInterfaceConfig('keyboardShortcuts', this.checked)">
                        <span class="toggle-slider"></span>
                    </label>
                    <span class="config-help">⌨️ Shortcuts</span>
                </div>
            </div>
        `;
    }

    /**
     * Construit l'affichage des statistiques système
     * @param {Object} stats - Statistiques système
     * @returns {string} - HTML des statistiques
     */
    buildSystemStats(stats = {}) {
        return `
            <div class="stats-grid">
                <div class="stat-item">
                    <div class="stat-label">CPU</div>
                    <div class="stat-value">${stats.cpuUsage || 0}%</div>
                    <div class="stat-bar">
                        <div class="stat-bar-fill" style="width: ${stats.cpuUsage || 0}%"></div>
                    </div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">Mémoire</div>
                    <div class="stat-value">${stats.memoryUsage || 0}%</div>
                    <div class="stat-bar">
                        <div class="stat-bar-fill" style="width: ${stats.memoryUsage || 0}%"></div>
                    </div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">FPS</div>
                    <div class="stat-value">${stats.currentFPS || 0}</div>
                    <div class="stat-info">Target: ${stats.targetFPS || 60}</div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">Latence moyenne</div>
                    <div class="stat-value">${stats.avgLatency || 0}ms</div>
                    <div class="stat-info">Max: ${stats.maxLatency || 0}ms</div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">Messages MIDI</div>
                    <div class="stat-value">${stats.midiMessagesProcessed || 0}</div>
                    <div class="stat-info">/sec</div>
                </div>

                <div class="stat-item">
                    <div class="stat-label">Uptime</div>
                    <div class="stat-value">${this.formatUptime(stats.uptime || 0)}</div>
                </div>
            </div>
        `;
    }

    /**
     * Construit la configuration avancée
     * @param {Object} config - Configuration avancée
     * @returns {string} - HTML de la configuration
     */
    buildAdvancedConfig(config = {}) {
        return `
            <div class="advanced-config">
                <div class="config-group">
                    <h4>🔍 Debug & Logs</h4>
                    <div class="config-row">
                        <label>Logging verbeux:</label>
                        <label class="toggle-switch">
                            <input type="checkbox" 
                                   ${config.verboseLogging ? 'checked' : ''}
                                   onchange="app.systemController.updateAdvancedConfig('verboseLogging', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                    <div class="config-row">
                        <label>Métriques temps réel:</label>
                        <label class="toggle-switch">
                            <input type="checkbox" 
                                   ${config.realtimeMetrics ? 'checked' : ''}
                                   onchange="app.systemController.updateAdvancedConfig('realtimeMetrics', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                </div>

                <div class="config-group">
                    <h4>⚡ Performance</h4>
                    <div class="config-row">
                        <label>Prédiction cache:</label>
                        <label class="toggle-switch">
                            <input type="checkbox" 
                                   ${config.predictiveCache ? 'checked' : ''}
                                   onchange="app.systemController.updateAdvancedConfig('predictiveCache', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                    <div class="config-row">
                        <label>Compression données:</label>
                        <label class="toggle-switch">
                            <input type="checkbox" 
                                   ${config.dataCompression ? 'checked' : ''}
                                   onchange="app.systemController.updateAdvancedConfig('dataCompression', this.checked)">
                            <span class="toggle-slider"></span>
                        </label>
                    </div>
                </div>

                <div class="config-group">
                    <h4>🛡️ Sécurité</h4>
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
     * @returns {string} - Uptime formaté
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
     * Met à jour l'affichage des statistiques en temps réel
     */
    startStatsUpdate() {
        if (this.statsUpdateInterval) {
            clearInterval(this.statsUpdateInterval);
        }

        this.statsUpdateInterval = setInterval(() => {
            const statsContainer = document.getElementById('systemStats');
            if (statsContainer && document.getElementById('system-page').classList.contains('active')) {
                // Émettre une demande de mise à jour des stats
                this.eventBus.emit('system:request_stats_update');
            }
        }, 1000);
    }

    /**
     * Arrête la mise à jour des statistiques
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
 * @param {Object} backendData - Données du backend
 * @returns {string} - HTML du statut backend
 */
buildBackendStatus(backendData = {}) {
    if (!backendData.connected) {
        return `
            <div class="backend-disconnected">
                <div class="warning-message">
                    ⚠️ Backend C++ non connecté
                </div>
                <p class="help-text">
                    Le backend MIDI n'est pas accessible. Vérifiez que :
                </p>
                <ul class="help-list">
                    <li>Le serveur C++ est démarré sur le port 8080</li>
                    <li>L'URL WebSocket est correcte (voir configuration)</li>
                    <li>Aucun pare-feu ne bloque la connexion</li>
                </ul>
                <button class="btn btn-primary" onclick="app.systemController.reconnectBackend()">
                    🔄 Tenter reconnexion
                </button>
            </div>
        `;
    }
    
    return `
        <div class="backend-connected">
            <div class="success-message">
                ✅ Backend opérationnel
            </div>
            
            <div class="backend-info-grid">
                <div class="info-item">
                    <div class="info-label">URL WebSocket</div>
                    <div class="info-value">${backendData.url || 'ws://localhost:8080'}</div>
                </div>
                
                <div class="info-item">
                    <div class="info-label">État</div>
                    <div class="info-value status-playing">
                        ${backendData.isPlaying ? '▶️ En lecture' : '⏸️ En pause'}
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
                    🔍 Tester connexion
                </button>
                <button class="btn btn-secondary" onclick="app.systemController.clearBackendQueue()">
                    🗑️ Vider file d'attente
                </button>
            </div>
        </div>
    `;
}
}
// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = SystemView;
}

if (typeof window !== 'undefined') {
    window.SystemView = SystemView;
}
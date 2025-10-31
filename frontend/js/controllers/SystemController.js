// ============================================================================
// Fichier: frontend/js/controllers/SystemController.js
// Version: 4.2.1 - API BACKEND FULL COMPATIBILITY
// Date: 2025-10-28
// ============================================================================
// Modifications:
//   - Support system.uptime, system.memory, system.disk
//   - Monitoring temps réel des ressources système
//   - Gestion format API v4.2.1 (request/response standardisé)
//   - Statistiques système enrichies
//   - Support logger.setLevel / logger.getLevel
//   - Intégration codes erreur API
// ============================================================================

class SystemController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.logger = window.logger || console;
        this.model = models.system || models.state;
        this.view = views.system;
        this.backend = window.app?.services?.backend || window.backendService;
        
        // Configuration par défaut
        this.defaultConfig = {
            audioConfig: { 
                bufferSize: 256, 
                sampleRate: 44100, 
                targetLatency: 10, 
                autoCompensation: true 
            },
            visualizerConfig: { 
                targetFPS: 60, 
                timeWindow: 10, 
                pianoKeyHeight: 20, 
                antiAliasing: true, 
                visualEffects: true 
            },
            interfaceConfig: { 
                theme: 'light', 
                animations: true, 
                soundNotifications: false, 
                showTooltips: true, 
                keyboardShortcuts: true 
            },
            advancedConfig: { 
                verboseLogging: false, 
                realtimeMetrics: true, 
                predictiveCache: true, 
                dataCompression: true, 
                strictMidiValidation: true 
            }
        };
        
        // État système
        this.systemHealth = 'good';
        this.offlineMode = false;
        this.reconnectButtonCreated = false;
        this.connectionMonitorTimer = null;
        this.statsMonitorTimer = null;
        
        // Statistiques système temps réel
        this.systemStats = {
            uptime: { days: 0, hours: 0, minutes: 0, seconds: 0, total_seconds: 0 },
            memory: { 
                total_mb: 0, 
                used_mb: 0, 
                free_mb: 0, 
                available_mb: 0,
                percent: 0,
                swap_total_mb: 0,
                swap_used_mb: 0,
                swap_percent: 0
            },
            disk: { 
                total_gb: 0, 
                used_gb: 0, 
                free_gb: 0, 
                percent: 0,
                mount_point: '/'
            },
            version: { version: 'unknown', name: 'MidiMind' },
            info: {},
            lastUpdate: null
        };
        
        // Info backend
        this.backendInfo = {
            version: null,
            commands: [],
            capabilities: []
        };
        
        // Logger backend
        this.backendLogLevel = 'INFO';
        
        this._fullyInitialized = true;
        this.initializeSystemConfig();
        this.startConnectionMonitor();
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    bindEvents() {
        // Instruments
        this.eventBus.on('instrument:connected', () => { 
            this.updateInstrumentLatencies(); 
            this.refreshSystemView(); 
        });
        this.eventBus.on('instrument:disconnected', () => { 
            this.updateInstrumentLatencies(); 
            this.refreshSystemView(); 
        });
        
        // Système
        this.eventBus.on('system:request_stats_update', () => this.updateSystemStats());
        
        // Navigation
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'system') this.onSystemPageActive();
            else this.onSystemPageInactive();
        });
        
        // Backend
        this.eventBus.on('backend:offline-mode', (data) => this.handleOfflineMode(data));
        this.eventBus.on('backend:connected', (data) => this.handleBackendConnected(data));
        this.eventBus.on('backend:disconnected', (data) => this.handleBackendDisconnected(data));
        
        if (this.logger?.info) {
            this.logger.info('SystemController', '✓ Events bound');
        }
    }

    initializeSystemConfig() {
        if (this.logger?.info) {
            this.logger.info('SystemController', 'Initializing system config...');
        }
        
        const config = this.loadConfig() || this.defaultConfig;
        
        if (this.model && typeof this.model.set === 'function') {
            this.model.set('systemConfig', config);
        }
        
        this.bindEvents();
        
        if (this.logger?.info) {
            this.logger.info('SystemController', '✓ System config initialized');
        }
    }

    loadConfig() {
        try {
            const saved = localStorage.getItem('midimind_system_config');
            return saved ? JSON.parse(saved) : null;
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'Failed to load config:', error);
            }
            return null;
        }
    }

    saveConfig(config) {
        try {
            localStorage.setItem('midimind_system_config', JSON.stringify(config));
            if (this.logger?.info) {
                this.logger.info('SystemController', '✓ Config saved');
            }
            return true;
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'Failed to save config:', error);
            }
            return false;
        }
    }

    // ========================================================================
    // BACKEND API - NOUVELLES COMMANDES
    // ========================================================================

    /**
     * Obtient le temps de fonctionnement du système
     * @returns {Promise<Object>}
     */
    async getSystemUptime() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('system.uptime', {});
            
            if (response.success) {
                this.systemStats.uptime = response.data;
                this.systemStats.lastUpdate = Date.now();
                return response.data;
            } else {
                throw new Error(response.error_message || 'Failed to get uptime');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'getSystemUptime failed:', error);
            }
            throw error;
        }
    }

    /**
     * Obtient l'utilisation mémoire
     * @returns {Promise<Object>}
     */
    async getSystemMemory() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('system.memory', {});
            
            if (response.success) {
                this.systemStats.memory = response.data;
                this.systemStats.lastUpdate = Date.now();
                return response.data;
            } else {
                throw new Error(response.error_message || 'Failed to get memory info');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'getSystemMemory failed:', error);
            }
            throw error;
        }
    }

    /**
     * Obtient l'utilisation disque
     * @returns {Promise<Object>}
     */
    async getSystemDisk() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('system.disk', {});
            
            if (response.success) {
                this.systemStats.disk = response.data;
                this.systemStats.lastUpdate = Date.now();
                return response.data;
            } else {
                throw new Error(response.error_message || 'Failed to get disk info');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'getSystemDisk failed:', error);
            }
            throw error;
        }
    }

    /**
     * Obtient la version du backend
     * @returns {Promise<Object>}
     */
    async getSystemVersion() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('system.version', {});
            
            if (response.success) {
                this.systemStats.version = response.data;
                this.backendInfo.version = response.data.version;
                return response.data;
            } else {
                throw new Error(response.error_message || 'Failed to get version');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'getSystemVersion failed:', error);
            }
            throw error;
        }
    }

    /**
     * Obtient les infos système générales
     * @returns {Promise<Object>}
     */
    async getSystemInfo() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('system.info', {});
            
            if (response.success) {
                this.systemStats.info = response.data;
                return response.data;
            } else {
                throw new Error(response.error_message || 'Failed to get system info');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'getSystemInfo failed:', error);
            }
            throw error;
        }
    }

    /**
     * Change le niveau de log du backend
     * @param {string} level - Niveau: DEBUG, INFO, WARNING, ERROR
     * @returns {Promise<boolean>}
     */
    async setLoggerLevel(level) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        const validLevels = ['DEBUG', 'INFO', 'WARNING', 'ERROR'];
        if (!validLevels.includes(level)) {
            throw new Error(`Invalid log level: ${level}`);
        }

        try {
            const response = await this.backend.sendCommand('logger.setLevel', { level });
            
            if (response.success) {
                this.backendLogLevel = level;
                
                if (this.logger?.info) {
                    this.logger.info('SystemController', `Logger level set to: ${level}`);
                }
                
                return true;
            } else {
                throw new Error(response.error_message || 'Failed to set logger level');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'setLoggerLevel failed:', error);
            }
            throw error;
        }
    }

    /**
     * Obtient le niveau de log actuel
     * @returns {Promise<string>}
     */
    async getLoggerLevel() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('logger.getLevel', {});
            
            if (response.success) {
                this.backendLogLevel = response.data.level;
                return response.data.level;
            } else {
                throw new Error(response.error_message || 'Failed to get logger level');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'getLoggerLevel failed:', error);
            }
            throw error;
        }
    }

    // ========================================================================
    // CONFIGURATION SYSTÈME
    // ========================================================================

    /**
     * Met à jour la configuration audio
     */
    async updateAudioConfig(config) {
        const currentConfig = this.model.get('systemConfig') || this.defaultConfig;
        currentConfig.audioConfig = { ...currentConfig.audioConfig, ...config };
        
        this.model.set('systemConfig', currentConfig);
        this.saveConfig(currentConfig);
        
        this.eventBus.emit('system:audio_config_changed', config);
        
        if (this.logger?.info) {
            this.logger.info('SystemController', 'Audio config updated:', config);
        }
    }

    /**
     * Met à jour la configuration du visualizer
     */
    async updateVisualizerConfig(config) {
        const currentConfig = this.model.get('systemConfig') || this.defaultConfig;
        currentConfig.visualizerConfig = { ...currentConfig.visualizerConfig, ...config };
        
        this.model.set('systemConfig', currentConfig);
        this.saveConfig(currentConfig);
        
        this.eventBus.emit('system:visualizer_config_changed', config);
        
        if (this.logger?.info) {
            this.logger.info('SystemController', 'Visualizer config updated:', config);
        }
    }

    /**
     * Met à jour la configuration de l'interface
     */
    async updateInterfaceConfig(config) {
        const currentConfig = this.model.get('systemConfig') || this.defaultConfig;
        currentConfig.interfaceConfig = { ...currentConfig.interfaceConfig, ...config };
        
        this.model.set('systemConfig', currentConfig);
        this.saveConfig(currentConfig);
        
        // Appliquer le thème si changé
        if (config.theme) {
            this.applyTheme(config.theme);
        }
        
        this.eventBus.emit('system:interface_config_changed', config);
        
        if (this.logger?.info) {
            this.logger.info('SystemController', 'Interface config updated:', config);
        }
    }

    /**
     * Met à jour la configuration avancée
     */
    async updateAdvancedConfig(config) {
        const currentConfig = this.model.get('systemConfig') || this.defaultConfig;
        currentConfig.advancedConfig = { ...currentConfig.advancedConfig, ...config };
        
        this.model.set('systemConfig', currentConfig);
        this.saveConfig(currentConfig);
        
        this.eventBus.emit('system:advanced_config_changed', config);
        
        if (this.logger?.info) {
            this.logger.info('SystemController', 'Advanced config updated:', config);
        }
    }

    /**
     * Applique un thème
     */
    applyTheme(theme) {
        document.documentElement.setAttribute('data-theme', theme);
        
        if (this.logger?.info) {
            this.logger.info('SystemController', `Theme applied: ${theme}`);
        }
    }

    /**
     * Réinitialise la configuration
     */
    async resetConfiguration() {
        this.model.set('systemConfig', this.defaultConfig);
        this.saveConfig(this.defaultConfig);
        
        this.eventBus.emit('system:config_reset');
        
        if (this.logger?.info) {
            this.logger.info('SystemController', 'Configuration reset to defaults');
        }
    }

    // ========================================================================
    // MISE À JOUR STATS SYSTÈME
    // ========================================================================

    /**
     * Met à jour toutes les statistiques système
     */
    async updateSystemStats() {
        if (!this.backend || !this.backend.isConnected()) {
            if (this.logger?.debug) {
                this.logger.debug('SystemController', 'Backend not connected, skipping stats update');
            }
            return;
        }

        try {
            await Promise.all([
                this.getSystemUptime().catch(e => {
                    if (this.logger?.debug) this.logger.debug('SystemController', 'Uptime fetch failed:', e);
                }),
                this.getSystemMemory().catch(e => {
                    if (this.logger?.debug) this.logger.debug('SystemController', 'Memory fetch failed:', e);
                }),
                this.getSystemDisk().catch(e => {
                    if (this.logger?.debug) this.logger.debug('SystemController', 'Disk fetch failed:', e);
                }),
                this.getSystemVersion().catch(e => {
                    if (this.logger?.debug) this.logger.debug('SystemController', 'Version fetch failed:', e);
                })
            ]);

            this.eventBus.emit('system:stats_updated', this.systemStats);
            
            if (this.logger?.debug) {
                this.logger.debug('SystemController', 'System stats updated');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'Failed to update system stats:', error);
            }
        }
    }

    /**
     * Démarre le monitoring périodique
     */
    startStatsMonitoring(interval = 5000) {
        this.stopStatsMonitoring();
        
        this.updateSystemStats();
        
        this.statsMonitorTimer = setInterval(() => {
            this.updateSystemStats();
        }, interval);
        
        if (this.logger?.debug) {
            this.logger.debug('SystemController', `Stats monitoring started (interval: ${interval}ms)`);
        }
    }

    /**
     * Arrête le monitoring périodique
     */
    stopStatsMonitoring() {
        if (this.statsMonitorTimer) {
            clearInterval(this.statsMonitorTimer);
            this.statsMonitorTimer = null;
            
            if (this.logger?.debug) {
                this.logger.debug('SystemController', 'Stats monitoring stopped');
            }
        }
    }

    // ========================================================================
    // GESTION BACKEND
    // ========================================================================

    getBackendStatus() {
        if (!this.backend) {
            return { 
                connected: false, 
                status: 'unavailable',
                offlineMode: true,
                reconnectionStopped: true
            };
        }

        const isConnected = typeof this.backend.isConnected === 'function' 
            ? this.backend.isConnected() 
            : false;

        return {
            connected: isConnected,
            status: isConnected ? 'connected' : 'disconnected',
            offlineMode: this.offlineMode,
            reconnectionStopped: this.backend.reconnectionStopped || false,
            reconnectAttempts: this.backend.reconnectAttempts || 0,
            maxReconnectAttempts: this.backend.maxReconnectAttempts || 10
        };
    }

    handleOfflineMode(data) {
        if (this.logger?.warn) {
            this.logger.warn('SystemController', '⚠️ Backend offline mode activated');
        }
        
        this.offlineMode = true;
        this.systemHealth = 'degraded';
        
        this.stopStatsMonitoring();
        this.stopConnectionMonitor();
        
        this.showReconnectButton();
        
        this.eventBus.emit('system:backend_offline');
    }

    handleBackendConnected(data) {
        if (this.logger?.info) {
            this.logger.info('SystemController', '✅ Backend connected');
        }
        
        this.offlineMode = false;
        this.systemHealth = 'good';
        
        this.removeReconnectButton();
        
        this.updateSystemStats();
        
        this.eventBus.emit('system:backend_online');
    }

    handleBackendDisconnected(data) {
        if (this.logger?.warn) {
            this.logger.warn('SystemController', '⚠️ Backend disconnected');
        }
        
        this.systemHealth = 'degraded';
        
        this.stopStatsMonitoring();
        
        this.eventBus.emit('system:backend_disconnected');
    }

    // ========================================================================
    // GESTION UI
    // ========================================================================

    refreshSystemView() {
        if (!this.view || typeof this.view.render !== 'function') {
            return;
        }

        const systemData = {
            config: this.model.get('systemConfig') || this.defaultConfig,
            stats: this.systemStats,
            health: this.systemHealth,
            backendStatus: this.getBackendStatus(),
            backendInfo: this.backendInfo
        };

        this.view.render(systemData);
    }

    showReconnectButton() {
        const systemElement = document.getElementById('system');
        if (!systemElement || this.reconnectButtonCreated) return;
        
        const backendSection = systemElement.querySelector('.backend-status') || 
                              document.createElement('div');
        backendSection.className = 'backend-status offline';
        
        const button = document.createElement('button');
        button.className = 'btn btn-primary reconnect-button';
        button.innerHTML = '🔄 Reconnect to Backend';
        button.onclick = () => this.reconnectManually();
        
        const info = document.createElement('div');
        info.className = 'backend-offline-info';
        info.innerHTML = `
            <p class="offline-message">
                <span class="status-indicator offline"></span>
                Backend offline. Some features unavailable.
            </p>
        `;
        
        backendSection.innerHTML = '';
        backendSection.appendChild(info);
        backendSection.appendChild(button);
        this.reconnectButtonCreated = true;
    }

    removeReconnectButton() {
        const systemElement = document.getElementById('system');
        if (!systemElement) return;
        
        const backendSection = systemElement.querySelector('.backend-status');
        if (backendSection) {
            backendSection.remove();
        }
        
        this.reconnectButtonCreated = false;
    }

    async reconnectManually() {
        if (!this.backend) return;
        
        try {
            if (this.logger?.info) {
                this.logger.info('SystemController', '🔄 Manual reconnect');
            }
            
            this.eventBus.emit('notification:show', { 
                message: 'Attempting to reconnect...', 
                type: 'info', 
                duration: 2000 
            });
            
            const button = document.querySelector('.reconnect-button');
            if (button) {
                button.disabled = true;
                button.innerHTML = '⏳ Connecting...';
            }
            
            let success = false;
            if (typeof this.backend.reconnectManually === 'function') {
                success = await this.backend.reconnectManually();
            } else if (typeof this.backend.connect === 'function') {
                success = await this.backend.connect();
            }
            
            if (success) {
                if (this.logger?.info) {
                    this.logger.info('SystemController', '✅ Reconnected');
                }
                
                this.eventBus.emit('notification:show', { 
                    message: 'Reconnected!', 
                    type: 'success', 
                    duration: 3000 
                });
            } else {
                throw new Error('Connection failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'Reconnect failed:', error);
            }
            
            this.eventBus.emit('notification:show', { 
                message: 'Reconnection failed: ' + error.message, 
                type: 'error', 
                duration: 5000 
            });
            
            const button = document.querySelector('.reconnect-button');
            if (button) {
                button.disabled = false;
                button.innerHTML = '🔄 Reconnect to Backend';
            }
        }
    }

    startConnectionMonitor() {
        this.stopConnectionMonitor();
        
        if (this.offlineMode) return;
        
        this.connectionMonitorTimer = setInterval(() => {
            if (this.backend && typeof this.backend.isConnected === 'function') {
                const status = this.getBackendStatus();
                
                if (status.offlineMode || status.reconnectionStopped) {
                    this.stopConnectionMonitor();
                }
            }
        }, 10000);
    }

    stopConnectionMonitor() {
        if (this.connectionMonitorTimer) {
            clearInterval(this.connectionMonitorTimer);
            this.connectionMonitorTimer = null;
        }
    }

    // ========================================================================
    // ÉVÉNEMENTS PAGE
    // ========================================================================

    onSystemPageActive() {
        this.refreshSystemView();
        
        // Démarrer monitoring si backend connecté
        if (this.backend && this.backend.isConnected()) {
            this.startStatsMonitoring();
        }
    }

    onSystemPageInactive() {
        // Arrêter monitoring pour économiser ressources
        this.stopStatsMonitoring();
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    /**
     * Obtient toutes les stats système en une fois
     * @returns {Object}
     */
    getSystemStats() {
        return {
            ...this.systemStats,
            health: this.systemHealth,
            backendInfo: this.backendInfo,
            backendConnected: this.backend?.isConnected() || false
        };
    }

    /**
     * Formate l'uptime en string lisible
     * @param {Object} uptime
     * @returns {string}
     */
    formatUptime(uptime) {
        if (!uptime) return 'N/A';
        
        const parts = [];
        if (uptime.days > 0) parts.push(`${uptime.days}d`);
        if (uptime.hours > 0) parts.push(`${uptime.hours}h`);
        if (uptime.minutes > 0) parts.push(`${uptime.minutes}m`);
        if (parts.length === 0) parts.push(`${uptime.seconds}s`);
        
        return parts.join(' ');
    }

    /**
     * Formate la mémoire en string lisible
     * @param {number} mb - Mémoire en MB
     * @returns {string}
     */
    formatMemory(mb) {
        if (mb >= 1024) {
            return `${(mb / 1024).toFixed(2)} GB`;
        }
        return `${mb.toFixed(0)} MB`;
    }

    /**
     * Formate le disque en string lisible
     * @param {number} gb - Disque en GB
     * @returns {string}
     */
    formatDisk(gb) {
        if (gb >= 1024) {
            return `${(gb / 1024).toFixed(2)} TB`;
        }
        return `${gb.toFixed(2)} GB`;
    }

    // ========================================================================
    // LEGACY / COMPATIBILITÉ
    // ========================================================================

    async reconnectBackend() { 
        return this.reconnectManually(); 
    }
    
    async refreshSystemStatus() { 
        this.refreshSystemView(); 
    }
    
    updateInstrumentLatencies() {
        // Placeholder pour compatibilité
    }

    updateFPSStats() {
        // Placeholder pour compatibilité
    }

    logDebug(...args) {
        if (this.logger?.debug) {
            this.logger.debug('SystemController', ...args);
        }
    }

    // ========================================================================
    // DESTRUCTION
    // ========================================================================

    destroy() {
        this.stopConnectionMonitor();
        this.stopStatsMonitoring();
        this.removeReconnectButton();
        
        if (this.logger?.info) {
            this.logger.info('SystemController', 'Destroyed');
        }
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = SystemController;
}

if (typeof window !== 'undefined') {
    window.SystemController = SystemController;
}
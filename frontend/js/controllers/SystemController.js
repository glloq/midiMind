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
     * Obtient les informations système complètes
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
     * Obtient la liste des commandes disponibles
     * @returns {Promise<Array>}
     */
    async getSystemCommands() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('system.commands', {});
            
            if (response.success) {
                this.backendInfo.commands = response.data.commands || [];
                this.backendInfo.capabilities = response.data.capabilities || [];
                return response.data;
            } else {
                throw new Error(response.error_message || 'Failed to get commands list');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'getSystemCommands failed:', error);
            }
            throw error;
        }
    }

    /**
     * Test de connectivité (ping)
     * @returns {Promise<Object>}
     */
    async pingBackend() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const startTime = Date.now();
            const response = await this.backend.sendCommand('system.ping', {});
            const latency = Date.now() - startTime;
            
            if (response.success) {
                return {
                    pong: response.data.pong,
                    latency: latency,
                    timestamp: Date.now()
                };
            } else {
                throw new Error(response.error_message || 'Ping failed');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'pingBackend failed:', error);
            }
            throw error;
        }
    }

    // ========================================================================
    // LOGGER BACKEND
    // ========================================================================

    /**
     * Définit le niveau de log du backend
     * @param {string} level - DEBUG, INFO, WARNING, ERROR, CRITICAL
     * @returns {Promise<boolean>}
     */
    async setBackendLogLevel(level) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        const validLevels = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'];
        if (!validLevels.includes(level)) {
            throw new Error(`Invalid log level. Must be one of: ${validLevels.join(', ')}`);
        }

        try {
            const response = await this.backend.sendCommand('logger.setLevel', { level });
            
            if (response.success) {
                this.backendLogLevel = level;
                
                if (this.logger?.info) {
                    this.logger.info('SystemController', `Backend log level set to: ${level}`);
                }
                
                this.eventBus.emit('system:log_level_changed', { level });
                return true;
            } else {
                throw new Error(response.error_message || 'Failed to set log level');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'setBackendLogLevel failed:', error);
            }
            throw error;
        }
    }

    /**
     * Obtient le niveau de log actuel du backend
     * @returns {Promise<string>}
     */
    async getBackendLogLevel() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        try {
            const response = await this.backend.sendCommand('logger.getLevel', {});
            
            if (response.success) {
                this.backendLogLevel = response.data.level;
                return response.data.level;
            } else {
                throw new Error(response.error_message || 'Failed to get log level');
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'getBackendLogLevel failed:', error);
            }
            throw error;
        }
    }

    // ========================================================================
    // MONITORING TEMPS RÉEL
    // ========================================================================

    /**
     * Met à jour toutes les statistiques système
     */
    async updateSystemStats() {
        if (!this.backend || !this.backend.isConnected()) {
            return;
        }

        try {
            // Exécuter en parallèle pour plus de rapidité
            const [uptime, memory, disk] = await Promise.allSettled([
                this.getSystemUptime(),
                this.getSystemMemory(),
                this.getSystemDisk()
            ]);

            // Analyser l'état de santé système
            this.analyzeSystemHealth();

            // Rafraîchir la vue si sur la page system
            this.refreshSystemView();

            // Émettre événement pour autres composants
            this.eventBus.emit('system:stats_updated', {
                uptime: uptime.status === 'fulfilled' ? uptime.value : null,
                memory: memory.status === 'fulfilled' ? memory.value : null,
                disk: disk.status === 'fulfilled' ? disk.value : null,
                health: this.systemHealth
            });

        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'updateSystemStats failed:', error);
            }
        }
    }

    /**
     * Analyse l'état de santé du système
     */
    analyzeSystemHealth() {
        let issues = [];

        // Vérifier mémoire
        if (this.systemStats.memory.percent > 90) {
            issues.push('critical_memory');
            this.systemHealth = 'critical';
        } else if (this.systemStats.memory.percent > 75) {
            issues.push('warning_memory');
            if (this.systemHealth === 'good') {
                this.systemHealth = 'warning';
            }
        }

        // Vérifier swap
        if (this.systemStats.memory.swap_percent > 50) {
            issues.push('warning_swap');
            if (this.systemHealth === 'good') {
                this.systemHealth = 'warning';
            }
        }

        // Vérifier disque
        if (this.systemStats.disk.percent > 95) {
            issues.push('critical_disk');
            this.systemHealth = 'critical';
        } else if (this.systemStats.disk.percent > 85) {
            issues.push('warning_disk');
            if (this.systemHealth === 'good') {
                this.systemHealth = 'warning';
            }
        }

        // Si aucun problème
        if (issues.length === 0) {
            this.systemHealth = 'good';
        }

        // Émettre alertes si nécessaire
        if (issues.length > 0) {
            this.eventBus.emit('system:health_issues', { 
                health: this.systemHealth, 
                issues 
            });
        }

        return this.systemHealth;
    }

    /**
     * Démarre le monitoring automatique des stats
     */
    startStatsMonitoring() {
        this.stopStatsMonitoring();

        // Première mise à jour immédiate
        this.updateSystemStats();

        // Puis toutes les 5 secondes
        this.statsMonitorTimer = setInterval(() => {
            this.updateSystemStats();
        }, 5000);

        if (this.logger?.info) {
            this.logger.info('SystemController', '✓ Stats monitoring started');
        }
    }

    /**
     * Arrête le monitoring automatique
     */
    stopStatsMonitoring() {
        if (this.statsMonitorTimer) {
            clearInterval(this.statsMonitorTimer);
            this.statsMonitorTimer = null;
        }
    }

    // ========================================================================
    // GESTION VUE
    // ========================================================================

    refreshSystemView() {
        // Vérifier container existe avant render
        if (!this.view) {
            if (this.logger?.debug) {
                this.logger.debug('SystemController', 'View not initialized');
            }
            return;
        }
        
        if (!this.view.container && this.view.element) {
            this.view.container = this.view.element;
        }
        
        if (!this.view.container) {
            if (this.logger?.debug) {
                this.logger.debug('SystemController', 'Container not ready, skipping render');
            }
            return;
        }
        
        if (typeof this.view.render !== 'function') {
            if (this.logger?.debug) {
                this.logger.debug('SystemController', 'View render not available');
            }
            return;
        }
        
        const backendStatus = this.getBackendStatus();
        const data = {
            systemHealth: this.systemHealth,
            audioConfig: this.defaultConfig.audioConfig,
            visualizerConfig: this.defaultConfig.visualizerConfig,
            interfaceConfig: this.defaultConfig.interfaceConfig,
            advancedConfig: this.defaultConfig.advancedConfig,
            backend: backendStatus,
            backendConnected: backendStatus.connected,
            offlineMode: this.offlineMode || backendStatus.offlineMode,
            
            // Nouvelles stats système
            stats: this.systemStats,
            backendInfo: this.backendInfo,
            backendLogLevel: this.backendLogLevel
        };
        
        this.view.render(data);
        
        if (this.offlineMode || backendStatus.offlineMode) {
            this.createReconnectButton();
        }
    }

    getBackendStatus() {
        if (!this.backend) {
            return { 
                connected: false, 
                offlineMode: true, 
                url: 'ws://localhost:8080', 
                queuedCommands: 0, 
                state: 'unavailable' 
            };
        }
        
        if (typeof this.backend.getStatus === 'function') {
            return this.backend.getStatus();
        }
        
        return {
            connected: this.backend.isConnected ? this.backend.isConnected() : false,
            offlineMode: this.backend.isOffline ? this.backend.isOffline() : false,
            url: this.backend.config?.url || 'ws://localhost:8080',
            queuedCommands: this.backend.messageQueue?.length || 0,
            state: this.backend.getConnectionState ? this.backend.getConnectionState() : 'unknown'
        };
    }

    getBackendData() { 
        return this.getBackendStatus(); 
    }

    // ========================================================================
    // GESTION CONNEXION BACKEND
    // ========================================================================

    handleOfflineMode(data) {
        this.offlineMode = true;
        
        if (this.logger?.warn) {
            this.logger.warn('SystemController', '🔴 Backend offline mode');
        }
        
        this.stopConnectionMonitor();
        this.stopStatsMonitoring();
        this.refreshSystemView();
        this.createReconnectButton();
    }

    handleBackendConnected(data) {
        this.offlineMode = false;
        
        if (this.logger?.info) {
            this.logger.info('SystemController', '✅ Backend reconnected');
        }
        
        this.startConnectionMonitor();
        
        // Charger info backend
        this.loadBackendInfo();
        
        this.refreshSystemView();
        this.removeReconnectButton();
    }

    handleBackendDisconnected(data) {
        if (this.logger?.warn) {
            this.logger.warn('SystemController', '🔴 Backend disconnected');
        }
        
        this.stopStatsMonitoring();
        this.refreshSystemView();
    }

    /**
     * Charge les informations du backend au démarrage
     */
    async loadBackendInfo() {
        if (!this.backend || !this.backend.isConnected()) {
            return;
        }

        try {
            // Charger version, commandes, log level
            await Promise.allSettled([
                this.getSystemVersion(),
                this.getSystemCommands(),
                this.getBackendLogLevel()
            ]);

            if (this.logger?.info) {
                this.logger.info('SystemController', 
                    `✓ Backend info loaded: ${this.backendInfo.version} ` +
                    `(${this.backendInfo.commands.length} commands)`
                );
            }
        } catch (error) {
            if (this.logger?.error) {
                this.logger.error('SystemController', 'loadBackendInfo failed:', error);
            }
        }
    }

    createReconnectButton() {
        if (this.reconnectButtonCreated) return;
        
        const systemElement = document.getElementById('system');
        if (!systemElement) return;
        
        let backendSection = systemElement.querySelector('.backend-status');
        if (!backendSection) {
            backendSection = document.createElement('div');
            backendSection.className = 'backend-status';
            systemElement.insertBefore(backendSection, systemElement.firstChild);
        }
        
        if (backendSection.querySelector('.reconnect-button')) return;
        
        const button = document.createElement('button');
        button.className = 'reconnect-button btn-primary';
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
// ============================================================================
// Fichier: frontend/js/controllers/SystemController.js
// Version: 3.1.0 - OFFLINE MODE + MANUAL RECONNECT
// Date: 2025-10-24
// ============================================================================

class SystemController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        this.logger = window.Logger || console;
        this.model = models.system || models.state;
        this.view = views.system;
        this.backend = window.app?.services?.backend || window.backendService;
        
        this.defaultConfig = {
            audioConfig: { bufferSize: 256, sampleRate: 44100, targetLatency: 10, autoCompensation: true },
            visualizerConfig: { targetFPS: 60, timeWindow: 10, pianoKeyHeight: 20, antiAliasing: true, visualEffects: true },
            interfaceConfig: { theme: 'light', animations: true, soundNotifications: false, showTooltips: true, keyboardShortcuts: true },
            advancedConfig: { verboseLogging: false, realtimeMetrics: true, predictiveCache: true, dataCompression: true, strictMidiValidation: true }
        };
        
        this.systemHealth = 'good';
        this.currentPreset = 'balanced';
        this.showAdvanced = false;
        this.statsUpdateInterval = null;
        this.statsMonitoringInterval = null;
        this.calibrationInProgress = false;
        this.connectionMonitorTimer = null;
        this.offlineMode = false;
        this.reconnectButtonCreated = false;
        
        this._fullyInitialized = true;
        this.startConnectionMonitor();
        this.initializeSystemConfig();
    }

    bindEvents() {
        this.eventBus.on('instrument:connected', () => { this.updateInstrumentLatencies(); this.refreshSystemView(); });
        this.eventBus.on('instrument:disconnected', () => { this.updateInstrumentLatencies(); this.refreshSystemView(); });
        this.eventBus.on('system:request_stats_update', () => this.updateSystemStats());
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'system') this.onSystemPageActive();
            else this.onSystemPageInactive();
        });
        this.eventBus.on('performance:fps_update', (data) => this.updateFPSStats(data.fps));
        this.eventBus.on('backend:offline-mode', (data) => this.handleOfflineMode(data));
        this.eventBus.on('backend:connected', (data) => this.handleBackendConnected(data));
        this.eventBus.on('backend:disconnected', (data) => this.handleBackendDisconnected(data));
        
        if (this.logger && this.logger.info) this.logger.info('SystemController', '✓ Events bound');
    }

    initializeSystemConfig() {
        if (this.logger && this.logger.info) this.logger.info('SystemController', 'Initializing system config...');
        const config = this.loadConfig() || this.defaultConfig;
        if (this.model && typeof this.model.set === 'function') this.model.set('systemConfig', config);
        this.bindEvents();
        if (this.logger && this.logger.info) this.logger.info('SystemController', '✓ System config initialized');
    }

    loadConfig() {
        try {
            const saved = localStorage.getItem('midimind_system_config');
            return saved ? JSON.parse(saved) : null;
        } catch (error) {
            if (this.logger && this.logger.error) this.logger.error('SystemController', 'Failed to load config:', error);
            return null;
        }
    }

    saveConfig(config) {
        try {
            localStorage.setItem('midimind_system_config', JSON.stringify(config));
            if (this.logger && this.logger.info) this.logger.info('SystemController', 'Config saved');
        } catch (error) {
            if (this.logger && this.logger.error) this.logger.error('SystemController', 'Failed to save config:', error);
        }
    }

    refreshSystemView() {
        if (!this.view || typeof this.view.render !== 'function') return;
        
        const backendStatus = this.getBackendStatus();
        const data = {
            systemHealth: this.systemHealth,
            audioConfig: this.defaultConfig.audioConfig,
            visualizerConfig: this.defaultConfig.visualizerConfig,
            interfaceConfig: this.defaultConfig.interfaceConfig,
            advancedConfig: this.defaultConfig.advancedConfig,
            backend: backendStatus,
            backendConnected: backendStatus.connected,
            offlineMode: this.offlineMode || backendStatus.offlineMode
        };
        
        this.view.render(data);
        if (this.offlineMode || backendStatus.offlineMode) this.createReconnectButton();
    }

    getBackendStatus() {
        if (!this.backend) {
            return { connected: false, offlineMode: true, url: 'ws://localhost:8080', queuedCommands: 0, state: 'unavailable' };
        }
        
        if (typeof this.backend.getStatus === 'function') return this.backend.getStatus();
        
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

    handleOfflineMode(data) {
        this.offlineMode = true;
        if (this.logger && this.logger.warn) this.logger.warn('SystemController', '📴 Backend offline mode activated');
        this.stopConnectionMonitor();
        this.refreshSystemView();
        this.createReconnectButton();
    }

    handleBackendConnected(data) {
        this.offlineMode = false;
        if (this.logger && this.logger.info) this.logger.info('SystemController', '✅ Backend reconnected');
        this.startConnectionMonitor();
        this.refreshSystemView();
        this.removeReconnectButton();
    }

    handleBackendDisconnected(data) {
        if (this.logger && this.logger.warn) this.logger.warn('SystemController', '🔴 Backend disconnected');
        this.refreshSystemView();
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
        info.innerHTML = `<p class="offline-message"><span class="status-indicator offline"></span>Backend is offline. Some features are unavailable.</p>`;
        
        backendSection.innerHTML = '';
        backendSection.appendChild(info);
        backendSection.appendChild(button);
        this.reconnectButtonCreated = true;
        
        if (this.logger && this.logger.debug) this.logger.debug('SystemController', 'Reconnect button created');
    }

    removeReconnectButton() {
        const systemElement = document.getElementById('system');
        if (!systemElement) return;
        
        const backendSection = systemElement.querySelector('.backend-status');
        if (backendSection) backendSection.remove();
        this.reconnectButtonCreated = false;
    }

    async reconnectManually() {
        if (!this.backend) {
            if (this.logger && this.logger.error) this.logger.error('SystemController', 'Backend service not available');
            return;
        }
        
        try {
            if (this.logger && this.logger.info) this.logger.info('SystemController', '🔄 Manual reconnection requested');
            
            this.eventBus.emit('notification:show', { message: 'Attempting to reconnect...', type: 'info', duration: 2000 });
            
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
                if (this.logger && this.logger.info) this.logger.info('SystemController', '✅ Manual reconnection successful');
                this.eventBus.emit('notification:show', { message: 'Reconnected successfully!', type: 'success', duration: 3000 });
            } else {
                throw new Error('Connection failed');
            }
            
        } catch (error) {
            if (this.logger && this.logger.error) this.logger.error('SystemController', 'Manual reconnection failed:', error);
            this.eventBus.emit('notification:show', { message: 'Reconnection failed: ' + error.message, type: 'error', duration: 5000 });
            
            const button = document.querySelector('.reconnect-button');
            if (button) {
                button.disabled = false;
                button.innerHTML = '🔄 Reconnect to Backend';
            }
        }
    }

    startConnectionMonitor() {
        this.stopConnectionMonitor();
        if (this.offlineMode) {
            if (this.logger && this.logger.debug) this.logger.debug('SystemController', 'Not starting monitor - offline mode active');
            return;
        }
        
        this.connectionMonitorTimer = setInterval(async () => {
            if (this.backend && typeof this.backend.isConnected === 'function') {
                const status = this.getBackendStatus();
                if (status.offlineMode || status.reconnectionStopped) this.stopConnectionMonitor();
            }
        }, 10000);
        
        if (this.logger && this.logger.debug) this.logger.debug('SystemController', 'Connection monitor started');
    }

    stopConnectionMonitor() {
        if (this.connectionMonitorTimer) {
            clearInterval(this.connectionMonitorTimer);
            this.connectionMonitorTimer = null;
            if (this.logger && this.logger.debug) this.logger.debug('SystemController', 'Connection monitor stopped');
        }
    }

    async reconnectBackend() {
        return this.reconnectManually();
    }

    async refreshSystemStatus() {
        if (this.logger && this.logger.info) this.logger.info('SystemController', 'Refreshing system status...');
        this.refreshSystemView();
    }

    async updateSystemStats() {
        this.refreshSystemView();
    }

    startStatsMonitoring() {
        if (this.statsMonitoringInterval) return;
        if (this.logger && this.logger.debug) this.logger.debug('SystemController', '📊 Starting stats monitoring');
        this.statsMonitoringInterval = setInterval(() => this.updateSystemStats(), 1000);
    }

    stopStatsMonitoring() {
        if (this.statsMonitoringInterval) {
            clearInterval(this.statsMonitoringInterval);
            this.statsMonitoringInterval = null;
            if (this.logger && this.logger.debug) this.logger.debug('SystemController', '⏸️ Stats monitoring stopped');
        }
    }

    updateInstrumentLatencies() {
        if (this.logger && this.logger.debug) this.logger.debug('SystemController', 'Updating instrument latencies...');
    }

    onSystemPageActive() {
        if (this.logger && this.logger.info) this.logger.info('SystemController', 'System page activated');
        this.startStatsMonitoring();
        this.refreshSystemView();
    }

    onSystemPageInactive() {
        if (this.logger && this.logger.info) this.logger.info('SystemController', 'System page deactivated');
        this.stopStatsMonitoring();
    }

    updateFPSStats(fps) {}

    logDebug(category, message, data = null) {
        if (!this.logger) {
            console.log(`[${category}] ${message}`, data || '');
            return;
        }
        if (typeof this.logger.debug === 'function') this.logger.debug(category, message, data);
        else if (typeof this.logger.info === 'function') this.logger.info(category, message, data);
        else console.log(`[${category}] ${message}`, data || '');
    }

    destroy() {
        this.stopStatsMonitoring();
        this.stopConnectionMonitor();
        this.removeReconnectButton();
        if (this.logger && this.logger.info) this.logger.info('SystemController', 'Destroyed');
    }
}

if (typeof module !== 'undefined' && module.exports) module.exports = SystemController;
if (typeof window !== 'undefined') window.SystemController = SystemController;
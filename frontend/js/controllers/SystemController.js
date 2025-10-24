// ============================================================================
// Fichier: frontend/js/controllers/SystemController.js
// Version: 3.1.1 - CONTAINER CHECK FIX
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
        this.offlineMode = false;
        this.reconnectButtonCreated = false;
        this.connectionMonitorTimer = null;
        
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

    refreshSystemView() {
        // ✅ VÉRIFIER container existe avant render
        if (!this.view) {
            if (this.logger && this.logger.debug) this.logger.debug('SystemController', 'View not initialized');
            return;
        }
        
        if (!this.view.container && this.view.element) {
            this.view.container = this.view.element;
        }
        
        if (!this.view.container) {
            if (this.logger && this.logger.debug) this.logger.debug('SystemController', 'Container not ready, skipping render');
            return;
        }
        
        if (typeof this.view.render !== 'function') {
            if (this.logger && this.logger.debug) this.logger.debug('SystemController', 'View render not available');
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

    getBackendData() { return this.getBackendStatus(); }

    handleOfflineMode(data) {
        this.offlineMode = true;
        if (this.logger && this.logger.warn) this.logger.warn('SystemController', '📴 Backend offline mode');
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
        info.innerHTML = `<p class="offline-message"><span class="status-indicator offline"></span>Backend offline. Some features unavailable.</p>`;
        
        backendSection.innerHTML = '';
        backendSection.appendChild(info);
        backendSection.appendChild(button);
        this.reconnectButtonCreated = true;
    }

    removeReconnectButton() {
        const systemElement = document.getElementById('system');
        if (!systemElement) return;
        const backendSection = systemElement.querySelector('.backend-status');
        if (backendSection) backendSection.remove();
        this.reconnectButtonCreated = false;
    }

    async reconnectManually() {
        if (!this.backend) return;
        
        try {
            if (this.logger && this.logger.info) this.logger.info('SystemController', '🔄 Manual reconnect');
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
                if (this.logger && this.logger.info) this.logger.info('SystemController', '✅ Reconnected');
                this.eventBus.emit('notification:show', { message: 'Reconnected!', type: 'success', duration: 3000 });
            } else {
                throw new Error('Connection failed');
            }
        } catch (error) {
            if (this.logger && this.logger.error) this.logger.error('SystemController', 'Reconnect failed:', error);
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
        if (this.offlineMode) return;
        
        this.connectionMonitorTimer = setInterval(() => {
            if (this.backend && typeof this.backend.isConnected === 'function') {
                const status = this.getBackendStatus();
                if (status.offlineMode || status.reconnectionStopped) this.stopConnectionMonitor();
            }
        }, 10000);
    }

    stopConnectionMonitor() {
        if (this.connectionMonitorTimer) {
            clearInterval(this.connectionMonitorTimer);
            this.connectionMonitorTimer = null;
        }
    }

    async reconnectBackend() { return this.reconnectManually(); }
    async refreshSystemStatus() { this.refreshSystemView(); }
    async updateSystemStats() { this.refreshSystemView(); }
    updateInstrumentLatencies() {}
    onSystemPageActive() { this.refreshSystemView(); }
    onSystemPageInactive() {}

    destroy() {
        this.stopConnectionMonitor();
        this.removeReconnectButton();
        if (this.logger && this.logger.info) this.logger.info('SystemController', 'Destroyed');
    }
}

if (typeof module !== 'undefined' && module.exports) module.exports = SystemController;
if (typeof window !== 'undefined') window.SystemController = SystemController;
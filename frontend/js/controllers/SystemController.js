// ============================================================================
// Fichier: frontend/js/controllers/SystemController.js
// Version: 3.0.1-FIXED
// Date: 2025-10-20
// ============================================================================
// CORRECTIONS v3.0.1:
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Fixed initialization order (logger before startConnectionMonitor)
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Added _fullyInitialized pattern
// ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Protected logger calls throughout
// ============================================================================


class SystemController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Initialize logger FIRST
        this.logger = window.logger || console;
        
        // Get specific model and view
        this.model = models.system || models.state;
        this.view = views.system;
        
        // Backend service
        this.backend = window.app?.services?.backend || window.backendService;
        
        // Configuration systÃƒÆ’Ã‚Â¨me par dÃƒÆ’Ã‚Â©faut
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
        
        // ÃƒÆ’Ã¢â‚¬Â°tat systÃƒÆ’Ã‚Â¨me
        this.systemHealth = 'good';
        this.currentPreset = 'balanced';
        this.showAdvanced = false;
        this.statsUpdateInterval = null;
        this.statsMonitoringInterval = null;
        this.calibrationInProgress = false;
        this.connectionMonitorTimer = null;
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        // Now start monitoring and initialize
        this.startConnectionMonitor();
        this.initializeSystemConfig();
    }

    /**
     * Configuration des ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
     */
    bindEvents() {
        // ÃƒÆ’Ã¢â‚¬Â°couter les changements d'instruments pour mettre ÃƒÆ’Ã‚Â  jour les latences
        this.eventBus.on('instrument:connected', (data) => {
            this.updateInstrumentLatencies();
            this.refreshSystemView();
        });
        
        this.eventBus.on('instrument:disconnected', (data) => {
            this.updateInstrumentLatencies();
            this.refreshSystemView();
        });
        
        // ÃƒÆ’Ã¢â‚¬Â°couter les demandes de mise ÃƒÆ’Ã‚Â  jour des stats
        this.eventBus.on('system:request_stats_update', () => {
            this.updateSystemStats();
        });
        
        // ÃƒÆ’Ã¢â‚¬Â°couter les changements de page
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'system') {
                this.onSystemPageActive();
            } else {
                this.onSystemPageInactive();
            }
        });
        
        // ÃƒÆ’Ã¢â‚¬Â°couter les changements de performance
        this.eventBus.on('performance:fps_update', (data) => {
            this.updateFPSStats(data.fps);
        });
        
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Å“ Events bound');
        }
    }

/**
 * MÃƒÂ©thode init() publique appelÃƒÂ©e par Application.js
 */
init() {
    if (this.logger && this.logger.info) {
        this.logger.info('SystemController', 'SystemController.init() called');
    }
    
    // S'assurer que la vue est rendue
    this.refreshSystemView();
    
    // Charger la configuration systÃƒÂ¨me
    this.initializeSystemConfig();
}

/**
 * RafraÃƒÂ®chit la vue systÃƒÂ¨me
 */
refreshSystemView() {
    if (!this.view) {
        return;
    }
    
    if (typeof this.view.render === 'function') {
        this.view.render();
        if (this.logger && this.logger.debug) {
            this.logger.debug('SystemController', 'SystemView rendered');
        }
    }
}

    /**
     * Initialise la configuration systÃƒÆ’Ã‚Â¨me
     */
    initializeSystemConfig() {
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'Initializing system config...');
        }
        
        // Load saved config or use defaults
        const savedConfig = this.loadConfig();
        const config = savedConfig || this.defaultConfig;
        
        if (this.model && typeof this.model.set === 'function') {
            this.model.set('systemConfig', config);
        }
        
        this.bindEvents();
        
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Å“ System config initialized');
        }
    }

    /**
     * Charge la configuration sauvegardÃƒÆ’Ã‚Â©e
     */
    loadConfig() {
        try {
            const saved = localStorage.getItem('midimind_system_config');
            return saved ? JSON.parse(saved) : null;
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('SystemController', 'Failed to load config:', error);
            }
            return null;
        }
    }

    /**
     * Sauvegarde la configuration
     */
    saveConfig(config) {
        try {
            localStorage.setItem('midimind_system_config', JSON.stringify(config));
            if (this.logger && this.logger.info) {
                this.logger.info('SystemController', 'Config saved');
            }
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('SystemController', 'Failed to save config:', error);
            }
        }
    }

    /**
     * RafraÃƒÆ’Ã‚Â®chit la vue systÃƒÆ’Ã‚Â¨me
     */

    /**
     * Obtient les donnÃƒÆ’Ã‚Â©es du backend
     */
    getBackendData() {
        if (!this.backend) {
            return {
                connected: false,
                url: 'ws://localhost:8080',
                queuedCommands: 0
            };
        }
        
        return {
            connected: this.backend.isConnected ? this.backend.isConnected() : false,
            url: this.backend.wsUrl || 'ws://localhost:8080',
            queuedCommands: this.backend.commandQueue?.length || 0
        };
    }

    /**
     * VÃƒÆ’Ã‚Â©rifie pÃƒÆ’Ã‚Â©riodiquement la connexion
     */
    startConnectionMonitor() {
        // ArrÃƒÆ’Ã‚Âªter monitor existant
        this.stopConnectionMonitor();
        
        this.connectionMonitorTimer = setInterval(async () => {
            if (this.backend && typeof this.backend.isConnected === 'function') {
                if (!this.backend.isConnected()) {
                    if (this.logger && this.logger.warn) {
                        this.logger.warn('SystemController', 'Backend disconnected, attempting reconnect...');
                    }
                    await this.reconnectBackend();
                }
            }
        }, 10000); // Check toutes les 10 secondes
        
        if (this.logger && this.logger.debug) {
            this.logger.debug('SystemController', 'Connection monitor started');
        }
    }

    /**
     * ArrÃƒÆ’Ã‚Âªte le monitoring de connexion
     */
    stopConnectionMonitor() {
        if (this.connectionMonitorTimer) {
            clearInterval(this.connectionMonitorTimer);
            this.connectionMonitorTimer = null;
        }
    }

    /**
     * Tente de reconnecter le backend
     */
    async reconnectBackend() {
        if (!this.backend) {
            if (this.logger && this.logger.error) {
                this.logger.error('SystemController', 'Backend service not available');
            }
            return;
        }
        
        try {
            if (this.logger && this.logger.info) {
                this.logger.info('SystemController', 'ÃƒÂ°Ã…Â¸Ã¢â‚¬ÂÃ¢â‚¬Å¾ Attempting reconnection...');
            }
            
            this.eventBus.emit('notification:show', {
                message: 'Reconnexion au backend...',
                type: 'info',
                duration: 2000
            });
            
            // DÃƒÆ’Ã‚Â©connecter proprement d'abord
            if (typeof this.backend.disconnect === 'function') {
                this.backend.disconnect();
            }
            
            // Attendre un peu
            await new Promise(resolve => setTimeout(resolve, 500));
            
            // Reconnecter
            if (typeof this.backend.connect === 'function') {
                await this.backend.connect();
            }
            
            if (this.logger && this.logger.info) {
                this.logger.info('SystemController', 'ÃƒÂ¢Ã…â€œÃ¢â‚¬Å“ Reconnection successful');
            }
            
            this.eventBus.emit('notification:show', {
                message: 'ReconnectÃƒÆ’Ã‚Â© avec succÃƒÆ’Ã‚Â¨s !',
                type: 'success',
                duration: 3000
            });
            
            // RafraÃƒÆ’Ã‚Â®chir ÃƒÆ’Ã‚Â©tat systÃƒÆ’Ã‚Â¨me
            await this.refreshSystemStatus();
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('SystemController', 'Reconnection failed:', error);
            }
            this.eventBus.emit('notification:show', {
                message: 'ÃƒÆ’Ã¢â‚¬Â°chec de reconnexion: ' + error.message,
                type: 'error',
                duration: 5000
            });
        }
    }

    /**
     * RafraÃƒÆ’Ã‚Â®chit le statut systÃƒÆ’Ã‚Â¨me
     */
    async refreshSystemStatus() {
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'Refreshing system status...');
        }
        
        this.refreshSystemView();
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour les statistiques systÃƒÆ’Ã‚Â¨me
     */
    async updateSystemStats() {
        // Update stats logic here
        this.refreshSystemView();
    }

    /**
     * DÃƒÆ’Ã‚Â©marre le monitoring temps rÃƒÆ’Ã‚Â©el
     */
    startStatsMonitoring() {
        if (this.statsMonitoringInterval) {
            return; // DÃƒÆ’Ã‚Â©jÃƒÆ’Ã‚Â  dÃƒÆ’Ã‚Â©marrÃƒÆ’Ã‚Â©
        }
        
        if (this.logger && this.logger.debug) {
            this.logger.debug('SystemController', 'ÃƒÂ°Ã…Â¸Ã¢â‚¬Å“Ã…Â  Starting stats monitoring');
        }
        
        // Update toutes les secondes
        this.statsMonitoringInterval = setInterval(() => {
            this.updateSystemStats();
        }, 1000);
    }

    /**
     * ArrÃƒÆ’Ã‚Âªte le monitoring
     */
    stopStatsMonitoring() {
        if (this.statsMonitoringInterval) {
            clearInterval(this.statsMonitoringInterval);
            this.statsMonitoringInterval = null;
            
            if (this.logger && this.logger.debug) {
                this.logger.debug('SystemController', 'ÃƒÂ¢Ã‚ÂÃ‚Â¸ÃƒÂ¯Ã‚Â¸Ã‚Â Stats monitoring stopped');
            }
        }
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour les latences des instruments
     */
    updateInstrumentLatencies() {
        if (this.logger && this.logger.debug) {
            this.logger.debug('SystemController', 'Updating instrument latencies...');
        }
        // Logic here
    }

    /**
     * Callback quand la page systÃƒÆ’Ã‚Â¨me devient active
     */
    onSystemPageActive() {
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'System page activated');
        }
        this.startStatsMonitoring();
        this.refreshSystemView();
    }

    /**
     * Callback quand la page systÃƒÆ’Ã‚Â¨me devient inactive
     */
    onSystemPageInactive() {
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'System page deactivated');
        }
        this.stopStatsMonitoring();
    }

    /**
     * Met ÃƒÆ’Ã‚Â  jour les stats FPS
     */
    updateFPSStats(fps) {
        // Update FPS logic
    }

    /**
     * Helper pour logger en debug
     */
    logDebug(category, message, data = null) {
        if (!this.logger) {
            console.log(`[${category}] ${message}`, data || '');
            return;
        }
        
        if (typeof this.logger.debug === 'function') {
            this.logger.debug(category, message, data);
        } else if (typeof this.logger.info === 'function') {
            this.logger.info(category, message, data);
        } else {
            console.log(`[${category}] ${message}`, data || '');
        }
    }

    /**
     * Nettoie les ressources du contrÃƒÆ’Ã‚Â´leur
     */
    destroy() {
        this.stopStatsMonitoring();
        this.stopConnectionMonitor();
        
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'Destroyed');
        }
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = SystemController;
}

if (typeof window !== 'undefined') {
    window.SystemController = SystemController;
}

// Export par dÃ©faut
window.SystemController = SystemController;

// ============================================================================
// FIN DU FICHIER SystemController.js v3.0.1-FIXED
// ============================================================================
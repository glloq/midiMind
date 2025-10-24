// ============================================================================
// Fichier: frontend/js/controllers/SystemController.js
// Version: 3.0.1-FIXED
// Date: 2025-10-20
// ============================================================================
// CORRECTIONS v3.0.1:
// ✅ Fixed initialization order (logger before startConnectionMonitor)
// ✅ Added _fullyInitialized pattern
// ✅ Protected logger calls throughout
// ============================================================================

class SystemController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
        // Initialize logger FIRST
        this.logger = window.Logger || console;
        
        // Get specific model and view
        this.model = models.system || models.state;
        this.view = views.system;
        
        // Backend service
        this.backend = window.app?.services?.backend || window.backendService;
        
        // Configuration système par défaut
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
     * Configuration des événements
     */
    bindEvents() {
        // Écouter les changements d'instruments pour mettre à jour les latences
        this.eventBus.on('instrument:connected', (data) => {
            this.updateInstrumentLatencies();
            this.refreshSystemView();
        });
        
        this.eventBus.on('instrument:disconnected', (data) => {
            this.updateInstrumentLatencies();
            this.refreshSystemView();
        });
        
        // Écouter les demandes de mise à jour des stats
        this.eventBus.on('system:request_stats_update', () => {
            this.updateSystemStats();
        });
        
        // Écouter les changements de page
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'system') {
                this.onSystemPageActive();
            } else {
                this.onSystemPageInactive();
            }
        });
        
        // Écouter les changements de performance
        this.eventBus.on('performance:fps_update', (data) => {
            this.updateFPSStats(data.fps);
        });
        
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', '✓ Events bound');
        }
    }

    /**
     * Initialise la configuration système
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
            this.logger.info('SystemController', '✓ System config initialized');
        }
    }

    /**
     * Charge la configuration sauvegardée
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
     * Rafraîchit la vue système
     */
    refreshSystemView() {
        if (!this.view || typeof this.view.render !== 'function') {
            return;
        }
        
        const data = {
            systemHealth: this.systemHealth,
            audioConfig: this.defaultConfig.audioConfig,
            visualizerConfig: this.defaultConfig.visualizerConfig,
            interfaceConfig: this.defaultConfig.interfaceConfig,
            advancedConfig: this.defaultConfig.advancedConfig,
            backend: this.getBackendData(),
            backendConnected: this.backend?.isConnected() || false
        };
        
        this.view.render(data);
    }

    /**
     * Obtient les données du backend
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
     * Vérifie périodiquement la connexion
     */
    startConnectionMonitor() {
        // Arrêter monitor existant
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
     * Arrête le monitoring de connexion
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
                this.logger.info('SystemController', '🔄 Attempting reconnection...');
            }
            
            this.eventBus.emit('notification:show', {
                message: 'Reconnexion au backend...',
                type: 'info',
                duration: 2000
            });
            
            // Déconnecter proprement d'abord
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
                this.logger.info('SystemController', '✓ Reconnection successful');
            }
            
            this.eventBus.emit('notification:show', {
                message: 'Reconnecté avec succès !',
                type: 'success',
                duration: 3000
            });
            
            // Rafraîchir état système
            await this.refreshSystemStatus();
            
        } catch (error) {
            if (this.logger && this.logger.error) {
                this.logger.error('SystemController', 'Reconnection failed:', error);
            }
            this.eventBus.emit('notification:show', {
                message: 'Échec de reconnexion: ' + error.message,
                type: 'error',
                duration: 5000
            });
        }
    }

    /**
     * Rafraîchit le statut système
     */
    async refreshSystemStatus() {
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'Refreshing system status...');
        }
        
        this.refreshSystemView();
    }

    /**
     * Met à jour les statistiques système
     */
    async updateSystemStats() {
        // Update stats logic here
        this.refreshSystemView();
    }

    /**
     * Démarre le monitoring temps réel
     */
    startStatsMonitoring() {
        if (this.statsMonitoringInterval) {
            return; // Déjà démarré
        }
        
        if (this.logger && this.logger.debug) {
            this.logger.debug('SystemController', '📊 Starting stats monitoring');
        }
        
        // Update toutes les secondes
        this.statsMonitoringInterval = setInterval(() => {
            this.updateSystemStats();
        }, 1000);
    }

    /**
     * Arrête le monitoring
     */
    stopStatsMonitoring() {
        if (this.statsMonitoringInterval) {
            clearInterval(this.statsMonitoringInterval);
            this.statsMonitoringInterval = null;
            
            if (this.logger && this.logger.debug) {
                this.logger.debug('SystemController', '⏸️ Stats monitoring stopped');
            }
        }
    }

    /**
     * Met à jour les latences des instruments
     */
    updateInstrumentLatencies() {
        if (this.logger && this.logger.debug) {
            this.logger.debug('SystemController', 'Updating instrument latencies...');
        }
        // Logic here
    }

    /**
     * Callback quand la page système devient active
     */
    onSystemPageActive() {
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'System page activated');
        }
        this.startStatsMonitoring();
        this.refreshSystemView();
    }

    /**
     * Callback quand la page système devient inactive
     */
    onSystemPageInactive() {
        if (this.logger && this.logger.info) {
            this.logger.info('SystemController', 'System page deactivated');
        }
        this.stopStatsMonitoring();
    }

    /**
     * Met à jour les stats FPS
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
     * Nettoie les ressources du contrôleur
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
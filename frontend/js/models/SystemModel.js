// ============================================================================
// Fichier: frontend/js/models/SystemModel.js
// Version: v3.0.0 - NOUVEAU (Création complète)
// Date: 2025-10-08
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Modèle de gestion de l'état système et configuration globale.
//   Monitoring, métriques, configuration MIDI, logs système.
//
// CRÉATION v3.0.0:
//   ✅ Intégration BackendService pour toutes les requêtes
//   ✅ Monitoring système (CPU, RAM, latence)
//   ✅ Configuration MIDI ports
//   ✅ Logs système
//   ✅ Statistiques globales
//
// Responsabilités:
//   - Stocker configuration système
//   - Monitorer performances (CPU, RAM, latence MIDI)
//   - Gérer logs système
//   - Configuration des ports MIDI
//   - Statistiques globales application
//
// Design Patterns:
//   - Observer (via BaseModel)
//   - Singleton (état système unique)
//
// Auteur: midiMind Team
// ============================================================================

class SystemModel extends BaseModel {
    constructor(eventBus, backend, logger) {
        super(eventBus, logger);
        
        // Dépendances
        this.backend = backend;
        this.logger = logger;
        
        // Initialiser les données via BaseModel
        this.initialize({
            // État système
            systemStatus: 'unknown',      // unknown, healthy, warning, critical
            uptime: 0,                     // ms
            backendConnected: false,
            
            // Configuration MIDI
            midiConfig: {
                latency: 0,                // ms
                bufferSize: 256,
                sampleRate: 44100,
                midiThrough: false,
                clockSource: 'internal'    // internal, external, auto
            },
            
            // Monitoring
            monitoring: {
                enabled: false,
                interval: 1000,            // ms
                history: []                // Historique des métriques
            },
            
            // Métriques actuelles
            metrics: {
                cpu: 0,                    // 0-100%
                memory: 0,                 // MB
                midiLatency: 0,            // ms
                messagesPerSecond: 0,
                activeConnections: 0,
                errors: 0
            },
            
            // Logs système
            logs: [],
            maxLogs: 500,
            
            // Configuration interface
            uiConfig: {
                theme: 'dark',             // dark, light
                language: 'fr',
                showDebug: false,
                autoConnect: true
            },
            
            // Version et build
            version: '3.0.0',
            buildDate: '2025-10-08',
            platform: 'RaspberryPi'
        });
        
        // Timers
        this.monitoringTimer = null;
        this.statsUpdateTimer = null;
        
        // Configuration
        this.config = {
            monitoringInterval: 1000,
            maxHistorySize: 100,
            maxLogSize: 500,
            autoStartMonitoring: false
        };
        
        this.logger.info('SystemModel', '✓ Model initialized');
        
        this.initialize();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    async initialize() {
        // Charger configuration depuis backend
        await this.loadSystemConfig();
        
        // Démarrer monitoring si auto-start
        if (this.config.autoStartMonitoring) {
            this.startMonitoring();
        }
        
        // Écouter événements backend
        this.attachEvents();
    }
    
    attachEvents() {
        // Backend connecté
        this.eventBus.on('backend:connected', () => {
            this.set('backendConnected', true);
            this.loadSystemConfig();
        });
        
        // Backend déconnecté
        this.eventBus.on('backend:disconnected', () => {
            this.set('backendConnected', false);
        });
        
        // Métriques reçues du backend
        this.eventBus.on('system:metrics', (data) => {
            this.updateMetrics(data);
        });
        
        // Shutdown
        this.eventBus.on('app:shutdown', () => {
            this.stopMonitoring();
        });
    }
    
    // ========================================================================
    // CONFIGURATION SYSTÈME
    // ========================================================================
    
    /**
     * Charge la configuration système depuis le backend
     */
    async loadSystemConfig() {
        if (!this.backend || !this.backend.isConnected()) {
            this.logger.warn('SystemModel', 'Backend not connected, cannot load config');
            return;
        }
        
        try {
            this.logger.info('SystemModel', 'Loading system configuration...');
            
            const response = await this.backend.sendCommand('system.config.get');
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to load config');
            }
            
            const config = response.data?.config || response.config || {};
            
            // Mettre à jour configuration MIDI
            if (config.midi) {
                this.update({
                    midiConfig: {
                        ...this.get('midiConfig'),
                        ...config.midi
                    }
                });
            }
            
            // Mettre à jour configuration UI
            if (config.ui) {
                this.update({
                    uiConfig: {
                        ...this.get('uiConfig'),
                        ...config.ui
                    }
                });
            }
            
            this.logger.info('SystemModel', '✓ System config loaded');
            
            this.eventBus.emit('system:config-loaded', config);
            
        } catch (error) {
            this.logger.error('SystemModel', 'Failed to load config:', error);
        }
    }
    
    /**
     * Sauvegarde la configuration système vers le backend
     */
    async saveSystemConfig() {
        if (!this.backend || !this.backend.isConnected()) {
            this.logger.warn('SystemModel', 'Backend not connected, cannot save config');
            return false;
        }
        
        try {
            this.logger.info('SystemModel', 'Saving system configuration...');
            
            const config = {
                midi: this.get('midiConfig'),
                ui: this.get('uiConfig')
            };
            
            const response = await this.backend.sendCommand('system.config.set', {
                config: config
            });
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to save config');
            }
            
            this.logger.info('SystemModel', '✓ System config saved');
            
            this.eventBus.emit('system:config-saved', config);
            
            return true;
            
        } catch (error) {
            this.logger.error('SystemModel', 'Failed to save config:', error);
            return false;
        }
    }
    
    // ========================================================================
    // CONFIGURATION MIDI
    // ========================================================================
    
    /**
     * Met à jour la configuration MIDI
     * @param {Object} updates - Mises à jour partielles
     */
    async updateMidiConfig(updates) {
        const currentConfig = this.get('midiConfig');
        const newConfig = { ...currentConfig, ...updates };
        
        this.set('midiConfig', newConfig);
        
        // Envoyer au backend
        if (this.backend && this.backend.isConnected()) {
            try {
                await this.backend.sendCommand('system.midi.config', {
                    config: newConfig
                });
                
                this.logger.info('SystemModel', 'MIDI config updated:', updates);
                
            } catch (error) {
                this.logger.error('SystemModel', 'Failed to update MIDI config:', error);
            }
        }
        
        this.eventBus.emit('system:midi-config-changed', newConfig);
    }
    
    /**
     * Liste les ports MIDI disponibles
     */
    async getMidiPorts() {
        if (!this.backend || !this.backend.isConnected()) {
            return { inputs: [], outputs: [] };
        }
        
        try {
            const response = await this.backend.sendCommand('system.midi.ports');
            
            if (!response.success) {
                throw new Error(response.error || 'Failed to get MIDI ports');
            }
            
            return response.data?.ports || { inputs: [], outputs: [] };
            
        } catch (error) {
            this.logger.error('SystemModel', 'Failed to get MIDI ports:', error);
            return { inputs: [], outputs: [] };
        }
    }
    
    // ========================================================================
    // MONITORING SYSTÈME
    // ========================================================================
    
    /**
     * Démarre le monitoring système
     */
    startMonitoring() {
        if (this.monitoringTimer) {
            this.logger.warn('SystemModel', 'Monitoring already running');
            return;
        }
        
        const monitoring = this.get('monitoring');
        monitoring.enabled = true;
        this.set('monitoring', monitoring);
        
        this.monitoringTimer = setInterval(() => {
            this.updateSystemMetrics();
        }, this.config.monitoringInterval);
        
        this.logger.info('SystemModel', 'Monitoring started');
        
        this.eventBus.emit('system:monitoring-started');
    }
    
    /**
     * Arrête le monitoring système
     */
    stopMonitoring() {
        if (this.monitoringTimer) {
            clearInterval(this.monitoringTimer);
            this.monitoringTimer = null;
        }
        
        const monitoring = this.get('monitoring');
        monitoring.enabled = false;
        this.set('monitoring', monitoring);
        
        this.logger.info('SystemModel', 'Monitoring stopped');
        
        this.eventBus.emit('system:monitoring-stopped');
    }
    
    /**
     * Met à jour les métriques système depuis le backend
     */
    async updateSystemMetrics() {
        if (!this.backend || !this.backend.isConnected()) {
            return;
        }
        
        try {
            const response = await this.backend.sendCommand('system.metrics');
            
            if (response.success && response.data) {
                this.updateMetrics(response.data);
            }
            
        } catch (error) {
            this.logger.error('SystemModel', 'Failed to update metrics:', error);
        }
    }
    
    /**
     * Met à jour les métriques en mémoire
     * @param {Object} data - Données métriques
     */
    updateMetrics(data) {
        if (!data) return;
        
        const metrics = {
            cpu: data.cpu || 0,
            memory: data.memory || 0,
            midiLatency: data.midiLatency || 0,
            messagesPerSecond: data.messagesPerSecond || 0,
            activeConnections: data.activeConnections || 0,
            errors: data.errors || 0,
            timestamp: Date.now()
        };
        
        this.set('metrics', metrics);
        
        // Ajouter à l'historique
        this.addToHistory(metrics);
        
        // Déterminer statut système
        this.updateSystemStatus(metrics);
        
        this.eventBus.emit('system:metrics-updated', metrics);
    }
    
    /**
     * Ajoute des métriques à l'historique
     * @private
     */
    addToHistory(metrics) {
        const monitoring = this.get('monitoring');
        
        monitoring.history.push(metrics);
        
        // Limiter taille historique
        if (monitoring.history.length > this.config.maxHistorySize) {
            monitoring.history.shift();
        }
        
        this.set('monitoring', monitoring, { silent: true });
    }
    
    /**
     * Met à jour le statut système basé sur les métriques
     * @private
     */
    updateSystemStatus(metrics) {
        let status = 'healthy';
        
        // CPU critique > 90%
        if (metrics.cpu > 90) {
            status = 'critical';
        }
        // CPU warning > 70%
        else if (metrics.cpu > 70) {
            status = 'warning';
        }
        
        // Latence critique > 50ms
        if (metrics.midiLatency > 50) {
            status = 'critical';
        }
        // Latence warning > 20ms
        else if (metrics.midiLatency > 20) {
            status = status === 'healthy' ? 'warning' : status;
        }
        
        // Erreurs
        if (metrics.errors > 0) {
            status = status === 'healthy' ? 'warning' : status;
        }
        
        this.set('systemStatus', status);
    }
    
    // ========================================================================
    // LOGS SYSTÈME
    // ========================================================================
    
    /**
     * Ajoute un log système
     * @param {string} level - debug, info, warn, error
     * @param {string} message - Message
     * @param {Object} data - Données additionnelles
     */
    addLog(level, message, data = null) {
        const logs = this.get('logs');
        
        const log = {
            timestamp: Date.now(),
            level: level,
            message: message,
            data: data
        };
        
        logs.push(log);
        
        // Limiter taille
        if (logs.length > this.get('maxLogs')) {
            logs.shift();
        }
        
        this.set('logs', logs);
        
        this.eventBus.emit('system:log-added', log);
    }
    
    /**
     * Récupère les logs filtrés
     * @param {Object} filters - Filtres (level, since, until)
     * @returns {Array}
     */
    getLogs(filters = {}) {
        let logs = this.get('logs');
        
        // Filtrer par level
        if (filters.level) {
            logs = logs.filter(log => log.level === filters.level);
        }
        
        // Filtrer par date
        if (filters.since) {
            logs = logs.filter(log => log.timestamp >= filters.since);
        }
        
        if (filters.until) {
            logs = logs.filter(log => log.timestamp <= filters.until);
        }
        
        return logs;
    }
    
    /**
     * Efface tous les logs
     */
    clearLogs() {
        this.set('logs', []);
        this.logger.info('SystemModel', 'Logs cleared');
        this.eventBus.emit('system:logs-cleared');
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * Obtient l'état système complet
     */
    getSystemState() {
        return {
            status: this.get('systemStatus'),
            uptime: this.get('uptime'),
            backendConnected: this.get('backendConnected'),
            metrics: this.get('metrics'),
            midiConfig: this.get('midiConfig'),
            uiConfig: this.get('uiConfig'),
            version: this.get('version')
        };
    }
    
    /**
     * Obtient les statistiques système
     */
    getStats() {
        const monitoring = this.get('monitoring');
        const logs = this.get('logs');
        
        return {
            uptime: this.get('uptime'),
            metricsCollected: monitoring.history.length,
            logsCount: logs.length,
            systemStatus: this.get('systemStatus'),
            monitoringEnabled: monitoring.enabled
        };
    }
    
    /**
     * Calcule l'uptime depuis le démarrage
     */
    updateUptime() {
        // Cette méthode serait appelée périodiquement
        // Pour l'instant on se base sur le timestamp de création
        const uptime = Date.now() - new Date(this.meta.created).getTime();
        this.set('uptime', uptime, { silent: true });
    }
    
    // ========================================================================
    // NETTOYAGE
    // ========================================================================
    
    /**
     * Détruit le modèle et nettoie les ressources
     */
    destroy() {
        this.logger.info('SystemModel', 'Destroying...');
        
        this.stopMonitoring();
        
        super.destroy();
        
        this.logger.info('SystemModel', '✓ Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = SystemModel;
}

if (typeof window !== 'undefined') {
    window.SystemModel = SystemModel;
}
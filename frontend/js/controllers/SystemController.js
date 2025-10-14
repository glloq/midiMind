// ===== SYSTEM CONTROLLER - Contrôleur de configuration système =====
// ====================================================================
// Gère toute la configuration système de l'application :
// - Interface avec SystemView pour l'affichage
// - Configuration audio/MIDI (buffer, latence, sample rate)
// - Calibration automatique des instruments
// - Présets de visualiseur (performance, qualité, équilibré)
// - Gestion des thèmes (clair/sombre)
// - Monitoring des performances en temps réel
// - Import/export des configurations
// - Mode expert avec paramètres avancés
// ====================================================================

class SystemController extends BaseController {
    constructor(eventBus, models, views, notifications, debugConsole) {
        super(eventBus, models, views, notifications, debugConsole);
        
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
        this.calibrationInProgress = false;
        this.connectionMonitorTimer = null;
		
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
    }

    /**
     * Initialise la configuration système
     */
    initializeSystemConfig() {
        const stateModel = this.getModel('state');
        const existingConfig = stateModel.get('systemConfig');
        
        if (!existingConfig) {
            stateModel.set('systemConfig', this.defaultConfig);
            this.logDebug('system', 'Configuration système initialisée avec les valeurs par défaut');
        } else {
            // Fusionner avec les valeurs par défaut pour les nouvelles options
            const mergedConfig = this.mergeConfigs(this.defaultConfig, existingConfig);
            stateModel.set('systemConfig', mergedConfig);
        }
        
        this.applySystemConfig();
    }

    /**
     * Page système activée
     */
    onSystemPageActive() {
        this.refreshSystemView();
        this.startStatsMonitoring();
        this.logDebug('system', 'Page système activée');
    }

    /**
     * Page système désactivée
     */
    onSystemPageInactive() {
        this.stopStatsMonitoring();
        this.logDebug('system', 'Page système désactivée');
    }

    // ===== CONFIGURATION AUDIO/MIDI =====

    /**
     * Met à jour une configuration audio
     * @param {string} key - Clé de configuration
     * @param {any} value - Nouvelle valeur
     */
    updateAudioConfig(key, value) {
        const stateModel = this.getModel('state');
        const systemConfig = { ...stateModel.get('systemConfig') };
        
        systemConfig.audioConfig[key] = value;
        stateModel.set('systemConfig', systemConfig);
        
        this.logDebug('system', `Configuration audio mise à jour: ${key} = ${value}`);
        
        // Appliquer immédiatement certains changements
        switch (key) {
            case 'bufferSize':
                this.applyBufferSizeChange(value);
                break;
            case 'targetLatency':
                this.applyLatencyTarget(value);
                break;
            case 'autoCompensation':
                this.applyAutoCompensation(value);
                break;
        }
        
        this.showNotification(`${key} mis à jour: ${value}`, 'success');
        this.refreshSystemView();
    }

    /**
     * Applique un changement de taille de buffer
     * @param {number} bufferSize - Nouvelle taille de buffer
     */
    applyBufferSizeChange(bufferSize) {
        // Notifier tous les composants qui utilisent l'audio
        this.eventBus.emit('audio:buffer_size_changed', { bufferSize });
        
        const latencyMs = (bufferSize / 44100) * 1000;
        this.logDebug('system', `Buffer audio: ${bufferSize} samples (${latencyMs.toFixed(1)}ms)`);
    }

    /**
     * Applique un changement de latence cible
     * @param {number} targetLatency - Latence cible en ms
     */
    applyLatencyTarget(targetLatency) {
        this.eventBus.emit('audio:latency_target_changed', { targetLatency });
        this.logDebug('system', `Latence cible: ${targetLatency}ms`);
    }

    /**
     * Active/désactive la compensation automatique
     * @param {boolean} enabled - État de la compensation
     */
    applyAutoCompensation(enabled) {
        this.eventBus.emit('audio:auto_compensation_changed', { enabled });
        this.logDebug('system', `Compensation automatique: ${enabled ? 'ON' : 'OFF'}`);
    }

    // ===== CALIBRATION AUTOMATIQUE =====

    /**
     * Lance la calibration automatique du système
     */
    async autoCalibrate() {
        if (this.calibrationInProgress) {
            this.showNotification('Calibration déjà en cours', 'warning');
            return;
        }
        
        this.calibrationInProgress = true;
        this.logDebug('system', 'Début de la calibration automatique...');
        this.showNotification('Calibration en cours...', 'info');
        
        try {
            // 1. Calibrer les performances du visualiseur
            const fpsResult = await this.calibrateVisualizerPerformance();
            
            // 2. Calibrer la latence des instruments
            const latencyResult = await this.calibrateInstrumentLatencies();
            
            // 3. Optimiser les paramètres audio
            const audioResult = await this.optimizeAudioSettings();
            
            // 4. Appliquer le preset optimal
            const optimalPreset = this.determineOptimalPreset(fpsResult, latencyResult, audioResult);
            this.applyVisualizerPreset(optimalPreset);
            
            this.logDebug('system', `Calibration terminée - Preset: ${optimalPreset}`);
            this.showNotification(`Calibration terminée (${optimalPreset})`, 'success');
            
        } catch (error) {
            this.logDebug('system', `Erreur calibration: ${error.message}`);
            this.showNotification('Erreur lors de la calibration', 'error');
        } finally {
            this.calibrationInProgress = false;
            this.refreshSystemView();
        }
    }

    /**
     * Calibre les performances du visualiseur
     * @returns {Promise<Object>} - Résultats de performance
     */
    async calibrateVisualizerPerformance() {
        this.logDebug('system', 'Calibration performance visualiseur...');
        
        return new Promise((resolve) => {
            const startTime = performance.now();
            let frameCount = 0;
            const testDuration = 3000; // 3 secondes
            
            const testLoop = () => {
                frameCount++;
                const elapsed = performance.now() - startTime;
                
                if (elapsed < testDuration) {
                    requestAnimationFrame(testLoop);
                } else {
                    const avgFPS = frameCount / (elapsed / 1000);
                    this.logDebug('system', `Performance mesurée: ${avgFPS.toFixed(1)} FPS`);
                    resolve({ avgFPS, frameCount, duration: elapsed });
                }
            };
            
            requestAnimationFrame(testLoop);
        });
    }

    /**
     * Calibre la latence de tous les instruments connectés
     * @returns {Promise<Object>} - Résultats de latence
     */
    async calibrateInstrumentLatencies() {
        const instrumentModel = this.getModel('instrument');
        const connectedInstruments = instrumentModel.getConnectedInstruments();
        
        this.logDebug('system', `Calibration latence de ${connectedInstruments.length} instruments...`);
        
        const results = [];
        for (const instrument of connectedInstruments) {
            try {
                const latency = await this.calibrateInstrument(instrument.id);
                results.push({ instrumentId: instrument.id, latency });
            } catch (error) {
                this.logDebug('system', `Erreur calibration ${instrument.name}: ${error.message}`);
            }
        }
        
        const maxLatency = Math.max(...results.map(r => r.latency), 0);
        return { instruments: results, maxLatency };
    }

    /**
     * Calibre un instrument spécifique
     * @param {string} instrumentId - ID de l'instrument
     * @returns {Promise<number>} - Latence mesurée
     */
    async calibrateInstrument(instrumentId) {
        const instrumentModel = this.getModel('instrument');
        const instrument = instrumentModel.getInstrumentById(instrumentId);
        
        if (!instrument || !instrument.connected) {
            throw new Error('Instrument non connecté');
        }
        
        this.logDebug('system', `Calibration ${instrument.name}...`);
        
        // Simuler la calibration de latence
        const samples = [];
        const sampleCount = 5;
        
        for (let i = 0; i < sampleCount; i++) {
            const startTime = performance.now();
            
            // Envoyer un message test silencieux
            await this.sendTestMessage(instrumentId);
            
            // Attendre la réponse (simulée)
            await this.sleep(Math.random() * 20 + 5); // 5-25ms
            
            const latency = performance.now() - startTime;
            samples.push(latency);
            
            // Pause entre les échantillons
            await this.sleep(50);
        }
        
        const avgLatency = samples.reduce((a, b) => a + b, 0) / samples.length;
        const jitter = Math.max(...samples) - Math.min(...samples);
        
        // Mettre à jour l'instrument
        instrumentModel.updateInstrument(instrumentId, {
            latency: avgLatency,
            jitter: jitter,
            lastCalibration: Date.now()
        });
        
        this.logDebug('system', `${instrument.name}: ${avgLatency.toFixed(1)}ms (jitter: ${jitter.toFixed(1)}ms)`);
        return avgLatency;
    }

    /**
     * Envoie un message de test pour calibration
     * @param {string} instrumentId - ID de l'instrument
     */
    async sendTestMessage(instrumentId) {
        const message = {
            type: 'noteOn',
            channel: 15, // Canal 16 (moins utilisé)
            note: 60,    // Do central
            velocity: 1, // Volume minimal
            instrumentId: instrumentId,
            timestamp: performance.now()
        };
        
        this.eventBus.emit('midi:test_message', message);
        
        // Note Off immédiat
        setTimeout(() => {
            this.eventBus.emit('midi:test_message', {
                ...message,
                type: 'noteOff',
                velocity: 0
            });
        }, 10);
    }

    // ===== PRESETS VISUALISEUR =====

    /**
     * Applique un preset de visualiseur
     * @param {string} presetName - Nom du preset
     */
    applyVisualizerPreset(presetName) {
        const presets = {
            performance: {
                targetFPS: 30,
                timeWindow: 5,
                pianoKeyHeight: 15,
                antiAliasing: false,
                visualEffects: false
            },
            balanced: {
                targetFPS: 60,
                timeWindow: 10,
                pianoKeyHeight: 20,
                antiAliasing: true,
                visualEffects: true
            },
            quality: {
                targetFPS: 120,
                timeWindow: 15,
                pianoKeyHeight: 25,
                antiAliasing: true,
                visualEffects: true
            }
        };
        
        const preset = presets[presetName];
        if (!preset) {
            this.showNotification('Preset inconnu', 'error');
            return;
        }
        
        // Appliquer le preset
        Object.keys(preset).forEach(key => {
            this.updateVisualizerConfig(key, preset[key]);
        });
        
        this.currentPreset = presetName;
        this.logDebug('system', `Preset visualiseur appliqué: ${presetName}`);
        this.showNotification(`Preset "${presetName}" appliqué`, 'success');
        
        // Notifier le visualiseur si disponible
        if (window.app?.visualizerController) {
            window.app.visualizerController.applyPreset(preset);
        }
        
        this.refreshSystemView();
    }

    /**
     * Détermine le preset optimal basé sur les résultats de calibration
     * @param {Object} fpsResult - Résultats FPS
     * @param {Object} latencyResult - Résultats latence
     * @param {Object} audioResult - Résultats audio
     * @returns {string} - Nom du preset optimal
     */
    determineOptimalPreset(fpsResult, latencyResult, audioResult) {
        const avgFPS = fpsResult.avgFPS;
        const maxLatency = latencyResult.maxLatency;
        
        if (avgFPS < 30 || maxLatency > 50) {
            return 'performance';
        } else if (avgFPS < 50 || maxLatency > 25) {
            return 'balanced';
        } else {
            return 'quality';
        }
    }

    // ===== CONFIGURATION VISUALISEUR =====

    /**
     * Met à jour une configuration visualiseur
     * @param {string} key - Clé de configuration
     * @param {any} value - Nouvelle valeur
     */
    updateVisualizerConfig(key, value) {
        const stateModel = this.getModel('state');
        const systemConfig = { ...stateModel.get('systemConfig') };
        
        systemConfig.visualizerConfig[key] = value;
        stateModel.set('systemConfig', systemConfig);
        
        this.logDebug('system', `Configuration visualiseur: ${key} = ${value}`);
        
        // Notifier le visualiseur
        this.eventBus.emit('visualizer:config_changed', { key, value });
        
        this.refreshSystemView();
    }

    // ===== CONFIGURATION INTERFACE =====

    /**
     * Change le thème de l'interface
     * @param {string} theme - Nom du thème ('light' ou 'dark')
     */
    setTheme(theme) {
        if (!['light', 'dark'].includes(theme)) {
            this.showNotification('Thème invalide', 'error');
            return;
        }
        
        this.updateInterfaceConfig('theme', theme);
        
        // Appliquer le thème immédiatement
        document.body.className = theme === 'dark' ? 'dark-mode' : '';
        
        this.logDebug('system', `Thème changé: ${theme}`);
        this.showNotification(`Thème ${theme === 'dark' ? 'sombre' : 'clair'} appliqué`, 'success');
    }

    /**
     * Met à jour une configuration interface
     * @param {string} key - Clé de configuration
     * @param {any} value - Nouvelle valeur
     */
    updateInterfaceConfig(key, value) {
        const stateModel = this.getModel('state');
        const systemConfig = { ...stateModel.get('systemConfig') };
        
        systemConfig.interfaceConfig[key] = value;
        stateModel.set('systemConfig', systemConfig);
        
        this.logDebug('system', `Configuration interface: ${key} = ${value}`);
        this.refreshSystemView();
    }

    /**
     * Active/désactive le mode expert
     * @param {boolean} enabled - État du mode expert
     */
    toggleAdvancedMode(enabled) {
        this.showAdvanced = enabled;
        this.logDebug('system', `Mode expert: ${enabled ? 'ON' : 'OFF'}`);
        this.refreshSystemView();
    }

    // ===== CONFIGURATION AVANCÉE =====

    /**
     * Met à jour une configuration avancée
     * @param {string} key - Clé de configuration
     * @param {any} value - Nouvelle valeur
     */
    updateAdvancedConfig(key, value) {
        const stateModel = this.getModel('state');
        const systemConfig = { ...stateModel.get('systemConfig') };
        
        systemConfig.advancedConfig[key] = value;
        stateModel.set('systemConfig', systemConfig);
        
        this.logDebug('system', `Configuration avancée: ${key} = ${value}`);
        
        // Appliquer certains changements immédiatement
        switch (key) {
            case 'verboseLogging':
                this.eventBus.emit('debug:verbose_logging', { enabled: value });
                break;
            case 'realtimeMetrics':
                if (value) {
                    this.startStatsMonitoring();
                } else {
                    this.stopStatsMonitoring();
                }
                break;
        }
        
        this.refreshSystemView();
    }

    // ===== MONITORING ET STATISTIQUES =====


/**
 * Démarre le monitoring temps réel
 */
startStatsMonitoring() {
    if (this.statsMonitoringInterval) {
        return; // Déjà démarré
    }
    
    this.logDebug('system', '📊 Starting stats monitoring');
    
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
        this.logDebug('system', '⏸️ Stats monitoring stopped');
    }
}
 
 
 
 /**
 * Met à jour les statistiques système
 */
async updateSystemStats() {
    const stateModel = this.getModel('state');
    
    // CPU Usage (estimation via tasks)
    const cpuUsage = this.estimateCPUUsage();
    
    // Memory Usage (via performance.memory si disponible)
    const memoryUsage = this.estimateMemoryUsage();
    
    // FPS actuel
    const currentFPS = this.getCurrentFPS();
    
    // Messages MIDI/sec
    const midiRate = this.getMidiMessageRate();
    
    // Latence estimée
    const latency = this.estimateCurrentLatency();
    
    // Uptime
    const uptime = this.getUptime();
    
    // Fichiers chargés
    const filesLoaded = this.getLoadedFilesCount();
    
    // Stocker dans state
    stateModel.set('systemStats', {
        cpuUsage,
        memoryUsage,
        currentFPS,
        midiRate,
        latency,
        uptime,
        filesLoaded,
        timestamp: Date.now()
    });
    
    // Émettre événement pour la vue
    this.eventBus.emit('system:stats-updated', {
        cpuUsage,
        memoryUsage,
        currentFPS,
        midiRate,
        latency
    });
}

/**
 * Estime le CPU usage
 * @private
 */
estimateCPUUsage() {
    // Utiliser performance.now() et mesurer les tasks
    const now = performance.now();
    const delta = now - (this.lastCPUCheck || now);
    this.lastCPUCheck = now;
    
    // Estimation basique (à améliorer avec Web Workers)
    const taskCount = this.getActiveTaskCount();
    return Math.min(100, taskCount * 5);
}

/**
 * Estime memory usage
 * @private
 */
estimateMemoryUsage() {
    if (performance.memory) {
        const used = performance.memory.usedJSHeapSize;
        const total = performance.memory.jsHeapSizeLimit;
        return (used / total) * 100;
    }
    return 0;
}

/**
 * Récupère FPS actuel
 * @private
 */
getCurrentFPS() {
    // Utiliser requestAnimationFrame pour mesurer
    if (!this.fpsCounter) {
        this.fpsCounter = { frames: 0, lastTime: performance.now() };
    }
    
    const now = performance.now();
    const delta = now - this.fpsCounter.lastTime;
    
    if (delta >= 1000) {
        const fps = (this.fpsCounter.frames / delta) * 1000;
        this.fpsCounter.frames = 0;
        this.fpsCounter.lastTime = now;
        this.fpsCounter.current = Math.round(fps);
    }
    
    this.fpsCounter.frames++;
    return this.fpsCounter.current || 60;
}


    /**
     * Met à jour les FPS actuels
     * @param {number} fps - Nouveaux FPS
     */
    updateFPSStats(fps) {
        this.currentFPS = fps;
    }

    /**
     * Obtient le taux de messages MIDI par seconde
     * @returns {number} - Messages/seconde
     */
    getMidiMessageRate() {
        // Cette valeur devrait être suivie par le système MIDI
        return this.midiMessageRate || 0;
    }

    /**
     * Obtient l'uptime de l'application
     * @returns {number} - Uptime en secondes
     */
    getUptime() {
        if (window.app?.startTime) {
            return Math.floor((performance.now() - window.app.startTime) / 1000);
        }
        return 0;
    }

    /**
     * Obtient le nombre de fichiers chargés
     * @returns {number} - Nombre de fichiers
     */
    getLoadedFilesCount() {
        const fileModel = this.getModel('file');
        const files = fileModel.get('files') || [];
        return files.filter(f => f.type === 'file').length;
    }

    /**
     * Remet à zéro les statistiques
     */
    resetStats() {
        this.currentFPS = 60;
        this.midiMessageRate = 0;
        this.lastPerfTime = performance.now();
        
        this.logDebug('system', 'Statistiques remises à zéro');
        this.showNotification('Statistiques réinitialisées', 'info');
        this.updateSystemStats();
    }

    // ===== IMPORT/EXPORT =====

    /**
     * Exporte la configuration système
     */
    exportSettings() {
        const stateModel = this.getModel('state');
        const systemConfig = stateModel.get('systemConfig');
        
        const exportData = {
            version: '1.0',
            timestamp: new Date().toISOString(),
            config: systemConfig
        };
        
        const blob = new Blob([JSON.stringify(exportData, null, 2)], { 
            type: 'application/json' 
        });
        const url = URL.createObjectURL(blob);
        
        const a = document.createElement('a');
        a.href = url;
        a.download = `midi-orchestrion-config-${new Date().toISOString().split('T')[0]}.json`;
        a.click();
        
        URL.revokeObjectURL(url);
        
        this.logDebug('system', 'Configuration exportée');
        this.showNotification('Configuration exportée', 'success');
    }

    /**
     * Importe une configuration système
     */
    importSettings() {
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.json';
        
        input.onchange = (event) => {
            const file = event.target.files[0];
            if (!file) return;
            
            const reader = new FileReader();
            reader.onload = (e) => {
                try {
                    const importData = JSON.parse(e.target.result);
                    
                    if (!importData.config) {
                        throw new Error('Format de fichier invalide');
                    }
                    
                    const stateModel = this.getModel('state');
                    stateModel.set('systemConfig', importData.config);
                    
                    this.applySystemConfig();
                    this.refreshSystemView();
                    
                    this.logDebug('system', 'Configuration importée');
                    this.showNotification('Configuration importée avec succès', 'success');
                    
                } catch (error) {
                    this.logDebug('system', `Erreur import: ${error.message}`);
                    this.showNotification('Erreur lors de l\'import', 'error');
                }
            };
            
            reader.readAsText(file);
        };
        
        input.click();
    }




/**
 * Export configuration
 */
exportConfig() {
    const stateModel = this.getModel('state');
    const systemConfig = stateModel.get('systemConfig') || this.defaultConfig;
    
    const exportData = {
        version: '3.0.0',
        exported: Date.now(),
        config: systemConfig
    };
    
    const json = JSON.stringify(exportData, null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = `midimind-config-${Date.now()}.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    this.showNotification('Configuration exported', 'success');
}
/**
 * Import configuration
 */
async importConfig(file) {
    try {
        const text = await file.text();
        const data = JSON.parse(text);
        
        // Valider
        if (!data.config || !data.version) {
            throw new Error('Invalid configuration file');
        }
        
        // Appliquer
        const stateModel = this.getModel('state');
        stateModel.set('systemConfig', data.config);
        
        // Sauvegarder
        window.storageService.save('systemConfig', data.config);
        
        // Rafraîchir vue
        this.refreshSystemView();
        
        this.showNotification('Configuration imported successfully', 'success');
        
    } catch (error) {
        this.showNotification('Failed to import configuration: ' + error.message, 'error');
    }
}

    // ===== UTILITAIRES =====

    /**
     * Applique toute la configuration système
     */
    applySystemConfig() {
        const stateModel = this.getModel('state');
        const config = stateModel.get('systemConfig');
        
        // Appliquer le thème
        if (config.interfaceConfig?.theme) {
            document.body.className = config.interfaceConfig.theme === 'dark' ? 'dark-mode' : '';
        }
        
        // Autres configurations...
        this.logDebug('system', 'Configuration système appliquée');
    }

    /**
     * Met à jour les latences des instruments
     */
    updateInstrumentLatencies() {
        // Cette méthode sera appelée quand les instruments changent
        const instrumentModel = this.getModel('instrument');
        const instruments = instrumentModel.getConnectedInstruments();
        
        // Calculer la latence maximale
        const latencies = instruments.map(i => i.latency || 0);
        const maxLatency = Math.max(...latencies, 0);
        
        // Mettre à jour l'état système
        if (maxLatency > 50) {
            this.systemHealth = 'warning';
        } else if (maxLatency > 100) {
            this.systemHealth = 'error';
        } else {
            this.systemHealth = 'good';
        }
    }

    /**
     * Optimise les paramètres audio automatiquement
     * @returns {Promise<Object>} - Résultats d'optimisation
     */
    async optimizeAudioSettings() {
        // Analyse de la performance actuelle
        const currentLatency = this.estimateCurrentLatency();
        
        let optimalBufferSize = 256;
        if (currentLatency > 50) {
            optimalBufferSize = 128; // Réduire la latence
        } else if (currentLatency < 10) {
            optimalBufferSize = 512; // Améliorer la stabilité
        }
        
        this.updateAudioConfig('bufferSize', optimalBufferSize);
        
        return { optimalBufferSize, currentLatency };
    }

    /**
     * Estime la latence actuelle du système
     * @returns {number} - Latence en ms
     */
    estimateCurrentLatency() {
        const instrumentModel = this.getModel('instrument');
        const instruments = instrumentModel.getConnectedInstruments();
        
        const latencies = instruments.map(i => i.latency || 0);
        return Math.max(...latencies, 0);
    }

    /**
     * Fusionne deux configurations
     * @param {Object} defaultConfig - Configuration par défaut
     * @param {Object} userConfig - Configuration utilisateur
     * @returns {Object} - Configuration fusionnée
     */
    mergeConfigs(defaultConfig, userConfig) {
        const merged = { ...defaultConfig };
        
        Object.keys(userConfig).forEach(key => {
            if (typeof userConfig[key] === 'object' && !Array.isArray(userConfig[key])) {
                merged[key] = { ...defaultConfig[key], ...userConfig[key] };
            } else {
                merged[key] = userConfig[key];
            }
        });
        
        return merged;
    }

    /**
     * Pause asynchrone
     * @param {number} ms - Durée en millisecondes
     * @returns {Promise} - Promise qui se résout après le délai
     */
    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    /**
     * Rafraîchit la vue système
     */
    refreshSystemView() {
        if (!document.getElementById('system-page')?.classList.contains('active')) return;
        
        const systemView = this.getView('system');
        if (!systemView) return;
        
        const data = this.buildSystemData();
        const container = document.getElementById('system-page');
        
        if (container) {
            container.innerHTML = systemView.buildTemplate(data);
        }
    }

    /**
     * Construit les données pour la vue système
     * @returns {Object} - Données pour SystemView
     */
    buildSystemData() {
    const stateModel = this.getModel('state');
    const instrumentModel = this.getModel('instrument');
    const systemConfig = stateModel.get('systemConfig') || this.defaultConfig;
    
    return {
        systemHealth: this.systemHealth,
        maxLatency: this.estimateCurrentLatency(),
        audioConfig: systemConfig.audioConfig,
        visualizerConfig: systemConfig.visualizerConfig,
        visualizerPreset: this.currentPreset,
        interfaceConfig: systemConfig.interfaceConfig,
        theme: systemConfig.interfaceConfig?.theme || 'light',
        advancedConfig: systemConfig.advancedConfig,
        showAdvanced: this.showAdvanced,
        instruments: instrumentModel.getConnectedInstruments(),
        backend: this.getBackendData(), // ⬅️ AJOUTER CETTE LIGNE
        backendConnected: this.getBackendData().connected, // ⬅️ AJOUTER CETTE LIGNE
        stats: {
            cpuUsage: this.estimateCPUUsage(),
            memoryUsage: this.estimateMemoryUsage(),
            avgFPS: this.getCurrentFPS(),
            midiMessagesPerSecond: this.getMidiMessageRate(),
            uptime: this.getUptime(),
            filesLoaded: this.getLoadedFilesCount()
        }
    };
}
// ============= GESTION BACKEND C++ =============

/**
 * Récupère les données du backend
 */
getBackendData() {
    if (!this.app || !this.app.playbackController || !this.app.playbackController.backend) {
        return {
            connected: false,
            url: 'ws://localhost:8080',
            queuedCommands: 0
        };
    }
    
    const backend = this.app.playbackController.backend;
    const state = backend.getBackendState();
    
    return {
        connected: backend.isConnected(),
        url: backend.wsUrl,
        isPlaying: state.isPlaying,
        position: state.position,
        tempo: state.tempo,
        currentFile: state.currentFile,
        queuedCommands: backend.commandQueue.length
    };
}

/**
 * Tente de reconnecter le backend
 * CORRECTION v3.0.2: Plus robuste
 */
async reconnectBackend() {
    if (!this.backend) {
        this.logger.error('SystemController', 'Backend service not available');
        return;
    }
    
    try {
        this.logger.info('SystemController', '🔄 Attempting reconnection...');
        this.eventBus.emit('notification:show', {
            message: 'Reconnexion au backend...',
            type: 'info',
            duration: 2000
        });
        
        // Déconnecter proprement d'abord
        this.backend.disconnect();
        
        // Attendre un peu
        await new Promise(resolve => setTimeout(resolve, 500));
        
        // Reconnecter
        await this.backend.connect();
        
        this.logger.info('SystemController', 'âœ… Reconnection successful');
        this.eventBus.emit('notification:show', {
            message: 'Reconnecté avec succès !',
            type: 'success',
            duration: 3000
        });
        
        // Rafraîchir état système
        await this.refreshSystemStatus();
        
    } catch (error) {
        this.logger.error('SystemController', 'Reconnection failed:', error);
        this.eventBus.emit('notification:show', {
            message: 'Échec de reconnexion: ' + error.message,
            type: 'error',
            duration: 5000
        });
    }
}

/**
 * Vérifie périodiquement la connexion
 */
startConnectionMonitor() {
    // Arrêter monitor existant
    this.stopConnectionMonitor();
    
    this.connectionMonitorTimer = setInterval(async () => {
        if (!this.backend.isConnected()) {
            this.logger.warn('SystemController', 'Backend disconnected, attempting reconnect...');
            await this.reconnectBackend();
        }
    }, 10000); // Check toutes les 10 secondes
    
    this.logger.debug('SystemController', 'Connection monitor started');
}

stopConnectionMonitor() {
    if (this.connectionMonitorTimer) {
        clearInterval(this.connectionMonitorTimer);
        this.connectionMonitorTimer = null;
    }
}

/**
 * Teste la connexion backend
 */
testBackendConnection() {
    if (!this.app || !this.app.playbackController || !this.app.playbackController.backend) {
        this.showNotification('Backend non initialisé', 'error');
        return;
    }
    
    const backend = this.app.playbackController.backend;
    
    if (backend.isConnected()) {
        this.showNotification('✅ Backend connecté et opérationnel', 'success');
        this.logDebug('backend', 'Test de connexion: OK');
    } else {
        this.showNotification('❌ Backend déconnecté', 'error');
        this.logDebug('backend', 'Test de connexion: ÉCHEC');
    }
}

/**
 * Vide la file d'attente des commandes
 */
clearBackendQueue() {
    if (!this.app || !this.app.playbackController || !this.app.playbackController.backend) {
        this.showNotification('Backend non initialisé', 'error');
        return;
    }
    
    const backend = this.app.playbackController.backend;
    backend.commandQueue = [];
    
    this.showNotification('File d\'attente vidée', 'success');
    this.logDebug('backend', 'File d\'attente des commandes vidée');
    this.refreshSystemView();
}
    /**
     * Nettoie les ressources du contrôleur
     */
    destroy() {
        this.stopStatsMonitoring();
    }
}
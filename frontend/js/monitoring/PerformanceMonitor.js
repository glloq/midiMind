// ============================================================================
// Fichier: frontend/js/monitoring/PerformanceMonitor.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Monitoring des performances de l'application en temps réel.
//   Métriques FPS, latence, mémoire, render time, etc.
//
// Fonctionnalités:
//   - Mesure FPS (frames per second)
//   - Mesure latence MIDI (input → output)
//   - Utilisation mémoire (heap size)
//   - Temps de rendu Canvas
//   - Détection frame drops
//   - Alertes automatiques (seuils dépassés)
//   - Historique métriques (graphiques)
//   - Export rapport performance
//
// Architecture:
//   PerformanceMonitor (classe singleton)
//   - Sampling périodique (requestAnimationFrame)
//   - Buffer circulaire pour historique
//   - Performance API navigateur
//
// Auteur: MidiMind Team
// ============================================================================

/**
 * @class PerformanceMonitor
 * @description Surveillance et analyse des performances frontend
 * 
 * Métriques suivies:
 * - Latence requêtes backend (ms)
 * - Latence événements EventBus (ms)
 * - FPS de rendu
 * - Utilisation mémoire
 * - Temps de traitement
 * - Événements par seconde
 */
class PerformanceMonitor {
    constructor(eventBus, logger) {
        this.eventBus = eventBus;
        this.logger = logger;
        
        // Configuration
        this.config = {
            enabled: true,
            sampleInterval: 1000,      // 1 seconde
            historySize: 100,          // 100 échantillons
            latencyThresholdMs: 50,    // Alerte si > 50ms
            fpsThresholdLow: 30,       // Alerte si < 30 FPS
            memoryThresholdMb: 500     // Alerte si > 500 MB
        };
        
        // Métriques en temps réel
        this.metrics = {
            // Latence
            latency: {
                backend: [],
                eventBus: [],
                render: [],
                total: []
            },
            
            // FPS
            fps: {
                current: 60,
                history: [],
                frameCount: 0,
                lastFrameTime: 0
            },
            
            // Mémoire
            memory: {
                used: 0,
                total: 0,
                history: []
            },
            
            // Événements
            events: {
                processed: 0,
                dropped: 0,
                rate: 0,
                history: []
            },
            
            // Temps de traitement
            processing: {
                average: 0,
                max: 0,
                history: []
            }
        };
        
        // Measurements en cours
        this.measurements = new Map();
        
        // Alertes
        this.alerts = [];
        
        // Timers
        this.sampleTimer = null;
        this.fpsTimer = null;
        
        this.init();
    }
    
    // ========================================================================
    // INITIALISATION
    // ========================================================================
    
    init() {
        this.logger.info('PerformanceMonitor', 'Initializing Performance Monitor v3.0.0');
        
        // Démarrer le monitoring
        this.startMonitoring();
        
        // Écouter les événements critiques
        this.setupEventListeners();
        
        // Démarrer le compteur FPS
        this.startFpsCounter();
    }
    
    setupEventListeners() {
        // Mesurer latence des commandes backend
        this.eventBus.on('backend:command:sent', (data) => {
            this.startMeasure(`backend_${data.requestId}`, 'backend');
        });
        
        this.eventBus.on('backend:command:response', (data) => {
            this.endMeasure(`backend_${data.requestId}`);
        });
        
        // Mesurer latence EventBus
        this.eventBus.on('*', () => {
            // Hook pour mesurer tous les événements
        });
    }
    
    // ========================================================================
    // MONITORING
    // ========================================================================
    
    startMonitoring() {
        if (this.sampleTimer) return;
        
        this.sampleTimer = setInterval(() => {
            this.collectSample();
        }, this.config.sampleInterval);
        
        this.logger.info('PerformanceMonitor', 'Monitoring started');
    }
    
    stopMonitoring() {
        if (this.sampleTimer) {
            clearInterval(this.sampleTimer);
            this.sampleTimer = null;
        }
        
        if (this.fpsTimer) {
            cancelAnimationFrame(this.fpsTimer);
            this.fpsTimer = null;
        }
        
        this.logger.info('PerformanceMonitor', 'Monitoring stopped');
    }
    
    collectSample() {
        if (!this.config.enabled) return;
        
        // Collecter métriques mémoire
        this.updateMemoryMetrics();
        
        // Calculer taux d'événements
        this.updateEventRate();
        
        // Vérifier les seuils
        this.checkThresholds();
        
        // Nettoyer l'historique
        this.cleanupHistory();
    }
    
    // ========================================================================
    // MESURE DE LATENCE
    // ========================================================================
    
    /**
     * Démarre une mesure de temps
     * @param {string} id - Identifiant unique de la mesure
     * @param {string} type - Type de mesure (backend, eventBus, render)
     */
    startMeasure(id, type = 'generic') {
        const measurement = {
            id,
            type,
            startTime: performance.now()
        };
        
        this.measurements.set(id, measurement);
    }
    
    /**
     * Termine une mesure et enregistre la latence
     * @param {string} id - Identifiant de la mesure
     * @returns {number} Latence en millisecondes
     */
    endMeasure(id) {
        const measurement = this.measurements.get(id);
        
        if (!measurement) {
            this.logger.warn('PerformanceMonitor', `Measurement ${id} not found`);
            return 0;
        }
        
        const endTime = performance.now();
        const latency = endTime - measurement.startTime;
        
        // Enregistrer dans l'historique approprié
        this.recordLatency(measurement.type, latency);
        
        // Supprimer la mesure
        this.measurements.delete(id);
        
        return latency;
    }
    
    /**
     * Enregistre une latence
     */
    recordLatency(type, latency) {
        if (!this.metrics.latency[type]) {
            this.metrics.latency[type] = [];
        }
        
        const history = this.metrics.latency[type];
        history.push({
            value: latency,
            timestamp: Date.now()
        });
        
        // Limiter la taille
        if (history.length > this.config.historySize) {
            history.shift();
        }
        
        // Vérifier seuil
        if (latency > this.config.latencyThresholdMs) {
            this.addAlert('latency', `High ${type} latency: ${latency.toFixed(2)}ms`, 'warning');
        }
    }
    
    /**
     * Mesure une fonction
     * @param {Function} fn - Fonction à mesurer
     * @param {string} name - Nom de la mesure
     * @returns {*} Résultat de la fonction
     */
    async measureFunction(fn, name = 'anonymous') {
        const id = `func_${name}_${Date.now()}`;
        this.startMeasure(id, 'processing');
        
        try {
            const result = await fn();
            const latency = this.endMeasure(id);
            
            this.logger.debug('PerformanceMonitor', 
                `${name} took ${latency.toFixed(2)}ms`);
            
            return result;
        } catch (error) {
            this.endMeasure(id);
            throw error;
        }
    }
    
    // ========================================================================
    // FPS COUNTER
    // ========================================================================
    
    startFpsCounter() {
        let lastTime = performance.now();
        let frameCount = 0;
        
        const measureFps = () => {
            const currentTime = performance.now();
            frameCount++;
            
            // Calculer FPS chaque seconde
            if (currentTime - lastTime >= 1000) {
                this.metrics.fps.current = Math.round(
                    (frameCount * 1000) / (currentTime - lastTime)
                );
                
                // Enregistrer dans l'historique
                this.metrics.fps.history.push({
                    value: this.metrics.fps.current,
                    timestamp: Date.now()
                });
                
                // Limiter la taille
                if (this.metrics.fps.history.length > this.config.historySize) {
                    this.metrics.fps.history.shift();
                }
                
                // Vérifier seuil
                if (this.metrics.fps.current < this.config.fpsThresholdLow) {
                    this.addAlert('fps', `Low FPS: ${this.metrics.fps.current}`, 'warning');
                }
                
                // Reset
                frameCount = 0;
                lastTime = currentTime;
            }
            
            this.fpsTimer = requestAnimationFrame(measureFps);
        };
        
        measureFps();
    }
    
    // ========================================================================
    // MÉMOIRE
    // ========================================================================
    
    updateMemoryMetrics() {
        if (!performance.memory) {
            return;  // Pas disponible dans tous les navigateurs
        }
        
        const usedMb = performance.memory.usedJSHeapSize / (1024 * 1024);
        const totalMb = performance.memory.totalJSHeapSize / (1024 * 1024);
        
        this.metrics.memory.used = usedMb;
        this.metrics.memory.total = totalMb;
        
        this.metrics.memory.history.push({
            used: usedMb,
            total: totalMb,
            timestamp: Date.now()
        });
        
        // Limiter la taille
        if (this.metrics.memory.history.length > this.config.historySize) {
            this.metrics.memory.history.shift();
        }
        
        // Vérifier seuil
        if (usedMb > this.config.memoryThresholdMb) {
            this.addAlert('memory', `High memory usage: ${usedMb.toFixed(2)} MB`, 'warning');
        }
    }
    
    // ========================================================================
    // ÉVÉNEMENTS
    // ========================================================================
    
    updateEventRate() {
        // Récupérer stats EventBus
        const eventBusMetrics = this.eventBus.getMetrics();
        
        this.metrics.events.processed = eventBusMetrics.eventsProcessed;
        this.metrics.events.dropped = eventBusMetrics.eventsDropped;
        
        // Calculer rate (événements/seconde)
        const history = this.metrics.events.history;
        if (history.length > 0) {
            const lastSample = history[history.length - 1];
            const deltaEvents = this.metrics.events.processed - lastSample.processed;
            const deltaTime = (Date.now() - lastSample.timestamp) / 1000;
            
            this.metrics.events.rate = Math.round(deltaEvents / deltaTime);
        }
        
        history.push({
            processed: this.metrics.events.processed,
            dropped: this.metrics.events.dropped,
            timestamp: Date.now()
        });
        
        // Limiter la taille
        if (history.length > this.config.historySize) {
            history.shift();
        }
    }
    
    // ========================================================================
    // ALERTES
    // ========================================================================
    
    addAlert(type, message, level = 'info') {
        const alert = {
            type,
            message,
            level,
            timestamp: Date.now()
        };
        
        this.alerts.push(alert);
        
        // Limiter à 50 alertes
        if (this.alerts.length > 50) {
            this.alerts.shift();
        }
        
        // Logger
        const logMethod = level === 'error' ? 'error' : 'warn';
        this.logger[logMethod]('PerformanceMonitor', message);
        
        // Émettre événement
        this.eventBus.emitLow('performance:alert', alert);
    }
    
    checkThresholds() {
        // Vérifier latence moyenne
        const avgBackendLatency = this.getAverageLatency('backend');
        if (avgBackendLatency > this.config.latencyThresholdMs) {
            this.addAlert('latency', 
                `Average backend latency high: ${avgBackendLatency.toFixed(2)}ms`, 
                'warning');
        }
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * Calcule la latence moyenne pour un type
     */
    getAverageLatency(type) {
        const history = this.metrics.latency[type];
        if (!history || history.length === 0) return 0;
        
        const sum = history.reduce((acc, item) => acc + item.value, 0);
        return sum / history.length;
    }
    
    /**
     * Récupère toutes les métriques
     */
    getMetrics() {
        return {
            latency: {
                backend: {
                    current: this.getLastValue(this.metrics.latency.backend),
                    average: this.getAverageLatency('backend'),
                    history: this.metrics.latency.backend.slice(-20)
                },
                eventBus: {
                    current: this.getLastValue(this.metrics.latency.eventBus),
                    average: this.getAverageLatency('eventBus')
                },
                render: {
                    current: this.getLastValue(this.metrics.latency.render),
                    average: this.getAverageLatency('render')
                }
            },
            fps: {
                current: this.metrics.fps.current,
                history: this.metrics.fps.history.slice(-20)
            },
            memory: {
                used: this.metrics.memory.used,
                total: this.metrics.memory.total,
                history: this.metrics.memory.history.slice(-20)
            },
            events: {
                rate: this.metrics.events.rate,
                processed: this.metrics.events.processed,
                dropped: this.metrics.events.dropped
            },
            alerts: this.alerts.slice(-10)
        };
    }
    
    /**
     * Récupère un rapport formaté
     */
    getReport() {
        const metrics = this.getMetrics();
        
        return `
=== Performance Report ===
Latency (Backend): ${metrics.latency.backend.average.toFixed(2)}ms avg
FPS: ${metrics.fps.current} 
Memory: ${metrics.memory.used.toFixed(2)} MB / ${metrics.memory.total.toFixed(2)} MB
Event Rate: ${metrics.events.rate} events/sec
Alerts: ${this.alerts.length}
========================
        `.trim();
    }
    
    /**
     * Exporte en JSON
     */
    exportMetrics() {
        return JSON.stringify(this.getMetrics(), null, 2);
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    getLastValue(history) {
        if (!history || history.length === 0) return 0;
        return history[history.length - 1].value;
    }
    
    cleanupHistory() {
        // Supprimer les mesures non terminées anciennes (> 10s)
        const now = performance.now();
        
        for (const [id, measurement] of this.measurements.entries()) {
            if (now - measurement.startTime > 10000) {
                this.logger.warn('PerformanceMonitor', 
                    `Measurement ${id} timed out, removing`);
                this.measurements.delete(id);
            }
        }
    }
    
    /**
     * Réinitialise toutes les métriques
     */
    reset() {
        this.metrics = {
            latency: { backend: [], eventBus: [], render: [], total: [] },
            fps: { current: 60, history: [], frameCount: 0, lastFrameTime: 0 },
            memory: { used: 0, total: 0, history: [] },
            events: { processed: 0, dropped: 0, rate: 0, history: [] },
            processing: { average: 0, max: 0, history: [] }
        };
        
        this.alerts = [];
        this.measurements.clear();
        
        this.logger.info('PerformanceMonitor', 'Metrics reset');
    }
    
    /**
     * Affiche les métriques dans la console
     */
    logMetrics() {
        console.log(this.getReport());
    }
    
    /**
     * Nettoie les ressources
     */
    destroy() {
        this.stopMonitoring();
        this.measurements.clear();
        this.logger.info('PerformanceMonitor', 'Destroyed');
    }
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = PerformanceMonitor;
}
window.PerformanceMonitor = PerformanceMonitor;
// ============================================================================
// EXEMPLE D'UTILISATION
// ============================================================================

/*
// Initialisation
const perfMonitor = new PerformanceMonitor(eventBus, logger);

// Mesurer une opération
perfMonitor.startMeasure('load_file', 'backend');
await loadFile();
const latency = perfMonitor.endMeasure('load_file');
console.log(`File loaded in ${latency}ms`);

// Mesurer une fonction
const result = await perfMonitor.measureFunction(
    async () => await complexOperation(),
    'complex_operation'
);

// Obtenir les métriques
const metrics = perfMonitor.getMetrics();
console.log('Backend latency:', metrics.latency.backend.average);
console.log('FPS:', metrics.fps.current);

// Afficher un rapport
perfMonitor.logMetrics();

// Exporter les métriques
const json = perfMonitor.exportMetrics();
*/

// ============================================================================
// FIN DU FICHIER PerformanceMonitor.js
// ============================================================================

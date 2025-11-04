// ============================================================================
// Fichier: frontend/js/controllers/PerformanceController.js
// Projet: MidiMind v3.1.0 - SystÃƒÆ’Ã‚Â¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.1.0 - OPTIMISÃƒÆ’Ã¢â‚¬Â°
// Date: 2025-11-01
// ============================================================================
// Description:
//   ContrÃƒÆ’Ã‚Â´leur de monitoring et optimisation des performances de l'application.
//   Mesure FPS, latence, utilisation mÃƒÆ’Ã‚Â©moire, et dÃƒÆ’Ã‚Â©clenche alertes si dÃƒÆ’Ã‚Â©gradation.
//
// FonctionnalitÃƒÆ’Ã‚Â©s:
//   - Monitoring FPS temps rÃƒÆ’Ã‚Â©el
//   - Mesure latence MIDI (input ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ output)
//   - Utilisation mÃƒÆ’Ã‚Â©moire (heap size)
//   - Temps de rendu Canvas
//   - DÃƒÆ’Ã‚Â©tection ralentissements (frame drops)
//   - Alertes automatiques si seuils dÃƒÆ’Ã‚Â©passÃƒÆ’Ã‚Â©s
//   - Logs de performance
//   - Suggestions d'optimisation
//
// Architecture:
//   PerformanceController extends BaseController
//   - Utilise PerformanceMonitor (utils/)
//   - Sampling pÃƒÆ’Ã‚Â©riodique (requestAnimationFrame)
//   - Historique mÃƒÆ’Ã‚Â©triques (buffer circulaire)
//
// MODIFICATIONS v3.1.0:
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Constructeur conforme ÃƒÆ’Ã‚Â  BaseController
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Utilisation cohÃƒÆ’Ã‚Â©rente de subscribe() pour ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Gestion robuste des ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements backend
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ Optimisation de la collecte de mÃƒÆ’Ã‚Â©triques
//   ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ MÃƒÆ’Ã‚Â©thodes helper de BaseController
//
// Auteur: MidiMind Team
// ============================================================================

class PerformanceController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        // MÃƒÆ’Ã‚Â©triques de performance
        this.metrics = {
            renderTimes: [],
            eventCounts: {},
            memoryUsage: [],
            commandLatencies: [],
            startTime: Date.now(),
            lastRenderStart: Date.now()
        };
        
        // Configuration
        this.config = {
            ...this.config,  // HÃƒÆ’Ã‚Â©riter de BaseController
            monitoringInterval: 10000,     // 10 secondes
            cleanupInterval: 60000,         // 1 minute
            maxRenderSamples: 100,
            maxMemorySamples: 60,
            maxLatencySamples: 50,
            warnRenderTime: 100,            // ms
            warnMemoryUsage: 0.8,           // 80%
            maxEventTypes: 100
        };
        
        // ÃƒÆ’Ã¢â‚¬Â°tat
        this.isMonitoring = false;
        this.monitoringTimer = null;
        this.cleanupTimer = null;
    }
    
    /**
     * Initialisation du contrÃƒÆ’Ã‚Â´leur
     */
    onInitialize() {
        this.logDebug('info', 'Initializing performance controller...');
        this.startMonitoring();
        this.logDebug('info', 'Performance monitoring started');
    }
    
    /**
     * Bind des ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
     */
    bindEvents() {
        // ÃƒÆ’Ã¢â‚¬Â°vÃƒÆ’Ã‚Â©nements de rendu
        this.subscribe('view:rendered', (data) => {
            this.recordRenderTime(data.view, Date.now());
        });
        
        // ÃƒÆ’Ã¢â‚¬Â°vÃƒÆ’Ã‚Â©nements backend pour mesurer latence
        this.subscribe('backend:command:sent', (data) => {
            this.recordCommandStart(data.id, data.command);
        });
        
        this.subscribe('backend:command:response', (data) => {
            this.recordCommandEnd(data.id);
        });
        
        // ÃƒÆ’Ã¢â‚¬Â°vÃƒÆ’Ã‚Â©nements systÃƒÆ’Ã‚Â¨me
        this.subscribe('system:memory:warning', () => {
            this.handleMemoryWarning();
        });
        
        // Intercepter tous les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements pour compter (avec prÃƒÆ’Ã‚Â©caution)
        this.interceptEventBus();
    }
    
    /**
     * Intercepter l'EventBus pour compter les ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
     */
    interceptEventBus() {
        if (this.eventBus && typeof this.eventBus.emit === 'function') {
            const originalEmit = this.eventBus.emit.bind(this.eventBus);
            this.eventBus.emit = (event, data) => {
                this.recordEvent(event);
                return originalEmit(event, data);
            };
        }
    }
    
    /**
     * Enregistrer un temps de rendu
     */
    recordRenderTime(view, time) {
        const duration = time - this.metrics.lastRenderStart;
        
        this.metrics.renderTimes.push({
            view,
            time,
            duration
        });
        
        this.metrics.lastRenderStart = time;
        
        // Garder seulement les N derniers
        if (this.metrics.renderTimes.length > this.config.maxRenderSamples) {
            this.metrics.renderTimes = this.metrics.renderTimes.slice(-this.config.maxRenderSamples);
        }
        
        // Avertir si temps de rendu ÃƒÆ’Ã‚Â©levÃƒÆ’Ã‚Â©
        if (duration > this.config.warnRenderTime) {
            this.logDebug('warn', `Slow render detected: ${view} took ${duration.toFixed(1)}ms`);
        }
    }
    
    /**
     * Enregistrer le dÃƒÆ’Ã‚Â©but d'une commande
     */
    recordCommandStart(id, command) {
        if (!this.commandTimings) {
            this.commandTimings = new Map();
        }
        this.commandTimings.set(id, {
            command,
            startTime: Date.now()
        });
    }
    
    /**
     * Enregistrer la fin d'une commande
     */
    recordCommandEnd(id) {
        if (!this.commandTimings || !this.commandTimings.has(id)) return;
        
        const timing = this.commandTimings.get(id);
        const latency = Date.now() - timing.startTime;
        
        this.metrics.commandLatencies.push({
            command: timing.command,
            latency,
            time: Date.now()
        });
        
        // Garder seulement les N derniers
        if (this.metrics.commandLatencies.length > this.config.maxLatencySamples) {
            this.metrics.commandLatencies = this.metrics.commandLatencies.slice(-this.config.maxLatencySamples);
        }
        
        this.commandTimings.delete(id);
    }
    
    /**
     * Enregistrer un ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
     */
    recordEvent(event) {
        this.metrics.eventCounts[event] = (this.metrics.eventCounts[event] || 0) + 1;
    }
    
    /**
     * DÃƒÆ’Ã‚Â©marrer le monitoring
     */
    startMonitoring() {
        if (this.isMonitoring) return;
        
        this.isMonitoring = true;
        
        // Monitoring pÃƒÆ’Ã‚Â©riodique
        this.monitoringTimer = setInterval(() => {
            this.collectMetrics();
        }, this.config.monitoringInterval);
        
        // Nettoyage pÃƒÆ’Ã‚Â©riodique
        this.cleanupTimer = setInterval(() => {
            this.cleanupMetrics();
        }, this.config.cleanupInterval);
        
        this.logDebug('info', 'Performance monitoring started');
    }
    
    /**
     * ArrÃƒÆ’Ã‚Âªter le monitoring
     */
    stopMonitoring() {
        if (!this.isMonitoring) return;
        
        this.isMonitoring = false;
        
        if (this.monitoringTimer) {
            clearInterval(this.monitoringTimer);
            this.monitoringTimer = null;
        }
        
        if (this.cleanupTimer) {
            clearInterval(this.cleanupTimer);
            this.cleanupTimer = null;
        }
        
        this.logDebug('info', 'Performance monitoring stopped');
    }
    
    /**
     * Collecter les mÃƒÆ’Ã‚Â©triques
     */
    collectMetrics() {
        // MÃƒÆ’Ã‚Â©moire
        if (performance.memory) {
            const memory = performance.memory;
            this.metrics.memoryUsage.push({
                time: Date.now(),
                used: memory.usedJSHeapSize,
                total: memory.totalJSHeapSize,
                limit: memory.jsHeapSizeLimit
            });
            
            // Garder seulement les N derniÃƒÆ’Ã‚Â¨res mesures
            if (this.metrics.memoryUsage.length > this.config.maxMemorySamples) {
                this.metrics.memoryUsage = this.metrics.memoryUsage.slice(-this.config.maxMemorySamples);
            }
        }
        
        // DÃƒÆ’Ã‚Â©tecter les problÃƒÆ’Ã‚Â¨mes de performance
        this.detectPerformanceIssues();
    }
    
    /**
     * DÃƒÆ’Ã‚Â©tecter les problÃƒÆ’Ã‚Â¨mes de performance
     */
    detectPerformanceIssues() {
        // VÃƒÆ’Ã‚Â©rifier les temps de rendu
        if (this.metrics.renderTimes.length > 0) {
            const recentRenders = this.metrics.renderTimes.slice(-10);
            const avgRenderTime = recentRenders.reduce((sum, r) => sum + r.duration, 0) / recentRenders.length;
            
            if (avgRenderTime > this.config.warnRenderTime) {
                this.logDebug('warn', `Performance degraded: average render time ${avgRenderTime.toFixed(1)}ms`);
                this.emitEvent('performance:warning', {
                    type: 'render',
                    value: avgRenderTime,
                    threshold: this.config.warnRenderTime
                });
            }
        }
        
        // VÃƒÆ’Ã‚Â©rifier l'usage mÃƒÆ’Ã‚Â©moire
        if (this.metrics.memoryUsage.length > 0) {
            const lastMemory = this.metrics.memoryUsage[this.metrics.memoryUsage.length - 1];
            const usageRatio = lastMemory.used / lastMemory.total;
            
            if (usageRatio > this.config.warnMemoryUsage) {
                this.logDebug('warn', `High memory usage detected: ${(usageRatio * 100).toFixed(1)}%`);
                this.emitEvent('performance:warning', {
                    type: 'memory',
                    value: usageRatio,
                    threshold: this.config.warnMemoryUsage
                });
            }
        }
        
        // VÃƒÆ’Ã‚Â©rifier les latences de commandes
        if (this.metrics.commandLatencies.length > 0) {
            const recentLatencies = this.metrics.commandLatencies.slice(-10);
            const avgLatency = recentLatencies.reduce((sum, l) => sum + l.latency, 0) / recentLatencies.length;
            
            if (avgLatency > 1000) { // Plus de 1 seconde
                this.logDebug('warn', `High command latency detected: ${avgLatency.toFixed(0)}ms`);
            }
        }
    }
    
    /**
     * GÃƒÆ’Ã‚Â©rer un avertissement mÃƒÆ’Ã‚Â©moire
     */
    handleMemoryWarning() {
        this.logDebug('warn', 'Memory warning received, attempting optimization...');
        this.optimizePerformance();
    }
    
    /**
     * Obtenir un rapport de performance
     */
    getPerformanceReport() {
        const uptime = Date.now() - this.metrics.startTime;
        
        // Temps de rendu
        const recentRenders = this.metrics.renderTimes.slice(-20);
        const avgRenderTime = recentRenders.length > 0 
            ? recentRenders.reduce((sum, r) => sum + r.duration, 0) / recentRenders.length 
            : 0;
        
        // Top ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements
        const topEvents = Object.entries(this.metrics.eventCounts)
            .sort((a, b) => b[1] - a[1])
            .slice(0, 5);
        
        // Latence moyenne
        const recentLatencies = this.metrics.commandLatencies.slice(-20);
        const avgLatency = recentLatencies.length > 0
            ? recentLatencies.reduce((sum, l) => sum + l.latency, 0) / recentLatencies.length
            : 0;
        
        return {
            uptime: this.formatDuration(uptime / 1000),
            avgRenderTime: avgRenderTime.toFixed(1) + 'ms',
            avgCommandLatency: avgLatency.toFixed(0) + 'ms',
            totalEvents: Object.values(this.metrics.eventCounts).reduce((sum, count) => sum + count, 0),
            topEvents: topEvents.map(([event, count]) => `${event}: ${count}`),
            memoryTrend: this.getMemoryTrend(),
            sampleCounts: {
                renders: this.metrics.renderTimes.length,
                memory: this.metrics.memoryUsage.length,
                latencies: this.metrics.commandLatencies.length
            }
        };
    }
    
    /**
     * Obtenir la tendance mÃƒÆ’Ã‚Â©moire
     */
    getMemoryTrend() {
        if (this.metrics.memoryUsage.length < 2) return 'Insufficient';
        
        const recent = this.metrics.memoryUsage.slice(-5);
        const first = recent[0].used;
        const last = recent[recent.length - 1].used;
        
        if (last > first * 1.1) return 'Growing';
        if (last < first * 0.9) return 'Decreasing';
        return 'Stable';
    }
    
    /**
     * Optimiser les performances
     */
    optimizePerformance() {
        this.logDebug('info', 'Optimizing performance...');
        
        // Nettoyer les mÃƒÆ’Ã‚Â©triques anciennes
        this.cleanupMetrics();
        
        // Forcer le garbage collection si disponible
        if (window.gc) {
            window.gc();
            this.logDebug('info', 'Garbage collection forced');
        }
        
        // ÃƒÆ’Ã¢â‚¬Â°mettre ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nement
        this.emitEvent('performance:optimized');
        
        this.logDebug('info', 'Performance optimization completed');
        this.notify('success', 'Performance optimized');
    }
    
    /**
     * Nettoyer les mÃƒÆ’Ã‚Â©triques anciennes
     */
    cleanupMetrics() {
        const now = Date.now();
        const oneHourAgo = now - 3600000;
        
        // Nettoyer les anciennes donnÃƒÆ’Ã‚Â©es
        this.metrics.renderTimes = this.metrics.renderTimes.filter(r => r.time > oneHourAgo);
        this.metrics.memoryUsage = this.metrics.memoryUsage.filter(m => m.time > oneHourAgo);
        this.metrics.commandLatencies = this.metrics.commandLatencies.filter(l => l.time > oneHourAgo);
        
        // RÃƒÆ’Ã‚Â©initialiser les compteurs d'ÃƒÆ’Ã‚Â©vÃƒÆ’Ã‚Â©nements si trop nombreux
        if (Object.keys(this.metrics.eventCounts).length > this.config.maxEventTypes) {
            // Garder seulement les top 50
            const topEvents = Object.entries(this.metrics.eventCounts)
                .sort((a, b) => b[1] - a[1])
                .slice(0, 50);
            this.metrics.eventCounts = Object.fromEntries(topEvents);
        }
        
        this.logDebug('debug', 'Metrics cleanup completed');
    }
    
    /**
     * Formater une durÃƒÆ’Ã‚Â©e
     */
    formatDuration(seconds) {
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = Math.floor(seconds % 60);
        
        if (hours > 0) {
            return `${hours}h ${minutes}m ${secs}s`;
        } else if (minutes > 0) {
            return `${minutes}m ${secs}s`;
        } else {
            return `${secs}s`;
        }
    }
    
    /**
     * Obtenir les statistiques actuelles
     */
    getStats() {
        return {
            isMonitoring: this.isMonitoring,
            metrics: {
                renderSamples: this.metrics.renderTimes.length,
                memorySamples: this.metrics.memoryUsage.length,
                latencySamples: this.metrics.commandLatencies.length,
                eventTypes: Object.keys(this.metrics.eventCounts).length
            },
            report: this.getPerformanceReport()
        };
    }
    
    /**
     * RÃƒÆ’Ã‚Â©initialiser les mÃƒÆ’Ã‚Â©triques
     */
    resetMetrics() {
        this.metrics = {
            renderTimes: [],
            eventCounts: {},
            memoryUsage: [],
            commandLatencies: [],
            startTime: Date.now(),
            lastRenderStart: Date.now()
        };
        
        this.logDebug('info', 'Performance metrics reset');
        this.emitEvent('performance:reset');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PerformanceController;
}

if (typeof window !== 'undefined') {
    window.PerformanceController = PerformanceController;
}
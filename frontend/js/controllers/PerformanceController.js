// ============================================================================
// Fichier: frontend/js/controllers/PerformanceController.js
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   Contrôleur de monitoring et optimisation des performances de l'application.
//   Mesure FPS, latence, utilisation mémoire, et déclenche alertes si dégradation.
//
// Fonctionnalités:
//   - Monitoring FPS temps réel
//   - Mesure latence MIDI (input → output)
//   - Utilisation mémoire (heap size)
//   - Temps de rendu Canvas
//   - Détection ralentissements (frame drops)
//   - Alertes automatiques si seuils dépassés
//   - Logs de performance
//   - Suggestions d'optimisation
//
// Architecture:
//   PerformanceController extends BaseController
//   - Utilise PerformanceMonitor (utils/)
//   - Sampling périodique (requestAnimationFrame)
//   - Historique métriques (buffer circulaire)
//
// Auteur: MidiMind Team
// ============================================================================
        // ===== PERFORMANCE CONTROLLER =====
        class PerformanceController extends BaseController {
            constructor(eventBus, models, views, notifications, debugConsole) {
                super(eventBus, models, views, notifications, debugConsole);
                this.metrics = {
                    renderTimes: [],
                    eventCounts: {},
                    memoryUsage: [],
                    startTime: Date.now()
                };
                
                this.startMonitoring();
            }

            bindEvents() {
                this.eventBus.on('view:rendered', (data) => {
                    this.recordRenderTime(data.view, Date.now());
                });
                
                // Intercepter tous les événements pour compter
                const originalEmit = this.eventBus.emit;
                this.eventBus.emit = (event, data) => {
                    this.recordEvent(event);
                    return originalEmit.call(this.eventBus, event, data);
                };
            }

            recordRenderTime(view, time) {
                this.metrics.renderTimes.push({
                    view,
                    time,
                    duration: time - (this.metrics.lastRenderStart || time)
                });
                
                // Garder seulement les 100 derniers
                if (this.metrics.renderTimes.length > 100) {
                    this.metrics.renderTimes = this.metrics.renderTimes.slice(-100);
                }
            }

            recordEvent(event) {
                this.metrics.eventCounts[event] = (this.metrics.eventCounts[event] || 0) + 1;
            }

            startMonitoring() {
                // Monitoring toutes les 10 secondes
                setInterval(() => {
                    this.collectMetrics();
                }, 10000);
                
                // Nettoyage périodique
                setInterval(() => {
                    this.cleanupMetrics();
                }, 60000);
            }

            collectMetrics() {
                const memory = performance.memory;
                if (memory) {
                    this.metrics.memoryUsage.push({
                        time: Date.now(),
                        used: memory.usedJSHeapSize,
                        total: memory.totalJSHeapSize,
                        limit: memory.jsHeapSizeLimit
                    });
                    
                    // Garder seulement les 60 dernières mesures (10 minutes)
                    if (this.metrics.memoryUsage.length > 60) {
                        this.metrics.memoryUsage = this.metrics.memoryUsage.slice(-60);
                    }
                }
                
                // Détecter les problèmes de performance
                this.detectPerformanceIssues();
            }

            detectPerformanceIssues() {
                const recentRenders = this.metrics.renderTimes.slice(-10);
                const avgRenderTime = recentRenders.reduce((sum, r) => sum + r.duration, 0) / recentRenders.length;
                
                if (avgRenderTime > 100) { // Plus de 100ms
                    this.logDebug('system', `Performance dégradée: rendu moyen ${avgRenderTime.toFixed(1)}ms`);
                }
                
                // Vérifier l'usage mémoire
                const lastMemory = this.metrics.memoryUsage[this.metrics.memoryUsage.length - 1];
                if (lastMemory && lastMemory.used > lastMemory.total * 0.8) {
                    this.logDebug('system', 'Usage mémoire élevé détecté');
                }
            }

            getPerformanceReport() {
                const uptime = Date.now() - this.metrics.startTime;
                const recentRenders = this.metrics.renderTimes.slice(-20);
                const avgRenderTime = recentRenders.length > 0 
                    ? recentRenders.reduce((sum, r) => sum + r.duration, 0) / recentRenders.length 
                    : 0;
                
                const topEvents = Object.entries(this.metrics.eventCounts)
                    .sort((a, b) => b[1] - a[1])
                    .slice(0, 5);
                
                return {
                    uptime: this.formatDuration(uptime / 1000),
                    avgRenderTime: avgRenderTime.toFixed(1) + 'ms',
                    totalEvents: Object.values(this.metrics.eventCounts).reduce((sum, count) => sum + count, 0),
                    topEvents: topEvents.map(([event, count]) => `${event}: ${count}`),
                    memoryTrend: this.getMemoryTrend()
                };
            }

            getMemoryTrend() {
                if (this.metrics.memoryUsage.length < 2) return 'Insuffisant';
                
                const recent = this.metrics.memoryUsage.slice(-5);
                const avg = recent.reduce((sum, m) => sum + m.used, 0) / recent.length;
                const first = recent[0].used;
                const last = recent[recent.length - 1].used;
                
                if (last > first * 1.1) return 'Croissant';
                if (last < first * 0.9) return 'Décroissant';
                return 'Stable';
            }

            optimizePerformance() {
                // Nettoyer les métriques anciennes
                this.cleanupMetrics();
                
                // Forcer le garbage collection si disponible
                if (window.gc) {
                    window.gc();
                    this.logDebug('system', 'Garbage collection forcé');
                }
                
                // Optimiser les événements
                this.debounceEvents();
                
                this.logDebug('system', 'Optimisation des performances effectuée');
                this.showNotification('Performances optimisées', 'success');
            }

            cleanupMetrics() {
                // Nettoyer les métriques anciennes
                const now = Date.now();
                const oneHourAgo = now - 3600000;
                
                this.metrics.renderTimes = this.metrics.renderTimes.filter(r => r.time > oneHourAgo);
                this.metrics.memoryUsage = this.metrics.memoryUsage.filter(m => m.time > oneHourAgo);
                
                // Réinitialiser les compteurs d'événements périodiquement
                if (Object.keys(this.metrics.eventCounts).length > 100) {
                    this.metrics.eventCounts = {};
                }
            }

            debounceEvents() {
                // Implémenter un debouncing pour les événements fréquents
                // Cette méthode pourrait être étendue selon les besoins
            }

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
        }

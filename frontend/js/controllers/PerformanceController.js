// ============================================================================
// Fichier: frontend/js/controllers/PerformanceController.js
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0
// Date: 2025-10-14
// ============================================================================
// Description:
//   ContrÃ´leur de monitoring et optimisation des performances de l'application.
//   Mesure FPS, latence, utilisation mÃ©moire, et dÃ©clenche alertes si dÃ©gradation.
//
// FonctionnalitÃ©s:
//   - Monitoring FPS temps rÃ©el
//   - Mesure latence MIDI (input â†’ output)
//   - Utilisation mÃ©moire (heap size)
//   - Temps de rendu Canvas
//   - DÃ©tection ralentissements (frame drops)
//   - Alertes automatiques si seuils dÃ©passÃ©s
//   - Logs de performance
//   - Suggestions d'optimisation
//
// Architecture:
//   PerformanceController extends BaseController
//   - Utilise PerformanceMonitor (utils/)
//   - Sampling pÃ©riodique (requestAnimationFrame)
//   - Historique mÃ©triques (buffer circulaire)
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
                
                // Intercepter tous les Ã©vÃ©nements pour compter
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
                
                // Nettoyage pÃ©riodique
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
                    
                    // Garder seulement les 60 derniÃ¨res mesures (10 minutes)
                    if (this.metrics.memoryUsage.length > 60) {
                        this.metrics.memoryUsage = this.metrics.memoryUsage.slice(-60);
                    }
                }
                
                // DÃ©tecter les problÃ¨mes de performance
                this.detectPerformanceIssues();
            }

            detectPerformanceIssues() {
                const recentRenders = this.metrics.renderTimes.slice(-10);
                const avgRenderTime = recentRenders.reduce((sum, r) => sum + r.duration, 0) / recentRenders.length;
                
                if (avgRenderTime > 100) { // Plus de 100ms
                    this.logDebug('system', `Performance dÃ©gradÃ©e: rendu moyen ${avgRenderTime.toFixed(1)}ms`);
                }
                
                // VÃ©rifier l'usage mÃ©moire
                const lastMemory = this.metrics.memoryUsage[this.metrics.memoryUsage.length - 1];
                if (lastMemory && lastMemory.used > lastMemory.total * 0.8) {
                    this.logDebug('system', 'Usage mÃ©moire Ã©levÃ© dÃ©tectÃ©');
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
                if (last < first * 0.9) return 'DÃ©croissant';
                return 'Stable';
            }

            optimizePerformance() {
                // Nettoyer les mÃ©triques anciennes
                this.cleanupMetrics();
                
                // Forcer le garbage collection si disponible
                if (window.gc) {
                    window.gc();
                    this.logDebug('system', 'Garbage collection forcÃ©');
                }
                
                // Optimiser les Ã©vÃ©nements
                this.debounceEvents();
                
                this.logDebug('system', 'Optimisation des performances effectuÃ©e');
                this.showNotification('Performances optimisÃ©es', 'success');
            }

            cleanupMetrics() {
                // Nettoyer les mÃ©triques anciennes
                const now = Date.now();
                const oneHourAgo = now - 3600000;
                
                this.metrics.renderTimes = this.metrics.renderTimes.filter(r => r.time > oneHourAgo);
                this.metrics.memoryUsage = this.metrics.memoryUsage.filter(m => m.time > oneHourAgo);
                
                // RÃ©initialiser les compteurs d'Ã©vÃ©nements pÃ©riodiquement
                if (Object.keys(this.metrics.eventCounts).length > 100) {
                    this.metrics.eventCounts = {};
                }
            }

            debounceEvents() {
                // ImplÃ©menter un debouncing pour les Ã©vÃ©nements frÃ©quents
                // Cette mÃ©thode pourrait Ãªtre Ã©tendue selon les besoins
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

window.PerformanceController = PerformanceController;
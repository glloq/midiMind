// ============================================================================
// Fichier: frontend/js/controllers/LatencyController.js
// Version: v1.0.1 - FIXED BACKEND SIGNATURE
// Date: 2025-10-28
// ============================================================================
// CORRECTIONS v1.0.1:
// ✓ CRITIQUE: Ajout paramètre backend au constructeur (6ème paramètre)
// ✓ Fix: super() appelle BaseController avec backend
// ✓ this.backend initialisé automatiquement via BaseController
// ============================================================================
// ============================================================================
// Description:
//   Contrôleur pour gérer la compensation de latence
//   - Configuration compensation par instrument
//   - Gestion offset global
//   - Activation/désactivation
//   - Monitoring des latences
// ============================================================================

class LatencyController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        this.backendService = backendService;
        this.latencyView = latencyView;
        
        // État de la latence
        this.state.latency = {
            enabled: false,
            globalOffset: 0,
            instruments: [],
            selectedInstrument: null
        };
        
        // Configuration
        this.config.autoRefresh = true;
        this.config.refreshInterval = 10000; // 10 secondes
        
        // Timer de rafraîchissement
        this.refreshTimer = null;
    }
    
    /**
     * Initialisation personnalisée
     */
    onInitialize() {
        this.logDebug('info', 'LatencyController initializing...');
        
        // Charger les données initiales
        this.loadLatencySettings();
        
        // Démarrer le rafraîchissement automatique si configuré
        if (this.config.autoRefresh) {
            this.startAutoRefresh();
        }
    }
    
    /**
     * Liaison des événements
     */
    bindEvents() {
        // Événements de la vue
        this.subscribe('latency:enable', () => this.enableCompensation());
        this.subscribe('latency:disable', () => this.disableCompensation());
        this.subscribe('latency:set-global-offset', (data) => this.setGlobalOffset(data.offset));
        this.subscribe('latency:set-compensation', (data) => {
            this.setInstrumentCompensation(data.instrumentId, data.offset);
        });
        this.subscribe('latency:refresh', () => this.loadLatencySettings());
        this.subscribe('latency:select-instrument', (data) => {
            this.selectInstrument(data.instrumentId);
        });
        
        // Événements backend
        this.subscribe('backend:connected', () => {
            this.loadLatencySettings();
        });
    }
    
    /**
     * Charge les paramètres de latence
     */
    async loadLatencySettings() {
        return this.executeAction('loadLatencySettings', async () => {
            try {
                // Charger l'offset global
                const globalResponse = await this.backendService.sendCommand('latency.getGlobalOffset', {});
                
                if (globalResponse.success) {
                    this.state.latency.globalOffset = globalResponse.data.offset_ms || 0;
                }
                
                // Charger la liste des instruments
                const instrumentsResponse = await this.backendService.sendCommand('latency.listInstruments', {});
                
                if (instrumentsResponse.success) {
                    this.state.latency.instruments = instrumentsResponse.data.instruments || [];
                }
                
                // Mettre à jour la vue
                this.updateView({
                    globalOffset: this.state.latency.globalOffset,
                    instruments: this.state.latency.instruments
                });
                
                this.emitEvent('latency:settings:loaded', {
                    globalOffset: this.state.latency.globalOffset,
                    instruments: this.state.latency.instruments
                });
                
                return {
                    globalOffset: this.state.latency.globalOffset,
                    instruments: this.state.latency.instruments
                };
                
            } catch (error) {
                this.handleError('Erreur lors du chargement des paramètres de latence', error);
                throw error;
            }
        });
    }
    
    /**
     * Active la compensation de latence
     */
    async enableCompensation() {
        return this.executeAction('enableCompensation', async () => {
            try {
                const response = await this.backendService.sendCommand('latency.enable', {});
                
                if (response.success) {
                    this.state.latency.enabled = true;
                    
                    this.showNotification('Compensation de latence activée', 'success');
                    
                    this.updateView({
                        enabled: true
                    });
                    
                    this.emitEvent('latency:enabled');
                }
                
                return response;
            } catch (error) {
                this.handleError('Erreur lors de l\'activation de la compensation', error);
                throw error;
            }
        });
    }
    
    /**
     * Désactive la compensation de latence
     */
    async disableCompensation() {
        return this.executeAction('disableCompensation', async () => {
            try {
                const response = await this.backendService.sendCommand('latency.disable', {});
                
                if (response.success) {
                    this.state.latency.enabled = false;
                    
                    this.showNotification('Compensation de latence désactivée', 'success');
                    
                    this.updateView({
                        enabled: false
                    });
                    
                    this.emitEvent('latency:disabled');
                }
                
                return response;
            } catch (error) {
                this.handleError('Erreur lors de la désactivation de la compensation', error);
                throw error;
            }
        });
    }
    
    /**
     * Définit l'offset global
     */
    async setGlobalOffset(offsetMs) {
        return this.executeAction('setGlobalOffset', async (data) => {
            try {
                const response = await this.backendService.sendCommand('latency.setGlobalOffset', {
                    offset_ms: data.offsetMs
                });
                
                if (response.success) {
                    this.state.latency.globalOffset = data.offsetMs;
                    
                    this.showNotification(
                        `Offset global défini à ${data.offsetMs.toFixed(1)} ms`,
                        'success'
                    );
                    
                    this.updateView({
                        globalOffset: data.offsetMs
                    });
                    
                    this.emitEvent('latency:global-offset:updated', {
                        offset: data.offsetMs
                    });
                }
                
                return response;
            } catch (error) {
                this.handleError('Erreur lors de la définition de l\'offset global', error);
                throw error;
            }
        }, { offsetMs });
    }
    
    /**
     * Définit la compensation pour un instrument
     */
    async setInstrumentCompensation(instrumentId, offsetMs) {
        return this.executeAction('setInstrumentCompensation', async (data) => {
            try {
                const response = await this.backendService.sendCommand('latency.setCompensation', {
                    instrument_id: data.instrumentId,
                    offset_ms: data.offsetMs
                });
                
                if (response.success) {
                    // Mettre à jour l'instrument dans la liste
                    const instrument = this.state.latency.instruments.find(
                        i => i.instrument_id === data.instrumentId
                    );
                    
                    if (instrument) {
                        instrument.compensation_offset_us = data.offsetMs * 1000; // Convert ms to us
                    }
                    
                    this.showNotification(
                        `Compensation pour ${data.instrumentId} définie à ${data.offsetMs.toFixed(1)} ms`,
                        'success'
                    );
                    
                    this.updateView({
                        instruments: this.state.latency.instruments
                    });
                    
                    this.emitEvent('latency:instrument:updated', {
                        instrumentId: data.instrumentId,
                        offset: data.offsetMs
                    });
                }
                
                return response;
            } catch (error) {
                this.handleError(
                    `Erreur lors de la définition de la compensation pour ${instrumentId}`,
                    error
                );
                throw error;
            }
        }, { instrumentId, offsetMs });
    }
    
    /**
     * Obtient la compensation d'un instrument
     */
    async getInstrumentCompensation(instrumentId) {
        return this.executeAction('getInstrumentCompensation', async (data) => {
            try {
                const response = await this.backendService.sendCommand('latency.getCompensation', {
                    instrument_id: data.instrumentId
                });
                
                if (response.success) {
                    const offsetMs = (response.data.compensation_offset_us || 0) / 1000;
                    
                    this.emitEvent('latency:compensation:loaded', {
                        instrumentId: data.instrumentId,
                        offset: offsetMs
                    });
                    
                    return offsetMs;
                }
                
                return 0;
            } catch (error) {
                this.logDebug('error', `Erreur lors de la récupération de la compensation: ${error.message}`);
                return 0;
            }
        }, { instrumentId });
    }
    
    /**
     * Sélectionne un instrument
     */
    selectInstrument(instrumentId) {
        this.state.latency.selectedInstrument = instrumentId;
        
        const instrument = this.state.latency.instruments.find(
            i => i.instrument_id === instrumentId
        );
        
        if (instrument) {
            this.updateView({
                selectedInstrument: instrument
            });
            
            this.emitEvent('latency:instrument:selected', {
                instrument
            });
        }
    }
    
    /**
     * Démarre le rafraîchissement automatique
     */
    startAutoRefresh() {
        this.stopAutoRefresh();
        
        this.refreshTimer = setInterval(() => {
            this.loadLatencySettings();
        }, this.config.refreshInterval);
        
        this.logDebug('info', 'Auto-refresh started');
    }
    
    /**
     * Arrête le rafraîchissement automatique
     */
    stopAutoRefresh() {
        if (this.refreshTimer) {
            clearInterval(this.refreshTimer);
            this.refreshTimer = null;
            this.logDebug('info', 'Auto-refresh stopped');
        }
    }
    
    /**
     * Met à jour la vue
     */
    updateView(data) {
        if (this.latencyView && typeof this.latencyView.render === 'function') {
            this.latencyView.render(data);
        }
    }
    
    /**
     * Afficher une notification
     */
    showNotification(message, type = 'info') {
        if (this.notifications && typeof this.notifications.show === 'function') {
            this.notifications.show(message, type);
        } else {
            this.logDebug(type, message);
        }
    }
    
    /**
     * Obtenir l'état actuel
     */
    getLatencyState() {
        return {
            ...this.state.latency,
            instrumentsCount: this.state.latency.instruments.length,
            averageLatency: this.calculateAverageLatency()
        };
    }
    
    /**
     * Calcule la latence moyenne de tous les instruments
     */
    calculateAverageLatency() {
        if (this.state.latency.instruments.length === 0) {
            return 0;
        }
        
        const sum = this.state.latency.instruments.reduce((acc, instrument) => {
            return acc + (instrument.avg_latency_us || 0);
        }, 0);
        
        return sum / this.state.latency.instruments.length / 1000; // Convert to ms
    }
    
    /**
     * Nettoyage lors de la destruction
     */
    destroy() {
        this.stopAutoRefresh();
        super.destroy();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = LatencyController;
}

if (typeof window !== 'undefined') {
    window.LatencyController = LatencyController;
}
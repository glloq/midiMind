// ============================================================================
// Fichier: frontend/js/views/LatencyView.js
// Version: v1.0.0
// Date: 2025-10-28
// ============================================================================
// Description:
//   Vue pour l'interface de calibration de latence
//   - Contrôles de compensation par instrument
//   - Configuration offset global
//   - Statistiques de latence
//   - Activation/désactivation
// ============================================================================

class LatencyView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        this.config.name = 'LatencyView';
        
        // Données de la vue
        this.data = {
            enabled: false,
            globalOffset: 0,
            instruments: [],
            selectedInstrument: null
        };
    }
    
    /**
     * Initialisation de la vue
     */
    onInitialize() {
        this.logDebug('info', 'LatencyView initialized');
    }
    
    /**
     * Construit le template HTML
     */
    buildTemplate() {
        return `
            <div class="latency-container">
                ${this.buildHeader()}
                ${this.buildGlobalSettings()}
                ${this.buildInstrumentsList()}
                ${this.buildStatistics()}
            </div>
        `;
    }
    
    /**
     * Construit l'en-tête
     */
    buildHeader() {
        const statusClass = this.data.enabled ? 'status-active' : 'status-inactive';
        const statusText = this.data.enabled ? 'Activée' : 'Désactivée';
        
        return `
            <div class="latency-header">
                <h2>
                    <i class="icon-clock"></i>
                    Compensation de Latence
                </h2>
                <div class="latency-status ${statusClass}">
                    <span class="status-indicator"></span>
                    ${statusText}
                </div>
            </div>
        `;
    }
    
    /**
     * Construit les paramètres globaux
     */
    buildGlobalSettings() {
        return `
            <div class="latency-global card">
                <h3>Paramètres Globaux</h3>
                
                <div class="global-controls">
                    <div class="control-group">
                        <label class="toggle-label">
                            <input type="checkbox" 
                                   id="latency-enable" 
                                   ${this.data.enabled ? 'checked' : ''}>
                            <span>Activer la compensation</span>
                        </label>
                    </div>
                    
                    <div class="control-group">
                        <label for="global-offset">Offset Global (ms):</label>
                        <div class="slider-container">
                            <input type="range" 
                                   id="global-offset" 
                                   min="-100" 
                                   max="100" 
                                   step="0.1" 
                                   value="${this.data.globalOffset}">
                            <input type="number" 
                                   id="global-offset-value" 
                                   min="-100" 
                                   max="100" 
                                   step="0.1" 
                                   value="${this.data.globalOffset}">
                            <span class="unit">ms</span>
                        </div>
                        <div class="slider-hint">
                            Décalage appliqué à tous les instruments
                        </div>
                    </div>
                    
                    <button class="btn btn-primary" id="apply-global">
                        <i class="icon-check"></i>
                        Appliquer
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Construit la liste des instruments
     */
    buildInstrumentsList() {
        return `
            <div class="latency-instruments card">
                <div class="instruments-header">
                    <h3>Compensation par Instrument</h3>
                    <button class="btn btn-sm" id="refresh-instruments">
                        <i class="icon-refresh"></i>
                        Actualiser
                    </button>
                </div>
                
                <div class="instruments-list">
                    ${this.data.instruments.length === 0
                        ? '<p class="no-instruments">Aucun instrument configuré.</p>'
                        : this.data.instruments.map(instrument => this.buildInstrumentItem(instrument)).join('')
                    }
                </div>
            </div>
        `;
    }
    
    /**
     * Construit un élément d'instrument
     */
    buildInstrumentItem(instrument) {
        const avgLatencyMs = (instrument.avg_latency_us || 0) / 1000;
        const compensationMs = (instrument.compensation_offset_us || 0) / 1000;
        const measurementCount = instrument.measurement_count || 0;
        const autoCalibration = instrument.auto_calibration || false;
        
        const statusClass = autoCalibration ? 'instrument-auto' : 'instrument-manual';
        const selectedClass = this.data.selectedInstrument?.instrument_id === instrument.instrument_id 
            ? 'instrument-selected' 
            : '';
        
        return `
            <div class="instrument-item ${statusClass} ${selectedClass}" 
                 data-instrument-id="${instrument.instrument_id}">
                <div class="instrument-header">
                    <div class="instrument-info">
                        <div class="instrument-name">
                            ${this.escapeHTML(instrument.instrument_id)}
                        </div>
                        <div class="instrument-stats">
                            <span class="stat">
                                <i class="icon-clock"></i>
                                Latence moy: ${avgLatencyMs.toFixed(2)} ms
                            </span>
                            <span class="stat">
                                <i class="icon-counter"></i>
                                ${measurementCount} mesures
                            </span>
                        </div>
                    </div>
                    ${autoCalibration 
                        ? '<span class="badge badge-auto">Auto</span>'
                        : '<span class="badge badge-manual">Manuel</span>'
                    }
                </div>
                
                <div class="instrument-controls">
                    <label>Compensation (ms):</label>
                    <div class="slider-container">
                        <input type="range" 
                               class="instrument-offset" 
                               data-instrument-id="${instrument.instrument_id}"
                               min="-50" 
                               max="50" 
                               step="0.1" 
                               value="${compensationMs}">
                        <input type="number" 
                               class="instrument-offset-value"
                               data-instrument-id="${instrument.instrument_id}"
                               min="-50" 
                               max="50" 
                               step="0.1" 
                               value="${compensationMs}">
                        <span class="unit">ms</span>
                    </div>
                    
                    <button class="btn btn-sm btn-apply-instrument" 
                            data-instrument-id="${instrument.instrument_id}">
                        <i class="icon-check"></i>
                        Appliquer
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Construit les statistiques
     */
    buildStatistics() {
        const avgLatency = this.calculateAverageLatency();
        const totalMeasurements = this.calculateTotalMeasurements();
        const instrumentsWithAuto = this.countAutoCalibration();
        
        return `
            <div class="latency-statistics card">
                <h3>Statistiques</h3>
                
                <div class="stats-grid">
                    <div class="stat-item">
                        <div class="stat-label">Instruments</div>
                        <div class="stat-value">${this.data.instruments.length}</div>
                    </div>
                    
                    <div class="stat-item">
                        <div class="stat-label">Latence Moyenne</div>
                        <div class="stat-value">${avgLatency.toFixed(2)} ms</div>
                    </div>
                    
                    <div class="stat-item">
                        <div class="stat-label">Mesures Totales</div>
                        <div class="stat-value">${totalMeasurements}</div>
                    </div>
                    
                    <div class="stat-item">
                        <div class="stat-label">Auto-calibration</div>
                        <div class="stat-value">${instrumentsWithAuto} / ${this.data.instruments.length}</div>
                    </div>
                </div>
            </div>
        `;
    }
    
    /**
     * Calcule la latence moyenne
     */
    calculateAverageLatency() {
        if (this.data.instruments.length === 0) return 0;
        
        const sum = this.data.instruments.reduce((acc, instrument) => {
            return acc + (instrument.avg_latency_us || 0);
        }, 0);
        
        return sum / this.data.instruments.length / 1000;
    }
    
    /**
     * Calcule le nombre total de mesures
     */
    calculateTotalMeasurements() {
        return this.data.instruments.reduce((acc, instrument) => {
            return acc + (instrument.measurement_count || 0);
        }, 0);
    }
    
    /**
     * Compte les instruments avec auto-calibration
     */
    countAutoCalibration() {
        return this.data.instruments.filter(i => i.auto_calibration).length;
    }
    
    /**
     * Attache les événements DOM
     */
    attachEvents() {
        // Activer/désactiver la compensation
        this.addDOMEventListener('#latency-enable', 'change', (e) => {
            if (e.target.checked) {
                this.emit('latency:enable');
            } else {
                this.emit('latency:disable');
            }
        });
        
        // Synchroniser le slider et l'input numérique pour l'offset global
        this.addDOMEventListener('#global-offset', 'input', (e) => {
            const value = parseFloat(e.target.value);
            document.getElementById('global-offset-value').value = value;
        });
        
        this.addDOMEventListener('#global-offset-value', 'input', (e) => {
            const value = parseFloat(e.target.value);
            document.getElementById('global-offset').value = value;
        });
        
        // Appliquer l'offset global
        this.addDOMEventListener('#apply-global', 'click', () => {
            const offset = parseFloat(document.getElementById('global-offset').value);
            this.emit('latency:set-global-offset', { offset });
        });
        
        // Actualiser la liste des instruments
        this.addDOMEventListener('#refresh-instruments', 'click', () => {
            this.emit('latency:refresh');
        });
        
        // Synchroniser les sliders et inputs pour chaque instrument
        this.addDOMEventListener('.instrument-offset', 'input', (e) => {
            const value = parseFloat(e.target.value);
            const instrumentId = e.target.dataset.instrumentId;
            const valueInput = this.container.querySelector(
                `.instrument-offset-value[data-instrument-id="${instrumentId}"]`
            );
            if (valueInput) {
                valueInput.value = value;
            }
        }, true);
        
        this.addDOMEventListener('.instrument-offset-value', 'input', (e) => {
            const value = parseFloat(e.target.value);
            const instrumentId = e.target.dataset.instrumentId;
            const slider = this.container.querySelector(
                `.instrument-offset[data-instrument-id="${instrumentId}"]`
            );
            if (slider) {
                slider.value = value;
            }
        }, true);
        
        // Appliquer la compensation pour un instrument
        this.addDOMEventListener('.btn-apply-instrument', 'click', (e) => {
            const instrumentId = e.target.closest('.btn-apply-instrument').dataset.instrumentId;
            const slider = this.container.querySelector(
                `.instrument-offset[data-instrument-id="${instrumentId}"]`
            );
            
            if (slider) {
                const offset = parseFloat(slider.value);
                this.emit('latency:set-compensation', {
                    instrumentId,
                    offset
                });
            }
        }, true);
        
        // Sélectionner un instrument
        this.addDOMEventListener('.instrument-item', 'click', (e) => {
            if (!e.target.closest('.instrument-controls')) {
                const instrumentId = e.target.closest('.instrument-item').dataset.instrumentId;
                this.emit('latency:select-instrument', { instrumentId });
            }
        }, true);
    }
    
    /**
     * Cache les éléments DOM
     */
    cacheElements() {
        this.elements = {
            enableCheckbox: this.container.querySelector('#latency-enable'),
            globalOffsetSlider: this.container.querySelector('#global-offset'),
            globalOffsetValue: this.container.querySelector('#global-offset-value'),
            instrumentsList: this.container.querySelector('.instruments-list')
        };
    }
    
    /**
     * Affiche un message d'erreur
     */
    showError(message) {
        const errorDiv = document.createElement('div');
        errorDiv.className = 'latency-error alert alert-danger';
        errorDiv.textContent = message;
        
        if (this.container) {
            this.container.insertBefore(errorDiv, this.container.firstChild);
            
            setTimeout(() => {
                errorDiv.remove();
            }, 5000);
        }
    }
    
    /**
     * Obtient l'état de la vue
     */
    getViewState() {
        return {
            ...this.getState(),
            instrumentsCount: this.data.instruments.length,
            averageLatency: this.calculateAverageLatency(),
            totalMeasurements: this.calculateTotalMeasurements()
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = LatencyView;
}

if (typeof window !== 'undefined') {
    window.LatencyView = LatencyView;
}
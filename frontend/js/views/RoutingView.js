// ============================================================================
// Fichier: frontend/js/views/RoutingView.js
// Version: v3.1.0 - SIMPLIFIED
// Date: 2025-10-16
// Projet: MidiMind v3.0 - SystÃ¨me d'Orchestration MIDI
// ============================================================================
// MODIFICATIONS v3.1.0:
// âœ“ Interface simplifiÃ©e (pas de matrice complexe)
// âœ“ Mode simple uniquement (1â†’1)
// âœ“ Stats visibles
// âœ“ Actions basiques
// ============================================================================

class RoutingView extends BaseView {
    constructor(container, eventBus, debugConsole) {
        super(container, eventBus, debugConsole, {
            name: 'RoutingView',
            autoRender: false
        });
        
        // Composants
        this.routingMatrix = null;
        
        // DonnÃ©es
        this.channels = [];
        this.instruments = [];
        this.routes = [];
        this.presets = [];
        
        // Logger initialization
        this.logger = window.logger || console;
        
        // Mark as fully initialized
        this._fullyInitialized = true;
        
        if (this.logger && this.logger.info) {
            this.logger.info('RoutingView', 'âœ“ RoutingView initialized (simple mode)');
        }
        
        // Now that all properties are set, do initial render
        if (this.container) {
            this.render();
        }
    }
    
    // Safe logging helper
    logDebug(category, message, level = 'debug') {
        if (!this.logger) {
            console.log(`[${category}] ${message}`);
            return;
        }
        
        // Map level to logger method
        const logMethod = level === 'warn' ? 'warn' : 
                         level === 'error' ? 'error' : 
                         level === 'info' ? 'info' : 'debug';
        
        if (typeof this.logger[logMethod] === 'function') {
            this.logger[logMethod](category, message);
        } else {
            console.log(`[${category}] ${message}`);
        }
    }
    
    // Override initialize to prevent BaseView auto-render before properties are ready
    initialize() {
        // This is called by BaseView constructor before our properties are set
        // Only render if we've finished our own constructor
        if (this._fullyInitialized && this.container) {
            this.render();
        }
    }
    
    // ========================================================================
    // RENDERING PRINCIPAL
    // ========================================================================
    
    render() {
        if (!this.container) return;
        
        // VÃ©rifier mode performance
        if (!PerformanceConfig.routing.allowComplexRouting) {
            this.renderSimpleRouting();
        } else {
            // Mode avancÃ© (dÃ©sactivÃ© par dÃ©faut)
            this.renderAdvancedRouting();
        }
    }
    
    // ========================================================================
    // MODE SIMPLE (1â†’1)
    // ========================================================================
    
    renderSimpleRouting() {
        const html = `
            <div class="routing-page-simple">
                <!-- Header -->
                <div class="page-header">
                    <h2>ðŸ”€ Routage MIDI (Mode Simple)</h2>
                    <p class="page-description">
                        Assignez chaque canal MIDI Ã  un instrument (routing 1â†’1)
                    </p>
                </div>
                
                <!-- Info Banner -->
                <div class="info-banner">
                    â„¹ï¸ Mode performance : Routing simple uniquement (1 canal â†’ 1 instrument)
                </div>
                
                <!-- Stats Cards -->
                <div class="routing-stats">
                    <div class="stat-card">
                        <div class="stat-icon">ðŸŽ¹</div>
                        <div class="stat-content">
                            <span class="stat-label">Canaux actifs</span>
                            <span class="stat-value" id="stat-channels">0</span>
                        </div>
                    </div>
                    
                    <div class="stat-card">
                        <div class="stat-icon">ðŸŽ¸</div>
                        <div class="stat-content">
                            <span class="stat-label">Instruments connectÃ©s</span>
                            <span class="stat-value" id="stat-instruments">0</span>
                        </div>
                    </div>
                    
                    <div class="stat-card">
                        <div class="stat-icon">ðŸ”—</div>
                        <div class="stat-content">
                            <span class="stat-label">Routes actives</span>
                            <span class="stat-value" id="stat-routes">0</span>
                        </div>
                    </div>
                    
                    <div class="stat-card">
                        <div class="stat-icon">ðŸ“Š</div>
                        <div class="stat-content">
                            <span class="stat-label">Canaux non assignÃ©s</span>
                            <span class="stat-value" id="stat-unassigned">0</span>
                        </div>
                    </div>
                </div>
                
                <!-- Matrice de routage (liste simple) -->
                <div class="routing-matrix-container" id="routing-matrix-container">
                    <!-- RoutingMatrix component ici -->
                </div>
                
                <!-- Actions -->
                <div class="routing-actions-panel">
                    <div class="actions-group">
                        <h4>Actions</h4>
                        <button class="btn btn-secondary" id="btn-refresh-routing">
                            ðŸ”„ RafraÃ®chir
                        </button>
                        <button class="btn btn-secondary" id="btn-export-routing">
                            ðŸ’¾ Exporter
                        </button>
                        <button class="btn btn-secondary" id="btn-import-routing">
                            ðŸ“‚ Importer
                        </button>
                    </div>
                    
                    <div class="actions-group">
                        <h4>Presets</h4>
                        <select class="preset-select" id="preset-select">
                            <option value="">-- SÃ©lectionner preset --</option>
                        </select>
                        <button class="btn btn-secondary" id="btn-save-preset">
                            ðŸ’¾ Sauvegarder preset
                        </button>
                        <button class="btn btn-secondary" id="btn-load-preset">
                            ðŸ“‚ Charger preset
                        </button>
                    </div>
                </div>
            </div>
        `;
        
        this.container.innerHTML = html;
        
        // Initialiser matrice simple
        this.initRoutingMatrix();
        
        // Attacher listeners
        this.attachEventListeners();
        
        // Mettre Ã  jour stats
        this.updateStats();
    }
    
    // ========================================================================
    // MODE AVANCÃ‰ (dÃ©sactivÃ© par dÃ©faut)
    // ========================================================================
    
    renderAdvancedRouting() {
        const html = `
            <div class="routing-page-advanced">
                <div class="info-banner warning">
                    âš ï¸ Mode avancÃ© dÃ©sactivÃ© en mode performance
                </div>
                <p>Pour activer le mode avancÃ©, modifiez PerformanceConfig.routing.allowComplexRouting</p>
            </div>
        `;
        
        this.container.innerHTML = html;
    }
    
    // ========================================================================
    // INITIALISATION MATRICE
    // ========================================================================
    
    initRoutingMatrix() {
        const matrixContainer = this.container.querySelector('#routing-matrix-container');
        
        if (!matrixContainer) {
            this.logDebug('routing', 'Matrix container not found', 'warn');
            return;
        }
        
        // Check if RoutingMatrix class is available
        if (typeof RoutingMatrix === 'undefined' || !window.RoutingMatrix) {
            this.logDebug('routing', 'RoutingMatrix class not loaded - showing fallback', 'warn');
            matrixContainer.innerHTML = `
                <div class="info-banner warning">
                    âš ï¸ RoutingMatrix component not loaded. Please ensure RoutingMatrix.js is included.
                </div>
                <div class="routing-list-fallback">
                    <h4>Routing Configuration</h4>
                    <p>The routing matrix component is not available. Please check your script loading order.</p>
                </div>
            `;
            return;
        }
        
        // CrÃ©er composant RoutingMatrix
        this.routingMatrix = new RoutingMatrix(matrixContainer, {
            mode: 'simple',
            channels: this.channels,
            instruments: this.instruments,
            onRouteChange: (channel, instrumentId) => {
                this.handleRouteChange(channel, instrumentId);
            },
            onTestRoute: (channel, instrumentId) => {
                this.handleTestRoute(channel, instrumentId);
            }
        });
        
        this.logDebug('routing', 'Matrix initialized');
    }
    
    // ========================================================================
    // EVENT LISTENERS
    // ========================================================================
    
    attachEventListeners() {
        // Refresh
        const refreshBtn = this.container.querySelector('#btn-refresh-routing');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.refresh();
            });
        }
        
        // Export
        const exportBtn = this.container.querySelector('#btn-export-routing');
        if (exportBtn) {
            exportBtn.addEventListener('click', () => {
                this.exportRouting();
            });
        }
        
        // Import
        const importBtn = this.container.querySelector('#btn-import-routing');
        if (importBtn) {
            importBtn.addEventListener('click', () => {
                this.importRouting();
            });
        }
        
        // Save preset
        const savePresetBtn = this.container.querySelector('#btn-save-preset');
        if (savePresetBtn) {
            savePresetBtn.addEventListener('click', () => {
                this.savePreset();
            });
        }
        
        // Load preset
        const loadPresetBtn = this.container.querySelector('#btn-load-preset');
        if (loadPresetBtn) {
            loadPresetBtn.addEventListener('click', () => {
                this.loadPreset();
            });
        }
    }
    
    // ========================================================================
    // HANDLERS
    // ========================================================================
    
    handleRouteChange(channel, instrumentId) {
        this.logDebug('routing', `Route changed: CH${channel} â†’ ${instrumentId || 'none'}`);
        
        // Ã‰mettre Ã©vÃ©nement
        this.eventBus.emit('routing:route-changed', {
            channel,
            instrumentId
        });
        
        // Mettre Ã  jour stats
        this.updateStats();
    }
    
    handleTestRoute(channel, instrumentId) {
        this.logDebug('routing', `Testing route: CH${channel} â†’ ${instrumentId}`);
        
        // Ã‰mettre Ã©vÃ©nement de test
        this.eventBus.emit('routing:test-route', {
            channel,
            instrumentId
        });
    }
    
    // ========================================================================
    // ACTIONS
    // ========================================================================
    
    refresh() {
        this.logDebug('routing', 'Refreshing routing view');
        
        // RÃ©initialiser matrice avec donnÃ©es actuelles
        if (this.routingMatrix) {
            this.routingMatrix.setChannels(this.channels);
            this.routingMatrix.setInstruments(this.instruments);
        }
        
        this.updateStats();
    }
    
    exportRouting() {
        if (!this.routingMatrix) {
            alert('Aucune route Ã  exporter');
            return;
        }
        
        const routes = this.routingMatrix.getRoutes();
        
        const data = {
            version: '3.1.0',
            type: 'simple_routing',
            routes: routes,
            exported: new Date().toISOString()
        };
        
        // TÃ©lÃ©charger fichier JSON
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `routing_${Date.now()}.json`;
        a.click();
        URL.revokeObjectURL(url);
        
        this.logDebug('routing', 'Routing exported');
    }
    
    importRouting() {
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.json';
        
        input.onchange = (e) => {
            const file = e.target.files[0];
            if (!file) return;
            
            const reader = new FileReader();
            reader.onload = (event) => {
                try {
                    const data = JSON.parse(event.target.result);
                    
                    if (data.routes && this.routingMatrix) {
                        this.routingMatrix.setRoutes(data.routes);
                        this.logDebug('routing', 'Routing imported');
                        alert('Routage importÃ© avec succÃ¨s');
                    }
                } catch (error) {
                    alert('Erreur lors de l\'import: ' + error.message);
                }
            };
            reader.readAsText(file);
        };
        
        input.click();
    }
    
    savePreset() {
        if (!this.routingMatrix) {
            alert('Aucune route Ã  sauvegarder');
            return;
        }
        
        const name = prompt('Nom du preset:');
        if (!name) return;
        
        const routes = this.routingMatrix.getRoutes();
        
        const preset = {
            id: `preset_${Date.now()}`,
            name: name,
            routes: routes,
            created: new Date().toISOString()
        };
        
        this.presets.push(preset);
        
        // Mettre Ã  jour dropdown
        this.updatePresetsDropdown();
        
        // Ã‰mettre Ã©vÃ©nement
        this.eventBus.emit('routing:preset-saved', { preset });
        
        this.logDebug('routing', `Preset saved: ${name}`);
        alert('Preset sauvegardÃ©');
    }
    
    loadPreset() {
        const select = this.container.querySelector('#preset-select');
        const presetId = select?.value;
        
        if (!presetId) {
            alert('SÃ©lectionnez un preset');
            return;
        }
        
        const preset = this.presets.find(p => p.id === presetId);
        
        if (!preset) {
            alert('Preset non trouvÃ©');
            return;
        }
        
        if (this.routingMatrix) {
            this.routingMatrix.setRoutes(preset.routes);
            
            // Ã‰mettre Ã©vÃ©nement
            this.eventBus.emit('routing:preset-loaded', { preset });
            
            this.logDebug('routing', `Preset loaded: ${preset.name}`);
            alert('Preset chargÃ©');
        }
    }
    
    // ========================================================================
    // UPDATE DATA
    // ========================================================================
    
    setChannels(channels) {
        this.channels = channels || [];
        
        if (this.routingMatrix) {
            this.routingMatrix.setChannels(this.channels);
        }
        
        this.updateStats();
    }
    
    setInstruments(instruments) {
        this.instruments = instruments || [];
        
        if (this.routingMatrix) {
            this.routingMatrix.setInstruments(this.instruments);
        }
        
        this.updateStats();
    }
    
    setRoutes(routes) {
        this.routes = routes || [];
        
        if (this.routingMatrix) {
            this.routingMatrix.setRoutes(this.routes);
        }
        
        this.updateStats();
    }
    
    setPresets(presets) {
        this.presets = presets || [];
        this.updatePresetsDropdown();
    }
    
    // ========================================================================
    // STATS
    // ========================================================================
    
    updateStats() {
        // Safety checks for uninitialized properties
        if (!this.channels) this.channels = [];
        if (!this.instruments) this.instruments = [];
        
        const activeChannels = this.routingMatrix ? 
            this.routingMatrix.getActiveChannels().length : 
            0;
        
        const connectedInstruments = this.instruments.filter(i => i && i.connected).length;
        
        const activeRoutes = this.routingMatrix ? 
            this.routingMatrix.getRoutes().length : 
            0;
        
        const unassigned = this.channels.length - activeChannels;
        
        // Mettre Ã  jour UI
        this.updateStatValue('stat-channels', this.channels.length);
        this.updateStatValue('stat-instruments', connectedInstruments);
        this.updateStatValue('stat-routes', activeRoutes);
        this.updateStatValue('stat-unassigned', unassigned);
    }
    
    updateStatValue(id, value) {
        const elem = this.container.querySelector(`#${id}`);
        if (elem) {
            elem.textContent = value;
        }
    }
    
    updatePresetsDropdown() {
        const select = this.container.querySelector('#preset-select');
        if (!select) return;
        
        select.innerHTML = '<option value="">-- SÃ©lectionner preset --</option>';
        
        this.presets.forEach(preset => {
            const option = document.createElement('option');
            option.value = preset.id;
            option.textContent = preset.name;
            select.appendChild(option);
        });
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        if (this.routingMatrix) {
            this.routingMatrix.destroy();
            this.routingMatrix = null;
        }
        
        super.destroy();
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = RoutingView;
}

if (typeof window !== 'undefined') {
    window.RoutingView = RoutingView;
}
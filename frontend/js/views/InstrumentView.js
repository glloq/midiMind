// ============================================================================
// Fichier: frontend/js/views/InstrumentView.js
// Version: v3.0.8 - FIXED (Initialization order corrected)
// Date: 2025-10-20
// ============================================================================
// VERSION Ã‰QUILIBRÃ‰E - Features utiles sans complexitÃ© excessive
// FIX: Proper initialization order to prevent undefined displayConfig error
// ============================================================================

class InstrumentView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        // Configure to prevent auto-render during construction
        this.config.autoRender = false;
        
        // Ã‰tat local
        this.localState = {
            selectedInstruments: new Set(),
            expandedInstruments: new Set(),
            displayMode: 'normal' // normal, compact, detailed
        };
        
        // Configuration
        this.displayConfig = {
            compactMode: false,
            showMetrics: true,
            showCapabilities: true
        };
        
        // Couleurs de connexion
        this.connectionColors = {
            'usb': '#3498db',
            'bluetooth': '#9b59b6',
            'network': '#1abc9c',
            'virtual': '#95a5a6'
        };
        
        // Logger safe - MUST be initialized before initialize() is called
        this.logger = window.logger || console;
        
        this.logger.info('InstrumentView', 'âœ“ View initialized (balanced version)');
    }
    
    // Override initialize to prevent BaseView from calling render before properties are set
    initialize() {
        // Don't call super.initialize() - it would call render() with autoRender
        // Properties are already initialized in constructor
        // Just do the initial render now that everything is ready
        if (this.container) {
            this.render();
        }
    }
    
    // ========================================================================
    // RENDU PRINCIPAL
    // ========================================================================
    
    render(data = {}) {
        if (!this.container) {
            if (this.logger) {
                this.logger.warning('InstrumentView', 'Container not found');
            }
            return;
        }
        
        // Safety check: ensure displayConfig exists
        if (!this.displayConfig) {
            if (this.logger) {
                this.logger.warning('InstrumentView', 'displayConfig not initialized yet, skipping render');
            }
            return;
        }
        
        const instruments = data.instruments || [];
        
        const html = `
            <div class="instrument-view">
                
                <!-- Header avec actions -->
                <div class="instrument-header">
                    <h2>ðŸŽ¹ MIDI Instruments</h2>
                    <div class="header-actions">
                        <button id="scan-instruments-btn" class="btn btn-primary">
                            ðŸ”„ Scan Devices
                        </button>
                        <button id="toggle-view-btn" class="btn btn-secondary">
                            ${this.displayConfig.compactMode ? 'ðŸ“‹ Normal View' : 'ðŸ“Š Compact View'}
                        </button>
                    </div>
                </div>
                
                <!-- Statistiques -->
                ${this.renderStats(instruments)}
                
                <!-- Liste des instruments -->
                <div class="instrument-list ${this.displayConfig.compactMode ? 'compact' : ''}">
                    ${instruments.length === 0 ? 
                        this.renderEmptyState() :
                        instruments.map(inst => this.renderInstrumentCard(inst)).join('')
                    }
                </div>
                
            </div>
        `;
        
        this.container.innerHTML = html;
        this.attachEventListeners();
        
        if (this.logger) {
            this.logger.debug('InstrumentView', `Rendered ${instruments.length} instruments`);
        }
    }
    
    // ========================================================================
    // RENDU STATISTIQUES
    // ========================================================================
    
    renderStats(instruments) {
        const total = instruments.length;
        const connected = instruments.filter(i => i.connected).length;
        const disconnected = total - connected;
        
        return `
            <div class="instrument-stats">
                <div class="stat-card">
                    <span class="stat-icon">ðŸ“Š</span>
                    <div class="stat-info">
                        <span class="stat-value">${total}</span>
                        <span class="stat-label">Total</span>
                    </div>
                </div>
                <div class="stat-card connected">
                    <span class="stat-icon">ðŸŸ¢</span>
                    <div class="stat-info">
                        <span class="stat-value">${connected}</span>
                        <span class="stat-label">Connected</span>
                    </div>
                </div>
                <div class="stat-card disconnected">
                    <span class="stat-icon">ðŸ”´</span>
                    <div class="stat-info">
                        <span class="stat-value">${disconnected}</span>
                        <span class="stat-label">Disconnected</span>
                    </div>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU CARTE INSTRUMENT
    // ========================================================================
    
    renderInstrumentCard(instrument) {
        const isSelected = this.localState.selectedInstruments.has(instrument.id);
        const isExpanded = this.localState.expandedInstruments.has(instrument.id);
        const connectionColor = this.connectionColors[instrument.type] || '#95a5a6';
        
        const cardClasses = [
            'instrument-card',
            instrument.connected ? 'connected' : 'disconnected',
            isSelected ? 'selected' : '',
            isExpanded ? 'expanded' : ''
        ].filter(Boolean).join(' ');
        
        return `
            <div class="${cardClasses}" 
                 data-instrument-id="${instrument.id}"
                 style="border-left: 4px solid ${connectionColor}">
                
                ${this.renderCardHeader(instrument)}
                
                ${isExpanded ? this.renderCardBody(instrument) : ''}
                
                ${this.renderCardFooter(instrument)}
                
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU HEADER CARTE
    // ========================================================================
    
    renderCardHeader(instrument) {
        const statusIcon = instrument.connected ? 'ðŸŸ¢' : 'ðŸ”´';
        const statusText = instrument.connected ? 'Connected' : 'Disconnected';
        
        return `
            <div class="card-header">
                <div class="instrument-info">
                    <div class="info-row">
                        <span class="status-indicator">${statusIcon}</span>
                        <h3 class="instrument-name">${this.escapeHtml(instrument.name)}</h3>
                    </div>
                    <div class="info-row details">
                        <span class="instrument-type">${instrument.type || 'MIDI'}</span>
                        ${instrument.manufacturer ? 
                            `<span class="instrument-manufacturer">${this.escapeHtml(instrument.manufacturer)}</span>` :
                            ''
                        }
                        <span class="instrument-status">${statusText}</span>
                    </div>
                </div>
                
                <div class="card-actions">
                    <button class="btn-icon expand-btn" 
                            data-id="${instrument.id}"
                            title="Show details">
                        ${this.localState.expandedInstruments.has(instrument.id) ? 'â–¼' : 'â–¶'}
                    </button>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU BODY CARTE (DÃ‰TAILS)
    // ========================================================================
    
    renderCardBody(instrument) {
        return `
            <div class="card-body">
                
                <!-- Informations dÃ©taillÃ©es -->
                <div class="detailed-info">
                    ${instrument.model ? 
                        `<div class="info-item">
                            <strong>Model:</strong> ${this.escapeHtml(instrument.model)}
                        </div>` : ''
                    }
                    ${instrument.id ? 
                        `<div class="info-item">
                            <strong>ID:</strong> <code>${this.escapeHtml(instrument.id)}</code>
                        </div>` : ''
                    }
                    ${instrument.port ? 
                        `<div class="info-item">
                            <strong>Port:</strong> ${this.escapeHtml(instrument.port)}
                        </div>` : ''
                    }
                </div>
                
                <!-- Capabilities -->
                ${this.renderCapabilities(instrument)}
                
                <!-- Metrics -->
                ${this.renderMetrics(instrument)}
                
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU CAPABILITIES
    // ========================================================================
    
    renderCapabilities(instrument) {
        if (!this.displayConfig.showCapabilities || !instrument.capabilities) {
            return '';
        }
        
        const caps = instrument.capabilities;
        
        return `
            <div class="capabilities">
                <h4>Capabilities</h4>
                <div class="capability-tags">
                    ${caps.input ? '<span class="cap-tag">ðŸ“¥ Input</span>' : ''}
                    ${caps.output ? '<span class="cap-tag">ðŸ“¤ Output</span>' : ''}
                    ${caps.clock ? '<span class="cap-tag">ðŸ• Clock</span>' : ''}
                    ${caps.program ? '<span class="cap-tag">ðŸŽ›ï¸ Program</span>' : ''}
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU METRICS
    // ========================================================================
    
    renderMetrics(instrument) {
        if (!this.displayConfig.showMetrics || !instrument.metrics) {
            return '';
        }
        
        const metrics = instrument.metrics;
        
        return `
            <div class="metrics">
                <h4>Metrics</h4>
                <div class="metric-grid">
                    ${metrics.notesReceived !== undefined ? 
                        `<div class="metric">
                            <span class="metric-label">Notes Received:</span>
                            <span class="metric-value">${metrics.notesReceived}</span>
                        </div>` : ''
                    }
                    ${metrics.notesSent !== undefined ? 
                        `<div class="metric">
                            <span class="metric-label">Notes Sent:</span>
                            <span class="metric-value">${metrics.notesSent}</span>
                        </div>` : ''
                    }
                    ${metrics.latency !== undefined ? 
                        `<div class="metric">
                            <span class="metric-label">Latency:</span>
                            <span class="metric-value">${metrics.latency}ms</span>
                        </div>` : ''
                    }
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU FOOTER CARTE
    // ========================================================================
    
    renderCardFooter(instrument) {
        return `
            <div class="card-footer">
                ${instrument.connected ? 
                    `<button class="btn btn-sm btn-danger disconnect-btn" 
                            data-id="${instrument.id}">
                        ðŸ”Œ Disconnect
                    </button>` : 
                    `<button class="btn btn-sm btn-success connect-btn" 
                            data-id="${instrument.id}">
                        ðŸ”Œ Connect
                    </button>`
                }
                ${instrument.connected ? 
                    `<button class="btn btn-sm btn-secondary config-btn" 
                            data-id="${instrument.id}">
                        âš™ï¸ Configure
                    </button>` : ''
                }
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU EMPTY STATE
    // ========================================================================
    
    renderEmptyState() {
        return `
            <div class="empty-state">
                <div class="empty-icon">ðŸŽ¹</div>
                <h3>No MIDI Instruments Found</h3>
                <p>Connect a MIDI device and click "Scan Devices" to detect it.</p>
                <button id="scan-empty-btn" class="btn btn-primary">
                    ðŸ”„ Scan Now
                </button>
            </div>
        `;
    }
    
    // ========================================================================
    // Ã‰VÃ‰NEMENTS
    // ========================================================================
    
    attachEventListeners() {
        // Bouton scan
        const scanBtn = this.container.querySelector('#scan-instruments-btn');
        if (scanBtn) {
            scanBtn.addEventListener('click', () => {
                this.emit('instrument:scan-requested');
            });
        }
        
        // Bouton scan empty state
        const scanEmptyBtn = this.container.querySelector('#scan-empty-btn');
        if (scanEmptyBtn) {
            scanEmptyBtn.addEventListener('click', () => {
                this.emit('instrument:scan-requested');
            });
        }
        
        // Toggle view mode
        const toggleViewBtn = this.container.querySelector('#toggle-view-btn');
        if (toggleViewBtn) {
            toggleViewBtn.addEventListener('click', () => {
                this.displayConfig.compactMode = !this.displayConfig.compactMode;
                this.emit('instrument:view-mode-changed', { 
                    compact: this.displayConfig.compactMode 
                });
                // Re-render needed
            });
        }
        
        // Boutons expand
        const expandBtns = this.container.querySelectorAll('.expand-btn');
        expandBtns.forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.stopPropagation();
                const instrumentId = btn.getAttribute('data-id');
                this.toggleExpand(instrumentId);
            });
        });
        
        // Boutons connect
        const connectBtns = this.container.querySelectorAll('.connect-btn');
        connectBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const instrumentId = btn.getAttribute('data-id');
                this.emit('instrument:connect-requested', { instrumentId });
            });
        });
        
        // Boutons disconnect
        const disconnectBtns = this.container.querySelectorAll('.disconnect-btn');
        disconnectBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const instrumentId = btn.getAttribute('data-id');
                this.emit('instrument:disconnect-requested', { instrumentId });
            });
        });
        
        // Boutons config
        const configBtns = this.container.querySelectorAll('.config-btn');
        configBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const instrumentId = btn.getAttribute('data-id');
                this.emit('instrument:configure-requested', { instrumentId });
            });
        });
    }
    
    // ========================================================================
    // INTERACTIONS
    // ========================================================================
    
    toggleExpand(instrumentId) {
        if (this.localState.expandedInstruments.has(instrumentId)) {
            this.localState.expandedInstruments.delete(instrumentId);
        } else {
            this.localState.expandedInstruments.add(instrumentId);
        }
        
        // Re-render la carte
        const card = this.container.querySelector(`[data-instrument-id="${instrumentId}"]`);
        if (card) {
            // Trouver l'instrument dans les donnÃ©es
            this.emit('instrument:expand-toggled', { instrumentId });
        }
    }
    
    selectInstrument(instrumentId) {
        this.localState.selectedInstruments.add(instrumentId);
        this.emit('instrument:selected', { instrumentId });
    }
    
    deselectInstrument(instrumentId) {
        this.localState.selectedInstruments.delete(instrumentId);
        this.emit('instrument:deselected', { instrumentId });
    }
    
    // ========================================================================
    // MISES Ã€ JOUR
    // ========================================================================
    
    updateInstrumentList(instruments) {
        this.render({ instruments });
    }
    
    updateInstrumentStatus(instrumentId, connected) {
        const card = this.container.querySelector(`[data-instrument-id="${instrumentId}"]`);
        
        if (card) {
            if (connected) {
                card.classList.remove('disconnected');
                card.classList.add('connected');
            } else {
                card.classList.remove('connected');
                card.classList.add('disconnected');
            }
        }
    }
    
    showScanProgress() {
        const scanBtn = this.container.querySelector('#scan-instruments-btn');
        if (scanBtn) {
            scanBtn.disabled = true;
            scanBtn.innerHTML = 'â³ Scanning...';
        }
    }
    
    hideScanProgress() {
        const scanBtn = this.container.querySelector('#scan-instruments-btn');
        if (scanBtn) {
            scanBtn.disabled = false;
            scanBtn.innerHTML = 'ðŸ”„ Scan Devices';
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    escapeHtml(text) {
        if (!text) return '';
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    clear() {
        if (this.container) {
            this.container.innerHTML = '<p class="loading">Loading instruments...</p>';
        }
    }
    
    showError(message) {
        if (this.container) {
            this.container.innerHTML = `
                <div class="error-message">
                    <p>âŒ ${this.escapeHtml(message)}</p>
                    <button id="retry-scan-btn" class="btn btn-primary">Retry Scan</button>
                </div>
            `;
            
            const retryBtn = this.container.querySelector('#retry-scan-btn');
            if (retryBtn) {
                retryBtn.addEventListener('click', () => {
                    this.emit('instrument:scan-requested');
                });
            }
        }
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = InstrumentView;
}

if (typeof window !== 'undefined') {
    window.InstrumentView = InstrumentView;
}
// ============================================================================
// Fichier: frontend/js/views/InstrumentView.js
// Version: v3.0.7 - BALANCED (More features, works perfectly)
// Date: 2025-10-19
// ============================================================================
// VERSION ÉQUILIBRÉE - Features utiles sans complexité excessive
// ============================================================================

class InstrumentView extends BaseView {
    constructor(containerId, eventBus) {
        // IMPORTANT: Initialize properties BEFORE calling super()
        // BaseView's constructor calls initialize() which calls render()
        // So these properties must exist before super() is called
        
        // État local
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
        
        // Now call super() after properties are initialized
        super(containerId, eventBus);
        
        // Couleurs de connexion
        this.connectionColors = {
            'usb': '#3498db',
            'bluetooth': '#9b59b6',
            'network': '#1abc9c',
            'virtual': '#95a5a6'
        };
        
        // Logger safe
        this.logger = window.Logger || console;
        
        this.logger.info('InstrumentView', '✓ View initialized (balanced version)');
    }
    
    // ========================================================================
    // RENDU PRINCIPAL
    // ========================================================================
    
    render(data = {}) {
        if (!this.container) {
            this.logger.warn('InstrumentView', 'Container not found');
            return;
        }
        
        const instruments = data.instruments || [];
        
        const html = `
            <div class="instrument-view">
                
                <!-- Header avec actions -->
                <div class="instrument-header">
                    <h2>🎹 MIDI Instruments</h2>
                    <div class="header-actions">
                        <button id="scan-instruments-btn" class="btn btn-primary">
                            🔄 Scan Devices
                        </button>
                        <button id="toggle-view-btn" class="btn btn-secondary">
                            ${this.displayConfig.compactMode ? '📋 Normal View' : '📊 Compact View'}
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
                    <span class="stat-icon">📊</span>
                    <div class="stat-info">
                        <span class="stat-value">${total}</span>
                        <span class="stat-label">Total</span>
                    </div>
                </div>
                <div class="stat-card connected">
                    <span class="stat-icon">🟢</span>
                    <div class="stat-info">
                        <span class="stat-value">${connected}</span>
                        <span class="stat-label">Connected</span>
                    </div>
                </div>
                <div class="stat-card disconnected">
                    <span class="stat-icon">🔴</span>
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
        const statusIcon = instrument.connected ? '🟢' : '🔴';
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
                        ${this.localState.expandedInstruments.has(instrument.id) ? '▼' : '▶'}
                    </button>
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU BODY CARTE (DÉTAILS)
    // ========================================================================
    
    renderCardBody(instrument) {
        return `
            <div class="card-body">
                
                <!-- Informations détaillées -->
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
                    ${instrument.ports ? 
                        `<div class="info-item">
                            <strong>Ports:</strong> 
                            In: ${instrument.ports.input || 0}, 
                            Out: ${instrument.ports.output || 0}
                        </div>` : ''
                    }
                </div>
                
                <!-- Capacités -->
                ${this.displayConfig.showCapabilities ? this.renderCapabilities(instrument) : ''}
                
                <!-- Métriques -->
                ${this.displayConfig.showMetrics && instrument.connected ? 
                    this.renderMetrics(instrument) : ''
                }
                
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU CAPACITÉS
    // ========================================================================
    
    renderCapabilities(instrument) {
        const capabilities = [];
        
        if (instrument.midiChannels) {
            capabilities.push(`🎛️ ${instrument.midiChannels} channels`);
        }
        
        if (instrument.sysexCapable) {
            capabilities.push('💾 SysEx');
        }
        
        if (instrument.velocitySensitive) {
            capabilities.push('🎹 Velocity');
        }
        
        if (instrument.aftertouch) {
            capabilities.push('👆 Aftertouch');
        }
        
        if (capabilities.length === 0) {
            return '';
        }
        
        return `
            <div class="capabilities">
                <strong>Capabilities:</strong>
                <div class="capability-badges">
                    ${capabilities.map(cap => `<span class="badge">${cap}</span>`).join('')}
                </div>
            </div>
        `;
    }
    
    // ========================================================================
    // RENDU MÉTRIQUES
    // ========================================================================
    
    renderMetrics(instrument) {
        if (!instrument.latency && !instrument.messagesReceived) {
            return '';
        }
        
        return `
            <div class="metrics">
                ${instrument.latency ? 
                    `<div class="metric-item">
                        <span class="metric-label">⚡ Latency:</span>
                        <span class="metric-value">${instrument.latency.toFixed(1)} ms</span>
                    </div>` : ''
                }
                ${instrument.messagesReceived ? 
                    `<div class="metric-item">
                        <span class="metric-label">📨 Messages:</span>
                        <span class="metric-value">${instrument.messagesReceived}</span>
                    </div>` : ''
                }
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
                    `<button class="btn btn-small btn-danger disconnect-btn" 
                             data-id="${instrument.id}">
                        Disconnect
                    </button>` :
                    `<button class="btn btn-small btn-primary connect-btn" 
                             data-id="${instrument.id}">
                        Connect
                    </button>`
                }
                
                ${instrument.connected && instrument.sysexCapable ?
                    `<button class="btn btn-small btn-secondary config-btn" 
                             data-id="${instrument.id}">
                        ⚙️ Configure
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
                <div class="empty-icon">🎹</div>
                <h3>No MIDI Instruments Found</h3>
                <p>Connect a MIDI device and click "Scan Devices" to detect it.</p>
                <button id="scan-empty-btn" class="btn btn-primary">
                    🔄 Scan Now
                </button>
            </div>
        `;
    }
    
    // ========================================================================
    // ÉVÉNEMENTS
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
            // Trouver l'instrument dans les données
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
    // MISES À JOUR
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
            scanBtn.innerHTML = '⏳ Scanning...';
        }
    }
    
    hideScanProgress() {
        const scanBtn = this.container.querySelector('#scan-instruments-btn');
        if (scanBtn) {
            scanBtn.disabled = false;
            scanBtn.innerHTML = '🔄 Scan Devices';
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
                    <p>❌ ${this.escapeHtml(message)}</p>
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
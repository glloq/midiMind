// ============================================================================
// Fichier: frontend/js/views/InstrumentView.js
// Version: v3.0.9 - FIXED LOGGER INITIALIZATION
// Date: 2025-10-24
// ============================================================================
// CORRECTIONS v3.0.9:
// ✅ CRITIQUE: Fixed logger initialization - create instance not class reference
// ✅ CRITIQUE: Override initialize() to prevent premature render
// ✅ CRITIQUE: Initialize logger in initialize() before any log calls
// ✅ Proper fallback to console if Logger not available
// ============================================================================
// VERSION ÉQUILIBRÉE - Features utiles sans complexité excessive
// FIX: Proper initialization order to prevent undefined displayConfig error
// ============================================================================

class InstrumentView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        // Configure to prevent auto-render during construction
        this.config.autoRender = false;
        this.config.name = 'InstrumentView';
        
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
        
        // Couleurs de connexion
        this.connectionColors = {
            'usb': '#3498db',
            'bluetooth': '#9b59b6',
            'network': '#1abc9c',
            'virtual': '#95a5a6'
        };
        
        // Logger - sera initialisé dans initialize()
        this.logger = null;
    }
    
    /**
     * OVERRIDE de BaseView.initialize()
     * Appelée par BaseView constructor AVANT que notre constructor soit fini
     */
    initialize() {
        // CRITIQUE: Initialiser le logger ICI, avant toute utilisation
        if (!this.logger) {
            if (typeof window !== 'undefined' && window.Logger) {
                // Créer une INSTANCE de Logger, pas juste une référence à la classe
                try {
                    this.logger = new window.Logger({
                        level: 'info',
                        enableConsole: true
                    });
                } catch (e) {
                    console.warn('[InstrumentView] Failed to create Logger instance, using console fallback');
                    this.logger = console;
                }
            } else {
                // Fallback vers console
                this.logger = console;
            }
        }
        
        // Initialiser les propriétés critiques si elles n'existent pas encore
        if (!this.displayConfig) {
            this.displayConfig = {
                compactMode: false,
                showMetrics: true,
                showCapabilities: true
            };
        }
        
        if (!this.localState) {
            this.localState = {
                selectedInstruments: new Set(),
                expandedInstruments: new Set(),
                displayMode: 'normal'
            };
        }
        
        if (!this.connectionColors) {
            this.connectionColors = {
                'usb': '#3498db',
                'bluetooth': '#9b59b6',
                'network': '#1abc9c',
                'virtual': '#95a5a6'
            };
        }
        
        // Désactiver autoRender
        this.config.autoRender = false;
        
        // Vérifier container
        if (!this.container) {
            if (this.logger && this.logger.error) {
                this.logger.error('InstrumentView', 'Container not found');
            } else {
                console.error('[InstrumentView] Container not found');
            }
            return;
        }
        
        // Maintenant on peut logger en toute sécurité
        if (this.logger && this.logger.info) {
            this.logger.info('InstrumentView', '✓ View initialized (balanced version)');
        } else {
            console.log('[InstrumentView] ✓ View initialized (balanced version)');
        }
        
        // Hook personnalisé
        if (typeof this.onInitialize === 'function') {
            this.onInitialize();
        }
        
        // Ne PAS rendre automatiquement - le controller le fera
    }
    
    /**
     * Hook appelé par initialize() après que tout soit prêt
     */
    onInitialize() {
        // Prêt pour le rendu si nécessaire
    }
    
    // ========================================================================
    // RENDU PRINCIPAL
    // ========================================================================
    
    buildTemplate(data = {}) {
        const tempDiv = document.createElement("div");
        const oldContainer = this.container;
        this.container = tempDiv;
        this.render();
        this.container = oldContainer;
        return tempDiv.innerHTML;
    }

    render(data = {}) {
        if (!this.container) {
            if (this.logger && this.logger.warn) {
                this.logger.warn('InstrumentView', 'Container not found');
            } else {
                console.warn('[InstrumentView] Container not found');
            }
            return;
        }
        
        // Safety check: ensure displayConfig exists
        if (!this.displayConfig) {
            if (this.logger && this.logger.warn) {
                this.logger.warn('InstrumentView', 'displayConfig not initialized yet, skipping render');
            } else {
                console.warn('[InstrumentView] displayConfig not initialized yet, skipping render');
            }
            return;
        }
        
        const instruments = data.instruments || [];
        
        const html = `
            <div class="instrument-view">
                
                <!-- Header avec actions -->
                <div class="instrument-header">
                    <div class="header-title">
                        <h2>🎹 Instruments MIDI</h2>
                        <span class="instrument-count">${instruments.length} instrument${instruments.length > 1 ? 's' : ''}</span>
                    </div>
                    
                    <div class="header-actions">
                        ${this.renderDisplayModeButtons()}
                        <button class="btn btn-primary" id="btn-refresh-instruments">
                            🔄 Rafraîchir
                        </button>
                    </div>
                </div>
                
                <!-- Liste des instruments -->
                <div class="instruments-list" id="instruments-list">
                    ${instruments.length > 0 ? 
                        instruments.map(inst => this.renderInstrument(inst)).join('') :
                        this.renderEmptyState()
                    }
                </div>
            </div>
        `;
        
        this.container.innerHTML = html;
        this.attachEventListeners();
    }
    
    // ========================================================================
    // COMPOSANTS UI
    // ========================================================================
    
    renderDisplayModeButtons() {
        return `
            <div class="display-mode-buttons">
                <button class="btn btn-sm ${this.localState.displayMode === 'normal' ? 'active' : ''}" 
                        data-mode="normal" title="Vue normale">
                    📋
                </button>
                <button class="btn btn-sm ${this.localState.displayMode === 'compact' ? 'active' : ''}" 
                        data-mode="compact" title="Vue compacte">
                    ⬜
                </button>
                <button class="btn btn-sm ${this.localState.displayMode === 'detailed' ? 'active' : ''}" 
                        data-mode="detailed" title="Vue détaillée">
                    📊
                </button>
            </div>
        `;
    }
    
    renderInstrument(instrument) {
        const isExpanded = this.localState.expandedInstruments.has(instrument.id);
        const isSelected = this.localState.selectedInstruments.has(instrument.id);
        
        return `
            <div class="instrument-card ${isSelected ? 'selected' : ''}" data-instrument-id="${instrument.id}">
                <div class="instrument-header-card">
                    <div class="instrument-info">
                        ${this.renderConnectionBadge(instrument.connection_type)}
                        <h3 class="instrument-name">${this.escapeHTML(instrument.name || instrument.id)}</h3>
                        ${instrument.port ? `<span class="instrument-port">${this.escapeHTML(instrument.port)}</span>` : ''}
                    </div>
                    
                    <div class="instrument-actions">
                        ${this.renderInstrumentStatus(instrument)}
                        <button class="btn-icon btn-expand" data-action="toggle-expand" title="${isExpanded ? 'Réduire' : 'Développer'}">
                            ${isExpanded ? '▼' : '▶'}
                        </button>
                    </div>
                </div>
                
                ${isExpanded ? this.renderInstrumentDetails(instrument) : ''}
            </div>
        `;
    }
    
    renderConnectionBadge(type) {
        const color = this.connectionColors[type] || '#95a5a6';
        const icons = {
            'usb': '🔌',
            'bluetooth': '📶',
            'network': '🌐',
            'virtual': '💻'
        };
        const icon = icons[type] || '❓';
        
        return `
            <span class="connection-badge" style="background-color: ${color};" title="${type}">
                ${icon}
            </span>
        `;
    }
    
    renderInstrumentStatus(instrument) {
        const isActive = instrument.status === 'active' || instrument.is_connected;
        
        return `
            <span class="status-indicator ${isActive ? 'active' : 'inactive'}" 
                  title="${isActive ? 'Connecté' : 'Déconnecté'}">
                ${isActive ? '🟢' : '🔴'}
            </span>
        `;
    }
    
    renderInstrumentDetails(instrument) {
        return `
            <div class="instrument-details">
                ${this.displayConfig.showMetrics ? this.renderMetrics(instrument) : ''}
                ${this.displayConfig.showCapabilities ? this.renderCapabilities(instrument) : ''}
                ${this.renderActions(instrument)}
            </div>
        `;
    }
    
    renderMetrics(instrument) {
        const metrics = instrument.metrics || {};
        
        return `
            <div class="instrument-metrics">
                <h4>Métriques</h4>
                <div class="metrics-grid">
                    <div class="metric-item">
                        <span class="metric-label">Notes jouées</span>
                        <span class="metric-value">${metrics.notes_played || 0}</span>
                    </div>
                    <div class="metric-item">
                        <span class="metric-label">Latence</span>
                        <span class="metric-value">${metrics.latency || 0}ms</span>
                    </div>
                    <div class="metric-item">
                        <span class="metric-label">Messages/s</span>
                        <span class="metric-value">${metrics.messages_per_second || 0}</span>
                    </div>
                </div>
            </div>
        `;
    }
    
    renderCapabilities(instrument) {
        const caps = instrument.capabilities || {};
        
        return `
            <div class="instrument-capabilities">
                <h4>Capacités</h4>
                <div class="capabilities-list">
                    ${caps.channels ? `<span class="capability">📻 ${caps.channels} canaux</span>` : ''}
                    ${caps.polyphony ? `<span class="capability">🎵 Polyphonie: ${caps.polyphony}</span>` : ''}
                    ${caps.program_change ? `<span class="capability">✓ Program Change</span>` : ''}
                    ${caps.pitch_bend ? `<span class="capability">✓ Pitch Bend</span>` : ''}
                </div>
            </div>
        `;
    }
    
    renderActions(instrument) {
        return `
            <div class="instrument-actions-bar">
                <button class="btn btn-sm btn-primary" data-action="select" data-instrument-id="${instrument.id}">
                    ${this.localState.selectedInstruments.has(instrument.id) ? '✓ Sélectionné' : 'Sélectionner'}
                </button>
                <button class="btn btn-sm btn-secondary" data-action="configure" data-instrument-id="${instrument.id}">
                    ⚙️ Configurer
                </button>
                <button class="btn btn-sm btn-secondary" data-action="test" data-instrument-id="${instrument.id}">
                    🎵 Tester
                </button>
            </div>
        `;
    }
    
    renderEmptyState() {
        return `
            <div class="empty-state">
                <div class="empty-icon">🎹</div>
                <h3>Aucun instrument détecté</h3>
                <p>Connectez un instrument MIDI et cliquez sur "Rafraîchir"</p>
                <button class="btn btn-primary" id="btn-scan-instruments">
                    🔍 Scanner les instruments
                </button>
            </div>
        `;
    }
    
    // ========================================================================
    // EVENT LISTENERS
    // ========================================================================
    
    attachEventListeners() {
        // Bouton rafraîchir
        const refreshBtn = this.container.querySelector('#btn-refresh-instruments');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.emit('instruments:refresh');
            });
        }
        
        // Bouton scanner
        const scanBtn = this.container.querySelector('#btn-scan-instruments');
        if (scanBtn) {
            scanBtn.addEventListener('click', () => {
                this.emit('instruments:scan');
            });
        }
        
        // Boutons de mode d'affichage
        const modeButtons = this.container.querySelectorAll('.display-mode-buttons button');
        modeButtons.forEach(btn => {
            btn.addEventListener('click', (e) => {
                const mode = e.currentTarget.dataset.mode;
                this.setDisplayMode(mode);
            });
        });
        
        // Boutons d'expansion
        const expandButtons = this.container.querySelectorAll('[data-action="toggle-expand"]');
        expandButtons.forEach(btn => {
            btn.addEventListener('click', (e) => {
                const card = e.target.closest('.instrument-card');
                const instrumentId = card.dataset.instrumentId;
                this.toggleExpand(instrumentId);
            });
        });
        
        // Boutons de sélection
        const selectButtons = this.container.querySelectorAll('[data-action="select"]');
        selectButtons.forEach(btn => {
            btn.addEventListener('click', (e) => {
                const instrumentId = e.currentTarget.dataset.instrumentId;
                this.toggleSelect(instrumentId);
            });
        });
        
        // Boutons de configuration
        const configButtons = this.container.querySelectorAll('[data-action="configure"]');
        configButtons.forEach(btn => {
            btn.addEventListener('click', (e) => {
                const instrumentId = e.currentTarget.dataset.instrumentId;
                this.emit('instruments:configure', { instrumentId });
            });
        });
        
        // Boutons de test
        const testButtons = this.container.querySelectorAll('[data-action="test"]');
        testButtons.forEach(btn => {
            btn.addEventListener('click', (e) => {
                const instrumentId = e.currentTarget.dataset.instrumentId;
                this.emit('instruments:test', { instrumentId });
            });
        });
    }
    
    // ========================================================================
    // ACTIONS
    // ========================================================================
    
    setDisplayMode(mode) {
        this.localState.displayMode = mode;
        this.displayConfig.compactMode = (mode === 'compact');
        this.render();
    }
    
    toggleExpand(instrumentId) {
        if (this.localState.expandedInstruments.has(instrumentId)) {
            this.localState.expandedInstruments.delete(instrumentId);
        } else {
            this.localState.expandedInstruments.add(instrumentId);
        }
        this.render();
    }
    
    toggleSelect(instrumentId) {
        if (this.localState.selectedInstruments.has(instrumentId)) {
            this.localState.selectedInstruments.delete(instrumentId);
        } else {
            this.localState.selectedInstruments.add(instrumentId);
        }
        
        this.emit('instruments:selection:changed', {
            instrumentId,
            selected: this.localState.selectedInstruments.has(instrumentId),
            selectedInstruments: Array.from(this.localState.selectedInstruments)
        });
        
        this.render();
    }
    
    // ========================================================================
    // API PUBLIQUE
    // ========================================================================
    
    updateInstruments(instruments) {
        this.render({ instruments });
    }
    
    selectInstrument(instrumentId) {
        this.localState.selectedInstruments.add(instrumentId);
        this.render();
    }
    
    deselectInstrument(instrumentId) {
        this.localState.selectedInstruments.delete(instrumentId);
        this.render();
    }
    
    clearSelection() {
        this.localState.selectedInstruments.clear();
        this.render();
    }
    
    getSelectedInstruments() {
        return Array.from(this.localState.selectedInstruments);
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    escapeHTML(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    // ========================================================================
    // DESTRUCTION
    // ========================================================================
    
    destroy() {
        this.localState.selectedInstruments.clear();
        this.localState.expandedInstruments.clear();
        
        super.destroy();
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
// ============================================================================
// Fichier: frontend/js/views/InstrumentView.js
// Version: v3.0.6 - MINIMAL (Basic display only)
// Date: 2025-10-19
// ============================================================================
// VERSION MINIMALE - Juste afficher les instruments
// - Liste des instruments
// - Statut connecté/déconnecté
// - Bouton scan
// ============================================================================

class InstrumentView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        this.logger = window.Logger || console;
        
        this.logger.info('InstrumentView', '✓ View initialized (minimal version)');
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
                <div class="instrument-header">
                    <h2>MIDI Instruments</h2>
                    <button id="scan-instruments-btn" class="btn btn-primary">
                        🔄 Scan
                    </button>
                </div>
                
                <div class="instrument-list">
                    ${instruments.length === 0 ? 
                        '<p class="no-instruments">No instruments found. Click Scan to detect devices.</p>' :
                        instruments.map(inst => this.renderInstrumentCard(inst)).join('')
                    }
                </div>
            </div>
        `;
        
        this.container.innerHTML = html;
        
        this.attachEventListeners();
        
        this.logger.debug('InstrumentView', `Rendered ${instruments.length} instruments`);
    }
    
    // ========================================================================
    // RENDU CARTE INSTRUMENT
    // ========================================================================
    
    renderInstrumentCard(instrument) {
        const statusClass = instrument.connected ? 'connected' : 'disconnected';
        const statusText = instrument.connected ? '🟢 Connected' : '🔴 Disconnected';
        
        return `
            <div class="instrument-card ${statusClass}" data-id="${instrument.id}">
                <div class="instrument-info">
                    <h3>${instrument.name || 'Unknown Device'}</h3>
                    <div class="instrument-details">
                        <span class="instrument-type">${instrument.type || 'MIDI'}</span>
                        <span class="instrument-status">${statusText}</span>
                    </div>
                    ${instrument.manufacturer ? 
                        `<p class="instrument-manufacturer">${instrument.manufacturer}</p>` : 
                        ''
                    }
                </div>
                
                <div class="instrument-actions">
                    ${instrument.connected ? 
                        `<button class="btn btn-small disconnect-btn" data-id="${instrument.id}">
                            Disconnect
                        </button>` :
                        `<button class="btn btn-small btn-primary connect-btn" data-id="${instrument.id}">
                            Connect
                        </button>`
                    }
                </div>
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
    }
    
    // ========================================================================
    // MISES À JOUR
    // ========================================================================
    
    updateInstrumentList(instruments) {
        this.render({ instruments });
    }
    
    updateInstrumentStatus(instrumentId, connected) {
        const card = this.container.querySelector(`[data-id="${instrumentId}"]`);
        
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
            scanBtn.textContent = '⏳ Scanning...';
        }
    }
    
    hideScanProgress() {
        const scanBtn = this.container.querySelector('#scan-instruments-btn');
        if (scanBtn) {
            scanBtn.disabled = false;
            scanBtn.textContent = '🔄 Scan';
        }
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    clear() {
        if (this.container) {
            this.container.innerHTML = '<p class="loading">Loading instruments...</p>';
        }
    }
    
    showError(message) {
        if (this.container) {
            this.container.innerHTML = `
                <div class="error-message">
                    <p>❌ ${message}</p>
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
// CSS MINIMAL (à ajouter dans ton CSS principal)
// ============================================================================
/*
.instrument-view {
    padding: 20px;
}

.instrument-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 20px;
}

.instrument-list {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
    gap: 15px;
}

.instrument-card {
    border: 1px solid #ddd;
    border-radius: 8px;
    padding: 15px;
    background: white;
}

.instrument-card.connected {
    border-color: #4CAF50;
    background: #f1f8f4;
}

.instrument-card.disconnected {
    border-color: #ccc;
    background: #f9f9f9;
}

.instrument-info h3 {
    margin: 0 0 10px 0;
    font-size: 18px;
}

.instrument-details {
    display: flex;
    gap: 10px;
    margin-bottom: 5px;
}

.instrument-type {
    padding: 2px 8px;
    background: #e0e0e0;
    border-radius: 4px;
    font-size: 12px;
}

.instrument-status {
    font-size: 14px;
}

.instrument-actions {
    margin-top: 10px;
}

.no-instruments {
    text-align: center;
    color: #666;
    padding: 40px;
}

.error-message {
    text-align: center;
    padding: 40px;
}
*/

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = InstrumentView;
}

if (typeof window !== 'undefined') {
    window.InstrumentView = InstrumentView;
}
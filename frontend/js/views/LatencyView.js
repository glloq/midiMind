// ============================================================================
// Fichier: frontend/js/views/LatencyView.js
// Version: v4.0.0
// ============================================================================

class LatencyView {
    constructor(container, eventBus) {
        this.container = typeof container === 'string' ? 
            document.getElementById(container) : container;
        this.eventBus = eventBus;
        
        this.state = {
            instruments: [],
            globalOffset: 0,
            enabled: false
        };
    }
    
    init() {
        if (!this.container) return;
        this.render();
        this.attachEvents();
        this.loadData();
    }
    
    render() {
        this.container.innerHTML = `
            <div class="latency-view">
                <div class="latency-header">
                    <h2>⏱️ Compensation de latence</h2>
                    <label>
                        <input type="checkbox" data-action="toggle-enabled" 
                               ${this.state.enabled ? 'checked' : ''} />
                        Activer compensation
                    </label>
                </div>
                
                <div class="latency-global">
                    <h3>Offset global</h3>
                    <div class="offset-control">
                        <input type="range" min="-200" max="200" value="${this.state.globalOffset}"
                               data-action="global-offset" />
                        <span>${this.state.globalOffset} ms</span>
                    </div>
                </div>
                
                <div class="latency-instruments">
                    <h3>Compensation par instrument</h3>
                    ${this.state.instruments.map(inst => `
                        <div class="instrument-latency" data-id="${inst.id}">
                            <span>${inst.name}</span>
                            <input type="number" value="${inst.compensation || 0}" 
                                   data-action="set-compensation" min="-500" max="500" />
                            <span>ms</span>
                        </div>
                    `).join('')}
                </div>
            </div>
        `;
    }
    
    attachEvents() {
        this.container.addEventListener('change', (e) => {
            const action = e.target.dataset.action;
            
            if (action === 'toggle-enabled') {
                this.toggleEnabled(e.target.checked);
            } else if (action === 'global-offset') {
                this.setGlobalOffset(parseInt(e.target.value));
            } else if (action === 'set-compensation') {
                const instEl = e.target.closest('.instrument-latency');
                const instId = instEl?.dataset.id;
                if (instId) this.setCompensation(instId, parseInt(e.target.value));
            }
        });
        
        if (!this.eventBus) return;
        
        this.eventBus.on('latency:instruments_list', (data) => {
            this.state.instruments = data.instruments || [];
            this.render();
        });
        
        this.eventBus.on('latency:compensation_updated', () => {
            this.loadData();
        });
    }
    
    toggleEnabled(enabled) {
        this.state.enabled = enabled;
        this.eventBus?.emit(enabled ? 'latency:enable_requested' : 'latency:disable_requested');
    }
    
    setGlobalOffset(offset) {
        this.state.globalOffset = offset;
        this.eventBus?.emit('latency:set_global_offset_requested', { offset });
    }
    
    setCompensation(instrumentId, compensation) {
        this.eventBus?.emit('latency:set_compensation_requested', {
            instrument_id: instrumentId,
            compensation
        });
    }
    
    loadData() {
        this.eventBus?.emit('latency:list_instruments_requested');
        this.eventBus?.emit('latency:get_global_offset_requested');
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = LatencyView;
}
if (typeof window !== 'undefined') {
    window.LatencyView = LatencyView;
}
// ============================================================================
// Fichier: frontend/js/views/LoggerView.js
// Version: v4.0.0
// ============================================================================

class LoggerView {
    constructor(container, eventBus) {
        this.container = typeof container === 'string' ? 
            document.getElementById(container) : container;
        this.eventBus = eventBus;
        
        this.state = {
            logs: [],
            level: 'info',
            filter: '',
            autoScroll: true
        };
    }
    
    init() {
        if (!this.container) return;
        this.render();
        this.attachEvents();
        this.loadLogs();
    }
    
    render() {
        this.container.innerHTML = `
            <div class="logger-view">
                <div class="logger-header">
                    <h2>📝 Logs système</h2>
                    <div class="logger-controls">
                        <select data-action="level">
                            <option value="debug" ${this.state.level === 'debug' ? 'selected' : ''}>Debug</option>
                            <option value="info" ${this.state.level === 'info' ? 'selected' : ''}>Info</option>
                            <option value="warning" ${this.state.level === 'warning' ? 'selected' : ''}>Warning</option>
                            <option value="error" ${this.state.level === 'error' ? 'selected' : ''}>Error</option>
                        </select>
                        <input type="text" placeholder="Filtrer..." data-action="filter" 
                               value="${this.state.filter}" />
                        <button data-action="clear">Effacer</button>
                        <button data-action="export">Exporter</button>
                    </div>
                </div>
                
                <div class="logger-content" id="loggerContent">
                    ${this.renderLogs()}
                </div>
            </div>
        `;
    }
    
    renderLogs() {
        const filtered = this.state.logs.filter(log => {
            if (this.state.filter) {
                return log.message?.toLowerCase().includes(this.state.filter.toLowerCase());
            }
            return true;
        });
        
        if (filtered.length === 0) return '<div class="logs-empty">Aucun log</div>';
        
        return filtered.map(log => `
            <div class="log-entry log-${log.level || 'info'}">
                <span class="log-time">${new Date(log.timestamp * 1000).toLocaleString()}</span>
                <span class="log-level">${log.level?.toUpperCase()}</span>
                <span class="log-message">${log.message || ''}</span>
            </div>
        `).join('');
    }
    
    attachEvents() {
        this.container.addEventListener('change', (e) => {
            const action = e.target.dataset.action;
            if (action === 'level') {
                this.setLevel(e.target.value);
            }
        });
        
        this.container.addEventListener('input', (e) => {
            if (e.target.dataset.action === 'filter') {
                this.state.filter = e.target.value;
                this.render();
            }
        });
        
        this.container.addEventListener('click', (e) => {
            const action = e.target.dataset.action;
            if (action === 'clear') this.clearLogs();
            if (action === 'export') this.exportLogs();
        });
        
        if (!this.eventBus) return;
        
        this.eventBus.on('logger:logs', (data) => {
            this.state.logs = data.logs || [];
            this.render();
            if (this.state.autoScroll) this.scrollToBottom();
        });
        
        this.eventBus.on('logger:level', (data) => {
            this.state.level = data.level;
        });
    }
    
    setLevel(level) {
        this.state.level = level;
        this.eventBus?.emit('logger:set_level_requested', { level });
    }
    
    clearLogs() {
        this.eventBus?.emit('logger:clear_requested');
    }
    
    exportLogs() {
        this.eventBus?.emit('logger:export_requested');
    }
    
    loadLogs() {
        this.eventBus?.emit('logger:get_logs_requested', { limit: 1000 });
        this.eventBus?.emit('logger:get_level_requested');
    }
    
    scrollToBottom() {
        const content = document.getElementById('loggerContent');
        if (content) content.scrollTop = content.scrollHeight;
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = LoggerView;
}
if (typeof window !== 'undefined') {
    window.LoggerView = LoggerView;
}
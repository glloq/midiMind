// ============================================================================
// Fichier: frontend/js/views/SystemView.js
// Version: v4.0.0 - CONFORMITÃ‰ API DOCUMENTATION
// Date: 2025-11-02
// ============================================================================
// AMÃ‰LIORATIONS v4.0.0:
// âœ… API v4.2.2: system.*, network.*, logger.*
// âœ… Monitoring temps rÃ©el
// âœ… Statistiques rÃ©seau
// âœ… Gestion des logs
// ============================================================================

class SystemView {
    constructor(container, eventBus) {
        if (typeof container === 'string') {
            this.container = document.getElementById(container) || document.querySelector(container);
        } else if (container instanceof HTMLElement) {
            this.container = container;
        } else {
            this.container = null;
        }
        
        if (!this.container) {
            console.error('[SystemView] Container not found');
        }
        
        this.eventBus = eventBus;
        this.logger = window.logger || console;
        
        // Ã‰tat
        this.state = {
            systemInfo: null,
            memory: null,
            disk: null,
            uptime: null,
            networkStatus: null,
            networkInterfaces: [],
            networkStats: null,
            logLevel: 'info',
            logs: [],
            refreshInterval: null
        };
        
        this.elements = {};
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    init() {
        if (!this.container) {
            this.logger.error('[SystemView] Cannot initialize');
            return;
        }
        
        this.render();
        this.cacheElements();
        this.attachEvents();
        this.loadSystemData();
        this.startAutoRefresh();
        
        this.logger.info('[SystemView] Initialized v4.0.0');
    }

    render() {
        if (!this.container) return;
        
        this.container.innerHTML = `
            <div class="page-header">
                <h1>âš™ï¸ SystÃ¨me & RÃ©seau</h1>
                <div class="header-actions">
                    <button class="btn-refresh" data-action="refresh">
                        ðŸ”„ Actualiser
                    </button>
                </div>
            </div>
            
            <div class="system-layout">
                <!-- Informations systÃ¨me -->
                <div class="system-section">
                    <h2>ðŸ“Š Informations systÃ¨me</h2>
                    <div id="systemInfo">
                        ${this.renderSystemInfo()}
                    </div>
                </div>
                
                <!-- MÃ©moire et disque -->
                <div class="system-section">
                    <h2>ðŸ’¾ Ressources</h2>
                    <div id="systemResources">
                        ${this.renderResources()}
                    </div>
                </div>
                
                <!-- RÃ©seau -->
                <div class="system-section">
                    <h2>ðŸŒ RÃ©seau</h2>
                    <div id="networkInfo">
                        ${this.renderNetwork()}
                    </div>
                </div>
                
                <!-- Logger -->
                <div class="system-section logs-section">
                    <div class="logs-header">
                        <h2>ðŸ“ Logs</h2>
                        <div class="logs-controls">
                            <select class="log-level-select" data-action="change-log-level">
                                <option value="debug" ${this.state.logLevel === 'debug' ? 'selected' : ''}>Debug</option>
                                <option value="info" ${this.state.logLevel === 'info' ? 'selected' : ''}>Info</option>
                                <option value="warning" ${this.state.logLevel === 'warning' ? 'selected' : ''}>Warning</option>
                                <option value="error" ${this.state.logLevel === 'error' ? 'selected' : ''}>Error</option>
                            </select>
                            <button class="btn-clear-logs" data-action="clear-logs">
                                ðŸ—‘ï¸ Effacer
                            </button>
                            <button class="btn-export-logs" data-action="export-logs">
                                ðŸ’¾ Exporter
                            </button>
                        </div>
                    </div>
                    <div id="logsContent">
                        ${this.renderLogs()}
                    </div>
                </div>
            </div>
        `;
    }

    cacheElements() {
        this.elements = {
            systemInfo: document.getElementById('systemInfo'),
            systemResources: document.getElementById('systemResources'),
            networkInfo: document.getElementById('networkInfo'),
            logsContent: document.getElementById('logsContent')
        };
    }

    attachEvents() {
        if (!this.container) return;
        
        this.container.addEventListener('click', (e) => {
            const action = e.target.closest('[data-action]')?.dataset.action;
            if (!action) return;
            
            switch (action) {
                case 'refresh':
                    this.loadSystemData();
                    break;
                case 'clear-logs':
                    this.clearLogs();
                    break;
                case 'export-logs':
                    this.exportLogs();
                    break;
            }
        });
        
        this.container.addEventListener('change', (e) => {
            const action = e.target.dataset.action;
            
            if (action === 'change-log-level') {
                this.changeLogLevel(e.target.value);
            }
        });
        
        this.setupEventBusListeners();
    }

    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        // system.* responses
        this.eventBus.on('system:info', (data) => {
            this.state.systemInfo = data;
            this.renderSystemInfoSection();
        });
        
        this.eventBus.on('system:uptime', (data) => {
            this.state.uptime = data.uptime;
            this.renderSystemInfoSection();
        });
        
        this.eventBus.on('system:memory', (data) => {
            this.state.memory = data;
            this.renderResourcesSection();
        });
        
        this.eventBus.on('system:disk', (data) => {
            this.state.disk = data;
            this.renderResourcesSection();
        });
        
        // network.* responses
        this.eventBus.on('network:status', (data) => {
            this.state.networkStatus = data;
            this.renderNetworkSection();
        });
        
        this.eventBus.on('network:interfaces', (data) => {
            this.state.networkInterfaces = data.interfaces || [];
            this.renderNetworkSection();
        });
        
        this.eventBus.on('network:stats', (data) => {
            this.state.networkStats = data;
            this.renderNetworkSection();
        });
        
        // logger.* responses
        this.eventBus.on('logger:level', (data) => {
            this.state.logLevel = data.level;
        });
        
        this.eventBus.on('logger:logs', (data) => {
            this.state.logs = data.logs || [];
            this.renderLogsSection();
        });
    }

    // ========================================================================
    // RENDERING - SYSTEM INFO
    // ========================================================================

    renderSystemInfo() {
        const info = this.state.systemInfo;
        const uptime = this.state.uptime;
        
        if (!info) {
            return '<div class="loading">Chargement...</div>';
        }
        
        return `
            <div class="info-grid">
                <div class="info-item">
                    <span class="info-label">Version:</span>
                    <span class="info-value">${info.version || 'â€”'}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Plateforme:</span>
                    <span class="info-value">${info.platform || 'â€”'}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Architecture:</span>
                    <span class="info-value">${info.arch || 'â€”'}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Uptime:</span>
                    <span class="info-value">${uptime ? this.formatUptime(uptime) : 'â€”'}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Hostname:</span>
                    <span class="info-value">${info.hostname || 'â€”'}</span>
                </div>
            </div>
        `;
    }

    renderSystemInfoSection() {
        if (this.elements.systemInfo) {
            this.elements.systemInfo.innerHTML = this.renderSystemInfo();
        }
    }

    // ========================================================================
    // RENDERING - RESOURCES
    // ========================================================================

    renderResources() {
        const memory = this.state.memory;
        const disk = this.state.disk;
        
        return `
            <div class="resources-grid">
                ${this.renderMemory(memory)}
                ${this.renderDisk(disk)}
            </div>
        `;
    }

    renderMemory(memory) {
        if (!memory) {
            return '<div class="loading">Chargement mÃ©moire...</div>';
        }
        
        const total = memory.total || 0;
        const used = memory.used || 0;
        const free = memory.free || 0;
        const percent = total > 0 ? ((used / total) * 100).toFixed(1) : 0;
        
        return `
            <div class="resource-card">
                <h3>ðŸ’¾ MÃ©moire RAM</h3>
                <div class="progress-bar">
                    <div class="progress-fill" style="width: ${percent}%"></div>
                </div>
                <div class="resource-stats">
                    <span>UtilisÃ©e: ${this.formatBytes(used)}</span>
                    <span>Libre: ${this.formatBytes(free)}</span>
                    <span>Total: ${this.formatBytes(total)}</span>
                </div>
                <div class="resource-percent">${percent}% utilisÃ©</div>
            </div>
        `;
    }

    renderDisk(disk) {
        if (!disk) {
            return '<div class="loading">Chargement disque...</div>';
        }
        
        const total = disk.total || 0;
        const used = disk.used || 0;
        const free = disk.free || 0;
        const percent = total > 0 ? ((used / total) * 100).toFixed(1) : 0;
        
        return `
            <div class="resource-card">
                <h3>ðŸ’¿ Disque</h3>
                <div class="progress-bar">
                    <div class="progress-fill" style="width: ${percent}%"></div>
                </div>
                <div class="resource-stats">
                    <span>UtilisÃ©: ${this.formatBytes(used)}</span>
                    <span>Libre: ${this.formatBytes(free)}</span>
                    <span>Total: ${this.formatBytes(total)}</span>
                </div>
                <div class="resource-percent">${percent}% utilisÃ©</div>
            </div>
        `;
    }

    renderResourcesSection() {
        if (this.elements.systemResources) {
            this.elements.systemResources.innerHTML = this.renderResources();
        }
    }

    // ========================================================================
    // RENDERING - NETWORK
    // ========================================================================

    renderNetwork() {
        const status = this.state.networkStatus;
        const interfaces = this.state.networkInterfaces;
        const stats = this.state.networkStats;
        
        return `
            <div class="network-grid">
                ${this.renderNetworkStatus(status)}
                ${this.renderNetworkInterfaces(interfaces)}
                ${this.renderNetworkStats(stats)}
            </div>
        `;
    }

    renderNetworkStatus(status) {
        if (!status) {
            return '<div class="loading">Chargement statut rÃ©seau...</div>';
        }
        
        return `
            <div class="network-card">
                <h3>ðŸŒ Statut</h3>
                <div class="network-status ${status.connected ? 'connected' : 'disconnected'}">
                    ${status.connected ? 'âœ“ ConnectÃ©' : 'âœ— DÃ©connectÃ©'}
                </div>
            </div>
        `;
    }

    renderNetworkInterfaces(interfaces) {
        if (!interfaces || interfaces.length === 0) {
            return '<div class="loading">Aucune interface rÃ©seau</div>';
        }
        
        return `
            <div class="network-card">
                <h3>ðŸ”Œ Interfaces</h3>
                <div class="interfaces-list">
                    ${interfaces.map(iface => `
                        <div class="interface-item">
                            <span class="interface-name">${iface.name}</span>
                            <span class="interface-ip">${iface.ip || 'â€”'}</span>
                        </div>
                    `).join('')}
                </div>
            </div>
        `;
    }

    renderNetworkStats(stats) {
        if (!stats) {
            return '<div class="loading">Chargement stats rÃ©seau...</div>';
        }
        
        return `
            <div class="network-card">
                <h3>ðŸ“Š Statistiques</h3>
                <div class="stats-grid">
                    <div class="stat-item">
                        <span class="stat-label">â†“ ReÃ§u:</span>
                        <span class="stat-value">${this.formatBytes(stats.rx_bytes || 0)}</span>
                    </div>
                    <div class="stat-item">
                        <span class="stat-label">â†‘ EnvoyÃ©:</span>
                        <span class="stat-value">${this.formatBytes(stats.tx_bytes || 0)}</span>
                    </div>
                </div>
            </div>
        `;
    }

    renderNetworkSection() {
        if (this.elements.networkInfo) {
            this.elements.networkInfo.innerHTML = this.renderNetwork();
        }
    }

    // ========================================================================
    // RENDERING - LOGS
    // ========================================================================

    renderLogs() {
        const logs = this.state.logs;
        
        if (!logs || logs.length === 0) {
            return `
                <div class="logs-empty">
                    <p>Aucun log disponible</p>
                </div>
            `;
        }
        
        return `
            <div class="logs-list">
                ${logs.map(log => this.renderLogEntry(log)).join('')}
            </div>
        `;
    }

    renderLogEntry(log) {
        const levelClass = `log-${log.level || 'info'}`;
        const timestamp = log.timestamp ? new Date(log.timestamp * 1000).toLocaleString() : 'â€”';
        
        return `
            <div class="log-entry ${levelClass}">
                <span class="log-time">${timestamp}</span>
                <span class="log-level">${log.level || 'INFO'}</span>
                <span class="log-message">${log.message || ''}</span>
            </div>
        `;
    }

    renderLogsSection() {
        if (this.elements.logsContent) {
            this.elements.logsContent.innerHTML = this.renderLogs();
        }
    }

    // ========================================================================
    // ACTIONS
    // ========================================================================

    async loadSystemData() {
        if (!this.eventBus) return;
        
        // Appels API parallÃ¨les
        this.eventBus.emit('system:info_requested');
        this.eventBus.emit('system:uptime_requested');
        this.eventBus.emit('system:memory_requested');
        this.eventBus.emit('system:disk_requested');
        this.eventBus.emit('network:status_requested');
        this.eventBus.emit('network:interfaces_requested');
        this.eventBus.emit('network:stats_requested');
        this.eventBus.emit('logger:get_logs_requested', { limit: 100 });
    }

    async changeLogLevel(level) {
        this.state.logLevel = level;
        
        // Appel API: logger.setLevel
        if (this.eventBus) {
            this.eventBus.emit('logger:set_level_requested', { level });
        }
    }

    async clearLogs() {
        if (!confirm('Effacer tous les logs ?')) return;
        
        // Appel API: logger.clear
        if (this.eventBus) {
            this.eventBus.emit('logger:clear_requested');
        }
        
        this.state.logs = [];
        this.renderLogsSection();
    }

    async exportLogs() {
        // Appel API: logger.export
        if (this.eventBus) {
            this.eventBus.emit('logger:export_requested');
        }
    }

    startAutoRefresh() {
        // RafraÃ®chir toutes les 5 secondes
        this.state.refreshInterval = setInterval(() => {
            this.loadSystemData();
        }, 5000);
    }

    stopAutoRefresh() {
        if (this.state.refreshInterval) {
            clearInterval(this.state.refreshInterval);
            this.state.refreshInterval = null;
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    formatBytes(bytes) {
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
        return (bytes / (1024 * 1024 * 1024)).toFixed(1) + ' GB';
    }

    formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const mins = Math.floor((seconds % 3600) / 60);
        
        const parts = [];
        if (days > 0) parts.push(`${days}j`);
        if (hours > 0) parts.push(`${hours}h`);
        if (mins > 0) parts.push(`${mins}m`);
        
        return parts.join(' ') || '0m';
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    destroy() {
        this.stopAutoRefresh();
        
        if (this.eventBus) {
            this.eventBus.off('system:info');
            this.eventBus.off('system:uptime');
            this.eventBus.off('system:memory');
            this.eventBus.off('system:disk');
            this.eventBus.off('network:status');
            this.eventBus.off('network:interfaces');
            this.eventBus.off('network:stats');
            this.eventBus.off('logger:level');
            this.eventBus.off('logger:logs');
        }
        
        this.logger.info('[SystemView] Destroyed');
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = SystemView;
}

if (typeof window !== 'undefined') {
    window.SystemView = SystemView;
}
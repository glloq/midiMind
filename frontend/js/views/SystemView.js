// ============================================================================
// Fichier: frontend/js/views/SystemView.js
// Version: v3.2.0 - SIGNATURE COHÉRENTE + API COMPLÈTE
// Date: 2025-11-01
// ============================================================================
// CORRECTIONS v3.2.0:
// ✅ Signature cohérente : constructor(containerId, eventBus, logger = null)
// ✅ Appel super() correct
// ✅ Affichage stats système (uptime, memory, disk)
// ✅ Affichage liste devices MIDI
// ✅ Boutons connect/disconnect/scan devices
// ============================================================================

class SystemView extends BaseView {
    constructor(containerId, eventBus, logger = null) {
        super(containerId, eventBus);
        
        this.logger = logger || window.logger || console;
        
        // État spécifique à la vue
        this.viewState = {
            systemInfo: null,
            devices: [],
            selectedDevice: null
        };
        
        this.log('info', 'SystemView', '✅ SystemView v3.2.0 initialized');
    }
    
    // ========================================================================
    // TEMPLATE PRINCIPAL
    // ========================================================================
    
    buildTemplate(data = {}) {
        const state = { ...this.viewState, ...data };
        
        return \`
            <div class="system-view-container">
                <div class="page-header">
                    <h1>⚙️ Système</h1>
                    <div class="header-actions">
                        <button class="btn-refresh" data-action="refresh-system">
                            🔄 Actualiser
                        </button>
                    </div>
                </div>
                
                <div class="system-grid">
                    <!-- Stats système -->
                    <div class="system-section">
                        <h2>📊 Statistiques Système</h2>
                        \${this.renderSystemStats(state)}
                    </div>
                    
                    <!-- Devices MIDI -->
                    <div class="system-section">
                        <h2>🎹 Périphériques MIDI</h2>
                        <div class="devices-actions">
                            <button class="btn-primary" data-action="scan-devices">
                                🔍 Scanner
                            </button>
                        </div>
                        \${this.renderDevicesList(state)}
                    </div>
                </div>
            </div>
        \`;
    }
    
    // ========================================================================
    // RENDERING STATS SYSTÈME
    // ========================================================================
    
    renderSystemStats(state) {
        const info = state.systemInfo;
        
        if (!info) {
            return \`
                <div class="stats-loading">
                    <p>Chargement des informations système...</p>
                </div>
            \`;
        }
        
        return \`
            <div class="stats-grid">
                <!-- Version -->
                <div class="stat-card">
                    <div class="stat-icon">🏷️</div>
                    <div class="stat-content">
                        <div class="stat-label">Version</div>
                        <div class="stat-value">\${info.version?.version || 'N/A'}</div>
                    </div>
                </div>
                
                <!-- Uptime -->
                <div class="stat-card">
                    <div class="stat-icon">⏱️</div>
                    <div class="stat-content">
                        <div class="stat-label">Uptime</div>
                        <div class="stat-value">\${this.formatUptime(info.uptime)}</div>
                    </div>
                </div>
                
                <!-- Memory -->
                <div class="stat-card">
                    <div class="stat-icon">💾</div>
                    <div class="stat-content">
                        <div class="stat-label">Mémoire</div>
                        <div class="stat-value">\${this.formatMemory(info.memory)}</div>
                        <div class="stat-bar">
                            <div class="stat-bar-fill" style="width: \${info.memory?.percent || 0}%"></div>
                        </div>
                    </div>
                </div>
                
                <!-- Disk -->
                <div class="stat-card">
                    <div class="stat-icon">💿</div>
                    <div class="stat-content">
                        <div class="stat-label">Disque</div>
                        <div class="stat-value">\${this.formatDisk(info.disk)}</div>
                        <div class="stat-bar">
                            <div class="stat-bar-fill" style="width: \${info.disk?.percent || 0}%"></div>
                        </div>
                    </div>
                </div>
            </div>
        \`;
    }
    
    // ========================================================================
    // RENDERING DEVICES
    // ========================================================================
    
    renderDevicesList(state) {
        const devices = state.devices || [];
        
        if (devices.length === 0) {
            return \`
                <div class="devices-empty">
                    <p>Aucun périphérique MIDI détecté</p>
                    <p class="text-muted">Cliquez sur "Scanner" pour rechercher des périphériques</p>
                </div>
            \`;
        }
        
        return \`
            <div class="devices-list">
                \${devices.map(device => this.renderDeviceCard(device)).join('')}
            </div>
        \`;
    }
    
    renderDeviceCard(device) {
        const isConnected = device.connected || false;
        const statusClass = isConnected ? 'connected' : 'disconnected';
        const statusIcon = isConnected ? '✅' : '⚪';
        const actionButton = isConnected 
            ? \`<button class="btn-danger" data-action="disconnect-device" data-device-id="\${device.id}">Déconnecter</button>\`
            : \`<button class="btn-primary" data-action="connect-device" data-device-id="\${device.id}">Connecter</button>\`;
        
        return \`
            <div class="device-card \${statusClass}" data-device-id="\${device.id}">
                <div class="device-header">
                    <span class="device-status">\${statusIcon}</span>
                    <span class="device-name">\${device.name || device.id}</span>
                </div>
                <div class="device-info">
                    <div class="device-type">\${device.type || 'MIDI'}</div>
                    <div class="device-id">\${device.id}</div>
                </div>
                <div class="device-actions">
                    \${actionButton}
                </div>
            </div>
        \`;
    }
    
    // ========================================================================
    // FORMATTERS
    // ========================================================================
    
    formatUptime(uptime) {
        if (!uptime) return 'N/A';
        
        const days = uptime.days || 0;
        const hours = uptime.hours || 0;
        const minutes = uptime.minutes || 0;
        
        if (days > 0) {
            return \`\${days}j \${hours}h \${minutes}m\`;
        } else if (hours > 0) {
            return \`\${hours}h \${minutes}m\`;
        } else {
            return \`\${minutes}m\`;
        }
    }
    
    formatMemory(memory) {
        if (!memory) return 'N/A';
        
        const used = memory.used_mb || 0;
        const total = memory.total_mb || 0;
        const percent = memory.percent || 0;
        
        return \`\${used.toFixed(0)} / \${total.toFixed(0)} MB (\${percent.toFixed(1)}%)\`;
    }
    
    formatDisk(disk) {
        if (!disk) return 'N/A';
        
        const used = disk.used_gb || 0;
        const total = disk.total_gb || 0;
        const percent = disk.percent || 0;
        
        return \`\${used.toFixed(1)} / \${total.toFixed(1)} GB (\${percent.toFixed(1)}%)\`;
    }
    
    // ========================================================================
    // UPDATE MÉTHODES
    // ========================================================================
    
    updateSystemInfo(systemInfo) {
        this.viewState.systemInfo = systemInfo;
        this.render();
    }
    
    updateDevices(devices) {
        this.viewState.devices = devices;
        this.render();
    }
    
    // ========================================================================
    // ÉVÉNEMENTS UI
    // ========================================================================
    
    attachEventListeners() {
        if (!this.container) return;
        
        // Délégation événements
        this.container.addEventListener('click', (e) => {
            const target = e.target.closest('[data-action]');
            if (!target) return;
            
            const action = target.dataset.action;
            const deviceId = target.dataset.deviceId;
            
            switch (action) {
                case 'refresh-system':
                    this.eventBus.emit('system:refresh-requested');
                    break;
                    
                case 'scan-devices':
                    this.eventBus.emit('system:scan-devices-requested');
                    break;
                    
                case 'connect-device':
                    if (deviceId) {
                        this.eventBus.emit('system:connect-device-requested', { deviceId });
                    }
                    break;
                    
                case 'disconnect-device':
                    if (deviceId) {
                        this.eventBus.emit('system:disconnect-device-requested', { deviceId });
                    }
                    break;
            }
        });
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        }
    }
}

// ============================================================================
// EXPORT
// ============================================================================
if (typeof window !== 'undefined') {
    window.SystemView = SystemView;
}
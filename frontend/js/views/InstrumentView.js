// ============================================================================
// Fichier: frontend/js/views/InstrumentView.js
// Version: v3.8.0 - PAGE INSTRUMENTS REFACTORISÉE
// Date: 2025-10-29
// Projet: MidiMind v3.1
// ============================================================================
// FONCTIONNALITÉS v3.8.0:
// ✅ Recherche et connexion (USB, Bluetooth, Réseau)
// ✅ Liste des instruments connectés
// ✅ Paramètres de délais par instrument
// ✅ Affichage paramètres selon type de connexion
// ============================================================================

class InstrumentView {
    constructor(container, eventBus) {
        // Container
        if (typeof container === 'string') {
            this.container = document.getElementById(container) || document.querySelector(container);
        } else if (container instanceof HTMLElement) {
            this.container = container;
        } else {
            this.container = null;
        }
        
        if (!this.container) {
            console.error('[InstrumentView] Container not found:', container);
        }
        
        this.eventBus = eventBus;
        this.logger = window.logger || console;
        
        // État
        this.state = {
            connectedDevices: [],
            discoveredDevices: [],
            scanning: {
                usb: false,
                bluetooth: false,
                network: false
            },
            selectedDevice: null
        };
        
        // Éléments DOM
        this.elements = {};
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    init() {
        if (!this.container) {
            this.logger.error('[InstrumentView] Cannot initialize: container not found');
            return;
        }
        
        this.render();
        this.cacheElements();
        this.attachEvents();
        this.loadConnectedDevices();
        
        this.logger.info('[InstrumentView] Initialized');
    }

    render() {
        if (!this.container) return;
        
        this.container.innerHTML = `
            <div class="page-header">
                <h1>🎸 Gestion des Instruments</h1>
            </div>
            
            <div class="instruments-layout">
                <!-- Recherche et connexion -->
                <div class="instruments-discover">
                    <div class="section-header">
                        <h2>Rechercher et connecter</h2>
                        <div class="discover-controls">
                            <button class="btn-scan" id="btnScanUSB" data-type="usb">
                                🔌 Scan USB
                            </button>
                            <button class="btn-scan" id="btnScanBluetooth" data-type="bluetooth">
                                📡 Scan Bluetooth
                            </button>
                            <button class="btn-scan" id="btnScanNetwork" data-type="network">
                                🌐 Scan Réseau
                            </button>
                        </div>
                    </div>
                    
                    <div class="devices-found" id="devicesFound">
                        ${this.renderEmptyDiscovery()}
                    </div>
                </div>
                
                <!-- Instruments connectés -->
                <div class="instruments-connected">
                    <div class="section-header">
                        <h2>Instruments connectés</h2>
                    </div>
                    
                    <div class="devices-list" id="connectedDevices">
                        ${this.renderEmptyConnected()}
                    </div>
                </div>
            </div>
        `;
    }

    cacheElements() {
        this.elements = {
            btnScanUSB: document.getElementById('btnScanUSB'),
            btnScanBluetooth: document.getElementById('btnScanBluetooth'),
            btnScanNetwork: document.getElementById('btnScanNetwork'),
            devicesFound: document.getElementById('devicesFound'),
            connectedDevices: document.getElementById('connectedDevices')
        };
    }

    attachEvents() {
        // Boutons de scan
        if (this.elements.btnScanUSB) {
            this.elements.btnScanUSB.addEventListener('click', () => this.scanDevices('usb'));
        }
        if (this.elements.btnScanBluetooth) {
            this.elements.btnScanBluetooth.addEventListener('click', () => this.scanDevices('bluetooth'));
        }
        if (this.elements.btnScanNetwork) {
            this.elements.btnScanNetwork.addEventListener('click', () => this.scanDevices('network'));
        }
        
        // Délégation d'événements
        if (this.elements.devicesFound) {
            this.elements.devicesFound.addEventListener('click', (e) => this.handleDiscoveredDeviceAction(e));
        }
        if (this.elements.connectedDevices) {
            this.elements.connectedDevices.addEventListener('click', (e) => this.handleConnectedDeviceAction(e));
        }
        
        // EventBus
        this.setupEventBusListeners();
    }

    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        this.eventBus.on('instruments:scan_started', (data) => {
            this.handleScanStarted(data.type);
        });
        
        this.eventBus.on('instruments:scan_completed', (data) => {
            this.handleScanCompleted(data.type, data.devices);
        });
        
        this.eventBus.on('instruments:device_connected', (data) => {
            this.handleDeviceConnected(data.device);
        });
        
        this.eventBus.on('instruments:device_disconnected', (data) => {
            this.handleDeviceDisconnected(data.device);
        });
        
        this.eventBus.on('instruments:list_updated', (data) => {
            this.state.connectedDevices = data.devices || [];
            this.renderConnectedDevices();
        });
    }

    // ========================================================================
    // SCAN DES PÉRIPHÉRIQUES
    // ========================================================================

    scanDevices(type) {
        this.logger.info(`[InstrumentView] Scanning ${type} devices...`);
        
        this.state.scanning[type] = true;
        this.updateScanButton(type, true);
        
        if (this.eventBus) {
            this.eventBus.emit('instruments:scan_requested', { type });
        }
        
        // Simuler un scan (sera remplacé par vraie logique)
        setTimeout(() => {
            this.handleScanCompleted(type, this.getMockDevices(type));
        }, 2000);
    }

    handleScanStarted(type) {
        this.state.scanning[type] = true;
        this.updateScanButton(type, true);
    }

    handleScanCompleted(type, devices) {
        this.state.scanning[type] = false;
        this.updateScanButton(type, false);
        
        // Filtrer les périphériques déjà connectés
        this.state.discoveredDevices = (devices || []).filter(device => {
            return !this.state.connectedDevices.some(connected => connected.id === device.id);
        });
        
        this.renderDiscoveredDevices();
    }

    updateScanButton(type, scanning) {
        const buttonMap = {
            usb: this.elements.btnScanUSB,
            bluetooth: this.elements.btnScanBluetooth,
            network: this.elements.btnScanNetwork
        };
        
        const button = buttonMap[type];
        if (!button) return;
        
        if (scanning) {
            button.classList.add('scanning');
            button.disabled = true;
        } else {
            button.classList.remove('scanning');
            button.disabled = false;
        }
    }

    // ========================================================================
    // RENDU DES PÉRIPHÉRIQUES DÉCOUVERTS
    // ========================================================================

    renderDiscoveredDevices() {
        if (!this.elements.devicesFound) return;
        
        if (!this.state.discoveredDevices || this.state.discoveredDevices.length === 0) {
            this.elements.devicesFound.innerHTML = this.renderEmptyDiscovery();
            return;
        }
        
        this.elements.devicesFound.innerHTML = this.state.discoveredDevices
            .map(device => this.renderDeviceCard(device))
            .join('');
    }

    renderDeviceCard(device) {
        const icon = this.getDeviceIcon(device.type);
        const typeLabel = this.getTypeLabel(device.type);
        
        return `
            <div class="device-card" data-device-id="${device.id}">
                <div class="device-card-header">
                    <div class="device-card-icon">${icon}</div>
                    <div class="device-card-info">
                        <div class="device-card-name">${device.name || 'Périphérique inconnu'}</div>
                        <div class="device-card-type">${typeLabel}</div>
                    </div>
                </div>
                
                <div class="device-card-actions">
                    <button class="btn-connect" data-action="connect" data-device-id="${device.id}">
                        🔗 Connecter
                    </button>
                </div>
            </div>
        `;
    }

    renderEmptyDiscovery() {
        return `
            <div class="empty-state">
                <div class="empty-state-icon">🔍</div>
                <div class="empty-state-text">Aucun périphérique trouvé</div>
                <div class="empty-state-hint">Lancez un scan pour rechercher des instruments</div>
            </div>
        `;
    }

    // ========================================================================
    // RENDU DES PÉRIPHÉRIQUES CONNECTÉS
    // ========================================================================

    renderConnectedDevices() {
        if (!this.elements.connectedDevices) return;
        
        if (!this.state.connectedDevices || this.state.connectedDevices.length === 0) {
            this.elements.connectedDevices.innerHTML = this.renderEmptyConnected();
            return;
        }
        
        this.elements.connectedDevices.innerHTML = this.state.connectedDevices
            .map(device => this.renderConnectedDevice(device))
            .join('');
    }

    renderConnectedDevice(device) {
        const icon = this.getDeviceIcon(device.type);
        const isExpanded = this.state.selectedDevice && this.state.selectedDevice.id === device.id;
        
        return `
            <div class="connected-device ${isExpanded ? 'expanded' : ''}" data-device-id="${device.id}">
                <div class="connected-device-header">
                    <div class="connected-device-icon">${icon}</div>
                    <div class="connected-device-info">
                        <div class="connected-device-name">${device.name || 'Périphérique inconnu'}</div>
                        <div class="connected-device-status">
                            <span class="status-indicator"></span>
                            <span>Connecté</span>
                        </div>
                    </div>
                    <div class="connected-device-actions">
                        <button class="btn-settings" data-action="settings" data-device-id="${device.id}">
                            ⚙️ Réglages
                        </button>
                        <button class="btn-disconnect" data-action="disconnect" data-device-id="${device.id}">
                            🔌 Déconnecter
                        </button>
                    </div>
                </div>
                
                ${isExpanded ? this.renderDeviceSettings(device) : ''}
            </div>
        `;
    }

    renderDeviceSettings(device) {
        return `
            <div class="instrument-settings-panel">
                <h3>Paramètres de ${device.name}</h3>
                
                <!-- Délai/Latence -->
                <div class="settings-group">
                    <label class="settings-label">Délai (ms)</label>
                    <input 
                        type="number" 
                        class="settings-input" 
                        value="${device.latency || 0}" 
                        min="0" 
                        max="1000"
                        data-setting="latency"
                        data-device-id="${device.id}"
                    />
                    <div class="settings-info">Compensation de latence pour ce périphérique</div>
                </div>
                
                ${this.renderTypeSpecificSettings(device)}
                
                <div class="settings-actions">
                    <button class="btn-action" data-action="save-settings" data-device-id="${device.id}">
                        💾 Enregistrer
                    </button>
                    <button class="btn-action" data-action="reset-settings" data-device-id="${device.id}">
                        🔄 Réinitialiser
                    </button>
                </div>
            </div>
        `;
    }

    renderTypeSpecificSettings(device) {
        switch (device.type) {
            case 'bluetooth':
                return this.renderBluetoothSettings(device);
            case 'network':
                return this.renderNetworkSettings(device);
            case 'usb':
                return this.renderUSBSettings(device);
            default:
                return '';
        }
    }

    renderBluetoothSettings(device) {
        const signalStrength = device.signalStrength || 0;
        const battery = device.battery || 100;
        
        return `
            <div class="settings-group">
                <label class="settings-label">Signal Bluetooth</label>
                <div class="signal-indicator">
                    <div class="signal-bar" style="width: ${signalStrength}%"></div>
                    <span>${signalStrength}%</span>
                </div>
            </div>
            
            <div class="settings-group">
                <label class="settings-label">Batterie</label>
                <div class="battery-indicator">
                    <div class="battery-level" style="width: ${battery}%"></div>
                    <span>${battery}%</span>
                </div>
            </div>
        `;
    }

    renderNetworkSettings(device) {
        return `
            <div class="settings-group">
                <label class="settings-label">Adresse IP</label>
                <input 
                    type="text" 
                    class="settings-input" 
                    value="${device.ip || ''}" 
                    data-setting="ip"
                    data-device-id="${device.id}"
                />
            </div>
            
            <div class="settings-group">
                <label class="settings-label">Port</label>
                <input 
                    type="number" 
                    class="settings-input" 
                    value="${device.port || 5004}" 
                    data-setting="port"
                    data-device-id="${device.id}"
                />
            </div>
        `;
    }

    renderUSBSettings(device) {
        return `
            <div class="settings-group">
                <label class="settings-label">Port USB</label>
                <input 
                    type="text" 
                    class="settings-input" 
                    value="${device.port || ''}" 
                    readonly
                />
                <div class="settings-info">Le port USB est détecté automatiquement</div>
            </div>
        `;
    }

    renderEmptyConnected() {
        return `
            <div class="empty-state">
                <div class="empty-state-icon">🎸</div>
                <div class="empty-state-text">Aucun instrument connecté</div>
                <div class="empty-state-hint">Scannez et connectez des instruments</div>
            </div>
        `;
    }

    // ========================================================================
    // ACTIONS SUR LES PÉRIPHÉRIQUES
    // ========================================================================

    handleDiscoveredDeviceAction(e) {
        const button = e.target.closest('[data-action]');
        if (!button) return;
        
        const action = button.dataset.action;
        const deviceId = button.dataset.deviceId;
        const device = this.state.discoveredDevices.find(d => d.id === deviceId);
        
        if (!device) return;
        
        if (action === 'connect') {
            this.connectDevice(device);
        }
    }

    handleConnectedDeviceAction(e) {
        const button = e.target.closest('[data-action]');
        if (!button) return;
        
        const action = button.dataset.action;
        const deviceId = button.dataset.deviceId;
        const device = this.state.connectedDevices.find(d => d.id === deviceId);
        
        if (!device) return;
        
        switch (action) {
            case 'settings':
                this.toggleDeviceSettings(device);
                break;
            case 'disconnect':
                this.disconnectDevice(device);
                break;
            case 'save-settings':
                this.saveDeviceSettings(device);
                break;
            case 'reset-settings':
                this.resetDeviceSettings(device);
                break;
        }
    }

    connectDevice(device) {
        this.logger.info('[InstrumentView] Connecting device:', device.name);
        
        if (this.eventBus) {
            this.eventBus.emit('instruments:connect_requested', { device });
        }
    }

    disconnectDevice(device) {
        this.logger.info('[InstrumentView] Disconnecting device:', device.name);
        
        if (confirm(`Déconnecter ${device.name} ?`)) {
            if (this.eventBus) {
                this.eventBus.emit('instruments:disconnect_requested', { device });
            }
        }
    }

    toggleDeviceSettings(device) {
        if (this.state.selectedDevice && this.state.selectedDevice.id === device.id) {
            this.state.selectedDevice = null;
        } else {
            this.state.selectedDevice = device;
        }
        
        this.renderConnectedDevices();
    }

    saveDeviceSettings(device) {
        this.logger.info('[InstrumentView] Saving settings for:', device.name);
        
        // Récupérer les valeurs des inputs
        const settings = {};
        const inputs = this.container.querySelectorAll(`[data-device-id="${device.id}"][data-setting]`);
        
        inputs.forEach(input => {
            const setting = input.dataset.setting;
            settings[setting] = input.value;
        });
        
        if (this.eventBus) {
            this.eventBus.emit('instruments:settings_saved', { device, settings });
        }
    }

    resetDeviceSettings(device) {
        this.logger.info('[InstrumentView] Resetting settings for:', device.name);
        
        if (confirm(`Réinitialiser les paramètres de ${device.name} ?`)) {
            if (this.eventBus) {
                this.eventBus.emit('instruments:settings_reset', { device });
            }
        }
    }

    handleDeviceConnected(device) {
        this.state.connectedDevices.push(device);
        this.renderConnectedDevices();
        
        // Retirer des découverts
        this.state.discoveredDevices = this.state.discoveredDevices.filter(d => d.id !== device.id);
        this.renderDiscoveredDevices();
    }

    handleDeviceDisconnected(device) {
        this.state.connectedDevices = this.state.connectedDevices.filter(d => d.id !== device.id);
        this.renderConnectedDevices();
    }

    // ========================================================================
    // CHARGEMENT DES DONNÉES
    // ========================================================================

    loadConnectedDevices() {
        if (this.eventBus) {
            this.eventBus.emit('instruments:load_requested');
        }
    }

    // ========================================================================
    // UTILITAIRES
    // ========================================================================

    getDeviceIcon(type) {
        const icons = {
            usb: '🔌',
            bluetooth: '📡',
            network: '🌐',
            virtual: '💻'
        };
        return icons[type] || '🎸';
    }

    getTypeLabel(type) {
        const labels = {
            usb: 'USB',
            bluetooth: 'Bluetooth',
            network: 'Réseau',
            virtual: 'Virtuel'
        };
        return labels[type] || 'Inconnu';
    }

    getMockDevices(type) {
        // Mock data pour le développement
        const mockDevices = {
            usb: [
                { id: 'usb1', name: 'Roland FP-30X', type: 'usb', port: '/dev/usb1' },
                { id: 'usb2', name: 'Yamaha P-125', type: 'usb', port: '/dev/usb2' }
            ],
            bluetooth: [
                { id: 'bt1', name: 'MIDI Bluetooth 1', type: 'bluetooth', signalStrength: 85, battery: 75 },
                { id: 'bt2', name: 'MIDI Bluetooth 2', type: 'bluetooth', signalStrength: 92, battery: 100 }
            ],
            network: [
                { id: 'net1', name: 'rtpMIDI Session', type: 'network', ip: '192.168.1.100', port: 5004 }
            ]
        };
        
        return mockDevices[type] || [];
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    destroy() {
        if (this.eventBus) {
            this.eventBus.off('instruments:scan_started');
            this.eventBus.off('instruments:scan_completed');
            this.eventBus.off('instruments:device_connected');
            this.eventBus.off('instruments:device_disconnected');
            this.eventBus.off('instruments:list_updated');
        }
        
        this.logger.info('[InstrumentView] Destroyed');
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
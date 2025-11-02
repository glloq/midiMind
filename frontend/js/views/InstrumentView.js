// ============================================================================
// Fichier: frontend/js/views/InstrumentView.js
// Version: v4.0.0 - CONFORMITÉ API DOCUMENTATION
// Date: 2025-11-02
// ============================================================================
// AMÉLIORATIONS v4.0.0:
// ✅ API v4.2.2: devices.*, bluetooth.*
// ✅ Événements device:connected, device:disconnected
// ✅ Hot-plug support (devices.startHotPlug, devices.stopHotPlug)
// ✅ Bluetooth pairing et scanning
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
            availableDevices: [],
            bluetoothDevices: [],
            scanning: {
                usb: false,
                bluetooth: false
            },
            hotPlugEnabled: false,
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
        this.loadDevices();
        this.checkHotPlugStatus();
        
        this.logger.info('[InstrumentView] Initialized v4.0.0');
    }

    render() {
        if (!this.container) return;
        
        this.container.innerHTML = `
            <div class="page-header">
                <h1>🎸 Gestion des Instruments</h1>
                <div class="header-actions">
                    <button class="btn-hotplug" id="btnToggleHotPlug" 
                            data-enabled="${this.state.hotPlugEnabled}">
                        ${this.state.hotPlugEnabled ? '🔌 Hot-Plug ON' : '🔌 Hot-Plug OFF'}
                    </button>
                </div>
            </div>
            
            <div class="instruments-layout">
                <!-- Scan et découverte -->
                <div class="instruments-discover">
                    <div class="section-header">
                        <h2>Rechercher et connecter</h2>
                        <div class="discover-controls">
                            <button class="btn-scan ${this.state.scanning.usb ? 'scanning' : ''}" 
                                    id="btnScanUSB" data-type="usb">
                                🔌 ${this.state.scanning.usb ? 'Scan...' : 'Scan USB'}
                            </button>
                            <button class="btn-scan ${this.state.scanning.bluetooth ? 'scanning' : ''}" 
                                    id="btnScanBluetooth" data-type="bluetooth">
                                📡 ${this.state.scanning.bluetooth ? 'Scan...' : 'Scan Bluetooth'}
                            </button>
                        </div>
                    </div>
                    
                    <div class="devices-found" id="devicesFound">
                        ${this.renderAvailableDevices()}
                    </div>
                    
                    <!-- Bluetooth paired devices -->
                    <div class="bluetooth-paired" id="bluetoothPaired">
                        ${this.renderBluetoothDevices()}
                    </div>
                </div>
                
                <!-- Instruments connectés -->
                <div class="instruments-connected">
                    <div class="section-header">
                        <h2>Instruments connectés</h2>
                        <button class="btn-disconnect-all" id="btnDisconnectAll">
                            🔌 Tout déconnecter
                        </button>
                    </div>
                    
                    <div class="devices-list" id="connectedDevices">
                        ${this.renderConnectedDevices()}
                    </div>
                </div>
            </div>
        `;
    }

    cacheElements() {
        this.elements = {
            btnScanUSB: document.getElementById('btnScanUSB'),
            btnScanBluetooth: document.getElementById('btnScanBluetooth'),
            btnToggleHotPlug: document.getElementById('btnToggleHotPlug'),
            btnDisconnectAll: document.getElementById('btnDisconnectAll'),
            devicesFound: document.getElementById('devicesFound'),
            connectedDevices: document.getElementById('connectedDevices'),
            bluetoothPaired: document.getElementById('bluetoothPaired')
        };
    }

    attachEvents() {
        // Scan buttons
        if (this.elements.btnScanUSB) {
            this.elements.btnScanUSB.addEventListener('click', () => this.scanDevices(true));
        }
        if (this.elements.btnScanBluetooth) {
            this.elements.btnScanBluetooth.addEventListener('click', () => this.scanBluetooth());
        }
        
        // Hot-plug toggle
        if (this.elements.btnToggleHotPlug) {
            this.elements.btnToggleHotPlug.addEventListener('click', () => this.toggleHotPlug());
        }
        
        // Disconnect all
        if (this.elements.btnDisconnectAll) {
            this.elements.btnDisconnectAll.addEventListener('click', () => this.disconnectAll());
        }
        
        // Délégation d'événements
        if (this.elements.devicesFound) {
            this.elements.devicesFound.addEventListener('click', (e) => this.handleAvailableDeviceAction(e));
        }
        if (this.elements.connectedDevices) {
            this.elements.connectedDevices.addEventListener('click', (e) => this.handleConnectedDeviceAction(e));
        }
        if (this.elements.bluetoothPaired) {
            this.elements.bluetoothPaired.addEventListener('click', (e) => this.handleBluetoothAction(e));
        }
        
        // EventBus
        this.setupEventBusListeners();
    }

    setupEventBusListeners() {
        if (!this.eventBus) return;
        
        // devices.list response
        this.eventBus.on('devices:listed', (data) => {
            this.updateDevicesFromList(data.devices || []);
        });
        
        // device:connected event
        this.eventBus.on('device:connected', (data) => {
            this.handleDeviceConnected(data);
        });
        
        // device:disconnected event
        this.eventBus.on('device:disconnected', (data) => {
            this.handleDeviceDisconnected(data);
        });
        
        // devices.scan response
        this.eventBus.on('devices:scanned', (data) => {
            this.state.availableDevices = data.devices || [];
            this.state.scanning.usb = false;
            this.renderAvailableDevicesList();
        });
        
        // bluetooth.scan response
        this.eventBus.on('bluetooth:scanned', (data) => {
            this.state.availableDevices = data.devices || [];
            this.state.scanning.bluetooth = false;
            this.renderAvailableDevicesList();
        });
        
        // bluetooth.paired response
        this.eventBus.on('bluetooth:paired_list', (data) => {
            this.state.bluetoothDevices = data.devices || [];
            this.renderBluetoothDevicesList();
        });
        
        // hot-plug status
        this.eventBus.on('hotplug:status', (data) => {
            this.state.hotPlugEnabled = data.enabled || false;
            this.updateHotPlugButton();
        });
    }

    // ========================================================================
    // RENDERING - AVAILABLE DEVICES
    // ========================================================================

    renderAvailableDevices() {
        const devices = this.state.availableDevices;
        
        if (devices.length === 0) {
            return `
                <div class="devices-empty">
                    <div class="empty-icon">🔍</div>
                    <p>Aucun device trouvé</p>
                    <p class="text-muted">Cliquez sur Scan pour rechercher</p>
                </div>
            `;
        }
        
        return `
            <div class="devices-grid">
                ${devices.map(device => this.renderAvailableDeviceCard(device)).join('')}
            </div>
        `;
    }

    renderAvailableDeviceCard(device) {
        const typeIcons = {
            0: '❓', // Unknown
            1: '🔌', // USB
            2: '📡', // BLE
            3: '💻'  // Virtual
        };
        
        const icon = typeIcons[device.type] || '🎸';
        const typeName = ['Unknown', 'USB', 'Bluetooth', 'Virtual'][device.type] || 'Unknown';
        
        return `
            <div class="device-card available" data-device-id="${device.id}">
                <div class="device-icon">${icon}</div>
                <div class="device-info">
                    <div class="device-name">${device.name}</div>
                    <div class="device-type">${typeName}</div>
                </div>
                <div class="device-actions">
                    <button class="btn-connect" data-action="connect-device">
                        Connecter
                    </button>
                </div>
            </div>
        `;
    }

    renderAvailableDevicesList() {
        if (this.elements.devicesFound) {
            this.elements.devicesFound.innerHTML = this.renderAvailableDevices();
        }
    }

    // ========================================================================
    // RENDERING - CONNECTED DEVICES
    // ========================================================================

    renderConnectedDevices() {
        const devices = this.state.connectedDevices;
        
        if (devices.length === 0) {
            return `
                <div class="devices-empty">
                    <div class="empty-icon">🎸</div>
                    <p>Aucun instrument connecté</p>
                    <p class="text-muted">Connectez un device pour commencer</p>
                </div>
            `;
        }
        
        return `
            <div class="devices-list">
                ${devices.map(device => this.renderConnectedDeviceCard(device)).join('')}
            </div>
        `;
    }

    renderConnectedDeviceCard(device) {
        const typeIcons = {
            0: '❓',
            1: '🔌',
            2: '📡',
            3: '💻'
        };
        
        const icon = typeIcons[device.type] || '🎸';
        const typeName = ['Unknown', 'USB', 'Bluetooth', 'Virtual'][device.type] || 'Unknown';
        
        return `
            <div class="device-card connected" data-device-id="${device.id}">
                <div class="device-icon">${icon}</div>
                <div class="device-info">
                    <div class="device-name">${device.name}</div>
                    <div class="device-type">${typeName}</div>
                    <div class="device-status connected">✓ Connecté</div>
                </div>
                <div class="device-actions">
                    <button class="btn-info" data-action="device-info">
                        ℹ️ Info
                    </button>
                    <button class="btn-disconnect" data-action="disconnect-device">
                        Déconnecter
                    </button>
                </div>
            </div>
        `;
    }

    renderConnectedDevicesList() {
        if (this.elements.connectedDevices) {
            this.elements.connectedDevices.innerHTML = this.renderConnectedDevices();
        }
    }

    // ========================================================================
    // RENDERING - BLUETOOTH DEVICES
    // ========================================================================

    renderBluetoothDevices() {
        const devices = this.state.bluetoothDevices;
        
        if (devices.length === 0) {
            return '';
        }
        
        return `
            <div class="bluetooth-section">
                <h3>Devices Bluetooth appairés</h3>
                <div class="bluetooth-list">
                    ${devices.map(device => this.renderBluetoothDeviceCard(device)).join('')}
                </div>
            </div>
        `;
    }

    renderBluetoothDeviceCard(device) {
        return `
            <div class="device-card bluetooth" data-device-id="${device.id}">
                <div class="device-icon">📡</div>
                <div class="device-info">
                    <div class="device-name">${device.name}</div>
                    <div class="device-address">${device.address || '—'}</div>
                </div>
                <div class="device-actions">
                    <button class="btn-connect" data-action="connect-bluetooth">
                        Connecter
                    </button>
                    <button class="btn-forget" data-action="forget-bluetooth">
                        Oublier
                    </button>
                </div>
            </div>
        `;
    }

    renderBluetoothDevicesList() {
        if (this.elements.bluetoothPaired) {
            this.elements.bluetoothPaired.innerHTML = this.renderBluetoothDevices();
        }
    }

    // ========================================================================
    // ACTIONS - SCAN
    // ========================================================================

    async scanDevices(fullScan = true) {
        this.state.scanning.usb = true;
        this.render();
        
        // Appel API: devices.scan
        if (this.eventBus) {
            this.eventBus.emit('devices:scan_requested', {
                full_scan: fullScan
            });
        }
    }

    async scanBluetooth() {
        this.state.scanning.bluetooth = true;
        this.render();
        
        // Appel API: bluetooth.scan
        if (this.eventBus) {
            this.eventBus.emit('bluetooth:scan_requested');
        }
    }

    // ========================================================================
    // ACTIONS - CONNECT/DISCONNECT
    // ========================================================================

    handleAvailableDeviceAction(e) {
        const action = e.target.closest('[data-action]')?.dataset.action;
        if (!action) return;
        
        const deviceCard = e.target.closest('.device-card');
        const deviceId = deviceCard?.dataset.deviceId;
        
        if (!deviceId) return;
        
        if (action === 'connect-device') {
            this.connectDevice(deviceId);
        }
    }

    handleConnectedDeviceAction(e) {
        const action = e.target.closest('[data-action]')?.dataset.action;
        if (!action) return;
        
        const deviceCard = e.target.closest('.device-card');
        const deviceId = deviceCard?.dataset.deviceId;
        
        if (!deviceId) return;
        
        switch (action) {
            case 'disconnect-device':
                this.disconnectDevice(deviceId);
                break;
            case 'device-info':
                this.showDeviceInfo(deviceId);
                break;
        }
    }

    handleBluetoothAction(e) {
        const action = e.target.closest('[data-action]')?.dataset.action;
        if (!action) return;
        
        const deviceCard = e.target.closest('.device-card');
        const deviceId = deviceCard?.dataset.deviceId;
        
        if (!deviceId) return;
        
        switch (action) {
            case 'connect-bluetooth':
                this.connectDevice(deviceId);
                break;
            case 'forget-bluetooth':
                this.forgetBluetooth(deviceId);
                break;
        }
    }

    async connectDevice(deviceId) {
        // Appel API: devices.connect
        if (this.eventBus) {
            this.eventBus.emit('devices:connect_requested', {
                device_id: deviceId
            });
        }
    }

    async disconnectDevice(deviceId) {
        // Appel API: devices.disconnect
        if (this.eventBus) {
            this.eventBus.emit('devices:disconnect_requested', {
                device_id: deviceId
            });
        }
    }

    async disconnectAll() {
        if (!confirm('Déconnecter tous les instruments ?')) return;
        
        // Appel API: devices.disconnectAll
        if (this.eventBus) {
            this.eventBus.emit('devices:disconnect_all_requested');
        }
    }

    async forgetBluetooth(deviceId) {
        if (!confirm('Oublier cet appareil Bluetooth ?')) return;
        
        // Appel API: bluetooth.forget
        if (this.eventBus) {
            this.eventBus.emit('bluetooth:forget_requested', {
                device_id: deviceId
            });
        }
    }

    async showDeviceInfo(deviceId) {
        // Appel API: devices.getInfo
        if (this.eventBus) {
            this.eventBus.emit('devices:info_requested', {
                device_id: deviceId
            });
        }
    }

    // ========================================================================
    // HOT-PLUG
    // ========================================================================

    async toggleHotPlug() {
        const newState = !this.state.hotPlugEnabled;
        
        // Appel API: devices.startHotPlug ou devices.stopHotPlug
        if (this.eventBus) {
            if (newState) {
                this.eventBus.emit('hotplug:start_requested');
            } else {
                this.eventBus.emit('hotplug:stop_requested');
            }
        }
    }

    async checkHotPlugStatus() {
        // Appel API: devices.getHotPlugStatus
        if (this.eventBus) {
            this.eventBus.emit('hotplug:status_requested');
        }
    }

    updateHotPlugButton() {
        if (this.elements.btnToggleHotPlug) {
            this.elements.btnToggleHotPlug.textContent = 
                this.state.hotPlugEnabled ? '🔌 Hot-Plug ON' : '🔌 Hot-Plug OFF';
            this.elements.btnToggleHotPlug.dataset.enabled = this.state.hotPlugEnabled;
        }
    }

    // ========================================================================
    // EVENT HANDLERS
    // ========================================================================

    handleDeviceConnected(data) {
        this.logger.info(`[InstrumentView] Device connected: ${data.device_id}`);
        this.loadDevices();
    }

    handleDeviceDisconnected(data) {
        this.logger.info(`[InstrumentView] Device disconnected: ${data.device_id}`);
        this.loadDevices();
    }

    updateDevicesFromList(devices) {
        // Séparer connectés et disponibles
        this.state.connectedDevices = devices.filter(d => d.status === 2); // Connected
        this.state.availableDevices = devices.filter(d => d.status !== 2 && d.available);
        
        this.renderConnectedDevicesList();
        this.renderAvailableDevicesList();
    }

    // ========================================================================
    // LOADING
    // ========================================================================

    async loadDevices() {
        // Appel API: devices.list
        if (this.eventBus) {
            this.eventBus.emit('devices:list_requested');
        }
    }

    async loadBluetoothPaired() {
        // Appel API: bluetooth.paired
        if (this.eventBus) {
            this.eventBus.emit('bluetooth:paired_requested');
        }
    }

    // ========================================================================
    // CLEANUP
    // ========================================================================

    destroy() {
        if (this.eventBus) {
            this.eventBus.off('devices:listed');
            this.eventBus.off('device:connected');
            this.eventBus.off('device:disconnected');
            this.eventBus.off('devices:scanned');
            this.eventBus.off('bluetooth:scanned');
            this.eventBus.off('bluetooth:paired_list');
            this.eventBus.off('hotplug:status');
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
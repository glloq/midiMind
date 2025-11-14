// ============================================================================
// Fichier: frontend/js/views/InstrumentView.js
// Version: v5.0.0 - BACKEND API ONLY (No WebMIDI)
// Date: 2025-11-14
// ============================================================================
// CORRECTIONS v5.0.0:
// ✅ CRITIQUE: Suppression complète du code WebMIDI (gestion backend uniquement)
// ✅ CRITIQUE: Ajout de 3 boutons séparés: USB, Network/WiFi, Bluetooth
// ✅ NOUVELLE FONCTION: Interface de modification des réglages instruments
// ✅ Fix: Toutes les connexions passent par l'API backend
// ============================================================================

class InstrumentView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        this.logger = window.logger || console;
        
        // État interne (init/render flags dans this.state hérité de BaseView)
        this.state = {
            initialized: false,   // ✅ FIX v4.1.5: Ajout flags initialisation
            rendered: false,      // ✅ FIX v4.1.5: Ajout flag rendu
            eventListenersAttached: false,  // ✅ FIX: Prevent infinite loop in setupEventBusListeners
        };

        this.viewState = {
            connectedDevices: [],
            availableDevices: [],
            bluetoothDevices: [],
            scanning: {
                usb: false,
                network: false,
                bluetooth: false
            },
            hotPlugEnabled: false,
            selectedDevice: null,
            editingDevice: null
        };
        
        // Éléments DOM
        this.elements = {};
        
        this.log('info', '[InstrumentView]', '✦ InstrumentView v5.0.0 initialized (Backend API Only)');
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    init() {
        // ✅ FIX v4.1.3: Early return si déjà initialisé
        if (this.state.initialized) {
            this.log('warn', '[InstrumentView]', 'Already initialized, skipping');
            return;
        }

        if (!this.container) {
            this.log('error', '[InstrumentView]', 'Cannot initialize: container not found (#instrument-view)');
            return;
        }

        this.render();
        this.cacheElements();
        this.attachEvents();
        this.loadDevices();
        this.checkHotPlugStatus();

        // ✅ FIX v4.1.3: Marquer comme initialisé
        this.state.initialized = true;
        this.state.rendered = true;

        this.log('info', '[InstrumentView]', 'Initialized v4.1.4');
    }

    render() {
        if (!this.container) {
            this.log('error', '[InstrumentView]', 'Cannot render: container not found');
            return;
        }

        // ✅ FIX v4.1.4: Ne pas re-render si déjà rendu
        if (this.state.rendered) {
            return;
        }

        this.container.innerHTML = `
            <div class="page-header">
                <h1>🎸 Gestion des Instruments</h1>
                <div class="header-actions">
                    <button class="btn-hotplug" id="btnToggleHotPlug"
                            data-enabled="${this.viewState.hotPlugEnabled}">
                        ${this.viewState.hotPlugEnabled ? '🔌 Hot-Plug ON' : '🔌 Hot-Plug OFF'}
                    </button>
                </div>
            </div>

            <div class="instruments-layout">
                <!-- Scan et découverte (Backend API) -->
                <div class="instruments-discover">
                    <div class="section-header">
                        <h2>🔍 Rechercher des instruments</h2>
                        <div class="discover-controls">
                            <button class="btn-scan ${this.viewState.scanning.usb ? 'scanning' : ''}"
                                    id="btnScanUSB" data-type="usb" title="Scanner les périphériques USB MIDI">
                                🔌 ${this.viewState.scanning.usb ? 'Scan...' : 'USB'}
                            </button>
                            <button class="btn-scan ${this.viewState.scanning.network ? 'scanning' : ''}"
                                    id="btnScanNetwork" data-type="network" title="Scanner le réseau/WiFi">
                                🌐 ${this.viewState.scanning.network ? 'Scan...' : 'Network/WiFi'}
                            </button>
                            <button class="btn-scan ${this.viewState.scanning.bluetooth ? 'scanning' : ''}"
                                    id="btnScanBluetooth" data-type="bluetooth" title="Scanner Bluetooth LE">
                                📡 ${this.viewState.scanning.bluetooth ? 'Scan...' : 'Bluetooth'}
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
                        <h2>📱 Instruments connectés</h2>
                        <button class="btn-disconnect-all" id="btnDisconnectAll">
                            🔌 Tout déconnecter
                        </button>
                    </div>

                    <div class="devices-list" id="connectedDevices">
                        ${this.renderConnectedDevices()}
                    </div>
                </div>
            </div>

            <!-- Modal de modification des réglages -->
            <div class="modal" id="settingsModal" style="display: none;">
                <div class="modal-content">
                    <div class="modal-header">
                        <h2>⚙️ Réglages de l'instrument</h2>
                        <button class="btn-close-modal" id="btnCloseModal">✕</button>
                    </div>
                    <div class="modal-body" id="settingsModalBody">
                        <!-- Contenu dynamique -->
                    </div>
                </div>
            </div>
        `;
    }

    show() {
        if (this.container) {
            this.container.style.display = 'block';
            this.state.visible = true;
            this.log('debug', '[InstrumentView]', 'Showing view');
        } else {
            this.log('error', '[InstrumentView]', 'Cannot show: container not found');
        }
    }

    hide() {
        if (this.container) {
            this.container.style.display = 'none';
            this.state.visible = false;
        }
    }

    cacheElements() {
        this.elements = {
            btnScanUSB: document.getElementById('btnScanUSB'),
            btnScanNetwork: document.getElementById('btnScanNetwork'),
            btnScanBluetooth: document.getElementById('btnScanBluetooth'),
            btnToggleHotPlug: document.getElementById('btnToggleHotPlug'),
            btnDisconnectAll: document.getElementById('btnDisconnectAll'),
            devicesFound: document.getElementById('devicesFound'),
            connectedDevices: document.getElementById('connectedDevices'),
            bluetoothPaired: document.getElementById('bluetoothPaired'),
            settingsModal: document.getElementById('settingsModal'),
            settingsModalBody: document.getElementById('settingsModalBody'),
            btnCloseModal: document.getElementById('btnCloseModal')
        };
    }

    attachEvents() {
        // Scan buttons
        if (this.elements.btnScanUSB) {
            this.elements.btnScanUSB.addEventListener('click', () => this.scanUSB());
        }
        if (this.elements.btnScanNetwork) {
            this.elements.btnScanNetwork.addEventListener('click', () => this.scanNetwork());
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

        // Modal close
        if (this.elements.btnCloseModal) {
            this.elements.btnCloseModal.addEventListener('click', () => this.closeSettingsModal());
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

        // ✅ FIX CRITICAL: Prevent infinite loop - only attach listeners once
        if (this.state.eventListenersAttached) {
            this.log('debug', '[InstrumentView]', 'Event listeners already attached, skipping');
            return;
        }

        this.state.eventListenersAttached = true;
        this.log('debug', '[InstrumentView]', 'Attaching event bus listeners');

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

        // devices.scan response (USB or Network)
        this.eventBus.on('devices:scanned', (data) => {
            const scanType = data.scan_type || 'usb'; // usb, network, or all
            this.viewState.availableDevices = data.devices || [];
            this.viewState.scanning.usb = false;
            this.viewState.scanning.network = false;
            this.renderAvailableDevicesList();
        });

        // bluetooth.scan response
        this.eventBus.on('bluetooth:scanned', (data) => {
            this.viewState.availableDevices = data.devices || [];
            this.viewState.scanning.bluetooth = false;
            this.renderAvailableDevicesList();
        });

        // bluetooth.paired response
        this.eventBus.on('bluetooth:paired_list', (data) => {
            this.viewState.bluetoothDevices = data.devices || [];
            this.renderBluetoothDevicesList();
        });

        // hot-plug status
        this.eventBus.on('hotplug:status', (data) => {
            this.viewState.hotPlugEnabled = data.enabled || false;
            this.updateHotPlugButton();
        });
    }

    // ========================================================================
    // RENDERING - AVAILABLE DEVICES
    // ========================================================================

    renderAvailableDevices() {
        const devices = this.viewState.availableDevices;
        
        if (devices.length === 0 && !this.viewState.scanning.usb && !this.viewState.scanning.bluetooth) {
            return `
                <div class="devices-empty">
                    <div class="empty-icon">🔍</div>
                    <p>Aucun périphérique détecté</p>
                    <p class="text-muted">Cliquez sur "Scan USB" ou "Scan Bluetooth" pour rechercher</p>
                </div>
            `;
        }
        
        if (this.viewState.scanning.usb || this.viewState.scanning.bluetooth) {
            return `
                <div class="devices-scanning">
                    <div class="spinner"></div>
                    <p>Recherche en cours...</p>
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
        const typeIcon = device.type === 'usb' ? '🔌' : 
                        device.type === 'bluetooth' ? '📡' : 
                        device.type === 'network' ? '🌐' : '🎹';
        
        return `
            <div class="device-card available" data-device-id="${device.id}">
                <div class="device-icon">${typeIcon}</div>
                <div class="device-info">
                    <div class="device-name">${this.escapeHtml(device.name)}</div>
                    <div class="device-type">${device.type.toUpperCase()}</div>
                    ${device.ports ? `<div class="device-ports">${device.ports.in}→${device.ports.out}</div>` : ''}
                </div>
                <div class="device-actions">
                    <button class="btn-connect" data-action="connect">Connecter</button>
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
        const devices = this.viewState.connectedDevices;
        
        if (devices.length === 0) {
            return `
                <div class="devices-empty">
                    <div class="empty-icon">🎸</div>
                    <p>Aucun instrument connecté</p>
                    <p class="text-muted">Connectez des périphériques MIDI pour commencer</p>
                </div>
            `;
        }
        
        return `
            <div class="devices-grid">
                ${devices.map(device => this.renderConnectedDeviceCard(device)).join('')}
            </div>
        `;
    }

    renderConnectedDeviceCard(device) {
        const typeIcon = device.type === 'usb' ? '🔌' :
                        device.type === 'bluetooth' ? '📡' :
                        device.type === 'network' ? '🌐' : '🎹';

        const statusClass = device.active ? 'active' : 'idle';

        return `
            <div class="device-card connected ${statusClass}" data-device-id="${device.id}">
                <div class="device-icon">${typeIcon}</div>
                <div class="device-info">
                    <div class="device-name">${this.escapeHtml(device.name)}</div>
                    <div class="device-status">
                        <span class="status-indicator ${statusClass}"></span>
                        <span class="status-text">${device.active ? 'Actif' : 'Inactif'}</span>
                    </div>
                    ${device.ports ? `<div class="device-ports">${device.ports.in}→${device.ports.out}</div>` : ''}
                </div>
                <div class="device-actions">
                    <button class="btn-settings" data-action="settings" title="Réglages">⚙️</button>
                    <button class="btn-test" data-action="test" title="Tester">🎵</button>
                    <button class="btn-disconnect" data-action="disconnect" title="Déconnecter">🔌</button>
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
        const devices = this.viewState.bluetoothDevices;
        
        if (devices.length === 0) {
            return '';
        }
        
        return `
            <div class="bluetooth-section">
                <h3>Appareils Bluetooth appairés</h3>
                <div class="devices-grid">
                    ${devices.map(device => this.renderBluetoothDeviceCard(device)).join('')}
                </div>
            </div>
        `;
    }

    renderBluetoothDeviceCard(device) {
        return `
            <div class="device-card bluetooth" data-device-address="${device.address}">
                <div class="device-icon">📡</div>
                <div class="device-info">
                    <div class="device-name">${this.escapeHtml(device.name || device.address)}</div>
                    <div class="device-address">${device.address}</div>
                </div>
                <div class="device-actions">
                    <button class="btn-pair" data-action="pair">Connecter</button>
                    <button class="btn-unpair" data-action="unpair">Oublier</button>
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
    // ACTIONS
    // ========================================================================

    scanUSB() {
        this.viewState.scanning.usb = true;
        this.renderAvailableDevicesList();

        if (this.eventBus) {
            this.eventBus.emit('devices:scan_requested', { connection_type: 'usb' });
        }
    }

    scanNetwork() {
        this.viewState.scanning.network = true;
        this.renderAvailableDevicesList();

        if (this.eventBus) {
            this.eventBus.emit('devices:scan_requested', { connection_type: 'network' });
        }
    }

    scanBluetooth() {
        this.viewState.scanning.bluetooth = true;
        this.renderAvailableDevicesList();

        if (this.eventBus) {
            this.eventBus.emit('bluetooth:scan_requested');
        }
    }

    toggleHotPlug() {
        if (this.eventBus) {
            this.eventBus.emit('hotplug:toggle_requested');
        }
    }

    disconnectAll() {
        if (confirm('Déconnecter tous les instruments ?')) {
            if (this.eventBus) {
                this.eventBus.emit('devices:disconnect_all_requested');
            }
        }
    }

    handleAvailableDeviceAction(e) {
        const action = e.target.dataset.action;
        if (!action) return;
        
        const card = e.target.closest('.device-card');
        const deviceId = card?.dataset.deviceId;
        
        if (action === 'connect' && deviceId) {
            this.connectDevice(deviceId);
        }
    }

    handleConnectedDeviceAction(e) {
        const action = e.target.dataset.action;
        if (!action) return;

        const card = e.target.closest('.device-card');
        const deviceId = card?.dataset.deviceId;

        if (!deviceId) return;

        switch (action) {
            case 'settings':
                this.showDeviceSettings(deviceId);
                break;
            case 'test':
                this.testDevice(deviceId);
                break;
            case 'disconnect':
                this.disconnectDevice(deviceId);
                break;
        }
    }

    handleBluetoothAction(e) {
        const action = e.target.dataset.action;
        if (!action) return;

        const card = e.target.closest('.device-card');
        const address = card?.dataset.deviceAddress;

        if (!address) return;

        switch (action) {
            case 'pair':
                this.pairBluetoothDevice(address);
                break;
            case 'unpair':
                this.unpairBluetoothDevice(address);
                break;
        }
    }

    connectDevice(deviceId) {
        if (this.eventBus) {
            this.eventBus.emit('device:connect_requested', { device_id: deviceId });
        }
    }

    disconnectDevice(deviceId) {
        if (this.eventBus) {
            this.eventBus.emit('device:disconnect_requested', { device_id: deviceId });
        }
    }

    testDevice(deviceId) {
        if (this.eventBus) {
            this.eventBus.emit('device:test_requested', { device_id: deviceId });
        }
    }

    pairBluetoothDevice(address) {
        if (this.eventBus) {
            this.eventBus.emit('bluetooth:pair_requested', { address });
        }
    }

    unpairBluetoothDevice(address) {
        if (this.eventBus) {
            this.eventBus.emit('bluetooth:unpair_requested', { address });
        }
    }

    // ========================================================================
    // SETTINGS MODAL
    // ========================================================================

    showDeviceSettings(deviceId) {
        const device = this.viewState.connectedDevices.find(d => d.id === deviceId);
        if (!device) {
            this.log('error', '[InstrumentView]', `Device ${deviceId} not found`);
            return;
        }

        this.viewState.editingDevice = device;

        // Demander les infos détaillées au backend
        if (this.eventBus) {
            this.eventBus.emit('device:info_requested', { device_id: deviceId });
        }

        // Afficher le modal avec les infos actuelles
        this.renderSettingsModal(device);
        this.openSettingsModal();
    }

    renderSettingsModal(device) {
        if (!this.elements.settingsModalBody) return;

        this.elements.settingsModalBody.innerHTML = `
            <form id="deviceSettingsForm" class="settings-form">
                <div class="form-group">
                    <label for="deviceName">Nom de l'instrument</label>
                    <input type="text" id="deviceName" name="name"
                           value="${this.escapeHtml(device.name)}" required>
                </div>

                <div class="form-group">
                    <label for="deviceType">Type de connexion</label>
                    <input type="text" id="deviceType" name="type"
                           value="${device.type.toUpperCase()}" disabled>
                </div>

                <div class="form-group">
                    <label for="latencyOffset">Compensation de latence (µs)</label>
                    <input type="number" id="latencyOffset" name="latency_offset"
                           value="${device.latency_offset || 0}" step="100">
                    <small class="form-hint">Délai pour compenser la latence de l'instrument</small>
                </div>

                <div class="form-group">
                    <label for="autoCalibration">
                        <input type="checkbox" id="autoCalibration" name="auto_calibration"
                               ${device.auto_calibration ? 'checked' : ''}>
                        Calibration automatique
                    </label>
                    <small class="form-hint">Ajuster automatiquement la latence</small>
                </div>

                <div class="form-group">
                    <label for="deviceEnabled">
                        <input type="checkbox" id="deviceEnabled" name="enabled"
                               ${device.enabled !== false ? 'checked' : ''}>
                        Instrument activé
                    </label>
                </div>

                <div class="form-actions">
                    <button type="button" class="btn-cancel" id="btnCancelSettings">Annuler</button>
                    <button type="submit" class="btn-save">💾 Enregistrer</button>
                </div>
            </form>
        `;

        // Attacher les événements
        const form = document.getElementById('deviceSettingsForm');
        const btnCancel = document.getElementById('btnCancelSettings');

        if (form) {
            form.addEventListener('submit', (e) => {
                e.preventDefault();
                this.saveDeviceSettings(device.id);
            });
        }

        if (btnCancel) {
            btnCancel.addEventListener('click', () => this.closeSettingsModal());
        }
    }

    saveDeviceSettings(deviceId) {
        const form = document.getElementById('deviceSettingsForm');
        if (!form) return;

        const formData = new FormData(form);
        const settings = {
            device_id: deviceId,
            name: formData.get('name'),
            latency_offset: parseInt(formData.get('latency_offset')) || 0,
            auto_calibration: formData.get('auto_calibration') === 'on',
            enabled: formData.get('enabled') === 'on'
        };

        if (this.eventBus) {
            this.eventBus.emit('device:settings_update_requested', settings);
        }

        this.closeSettingsModal();
    }

    openSettingsModal() {
        if (this.elements.settingsModal) {
            this.elements.settingsModal.style.display = 'flex';
        }
    }

    closeSettingsModal() {
        if (this.elements.settingsModal) {
            this.elements.settingsModal.style.display = 'none';
        }
        this.viewState.editingDevice = null;
    }

    // ========================================================================
    // DEVICE UPDATES
    // ========================================================================

    updateDevicesFromList(devices) {
        this.viewState.connectedDevices = devices.filter(d => d.connected);
        this.renderConnectedDevicesList();
    }

    handleDeviceConnected(data) {
        const device = data.device;
        if (!device) return;
        
        // Ajouter aux connectés
        const exists = this.viewState.connectedDevices.find(d => d.id === device.id);
        if (!exists) {
            this.viewState.connectedDevices.push(device);
            this.renderConnectedDevicesList();
        }
        
        // Retirer des disponibles
        this.viewState.availableDevices = this.viewState.availableDevices.filter(d => d.id !== device.id);
        this.renderAvailableDevicesList();
    }

    handleDeviceDisconnected(data) {
        const deviceId = data.device_id;
        if (!deviceId) return;
        
        // Retirer des connectés
        this.viewState.connectedDevices = this.viewState.connectedDevices.filter(d => d.id !== deviceId);
        this.renderConnectedDevicesList();
    }

    updateHotPlugButton() {
        if (this.elements.btnToggleHotPlug) {
            this.elements.btnToggleHotPlug.dataset.enabled = this.viewState.hotPlugEnabled;
            this.elements.btnToggleHotPlug.textContent = this.viewState.hotPlugEnabled ? 
                '🔌 Hot-Plug ON' : '🔌 Hot-Plug OFF';
        }
    }

    // ========================================================================
    // DATA LOADING
    // ========================================================================

    loadDevices() {
        if (this.eventBus) {
            this.eventBus.emit('devices:list_requested');
        }
    }

    checkHotPlugStatus() {
        if (this.eventBus) {
            this.eventBus.emit('hotplug:status_requested');
        }
    }

    // ========================================================================
    // UTILITY
    // ========================================================================

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
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
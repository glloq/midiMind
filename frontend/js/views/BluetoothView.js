// ============================================================================
// Fichier: frontend/js/views/BluetoothView.js
// Version: v4.0.0
// ============================================================================

class BluetoothView {
    constructor(containerId, eventBus) {
        this.container = typeof containerId === 'string' ? 
            document.getElementById(containerId) : containerId;
        this.eventBus = eventBus;
        
        this.state = {
            scanning: false,
            pairedDevices: [],
            availableDevices: [],
            selectedDevice: null
        };
    }
    
    init() {
        if (!this.container) return;
        this.render();
        this.attachEvents();
        this.loadPairedDevices();
    }
    
    render() {
        this.container.innerHTML = `
            <div class="bluetooth-view">
                <div class="bt-header">
                    <h2>📡 Bluetooth MIDI</h2>
                    <button data-action="scan" ${this.state.scanning ? 'disabled' : ''}>
                        ${this.state.scanning ? 'Scan...' : 'Scanner'}
                    </button>
                </div>
                
                <div class="bt-paired">
                    <h3>Appareils appairés</h3>
                    <div class="device-list">
                        ${this.state.pairedDevices.length > 0 ? 
                            this.state.pairedDevices.map(d => this.renderDevice(d, true)).join('') :
                            '<p class="empty">Aucun appareil appairé</p>'}
                    </div>
                </div>
                
                <div class="bt-available">
                    <h3>Appareils disponibles</h3>
                    <div class="device-list">
                        ${this.state.availableDevices.length > 0 ?
                            this.state.availableDevices.map(d => this.renderDevice(d, false)).join('') :
                            '<p class="empty">Lancez un scan</p>'}
                    </div>
                </div>
            </div>
        `;
    }
    
    renderDevice(device, isPaired) {
        return `
            <div class="bt-device" data-device-id="${device.id}">
                <div class="device-info">
                    <div class="device-name">${device.name}</div>
                    <div class="device-address">${device.address || '—'}</div>
                    ${device.signal ? `<div class="device-signal">Signal: ${device.signal}%</div>` : ''}
                </div>
                <div class="device-actions">
                    ${isPaired ? `
                        <button data-action="connect">Connecter</button>
                        <button data-action="forget">Oublier</button>
                    ` : `
                        <button data-action="pair">Appairer</button>
                    `}
                </div>
            </div>
        `;
    }
    
    attachEvents() {
        this.container.addEventListener('click', (e) => {
            const action = e.target.dataset.action;
            const deviceEl = e.target.closest('.bt-device');
            const deviceId = deviceEl?.dataset.deviceId;
            
            switch(action) {
                case 'scan': this.scan(); break;
                case 'pair': if (deviceId) this.pair(deviceId); break;
                case 'connect': if (deviceId) this.connect(deviceId); break;
                case 'forget': if (deviceId) this.forget(deviceId); break;
            }
        });
        
        if (!this.eventBus) return;
        
        this.eventBus.on('bluetooth:scanned', (data) => {
            this.state.availableDevices = data.devices || [];
            this.state.scanning = false;
            this.render();
        });
        
        this.eventBus.on('bluetooth:paired_list', (data) => {
            this.state.pairedDevices = data.devices || [];
            this.render();
        });
    }
    
    scan() {
        this.state.scanning = true;
        this.render();
        this.eventBus?.emit('bluetooth:scan_requested');
    }
    
    pair(deviceId) {
        this.eventBus?.emit('bluetooth:pair_requested', { device_id: deviceId });
    }
    
    connect(deviceId) {
        this.eventBus?.emit('devices:connect_requested', { device_id: deviceId });
    }
    
    forget(deviceId) {
        this.eventBus?.emit('bluetooth:forget_requested', { device_id: deviceId });
    }
    
    loadPairedDevices() {
        this.eventBus?.emit('bluetooth:paired_requested');
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BluetoothView;
}
if (typeof window !== 'undefined') {
    window.BluetoothView = BluetoothView;
}
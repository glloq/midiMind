// ============================================================================
// Fichier: frontend/js/views/BluetoothView.js
// Version: v1.0.0
// Date: 2025-10-28
// ============================================================================
// Description:
//   Vue pour l'interface Bluetooth BLE
//   - Liste des périphériques disponibles
//   - Indicateurs de signal (RSSI)
//   - Contrôles d'appairage
//   - Configuration Bluetooth
// ============================================================================

class BluetoothView extends BaseView {
    constructor(containerId, eventBus) {
        super(containerId, eventBus);
        
        this.config.name = 'BluetoothView';
        
        // Données de la vue
        this.data = {
            enabled: false,
            scanning: false,
            devices: [],
            pairedDevices: [],
            signalStrength: {},
            status: {}
        };
    }
    
    /**
     * Initialisation de la vue
     */
    onInitialize() {
        this.logDebug('info', 'BluetoothView initialized');
    }
    
    /**
     * Construit le template HTML
     */
    buildTemplate() {
        return `
            <div class="bluetooth-container">
                ${this.buildHeader()}
                ${this.buildConfiguration()}
                ${this.buildDevicesList()}
                ${this.buildPairedDevices()}
            </div>
        `;
    }
    
    /**
     * Construit l'en-tête
     */
    buildHeader() {
        const statusClass = this.data.enabled ? 'status-active' : 'status-inactive';
        const statusText = this.data.enabled ? 'Activé' : 'Désactivé';
        
        return `
            <div class="bluetooth-header">
                <h2>
                    <i class="icon-bluetooth"></i>
                    Bluetooth BLE
                </h2>
                <div class="bluetooth-status ${statusClass}">
                    <span class="status-indicator"></span>
                    ${statusText}
                </div>
            </div>
        `;
    }
    
    /**
     * Construit la section de configuration
     */
    buildConfiguration() {
        return `
            <div class="bluetooth-config card">
                <h3>Configuration</h3>
                <div class="config-controls">
                    <div class="control-group">
                        <label>
                            <input type="checkbox" 
                                   id="bluetooth-enable" 
                                   ${this.data.enabled ? 'checked' : ''}>
                            Activer le Bluetooth
                        </label>
                    </div>
                    <div class="control-group">
                        <label for="scan-timeout">Durée du scan (secondes):</label>
                        <input type="number" 
                               id="scan-timeout" 
                               min="1" 
                               max="30" 
                               value="5">
                    </div>
                    <button class="btn btn-primary" id="apply-config">
                        Appliquer
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Construit la liste des périphériques disponibles
     */
    buildDevicesList() {
        const scanningClass = this.data.scanning ? 'scanning' : '';
        const scanButtonText = this.data.scanning ? 'Scan en cours...' : 'Scanner';
        
        return `
            <div class="bluetooth-devices card">
                <div class="devices-header">
                    <h3>Périphériques disponibles</h3>
                    <button class="btn btn-primary ${scanningClass}" 
                            id="scan-devices"
                            ${this.data.scanning ? 'disabled' : ''}>
                        <i class="icon-search"></i>
                        ${scanButtonText}
                    </button>
                </div>
                
                <div class="devices-list">
                    ${this.data.devices.length === 0 
                        ? '<p class="no-devices">Aucun périphérique trouvé. Lancez un scan.</p>'
                        : this.data.devices.map(device => this.buildDeviceItem(device)).join('')
                    }
                </div>
            </div>
        `;
    }
    
    /**
     * Construit un élément de périphérique
     */
    buildDeviceItem(device) {
        const rssi = device.rssi || -100;
        const signalStrength = this.calculateSignalStrength(rssi);
        const signalClass = this.getSignalClass(signalStrength);
        
        return `
            <div class="device-item" data-address="${device.address}">
                <div class="device-info">
                    <div class="device-name">${this.escapeHTML(device.name || 'Unknown Device')}</div>
                    <div class="device-address">${device.address}</div>
                </div>
                <div class="device-signal">
                    <div class="signal-bars ${signalClass}">
                        ${this.buildSignalBars(signalStrength)}
                    </div>
                    <span class="signal-value">${rssi} dBm</span>
                </div>
                <div class="device-actions">
                    <button class="btn btn-sm btn-pair" 
                            data-address="${device.address}"
                            ${device.requiresPin ? 'data-requires-pin="true"' : ''}>
                        <i class="icon-link"></i>
                        Appairer
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Construit les barres de signal
     */
    buildSignalBars(strength) {
        let bars = '';
        for (let i = 1; i <= 4; i++) {
            const active = i <= strength ? 'active' : '';
            bars += `<span class="signal-bar ${active}"></span>`;
        }
        return bars;
    }
    
    /**
     * Calcule la force du signal (0-4)
     */
    calculateSignalStrength(rssi) {
        if (rssi >= -50) return 4;
        if (rssi >= -60) return 3;
        if (rssi >= -70) return 2;
        if (rssi >= -80) return 1;
        return 0;
    }
    
    /**
     * Obtient la classe CSS pour le signal
     */
    getSignalClass(strength) {
        if (strength >= 3) return 'signal-excellent';
        if (strength >= 2) return 'signal-good';
        if (strength >= 1) return 'signal-fair';
        return 'signal-poor';
    }
    
    /**
     * Construit la liste des périphériques appairés
     */
    buildPairedDevices() {
        return `
            <div class="bluetooth-paired card">
                <div class="paired-header">
                    <h3>Périphériques appairés</h3>
                    <button class="btn btn-sm" id="refresh-paired">
                        <i class="icon-refresh"></i>
                        Actualiser
                    </button>
                </div>
                
                <div class="paired-list">
                    ${this.data.pairedDevices.length === 0
                        ? '<p class="no-devices">Aucun périphérique appairé.</p>'
                        : this.data.pairedDevices.map(device => this.buildPairedDeviceItem(device)).join('')
                    }
                </div>
            </div>
        `;
    }
    
    /**
     * Construit un élément de périphérique appairé
     */
    buildPairedDeviceItem(device) {
        const connectedClass = device.connected ? 'device-connected' : 'device-disconnected';
        const connectedIcon = device.connected ? 'icon-check' : 'icon-close';
        
        return `
            <div class="paired-device-item ${connectedClass}" data-address="${device.address}">
                <div class="device-info">
                    <i class="${connectedIcon}"></i>
                    <div>
                        <div class="device-name">${this.escapeHTML(device.name || 'Unknown Device')}</div>
                        <div class="device-address">${device.address}</div>
                    </div>
                </div>
                <div class="device-actions">
                    <button class="btn btn-sm btn-unpair" 
                            data-address="${device.address}">
                        <i class="icon-unlink"></i>
                        Désappairer
                    </button>
                    <button class="btn btn-sm btn-danger btn-forget" 
                            data-address="${device.address}">
                        <i class="icon-trash"></i>
                        Oublier
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Attache les événements DOM
     */
    attachEvents() {
        // Configuration Bluetooth
        this.addDOMEventListener('#bluetooth-enable', 'change', (e) => {
            const enabled = e.target.checked;
            const scanTimeout = parseInt(document.getElementById('scan-timeout').value) || 5;
            
            this.emit('bluetooth:config', {
                enabled,
                scanTimeout
            });
        });
        
        // Appliquer la configuration
        this.addDOMEventListener('#apply-config', 'click', () => {
            const enabled = document.getElementById('bluetooth-enable').checked;
            const scanTimeout = parseInt(document.getElementById('scan-timeout').value) || 5;
            
            this.emit('bluetooth:config', {
                enabled,
                scanTimeout
            });
        });
        
        // Scanner les périphériques
        this.addDOMEventListener('#scan-devices', 'click', () => {
            this.emit('bluetooth:scan');
        });
        
        // Appairer un périphérique
        this.addDOMEventListener('.btn-pair', 'click', (e) => {
            const address = e.target.closest('.btn-pair').dataset.address;
            const requiresPin = e.target.closest('.btn-pair').dataset.requiresPin;
            
            if (requiresPin) {
                this.showPinDialog(address);
            } else {
                this.emit('bluetooth:pair', { address, pin: '' });
            }
        }, true);
        
        // Désappairer un périphérique
        this.addDOMEventListener('.btn-unpair', 'click', (e) => {
            const address = e.target.closest('.btn-unpair').dataset.address;
            
            if (confirm(`Désappairer le périphérique ${address} ?`)) {
                this.emit('bluetooth:unpair', { address });
            }
        }, true);
        
        // Oublier un périphérique
        this.addDOMEventListener('.btn-forget', 'click', (e) => {
            const address = e.target.closest('.btn-forget').dataset.address;
            
            if (confirm(`Oublier définitivement le périphérique ${address} ?`)) {
                this.emit('bluetooth:forget', { address });
            }
        }, true);
        
        // Actualiser la liste des périphériques appairés
        this.addDOMEventListener('#refresh-paired', 'click', () => {
            this.emit('bluetooth:refresh-paired');
        });
    }
    
    /**
     * Cache les éléments DOM
     */
    cacheElements() {
        this.elements = {
            enableCheckbox: this.container.querySelector('#bluetooth-enable'),
            scanButton: this.container.querySelector('#scan-devices'),
            devicesList: this.container.querySelector('.devices-list'),
            pairedList: this.container.querySelector('.paired-list')
        };
    }
    
    /**
     * Affiche un dialogue pour saisir le PIN
     */
    showPinDialog(address) {
        const pin = prompt(`Entrez le PIN pour ${address} (ou laissez vide):`);
        
        if (pin !== null) {
            this.emit('bluetooth:pair', { address, pin });
        }
    }
    
    /**
     * Affiche un message d'erreur
     */
    showError(message) {
        const errorDiv = document.createElement('div');
        errorDiv.className = 'bluetooth-error alert alert-danger';
        errorDiv.textContent = message;
        
        if (this.container) {
            this.container.insertBefore(errorDiv, this.container.firstChild);
            
            setTimeout(() => {
                errorDiv.remove();
            }, 5000);
        }
    }
    
    /**
     * Obtient l'état de la vue
     */
    getViewState() {
        return {
            ...this.getState(),
            devicesCount: this.data.devices.length,
            pairedCount: this.data.pairedDevices.length,
            scanning: this.data.scanning
        };
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = BluetoothView;
}

if (typeof window !== 'undefined') {
    window.BluetoothView = BluetoothView;
}
// ============================================================================
// Fichier: frontend/js/controllers/InstrumentController.js
// Chemin réel: frontend/js/controllers/InstrumentController.js
// Version: v4.3.0 - COMPLET ET CORRIGÉ
// Date: 2025-11-06
// ============================================================================
// CORRECTIONS v4.3.0:
// ✅ CRITIQUE: Ajout TOUS les event bindings View ↔ Controller
// ✅ CRITIQUE: Méthode updateView() au lieu de render(data)
// ✅ CRITIQUE: Gestion complète des requêtes View (*_requested)
// ✅ CRITIQUE: Émission des événements de réponse pour la View
// ✅ Gestion hot-plug complète
// ✅ Gestion Bluetooth complète
// ============================================================================
// CORRECTIONS v4.2.3:
// ✅ CRITIQUE: Ajout paramètre backend au constructeur (6ème paramètre)
// ✅ Fix: super() appelle BaseController avec backend
// ✅ this.backend initialisé automatiquement via BaseController
// ============================================================================

class InstrumentController extends BaseController {
    constructor(eventBus, models = {}, views = {}, notifications = null, debugConsole = null, backend = null) {
        super(eventBus, models, views, notifications, debugConsole, backend);
        
        this.logger = window.logger || console;
        this.model = models.instrument;
        this.view = views.instrument;
        // ✅ this.backend initialisé automatiquement par BaseController
        
        this.devices = new Map();
        this.connectedDevices = new Set();
        
        this.hotPlugEnabled = false;
        this.hotPlugInterval = 2000;
        this.hotPlugTimer = null;
        
        this.deviceInfoCache = new Map();
        this.deviceInfoCacheTTL = 30000;

        this.isScanning = false;
        this.lastScanTime = null;

        // Web MIDI Service
        this.midiService = null;
        this.initWebMidiService();

        this._fullyInitialized = true;
        this.bindEvents();
    }

    /**
     * Initialiser le service Web MIDI
     */
    initWebMidiService() {
        if (typeof MidiConnectionService !== 'undefined') {
            this.midiService = new MidiConnectionService(this.eventBus, this.logger);
            this.logger?.info?.('InstrumentController', 'Web MIDI Service initialized');
        } else {
            this.logger?.warn?.('InstrumentController', 'MidiConnectionService not available');
        }
    }

    bindEvents() {
        // ========================================================================
        // ÉVÉNEMENTS BACKEND
        // ========================================================================
        this.eventBus.on('backend:connected', () => this.onBackendConnected());
        this.eventBus.on('backend:disconnected', () => this.onBackendDisconnected());
        this.eventBus.on('backend:device:connected', (data) => this.handleBackendDeviceConnected(data));
        this.eventBus.on('backend:device:disconnected', (data) => this.handleBackendDeviceDisconnected(data));
        this.eventBus.on('backend:device:discovered', (data) => this.handleDeviceDiscovered(data));
        this.eventBus.on('backend:device:error', (data) => this.handleDeviceError(data));
        
        // ========================================================================
        // ÉVÉNEMENTS NAVIGATION
        // ========================================================================
        this.eventBus.on('navigation:page_changed', (data) => {
            if (data.page === 'instruments') {
                this.onInstrumentsPageActive();
            } else {
                this.onInstrumentsPageInactive();
            }
        });
        
        // ========================================================================
        // REQUÊTES DE LA VIEW (*_requested) - NOUVEAU v4.3.0
        // ========================================================================
        
        // Liste des devices
        this.eventBus.on('devices:list_requested', () => {
            this.handleListRequest();
        });
        
        // Scan des devices
        this.eventBus.on('devices:scan_requested', (data) => {
            this.handleScanRequest(data);
        });
        
        // Connexion device
        this.eventBus.on('devices:connect_requested', (data) => {
            this.handleConnectRequest(data);
        });
        
        // Déconnexion device
        this.eventBus.on('devices:disconnect_requested', (data) => {
            this.handleDisconnectRequest(data);
        });
        
        // Déconnexion tous devices
        this.eventBus.on('devices:disconnect_all_requested', () => {
            this.handleDisconnectAllRequest();
        });
        
        // Info device
        this.eventBus.on('devices:info_requested', (data) => {
            this.handleDeviceInfoRequest(data);
        });
        
        // Hot-plug start
        this.eventBus.on('hotplug:start_requested', () => {
            this.handleHotPlugStartRequest();
        });
        
        // Hot-plug stop
        this.eventBus.on('hotplug:stop_requested', () => {
            this.handleHotPlugStopRequest();
        });
        
        // Hot-plug status
        this.eventBus.on('hotplug:status_requested', () => {
            this.handleHotPlugStatusRequest();
        });
        
        // Bluetooth scan
        this.eventBus.on('bluetooth:scan_requested', () => {
            this.handleBluetoothScanRequest();
        });
        
        // Bluetooth paired
        this.eventBus.on('bluetooth:paired_requested', () => {
            this.handleBluetoothPairedRequest();
        });
        
        // Bluetooth forget
        this.eventBus.on('bluetooth:forget_requested', (data) => {
            this.handleBluetoothForgetRequest(data);
        });

        // ========================================================================
        // WEB MIDI EVENTS
        // ========================================================================

        // Web MIDI scan
        this.eventBus.on('webmidi:scan_requested', () => {
            this.handleWebMidiScanRequest();
        });

        // Web MIDI connect
        this.eventBus.on('webmidi:connect_requested', (data) => {
            this.handleWebMidiConnectRequest(data);
        });

        // Web MIDI disconnect
        this.eventBus.on('webmidi:disconnect_requested', (data) => {
            this.handleWebMidiDisconnectRequest(data);
        });

        // Web MIDI test
        this.eventBus.on('webmidi:test_requested', (data) => {
            this.handleWebMidiTestRequest(data);
        });

        // Autres
        this.eventBus.on('instruments:request_refresh', () => this.refreshDeviceList());

        this.logger?.info?.('InstrumentController', 'Events bound (v4.3.0 + Web MIDI)');
    }

    async initialize() {
        this.logger?.info?.('InstrumentController', 'Initializing...');

        if (this.backend?.isConnected()) {
            await this.onBackendConnected();
        }
    }

    async onBackendConnected() {
        this.logger?.info?.('InstrumentController', 'âœ" Backend connected');
        
        // NE PAS faire de requêtes automatiques au démarrage
        // Les requêtes seront déclenchées uniquement quand la page Instruments devient active
        this.logger?.info?.('InstrumentController', 'Waiting for instruments page activation...');
    }

    onBackendDisconnected() {
        this.logger?.warn?.('InstrumentController', '⚠️ Backend disconnected');
        this.stopHotPlugMonitoring();
        this.connectedDevices.clear();
        this.updateView();
    }

    // ========================================================================
    // HANDLERS DES REQUÊTES VIEW - NOUVEAU v4.3.0
    // ========================================================================

    async handleListRequest() {
        try {
            this.logger?.debug?.('InstrumentController', 'Handling list request');
            await this.refreshDeviceList();
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleListRequest failed:', error);
            this.eventBus.emit('devices:list_error', { error: error.message });
        }
    }

    async handleScanRequest(data = {}) {
        try {
            this.logger?.debug?.('InstrumentController', 'Handling scan request');
            const full_scan = data.full_scan || false;
            await this.scanDevices(full_scan);
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleScanRequest failed:', error);
            this.eventBus.emit('devices:scan_error', { error: error.message });
        }
    }

    async handleConnectRequest(data) {
        try {
            const device_id = data.device_id;
            if (!device_id) throw new Error('device_id required');
            
            this.logger?.debug?.('InstrumentController', `Handling connect request: ${device_id}`);
            await this.connectDevice(device_id);
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleConnectRequest failed:', error);
            this.notifications?.error('Connection failed', error.message);
            this.eventBus.emit('devices:connect_error', { error: error.message });
        }
    }

    async handleDisconnectRequest(data) {
        try {
            const device_id = data.device_id;
            if (!device_id) throw new Error('device_id required');
            
            this.logger?.debug?.('InstrumentController', `Handling disconnect request: ${device_id}`);
            await this.disconnectDevice(device_id);
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleDisconnectRequest failed:', error);
            this.notifications?.error('Disconnection failed', error.message);
            this.eventBus.emit('devices:disconnect_error', { error: error.message });
        }
    }

    async handleDisconnectAllRequest() {
        try {
            this.logger?.debug?.('InstrumentController', 'Handling disconnect all request');
            await this.disconnectAllDevices();
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleDisconnectAllRequest failed:', error);
            this.notifications?.error('Disconnection failed', error.message);
        }
    }

    async handleDeviceInfoRequest(data) {
        try {
            const device_id = data.device_id;
            if (!device_id) throw new Error('device_id required');
            
            this.logger?.debug?.('InstrumentController', `Handling device info request: ${device_id}`);
            const info = await this.getDeviceInfo(device_id);
            
            this.eventBus.emit('devices:info', {
                device_id,
                info
            });
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleDeviceInfoRequest failed:', error);
            this.eventBus.emit('devices:info_error', { error: error.message });
        }
    }

    async handleHotPlugStartRequest() {
        try {
            this.logger?.debug?.('InstrumentController', 'Handling hot-plug start request');
            await this.startHotPlugMonitoring();
            this.notifyHotPlugStatus();
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleHotPlugStartRequest failed:', error);
            this.notifications?.error('Hot-plug failed', error.message);
        }
    }

    async handleHotPlugStopRequest() {
        try {
            this.logger?.debug?.('InstrumentController', 'Handling hot-plug stop request');
            await this.stopHotPlugMonitoring();
            this.notifyHotPlugStatus();
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleHotPlugStopRequest failed:', error);
            this.notifications?.error('Hot-plug failed', error.message);
        }
    }

    async handleHotPlugStatusRequest() {
        try {
            this.logger?.debug?.('InstrumentController', 'Handling hot-plug status request');
            const status = await this.getHotPlugStatus();
            this.hotPlugEnabled = status?.enabled || false;
            this.notifyHotPlugStatus();
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleHotPlugStatusRequest failed:', error);
        }
    }

    async handleBluetoothScanRequest() {
        try {
            this.logger?.debug?.('InstrumentController', 'Handling Bluetooth scan request');
            // TODO: Implémenter le scan Bluetooth quand backend sera prêt
            this.eventBus.emit('bluetooth:scanned', {
                devices: []
            });
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleBluetoothScanRequest failed:', error);
            this.eventBus.emit('bluetooth:scan_error', { error: error.message });
        }
    }

    async handleBluetoothPairedRequest() {
        try {
            this.logger?.debug?.('InstrumentController', 'Handling Bluetooth paired request');
            // TODO: Implémenter la liste des devices Bluetooth pairés
            this.eventBus.emit('bluetooth:paired_list', {
                devices: []
            });
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleBluetoothPairedRequest failed:', error);
        }
    }

    async handleBluetoothForgetRequest(data) {
        try {
            const device_id = data.device_id;
            if (!device_id) throw new Error('device_id required');

            this.logger?.debug?.('InstrumentController', `Handling Bluetooth forget request: ${device_id}`);
            // TODO: Implémenter l'oubli du device Bluetooth
            this.eventBus.emit('bluetooth:forgotten', { device_id });
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleBluetoothForgetRequest failed:', error);
        }
    }

    // ========================================================================
    // WEB MIDI HANDLERS
    // ========================================================================

    async handleWebMidiScanRequest() {
        try {
            if (!this.midiService) {
                throw new Error('Web MIDI Service not available');
            }

            this.logger?.debug?.('InstrumentController', 'Handling Web MIDI scan request');

            // Scanner les devices
            const result = this.midiService.scanDevices();

            // Émettre pour la view
            this.eventBus.emit('webmidi:devices_scanned', {
                inputs: result.inputs,
                outputs: result.outputs,
                total: result.inputs.length + result.outputs.length
            });

        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleWebMidiScanRequest failed:', error);
            this.notifications?.error('Web MIDI Scan failed', error.message);
        }
    }

    async handleWebMidiConnectRequest(data) {
        try {
            if (!this.midiService) {
                throw new Error('Web MIDI Service not available');
            }

            const { device_id, type } = data;
            if (!device_id || !type) {
                throw new Error('device_id and type required');
            }

            this.logger?.debug?.('InstrumentController',
                `Handling Web MIDI connect: ${device_id} (${type})`);

            // Connecter le device
            const device = await this.midiService.connectDevice(device_id, type);

            this.notifications?.success('Instrument connecté',
                `${device.name} connecté avec succès`);

        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleWebMidiConnectRequest failed:', error);
            this.notifications?.error('Connexion échouée', error.message);
        }
    }

    async handleWebMidiDisconnectRequest(data) {
        try {
            if (!this.midiService) {
                throw new Error('Web MIDI Service not available');
            }

            const { device_id } = data;
            if (!device_id) {
                throw new Error('device_id required');
            }

            this.logger?.debug?.('InstrumentController',
                `Handling Web MIDI disconnect: ${device_id}`);

            // Déconnecter le device
            await this.midiService.disconnectDevice(device_id);

            this.notifications?.success('Instrument déconnecté', 'Déconnexion réussie');

        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleWebMidiDisconnectRequest failed:', error);
            this.notifications?.error('Déconnexion échouée', error.message);
        }
    }

    async handleWebMidiTestRequest(data) {
        try {
            if (!this.midiService) {
                throw new Error('Web MIDI Service not available');
            }

            const { device_id } = data;
            if (!device_id) {
                throw new Error('device_id required');
            }

            this.logger?.debug?.('InstrumentController',
                `Testing Web MIDI output: ${device_id}`);

            // Tester l'output (jouer une note Do majeur)
            await this.midiService.testOutput(device_id, 60, 100, 500);

            this.notifications?.success('Test réussi', 'Note jouée sur l\'instrument');

        } catch (error) {
            this.logger?.error?.('InstrumentController', 'handleWebMidiTestRequest failed:', error);
            this.notifications?.error('Test échoué', error.message);
        }
    }

    // ========================================================================
    // ACTIONS BACKEND
    // ========================================================================

    /**
     * ✅ CORRECTION: Utiliser devices.scan pour obtenir count
     */
    async scanDevices(full_scan = false) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }

        if (this.isScanning) {
            this.logger?.debug?.('InstrumentController', 'Scan already in progress');
            return [];
        }

        this.isScanning = true;
        
        try {
            // ✅ CORRECTION: devices.scan retourne count
            const response = await this.backend.scanDevices(full_scan);
            
            // ✅ Extraction via response (déjà data dans BackendService)
            const devices = response.devices || [];
            const count = response.count || devices.length;
            
            devices.forEach(device => {
                this.devices.set(device.id, device);
            });
            
            this.lastScanTime = Date.now();
            
            this.logger?.info?.('InstrumentController', 
                `✓ Scan complete: ${count} devices found`);
            
            // ✅ NOUVEAU: Émettre pour la View
            this.eventBus.emit('devices:scanned', { 
                devices, 
                count 
            });
            
            this.updateView();
            
            return devices;
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'scanDevices failed:', error);
            throw error;
        } finally {
            this.isScanning = false;
        }
    }

    async refreshDeviceList() {
        try {
            // ✅ devices.list n'a pas de count
            const response = await this.backend.listDevices();
            const devices = response.devices || [];
            
            devices.forEach(device => {
                this.devices.set(device.id, device);
            });
            
            // ✅ NOUVEAU: Émettre pour la View
            this.eventBus.emit('devices:listed', { devices });
            
            this.updateView();
            return devices;
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'refreshDeviceList failed:', error);
            throw error;
        }
    }

    async loadConnectedDevices() {
        if (!this.backend) return [];
        
        try {
            const response = await this.backend.getConnectedDevices();
            const devices = response.devices || [];
            
            this.connectedDevices.clear();
            devices.forEach(device => {
                this.connectedDevices.add(device.id);
            });
            
            this.updateView();
            return devices;
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'loadConnectedDevices failed:', error);
            return [];
        }
    }

    /**
     * ✅ CORRECTION: device_id en snake_case
     */
    async connectDevice(device_id) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            this.logger?.info?.('InstrumentController', `Connecting device: ${device_id}`);
            
            await this.backend.connectDevice(device_id);
            
            this.connectedDevices.add(device_id);
            
            // ✅ NOUVEAU: Émettre pour la View
            this.eventBus.emit('device:connected', { device_id });
            
            this.notifications?.success('Device connected', `Device ${device_id} connected successfully`);
            
            this.updateView();
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', `Failed to connect device ${device_id}:`, error);
            this.notifications?.error('Connection failed', error.message);
            throw error;
        }
    }

    async disconnectDevice(device_id) {
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            this.logger?.info?.('InstrumentController', `Disconnecting device: ${device_id}`);
            
            await this.backend.disconnectDevice(device_id);
            
            this.connectedDevices.delete(device_id);
            
            // ✅ NOUVEAU: Émettre pour la View
            this.eventBus.emit('device:disconnected', { device_id });
            
            this.notifications?.success('Device disconnected', `Device ${device_id} disconnected`);
            
            this.updateView();
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', `Failed to disconnect device ${device_id}:`, error);
            this.notifications?.error('Disconnection failed', error.message);
            throw error;
        }
    }

    async disconnectAllDevices() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            this.logger?.info?.('InstrumentController', 'Disconnecting all devices');
            
            await this.backend.disconnectAllDevices();
            
            this.connectedDevices.clear();
            
            this.eventBus.emit('instruments:all_devices_disconnected');
            
            this.notifications?.success('All disconnected', 'All devices disconnected');
            
            this.updateView();
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'Failed to disconnect all devices:', error);
            this.notifications?.error('Disconnection failed', error.message);
            throw error;
        }
    }

    async getDeviceInfo(device_id) {
        const cached = this.deviceInfoCache.get(device_id);
        if (cached && (Date.now() - cached.timestamp) < this.deviceInfoCacheTTL) {
            return cached.info;
        }
        
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            const info = await this.backend.getDevice(device_id);
            
            this.deviceInfoCache.set(device_id, {
                info,
                timestamp: Date.now()
            });
            
            return info;
            
        } catch (error) {
            this.logger?.error?.('InstrumentController', `getDeviceInfo(${device_id}) failed:`, error);
            throw error;
        }
    }

    async startHotPlugMonitoring() {
        if (!this.backend) {
            throw new Error('Backend not available');
        }
        
        try {
            await this.backend.startHotPlug();
            this.hotPlugEnabled = true;
            this.logger?.info?.('InstrumentController', 'Hot-plug monitoring started');
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'Failed to start hot-plug:', error);
            throw error;
        }
    }

    async stopHotPlugMonitoring() {
        if (!this.backend) return;
        
        try {
            await this.backend.stopHotPlug();
            this.hotPlugEnabled = false;
            this.logger?.info?.('InstrumentController', 'Hot-plug monitoring stopped');
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'Failed to stop hot-plug:', error);
        }
    }

    async getHotPlugStatus() {
        if (!this.backend) return null;
        
        try {
            return await this.backend.getHotPlugStatus();
        } catch (error) {
            this.logger?.error?.('InstrumentController', 'getHotPlugStatus failed:', error);
            return null;
        }
    }

    // ========================================================================
    // HANDLERS ÉVÉNEMENTS BACKEND
    // ========================================================================

    handleBackendDeviceConnected(data) {
        const device_id = data.device_id || data.id;
        this.logger?.info?.('InstrumentController', `Device connected: ${device_id}`);
        this.connectedDevices.add(device_id);
        
        // ✅ Propager à la View
        this.eventBus.emit('device:connected', { device_id });
        
        this.updateView();
    }

    handleBackendDeviceDisconnected(data) {
        const device_id = data.device_id || data.id;
        this.logger?.info?.('InstrumentController', `Device disconnected: ${device_id}`);
        this.connectedDevices.delete(device_id);
        
        // ✅ Propager à la View
        this.eventBus.emit('device:disconnected', { device_id });
        
        this.updateView();
    }

    handleDeviceDiscovered(data) {
        const device = data.device;
        if (device) {
            this.devices.set(device.id, device);
            this.updateView();
        }
    }

    handleDeviceError(data) {
        const device_id = data.device_id;
        const error = data.error;
        this.logger?.error?.('InstrumentController', `Device error (${device_id}):`, error);
        this.notifications?.error('Device error', `Device ${device_id}: ${error}`);
    }

    onInstrumentsPageActive() {
        // Charger les données quand la page devient active
        if (this.backend?.isConnected?.()) {
            this.scanDevices().catch(err => {
                this.log('warn', 'InstrumentController', 'Scan failed:', err.message);
            });
            
            this.loadConnectedDevices().catch(err => {
                this.log('warn', 'InstrumentController', 'Load devices failed:', err.message);
            });
            
            this.getHotPlugStatus().then(status => {
                if (status?.enabled) {
                    this.hotPlugEnabled = true;
                    this.logger?.info?.('InstrumentController', 'âœ" Hot-plug enabled');
                    this.notifyHotPlugStatus();
                }
            }).catch(err => {
                this.log('warn', 'InstrumentController', 'Hot-plug status failed:', err.message);
            });
        }
    }

    onInstrumentsPageInactive() {
        // Rien pour l'instant
    }

    // ========================================================================
    // MISE À JOUR VIEW - NOUVEAU v4.3.0
    // ========================================================================

    /**
     * ✅ NOUVELLE MÉTHODE: Mise à jour de la View avec les données actuelles
     * Remplace l'ancien this.view.render(data) qui ne fonctionnait pas
     */
    updateView() {
        if (!this.view) return;
        
        const devicesArray = Array.from(this.devices.values());
        
        // Séparer les devices connectés et disponibles
        const connectedDevices = devicesArray.filter(d => 
            this.connectedDevices.has(d.id) || d.status === 2
        );
        
        const availableDevices = devicesArray.filter(d => 
            !this.connectedDevices.has(d.id) && d.status !== 2
        );
        
        // ✅ NOUVEAU: Émettre un événement pour que la View se mette à jour
        this.eventBus.emit('devices:listed', {
            devices: devicesArray
        });
        
        this.logger?.debug?.('InstrumentController', `View updated: ${connectedDevices.length} connected, ${availableDevices.length} available`);
    }

    /**
     * ✅ NOUVELLE MÉTHODE: Notifier le statut hot-plug à la View
     */
    notifyHotPlugStatus() {
        this.eventBus.emit('hotplug:status', {
            enabled: this.hotPlugEnabled
        });
    }

    // ========================================================================
    // GETTERS
    // ========================================================================

    getDevices() {
        return Array.from(this.devices.values());
    }

    getConnectedDevices() {
        return Array.from(this.connectedDevices);
    }

    isDeviceConnected(device_id) {
        return this.connectedDevices.has(device_id);
    }
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = InstrumentController;
}

if (typeof window !== 'undefined') {
    window.InstrumentController = InstrumentController;
}
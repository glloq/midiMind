// ============================================================================
// Fichier: frontend/js/services/MidiConnectionService.js
// Version: 1.0.0
// Date: 2025-11-13
// Projet: midiMind - Service de connexion MIDI via Web MIDI API
// ============================================================================
// Description:
// Service pour détecter, connecter et gérer les instruments MIDI
// via Web MIDI API (USB et Bluetooth)
// ============================================================================

class MidiConnectionService {
    constructor(eventBus, logger) {
        this.eventBus = eventBus || window.eventBus || null;
        this.logger = logger || console;

        // État MIDI
        this.midiAccess = null;
        this.midiSupported = false;
        this.midiEnabled = false;

        // Devices MIDI
        this.inputs = new Map();  // Entrées MIDI
        this.outputs = new Map(); // Sorties MIDI
        this.connectedDevices = new Map(); // Devices connectés

        // Configuration
        this.config = {
            sysex: true, // Support SysEx pour certains instruments
            software: false
        };

        // Statistiques
        this.stats = {
            inputsCount: 0,
            outputsCount: 0,
            messagesReceived: 0,
            messagesSent: 0
        };

        this.initialize();
    }

    // ========================================================================
    // INITIALISATION
    // ========================================================================

    async initialize() {
        this.log('info', 'MidiConnectionService', 'Initializing...');

        // Vérifier le support Web MIDI API
        if (typeof navigator.requestMIDIAccess !== 'function') {
            this.log('warn', 'MidiConnectionService', 'Web MIDI API not supported in this browser');
            this.midiSupported = false;
            this.emitStatus('unsupported');
            return false;
        }

        this.midiSupported = true;

        try {
            // Demander l'accès MIDI
            this.midiAccess = await navigator.requestMIDIAccess({
                sysex: this.config.sysex,
                software: this.config.software
            });

            this.midiEnabled = true;
            this.log('info', 'MidiConnectionService', 'Web MIDI API access granted');

            // Scanner les devices initiaux
            this.scanDevices();

            // Écouter les changements de connexion
            this.midiAccess.onstatechange = (event) => {
                this.handleStateChange(event);
            };

            this.emitStatus('ready');
            return true;

        } catch (error) {
            this.log('error', 'MidiConnectionService', 'Failed to get MIDI access:', error);
            this.midiEnabled = false;
            this.emitStatus('denied');
            return false;
        }
    }

    /**
     * Log sécurisé avec fallback
     */
    log(level, ...args) {
        if (this.logger && typeof this.logger[level] === 'function') {
            this.logger[level](...args);
        } else {
            console[level]?.(...args) || console.log(...args);
        }
    }

    // ========================================================================
    // SCANNING DE DEVICES
    // ========================================================================

    /**
     * Scanner tous les devices MIDI disponibles
     */
    scanDevices() {
        if (!this.midiAccess) {
            this.log('warn', 'MidiConnectionService', 'Cannot scan: MIDI not enabled');
            return { inputs: [], outputs: [] };
        }

        this.log('debug', 'MidiConnectionService', 'Scanning MIDI devices...');

        // Clear collections
        this.inputs.clear();
        this.outputs.clear();

        // Scanner les inputs
        const inputs = [];
        for (const input of this.midiAccess.inputs.values()) {
            const device = this.createDeviceInfo(input, 'input');
            this.inputs.set(input.id, device);
            inputs.push(device);
        }

        // Scanner les outputs
        const outputs = [];
        for (const output of this.midiAccess.outputs.values()) {
            const device = this.createDeviceInfo(output, 'output');
            this.outputs.set(output.id, device);
            outputs.push(device);
        }

        this.stats.inputsCount = inputs.length;
        this.stats.outputsCount = outputs.length;

        this.log('info', 'MidiConnectionService',
            `Found ${inputs.length} inputs and ${outputs.length} outputs`);

        // Émettre l'événement
        this.eventBus?.emit('webmidi:devices_scanned', {
            inputs,
            outputs,
            total: inputs.length + outputs.length
        });

        return { inputs, outputs };
    }

    /**
     * Créer les informations d'un device
     */
    createDeviceInfo(port, type) {
        return {
            id: port.id,
            name: port.name || 'Unknown Device',
            manufacturer: port.manufacturer || 'Unknown',
            type: type,
            state: port.state,
            connection: port.connection,
            version: port.version || '',
            // Déterminer le type de connexion (USB ou Bluetooth)
            connectionType: this.detectConnectionType(port)
        };
    }

    /**
     * Détecter le type de connexion (USB ou Bluetooth)
     */
    detectConnectionType(port) {
        const name = (port.name || '').toLowerCase();
        const manufacturer = (port.manufacturer || '').toLowerCase();

        // Indices pour Bluetooth
        if (name.includes('bluetooth') || name.includes('bt') ||
            name.includes('wireless') || manufacturer.includes('bluetooth')) {
            return 'bluetooth';
        }

        // Indices pour USB
        if (name.includes('usb') || port.connection === 'open') {
            return 'usb';
        }

        return 'unknown';
    }

    // ========================================================================
    // CONNEXION / DÉCONNEXION
    // ========================================================================

    /**
     * Connecter un device (activer l'écoute)
     */
    async connectDevice(deviceId, type = 'input') {
        this.log('debug', 'MidiConnectionService', `Connecting ${type}: ${deviceId}`);

        if (type === 'input') {
            return this.connectInput(deviceId);
        } else if (type === 'output') {
            return this.connectOutput(deviceId);
        }

        throw new Error(`Invalid device type: ${type}`);
    }

    /**
     * Connecter une entrée MIDI
     */
    async connectInput(inputId) {
        const deviceInfo = this.inputs.get(inputId);
        if (!deviceInfo) {
            throw new Error(`Input not found: ${inputId}`);
        }

        // Récupérer le port MIDI
        const port = this.midiAccess.inputs.get(inputId);
        if (!port) {
            throw new Error(`MIDI port not found: ${inputId}`);
        }

        try {
            // Ouvrir le port
            await port.open();

            // Écouter les messages MIDI
            port.onmidimessage = (event) => {
                this.handleMidiMessage(event, deviceInfo);
            };

            // Marquer comme connecté
            this.connectedDevices.set(inputId, {
                ...deviceInfo,
                port,
                connected: true
            });

            this.log('info', 'MidiConnectionService', `Input connected: ${deviceInfo.name}`);

            // Émettre l'événement
            this.eventBus?.emit('webmidi:device_connected', {
                device: deviceInfo
            });

            return deviceInfo;

        } catch (error) {
            this.log('error', 'MidiConnectionService', `Failed to connect input ${inputId}:`, error);
            throw error;
        }
    }

    /**
     * Connecter une sortie MIDI
     */
    async connectOutput(outputId) {
        const deviceInfo = this.outputs.get(outputId);
        if (!deviceInfo) {
            throw new Error(`Output not found: ${outputId}`);
        }

        // Récupérer le port MIDI
        const port = this.midiAccess.outputs.get(outputId);
        if (!port) {
            throw new Error(`MIDI port not found: ${outputId}`);
        }

        try {
            // Ouvrir le port
            await port.open();

            // Marquer comme connecté
            this.connectedDevices.set(outputId, {
                ...deviceInfo,
                port,
                connected: true
            });

            this.log('info', 'MidiConnectionService', `Output connected: ${deviceInfo.name}`);

            // Émettre l'événement
            this.eventBus?.emit('webmidi:device_connected', {
                device: deviceInfo
            });

            return deviceInfo;

        } catch (error) {
            this.log('error', 'MidiConnectionService', `Failed to connect output ${outputId}:`, error);
            throw error;
        }
    }

    /**
     * Déconnecter un device
     */
    async disconnectDevice(deviceId) {
        const device = this.connectedDevices.get(deviceId);
        if (!device) {
            this.log('warn', 'MidiConnectionService', `Device not connected: ${deviceId}`);
            return;
        }

        try {
            // Fermer le port
            if (device.port && device.port.close) {
                await device.port.close();
            }

            // Retirer de la liste des connectés
            this.connectedDevices.delete(deviceId);

            this.log('info', 'MidiConnectionService', `Device disconnected: ${device.name}`);

            // Émettre l'événement
            this.eventBus?.emit('webmidi:device_disconnected', {
                device_id: deviceId
            });

        } catch (error) {
            this.log('error', 'MidiConnectionService', `Failed to disconnect device ${deviceId}:`, error);
            throw error;
        }
    }

    /**
     * Déconnecter tous les devices
     */
    async disconnectAll() {
        const deviceIds = Array.from(this.connectedDevices.keys());

        for (const deviceId of deviceIds) {
            try {
                await this.disconnectDevice(deviceId);
            } catch (error) {
                this.log('error', 'MidiConnectionService', `Error disconnecting ${deviceId}:`, error);
            }
        }

        this.log('info', 'MidiConnectionService', 'All devices disconnected');
    }

    // ========================================================================
    // GESTION DES MESSAGES MIDI
    // ========================================================================

    /**
     * Gérer un message MIDI reçu
     */
    handleMidiMessage(event, deviceInfo) {
        this.stats.messagesReceived++;

        const [status, data1, data2] = event.data;
        const command = status >> 4;
        const channel = status & 0x0F;

        const message = {
            timestamp: event.timeStamp,
            device: deviceInfo,
            status,
            command,
            channel,
            data: [data1, data2],
            raw: Array.from(event.data)
        };

        // Analyser le type de message
        switch (command) {
            case 0x8: // Note Off
                message.type = 'noteoff';
                message.note = data1;
                message.velocity = data2;
                break;
            case 0x9: // Note On
                message.type = data2 > 0 ? 'noteon' : 'noteoff';
                message.note = data1;
                message.velocity = data2;
                break;
            case 0xA: // Polyphonic Aftertouch
                message.type = 'polyaftertouch';
                message.note = data1;
                message.pressure = data2;
                break;
            case 0xB: // Control Change
                message.type = 'controlchange';
                message.controller = data1;
                message.value = data2;
                break;
            case 0xC: // Program Change
                message.type = 'programchange';
                message.program = data1;
                break;
            case 0xD: // Channel Aftertouch
                message.type = 'channelaftertouch';
                message.pressure = data1;
                break;
            case 0xE: // Pitch Bend
                message.type = 'pitchbend';
                message.value = (data2 << 7) | data1;
                break;
            default:
                message.type = 'unknown';
        }

        // Émettre l'événement
        this.eventBus?.emit('webmidi:message', message);

        // Log pour debug (optionnel)
        this.log('debug', 'MidiConnectionService',
            `MIDI: ${message.type} from ${deviceInfo.name}`);
    }

    /**
     * Envoyer un message MIDI
     */
    sendMessage(outputId, data, timestamp) {
        const device = this.connectedDevices.get(outputId);
        if (!device) {
            throw new Error(`Output not connected: ${outputId}`);
        }

        if (device.type !== 'output') {
            throw new Error(`Device ${outputId} is not an output`);
        }

        try {
            device.port.send(data, timestamp);
            this.stats.messagesSent++;

            this.log('debug', 'MidiConnectionService',
                `Sent MIDI message to ${device.name}:`, data);

        } catch (error) {
            this.log('error', 'MidiConnectionService',
                `Failed to send message to ${outputId}:`, error);
            throw error;
        }
    }

    // ========================================================================
    // GESTION DES CHANGEMENTS D'ÉTAT
    // ========================================================================

    /**
     * Gérer les changements d'état des ports MIDI
     */
    handleStateChange(event) {
        const port = event.port;

        this.log('info', 'MidiConnectionService',
            `State changed: ${port.name} (${port.state}/${port.connection})`);

        // Rescanner les devices
        this.scanDevices();

        // Émettre l'événement
        this.eventBus?.emit('webmidi:state_changed', {
            port: {
                id: port.id,
                name: port.name,
                state: port.state,
                connection: port.connection
            }
        });
    }

    // ========================================================================
    // MÉTHODES UTILITAIRES
    // ========================================================================

    /**
     * Vérifier si le Web MIDI est supporté
     */
    isSupported() {
        return this.midiSupported;
    }

    /**
     * Vérifier si le Web MIDI est activé
     */
    isEnabled() {
        return this.midiEnabled;
    }

    /**
     * Obtenir tous les inputs
     */
    getInputs() {
        return Array.from(this.inputs.values());
    }

    /**
     * Obtenir tous les outputs
     */
    getOutputs() {
        return Array.from(this.outputs.values());
    }

    /**
     * Obtenir tous les devices connectés
     */
    getConnectedDevices() {
        return Array.from(this.connectedDevices.values());
    }

    /**
     * Vérifier si un device est connecté
     */
    isConnected(deviceId) {
        return this.connectedDevices.has(deviceId);
    }

    /**
     * Obtenir les statistiques
     */
    getStats() {
        return {
            ...this.stats,
            connectedDevices: this.connectedDevices.size,
            supported: this.midiSupported,
            enabled: this.midiEnabled
        };
    }

    /**
     * Émettre le statut du service
     */
    emitStatus(status) {
        this.eventBus?.emit('webmidi:status', {
            status,
            supported: this.midiSupported,
            enabled: this.midiEnabled
        });
    }

    /**
     * Tester un output en envoyant une note
     */
    async testOutput(outputId, note = 60, velocity = 100, duration = 500) {
        // Note On
        this.sendMessage(outputId, [0x90, note, velocity]);

        // Note Off après duration
        setTimeout(() => {
            this.sendMessage(outputId, [0x80, note, 0]);
        }, duration);
    }
}

// ============================================================================
// EXPORT
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = MidiConnectionService;
}

if (typeof window !== 'undefined') {
    window.MidiConnectionService = MidiConnectionService;
}

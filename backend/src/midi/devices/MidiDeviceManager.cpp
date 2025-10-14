// ============================================================================
// Fichier: backend/src/midi/devices/MidiDeviceManager.cpp
// Version: 3.2.0 - CORRECTIONS CRITIQUES APPLIQUÉES
// Date: 2025-10-13
// ============================================================================
// CORRECTIONS v3.2.0:
//   ✅ FIX #1: Réception MIDI implémentée (callback device dans connect())
//   ✅ FIX #2: Méthode reconnectDevice() ajoutée
//   ✅ FIX #3: Scan Bluetooth avec BleMidiPlugin (scan BLE réel)
//
// Description:
//   Gestionnaire centralisé de tous les périphériques MIDI.
//   Thread-safe avec Observer Pattern et Factory Pattern.
//
// Auteur: MidiMind Team (Corrections par Claude)
// ============================================================================

#include "MidiDeviceManager.h"
#include "../core/Logger.h"
#include "../core/Config.h"
#include "devices/UsbMidiDevice.h"
#include "devices/VirtualMidiDevice.h"
#include "devices/NetworkMidiDevice.h"
#include "devices/BleMidiDevice.h"
#include "plugins/BleMidiPlugin.h"  // ✅ NOUVEAU: Pour scan BLE réel
#include <algorithm>
#include <thread>
#include <chrono>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MidiDeviceManager::MidiDeviceManager() {
    Logger::info("MidiDeviceManager", "╔═══════════════════════════════════════╗");
    Logger::info("MidiDeviceManager", "  Initializing MidiDeviceManager v3.2");
    Logger::info("MidiDeviceManager", "╚═══════════════════════════════════════╝");
    
    availableDevices_.clear();
    connectedDevices_.clear();
    
    Logger::info("MidiDeviceManager", "✓ MidiDeviceManager initialized");
}

MidiDeviceManager::~MidiDeviceManager() {
    Logger::info("MidiDeviceManager", "Shutting down MidiDeviceManager...");
    disconnectAll();
    Logger::info("MidiDeviceManager", "✓ MidiDeviceManager destroyed");
}

// ============================================================================
// DÉCOUVERTE DE DEVICES
// ============================================================================

std::vector<DeviceInfo> MidiDeviceManager::discoverDevices(bool fullScan) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    Logger::info("MidiDeviceManager", "╔═══════════════════════════════════════╗");
    Logger::info("MidiDeviceManager", "  Starting device discovery");
    Logger::info("MidiDeviceManager", "╚═══════════════════════════════════════╝");
    
    if (fullScan) {
        availableDevices_.clear();
        Logger::info("MidiDeviceManager", "Full scan mode: clearing device cache");
    }
    
    // Scanner chaque type
    scanUSBDevices();
    scanVirtualDevices();
    scanNetworkDevices();
    scanBluetoothDevices();  // ✅ AMÉLIORÉ: Maintenant avec scan BLE réel
    
    Logger::info("MidiDeviceManager", "╔═══════════════════════════════════════╗");
    Logger::info("MidiDeviceManager", "  Discovery complete");
    Logger::info("MidiDeviceManager", "  Total devices: " + 
                std::to_string(availableDevices_.size()));
    Logger::info("MidiDeviceManager", "╚═══════════════════════════════════════╝");
    
    return availableDevices_;
}

std::vector<DeviceInfo> MidiDeviceManager::getAvailableDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    return availableDevices_;
}

// ============================================================================
// SCANNERS PRIVÉS - USB
// ============================================================================

void MidiDeviceManager::scanUSBDevices() {
    Logger::info("MidiDeviceManager", "Scanning USB MIDI devices (ALSA)...");
    
    int usbCount = 0;
    
    try {
#ifdef __linux__
        // Scanner via ALSA snd_seq
        snd_seq_t* seq = nullptr;
        
        if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
            Logger::warn("MidiDeviceManager", "Failed to open ALSA sequencer");
            return;
        }
        
        snd_seq_client_info_t* cinfo;
        snd_seq_port_info_t* pinfo;
        
        snd_seq_client_info_alloca(&cinfo);
        snd_seq_port_info_alloca(&pinfo);
        
        snd_seq_client_info_set_client(cinfo, -1);
        
        while (snd_seq_query_next_client(seq, cinfo) >= 0) {
            int client = snd_seq_client_info_get_client(cinfo);
            
            // Ignorer le client système
            if (client == 0) continue;
            
            snd_seq_port_info_set_client(pinfo, client);
            snd_seq_port_info_set_port(pinfo, -1);
            
            while (snd_seq_query_next_port(seq, pinfo) >= 0) {
                // Vérifier capabilities
                unsigned int caps = snd_seq_port_info_get_capability(pinfo);
                
                if ((caps & SND_SEQ_PORT_CAP_WRITE) || 
                    (caps & SND_SEQ_PORT_CAP_READ)) {
                    
                    std::string clientName = snd_seq_client_info_get_name(cinfo);
                    std::string portName = snd_seq_port_info_get_name(pinfo);
                    int port = snd_seq_port_info_get_port(pinfo);
                    
                    std::string deviceId = "usb_" + std::to_string(client) + 
                                          "_" + std::to_string(port);
                    std::string deviceName = clientName + " - " + portName;
                    
                    DeviceInfo dev(
                        deviceId,
                        deviceName,
                        DeviceType::USB,
                        DeviceDirection::BIDIRECTIONAL
                    );
                    
                    dev.metadata["alsa_client"] = client;
                    dev.metadata["alsa_port"] = port;
                    dev.metadata["discovery"] = "alsa";
                    
                    // Vérifier si déjà connecté
                    if (connectedDevices_.find(deviceId) != connectedDevices_.end()) {
                        dev.connected = true;
                    }
                    
                    availableDevices_.push_back(dev);
                    usbCount++;
                    
                    Logger::info("MidiDeviceManager", 
                        "USB device found: " + deviceName);
                }
            }
        }
        
        snd_seq_close(seq);
#else
        Logger::warn("MidiDeviceManager", "USB scanning not supported on this platform");
#endif
        
        // Fallback: charger depuis config
        if (Config::instance().contains("usb_devices")) {
            auto usbDevs = Config::instance().getValue("usb_devices");
            
            for (const auto& devConfig : usbDevs) {
                DeviceInfo dev(
                    devConfig["id"],
                    devConfig["name"],
                    DeviceType::USB,
                    DeviceDirection::BIDIRECTIONAL
                );
                
                dev.metadata["alsa_client"] = devConfig.value("alsa_client", 0);
                dev.metadata["alsa_port"] = devConfig.value("alsa_port", 0);
                dev.metadata["discovery"] = "config";
                
                availableDevices_.push_back(dev);
                usbCount++;
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiDeviceManager", 
            "USB scan failed: " + std::string(e.what()));
    }
    
    Logger::info("MidiDeviceManager", 
        "✓ USB scan complete (" + std::to_string(usbCount) + " devices)");
}

// ============================================================================
// SCANNERS PRIVÉS - VIRTUAL
// ============================================================================

void MidiDeviceManager::scanVirtualDevices() {
    Logger::info("MidiDeviceManager", "Scanning virtual MIDI devices...");
    
    int virtualCount = 0;
    
    // Charger depuis config
    if (Config::instance().contains("virtual_devices")) {
        auto virtualDevs = Config::instance().getValue("virtual_devices");
        
        for (const auto& devConfig : virtualDevs) {
            DeviceInfo dev(
                devConfig["id"],
                devConfig["name"],
                DeviceType::VIRTUAL,
                DeviceDirection::BIDIRECTIONAL
            );
            
            dev.metadata["discovery"] = "config";
            
            availableDevices_.push_back(dev);
            virtualCount++;
        }
    }
    
    // Toujours créer au moins un virtual device par défaut
    if (virtualCount == 0) {
        DeviceInfo dev(
            "virtual_default",
            "MidiMind Virtual Port",
            DeviceType::VIRTUAL,
            DeviceDirection::BIDIRECTIONAL
        );
        
        dev.metadata["discovery"] = "default";
        
        availableDevices_.push_back(dev);
        virtualCount++;
    }
    
    Logger::info("MidiDeviceManager", 
        "✓ Virtual scan complete (" + std::to_string(virtualCount) + " devices)");
}

// ============================================================================
// SCANNERS PRIVÉS - NETWORK
// ============================================================================

void MidiDeviceManager::scanNetworkDevices() {
    Logger::info("MidiDeviceManager", "Scanning network MIDI devices...");
    
    int networkCount = 0;
    
    try {
        // TODO: Scanner via mDNS pour RTP-MIDI
        // MdnsDiscovery mdns;
        // auto services = mdns.scan("_apple-midi._udp");
        
        // Fallback: charger depuis config
        if (Config::instance().contains("network_devices")) {
            auto netDevs = Config::instance().getValue("network_devices");
            
            for (const auto& devConfig : netDevs) {
                DeviceInfo dev(
                    devConfig["id"],
                    devConfig["name"],
                    DeviceType::NETWORK,
                    DeviceDirection::BIDIRECTIONAL
                );
                
                dev.address = devConfig.value("address", "");
                dev.metadata["port"] = devConfig.value("port", 5004);
                dev.metadata["protocol"] = devConfig.value("protocol", "rtp-midi");
                dev.metadata["discovery"] = "config";
                
                // Vérifier si déjà connecté
                if (connectedDevices_.find(dev.id) != connectedDevices_.end()) {
                    dev.connected = true;
                }
                
                availableDevices_.push_back(dev);
                networkCount++;
                
                Logger::info("MidiDeviceManager", 
                    "Network device found: " + dev.name + " (" + dev.address + ")");
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiDeviceManager", 
            "Network scan failed: " + std::string(e.what()));
    }
    
    Logger::info("MidiDeviceManager", 
        "✓ Network scan complete (" + std::to_string(networkCount) + " devices)");
}

// ============================================================================
// SCANNERS PRIVÉS - BLUETOOTH (✅ FIX #3: SCAN BLE RÉEL)
// ============================================================================

void MidiDeviceManager::scanBluetoothDevices() {
    Logger::info("MidiDeviceManager", "Scanning Bluetooth LE MIDI devices...");
    
    int bleCount = 0;
    
    try {
        // ✅ FIX #3: SCAN BLE RÉEL avec BleMidiPlugin
        BleMidiPlugin blePlugin;
        
        if (blePlugin.initialize()) {
            Logger::info("MidiDeviceManager", "BleMidiPlugin initialized, scanning...");
            
            auto bleDevices = blePlugin.discover();
            
            for (const auto& bleInfo : bleDevices) {
                // Vérifier si déjà connecté
                bool isConnected = connectedDevices_.find(bleInfo.id) != 
                                  connectedDevices_.end();
                
                DeviceInfo dev = bleInfo;
                dev.connected = isConnected;
                dev.metadata["discovery"] = "ble_scan";
                
                availableDevices_.push_back(dev);
                bleCount++;
                
                Logger::info("MidiDeviceManager", 
                    "Bluetooth device found: " + bleInfo.name + 
                    " (" + bleInfo.metadata.value("address", "unknown") + ")");
            }
            
            blePlugin.shutdown();
        } else {
            Logger::warn("MidiDeviceManager", 
                "BleMidiPlugin initialization failed, falling back to config");
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiDeviceManager", 
            "Bluetooth scan failed: " + std::string(e.what()));
    }
    
    // Fallback: charger depuis config
    if (Config::instance().contains("bluetooth_devices")) {
        auto bleDevs = Config::instance().getValue("bluetooth_devices");
        
        for (const auto& devConfig : bleDevs) {
            DeviceInfo dev(
                devConfig["id"],
                devConfig["name"],
                DeviceType::BLUETOOTH,
                DeviceDirection::BIDIRECTIONAL
            );
            
            dev.address = devConfig.value("address", "");
            dev.metadata["discovery"] = "config";
            
            // Vérifier si déjà connecté
            if (connectedDevices_.find(dev.id) != connectedDevices_.end()) {
                dev.connected = true;
            }
            
            availableDevices_.push_back(dev);
            bleCount++;
            
            Logger::info("MidiDeviceManager", 
                "Bluetooth device found (config): " + dev.name + 
                " (" + dev.address + ")");
        }
    }
    
    Logger::info("MidiDeviceManager", 
        "✓ Bluetooth scan complete (" + std::to_string(bleCount) + " devices)");
}

// ============================================================================
// CONNEXION (✅ FIX #1: RÉCEPTION MIDI)
// ============================================================================

bool MidiDeviceManager::connect(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    Logger::info("MidiDeviceManager", "Connecting to device: " + deviceId);
    
    // Vérifier si déjà connecté
    if (connectedDevices_.find(deviceId) != connectedDevices_.end()) {
        Logger::warn("MidiDeviceManager", "Device already connected: " + deviceId);
        return true;
    }
    
    // Trouver le device dans availableDevices_
    auto it = std::find_if(availableDevices_.begin(), availableDevices_.end(),
        [&deviceId](const DeviceInfo& info) {
            return info.id == deviceId;
        });
    
    if (it == availableDevices_.end()) {
        Logger::error("MidiDeviceManager", "Device not found: " + deviceId);
        return false;
    }
    
    // Créer le device via Factory
    auto device = createDevice(*it);
    if (!device) {
        Logger::error("MidiDeviceManager", "Failed to create device: " + deviceId);
        return false;
    }
    
    // Ouvrir le device
    if (!device->open()) {
        Logger::error("MidiDeviceManager", "Failed to open device: " + deviceId);
        return false;
    }
    
    // ✅ FIX #1: CONFIGURER CALLBACK DE RÉCEPTION MIDI
    device->setMessageCallback([this, deviceId](const MidiMessage& message) {
        // Appeler le callback onMidiReceived_ si configuré
        if (onMidiReceived_) {
            try {
                onMidiReceived_(deviceId, message);
            } catch (const std::exception& e) {
                Logger::error("MidiDeviceManager", 
                    "Exception in MIDI receive callback: " + std::string(e.what()));
            }
        }
    });
    
    // Ajouter à connectedDevices_
    connectedDevices_[deviceId] = device;
    
    // Mettre à jour le flag connected
    it->connected = true;
    
    Logger::info("MidiDeviceManager", "✓ Device connected: " + deviceId);
    
    // Callback de connexion
    if (onDeviceConnected_) {
        onDeviceConnected_(deviceId);
    }
    
    return true;
}

// ============================================================================
// DÉCONNEXION
// ============================================================================

void MidiDeviceManager::disconnect(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    Logger::info("MidiDeviceManager", "Disconnecting device: " + deviceId);
    
    auto it = connectedDevices_.find(deviceId);
    if (it == connectedDevices_.end()) {
        Logger::warn("MidiDeviceManager", "Device not connected: " + deviceId);
        return;
    }
    
    // Fermer le device
    it->second->close();
    
    // Retirer de connectedDevices_
    connectedDevices_.erase(it);
    
    // Mettre à jour availableDevices_
    auto devIt = std::find_if(availableDevices_.begin(), availableDevices_.end(),
        [&deviceId](const DeviceInfo& info) {
            return info.id == deviceId;
        });
    
    if (devIt != availableDevices_.end()) {
        devIt->connected = false;
    }
    
    Logger::info("MidiDeviceManager", "✓ Device disconnected: " + deviceId);
    
    // Callback de déconnexion
    if (onDeviceDisconnected_) {
        onDeviceDisconnected_(deviceId);
    }
}

void MidiDeviceManager::disconnectAll() {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    Logger::info("MidiDeviceManager", 
        "Disconnecting all devices (" + 
        std::to_string(connectedDevices_.size()) + ")...");
    
    for (auto& [id, device] : connectedDevices_) {
        Logger::info("MidiDeviceManager", "Closing device: " + id);
        device->close();
    }
    
    connectedDevices_.clear();
    
    // Mettre à jour tous les flags
    for (auto& dev : availableDevices_) {
        dev.connected = false;
    }
    
    Logger::info("MidiDeviceManager", "✓ All devices disconnected");
}

bool MidiDeviceManager::isConnected(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    return connectedDevices_.find(deviceId) != connectedDevices_.end();
}

// ============================================================================
// RECONNEXION (✅ FIX #2: NOUVELLE MÉTHODE)
// ============================================================================

bool MidiDeviceManager::reconnectDevice(const std::string& deviceId) {
    Logger::info("MidiDeviceManager", 
        "Reconnecting device: " + deviceId + "...");
    
    // Déconnecter
    disconnect(deviceId);
    
    // Attendre un peu pour laisser le temps au device de se fermer
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Reconnecter
    bool success = connect(deviceId);
    
    if (success) {
        Logger::info("MidiDeviceManager", "✓ Device reconnected: " + deviceId);
    } else {
        Logger::error("MidiDeviceManager", "Failed to reconnect device: " + deviceId);
    }
    
    return success;
}

// ============================================================================
// ACCÈS AUX DEVICES
// ============================================================================

std::shared_ptr<MidiDevice> MidiDeviceManager::getDevice(
    const std::string& deviceId
) const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    auto it = connectedDevices_.find(deviceId);
    return (it != connectedDevices_.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<MidiDevice>> MidiDeviceManager::getConnectedDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    std::vector<std::shared_ptr<MidiDevice>> devices;
    for (const auto& [id, device] : connectedDevices_) {
        devices.push_back(device);
    }
    return devices;
}

std::vector<std::shared_ptr<MidiDevice>> MidiDeviceManager::getDevicesByType(
    DeviceType type
) const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    std::vector<std::shared_ptr<MidiDevice>> result;
    
    for (const auto& [id, device] : connectedDevices_) {
        if (device->getType() == type) {
            result.push_back(device);
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<MidiDevice>> MidiDeviceManager::getNetworkDevices() const {
    return getDevicesByType(DeviceType::NETWORK);
}

std::vector<std::shared_ptr<MidiDevice>> MidiDeviceManager::getBluetoothDevices() const {
    return getDevicesByType(DeviceType::BLUETOOTH);
}

// ============================================================================
// ENVOI DE MESSAGES
// ============================================================================

bool MidiDeviceManager::sendMessage(
    const std::string& deviceId, 
    const MidiMessage& message
) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    auto it = connectedDevices_.find(deviceId);
    if (it == connectedDevices_.end()) {
        Logger::warn("MidiDeviceManager", 
            "Cannot send to disconnected device: " + deviceId);
        return false;
    }
    
    return it->second->send(message);
}

void MidiDeviceManager::broadcastMessage(const MidiMessage& message) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    for (auto& [id, device] : connectedDevices_) {
        try {
            device->send(message);
        } catch (const std::exception& e) {
            Logger::error("MidiDeviceManager", 
                "Failed to broadcast to " + id + ": " + std::string(e.what()));
        }
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void MidiDeviceManager::setOnDeviceConnected(DeviceConnectedCallback callback) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    onDeviceConnected_ = callback;
}

void MidiDeviceManager::setOnDeviceDisconnected(DeviceDisconnectedCallback callback) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    onDeviceDisconnected_ = callback;
}

void MidiDeviceManager::setOnMidiReceived(MidiReceivedCallback callback) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    onMidiReceived_ = callback;
    
    Logger::info("MidiDeviceManager", "✓ MIDI receive callback configured");
}

// ============================================================================
// FACTORY PATTERN - CRÉATION DE DEVICES
// ============================================================================

std::shared_ptr<MidiDevice> MidiDeviceManager::createDevice(const DeviceInfo& info) {
    Logger::info("MidiDeviceManager", "Creating device: " + info.name);
    
    try {
        switch (info.type) {
            case DeviceType::USB: {
                Logger::info("MidiDeviceManager", "Creating USB MIDI device...");
                
                int alsaClient = info.metadata.value("alsa_client", 0);
                int alsaPort = info.metadata.value("alsa_port", 0);
                
                return std::make_shared<UsbMidiDevice>(
                    info.id,
                    info.name,
                    alsaClient,
                    alsaPort
                );
            }
            
            case DeviceType::NETWORK: {
                Logger::info("MidiDeviceManager", "Creating Network MIDI device...");
                
                std::string address = info.metadata.value("address", "");
                int port = info.metadata.value("port", 5004);
                
                return std::make_shared<NetworkMidiDevice>(
                    info.id,
                    info.name,
                    address,
                    port
                );
            }
            
            case DeviceType::BLUETOOTH: {
                Logger::info("MidiDeviceManager", "Creating Bluetooth MIDI device...");
                
                std::string address = info.metadata.value("address", "");
                
                return std::make_shared<BleMidiDevice>(
                    info.id,
                    info.name,
                    address
                );
            }
            
            case DeviceType::VIRTUAL: {
                Logger::info("MidiDeviceManager", "Creating Virtual MIDI device...");
                
                return std::make_shared<VirtualMidiDevice>(
                    info.id,
                    info.name
                );
            }
            
            default:
                Logger::error("MidiDeviceManager", 
                    "Unknown device type: " + std::to_string(static_cast<int>(info.type)));
                return nullptr;
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiDeviceManager", 
            "Failed to create device: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiDeviceManager.cpp v3.2.0 - CORRIGÉ
// ============================================================================
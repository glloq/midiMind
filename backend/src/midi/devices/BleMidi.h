// ============================================================================
// Fichier: backend/src/midi/devices/MidiDeviceManager.cpp
// VERSION COMPLÈTE MISE À JOUR v3.1
// Projet: midiMind - Système d'Orchestration MIDI
// ============================================================================

#include "MidiDeviceManager.h"
#include "UsbMidiDevice.h"
#include "VirtualMidiDevice.h"
#include "WifiDevice.h"
#include "BleMidi.h"
#include "../../core/Logger.h"
#include "../../core/Config.h"
#include <alsa/asoundlib.h>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MidiDeviceManager::MidiDeviceManager() {
    Logger::info("MidiDeviceManager", "MidiDeviceManager constructed");
}

MidiDeviceManager::~MidiDeviceManager() {
    disconnectAll();
    Logger::info("MidiDeviceManager", "MidiDeviceManager destroyed");
}

// ============================================================================
// DÉCOUVERTE DE PÉRIPHÉRIQUES
// ============================================================================

std::vector<DeviceInfo> MidiDeviceManager::discoverDevices(bool rescan) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    if (rescan) {
        Logger::info("MidiDeviceManager", "Rescanning for MIDI devices...");
        availableDevices_.clear();
    }
    
    // Scanner USB (ALSA)
    discoverUSBDevices();
    
    // Scanner réseau (RTP-MIDI)
    discoverNetworkDevices();
    
    // Scanner Bluetooth
    discoverBluetoothDevices();
    
    Logger::info("MidiDeviceManager", 
        "Found " + std::to_string(availableDevices_.size()) + " devices");
    
    return availableDevices_;
}

std::vector<DeviceInfo> MidiDeviceManager::getAvailableDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    return availableDevices_;
}

DeviceInfo MidiDeviceManager::getDeviceInfo(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    for (const auto& info : availableDevices_) {
        if (info.id == deviceId) {
            return info;
        }
    }
    
    return DeviceInfo();
}

// ============================================================================
// CONNEXION / DÉCONNEXION
// ============================================================================

bool MidiDeviceManager::connect(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    // Vérifier si déjà connecté
    if (connectedDevices_.find(deviceId) != connectedDevices_.end()) {
        Logger::warn("MidiDeviceManager", "Device already connected: " + deviceId);
        return true;
    }
    
    // Trouver les infos du device
    DeviceInfo info;
    bool found = false;
    for (const auto& dev : availableDevices_) {
        if (dev.id == deviceId) {
            info = dev;
            found = true;
            break;
        }
    }
    
    if (!found) {
        Logger::error("MidiDeviceManager", "Device not found: " + deviceId);
        return false;
    }
    
    // Créer le device
    auto device = createDevice(info);
    if (!device) {
        Logger::error("MidiDeviceManager", "Failed to create device: " + deviceId);
        return false;
    }
    
    // Connecter
    if (!device->connect()) {
        Logger::error("MidiDeviceManager", "Failed to connect device: " + deviceId);
        return false;
    }
    
    // Stocker
    connectedDevices_[deviceId] = device;
    
    // Mettre à jour l'info
    for (auto& dev : availableDevices_) {
        if (dev.id == deviceId) {
            dev.connected = true;
            break;
        }
    }
    
    Logger::info("MidiDeviceManager", "✓ Connected device: " + deviceId);
    
    // Notifier via callback
    if (onDeviceConnected_) {
        onDeviceConnected_(device);
    }
    
    return true;
}

bool MidiDeviceManager::disconnect(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    auto it = connectedDevices_.find(deviceId);
    if (it == connectedDevices_.end()) {
        Logger::warn("MidiDeviceManager", "Device not connected: " + deviceId);
        return false;
    }
    
    Logger::info("MidiDeviceManager", "Disconnecting device: " + deviceId);
    
    try {
        it->second->disconnect();
    } catch (const std::exception& e) {
        Logger::error("MidiDeviceManager", 
            "Exception during disconnect: " + std::string(e.what()));
    }
    
    connectedDevices_.erase(it);
    
    // Mettre à jour l'info
    for (auto& dev : availableDevices_) {
        if (dev.id == deviceId) {
            dev.connected = false;
            break;
        }
    }
    
    Logger::info("MidiDeviceManager", "✓ Disconnected device: " + deviceId);
    
    // Notifier via callback
    if (onDeviceDisconnected_) {
        onDeviceDisconnected_(deviceId);
    }
    
    return true;
}

void MidiDeviceManager::disconnectAll() {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    if (connectedDevices_.empty()) {
        return;
    }
    
    Logger::info("MidiDeviceManager", 
        "Disconnecting " + std::to_string(connectedDevices_.size()) + " devices...");
    
    for (auto& [id, device] : connectedDevices_) {
        try {
            device->disconnect();
            
            // Mettre à jour l'info
            for (auto& dev : availableDevices_) {
                if (dev.id == id) {
                    dev.connected = false;
                    break;
                }
            }
            
            Logger::info("MidiDeviceManager", "  - Disconnected: " + id);
            
        } catch (const std::exception& e) {
            Logger::error("MidiDeviceManager", 
                "Error disconnecting " + id + ": " + std::string(e.what()));
        }
    }
    
    connectedDevices_.clear();
}

// ============================================================================
// ACCÈS AUX DEVICES
// ============================================================================

std::shared_ptr<MidiDevice> MidiDeviceManager::getDevice(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    auto it = connectedDevices_.find(deviceId);
    return it != connectedDevices_.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<MidiDevice>> MidiDeviceManager::getConnectedDevices() const {
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    std::vector<std::shared_ptr<MidiDevice>> devices;
    for (const auto& [id, device] : connectedDevices_) {
        devices.push_back(device);
    }
    return devices;
}

std::vector<std::shared_ptr<MidiDevice>> MidiDeviceManager::getDevicesByType(DeviceType type) const {
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

bool MidiDeviceManager::sendMessage(const std::string& deviceId, const MidiMessage& message) {
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
                "Exception broadcasting to " + id + ": " + std::string(e.what()));
        }
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void MidiDeviceManager::onDeviceConnected(DeviceCallback callback) {
    onDeviceConnected_ = callback;
}

void MidiDeviceManager::onDeviceDisconnected(DeviceCallback callback) {
    onDeviceDisconnected_ = callback;
}

// ============================================================================
// AUTO-RECONNECT
// ============================================================================

bool MidiDeviceManager::reconnectDevice(const std::string& deviceId) {
    Logger::info("MidiDeviceManager", "Attempting to reconnect device: " + deviceId);
    
    std::lock_guard<std::mutex> lock(devicesMutex_);
    
    auto it = connectedDevices_.find(deviceId);
    if (it == connectedDevices_.end()) {
        Logger::error("MidiDeviceManager", "Device not found for reconnection: " + deviceId);
        return false;
    }
    
    auto device = it->second;
    
    // Déconnecter proprement
    device->disconnect();
    
    // Attendre un peu
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Reconnecter
    if (device->connect()) {
        Logger::info("MidiDeviceManager", "✓ Device reconnected: " + deviceId);
        if (onDeviceConnected_) {
            onDeviceConnected_(device);
        }
        return true;
    } else {
        Logger::error("MidiDeviceManager", "Failed to reconnect device: " + deviceId);
        return false;
    }
}

// ============================================================================
// MÉTHODES PRIVÉES - DÉCOUVERTE USB/ALSA
// ============================================================================

void MidiDeviceManager::discoverUSBDevices() {
    Logger::info("MidiDeviceManager", "Scanning ALSA USB MIDI devices...");
    
    snd_seq_t* seq = nullptr;
    
    // Ouvrir le séquenceur ALSA
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        Logger::error("MidiDeviceManager", "Cannot open ALSA sequencer");
        return;
    }
    
    snd_seq_client_info_t* cinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    
    int usbCount = 0;
    
    // Parcourir tous les clients ALSA
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);
        
        // Ignorer le client système
        if (client == 0) continue;
        
        const char* clientName = snd_seq_client_info_get_name(cinfo);
        
        snd_seq_port_info_t* pinfo;
        snd_seq_port_info_alloca(&pinfo);
        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        
        // Parcourir tous les ports du client
        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            unsigned int caps = snd_seq_port_info_get_capability(pinfo);
            
            // Vérifier si c'est un port MIDI utilisable
            if ((caps & SND_SEQ_PORT_CAP_READ) || (caps & SND_SEQ_PORT_CAP_WRITE)) {
                int port = snd_seq_port_info_get_port(pinfo);
                const char* portName = snd_seq_port_info_get_name(pinfo);
                
                // Créer l'ID unique
                std::string deviceId = "usb_" + std::to_string(client) + "_" + std::to_string(port);
                
                // Déterminer direction
                DeviceDirection direction = DeviceDirection::OUTPUT;
                if (caps & SND_SEQ_PORT_CAP_READ) {
                    direction = DeviceDirection::INPUT;
                }
                if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_WRITE)) {
                    direction = DeviceDirection::BIDIRECTIONAL;
                }
                
                // Créer DeviceInfo
                DeviceInfo dev(deviceId, portName, DeviceType::USB, direction);
                dev.manufacturer = clientName;
                dev.model = portName;
                dev.metadata["alsa_client"] = client;
                dev.metadata["alsa_port"] = port;
                
                // Vérifier si déjà connecté
                if (connectedDevices_.find(deviceId) != connectedDevices_.end()) {
                    dev.connected = true;
                }
                
                availableDevices_.push_back(dev);
                usbCount++;
                
                Logger::debug("MidiDeviceManager", 
                    "  Found: " + deviceId + " - " + std::string(portName));
            }
        }
    }
    
    snd_seq_close(seq);
    
    Logger::info("MidiDeviceManager", "Found " + std::to_string(usbCount) + " USB MIDI devices");
}

// ============================================================================
// MÉTHODES PRIVÉES - DÉCOUVERTE RÉSEAU (MISE À JOUR v3.1)
// ============================================================================

void MidiDeviceManager::discoverNetworkDevices() {
    Logger::info("MidiDeviceManager", "Scanning network MIDI devices...");
    
    int netCount = 0;
    
    try {
        // ✅ NOUVEAU v3.1 : Utiliser la découverte mDNS
        auto networkServices = MdnsDiscoveryHelper::discoverServices(3);
        
        for (const auto& service : networkServices) {
            DeviceInfo info;
            info.id = "net_" + service.address + "_" + std::to_string(service.port);
            info.name = service.name.empty() ? ("Network MIDI " + service.address) : service.name;
            info.type = DeviceType::NETWORK;
            info.direction = DeviceDirection::BIDIRECTIONAL;
            info.metadata["address"] = service.address;
            info.metadata["port"] = service.port;
            info.metadata["hostname"] = service.hostname;
            info.metadata["discovery"] = "mdns";
            
            // Vérifier si déjà connecté
            if (connectedDevices_.find(info.id) != connectedDevices_.end()) {
                info.connected = true;
            }
            
            availableDevices_.push_back(info);
            netCount++;
            
            Logger::info("MidiDeviceManager", "Network device found: " + info.name + 
                        " at " + service.address + ":" + std::to_string(service.port));
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiDeviceManager", "mDNS discovery failed: " + std::string(e.what()));
    }
    
    // Fallback: chercher dans la config
    if (Config::instance().contains("network_devices")) {
        auto networkDevs = Config::instance().getValue("network_devices");
        
        for (const auto& devConfig : networkDevs) {
            DeviceInfo dev(
                devConfig["id"],
                devConfig["name"],
                DeviceType::NETWORK,
                DeviceDirection::BIDIRECTIONAL
            );
            
            dev.address = devConfig.value("address", "");
            dev.metadata["port"] = devConfig.value("port", 5004);
            dev.metadata["discovery"] = "config";
            
            availableDevices_.push_back(dev);
            netCount++;
        }
    }
    
    Logger::info("MidiDeviceManager", "✓ Network scan complete (" + 
                std::to_string(netCount) + " devices)");
}

// ============================================================================
// MÉTHODES PRIVÉES - DÉCOUVERTE BLUETOOTH (MISE À JOUR v3.1)
// ============================================================================

void MidiDeviceManager::discoverBluetoothDevices() {
    Logger::info("MidiDeviceManager", "Scanning Bluetooth LE MIDI devices...");
    
    int bleCount = 0;
    
    try {
        // ✅ NOUVEAU v3.1 : Utiliser le plugin BleMidiPlugin
        BleMidiPlugin blePlugin;
        auto bleDevices = blePlugin.scan();
        
        for (const auto& bleInfo : bleDevices) {
            // Vérifier si déjà connecté
            if (connectedDevices_.find(bleInfo.id) != connectedDevices_.end()) {
                bleInfo.connected = true;
            }
            
            availableDevices_.push_back(bleInfo);
            bleCount++;
            
            Logger::info("MidiDeviceManager", "Bluetooth device found: " + bleInfo.name + 
                        " (" + bleInfo.metadata.value("address", "") + ")");
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiDeviceManager", "Bluetooth scan failed: " + std::string(e.what()));
    }
    
    // Fallback: chercher dans la config
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
            
            availableDevices_.push_back(dev);
            bleCount++;
        }
    }
    
    Logger::info("MidiDeviceManager", "✓ Bluetooth scan complete (" + 
                std::to_string(bleCount) + " devices)");
}

// ============================================================================
// MÉTHODE PRIVÉE - CRÉATION DE DEVICE (MISE À JOUR v3.1)
// ============================================================================

std::shared_ptr<MidiDevice> MidiDeviceManager::createDevice(const DeviceInfo& info) {
    Logger::info("MidiDeviceManager", "Creating device: " + info.name);
    
    try {
        switch (info.type) {
            // ================================================================
            // USB MIDI (ALSA)
            // ================================================================
            case DeviceType::USB: {
                auto device = std::make_shared<UsbMidiDevice>(
                    info.id,
                    info.name,
                    info.metadata.value("alsa_client", 0),
                    info.metadata.value("alsa_port", 0)
                );
                
                Logger::info("MidiDeviceManager", "✓ USB device created: " + info.id);
                return device;
            }
            
            // ================================================================
            // ✅ NETWORK MIDI - IMPLÉMENTATION COMPLÈTE v3.1
            // ================================================================
            case DeviceType::NETWORK: {
                Logger::info("MidiDeviceManager", "Creating network MIDI device...");
                
                // Récupérer les paramètres réseau
                std::string address = info.metadata.value("address", "");
                int port = info.metadata.value("port", 5004);
                
                if (address.empty()) {
                    Logger::error("MidiDeviceManager", "Network device requires address");
                    return nullptr;
                }
                
                // Créer WifiMidiDevice (RTP-MIDI ou MIDI over TCP/IP)
                auto device = std::make_shared<WifiMidiDevice>(
                    info.id,
                    info.name,
                    address,
                    port
                );
                
                Logger::info("MidiDeviceManager", "✓ Network device created: " + info.id + 
                            " (" + address + ":" + std::to_string(port) + ")");
                
                return device;
            }
            
            // ================================================================
            // ✅ BLUETOOTH LE MIDI - IMPLÉMENTATION COMPLÈTE v3.1
            // ================================================================
            case DeviceType::BLUETOOTH: {
                Logger::info("MidiDeviceManager", "Creating Bluetooth LE MIDI device...");
                
                // Récupérer l'adresse Bluetooth
                std::string btAddress = info.metadata.value("address", "");
                
                if (btAddress.empty()) {
                    Logger::error("MidiDeviceManager", "Bluetooth device requires address");
                    return nullptr;
                }
                
                // Créer BleMidiDevice
                auto device = std::make_shared<BleMidiDevice>(
                    info.id,
                    info.name,
                    btAddress
                );
                
                Logger::info("MidiDeviceManager", "✓ Bluetooth device created: " + info.id + 
                            " (" + btAddress + ")");
                
                return device;
            }
            
            // ================================================================
            // VIRTUAL MIDI
            // ================================================================
            case DeviceType::VIRTUAL: {
                auto device = std::make_shared<VirtualMidiDevice>(info.id, info.name);
                Logger::info("MidiDeviceManager", "✓ Virtual device created: " + info.id);
                return device;
            }
            
            // ================================================================
            // TYPE INCONNU
            // ================================================================
            default:
                Logger::error("MidiDeviceManager", "Unknown device type: " + 
                            std::to_string(static_cast<int>(info.type)));
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
// FIN DU FICHIER MidiDeviceManager.cpp
// ============================================================================

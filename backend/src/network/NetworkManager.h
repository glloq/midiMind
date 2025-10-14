// ============================================================================
// Fichier: src/network/NetworkManager.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "NetworkManager.h"
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

NetworkManager::NetworkManager() {
    Logger::info("NetworkManager", "═══════════════════════════════════════");
    Logger::info("NetworkManager", "  Initializing NetworkManager v3.0");
    Logger::info("NetworkManager", "═══════════════════════════════════════");
    
    try {
        // Créer les composants
        Logger::info("NetworkManager", "Creating RTP-MIDI Server...");
        rtpMidiServer_ = std::make_unique<RtpMidiServer>();
        Logger::info("NetworkManager", "✓ RTP-MIDI Server created");
        
        Logger::info("NetworkManager", "Creating mDNS Discovery...");
        mdnsDiscovery_ = std::make_unique<MdnsDiscovery>();
        Logger::info("NetworkManager", "✓ mDNS Discovery created");
        
        Logger::info("NetworkManager", "Creating BLE MIDI Device...");
        bleMidiDevice_ = std::make_unique<BleMidiDevice>();
        Logger::info("NetworkManager", "✓ BLE MIDI Device created");
        
        Logger::info("NetworkManager", "Creating WiFi Hotspot...");
        wifiHotspot_ = std::make_unique<WiFiHotspot>();
        Logger::info("NetworkManager", "✓ WiFi Hotspot created");
        
        // Initialiser les statistiques
        stats_ = {};
        
        Logger::info("NetworkManager", "✓ NetworkManager initialized successfully");
        
    } catch (const std::exception& e) {
        Logger::error("NetworkManager", "Initialization failed: " + std::string(e.what()));
        throw;
    }
}

NetworkManager::~NetworkManager() {
    Logger::info("NetworkManager", "Shutting down NetworkManager...");
    
    // Arrêter tous les services
    stopRtpMidi();
    stopDiscovery();
    stopBleMidi();
    stopWiFiHotspot();
    
    Logger::info("NetworkManager", "✓ NetworkManager destroyed");
}

// ============================================================================
// CONTRÔLE DES SERVICES - RTP-MIDI
// ============================================================================

bool NetworkManager::startRtpMidi(uint16_t port, const std::string& serviceName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Starting RTP-MIDI service...");
    Logger::info("NetworkManager", "  Port: " + std::to_string(port));
    Logger::info("NetworkManager", "  Service: " + serviceName);
    
    if (rtpMidiServer_->isRunning()) {
        Logger::warn("NetworkManager", "RTP-MIDI already running");
        return false;
    }
    
    // Configurer les callbacks
    rtpMidiServer_->setOnMidiReceived([this](const MidiMessage& msg, const std::string& sessionId) {
        Logger::debug("NetworkManager", "MIDI received from RTP session: " + sessionId);
        // TODO: Router vers MidiRouter
    });
    
    rtpMidiServer_->setOnClientConnected([this](const std::string& sessionId, const std::string& clientName) {
        Logger::info("NetworkManager", "RTP-MIDI client connected: " + clientName);
        
        NetworkDeviceInfo info;
        info.id = sessionId;
        info.name = clientName;
        info.type = NetworkDeviceType::RTP_MIDI;
        info.connected = true;
        info.lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        handleDeviceConnected(sessionId);
        
        if (onDeviceConnected_) {
            onDeviceConnected_(sessionId);
        }
    });
    
    rtpMidiServer_->setOnClientDisconnected([this](const std::string& sessionId) {
        Logger::info("NetworkManager", "RTP-MIDI client disconnected: " + sessionId);
        handleDeviceDisconnected(sessionId);
        
        if (onDeviceDisconnected_) {
            onDeviceDisconnected_(sessionId);
        }
    });
    
    // Démarrer le serveur
    if (rtpMidiServer_->start(port, serviceName)) {
        Logger::info("NetworkManager", "✓ RTP-MIDI service started");
        return true;
    }
    
    Logger::error("NetworkManager", "Failed to start RTP-MIDI service");
    return false;
}

void NetworkManager::stopRtpMidi() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (rtpMidiServer_ && rtpMidiServer_->isRunning()) {
        Logger::info("NetworkManager", "Stopping RTP-MIDI service...");
        rtpMidiServer_->stop();
        Logger::info("NetworkManager", "✓ RTP-MIDI service stopped");
    }
}

bool NetworkManager::isRtpMidiRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rtpMidiServer_ && rtpMidiServer_->isRunning();
}

// ============================================================================
// CONTRÔLE DES SERVICES - DÉCOUVERTE mDNS
// ============================================================================

bool NetworkManager::startDiscovery() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Starting mDNS discovery...");
    
    if (mdnsDiscovery_->isRunning()) {
        Logger::warn("NetworkManager", "mDNS discovery already running");
        return false;
    }
    
    // Configurer les callbacks
    mdnsDiscovery_->setOnServiceDiscovered([this](const ServiceInfo& info) {
        Logger::info("NetworkManager", "Service discovered: " + info.name + 
                    " at " + info.address + ":" + std::to_string(info.port));
        
        // Convertir en NetworkDeviceInfo
        NetworkDeviceInfo deviceInfo;
        deviceInfo.id = info.name;
        deviceInfo.name = info.name;
        deviceInfo.type = NetworkDeviceType::RTP_MIDI;
        deviceInfo.address = info.address;
        deviceInfo.port = info.port;
        deviceInfo.connected = false;
        deviceInfo.lastSeen = info.lastSeen;
        
        handleDeviceDiscovered(deviceInfo);
        
        if (onDeviceDiscovered_) {
            onDeviceDiscovered_(deviceInfo);
        }
    });
    
    mdnsDiscovery_->setOnServiceRemoved([this](const std::string& serviceName) {
        Logger::info("NetworkManager", "Service removed: " + serviceName);
        
        // Retirer de la liste des devices découverts
        auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
            [&serviceName](const NetworkDeviceInfo& info) {
                return info.id == serviceName;
            });
        
        if (it != discoveredDevices_.end()) {
            discoveredDevices_.erase(it);
        }
    });
    
    // Démarrer la découverte
    if (mdnsDiscovery_->start()) {
        // Rechercher les services RTP-MIDI
        mdnsDiscovery_->browse("_apple-midi._udp");
        
        Logger::info("NetworkManager", "✓ mDNS discovery started");
        return true;
    }
    
    Logger::error("NetworkManager", "Failed to start mDNS discovery");
    return false;
}

void NetworkManager::stopDiscovery() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (mdnsDiscovery_ && mdnsDiscovery_->isRunning()) {
        Logger::info("NetworkManager", "Stopping mDNS discovery...");
        mdnsDiscovery_->stop();
        Logger::info("NetworkManager", "✓ mDNS discovery stopped");
    }
}

// ============================================================================
// CONTRÔLE DES SERVICES - BLUETOOTH MIDI
// ============================================================================

bool NetworkManager::startBleMidi(const std::string& deviceName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Starting BLE MIDI...");
    Logger::info("NetworkManager", "  Device name: " + deviceName);
    
    if (bleMidiDevice_->isRunning()) {
        Logger::warn("NetworkManager", "BLE MIDI already running");
        return false;
    }
    
    // Configurer les callbacks
    bleMidiDevice_->setOnMidiReceived([this](const MidiMessage& msg) {
        Logger::debug("NetworkManager", "MIDI received from BLE");
        // TODO: Router vers MidiRouter
    });
    
    bleMidiDevice_->setOnClientConnected([this](const std::string& address) {
        Logger::info("NetworkManager", "BLE client connected: " + address);
        
        NetworkDeviceInfo info;
        info.id = "ble_" + address;
        info.name = "BLE MIDI Client";
        info.type = NetworkDeviceType::BLE_MIDI;
        info.address = address;
        info.connected = true;
        info.lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        handleDeviceConnected(info.id);
        
        if (onDeviceConnected_) {
            onDeviceConnected_(info.id);
        }
    });
    
    bleMidiDevice_->setOnClientDisconnected([this](const std::string& address) {
        Logger::info("NetworkManager", "BLE client disconnected: " + address);
        handleDeviceDisconnected("ble_" + address);
        
        if (onDeviceDisconnected_) {
            onDeviceDisconnected_("ble_" + address);
        }
    });
    
    // Démarrer BLE MIDI
    if (bleMidiDevice_->start(deviceName)) {
        Logger::info("NetworkManager", "✓ BLE MIDI started");
        return true;
    }
    
    Logger::error("NetworkManager", "Failed to start BLE MIDI");
    return false;
}

void NetworkManager::stopBleMidi() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (bleMidiDevice_ && bleMidiDevice_->isRunning()) {
        Logger::info("NetworkManager", "Stopping BLE MIDI...");
        bleMidiDevice_->stop();
        Logger::info("NetworkManager", "✓ BLE MIDI stopped");
    }
}

bool NetworkManager::isBleMidiRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bleMidiDevice_ && bleMidiDevice_->isRunning();
}

// ============================================================================
// CONTRÔLE DES SERVICES - WIFI HOTSPOT
// ============================================================================

bool NetworkManager::startWiFiHotspot(const std::string& ssid, 
                                     const std::string& password,
                                     uint8_t channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Starting WiFi Hotspot...");
    Logger::info("NetworkManager", "  SSID: " + ssid);
    Logger::info("NetworkManager", "  Channel: " + std::to_string(channel));
    
    if (wifiHotspot_->isRunning()) {
        Logger::warn("NetworkManager", "WiFi Hotspot already running");
        return false;
    }
    
    // Configurer les callbacks
    wifiHotspot_->setOnClientConnected([this](const WiFiClient& client) {
        Logger::info("NetworkManager", "WiFi client connected: " + client.ipAddress + 
                    " (" + client.macAddress + ")");
        
        stats_.hotspotClients++;
    });
    
    wifiHotspot_->setOnClientDisconnected([this](const std::string& macAddress) {
        Logger::info("NetworkManager", "WiFi client disconnected: " + macAddress);
        
        if (stats_.hotspotClients > 0) {
            stats_.hotspotClients--;
        }
    });
    
    // Démarrer le hotspot
    if (wifiHotspot_->start(ssid, password, channel)) {
        stats_.hotspotActive = true;
        Logger::info("NetworkManager", "✓ WiFi Hotspot started");
        return true;
    }
    
    Logger::error("NetworkManager", "Failed to start WiFi Hotspot");
    return false;
}

void NetworkManager::stopWiFiHotspot() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (wifiHotspot_ && wifiHotspot_->isRunning()) {
        Logger::info("NetworkManager", "Stopping WiFi Hotspot...");
        wifiHotspot_->stop();
        stats_.hotspotActive = false;
        stats_.hotspotClients = 0;
        Logger::info("NetworkManager", "✓ WiFi Hotspot stopped");
    }
}

bool NetworkManager::isWiFiHotspotRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wifiHotspot_ && wifiHotspot_->isRunning();
}

// ============================================================================
// GESTION DES DEVICES
// ============================================================================

std::vector<NetworkDeviceInfo> NetworkManager::listDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return discoveredDevices_;
}

bool NetworkManager::connectDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Connecting to device: " + deviceId);
    
    // Trouver le device
    auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
        [&deviceId](const NetworkDeviceInfo& info) {
            return info.id == deviceId;
        });
    
    if (it == discoveredDevices_.end()) {
        Logger::error("NetworkManager", "Device not found: " + deviceId);
        return false;
    }
    
    // Pour RTP-MIDI, la connexion se fait automatiquement
    // Le client doit se connecter à notre serveur
    
    Logger::info("NetworkManager", "Device connection initiated");
    return true;
}

bool NetworkManager::disconnectDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Disconnecting device: " + deviceId);
    
    // Fermer la session RTP-MIDI si c'est un device RTP
    if (rtpMidiServer_ && rtpMidiServer_->isRunning()) {
        rtpMidiServer_->closeSession(deviceId);
    }
    
    // Mettre à jour l'état
    auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
        [&deviceId](const NetworkDeviceInfo& info) {
            return info.id == deviceId;
        });
    
    if (it != discoveredDevices_.end()) {
        it->connected = false;
    }
    
    Logger::info("NetworkManager", "Device disconnected");
    return true;
}

std::optional<NetworkDeviceInfo> NetworkManager::getDevice(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
        [&deviceId](const NetworkDeviceInfo& info) {
            return info.id == deviceId;
        });
    
    if (it != discoveredDevices_.end()) {
        return *it;
    }
    
    return std::nullopt;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void NetworkManager::setOnDeviceDiscovered(DeviceDiscoveredCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onDeviceDiscovered_ = callback;
}

void NetworkManager::setOnDeviceConnected(DeviceConnectedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onDeviceConnected_ = callback;
}

void NetworkManager::setOnDeviceDisconnected(DeviceDisconnectedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onDeviceDisconnected_ = callback;
}

// ============================================================================
// STATISTIQUES
// ============================================================================

NetworkStatistics NetworkManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    NetworkStatistics stats = stats_;
    
    // Mettre à jour avec les stats des composants
    if (rtpMidiServer_ && rtpMidiServer_->isRunning()) {
        auto rtpStats = rtpMidiServer_->getStatistics();
        stats.rtpDevicesDiscovered = discoveredDevices_.size();
        stats.rtpDevicesConnected = rtpStats["active_sessions"].get<size_t>();
        stats.rtpBytesReceived = rtpStats["bytes_received"].get<uint64_t>();
        stats.rtpBytesSent = rtpStats["bytes_sent"].get<uint64_t>();
    }
    
    if (bleMidiDevice_ && bleMidiDevice_->isRunning()) {
        auto bleStats = bleMidiDevice_->getStatistics();
        stats.bleDevicesConnected = bleStats["connected_clients"].get<size_t>();
        stats.bleBytesReceived = bleStats["bytes_received"].get<uint64_t>();
        stats.bleBytesSent = bleStats["bytes_sent"].get<uint64_t>();
    }
    
    if (wifiHotspot_ && wifiHotspot_->isRunning()) {
        auto wifiStats = wifiHotspot_->getStatistics();
        stats.hotspotActive = wifiStats["running"].get<bool>();
        stats.hotspotClients = wifiStats["connected_clients"].get<size_t>();
    }
    
    return stats;
}

std::string NetworkManager::getLocalIpAddress() const {
    return detectLocalIpAddress();
}

json NetworkManager::getNetworkInfo() const {
    json info;
    
    // IP Address
    info["ip_address"] = detectLocalIpAddress();
    info["mac_address"] = detectMacAddress();
    
    // Hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        info["hostname"] = std::string(hostname);
    } else {
        info["hostname"] = "unknown";
    }
    
    // Mode réseau
    if (wifiHotspot_ && wifiHotspot_->isRunning()) {
        info["network_mode"] = "hotspot";
    } else {
        info["network_mode"] = "client";
    }
    
    // ========================================================================
    // AMÉLIORATION: Détection réelle du WiFi
    // ========================================================================
    
    // État WiFi connecté (via wpa_cli)
    bool wifiConnected = false;
    std::string wifiSsid = "";
    int wifiSignal = 0;
    
    FILE* pipe = popen("wpa_cli -i wlan0 status 2>/dev/null", "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            
            // Supprimer le retour à la ligne
            if (!line.empty() && line.back() == '\n') {
                line.pop_back();
            }
            
            // Parser wpa_state=COMPLETED
            if (line.find("wpa_state=COMPLETED") != std::string::npos) {
                wifiConnected = true;
            }
            
            // Parser ssid=MonReseau
            if (line.find("ssid=") == 0) {
                wifiSsid = line.substr(5);
            }
        }
        pclose(pipe);
    }
    
    // Obtenir la force du signal WiFi (si connecté)
    if (wifiConnected) {
        pipe = popen("iw dev wlan0 link 2>/dev/null | grep signal | awk '{print $2}'", "r");
        if (pipe) {
            char buffer[32];
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                try {
                    wifiSignal = std::stoi(std::string(buffer));
                } catch (...) {
                    wifiSignal = 0;
                }
            }
            pclose(pipe);
        }
    }
    
    info["wifi_connected"] = wifiConnected;
    info["wifi_ssid"] = wifiSsid;
    info["wifi_signal"] = wifiSignal;
    
    // ========================================================================
    // Interfaces réseau disponibles
    // ========================================================================
    
    json interfaces = json::array();
    
    struct ifaddrs* ifAddrStruct = nullptr;
    getifaddrs(&ifAddrStruct);
    
    for (struct ifaddrs* ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        
        if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4
            json iface;
            iface["name"] = std::string(ifa->ifa_name);
            
            // IP Address
            void* tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            iface["ip"] = std::string(addressBuffer);
            
            // Type d'interface
            std::string ifname = ifa->ifa_name;
            if (ifname.find("lo") == 0) {
                iface["type"] = "loopback";
            } else if (ifname.find("eth") == 0) {
                iface["type"] = "ethernet";
            } else if (ifname.find("wlan") == 0) {
                iface["type"] = "wifi";
            } else {
                iface["type"] = "other";
            }
            
            interfaces.push_back(iface);
        }
    }
    
    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }
    
    info["interfaces"] = interfaces;
    
    // ========================================================================
    // Statistiques réseau globales
    // ========================================================================
    
    // Lire /proc/net/dev pour les stats
    std::ifstream netDev("/proc/net/dev");
    if (netDev.is_open()) {
        std::string line;
        std::getline(netDev, line); // Skip header 1
        std::getline(netDev, line); // Skip header 2
        
        uint64_t totalRx = 0;
        uint64_t totalTx = 0;
        
        while (std::getline(netDev, line)) {
            // Format: interface: rx_bytes rx_packets ... tx_bytes tx_packets ...
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string ifname = line.substr(0, colonPos);
                
                // Trim whitespace
                ifname.erase(0, ifname.find_first_not_of(" \t"));
                ifname.erase(ifname.find_last_not_of(" \t") + 1);
                
                // Ignorer loopback
                if (ifname == "lo") continue;
                
                std::istringstream iss(line.substr(colonPos + 1));
                uint64_t rxBytes, rxPackets, rxErrs, rxDrop;
                uint64_t txBytes, txPackets, txErrs, txDrop;
                
                // Lire les colonnes importantes
                if (iss >> rxBytes >> rxPackets >> rxErrs >> rxDrop) {
                    // Skip 4 columns
                    uint64_t dummy;
                    iss >> dummy >> dummy >> dummy >> dummy;
                    
                    if (iss >> txBytes >> txPackets) {
                        totalRx += rxBytes;
                        totalTx += txBytes;
                    }
                }
            }
        }
        
        info["total_bytes_received"] = totalRx;
        info["total_bytes_sent"] = totalTx;
        
        netDev.close();
    }
    
    return info;
}


// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void NetworkManager::handleDeviceDiscovered(const NetworkDeviceInfo& info) {
    // Ajouter à la liste si pas déjà présent
    auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
        [&info](const NetworkDeviceInfo& d) {
            return d.id == info.id;
        });
    
    if (it == discoveredDevices_.end()) {
        discoveredDevices_.push_back(info);
        stats_.rtpDevicesDiscovered++;
    } else {
        // Mettre à jour
        *it = info;
    }
}

void NetworkManager::handleDeviceConnected(const std::string& deviceId) {
    auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
        [&deviceId](const NetworkDeviceInfo& info) {
            return info.id == deviceId;
        });
    
    if (it != discoveredDevices_.end()) {
        it->connected = true;
        it->lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
}

void NetworkManager::handleDeviceDisconnected(const std::string& deviceId) {
    auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
        [&deviceId](const NetworkDeviceInfo& info) {
            return info.id == deviceId;
        });
    
    if (it != discoveredDevices_.end()) {
        it->connected = false;
    }
}

std::string NetworkManager::detectLocalIpAddress() const {
    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa = nullptr;
    void* tmpAddrPtr = nullptr;
    std::string result = "127.0.0.1";
    
    getifaddrs(&ifAddrStruct);
    
    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        
        // IPv4
        if (ifa->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            
            std::string addr(addressBuffer);
            
            // Ignorer loopback
            if (addr != "127.0.0.1" && addr.find("127.") != 0) {
                result = addr;
                break;
            }
        }
    }
    
    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }
    
    return result;
}

std::string NetworkManager::detectMacAddress() const {
    struct ifaddrs* ifAddrStruct = nullptr;
    std::string result = "00:00:00:00:00:00";
    
    getifaddrs(&ifAddrStruct);
    
    for (struct ifaddrs* ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        
        // Chercher wlan0 ou eth0
        std::string ifname(ifa->ifa_name);
        if (ifname == "wlan0" || ifname == "eth0") {
            // Lire l'adresse MAC depuis /sys/class/net/
            std::string path = "/sys/class/net/" + ifname + "/address";
            std::ifstream macFile(path);
            
            if (macFile.is_open()) {
                std::getline(macFile, result);
                macFile.close();
                break;
            }
        }
    }
    
    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }
    
    return result;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER NetworkManager.cpp
// ============================================================================
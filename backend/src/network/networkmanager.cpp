// ============================================================================
// Fichier: src/network/networkmanager.cpp
// Version: 3.0.0 - COMPLET
// Date: 2025-10-15
// ============================================================================

#include "networkmanager.h"
#include "../core/Logger.h"
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
    Logger::info("NetworkManager", "╔═══════════════════════════════════════╗");
    Logger::info("NetworkManager", "  Initializing NetworkManager v3.0");
    Logger::info("NetworkManager", "╚═══════════════════════════════════════╝");
    
    try {
        // Créer les gestionnaires
        Logger::info("NetworkManager", "Creating WifiManager...");
        wifiManager_ = std::make_unique<WifiManager>();
        Logger::info("NetworkManager", "✓ WifiManager created");
        
        Logger::info("NetworkManager", "Creating BluetoothManager...");
        bluetoothManager_ = std::make_unique<BluetoothManager>();
        Logger::info("NetworkManager", "✓ BluetoothManager created");
        
        Logger::info("NetworkManager", "Creating WiFiHotspot...");
        wifiHotspot_ = std::make_unique<WiFiHotspot>();
        Logger::info("NetworkManager", "✓ WiFiHotspot created");
        
        Logger::info("NetworkManager", "Creating BleMidiDevice...");
        bleMidiDevice_ = std::make_unique<BleMidiDevice>();
        Logger::info("NetworkManager", "✓ BleMidiDevice created");
        
        Logger::info("NetworkManager", "Creating MdnsDiscovery...");
        mdnsDiscovery_ = std::make_unique<MdnsDiscovery>();
        Logger::info("NetworkManager", "✓ MdnsDiscovery created");
        
        Logger::info("NetworkManager", "Creating RtpMidiServer...");
        rtpMidiServer_ = std::make_unique<RtpMidiServer>();
        Logger::info("NetworkManager", "✓ RtpMidiServer created");
        
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
    disconnectWifi();
    stopBluetoothScan();
    
    Logger::info("NetworkManager", "✓ NetworkManager shut down");
}

// ============================================================================
// WIFI CLIENT
// ============================================================================

bool NetworkManager::startWifiScan() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Starting WiFi scan...");
    
    // Configurer le callback
    wifiManager_->setOnScanComplete([this](const std::vector<WiFiNetwork>& networks) {
        Logger::info("NetworkManager", "WiFi scan complete: " + 
                    std::to_string(networks.size()) + " networks found");
        
        // Convertir en NetworkDeviceInfo et notifier
        for (const auto& network : networks) {
            NetworkDeviceInfo info;
            info.id = "wifi_" + network.ssid;
            info.name = network.ssid;
            info.type = NetworkDeviceType::WIFI_CLIENT;
            info.address = network.bssid;
            info.port = 0;
            info.connected = network.connected;
            info.lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            handleDeviceDiscovered(info);
        }
    });
    
    return wifiManager_->startScan();
}

bool NetworkManager::connectWifi(const std::string& ssid, 
                                 const std::string& password,
                                 bool autoReconnect) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Connecting to WiFi: " + ssid);
    
    // Configurer les callbacks
    wifiManager_->setOnConnectionChange([this](bool success, const std::string& ssid) {
        if (success) {
            Logger::info("NetworkManager", "WiFi connected: " + ssid);
            stats_.wifiConnected = true;
            stats_.wifiSsid = ssid;
            
            if (onDeviceConnected_) {
                onDeviceConnected_("wifi_" + ssid);
            }
        } else {
            Logger::error("NetworkManager", "WiFi connection failed: " + ssid);
        }
    });
    
    wifiManager_->setOnDisconnection([this](const std::string& ssid) {
        Logger::info("NetworkManager", "WiFi disconnected: " + ssid);
        stats_.wifiConnected = false;
        stats_.wifiSsid.clear();
        
        if (onDeviceDisconnected_) {
            onDeviceDisconnected_("wifi_" + ssid);
        }
    });
    
    return wifiManager_->connect(ssid, password, autoReconnect);
}

bool NetworkManager::disconnectWifi() {
    std::lock_guard<std::mutex> lock(mutex_);
    return wifiManager_->disconnect();
}

bool NetworkManager::isWifiConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wifiManager_->isConnected();
}

std::vector<WiFiNetwork> NetworkManager::getWifiNetworks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wifiManager_->getLastScanResults();
}

// ============================================================================
// WIFI HOTSPOT
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
        
        // Créer NetworkDeviceInfo
        NetworkDeviceInfo info;
        info.id = "hotspot_" + client.macAddress;
        info.name = client.hostname.empty() ? "WiFi Client" : client.hostname;
        info.type = NetworkDeviceType::WIFI_CLIENT;
        info.address = client.ipAddress;
        info.port = 0;
        info.connected = true;
        info.lastSeen = client.connectedSince;
        
        handleDeviceConnected(info.id);
    });
    
    wifiHotspot_->setOnClientDisconnected([this](const std::string& macAddress) {
        Logger::info("NetworkManager", "WiFi client disconnected: " + macAddress);
        
        if (stats_.hotspotClients > 0) {
            stats_.hotspotClients--;
        }
        
        handleDeviceDisconnected("hotspot_" + macAddress);
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

std::vector<WiFiClient> NetworkManager::getHotspotClients() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wifiHotspot_->listClients();
}

// ============================================================================
// BLUETOOTH
// ============================================================================

bool NetworkManager::startBluetoothScan(int duration) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Starting Bluetooth scan...");
    
    // Initialiser si nécessaire
    if (!bluetoothManager_->isInitialized()) {
        if (!bluetoothManager_->initialize()) {
            Logger::error("NetworkManager", "Failed to initialize BluetoothManager");
            return false;
        }
    }
    
    // Configurer les callbacks
    bluetoothManager_->setOnDeviceDiscovered([this](const BluetoothDevice& device) {
        Logger::info("NetworkManager", "Bluetooth device discovered: " + device.name + 
                    " (" + device.address + ")");
        
        NetworkDeviceInfo info;
        info.id = "bt_" + device.address;
        info.name = device.name;
        info.type = NetworkDeviceType::BLUETOOTH_DEVICE;
        info.address = device.address;
        info.port = 0;
        info.connected = (device.state == BluetoothDeviceState::CONNECTED);
        info.lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        handleDeviceDiscovered(info);
        stats_.bluetoothDevicesDiscovered++;
    });
    
    bluetoothManager_->setOnDeviceStateChanged([this](const std::string& address, 
                                                      BluetoothDeviceState state) {
        if (state == BluetoothDeviceState::CONNECTED) {
            handleDeviceConnected("bt_" + address);
        } else if (state == BluetoothDeviceState::PAIRED) {
            stats_.bluetoothDevicesPaired++;
        }
    });
    
    return bluetoothManager_->startScan(duration);
}

void NetworkManager::stopBluetoothScan() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (bluetoothManager_) {
        bluetoothManager_->stopScan();
    }
}

bool NetworkManager::pairBluetoothDevice(const std::string& address, const std::string& pin) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Pairing Bluetooth device: " + address);
    return bluetoothManager_->pair(address, pin);
}

bool NetworkManager::connectBluetoothDevice(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Connecting Bluetooth device: " + address);
    return bluetoothManager_->connect(address);
}

bool NetworkManager::disconnectBluetoothDevice(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Disconnecting Bluetooth device: " + address);
    return bluetoothManager_->disconnect(address);
}

std::vector<BluetoothDevice> NetworkManager::getBluetoothDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bluetoothManager_->getDiscoveredDevices();
}

// ============================================================================
// BLE MIDI
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
    bleMidiDevice_->setOnClientConnected([this](const std::string& address) {
        Logger::info("NetworkManager", "BLE MIDI client connected: " + address);
        
        NetworkDeviceInfo info;
        info.id = "ble_" + address;
        info.name = "BLE MIDI Client";
        info.type = NetworkDeviceType::BLE_MIDI;
        info.address = address;
        info.port = 0;
        info.connected = true;
        info.lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        handleDeviceConnected(info.id);
    });
    
    bleMidiDevice_->setOnClientDisconnected([this](const std::string& address) {
        Logger::info("NetworkManager", "BLE MIDI client disconnected: " + address);
        handleDeviceDisconnected("ble_" + address);
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
// RTP-MIDI
// ============================================================================

bool NetworkManager::startRtpMidi(uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Starting RTP-MIDI server...");
    Logger::info("NetworkManager", "  Port: " + std::to_string(port));
    
    if (rtpMidiServer_->isRunning()) {
        Logger::warn("NetworkManager", "RTP-MIDI server already running");
        return false;
    }
    
    // Configurer les callbacks
    rtpMidiServer_->setOnClientConnected([this](const std::string& sessionId) {
        Logger::info("NetworkManager", "RTP-MIDI client connected: " + sessionId);
        handleDeviceConnected("rtp_" + sessionId);
    });
    
    rtpMidiServer_->setOnClientDisconnected([this](const std::string& sessionId) {
        Logger::info("NetworkManager", "RTP-MIDI client disconnected: " + sessionId);
        handleDeviceDisconnected("rtp_" + sessionId);
    });
    
    // Démarrer le serveur
    if (rtpMidiServer_->start(port)) {
        Logger::info("NetworkManager", "✓ RTP-MIDI server started");
        return true;
    }
    
    Logger::error("NetworkManager", "Failed to start RTP-MIDI server");
    return false;
}

void NetworkManager::stopRtpMidi() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (rtpMidiServer_ && rtpMidiServer_->isRunning()) {
        Logger::info("NetworkManager", "Stopping RTP-MIDI server...");
        rtpMidiServer_->stop();
        Logger::info("NetworkManager", "✓ RTP-MIDI server stopped");
    }
}

bool NetworkManager::isRtpMidiRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rtpMidiServer_ && rtpMidiServer_->isRunning();
}

// ============================================================================
// mDNS DISCOVERY
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
        deviceInfo.id = "mdns_" + info.name;
        deviceInfo.name = info.name;
        deviceInfo.type = NetworkDeviceType::RTP_MIDI;
        deviceInfo.address = info.address;
        deviceInfo.port = info.port;
        deviceInfo.connected = false;
        deviceInfo.lastSeen = info.lastSeen;
        
        handleDeviceDiscovered(deviceInfo);
    });
    
    mdnsDiscovery_->setOnServiceRemoved([this](const std::string& serviceName) {
        Logger::info("NetworkManager", "Service removed: " + serviceName);
        
        // Retirer de la liste des devices découverts
        auto it = std::find_if(discoveredDevices_.begin(), discoveredDevices_.end(),
            [&serviceName](const NetworkDeviceInfo& info) {
                return info.id == "mdns_" + serviceName;
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

bool NetworkManager::publishService(const std::string& name, 
                                    const std::string& type,
                                    uint16_t port) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Publishing service: " + name);
    return mdnsDiscovery_->publish(name, type, port);
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
    
    // Router selon le type
    if (deviceId.find("bt_") == 0) {
        std::string address = deviceId.substr(3);
        return bluetoothManager_->connect(address);
    }
    
    Logger::info("NetworkManager", "Device connection initiated");
    return true;
}

bool NetworkManager::disconnectDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("NetworkManager", "Disconnecting device: " + deviceId);
    
    // Router selon le type
    if (deviceId.find("rtp_") == 0) {
        std::string sessionId = deviceId.substr(4);
        rtpMidiServer_->closeSession(sessionId);
    } else if (deviceId.find("bt_") == 0) {
        std::string address = deviceId.substr(3);
        return bluetoothManager_->disconnect(address);
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
// STATISTIQUES & INFORMATIONS
// ============================================================================

NetworkStatistics NetworkManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    NetworkStatistics stats = stats_;
    
    // Mettre à jour avec les stats des composants
    if (rtpMidiServer_ && rtpMidiServer_->isRunning()) {
        auto rtpStats = rtpMidiServer_->getStatistics();
        stats.rtpDevicesDiscovered = discoveredDevices_.size();
        // stats.rtpDevicesConnected = rtpStats["active_sessions"];
        // stats.rtpBytesReceived = rtpStats["bytes_received"];
        // stats.rtpBytesSent = rtpStats["bytes_sent"];
    }
    
    if (bleMidiDevice_ && bleMidiDevice_->isRunning()) {
        auto bleStats = bleMidiDevice_->getStatistics();
        // stats.bleDevicesConnected = bleStats["connected_clients"];
        // stats.bleBytesReceived = bleStats["bytes_received"];
        // stats.bleBytesSent = bleStats["bytes_sent"];
    }
    
    if (wifiHotspot_ && wifiHotspot_->isRunning()) {
        auto wifiStats = wifiHotspot_->getStatistics();
        // stats.hotspotActive = wifiStats["running"];
        // stats.hotspotClients = wifiStats["connected_clients"];
    }
    
    // Stats WiFi client
    if (wifiManager_->isConnected()) {
        stats.wifiConnected = true;
        stats.wifiSsid = wifiManager_->getConnectedSsid();
        
        auto connStats = wifiManager_->getConnectionStats();
        if (connStats) {
            stats.wifiSignalStrength = connStats->signalStrength;
        }
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
    
    // Interfaces réseau
    info["interfaces"] = json::array();
    struct ifaddrs* ifAddrStruct = nullptr;
    getifaddrs(&ifAddrStruct);
    
    for (struct ifaddrs* ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            json iface;
            iface["name"] = ifa->ifa_name;
            
            void* tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            iface["address"] = addressBuffer;
            
            info["interfaces"].push_back(iface);
        }
    }
    
    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }
    
    // Trafic réseau (lecture depuis /proc/net/dev)
    std::ifstream netDev("/proc/net/dev");
    if (netDev.is_open()) {
        std::string line;
        uint64_t totalRx = 0, totalTx = 0;
        
        while (std::getline(netDev, line)) {
            if (line.find(":") != std::string::npos) {
                std::istringstream iss(line);
                std::string iface;
                uint64_t rx, tx;
                
                iss >> iface >> rx;
                for (int i = 0; i < 7; i++) iss >> tx;
                
                if (iface.find("lo") == std::string::npos) {
                    totalRx += rx;
                    totalTx += tx;
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
        
        if (onDeviceDiscovered_) {
            onDeviceDiscovered_(info);
        }
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
    
    if (onDeviceConnected_) {
        onDeviceConnected_(deviceId);
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
    
    if (onDeviceDisconnected_) {
        onDeviceDisconnected_(deviceId);
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
// FIN DU FICHIER networkmanager.cpp v3.0.0
// ============================================================================
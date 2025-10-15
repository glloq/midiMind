// ============================================================================
// Fichier: src/network/WifiManager.cpp
// Version: 1.0.0
// Date: 2025-10-15
// ============================================================================

#include "WifiManager.h"
#include "../core/Logger.h"
#include <chrono>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <thread>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

WifiManager::WifiManager()
    : interface_("wlan0")
    , autoReconnect_(true)
    , scanning_(false)
    , connected_(false)
    , connecting_(false)
    , running_(false)
    , reconnectAttempts_(0) {
    
    Logger::info("WifiManager", "WifiManager constructed");
}

WifiManager::~WifiManager() {
    Logger::info("WifiManager", "Shutting down WifiManager...");
    
    // Arrêter tous les threads
    running_ = false;
    
    if (scanThread_.joinable()) {
        scanThread_.join();
    }
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    if (connectionThread_.joinable()) {
        connectionThread_.join();
    }
    
    Logger::info("WifiManager", "WifiManager destroyed");
}

// ============================================================================
// SCAN
// ============================================================================

bool WifiManager::startScan() {
    if (scanning_) {
        Logger::warn("WifiManager", "Scan already in progress");
        return false;
    }
    
    Logger::info("WifiManager", "Starting WiFi scan...");
    
    scanning_ = true;
    
    // Lancer le scan dans un thread séparé
    if (scanThread_.joinable()) {
        scanThread_.join();
    }
    
    scanThread_ = std::thread([this]() {
        scanLoop();
    });
    
    return true;
}

bool WifiManager::isScanning() const {
    return scanning_;
}

std::vector<WiFiNetwork> WifiManager::getLastScanResults() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastScanResults_;
}

void WifiManager::scanLoop() {
    Logger::info("WifiManager", "Scan loop started");
    
    try {
        // Déclencher le scan
        std::string scanCmd = "sudo iw dev " + interface_ + " scan 2>&1";
        std::string output = executeCommandWithOutput(scanCmd);
        
        // Parser les résultats
        auto networks = parseIwlistOutput(output);
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastScanResults_ = networks;
        }
        
        Logger::info("WifiManager", "Scan complete: " + std::to_string(networks.size()) + " networks found");
        
        // Callback
        if (onScanComplete_) {
            onScanComplete_(networks);
        }
        
    } catch (const std::exception& e) {
        Logger::error("WifiManager", "Scan error: " + std::string(e.what()));
    }
    
    scanning_ = false;
    Logger::info("WifiManager", "Scan loop stopped");
}

// ============================================================================
// CONNEXION
// ============================================================================

bool WifiManager::connect(const std::string& ssid, 
                          const std::string& password,
                          bool autoReconnect) {
    if (connecting_) {
        Logger::warn("WifiManager", "Connection already in progress");
        return false;
    }
    
    if (ssid.empty()) {
        Logger::error("WifiManager", "SSID cannot be empty");
        return false;
    }
    
    Logger::info("WifiManager", "Connecting to: " + ssid);
    
    autoReconnect_ = autoReconnect;
    pendingConnectSsid_ = ssid;
    pendingConnectPassword_ = password;
    connecting_ = true;
    reconnectAttempts_ = 0;
    
    // Lancer la connexion dans un thread
    if (connectionThread_.joinable()) {
        connectionThread_.join();
    }
    
    connectionThread_ = std::thread([this]() {
        connectionLoop();
    });
    
    return true;
}

bool WifiManager::disconnect() {
    if (!connected_) {
        Logger::info("WifiManager", "Already disconnected");
        return true;
    }
    
    Logger::info("WifiManager", "Disconnecting from: " + connectedSsid_);
    
    // Arrêter wpa_supplicant
    executeCommand("sudo wpa_cli -i " + interface_ + " disconnect");
    executeCommand("sudo killall wpa_supplicant");
    executeCommand("sudo ip link set " + interface_ + " down");
    
    std::string oldSsid = connectedSsid_;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = false;
        connectedSsid_.clear();
        currentStats_ = {};
    }
    
    if (onDisconnection_) {
        onDisconnection_(oldSsid);
    }
    
    Logger::info("WifiManager", "Disconnected");
    return true;
}

bool WifiManager::isConnected() const {
    return connected_;
}

std::string WifiManager::getConnectedSsid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connectedSsid_;
}

void WifiManager::connectionLoop() {
    Logger::info("WifiManager", "Connection loop started");
    
    bool success = false;
    
    try {
        // Arrêter les connexions existantes
        executeCommand("sudo killall wpa_supplicant 2>/dev/null");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Activer l'interface
        executeCommand("sudo ip link set " + interface_ + " up");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Configurer wpa_supplicant
        if (!configureWpaSupplicant(pendingConnectSsid_, pendingConnectPassword_)) {
            Logger::error("WifiManager", "Failed to configure wpa_supplicant");
            goto cleanup;
        }
        
        // Démarrer wpa_supplicant
        std::string wpaCmd = "sudo wpa_supplicant -B -i " + interface_ + 
                            " -c /tmp/wpa_supplicant_midimind.conf 2>&1";
        
        if (!executeCommand(wpaCmd)) {
            Logger::error("WifiManager", "Failed to start wpa_supplicant");
            goto cleanup;
        }
        
        // Attendre la connexion (max 15s)
        for (int i = 0; i < 30; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Vérifier l'état
            std::string status = executeCommandWithOutput("sudo wpa_cli -i " + interface_ + " status | grep wpa_state");
            
            if (status.find("COMPLETED") != std::string::npos) {
                Logger::info("WifiManager", "WPA connection established");
                
                // Obtenir une IP via DHCP
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                executeCommand("sudo dhclient -r " + interface_ + " 2>/dev/null");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                if (executeCommand("sudo dhclient " + interface_)) {
                    success = true;
                    break;
                }
            }
        }
        
        if (success) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                connected_ = true;
                connectedSsid_ = pendingConnectSsid_;
            }
            
            Logger::info("WifiManager", "✓ Connected to: " + pendingConnectSsid_);
            
            // Démarrer le monitoring
            if (!running_) {
                running_ = true;
                if (monitorThread_.joinable()) {
                    monitorThread_.join();
                }
                monitorThread_ = std::thread([this]() {
                    monitorLoop();
                });
            }
        } else {
            Logger::error("WifiManager", "Connection timeout");
        }
        
    } catch (const std::exception& e) {
        Logger::error("WifiManager", "Connection error: " + std::string(e.what()));
        success = false;
    }
    
cleanup:
    connecting_ = false;
    
    if (onConnectionChange_) {
        onConnectionChange_(success, pendingConnectSsid_);
    }
    
    Logger::info("WifiManager", "Connection loop stopped");
}

void WifiManager::monitorLoop() {
    Logger::info("WifiManager", "Monitor loop started");
    
    while (running_ && connected_) {
        try {
            // Récupérer les stats
            std::string iwconfigOutput = executeCommandWithOutput("iwconfig " + interface_ + " 2>&1");
            auto stats = parseIwconfigOutput(iwconfigOutput);
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                currentStats_ = stats;
                
                // Vérifier si toujours connecté
                if (!stats.connected) {
                    Logger::warn("WifiManager", "Connection lost");
                    connected_ = false;
                    
                    if (onDisconnection_) {
                        onDisconnection_(connectedSsid_);
                    }
                    
                    // Auto-reconnexion
                    if (autoReconnect_ && reconnectAttempts_ < MAX_RECONNECT_ATTEMPTS) {
                        reconnectAttempts_++;
                        Logger::info("WifiManager", "Auto-reconnect attempt " + 
                                   std::to_string(reconnectAttempts_));
                        connect(connectedSsid_, pendingConnectPassword_, true);
                    }
                    break;
                }
            }
            
        } catch (const std::exception& e) {
            Logger::error("WifiManager", "Monitor error: " + std::string(e.what()));
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    running_ = false;
    Logger::info("WifiManager", "Monitor loop stopped");
}

// ============================================================================
// STATISTIQUES
// ============================================================================

std::optional<WiFiConnectionStats> WifiManager::getConnectionStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return std::nullopt;
    }
    
    return currentStats_;
}

json WifiManager::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json status;
    status["connected"] = connected_.load();
    status["scanning"] = scanning_.load();
    status["connecting"] = connecting_.load();
    status["interface"] = interface_;
    status["auto_reconnect"] = autoReconnect_;
    
    if (connected_) {
        status["ssid"] = connectedSsid_;
        status["signal_strength"] = currentStats_.signalStrength;
        status["link_speed"] = currentStats_.linkSpeed;
        status["ip_address"] = currentStats_.ipAddress;
    }
    
    return status;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void WifiManager::setInterface(const std::string& interface) {
    std::lock_guard<std::mutex> lock(mutex_);
    interface_ = interface;
    Logger::info("WifiManager", "Interface set to: " + interface);
}

std::string WifiManager::getInterface() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return interface_;
}

void WifiManager::setAutoReconnect(bool enable) {
    autoReconnect_ = enable;
    Logger::info("WifiManager", "Auto-reconnect: " + std::string(enable ? "enabled" : "disabled"));
}

bool WifiManager::isAutoReconnectEnabled() const {
    return autoReconnect_;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void WifiManager::setOnScanComplete(ScanCompleteCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onScanComplete_ = callback;
}

void WifiManager::setOnConnectionChange(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onConnectionChange_ = callback;
}

void WifiManager::setOnDisconnection(DisconnectionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onDisconnection_ = callback;
}

// ============================================================================
// UTILITAIRES
// ============================================================================

bool WifiManager::areDependenciesInstalled() {
    bool hasIw = (system("which iw > /dev/null 2>&1") == 0);
    bool hasWpa = (system("which wpa_supplicant > /dev/null 2>&1") == 0);
    bool hasDhclient = (system("which dhclient > /dev/null 2>&1") == 0);
    
    return hasIw && hasWpa && hasDhclient;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

bool WifiManager::executeCommand(const std::string& command) const {
    Logger::debug("WifiManager", "Executing: " + command);
    int result = system(command.c_str());
    return result == 0;
}

std::string WifiManager::executeCommandWithOutput(const std::string& command) const {
    Logger::debug("WifiManager", "Executing: " + command);
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    std::ostringstream output;
    char buffer[256];
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output << buffer;
    }
    
    pclose(pipe);
    return output.str();
}

std::vector<WiFiNetwork> WifiManager::parseIwlistOutput(const std::string& output) const {
    std::vector<WiFiNetwork> networks;
    
    std::istringstream stream(output);
    std::string line;
    WiFiNetwork current;
    bool inCell = false;
    
    while (std::getline(stream, line)) {
        // Nouveau réseau
        if (line.find("BSS ") == 0 || line.find("Cell ") != std::string::npos) {
            if (inCell && !current.ssid.empty()) {
                networks.push_back(current);
            }
            current = WiFiNetwork{};
            inCell = true;
            
            // Extraire BSSID
            size_t pos = line.find("(on");
            if (pos != std::string::npos) {
                current.bssid = line.substr(4, pos - 5);
            }
        }
        
        // SSID
        if (line.find("SSID: ") != std::string::npos) {
            size_t pos = line.find("SSID: ") + 6;
            current.ssid = line.substr(pos);
            current.ssid.erase(std::remove(current.ssid.begin(), current.ssid.end(), '"'), current.ssid.end());
        }
        
        // Signal
        if (line.find("signal:") != std::string::npos) {
            size_t pos = line.find("signal:") + 7;
            std::string sig = line.substr(pos);
            current.signalStrength = std::stoi(sig);
        }
        
        // Fréquence
        if (line.find("freq:") != std::string::npos) {
            size_t pos = line.find("freq:") + 5;
            current.frequency = std::stoi(line.substr(pos));
            current.channel = (current.frequency - 2412) / 5 + 1;
        }
        
        // Sécurité
        if (line.find("WPA2") != std::string::npos) {
            current.security = "WPA2";
        } else if (line.find("WPA") != std::string::npos) {
            current.security = "WPA";
        } else if (line.find("WEP") != std::string::npos) {
            current.security = "WEP";
        } else if (current.security.empty()) {
            current.security = "Open";
        }
    }
    
    // Ajouter le dernier
    if (inCell && !current.ssid.empty()) {
        networks.push_back(current);
    }
    
    return networks;
}

WiFiConnectionStats WifiManager::parseIwconfigOutput(const std::string& output) const {
    WiFiConnectionStats stats = {};
    
    // Vérifier si connecté
    stats.connected = (output.find("ESSID:") != std::string::npos) &&
                     (output.find("ESSID:off") == std::string::npos);
    
    if (!stats.connected) {
        return stats;
    }
    
    std::istringstream stream(output);
    std::string line;
    
    while (std::getline(stream, line)) {
        // SSID
        if (line.find("ESSID:") != std::string::npos) {
            size_t start = line.find("ESSID:\"") + 7;
            size_t end = line.find("\"", start);
            if (end != std::string::npos) {
                stats.ssid = line.substr(start, end - start);
            }
        }
        
        // Signal
        if (line.find("Signal level=") != std::string::npos) {
            size_t pos = line.find("Signal level=") + 13;
            std::string sig = line.substr(pos);
            stats.signalStrength = std::stoi(sig);
        }
        
        // Link speed
        if (line.find("Bit Rate=") != std::string::npos) {
            size_t pos = line.find("Bit Rate=") + 9;
            std::string speed = line.substr(pos);
            stats.linkSpeed = std::stoi(speed);
        }
    }
    
    // IP address
    std::string ipCmd = "ip addr show " + interface_ + " | grep 'inet ' | awk '{print $2}' | cut -d/ -f1";
    stats.ipAddress = executeCommandWithOutput(ipCmd);
    stats.ipAddress.erase(std::remove(stats.ipAddress.begin(), stats.ipAddress.end(), '\n'), stats.ipAddress.end());
    
    return stats;
}

bool WifiManager::configureWpaSupplicant(const std::string& ssid, const std::string& password) {
    std::ostringstream config;
    
    config << "ctrl_interface=/var/run/wpa_supplicant\n";
    config << "update_config=1\n";
    config << "country=FR\n\n";
    config << "network={\n";
    config << "    ssid=\"" << ssid << "\"\n";
    
    if (!password.empty()) {
        config << "    psk=\"" << password << "\"\n";
    } else {
        config << "    key_mgmt=NONE\n";
    }
    
    config << "    scan_ssid=1\n";
    config << "}\n";
    
    return writeFile("/tmp/wpa_supplicant_midimind.conf", config.str());
}

std::string WifiManager::readFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bool WifiManager::writeFile(const std::string& path, const std::string& content) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        Logger::error("WifiManager", "Cannot write file: " + path);
        return false;
    }
    
    file << content;
    file.close();
    
    Logger::debug("WifiManager", "File written: " + path);
    return true;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER WifiManager.cpp v1.0.0
// ============================================================================
// ============================================================================
// Fichier: src/network/wifi/WiFiHotspot.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "WiFiHotspot.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

WiFiHotspot::WiFiHotspot()
    : running_(false)
    , channel_(6)
    , interface_("wlan0")
    , hostapdPid_(-1)
    , dnsmasqPid_(-1) {
    
    Logger::info("WiFiHotspot", "WiFiHotspot constructed");
}

WiFiHotspot::~WiFiHotspot() {
    stop();
    Logger::info("WiFiHotspot", "WiFiHotspot destroyed");
}

// ============================================================================
// CONTRÔLE
// ============================================================================

bool WiFiHotspot::start(const std::string& ssid,
                       const std::string& password,
                       uint8_t channel,
                       const std::string& ipAddress) {
    if (running_) {
        Logger::warn("WiFiHotspot", "Already running");
        return false;
    }
    
    Logger::info("WiFiHotspot", "═══════════════════════════════════════");
    Logger::info("WiFiHotspot", "  Starting WiFi Hotspot");
    Logger::info("WiFiHotspot", "═══════════════════════════════════════");
    Logger::info("WiFiHotspot", "  SSID: " + ssid);
    Logger::info("WiFiHotspot", "  Channel: " + std::to_string(channel));
    Logger::info("WiFiHotspot", "  IP: " + ipAddress);
    
    // Vérifier les paramètres
    if (ssid.empty()) {
        Logger::error("WiFiHotspot", "SSID cannot be empty");
        return false;
    }
    
    if (password.length() < 8) {
        Logger::error("WiFiHotspot", "Password must be at least 8 characters");
        return false;
    }
    
    if (channel < 1 || channel > 11) {
        Logger::error("WiFiHotspot", "Channel must be between 1 and 11");
        return false;
    }
    
    // Vérifier les dépendances
    if (!areDependenciesInstalled()) {
        Logger::error("WiFiHotspot", "Missing dependencies (hostapd/dnsmasq)");
        Logger::info("WiFiHotspot", "Install with: sudo apt-get install hostapd dnsmasq");
        return false;
    }
    
    // Sauvegarder la configuration
    ssid_ = ssid;
    password_ = password;
    channel_ = channel;
    ipAddress_ = ipAddress;
    
    // Sauvegarder la config réseau actuelle
    backupNetworkConfig();
    
    // Configurer l'interface réseau
    if (!configureInterface()) {
        Logger::error("WiFiHotspot", "Failed to configure network interface");
        return false;
    }
    
    // Configurer hostapd
    if (!configureHostapd()) {
        Logger::error("WiFiHotspot", "Failed to configure hostapd");
        return false;
    }
    
    // Configurer dnsmasq
    if (!configureDnsmasq()) {
        Logger::error("WiFiHotspot", "Failed to configure dnsmasq");
        return false;
    }
    
    // Démarrer hostapd
    if (!startHostapd()) {
        Logger::error("WiFiHotspot", "Failed to start hostapd");
        return false;
    }
    
    // Démarrer dnsmasq
    if (!startDnsmasq()) {
        Logger::error("WiFiHotspot", "Failed to start dnsmasq");
        stopHostapd();
        return false;
    }
    
    running_ = true;
    
    // Démarrer le thread de monitoring
    monitoringThread_ = std::thread([this]() {
        monitoringLoop();
    });
    
    Logger::info("WiFiHotspot", "✓ WiFi Hotspot started");
    Logger::info("WiFiHotspot", "  Connect to SSID: " + ssid_);
    Logger::info("WiFiHotspot", "  Gateway: " + ipAddress_);
    
    return true;
}

void WiFiHotspot::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("WiFiHotspot", "Stopping WiFi Hotspot...");
    
    running_ = false;
    
    // Attendre le thread
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    
    // Arrêter les services
    stopDnsmasq();
    stopHostapd();
    
    // Restaurer la config réseau
    restoreNetworkConfig();
    
    Logger::info("WiFiHotspot", "✓ WiFi Hotspot stopped");
}

bool WiFiHotspot::isRunning() const {
    return running_;
}

// ============================================================================
// GESTION DES CLIENTS
// ============================================================================

std::vector<WiFiClient> WiFiHotspot::listClients() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connectedClients_;
}

std::optional<WiFiClient> WiFiHotspot::getClient(const std::string& macAddress) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(connectedClients_.begin(), connectedClients_.end(),
        [&macAddress](const WiFiClient& client) {
            return client.macAddress == macAddress;
        });
    
    if (it != connectedClients_.end()) {
        return *it;
    }
    
    return std::nullopt;
}

bool WiFiHotspot::disconnectClient(const std::string& macAddress) {
    if (!running_) {
        return false;
    }
    
    Logger::info("WiFiHotspot", "Disconnecting client: " + macAddress);
    
    // Utiliser hostapd_cli pour déconnecter le client
    std::string command = "hostapd_cli disassociate " + macAddress;
    
    if (executeCommand(command)) {
        Logger::info("WiFiHotspot", "✓ Client disconnected");
        return true;
    }
    
    Logger::error("WiFiHotspot", "Failed to disconnect client");
    return false;
}

// ========================================================================
// CALLBACKS
// ========================================================================

void WiFiHotspot::setOnClientConnected(ClientConnectedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onClientConnected_ = callback;
}

void WiFiHotspot::setOnClientDisconnected(ClientDisconnectedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onClientDisconnected_ = callback;
}

// ========================================================================
// CONFIGURATION
// ========================================================================

bool WiFiHotspot::changeSsid(const std::string& newSsid) {
    if (newSsid.empty()) {
        return false;
    }
    
    ssid_ = newSsid;
    Logger::info("WiFiHotspot", "SSID changed to: " + newSsid);
    Logger::info("WiFiHotspot", "Restart hotspot for changes to take effect");
    
    return true;
}

bool WiFiHotspot::changePassword(const std::string& newPassword) {
    if (newPassword.length() < 8) {
        Logger::error("WiFiHotspot", "Password must be at least 8 characters");
        return false;
    }
    
    password_ = newPassword;
    Logger::info("WiFiHotspot", "Password changed");
    Logger::info("WiFiHotspot", "Restart hotspot for changes to take effect");
    
    return true;
}

bool WiFiHotspot::changeChannel(uint8_t channel) {
    if (channel < 1 || channel > 11) {
        Logger::error("WiFiHotspot", "Channel must be between 1 and 11");
        return false;
    }
    
    channel_ = channel;
    Logger::info("WiFiHotspot", "Channel changed to: " + std::to_string(channel));
    Logger::info("WiFiHotspot", "Restart hotspot for changes to take effect");
    
    return true;
}

// ========================================================================
// INFORMATIONS
// ========================================================================

json WiFiHotspot::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["running"] = running_.load();
    stats["ssid"] = ssid_;
    stats["channel"] = channel_;
    stats["ip_address"] = ipAddress_;
    stats["connected_clients"] = connectedClients_.size();
    
    uint64_t totalRx = 0;
    uint64_t totalTx = 0;
    
    for (const auto& client : connectedClients_) {
        totalRx += client.bytesReceived;
        totalTx += client.bytesSent;
    }
    
    stats["bytes_received"] = totalRx;
    stats["bytes_sent"] = totalTx;
    
    return stats;
}

json WiFiHotspot::getConfiguration() const {
    json config;
    config["ssid"] = ssid_;
    config["channel"] = channel_;
    config["ip_address"] = ipAddress_;
    config["interface"] = interface_;
    return config;
}

bool WiFiHotspot::areDependenciesInstalled() {
    // Vérifier hostapd
    bool hasHostapd = (system("which hostapd > /dev/null 2>&1") == 0);
    
    // Vérifier dnsmasq
    bool hasDnsmasq = (system("which dnsmasq > /dev/null 2>&1") == 0);
    
    return hasHostapd && hasDnsmasq;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void WiFiHotspot::monitoringLoop() {
    Logger::info("WiFiHotspot", "Monitoring loop started");
    
    while (running_) {
        // Parser les clients connectés
        auto clients = parseConnectedClients();
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Détecter les nouveaux clients
            for (const auto& client : clients) {
                auto it = std::find_if(connectedClients_.begin(), connectedClients_.end(),
                    [&client](const WiFiClient& c) {
                        return c.macAddress == client.macAddress;
                    });
                
                if (it == connectedClients_.end()) {
                    // Nouveau client
                    connectedClients_.push_back(client);
                    Logger::info("WiFiHotspot", "Client connected: " + client.ipAddress + 
                                " (" + client.macAddress + ")");
                    
                    if (onClientConnected_) {
                        onClientConnected_(client);
                    }
                } else {
                    // Mettre à jour les stats
                    *it = client;
                }
            }
            
            // Détecter les clients déconnectés
            auto it = connectedClients_.begin();
            while (it != connectedClients_.end()) {
                bool found = std::any_of(clients.begin(), clients.end(),
                    [&it](const WiFiClient& c) {
                        return c.macAddress == it->macAddress;
                    });
                
                if (!found) {
                    // Client déconnecté
                    Logger::info("WiFiHotspot", "Client disconnected: " + it->macAddress);
                    
                    if (onClientDisconnected_) {
                        onClientDisconnected_(it->macAddress);
                    }
                    
                    it = connectedClients_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        // Attendre 5 secondes
        for (int i = 0; i < 50 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    Logger::info("WiFiHotspot", "Monitoring loop stopped");
}

bool WiFiHotspot::configureHostapd() {
    Logger::info("WiFiHotspot", "Configuring hostapd...");
    
    std::ostringstream config;
    config << "interface=" << interface_ << "\n";
    config << "driver=nl80211\n";
    config << "ssid=" << ssid_ << "\n";
    config << "hw_mode=g\n";
    config << "channel=" << static_cast<int>(channel_) << "\n";
    config << "wmm_enabled=0\n";
    config << "macaddr_acl=0\n";
    config << "auth_algs=1\n";
    config << "ignore_broadcast_ssid=0\n";
    config << "wpa=2\n";
    config << "wpa_passphrase=" << password_ << "\n";
    config << "wpa_key_mgmt=WPA-PSK\n";
    config << "wpa_pairwise=TKIP\n";
    config << "rsn_pairwise=CCMP\n";
    
    if (!writeFile(HOSTAPD_CONF, config.str())) {
        Logger::error("WiFiHotspot", "Failed to write hostapd config");
        return false;
    }
    
    Logger::info("WiFiHotspot", "✓ hostapd configured");
    return true;
}

bool WiFiHotspot::configureDnsmasq() {
    Logger::info("WiFiHotspot", "Configuring dnsmasq...");
    
    std::ostringstream config;
    config << "interface=" << interface_ << "\n";
    config << "dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h\n";
    config << "domain=local\n";
    config << "address=/midimind.local/" << ipAddress_ << "\n";
    
    if (!writeFile(DNSMASQ_CONF, config.str())) {
        Logger::error("WiFiHotspot", "Failed to write dnsmasq config");
        return false;
    }
    
    Logger::info("WiFiHotspot", "✓ dnsmasq configured");
    return true;
}

bool WiFiHotspot::configureInterface() {
    Logger::info("WiFiHotspot", "Configuring network interface...");
    
    // Configurer l'IP statique
    std::string cmd = "ip addr flush dev " + interface_;
    if (!executeCommand(cmd)) {
        return false;
    }
    
    cmd = "ip addr add " + ipAddress_ + "/24 dev " + interface_;
    if (!executeCommand(cmd)) {
        return false;
    }
    
    cmd = "ip link set " + interface_ + " up";
    if (!executeCommand(cmd)) {
        return false;
    }
    
    Logger::info("WiFiHotspot", "✓ Interface configured");
    return true;
}

bool WiFiHotspot::startHostapd() {
    Logger::info("WiFiHotspot", "Starting hostapd...");
    
    std::string cmd = "hostapd -B " + std::string(HOSTAPD_CONF);
    
    if (executeCommand(cmd)) {
        // Récupérer le PID (simplifié)
        hostapdPid_ = 1; // Dans une vraie implémentation, parser le PID
        Logger::info("WiFiHotspot", "✓ hostapd started");
        return true;
    }
    
    Logger::error("WiFiHotspot", "Failed to start hostapd");
    return false;
}

void WiFiHotspot::stopHostapd() {
    if (hostapdPid_ <= 0) {
        return;
    }
    
    Logger::info("WiFiHotspot", "Stopping hostapd...");
    
    executeCommand("killall hostapd");
    hostapdPid_ = -1;
    
    Logger::info("WiFiHotspot", "✓ hostapd stopped");
}

bool WiFiHotspot::startDnsmasq() {
    Logger::info("WiFiHotspot", "Starting dnsmasq...");
    
    std::string cmd = "dnsmasq -C " + std::string(DNSMASQ_CONF);
    
    if (executeCommand(cmd)) {
        dnsmasqPid_ = 1;
        Logger::info("WiFiHotspot", "✓ dnsmasq started");
        return true;
    }
    
    Logger::error("WiFiHotspot", "Failed to start dnsmasq");
    return false;
}

void WiFiHotspot::stopDnsmasq() {
    if (dnsmasqPid_ <= 0) {
        return;
    }
    
    Logger::info("WiFiHotspot", "Stopping dnsmasq...");
    
    executeCommand("killall dnsmasq");
    dnsmasqPid_ = -1;
    
    Logger::info("WiFiHotspot", "✓ dnsmasq stopped");
}

void WiFiHotspot::backupNetworkConfig() {
    Logger::info("WiFiHotspot", "Backing up network configuration...");
    
    // Dans une vraie implémentation, sauvegarder /etc/network/interfaces
    // ou /etc/dhcpcd.conf
    
    Logger::info("WiFiHotspot", "✓ Network config backed up (stub)");
}

void WiFiHotspot::restoreNetworkConfig() {
    Logger::info("WiFiHotspot", "Restoring network configuration...");
    
    // Restaurer la config originale
    executeCommand("ip addr flush dev " + interface_);
    executeCommand("dhclient " + interface_);
    
    Logger::info("WiFiHotspot", "✓ Network config restored");
}


// ============================================================================
// AMÉLIORATION: parseConnectedClients() - IMPLÉMENTATION RÉELLE
// ============================================================================

std::vector<WiFiClient> WiFiHotspot::parseConnectedClients() const {
    std::vector<WiFiClient> clients;
    
    if (!running_) {
        return clients;
    }
    
    // ========================================================================
    // MÉTHODE 1: Parser /var/lib/misc/dnsmasq.leases pour les IPs DHCP
    // ========================================================================
    std::ifstream leasesFile("/var/lib/misc/dnsmasq.leases");
    if (leasesFile.is_open()) {
        std::string line;
        while (std::getline(leasesFile, line)) {
            // Format: timestamp mac_address ip_address hostname client_id
            // Exemple: 1234567890 aa:bb:cc:dd:ee:ff 192.168.4.10 iPhone *
            std::istringstream iss(line);
            std::string timestamp, mac, ip, hostname;
            
            if (iss >> timestamp >> mac >> ip >> hostname) {
                WiFiClient client;
                client.macAddress = mac;
                client.ipAddress = ip;
                client.hostname = (hostname != "*") ? hostname : "Unknown";
                
                // Convertir timestamp
                try {
                    client.connectedSince = std::stoull(timestamp) * 1000; // Convert to ms
                } catch (...) {
                    client.connectedSince = 0;
                }
                
                clients.push_back(client);
            }
        }
        leasesFile.close();
    }
    
    // ========================================================================
    // MÉTHODE 2: Enrichir avec hostapd_cli all_sta pour signal & stats
    // ========================================================================
    std::string hostapdCmd = "hostapd_cli -i " + interface_ + " all_sta 2>/dev/null";
    FILE* pipe = popen(hostapdCmd.c_str(), "r");
    
    if (pipe) {
        char buffer[256];
        std::string currentMac;
        
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            
            // Supprimer les retours à la ligne
            line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            
            if (line.empty()) continue;
            
            // Détecter une nouvelle station (adresse MAC)
            std::regex macRegex("^([0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2})$");
            std::smatch macMatch;
            if (std::regex_match(line, macMatch, macRegex)) {
                currentMac = macMatch[1];
                continue;
            }
            
            // Parser les propriétés de la station courante
            if (!currentMac.empty()) {
                size_t eqPos = line.find('=');
                if (eqPos != std::string::npos) {
                    std::string key = line.substr(0, eqPos);
                    std::string value = line.substr(eqPos + 1);
                    
                    // Trouver le client correspondant dans notre liste
                    auto it = std::find_if(clients.begin(), clients.end(),
                        [&currentMac](const WiFiClient& c) {
                            return c.macAddress == currentMac;
                        });
                    
                    if (it != clients.end()) {
                        // Extraire les stats
                        if (key == "rx_bytes") {
                            try { it->bytesReceived = std::stoull(value); } catch (...) {}
                        }
                        else if (key == "tx_bytes") {
                            try { it->bytesSent = std::stoull(value); } catch (...) {}
                        }
                        else if (key == "signal") {
                            try { it->signalStrength = std::stoi(value); } catch (...) {}
                        }
                    }
                }
            }
        }
        
        pclose(pipe);
    }
    
    // ========================================================================
    // MÉTHODE 3 (FALLBACK): Si aucune méthode ne fonctionne, scanner ARP
    // ========================================================================
    if (clients.empty()) {
        std::ifstream arpFile("/proc/net/arp");
        if (arpFile.is_open()) {
            std::string line;
            std::getline(arpFile, line); // Skip header
            
            while (std::getline(arpFile, line)) {
                std::istringstream iss(line);
                std::string ip, hwType, flags, mac, mask, device;
                
                if (iss >> ip >> hwType >> flags >> mac >> mask >> device) {
                    // Vérifier que c'est sur notre interface et dans notre subnet
                    if (device == interface_ && ip.find("192.168.4.") == 0) {
                        WiFiClient client;
                        client.ipAddress = ip;
                        client.macAddress = mac;
                        client.hostname = "Unknown";
                        client.connectedSince = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                        ).count();
                        client.bytesReceived = 0;
                        client.bytesSent = 0;
                        client.signalStrength = -50; // Valeur par défaut
                        
                        // Vérifier qu'on ne l'a pas déjà ajouté
                        auto it = std::find_if(clients.begin(), clients.end(),
                            [&mac](const WiFiClient& c) {
                                return c.macAddress == mac;
                            });
                        
                        if (it == clients.end()) {
                            clients.push_back(client);
                        }
                    }
                }
            }
            arpFile.close();
        }
    }
    
    return clients;
}

// ============================================================================
// AMÉLIORATION: Helpers pour lecture/écriture de fichiers système
// ============================================================================

bool WiFiHotspot::executeCommand(const std::string& command) const {
    Logger::debug("WiFiHotspot", "Executing: " + command);
    
    int result = system(command.c_str());
    
    if (result == 0) {
        return true;
    }
    
    Logger::warn("WiFiHotspot", "Command failed with code: " + std::to_string(result));
    return false;
}

std::string WiFiHotspot::readFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::warn("WiFiHotspot", "Cannot read file: " + path);
        return "";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    file.close();
    
    return content.str();
}

bool WiFiHotspot::writeFile(const std::string& path, const std::string& content) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        Logger::error("WiFiHotspot", "Cannot write file: " + path);
        return false;
    }
    
    file << content;
    file.close();
    
    return true;
}


} // namespace midiMind

// ============================================================================
// FIN DU FICHIER WiFiHotspot.cpp
// ============================================================================
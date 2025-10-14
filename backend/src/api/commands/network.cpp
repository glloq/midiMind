// ============================================================================
// Fichier: backend/src/api/commands/network.cpp
// Version: 3.0.1 - NOUVEAU FICHIER
// Date: 2025-10-12
// ============================================================================
// Description:
//   Handlers pour les commandes réseau.
//   Configuration WiFi, hotspot, et informations réseau.
//
// Commandes:
//   - network.status      : État du réseau
//   - network.getInterfaces : Liste des interfaces réseau
//   - network.scanWifi    : Scanner réseaux WiFi disponibles
//   - network.connectWifi : Se connecter à un réseau WiFi
//   - network.startHotspot : Démarrer hotspot WiFi
//   - network.stopHotspot  : Arrêter hotspot
//
// Note: Fonctionnalités basiques implémentées. Pour production complète,
//       intégrer avec NetworkManager ou nmcli.
//
// Auteur: MidiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <array>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// UTILITAIRES RÉSEAU
// ============================================================================

/**
 * @brief Exécute une commande shell et retourne la sortie
 */
static std::string executeCommand(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        Logger::error("NetworkAPI", "Failed to execute command: " + command);
        return "";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    pclose(pipe);
    return result;
}

/**
 * @brief Récupère les interfaces réseau
 */
static json getNetworkInterfaces() {
    json interfaces = json::array();
    
    std::string output = executeCommand("ip -j addr show");
    
    if (output.empty()) {
        // Fallback: parsing manuel
        output = executeCommand("ip addr show");
        // TODO: Parser la sortie manuellement si nécessaire
        return interfaces;
    }
    
    try {
        // Parse la sortie JSON de ip
        auto ipData = json::parse(output);
        
        for (const auto& iface : ipData) {
            if (iface.contains("ifname")) {
                std::string name = iface["ifname"];
                
                // Extraire adresse IP si disponible
                std::string ip = "N/A";
                if (iface.contains("addr_info")) {
                    for (const auto& addr : iface["addr_info"]) {
                        if (addr.contains("local") && addr["family"] == "inet") {
                            ip = addr["local"];
                            break;
                        }
                    }
                }
                
                interfaces.push_back({
                    {"name", name},
                    {"ip", ip},
                    {"state", iface.value("operstate", "unknown")}
                });
            }
        }
    } catch (const json::exception& e) {
        Logger::error("NetworkAPI", "Failed to parse network interfaces");
    }
    
    return interfaces;
}

/**
 * @brief Vérifie si le hotspot est actif
 */
static bool isHotspotActive() {
    std::string output = executeCommand("nmcli con show --active | grep hotspot");
    return !output.empty();
}

// ============================================================================
// FONCTION: registerNetworkCommands()
// Enregistre toutes les commandes réseau (6 commandes)
// ============================================================================
void registerNetworkCommands(CommandFactory& factory) {
    
    Logger::info("NetworkHandlers", "Registering network commands...");
    
    // ========================================================================
    // network.status - État général du réseau
    // ========================================================================
    factory.registerCommand("network.status",
        [](const json& params) -> json {
            Logger::debug("NetworkAPI", "Getting network status...");
            
            try {
                auto interfaces = getNetworkInterfaces();
                bool hotspotActive = isHotspotActive();
                
                return {
                    {"success", true},
                    {"data", {
                        {"interfaces", interfaces},
                        {"hotspot_active", hotspotActive},
                        {"internet_connected", !interfaces.empty()} // Simplifié
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to get network status: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get network status: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // network.getInterfaces - Liste les interfaces réseau
    // ========================================================================
    factory.registerCommand("network.getInterfaces",
        [](const json& params) -> json {
            Logger::debug("NetworkAPI", "Getting network interfaces...");
            
            try {
                auto interfaces = getNetworkInterfaces();
                
                return {
                    {"success", true},
                    {"data", {
                        {"interfaces", interfaces},
                        {"count", interfaces.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to get interfaces: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get interfaces: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // network.scanWifi - Scanner les réseaux WiFi disponibles
    // ========================================================================
    factory.registerCommand("network.scanWifi",
        [](const json& params) -> json {
            Logger::debug("NetworkAPI", "Scanning WiFi networks...");
            
            try {
                // Utiliser nmcli ou iw pour scanner
                std::string output = executeCommand(
                    "nmcli -t -f SSID,SIGNAL,SECURITY dev wifi list");
                
                json networks = json::array();
                std::istringstream stream(output);
                std::string line;
                
                while (std::getline(stream, line)) {
                    if (line.empty()) continue;
                    
                    // Parse format: SSID:SIGNAL:SECURITY
                    size_t pos1 = line.find(':');
                    size_t pos2 = line.find(':', pos1 + 1);
                    
                    if (pos1 != std::string::npos && pos2 != std::string::npos) {
                        std::string ssid = line.substr(0, pos1);
                        std::string signal = line.substr(pos1 + 1, pos2 - pos1 - 1);
                        std::string security = line.substr(pos2 + 1);
                        
                        if (!ssid.empty()) {
                            networks.push_back({
                                {"ssid", ssid},
                                {"signal", signal},
                                {"security", security},
                                {"encrypted", security != "--"}
                            });
                        }
                    }
                }
                
                Logger::info("NetworkAPI", 
                    "Found " + std::to_string(networks.size()) + " WiFi networks");
                
                return {
                    {"success", true},
                    {"data", {
                        {"networks", networks},
                        {"count", networks.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to scan WiFi: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to scan WiFi: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // network.connectWifi - Se connecter à un réseau WiFi
    // ========================================================================
    factory.registerCommand("network.connectWifi",
        [](const json& params) -> json {
            Logger::debug("NetworkAPI", "Connecting to WiFi...");
            
            try {
                // Validation
                if (!params.contains("ssid")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: ssid"}
                    };
                }
                
                std::string ssid = params["ssid"];
                std::string password = params.value("password", "");
                
                // Construire commande nmcli
                std::string command = "nmcli dev wifi connect \"" + ssid + "\"";
                if (!password.empty()) {
                    command += " password \"" + password + "\"";
                }
                
                std::string output = executeCommand(command);
                
                // Vérifier succès
                bool success = (output.find("successfully") != std::string::npos);
                
                if (success) {
                    Logger::info("NetworkAPI", "Connected to WiFi: " + ssid);
                    return {
                        {"success", true},
                        {"message", "Connected to WiFi successfully"},
                        {"ssid", ssid}
                    };
                } else {
                    Logger::error("NetworkAPI", "Failed to connect to WiFi: " + ssid);
                    return {
                        {"success", false},
                        {"error", "Failed to connect to WiFi: " + output}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to connect WiFi: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to connect WiFi: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // network.startHotspot - Démarrer un hotspot WiFi
    // ========================================================================
    factory.registerCommand("network.startHotspot",
        [](const json& params) -> json {
            Logger::debug("NetworkAPI", "Starting WiFi hotspot...");
            
            try {
                // Paramètres par défaut
                std::string ssid = params.value("ssid", "MidiMind");
                std::string password = params.value("password", "midimind123");
                
                // Validation password (min 8 caractères pour WPA2)
                if (password.length() < 8) {
                    return {
                        {"success", false},
                        {"error", "Password must be at least 8 characters"}
                    };
                }
                
                // Créer hotspot avec nmcli
                std::string command = 
                    "nmcli con add type wifi ifname wlan0 con-name hotspot "
                    "autoconnect no ssid \"" + ssid + "\" && "
                    "nmcli con modify hotspot 802-11-wireless.mode ap "
                    "802-11-wireless.band bg ipv4.method shared && "
                    "nmcli con modify hotspot wifi-sec.key-mgmt wpa-psk && "
                    "nmcli con modify hotspot wifi-sec.psk \"" + password + "\" && "
                    "nmcli con up hotspot";
                
                std::string output = executeCommand(command);
                
                bool success = isHotspotActive();
                
                if (success) {
                    Logger::info("NetworkAPI", "Hotspot started: " + ssid);
                    return {
                        {"success", true},
                        {"message", "Hotspot started successfully"},
                        {"data", {
                            {"ssid", ssid},
                            {"ip", "10.42.0.1"} // IP par défaut nmcli
                        }}
                    };
                } else {
                    Logger::error("NetworkAPI", "Failed to start hotspot");
                    return {
                        {"success", false},
                        {"error", "Failed to start hotspot: " + output}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to start hotspot: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to start hotspot: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // network.stopHotspot - Arrêter le hotspot WiFi
    // ========================================================================
    factory.registerCommand("network.stopHotspot",
        [](const json& params) -> json {
            Logger::debug("NetworkAPI", "Stopping WiFi hotspot...");
            
            try {
                std::string output = executeCommand("nmcli con down hotspot");
                
                bool success = !isHotspotActive();
                
                if (success) {
                    Logger::info("NetworkAPI", "Hotspot stopped");
                    return {
                        {"success", true},
                        {"message", "Hotspot stopped successfully"}
                    };
                } else {
                    Logger::error("NetworkAPI", "Failed to stop hotspot");
                    return {
                        {"success", false},
                        {"error", "Failed to stop hotspot"}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to stop hotspot: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to stop hotspot: " + std::string(e.what())}
                };
            }
        }
    );
    
    Logger::info("NetworkHandlers", "✓ Registered 6 network commands");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER network.cpp
// ============================================================================

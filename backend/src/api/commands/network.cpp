// ============================================================================
// Fichier: /home/pi/midiMind/backend/src/api/commands/network.cpp
// Version: 3.0.5
// Date: 2025-10-16
// ============================================================================
// Description:
//   Handlers pour les commandes de gestion réseau
//
// CORRECTIONS v3.0.5:
//   ✅ Correction appels registerCommand (2 paramètres)
//
// Commandes implémentées:
//   - network.getStatus  : État de la connexion réseau
//   - network.scan       : Scanner les réseaux Wi-Fi
//   - network.connect    : Connecter à un réseau
//   - network.disconnect : Déconnecter
//   - network.getConfig  : Configuration réseau
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../network/NetworkManager.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FONCTION: registerNetworkCommands()
// ============================================================================

void registerNetworkCommands(
    CommandFactory& factory,
    std::shared_ptr<NetworkManager> networkManager
) {
    if (!networkManager) {
        Logger::error("NetworkCommands", 
            "Cannot register commands: NetworkManager is null");
        return;
    }
    
    Logger::info("NetworkHandlers", "Registering network commands...");

    // ========================================================================
    // network.getStatus - État de la connexion
    // ========================================================================
    
    factory.registerCommand("network.getStatus",
        [networkManager](const json& params) -> json {
            Logger::debug("NetworkAPI", "Getting network status...");
            
            try {
                auto info = networkManager->getNetworkInfo();
                
                return {
                    {"success", true},
                    {"data", info}
                };
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to get status: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "STATUS_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // network.scan - Scanner les réseaux Wi-Fi
    // ========================================================================
    
    factory.registerCommand("network.scan",
        [networkManager](const json& params) -> json {
            Logger::debug("NetworkAPI", "Scanning networks...");
            
            try {
                networkManager->startWifiScan();
                
                // Attendre un peu pour les résultats
                std::this_thread::sleep_for(std::chrono::seconds(2));
                
                auto networks = networkManager->getWifiNetworks();
                json networksJson = json::array();
                
                for (const auto& network : networks) {
                    networksJson.push_back({
                        {"ssid", network.ssid},
                        {"signal", network.signal},
                        {"security", network.security}
                    });
                }
                
                Logger::info("NetworkAPI", 
                    "Found " + std::to_string(networks.size()) + " networks");
                
                return {
                    {"success", true},
                    {"data", {
                        {"networks", networksJson},
                        {"count", networks.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to scan: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "SCAN_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // network.connect - Connecter à un réseau
    // ========================================================================
    
    factory.registerCommand("network.connect",
        [networkManager](const json& params) -> json {
            Logger::debug("NetworkAPI", "Connecting to network...");
            
            try {
                if (!params.contains("ssid") || !params.contains("password")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters: ssid, password"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string ssid = params["ssid"];
                std::string password = params["password"];
                
                bool success = networkManager->connectWifi(ssid, password);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to connect to network"},
                        {"error_code", "CONNECT_FAILED"}
                    };
                }
                
                Logger::info("NetworkAPI", "✓ Connected to: " + ssid);
                
                return {
                    {"success", true},
                    {"data", {
                        {"ssid", ssid},
                        {"connected", true}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to connect: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // network.disconnect - Déconnecter
    // ========================================================================
    
    factory.registerCommand("network.disconnect",
        [networkManager](const json& params) -> json {
            Logger::debug("NetworkAPI", "Disconnecting from network...");
            
            try {
                bool success = networkManager->disconnectWifi();
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to disconnect"},
                        {"error_code", "DISCONNECT_FAILED"}
                    };
                }
                
                Logger::info("NetworkAPI", "✓ Disconnected");
                
                return {
                    {"success", true},
                    {"message", "Disconnected successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to disconnect: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // network.getConfig - Configuration réseau
    // ========================================================================
    
    factory.registerCommand("network.getConfig",
        [networkManager](const json& params) -> json {
            Logger::debug("NetworkAPI", "Getting network configuration...");
            
            try {
                auto info = networkManager->getNetworkInfo();
                
                return {
                    {"success", true},
                    {"data", info}
                };
                
            } catch (const std::exception& e) {
                Logger::error("NetworkAPI", 
                    "Failed to get config: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "CONFIG_FAILED"}
                };
            }
        }
    );
    
    Logger::info("NetworkHandlers", "✓ Network commands registered");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER network.cpp v3.0.5
// ============================================================================

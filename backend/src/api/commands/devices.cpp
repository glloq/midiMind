// ============================================================================
// Fichier: backend/src/api/commands/devices.cpp
// Version: 3.0.1-corrections
// Date: 2025-10-15
// ============================================================================
// Description:
//   Handlers pour les commandes de gestion des périphériques MIDI
//   VERSION LAMBDA DIRECTE (json -> json)
//
// CORRECTIONS v3.0.1:
//   ✅ Ajout error_code pour toutes les erreurs
//   ✅ Format de retour harmonisé avec enveloppe "data"
//   ✅ Logging amélioré
//   ✅ Validation des paramètres renforcée
//
// Commandes implémentées (5 commandes):
//   - devices.scan         : Scanner les périphériques disponibles
//   - devices.list         : Lister tous les périphériques
//   - devices.connect      : Connecter un périphérique
//   - devices.disconnect   : Déconnecter un périphérique
//   - devices.info         : Informations sur un périphérique
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/devices/MidiDeviceManager.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FONCTION: registerDeviceCommands()
// Enregistre toutes les commandes de gestion des périphériques (5 commandes)
// ============================================================================

void registerDeviceCommands(CommandFactory& factory, 
                           std::shared_ptr<MidiDeviceManager> deviceManager) {
    
    Logger::info("DeviceHandlers", "Registering device commands...");
    
    // ========================================================================
    // devices.scan - Scanner les périphériques disponibles
    // ========================================================================
    
    factory.registerCommand("devices.scan",
        [deviceManager](const json& params) -> json {
            Logger::debug("DeviceAPI", "Scanning devices...");
            
            try {
                // Lancer le scan
                deviceManager->scanDevices();
                
                // Récupérer les périphériques découverts
                auto devices = deviceManager->getDevices();
                
                json devicesJson = json::array();
                for (const auto& dev : devices) {
                    devicesJson.push_back({
                        {"id", dev.id},
                        {"name", dev.name},
                        {"type", dev.type},
                        {"manufacturer", dev.manufacturer},
                        {"connected", dev.connected}
                    });
                }
                
                Logger::info("DeviceAPI", 
                    "✓ Scan completed, found " + std::to_string(devices.size()) + " device(s)");
                
                return {
                    {"success", true},
                    {"message", "Scan completed successfully"},
                    {"data", {
                        {"devices", devicesJson},
                        {"count", devices.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to scan devices: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to scan devices: " + std::string(e.what())},
                    {"error_code", "SCAN_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // devices.list - Lister tous les périphériques
    // ========================================================================
    
    factory.registerCommand("devices.list",
        [deviceManager](const json& params) -> json {
            Logger::debug("DeviceAPI", "Listing devices...");
            
            try {
                auto devices = deviceManager->getDevices();
                
                json devicesJson = json::array();
                for (const auto& dev : devices) {
                    devicesJson.push_back({
                        {"id", dev.id},
                        {"name", dev.name},
                        {"type", dev.type},
                        {"connected", dev.connected},
                        {"manufacturer", dev.manufacturer},
                        {"port", dev.port}
                    });
                }
                
                Logger::debug("DeviceAPI", 
                    "Listed " + std::to_string(devices.size()) + " device(s)");
                
                return {
                    {"success", true},
                    {"data", {
                        {"devices", devicesJson},
                        {"count", devices.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to list devices: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to list devices: " + std::string(e.what())},
                    {"error_code", "LIST_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // devices.connect - Connecter un périphérique
    // ========================================================================
    
    factory.registerCommand("devices.connect",
        [deviceManager](const json& params) -> json {
            Logger::debug("DeviceAPI", "Connecting device...");
            
            try {
                // Validation
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                // Vérifier si déjà connecté
                if (deviceManager->isConnected(deviceId)) {
                    Logger::debug("DeviceAPI", "Device already connected: " + deviceId);
                    return {
                        {"success", true},
                        {"message", "Device already connected"},
                        {"data", {
                            {"device_id", deviceId},
                            {"status", "already_connected"}
                        }}
                    };
                }
                
                // Connecter
                bool connected = deviceManager->connectDevice(deviceId);
                
                if (!connected) {
                    Logger::error("DeviceAPI", "Failed to connect device: " + deviceId);
                    return {
                        {"success", false},
                        {"error", "Failed to connect device"},
                        {"error_code", "CONNECTION_FAILED"},
                        {"data", {
                            {"device_id", deviceId}
                        }}
                    };
                }
                
                // Récupérer les infos du device
                auto info = deviceManager->getDeviceInfo(deviceId);
                
                Logger::info("DeviceAPI", "✓ Device connected: " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "Device connected successfully"},
                    {"data", {
                        {"device_id", deviceId},
                        {"device_name", info.name},
                        {"manufacturer", info.manufacturer},
                        {"type", info.type}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to connect: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to connect device: " + std::string(e.what())},
                    {"error_code", "CONNECTION_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // devices.disconnect - Déconnecter un périphérique
    // ========================================================================
    
    factory.registerCommand("devices.disconnect",
        [deviceManager](const json& params) -> json {
            Logger::debug("DeviceAPI", "Disconnecting device...");
            
            try {
                // Validation
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                // Vérifier si connecté
                if (!deviceManager->isConnected(deviceId)) {
                    Logger::debug("DeviceAPI", "Device not connected: " + deviceId);
                    return {
                        {"success", true},
                        {"message", "Device not connected"},
                        {"data", {
                            {"device_id", deviceId},
                            {"status", "not_connected"}
                        }}
                    };
                }
                
                // Déconnecter
                bool disconnected = deviceManager->disconnectDevice(deviceId);
                
                if (!disconnected) {
                    Logger::error("DeviceAPI", "Failed to disconnect device: " + deviceId);
                    return {
                        {"success", false},
                        {"error", "Failed to disconnect device"},
                        {"error_code", "DISCONNECTION_FAILED"},
                        {"data", {
                            {"device_id", deviceId}
                        }}
                    };
                }
                
                Logger::info("DeviceAPI", "✓ Device disconnected: " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "Device disconnected successfully"},
                    {"data", {
                        {"device_id", deviceId}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to disconnect: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to disconnect device: " + std::string(e.what())},
                    {"error_code", "DISCONNECTION_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // devices.info - Informations sur un périphérique
    // ========================================================================
    
    factory.registerCommand("devices.info",
        [deviceManager](const json& params) -> json {
            Logger::debug("DeviceAPI", "Getting device info...");
            
            try {
                // Validation
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                // Vérifier que le device existe
                auto device = deviceManager->getDevice(deviceId);
                if (!device) {
                    Logger::warn("DeviceAPI", "Device not found: " + deviceId);
                    return {
                        {"success", false},
                        {"error", "Device not found"},
                        {"error_code", "DEVICE_NOT_FOUND"},
                        {"data", {
                            {"device_id", deviceId}
                        }}
                    };
                }
                
                // Récupérer infos complètes
                auto info = deviceManager->getDeviceInfo(deviceId);
                
                json deviceInfo = {
                    {"id", info.id},
                    {"name", info.name},
                    {"type", info.type},
                    {"manufacturer", info.manufacturer},
                    {"port", info.port},
                    {"connected", info.connected},
                    {"capabilities", {
                        {"input", info.hasInput},
                        {"output", info.hasOutput}
                    }}
                };
                
                Logger::debug("DeviceAPI", "✓ Device info retrieved: " + deviceId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"device", deviceInfo}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to get device info: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get device info: " + std::string(e.what())},
                    {"error_code", "INFO_FAILED"}
                };
            }
        }
    );
    
    Logger::info("DeviceHandlers", "✅ Device commands registered (5 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER devices.cpp v3.0.1-corrections
// ============================================================================

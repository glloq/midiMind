// ============================================================================
// Fichier: backend/src/api/devices.cpp
// Version: 3.0.0
// ============================================================================
// Description:
//   Handlers pour les commandes de gestion des périphériques MIDI
//   VERSION LAMBDA DIRECTE (json -> json)
//
// Commandes implémentées:
//   - devices.scan         : Scanner les périphériques disponibles
//   - devices.list         : Lister tous les périphériques
//   - devices.connect      : Connecter un périphérique
//   - devices.disconnect   : Déconnecter un périphérique
//   - devices.info         : Informations sur un périphérique
//
// Auteur: midiMind Team
// Date: 2025-10-09
// ============================================================================

#include "../core/commands/CommandFactory.h"
#include "../midi/devices/MidiDeviceManager.h"
#include "../core/Logger.h"

namespace midiMind {

// ============================================================================
// FONCTION: registerDeviceCommands()
// Enregistre toutes les commandes de gestion des périphériques
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
                
                // Récupérer la liste mise à jour
                auto devices = deviceManager->getAvailableDevices();
                
                Logger::info("DeviceAPI", 
                    "Scan complete: " + std::to_string(devices.size()) + " devices found");
                
                // Convertir en JSON
                json devicesJson = json::array();
                for (const auto& dev : devices) {
                    devicesJson.push_back({
                        {"id", dev.id},
                        {"name", dev.name},
                        {"type", dev.type},
                        {"connected", dev.connected}
                    });
                }
                
                return {
                    {"success", true},
                    {"message", "Scan completed"},
                    {"count", devices.size()},
                    {"devices", devicesJson}
                };
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to scan: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to scan devices: " + std::string(e.what())}
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
                auto devices = deviceManager->getAvailableDevices();
                
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
                
                return {
                    {"success", true},
                    {"count", devices.size()},
                    {"devices", devicesJson}
                };
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to list devices: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to list devices: " + std::string(e.what())}
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
                        {"error", "Missing required parameter: device_id"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                // Vérifier si déjà connecté
                if (deviceManager->isConnected(deviceId)) {
                    return {
                        {"success", true},
                        {"message", "Device already connected"},
                        {"device_id", deviceId}
                    };
                }
                
                // Connecter
                bool connected = deviceManager->connectDevice(deviceId);
                
                if (connected) {
                    Logger::info("DeviceAPI", "Device connected: " + deviceId);
                    
                    // Récupérer les infos du device
                    auto info = deviceManager->getDeviceInfo(deviceId);
                    
                    return {
                        {"success", true},
                        {"message", "Device connected successfully"},
                        {"device_id", deviceId},
                        {"device_name", info.name}
                    };
                } else {
                    return {
                        {"success", false},
                        {"error", "Failed to connect device"},
                        {"device_id", deviceId}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to connect: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to connect device: " + std::string(e.what())}
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
                        {"error", "Missing required parameter: device_id"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                // Vérifier si connecté
                if (!deviceManager->isConnected(deviceId)) {
                    return {
                        {"success", true},
                        {"message", "Device already disconnected"},
                        {"device_id", deviceId}
                    };
                }
                
                // Déconnecter
                bool disconnected = deviceManager->disconnect(deviceId);
                
                if (disconnected) {
                    Logger::info("DeviceAPI", "Device disconnected: " + deviceId);
                    
                    return {
                        {"success", true},
                        {"message", "Device disconnected successfully"},
                        {"device_id", deviceId}
                    };
                } else {
                    return {
                        {"success", false},
                        {"error", "Failed to disconnect device"},
                        {"device_id", deviceId}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to disconnect: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to disconnect device: " + std::string(e.what())}
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
                        {"error", "Missing required parameter: device_id"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                // Récupérer les infos
                auto info = deviceManager->getDeviceInfo(deviceId);
                
                if (info.id.empty()) {
                    return {
                        {"success", false},
                        {"error", "Device not found"},
                        {"device_id", deviceId}
                    };
                }
                
                return {
                    {"success", true},
                    {"device", {
                        {"id", info.id},
                        {"name", info.name},
                        {"type", info.type},
                        {"manufacturer", info.manufacturer},
                        {"port", info.port},
                        {"connected", info.connected}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("DeviceAPI", 
                    "Failed to get device info: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get device info: " + std::string(e.what())}
                };
            }
        }
    );
    
    Logger::info("DeviceHandlers", 
        "✓ Device commands registered (5 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER devices.cpp
// ============================================================================

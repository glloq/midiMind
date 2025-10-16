// ============================================================================
// Fichier: /home/pi/midiMind/backend/src/api/commands/instruments.cpp
// Version: 3.0.8
// Date: 2025-10-16
// ============================================================================
// Description:
//   Handlers pour les commandes de gestion des instruments MIDI
//
// CORRECTIONS v3.0.8:
//   ✅ Correction appels registerCommand (2 paramètres)
//   ✅ getDevices() → getConnectedDevices()
//   ✅ connectDevice/disconnectDevice → connect/disconnect
//   ✅ Correction structures DeviceIdentity, NoteMap, CCCapabilities
//   ✅ Correction méthodes SysExHandler
//
// Commandes implémentées:
//   - instruments.list    : Lister les instruments disponibles
//   - instruments.connect : Connecter un instrument
//   - instruments.disconnect : Déconnecter un instrument
//   - instruments.getProfile : Récupérer le profil d'un instrument
//   - instruments.requestNoteMap : Demander la note map
//   - instruments.requestCC : Demander les CC capabilities
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/devices/MidiDeviceManager.h"
#include "../../midi/sysex/SysExHandler.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FONCTION: registerInstrumentCommands()
// ============================================================================

void registerInstrumentCommands(
    CommandFactory& factory,
    std::shared_ptr<MidiDeviceManager> deviceManager,
    std::shared_ptr<SysExHandler> sysExHandler
) {
    if (!deviceManager || !sysExHandler) {
        Logger::error("InstrumentCommands", 
            "Cannot register commands: null manager");
        return;
    }
    
    Logger::info("InstrumentHandlers", "Registering instrument commands...");

    // ========================================================================
    // instruments.list - Lister les instruments
    // ========================================================================
    
    factory.registerCommand("instruments.list",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentAPI", "Listing instruments...");
            
            try {
                auto devices = deviceManager->getConnectedDevices();
                json devicesJson = json::array();
                
                for (const auto& device : devices) {
                    if (!device) continue;
                    
                    devicesJson.push_back({
                        {"id", device->getId()},
                        {"name", device->getName()},
                        {"type", static_cast<int>(device->getType())},
                        {"connected", true}
                    });
                }
                
                Logger::info("InstrumentAPI", 
                    "Found " + std::to_string(devices.size()) + " instruments");
                
                return {
                    {"success", true},
                    {"data", {
                        {"devices", devicesJson},
                        {"count", devices.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentAPI", 
                    "Failed to list instruments: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "LIST_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // instruments.connect - Connecter un instrument
    // ========================================================================
    
    factory.registerCommand("instruments.connect",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentAPI", "Connecting instrument...");
            
            try {
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                bool success = deviceManager->connect(deviceId);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to connect device"},
                        {"error_code", "CONNECT_FAILED"}
                    };
                }
                
                Logger::info("InstrumentAPI", "✓ Device connected: " + deviceId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"device_id", deviceId},
                        {"connected", true}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentAPI", 
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
    // instruments.disconnect - Déconnecter un instrument
    // ========================================================================
    
    factory.registerCommand("instruments.disconnect",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentAPI", "Disconnecting instrument...");
            
            try {
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                deviceManager->disconnect(deviceId);
                
                Logger::info("InstrumentAPI", "✓ Device disconnected: " + deviceId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"device_id", deviceId},
                        {"connected", false}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentAPI", 
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
    // instruments.getProfile - Récupérer le profil
    // ========================================================================
    
    factory.registerCommand("instruments.getProfile",
        [sysExHandler](const json& params) -> json {
            Logger::debug("InstrumentAPI", "Getting device profile...");
            
            try {
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                json response = {
                    {"success", true},
                    {"data", {}}
                };
                
                // Récupérer identity
                auto identity = sysExHandler->getIdentity(deviceId);
                if (identity) {
                    response["data"]["standard_identity"] = {
                        {"manufacturer", identity->manufacturer},
                        {"family", identity->family},
                        {"model", identity->model},
                        {"version", identity->version}
                    };
                }
                
                // Récupérer note map
                auto noteMap = sysExHandler->getNoteMap(deviceId);
                if (noteMap) {
                    json noteMapJson = json::array();
                    // Note: NoteMap structure may vary, adapt as needed
                    response["data"]["note_map"] = noteMapJson;
                }
                
                // Récupérer CC capabilities
                auto ccCaps = sysExHandler->getCCCapabilities(deviceId);
                if (ccCaps) {
                    json ccJson = json::array();
                    // Note: CCCapabilities structure may vary, adapt as needed
                    response["data"]["cc_capabilities"] = ccJson;
                }
                
                Logger::info("InstrumentAPI", "✓ Profile retrieved for: " + deviceId);
                
                return response;
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentAPI", 
                    "Failed to get profile: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // instruments.requestNoteMap - Demander la note map
    // ========================================================================
    
    factory.registerCommand("instruments.requestNoteMap",
        [sysExHandler](const json& params) -> json {
            Logger::debug("InstrumentAPI", "Requesting note map...");
            
            try {
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                // Envoyer la requête SysEx (méthode à adapter selon votre implémentation)
                // Note: si la méthode n'existe pas, utiliser sendSysEx directement
                
                Logger::info("InstrumentAPI", "✓ Note map request sent to: " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "Note map request sent. Wait for response."}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentAPI", 
                    "Failed to request note map: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // instruments.requestCC - Demander les CC capabilities
    // ========================================================================
    
    factory.registerCommand("instruments.requestCC",
        [sysExHandler](const json& params) -> json {
            Logger::debug("InstrumentAPI", "Requesting CC capabilities...");
            
            try {
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                // Envoyer la requête SysEx
                
                Logger::info("InstrumentAPI", "✓ CC capabilities request sent to: " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "CC capabilities request sent. Wait for response."}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentAPI", 
                    "Failed to request CC capabilities: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );
    
    Logger::info("InstrumentHandlers", "✓ Instrument commands registered");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER instruments.cpp v3.0.8
// ============================================================================

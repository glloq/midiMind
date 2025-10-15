// ============================================================================
// Fichier: backend/src/api/commands/instruments.cpp
// Version: 3.0.1-corrections
// Date: 2025-10-15
// ============================================================================
// Description:
//   Handlers pour les commandes de gestion des instruments MIDI
//   avec profils avancés et capacités SysEx
//
// CORRECTIONS v3.0.1:
//   ✅ Ajout error_code pour toutes les erreurs
//   ✅ Format de retour harmonisé avec enveloppe "data"
//   ✅ Validation des paramètres renforcée
//   ✅ Logging amélioré
//   ✅ Gestion des profils instruments
//
// Commandes implémentées (8 commandes):
//   - instruments.scan              : Scanner les instruments disponibles
//   - instruments.list              : Lister tous les instruments
//   - instruments.connect           : Connecter un instrument
//   - instruments.disconnect        : Déconnecter un instrument
//   - instruments.getProfile        : Obtenir le profil complet d'un instrument
//   - instruments.requestIdentity   : Demander l'identité via SysEx
//   - instruments.requestNoteMap    : Demander la note map via SysEx
//   - instruments.requestCC         : Demander les capacités CC via SysEx
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
// Enregistre toutes les commandes de gestion des instruments (8 commandes)
// ============================================================================

void registerInstrumentCommands(
    CommandFactory& factory,
    std::shared_ptr<MidiDeviceManager> deviceManager,
    std::shared_ptr<SysExHandler> sysExHandler
) {
    
    Logger::info("InstrumentHandlers", "Registering instrument commands...");
    
    // ========================================================================
    // instruments.scan - Scanner les instruments disponibles
    // ========================================================================
    
    factory.registerCommand("instruments.scan",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Scanning for instruments...");
            
            try {
                // Lancer une découverte complète
                auto foundDevices = deviceManager->discoverDevices(true);
                
                json deviceList = json::array();
                for (const auto& info : foundDevices) {
                    json deviceInfo;
                    deviceInfo["id"] = info.id;
                    deviceInfo["name"] = info.name;
                    deviceInfo["type"] = static_cast<int>(info.type);
                    deviceInfo["manufacturer"] = info.manufacturer;
                    deviceInfo["connected"] = info.connected;
                    deviceInfo["port"] = info.port;
                    
                    deviceList.push_back(deviceInfo);
                }
                
                Logger::info("InstrumentsAPI", 
                    "✓ Scan completed, found " + std::to_string(foundDevices.size()) + " device(s)");
                
                return {
                    {"success", true},
                    {"message", "Device scan completed"},
                    {"data", {
                        {"devices", deviceList},
                        {"count", foundDevices.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", "Scan failed: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Scan failed: " + std::string(e.what())},
                    {"error_code", "SCAN_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // instruments.list - Lister tous les instruments
    // ========================================================================
    
    factory.registerCommand("instruments.list",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Listing instruments...");
            
            try {
                auto devices = deviceManager->getDevices();
                
                json deviceList = json::array();
                for (const auto& dev : devices) {
                    json deviceInfo;
                    deviceInfo["id"] = dev.id;
                    deviceInfo["name"] = dev.name;
                    deviceInfo["type"] = dev.type;
                    deviceInfo["manufacturer"] = dev.manufacturer;
                    deviceInfo["connected"] = dev.connected;
                    deviceInfo["port"] = dev.port;
                    
                    deviceList.push_back(deviceInfo);
                }
                
                Logger::debug("InstrumentsAPI", 
                    "Listed " + std::to_string(deviceList.size()) + " instrument(s)");
                
                return {
                    {"success", true},
                    {"data", {
                        {"devices", deviceList},
                        {"count", deviceList.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", 
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
    // instruments.connect - Connecter un instrument
    // ========================================================================
    
    factory.registerCommand("instruments.connect",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Connecting to device...");
            
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
                    Logger::debug("InstrumentsAPI", "Device already connected: " + deviceId);
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
                bool success = deviceManager->connectDevice(deviceId);
                
                if (!success) {
                    Logger::error("InstrumentsAPI", 
                        "Failed to connect to device: " + deviceId);
                    return {
                        {"success", false},
                        {"error", "Failed to connect to device: " + deviceId},
                        {"error_code", "CONNECTION_FAILED"}
                    };
                }
                
                Logger::info("InstrumentsAPI", "✓ Connected to device: " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "Device connected successfully"},
                    {"data", {
                        {"device_id", deviceId}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", 
                    "Connection failed: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Connection failed: " + std::string(e.what())},
                    {"error_code", "CONNECTION_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // instruments.disconnect - Déconnecter un instrument
    // ========================================================================
    
    factory.registerCommand("instruments.disconnect",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Disconnecting device...");
            
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
                    Logger::debug("InstrumentsAPI", "Device not connected: " + deviceId);
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
                bool success = deviceManager->disconnectDevice(deviceId);
                
                if (!success) {
                    Logger::error("InstrumentsAPI", 
                        "Failed to disconnect device: " + deviceId);
                    return {
                        {"success", false},
                        {"error", "Failed to disconnect device: " + deviceId},
                        {"error_code", "DISCONNECTION_FAILED"}
                    };
                }
                
                Logger::info("InstrumentsAPI", "✓ Disconnected device: " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "Device disconnected successfully"},
                    {"data", {
                        {"device_id", deviceId}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", 
                    "Disconnection failed: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Disconnection failed: " + std::string(e.what())},
                    {"error_code", "DISCONNECTION_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // instruments.getProfile - Obtenir le profil complet d'un instrument
    // ========================================================================
    
    factory.registerCommand("instruments.getProfile",
        [deviceManager, sysExHandler](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Getting device profile...");
            
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
                    Logger::warn("InstrumentsAPI", "Device not found: " + deviceId);
                    return {
                        {"success", false},
                        {"error", "Device not found"},
                        {"error_code", "DEVICE_NOT_FOUND"},
                        {"data", {
                            {"device_id", deviceId}
                        }}
                    };
                }
                
                json response;
                response["success"] = true;
                response["data"]["device"]["id"] = deviceId;
                
                // Vérifier si connecté
                bool isConnected = device->isConnected();
                response["data"]["device"]["connected"] = isConnected;
                
                if (!isConnected) {
                    response["data"]["device"]["status"] = "disconnected";
                    response["data"]["note"] = "Device must be connected to retrieve full profile";
                    return response;
                }
                
                response["data"]["device"]["status"] = "connected";
                
                // Standard Identity (Universal SysEx)
                if (sysExHandler) {
                    auto stdIdentity = sysExHandler->getDeviceIdentity(deviceId);
                    if (stdIdentity) {
                        response["data"]["standard_identity"]["manufacturer_id"] = stdIdentity->manufacturerId;
                        response["data"]["standard_identity"]["family_code"] = stdIdentity->familyCode;
                        response["data"]["standard_identity"]["model_number"] = stdIdentity->modelNumber;
                        response["data"]["standard_identity"]["version"]["major"] = stdIdentity->versionMajor;
                        response["data"]["standard_identity"]["version"]["minor"] = stdIdentity->versionMinor;
                        response["data"]["standard_identity"]["version"]["patch"] = stdIdentity->versionPatch;
                    } else {
                        response["data"]["standard_identity"] = nullptr;
                    }
                    
                    // Custom Identity (Application-specific)
                    auto identity = sysExHandler->getCustomIdentity(deviceId);
                    if (identity) {
                        response["data"]["identity"]["unique_id"] = identity->uniqueId;
                        response["data"]["identity"]["name"] = identity->name;
                        response["data"]["identity"]["type"] = identity->type;
                        response["data"]["identity"]["firmware"]["major"] = identity->firmwareVersion[0];
                        response["data"]["identity"]["firmware"]["minor"] = identity->firmwareVersion[1];
                        response["data"]["identity"]["firmware"]["patch"] = identity->firmwareVersion[2];
                        response["data"]["identity"]["firmware"]["build"] = identity->firmwareVersion[3];
                        
                        response["data"]["playable"]["range"]["first"] = identity->firstNote;
                        response["data"]["playable"]["range"]["count"] = identity->noteCount;
                        response["data"]["playable"]["range"]["last"] = identity->firstNote + identity->noteCount - 1;
                        response["data"]["playable"]["polyphony"]["max"] = identity->maxPolyphony;
                        
                        response["data"]["performance"]["latency_ms"] = identity->responseDelay;
                        response["data"]["programs"]["count"] = identity->programCount;
                    } else {
                        response["data"]["identity"] = nullptr;
                    }
                    
                    // Note Map
                    auto noteMap = sysExHandler->getNoteMap(deviceId);
                    if (noteMap) {
                        json noteMapJson = json::array();
                        for (const auto& mapping : noteMap->mappings) {
                            noteMapJson.push_back({
                                {"midi_note", mapping.midiNote},
                                {"physical_note", mapping.physicalNote},
                                {"name", mapping.name}
                            });
                        }
                        response["data"]["note_map"] = noteMapJson;
                    } else {
                        response["data"]["note_map"] = nullptr;
                    }
                    
                    // CC Capabilities
                    auto ccCaps = sysExHandler->getCCCapabilities(deviceId);
                    if (ccCaps) {
                        json ccJson = json::array();
                        for (const auto& cc : ccCaps->controllers) {
                            ccJson.push_back({
                                {"cc_number", cc.ccNumber},
                                {"name", cc.name},
                                {"min", cc.minValue},
                                {"max", cc.maxValue},
                                {"default", cc.defaultValue}
                            });
                        }
                        response["data"]["cc_capabilities"] = ccJson;
                    } else {
                        response["data"]["cc_capabilities"] = nullptr;
                    }
                }
                
                Logger::debug("InstrumentsAPI", "✓ Profile retrieved for: " + deviceId);
                
                return response;
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", 
                    "Failed to get profile: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get profile: " + std::string(e.what())},
                    {"error_code", "PROFILE_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // instruments.requestIdentity - Demander l'identité via SysEx
    // ========================================================================
    
    factory.registerCommand("instruments.requestIdentity",
        [deviceManager, sysExHandler](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Requesting device identity...");
            
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
                
                // Vérifier que le device est connecté
                if (!deviceManager->isConnected(deviceId)) {
                    return {
                        {"success", false},
                        {"error", "Device not connected"},
                        {"error_code", "DEVICE_NOT_CONNECTED"},
                        {"data", {
                            {"device_id", deviceId}
                        }}
                    };
                }
                
                // Envoyer la requête SysEx
                if (!sysExHandler) {
                    return {
                        {"success", false},
                        {"error", "SysEx handler not available"},
                        {"error_code", "SYSEX_NOT_AVAILABLE"}
                    };
                }
                
                bool sent = sysExHandler->requestIdentity(deviceId);
                
                if (!sent) {
                    Logger::error("InstrumentsAPI", 
                        "Failed to send identity request to: " + deviceId);
                    return {
                        {"success", false},
                        {"error", "Failed to send identity request"},
                        {"error_code", "SYSEX_SEND_FAILED"}
                    };
                }
                
                Logger::info("InstrumentsAPI", 
                    "✓ Identity request sent to: " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "Identity request sent"},
                    {"data", {
                        {"device_id", deviceId},
                        {"note", "Response will be received via event 'sysex:identity'"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", 
                    "Failed to request identity: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to request identity: " + std::string(e.what())},
                    {"error_code", "REQUEST_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // instruments.requestNoteMap - Demander la note map via SysEx
    // ========================================================================
    
    factory.registerCommand("instruments.requestNoteMap",
        [deviceManager, sysExHandler](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Requesting note map...");
            
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
                
                // Vérifier que le device est connecté
                if (!deviceManager->isConnected(deviceId)) {
                    return {
                        {"success", false},
                        {"error", "Device not connected"},
                        {"error_code", "DEVICE_NOT_CONNECTED"},
                        {"data", {
                            {"device_id", deviceId}
                        }}
                    };
                }
                
                // Envoyer la requête SysEx
                if (!sysExHandler) {
                    return {
                        {"success", false},
                        {"error", "SysEx handler not available"},
                        {"error_code", "SYSEX_NOT_AVAILABLE"}
                    };
                }
                
                bool sent = sysExHandler->requestNoteMap(deviceId);
                
                if (!sent) {
                    Logger::error("InstrumentsAPI", 
                        "Failed to send note map request to: " + deviceId);
                    return {
                        {"success", false},
                        {"error", "Failed to send note map request"},
                        {"error_code", "SYSEX_SEND_FAILED"}
                    };
                }
                
                Logger::info("InstrumentsAPI", 
                    "✓ Note map request sent to: " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "Note map request sent"},
                    {"data", {
                        {"device_id", deviceId},
                        {"note", "Response will be received via event 'sysex:notemap'"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", 
                    "Failed to request note map: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to request note map: " + std::string(e.what())},
                    {"error_code", "REQUEST_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // instruments.requestCC - Demander les capacités CC via SysEx
    // ========================================================================
    
    factory.registerCommand("instruments.requestCC",
        [deviceManager, sysExHandler](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Requesting CC capabilities...");
            
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
                
                // Vérifier que le device est connecté
                if (!deviceManager->isConnected(deviceId)) {
                    return {
                        {"success", false},
                        {"error", "Device not connected"},
                        {"error_code", "DEVICE_NOT_CONNECTED"},
                        {"data", {
                            {"device_id", deviceId}
                        }}
                    };
                }
                
                // Envoyer la requête SysEx
                if (!sysExHandler) {
                    return {
                        {"success", false},
                        {"error", "SysEx handler not available"},
                        {"error_code", "SYSEX_NOT_AVAILABLE"}
                    };
                }
                
                bool sent = sysExHandler->requestCCCapabilities(deviceId);
                
                if (!sent) {
                    Logger::error("InstrumentsAPI", 
                        "Failed to send CC request to: " + deviceId);
                    return {
                        {"success", false},
                        {"error", "Failed to send CC capabilities request"},
                        {"error_code", "SYSEX_SEND_FAILED"}
                    };
                }
                
                Logger::info("InstrumentsAPI", 
                    "✓ CC capabilities request sent to: " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "CC capabilities request sent"},
                    {"data", {
                        {"device_id", deviceId},
                        {"note", "Response will be received via event 'sysex:cc_capabilities'"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", 
                    "Failed to request CC: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to request CC: " + std::string(e.what())},
                    {"error_code", "REQUEST_ERROR"}
                };
            }
        }
    );
    
    Logger::info("InstrumentHandlers", "✅ Instrument commands registered (8 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER instruments.cpp v3.0.1-corrections
// ============================================================================

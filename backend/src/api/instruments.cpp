// ============================================================================
// instruments.cpp - VERSION CORRIGÉE avec commande WebSocket
// Suppression de handleGetDeviceProfile (httplib) 
// Ajout de instruments.getProfile (WebSocket)
// ============================================================================

#include "../core/commands/CommandFactory.h"
#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/sysex/SysExHandler.h"
#include "../core/Logger.h"

namespace midiMind {

// ============================================================================
// FONCTION: registerInstrumentCommands()
// ============================================================================
void registerInstrumentCommands(
    CommandFactory& factory,
    std::shared_ptr<MidiDeviceManager> deviceManager,
    std::shared_ptr<SysExHandler> sysExHandler)  // Ajout du SysExHandler en paramètre
	registerInstrumentCommands(*commandProcessor_, deviceManager_, sysexHandler_);
{
    Logger::info("InstrumentHandlers", "Registering instrument commands...");
    
    // ========================================================================
    // instruments.list
    // ========================================================================
    factory.registerCommand("instruments.list",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Listing MIDI devices...");
            
            try {
                // ✅ CORRIGÉ: getConnectedDevices au lieu de getAllDevices
                auto devices = deviceManager->getConnectedDevices();
                
                json deviceList = json::array();
                for (const auto& device : devices) {
                    json deviceInfo;
                    deviceInfo["id"] = device->getId();
                    deviceInfo["name"] = device->getName();
                    deviceInfo["type"] = device->getTypeString();
                    deviceInfo["connected"] = device->isConnected();
                    deviceInfo["ports"] = {
                        {"input", device->hasInput()},
                        {"output", device->hasOutput()}
                    };
                    
                    deviceList.push_back(deviceInfo);
                }
                
                Logger::info("InstrumentsAPI", 
                    "Listed " + std::to_string(deviceList.size()) + " devices");
                
                return {
                    {"success", true},
                    {"devices", deviceList},
                    {"count", deviceList.size()}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", "Failed to list devices: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to list devices: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // instruments.connect
    // ========================================================================
    factory.registerCommand("instruments.connect",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Connecting to device...");
            
            try {
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                bool success = deviceManager->connectDevice(deviceId);
                
                if (success) {
                    Logger::info("InstrumentsAPI", "Connected to device: " + deviceId);
                    return {
                        {"success", true},
                        {"message", "Device connected successfully"},
                        {"device_id", deviceId}
                    };
                } else {
                    return {
                        {"success", false},
                        {"error", "Failed to connect to device: " + deviceId}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", "Connection failed: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Connection failed: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // instruments.disconnect
    // ========================================================================
    factory.registerCommand("instruments.disconnect",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Disconnecting device...");
            
            try {
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                bool success = deviceManager->disconnectDevice(deviceId);
                
                if (success) {
                    Logger::info("InstrumentsAPI", "Disconnected device: " + deviceId);
                    return {
                        {"success", true},
                        {"message", "Device disconnected successfully"},
                        {"device_id", deviceId}
                    };
                } else {
                    return {
                        {"success", false},
                        {"error", "Failed to disconnect device: " + deviceId}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", "Disconnection failed: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Disconnection failed: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // instruments.scan
    // ========================================================================
    factory.registerCommand("instruments.scan",
        [deviceManager](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Scanning for devices...");
            
            try {
                auto foundDevices = deviceManager->discoverDevices(true);
                
                json deviceList = json::array();
                for (const auto& info : foundDevices) {
                    json deviceInfo;
                    deviceInfo["id"] = info.id;
                    deviceInfo["name"] = info.name;
                    deviceInfo["type"] = static_cast<int>(info.type);
                    deviceInfo["connected"] = info.connected;
                    
                    deviceList.push_back(deviceInfo);
                }
                
                Logger::info("InstrumentsAPI", 
                    "Scan completed, found " + std::to_string(foundDevices.size()) + " devices");
                
                return {
                    {"success", true},
                    {"message", "Device scan completed"},
                    {"found_devices", deviceList},
                    {"count", foundDevices.size()}
                };
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", "Scan failed: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Scan failed: " + std::string(e.what())}
                };
            }
        }
    );
    
    // ========================================================================
    // ✨ NOUVEAU: instruments.getProfile (via WebSocket au lieu de HTTP)
    // ========================================================================
    factory.registerCommand("instruments.getProfile",
        [deviceManager, sysExHandler](const json& params) -> json {
            Logger::debug("InstrumentsAPI", "Getting device profile...");
            
            try {
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"}
                    };
                }
                
                std::string deviceId = params["device_id"];
                
                json response;
                response["success"] = true;
                response["device"]["id"] = deviceId;
                
                // Vérifier si connecté
                auto device = deviceManager->getDevice(deviceId);
                bool isConnected = (device && device->isConnected());
                response["device"]["connected"] = isConnected;
                
                if (!isConnected) {
                    response["device"]["status"] = "disconnected";
                    return response;
                }
                
                response["device"]["status"] = "connected";
                
                // Standard Identity
                auto stdIdentity = sysExHandler->getDeviceIdentity(deviceId);
                if (stdIdentity) {
                    response["standard_identity"]["manufacturer_id"] = stdIdentity->manufacturerId;
                    response["standard_identity"]["family_code"] = stdIdentity->familyCode;
                    response["standard_identity"]["model_number"] = stdIdentity->modelNumber;
                    response["standard_identity"]["version"]["major"] = stdIdentity->versionMajor;
                    response["standard_identity"]["version"]["minor"] = stdIdentity->versionMinor;
                    response["standard_identity"]["version"]["patch"] = stdIdentity->versionPatch;
                } else {
                    response["standard_identity"] = nullptr;
                }
                
                // Custom Identity (Bloc 1)
                auto identity = sysExHandler->getCustomIdentity(deviceId);
                if (identity) {
                    response["identity"]["unique_id"] = identity->uniqueId;
                    response["identity"]["name"] = identity->name;
                    response["identity"]["type"] = identity->type;
                    response["identity"]["firmware"]["major"] = identity->firmwareVersion[0];
                    response["identity"]["firmware"]["minor"] = identity->firmwareVersion[1];
                    response["identity"]["firmware"]["patch"] = identity->firmwareVersion[2];
                    response["identity"]["firmware"]["build"] = identity->firmwareVersion[3];
                    response["playable"]["range"]["first"] = identity->firstNote;
                    response["playable"]["range"]["count"] = identity->noteCount;
                    response["playable"]["range"]["last"] = identity->firstNote + identity->noteCount - 1;
                    response["playable"]["polyphony"]["max"] = identity->maxPolyphony;
                    response["performance"]["latency_ms"] = identity->responseDelay;
                    response["programs"]["count"] = identity->programCount;
                } else {
                    response["identity"] = nullptr;
                }
                
                // Note Map (Bloc 2)
                auto noteMap = sysExHandler->getNoteMap(deviceId);
                if (noteMap) {
                    response["playable"]["note_map"]["defined"] = true;
                    json playableNotes = json::array();
                    for (int i = 0; i < 128; i++) {
                        if (noteMap->isNotePlayable(i)) {
                            playableNotes.push_back(i);
                        }
                    }
                    response["playable"]["note_map"]["playable_notes"] = playableNotes;
                    response["playable"]["note_map"]["count"] = playableNotes.size();
                } else {
                    response["playable"]["note_map"]["defined"] = false;
                }
                
                // CC Capabilities (Bloc 3)
                auto ccCaps = sysExHandler->getCCCapabilities(deviceId);
                if (ccCaps) {
                    response["controllers"]["cc_supported"]["count"] = ccCaps->supportedCC.size();
                    response["controllers"]["cc_supported"]["list"] = ccCaps->supportedCC;
                } else {
                    response["controllers"]["cc_supported"] = nullptr;
                }
                
                // Air Capabilities (Bloc 4)
                auto airCaps = sysExHandler->getAirCapabilities(deviceId);
                if (airCaps) {
                    response["breath"]["has_breath_control"] = (airCaps->breathType != BreathType::NONE);
                    response["breath"]["type"] = static_cast<int>(airCaps->breathType);
                    response["breath"]["cc"] = airCaps->breathCC;
                    response["breath"]["range"]["min"] = airCaps->minValue;
                    response["breath"]["range"]["max"] = airCaps->maxValue;
                } else {
                    response["breath"]["has_breath_control"] = false;
                }
                
                // Light Capabilities (Bloc 5)
                auto lightCaps = sysExHandler->getLightCapabilities(deviceId);
                if (lightCaps) {
                    response["lights"]["has_lights"] = (lightCaps->ledCount > 0);
                    response["lights"]["count"] = lightCaps->ledCount;
                    response["lights"]["control_method"] = static_cast<int>(lightCaps->controlMethod);
                } else {
                    response["lights"]["has_lights"] = false;
                }
                
                // Sensors Feedback (Bloc 7)
                auto sensors = sysExHandler->getSensorsFeedback(deviceId);
                if (sensors) {
                    response["sensors"]["count"] = sensors->sensors.size();
                    json sensorsArray = json::array();
                    for (const auto& sensor : sensors->sensors) {
                        json sensorInfo;
                        sensorInfo["id"] = sensor.id;
                        sensorInfo["type"] = static_cast<int>(sensor.type);
                        sensorInfo["value"] = sensor.value;
                        sensorsArray.push_back(sensorInfo);
                    }
                    response["sensors"]["list"] = sensorsArray;
                } else {
                    response["sensors"]["count"] = 0;
                }
                
                // Sync Clock (Bloc 8)
                auto sync = sysExHandler->getSyncClock(deviceId);
                if (sync) {
                    response["sync"]["midi_clock"] = sync->midiClockSupport;
                    response["sync"]["mtc"] = sync->mtcSupport;
                    response["sync"]["internal_tempo"] = sync->internalBPM;
                    response["sync"]["tempo_range"]["min"] = sync->minBPM;
                    response["sync"]["tempo_range"]["max"] = sync->maxBPM;
                } else {
                    response["sync"] = nullptr;
                }
                
                Logger::info("InstrumentsAPI", "Profile sent for device " + deviceId);
                return response;
                
            } catch (const std::exception& e) {
                Logger::error("InstrumentsAPI", "Failed to get profile: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get device profile: " + std::string(e.what())}
                };
            }
        }
    );
    
    Logger::info("InstrumentHandlers", "✓ Registered 5 instrument commands");
}

} // namespace midiMind
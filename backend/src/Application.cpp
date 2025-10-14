// ============================================================================
// Fichier: backend/src/Application.cpp
// Version: 3.0.0-refonte
// Date: 2025-10-09
// ============================================================================
// Description:
//   Application principale avec intégration complète du protocole unifié.
//   Initialise tous les composants et configure les callbacks.
// ============================================================================

#include "Application.h"
#include "core/Logger.h"
#include "api/MessageEnvelope.h"
#include <csignal>

namespace midiMind {

// ============================================================================
// SINGLETON
// ============================================================================

Application& Application::instance() {
    static Application instance;
    return instance;
}

// ============================================================================
// CONSTRUCTEUR
// ============================================================================

Application::Application()
    : initialized_(false)
    , running_(false)
{
    Logger::info("Application", "Creating application instance...");
}

Application::~Application() {
    stop();
}

// ============================================================================
// INITIALISATION
// ============================================================================

bool Application::initialize(const std::string& configPath) {
    if (initialized_) {
        Logger::warn("Application", "Already initialized");
        return true;
    }
    
    Logger::info("Application", "Initializing midiMind v3.0...");
    Logger::info("Application", "Protocol version: " + std::string(protocol::PROTOCOL_VERSION));
    
    try {
        // ====================================================================
        // 1. INITIALISER LES COMPOSANTS CORE
        // ====================================================================
        
        Logger::info("Application", "Step 1/5: Initializing core components...");
        
        // Device Manager
        deviceManager_ = std::make_shared<MidiDeviceManager>();
        Logger::info("Application", "  ✓ Device Manager initialized");
        
        // Router
        router_ = std::make_shared<MidiRouter>();
        Logger::info("Application", "  ✓ MIDI Router initialized");
        
        // Player
        player_ = std::make_shared<MidiPlayer>(router_);
        Logger::info("Application", "  ✓ MIDI Player initialized");
        
        // File Manager
        fileManager_ = std::make_shared<MidiFileManager>();
        Logger::info("Application", "  ✓ File Manager initialized");
        
        // SysEx Handler
        sysexHandler_ = std::make_shared<SysExHandler>();
        Logger::info("Application", "  ✓ SysEx Handler initialized");
        
        // ====================================================================
        // 2. INITIALISER LE COMMAND PROCESSOR
        // ====================================================================
        
        Logger::info("Application", "Step 2/5: Initializing command processor...");
        
        commandProcessor_ = std::make_shared<CommandProcessorV2>(
            deviceManager_,
            router_,
            player_,
            fileManager_
        );
        
        Logger::info("Application", "  ✓ Command Processor initialized");
        
        // ====================================================================
        // 3. INITIALISER L'API SERVER
        // ====================================================================
        
        Logger::info("Application", "Step 3/5: Initializing API server...");
        
        apiServer_ = std::make_shared<ApiServer>();
        
        // Configurer le callback de commandes
        apiServer_->setCommandCallback(
            [this](const json& command) -> json {
                return commandProcessor_->processCommand(command);
            }
        );
        
        // Configurer le SysEx handler pour les événements
        apiServer_->setSysExHandler(sysexHandler_);
        
        Logger::info("Application", "  ✓ API Server initialized");
        
        // ====================================================================
        // 4. CONFIGURER LES ÉVÉNEMENTS MIDI
        // ====================================================================
        
        Logger::info("Application", "Step 4/5: Configuring MIDI event broadcasting...");
        
        // Callback pour messages MIDI routés
        router_->setMessageCallback(
            [this](const MidiMessage& msg) {
                this->broadcastMidiMessage(msg);
            }
        );
        
        // Callback pour changements d'état du player
        player_->setStateCallback(
            [this](const std::string& state, double position) {
                this->broadcastPlaybackState(state, position);
            }
        );
        
        // Callback pour événements devices
        deviceManager_->setDeviceCallback(
            [this](const std::string& event, const std::string& deviceId) {
                this->broadcastDeviceEvent(event, deviceId);
            }
        );
        
        Logger::info("Application", "  ✓ Event broadcasting configured");
        
        // ====================================================================
        // 5. CONFIGURER LES CALLBACKS SYSEX
        // ====================================================================
        
        Logger::info("Application", "Step 5/5: Configuring SysEx callbacks...");
        
        // Callback Identity (Bloc 1)
        sysexHandler_->setOnIdentity(
            [this](const std::string& deviceId, const Identity& identity) {
                auto event = MessageEnvelope::createEvent(
                    "sysex:identity",
                    {
                        {"device_id", deviceId},
                        {"manufacturer", identity.manufacturerName},
                        {"model", identity.modelName},
                        {"version", identity.firmwareVersion}
                    },
                    protocol::EventPriority::NORMAL
                );
                apiServer_->broadcast(event);
            }
        );
        
        // Callback NoteMap (Bloc 2)
        sysexHandler_->setOnNoteMap(
            [this](const std::string& deviceId, const NoteMap& noteMap) {
                json notes = json::array();
                for (const auto& note : noteMap.playableNotes) {
                    notes.push_back(note);
                }
                
                auto event = MessageEnvelope::createEvent(
                    "sysex:notemap",
                    {
                        {"device_id", deviceId},
                        {"playable_notes", notes},
                        {"octave_range", {noteMap.minOctave, noteMap.maxOctave}}
                    },
                    protocol::EventPriority::NORMAL
                );
                apiServer_->broadcast(event);
            }
        );
        
        // Callback CCCapabilities (Bloc 3)
        sysexHandler_->setOnCCCapabilities(
            [this](const std::string& deviceId, const CCCapabilities& cc) {
                json ccList = json::array();
                for (const auto& ccNum : cc.supportedCCs) {
                    ccList.push_back(ccNum);
                }
                
                auto event = MessageEnvelope::createEvent(
                    "sysex:cc_capabilities",
                    {
                        {"device_id", deviceId},
                        {"supported_ccs", ccList}
                    },
                    protocol::EventPriority::NORMAL
                );
                apiServer_->broadcast(event);
            }
        );
        
        // Callback AirCapabilities (Bloc 4)
        sysexHandler_->setOnAirCapabilities(
            [this](const std::string& deviceId, const AirCapabilities& air) {
                auto event = MessageEnvelope::createEvent(
                    "sysex:air_capabilities",
                    {
                        {"device_id", deviceId},
                        {"breath_control", air.breathControl},
                        {"aftertouch", air.channelPressure}
                    },
                    protocol::EventPriority::NORMAL
                );
                apiServer_->broadcast(event);
            }
        );
        
        // Callback LightCapabilities (Bloc 5)
        sysexHandler_->setOnLightCapabilities(
            [this](const std::string& deviceId, const LightCapabilities& light) {
                auto event = MessageEnvelope::createEvent(
                    "sysex:light_capabilities",
                    {
                        {"device_id", deviceId},
                        {"rgb_support", light.rgbSupport},
                        {"brightness_levels", light.brightnessLevels}
                    },
                    protocol::EventPriority::NORMAL
                );
                apiServer_->broadcast(event);
            }
        );
        
        // Callback SensorsFeedback (Bloc 7)
        sysexHandler_->setOnSensorsFeedback(
            [this](const std::string& deviceId, const SensorsFeedback& sensors) {
                auto event = MessageEnvelope::createEvent(
                    "sysex:sensors",
                    {
                        {"device_id", deviceId},
                        {"gyroscope", sensors.gyroscope},
                        {"accelerometer", sensors.accelerometer}
                    },
                    protocol::EventPriority::NORMAL
                );
                apiServer_->broadcast(event);
            }
        );
        
        // Callback SyncClock (Bloc 8)
        sysexHandler_->setOnSyncClock(
            [this](const std::string& deviceId, const SyncClock& sync) {
                auto event = MessageEnvelope::createEvent(
                    "sysex:sync_clock",
                    {
                        {"device_id", deviceId},
                        {"midi_clock", sync.midiClockSupport},
                        {"mtc", sync.mtcSupport},
                        {"internal_bpm", sync.internalBPM}
                    },
                    protocol::EventPriority::NORMAL
                );
                apiServer_->broadcast(event);
            }
        );
        
        Logger::info("Application", "  ✓ SysEx callbacks configured (7 callbacks)");
        
        // ====================================================================
        // SUCCÈS
        // ====================================================================
        
        initialized_ = true;
        
        Logger::info("Application", "");
        Logger::info("Application", "========================================");
        Logger::info("Application", "✓ midiMind v3.0 initialized successfully");
        Logger::info("Application", "  Protocol: v" + std::string(protocol::PROTOCOL_VERSION));
        Logger::info("Application", "  Components: 6 initialized");
        Logger::info("Application", "  Commands: " + std::to_string(commandProcessor_->getCommandCount()));
        Logger::info("Application", "========================================");
        Logger::info("Application", "");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Initialization failed: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// DÉMARRAGE
// ============================================================================

bool Application::start() {
    if (!initialized_) {
        Logger::error("Application", "Cannot start: not initialized");
        return false;
    }
    
    if (running_) {
        Logger::warn("Application", "Already running");
        return true;
    }
    
    Logger::info("Application", "Starting services...");
    
    try {
        // Démarrer l'API Server
        apiServer_->start(8080);
        Logger::info("Application", "  ✓ API Server started on port 8080");
        
        // Scanner les devices
        deviceManager_->scanDevices();
        Logger::info("Application", "  ✓ Device scan initiated");
        
        running_ = true;
        
        Logger::info("Application", "");
        Logger::info("Application", "========================================");
        Logger::info("Application", "✓ midiMind v3.0 is now running");
        Logger::info("Application", "  WebSocket: ws://localhost:8080");
        Logger::info("Application", "  Ready to accept connections");
        Logger::info("Application", "========================================");
        Logger::info("Application", "");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Failed to start: " + std::string(e.what()));
        running_ = false;
        return false;
    }
}

// ============================================================================
// ARRÊT
// ============================================================================

void Application::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("Application", "Stopping services...");
    
    running_ = false;
    
    // Arrêter l'API Server
    if (apiServer_) {
        apiServer_->stop();
        Logger::info("Application", "  ✓ API Server stopped");
    }
    
    // Arrêter le player
    if (player_) {
        player_->stop();
        Logger::info("Application", "  ✓ Player stopped");
    }
    
    Logger::info("Application", "✓ midiMind stopped");
}

// ============================================================================
// BROADCAST D'ÉVÉNEMENTS
// ============================================================================

void Application::broadcastMidiMessage(const MidiMessage& msg) {
    if (!apiServer_ || !running_) return;
    
    try {
        // Priorité haute pour messages MIDI
        auto event = MessageEnvelope::createEvent(
            "midi:message",
            {
                {"status", msg.status},
                {"data1", msg.data1},
                {"data2", msg.data2},
                {"timestamp", msg.timestamp}
            },
            protocol::EventPriority::HIGH
        );
        
        apiServer_->broadcast(event);
        
    } catch (const std::exception& e) {
        Logger::error("Application", 
            "Error broadcasting MIDI message: " + std::string(e.what()));
    }
}

void Application::broadcastPlaybackState(const std::string& state, double position) {
    if (!apiServer_ || !running_) return;
    
    try {
        auto event = MessageEnvelope::createEvent(
            "playback:state",
            {
                {"state", state},
                {"position", position},
                {"duration", player_->getDuration()},
                {"tempo", player_->getTempo()},
                {"loop", player_->isLooping()}
            },
            protocol::EventPriority::NORMAL
        );
        
        apiServer_->broadcast(event);
        
    } catch (const std::exception& e) {
        Logger::error("Application", 
            "Error broadcasting playback state: " + std::string(e.what()));
    }
}

void Application::broadcastDeviceEvent(const std::string& event, 
                                       const std::string& deviceId) {
    if (!apiServer_ || !running_) return;
    
    try {
        auto device = deviceManager_->getDevice(deviceId);
        
        json deviceData;
        deviceData["device_id"] = deviceId;
        
        if (device) {
            deviceData["name"] = device->getName();
            deviceData["connected"] = device->isConnected();
        }
        
        auto envelope = MessageEnvelope::createEvent(
            "device:" + event,
            deviceData,
            protocol::EventPriority::NORMAL
        );
        
        apiServer_->broadcast(envelope);
        
    } catch (const std::exception& e) {
        Logger::error("Application", 
            "Error broadcasting device event: " + std::string(e.what()));
    }
}

// ============================================================================
// ACCESSEURS
// ============================================================================

std::shared_ptr<ApiServer> Application::getApiServer() {
    return apiServer_;
}

std::shared_ptr<MidiRouter> Application::getMidiRouter() {
    return router_;
}

std::shared_ptr<MidiPlayer> Application::getMidiPlayer() {
    return player_;
}

std::shared_ptr<MidiDeviceManager> Application::getDeviceManager() {
    return deviceManager_;
}

std::shared_ptr<CommandProcessorV2> Application::getCommandProcessor() {
    return commandProcessor_;
}

bool Application::isInitialized() const {
    return initialized_;
}

bool Application::isRunning() const {
    return running_;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.cpp
// ============================================================================

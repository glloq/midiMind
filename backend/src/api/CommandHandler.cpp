// ============================================================================
// File: backend/src/api/CommandHandler.cpp
// Version: 4.2.2
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#include "CommandHandler.h"
#include "../core/Logger.h"
#include "../timing/LatencyCompensator.h"
#include "../storage/InstrumentDatabase.h"
#include "../storage/PresetManager.h"
#include "../storage/MidiDatabase.h"
#include <chrono>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include "../midi/JsonMidiConverter.h"

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================


CommandHandler::CommandHandler(
    std::shared_ptr<MidiDeviceManager> deviceManager,
    std::shared_ptr<MidiRouter> router,
    std::shared_ptr<MidiPlayer> player,
    std::shared_ptr<FileManager> fileManager,
    std::shared_ptr<LatencyCompensator> compensator,
    std::shared_ptr<InstrumentDatabase> instrumentDb,
    std::shared_ptr<PresetManager> presetManager,
    std::shared_ptr<EventBus> eventBus,
    std::shared_ptr<MidiDatabase> midiDatabase,
    std::shared_ptr<PlaylistManager> playlistManager)
    : deviceManager_(deviceManager)
    , router_(router)
    , player_(player)
    , fileManager_(fileManager)
    , compensator_(compensator)
    , instrumentDb_(instrumentDb)
    , presetManager_(presetManager)
    , eventBus_(eventBus)
    , midiDatabase_(midiDatabase)
    , playlistManager_(playlistManager)
{
    Logger::info("CommandHandler", "Initializing CommandHandler v4.2.2...");
    registerAllCommands();
    Logger::info("CommandHandler", 
                "✓ CommandHandler initialized (" + 
                std::to_string(commands_.size()) + " commands)");
}


CommandHandler::~CommandHandler() {
    Logger::info("CommandHandler", "CommandHandler destroyed");
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================

json CommandHandler::processCommand(const json& command) {
    try {
        std::string error;
        if (!validateCommand(command, error)) {
            return createErrorResponse(error, "INVALID_COMMAND");
        }
        
        // CORRECTION CRITIQUE 1: Validation sécurisée avant accès
        if (!command.contains("command") || !command["command"].is_string()) {
            return createErrorResponse("Missing or invalid 'command' field", "INVALID_COMMAND");
        }
        
        std::string commandName = command["command"];
        json params = command.value("params", json::object());
        
        // Find and copy command function under lock
        CommandFunction func;
        {
            std::lock_guard<std::mutex> lock(commandsMutex_);
            
            auto it = commands_.find(commandName);
            if (it == commands_.end()) {
                return createErrorResponse(
                    "Unknown command: " + commandName,
                    "UNKNOWN_COMMAND");
            }
            
            func = it->second;  // Copy function
        }
        // Lock released here - execute command without holding lock
        
        try {
            json data = func(params);
            return createSuccessResponse(data);
            
        } catch (const std::exception& e) {
            return createErrorResponse(
                std::string(e.what()),
                "COMMAND_FAILED");
        }
        
    } catch (const std::exception& e) {
        return createErrorResponse(
            "Failed to process command: " + std::string(e.what()),
            "INTERNAL_ERROR");
    }
}

json CommandHandler::processCommand(const std::string& jsonString) {
    try {
        json command = json::parse(jsonString);
        return processCommand(command);
        
    } catch (const json::parse_error& e) {
        return createErrorResponse(
            "Invalid JSON: " + std::string(e.what()),
            "PARSE_ERROR");
    }
}

// ============================================================================
// COMMAND REGISTRATION
// ============================================================================

void CommandHandler::registerCommand(const std::string& name, 
                                    CommandFunction function) {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    commands_[name] = function;
    Logger::debug("CommandHandler", "Registered command: " + name);
}

bool CommandHandler::unregisterCommand(const std::string& name) {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        commands_.erase(it);
        Logger::debug("CommandHandler", "Unregistered command: " + name);
        return true;
    }
    
    return false;
}

// ============================================================================
// INTROSPECTION
// ============================================================================

size_t CommandHandler::getCommandCount() const {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    return commands_.size();
}

std::vector<std::string> CommandHandler::listCommands() const {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    
    std::vector<std::string> result;
    result.reserve(commands_.size());
    
    for (const auto& [name, func] : commands_) {
        result.push_back(name);
    }
    
    std::sort(result.begin(), result.end());
    
    return result;
}

std::unordered_map<std::string, std::vector<std::string>> 
CommandHandler::listCommandsByCategory() const {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    
    std::unordered_map<std::string, std::vector<std::string>> result;
    
    for (const auto& [name, func] : commands_) {
        size_t dotPos = name.find('.');
        std::string category = (dotPos != std::string::npos) 
            ? name.substr(0, dotPos) 
            : "other";
        
        result[category].push_back(name);
    }
    
    for (auto& [category, commands] : result) {
        std::sort(commands.begin(), commands.end());
    }
    
    return result;
}

bool CommandHandler::hasCommand(const std::string& name) const {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    return commands_.find(name) != commands_.end();
}

// ============================================================================
// REGISTRATION DISPATCHER
// ============================================================================

void CommandHandler::registerAllCommands() {
    registerDeviceCommands();
    registerRoutingCommands();
    registerPlaybackCommands();
    registerFileCommands();
    registerMidiCommands(); 
	registerPlaylistCommands();	
    registerSystemCommands();
    registerNetworkCommands();
    registerLoggerCommands();
    registerLatencyCommands();
    registerPresetCommands();
}

// ============================================================================
// DEVICE COMMANDS (12 commands)
// ============================================================================

void CommandHandler::registerDeviceCommands() {
    if (!deviceManager_) {
        Logger::warning("CommandHandler", 
                    "DeviceManager not available, skipping device commands");
        return;
    }
    
    // devices.list
    registerCommand("devices.list", [this](const json& params) {
        auto devices = deviceManager_->getAvailableDevices();
        
        json devicesJson = json::array();
        for (const auto& device : devices) {
            devicesJson.push_back(device);
        }
        
        return json{
            {"devices", devicesJson},
            {"count", devices.size()}
        };
    });
    
    // devices.listDetailed
    registerCommand("devices.listDetailed", [this](const json& params) {
        auto devices = deviceManager_->getDetailedDeviceList();
        
        json devicesJson = json::array();
        for (const auto& device : devices) {
            devicesJson.push_back(device);
        }
        
        return json{
            {"devices", devicesJson},
            {"count", devices.size()}
        };
    });
    
    // devices.refresh
    registerCommand("devices.refresh", [this](const json& params) {
        bool success = deviceManager_->refreshDevices();
        auto devices = deviceManager_->getAvailableDevices();
        
        json devicesJson = json::array();
        for (const auto& device : devices) {
            devicesJson.push_back(device);
        }
        
        return json{
            {"success", success},
            {"devices", devicesJson},
            {"count", devices.size()}
        };
    });
    
    // devices.get
    registerCommand("devices.get", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        auto device = deviceManager_->getDeviceDetails(deviceId);
        
        return json{{"device", device}};
    });
    
    // devices.connect
    registerCommand("devices.connect", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        bool success = deviceManager_->connectDevice(deviceId);
        
        return json{
            {"success", success},
            {"device_id", deviceId},
            {"status", success ? "connected" : "failed"}
        };
    });
    
    // devices.disconnect
    registerCommand("devices.disconnect", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        bool success = deviceManager_->disconnectDevice(deviceId);
        
        return json{
            {"success", success},
            {"device_id", deviceId},
            {"status", success ? "disconnected" : "failed"}
        };
    });
    
    // devices.status
    registerCommand("devices.status", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        bool isConnected = deviceManager_->isDeviceConnected(deviceId);
        
        return json{
            {"device_id", deviceId},
            {"connected", isConnected},
            {"status", isConnected ? "connected" : "disconnected"}
        };
    });
    
    // devices.testConnection
    registerCommand("devices.testConnection", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        bool success = deviceManager_->testDeviceConnection(deviceId);
        
        return json{
            {"success", success},
            {"device_id", deviceId},
            {"test_result", success ? "passed" : "failed"}
        };
    });
    
    // devices.rename
    registerCommand("devices.rename", [this](const json& params) {
        if (!params.contains("device_id") || !params.contains("new_name")) {
            throw std::runtime_error("Missing device_id or new_name parameter");
        }
        
        std::string deviceId = params["device_id"];
        std::string newName = params["new_name"];
        
        bool success = deviceManager_->renameDevice(deviceId, newName);
        
        return json{
            {"success", success},
            {"device_id", deviceId},
            {"new_name", newName}
        };
    });
    
    // devices.setAlias
    registerCommand("devices.setAlias", [this](const json& params) {
        if (!params.contains("device_id") || !params.contains("alias")) {
            throw std::runtime_error("Missing device_id or alias parameter");
        }
        
        std::string deviceId = params["device_id"];
        std::string alias = params["alias"];
        
        bool success = deviceManager_->setDeviceAlias(deviceId, alias);
        
        return json{
            {"success", success},
            {"device_id", deviceId},
            {"alias", alias}
        };
    });
    
    // devices.getCapabilities
    registerCommand("devices.getCapabilities", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        auto capabilities = deviceManager_->getDeviceCapabilities(deviceId);
        
        return json{
            {"device_id", deviceId},
            {"capabilities", capabilities}
        };
    });
    
    // devices.scan
    registerCommand("devices.scan", [this](const json& params) {
        bool includeVirtual = params.value("include_virtual", true);
        auto devices = deviceManager_->scanDevices(includeVirtual);
        
        json devicesJson = json::array();
        for (const auto& device : devices) {
            devicesJson.push_back(device);
        }
        
        return json{
            {"devices", devicesJson},
            {"count", devices.size()}
        };
    });
    
    Logger::debug("CommandHandler", "✓ Device commands registered (12 commands)");
}

// ============================================================================
// ROUTING COMMANDS (9 commands)
// ============================================================================

void CommandHandler::registerRoutingCommands() {
    if (!router_) {
        Logger::warning("CommandHandler", 
                    "Router not available, skipping routing commands");
        return;
    }
    
    // routing.addRoute
    registerCommand("routing.addRoute", [this](const json& params) {
        if (!params.contains("source") || !params.contains("destination")) {
            throw std::runtime_error("Missing source or destination parameter");
        }
        
        std::string source = params["source"];
        std::string destination = params["destination"];
        
        bool success = router_->addRoute(source, destination);
        
        return json{
            {"success", success},
            {"source", source},
            {"destination", destination}
        };
    });
    
    // routing.removeRoute
    registerCommand("routing.removeRoute", [this](const json& params) {
        if (!params.contains("source") || !params.contains("destination")) {
            throw std::runtime_error("Missing source or destination parameter");
        }
        
        std::string source = params["source"];
        std::string destination = params["destination"];
        
        bool success = router_->removeRoute(source, destination);
        
        return json{
            {"success", success},
            {"source", source},
            {"destination", destination}
        };
    });
    
    // routing.clearRoutes
    registerCommand("routing.clearRoutes", [this](const json& params) {
        if (params.contains("source")) {
            std::string source = params["source"];
            router_->clearRoutes(source);
            
            return json{
                {"success", true},
                {"source", source}
            };
        } else {
            router_->clearAllRoutes();
            
            return json{{"success", true}};
        }
    });
    
    // routing.listRoutes
    registerCommand("routing.listRoutes", [this](const json& params) {
        auto routes = router_->getRoutes();
        
        json routesJson = json::array();
        for (const auto& [source, destinations] : routes) {
            for (const auto& dest : destinations) {
                routesJson.push_back({
                    {"source", source},
                    {"destination", dest}
                });
            }
        }
        
        return json{
            {"routes", routesJson},
            {"count", routesJson.size()}
        };
    });
    
    // routing.getDestinations
    registerCommand("routing.getDestinations", [this](const json& params) {
        if (!params.contains("source")) {
            throw std::runtime_error("Missing source parameter");
        }
        
        std::string source = params["source"];
        auto destinations = router_->getDestinations(source);
        
        json destsJson = json::array();
        for (const auto& dest : destinations) {
            destsJson.push_back(dest);
        }
        
        return json{
            {"source", source},
            {"destinations", destsJson},
            {"count", destinations.size()}
        };
    });
    
    // routing.setFilter
    registerCommand("routing.setFilter", [this](const json& params) {
        if (!params.contains("source") || 
            !params.contains("destination") || 
            !params.contains("filter")) {
            throw std::runtime_error("Missing required parameters");
        }
        
        std::string source = params["source"];
        std::string destination = params["destination"];
        json filter = params["filter"];
        
        bool success = router_->setRouteFilter(source, destination, filter);
        
        return json{
            {"success", success},
            {"source", source},
            {"destination", destination}
        };
    });
    
    // routing.removeFilter
    registerCommand("routing.removeFilter", [this](const json& params) {
        if (!params.contains("source") || !params.contains("destination")) {
            throw std::runtime_error("Missing source or destination parameter");
        }
        
        std::string source = params["source"];
        std::string destination = params["destination"];
        
        bool success = router_->removeRouteFilter(source, destination);
        
        return json{
            {"success", success},
            {"source", source},
            {"destination", destination}
        };
    });
    
    // routing.getFilter
    registerCommand("routing.getFilter", [this](const json& params) {
        if (!params.contains("source") || !params.contains("destination")) {
            throw std::runtime_error("Missing source or destination parameter");
        }
        
        std::string source = params["source"];
        std::string destination = params["destination"];
        
        auto filter = router_->getRouteFilter(source, destination);
        
        return json{
            {"source", source},
            {"destination", destination},
            {"filter", filter}
        };
    });
    
    // routing.enable
    registerCommand("routing.enable", [this](const json& params) {
        router_->enableRouting(true);
        
        return json{
            {"success", true},
            {"routing_enabled", true}
        };
    });
    
    Logger::debug("CommandHandler", "✓ Routing commands registered (9 commands)");
}

// ============================================================================
// PLAYBACK COMMANDS (14 commands)
// ============================================================================

void CommandHandler::registerPlaybackCommands() {
    if (!player_) {
        Logger::warning("CommandHandler", 
                    "MidiPlayer not available, skipping playback commands");
        return;
    }
    
    // playback.load
    registerCommand("playback.load", [this](const json& params) {
        if (!params.contains("file_path")) {
            throw std::runtime_error("Missing file_path parameter");
        }
        
        std::string filePath = params["file_path"];
        bool success = player_->loadFile(filePath);
        
        return json{
            {"success", success},
            {"file_path", filePath}
        };
    });
    
    // playback.play
    registerCommand("playback.play", [this](const json& params) {
        bool success = player_->play();
        
        return json{
            {"success", success},
            {"state", "playing"}
        };
    });
    
    // playback.pause
    registerCommand("playback.pause", [this](const json& params) {
        bool success = player_->pause();
        
        return json{
            {"success", success},
            {"state", "paused"}
        };
    });
    
    // playback.stop
    registerCommand("playback.stop", [this](const json& params) {
        bool success = player_->stop();
        
        return json{
            {"success", success},
            {"state", "stopped"}
        };
    });
    
    // playback.seek
    registerCommand("playback.seek", [this](const json& params) {
        if (!params.contains("position_ms")) {
            throw std::runtime_error("Missing position_ms parameter");
        }
        
        uint64_t positionMs = params["position_ms"];
        bool success = player_->seek(positionMs);
        
        return json{
            {"success", success},
            {"position_ms", positionMs}
        };
    });
    
    // playback.getStatus
    registerCommand("playback.getStatus", [this](const json& params) {
        auto status = player_->getStatus();
        
        return json{{"status", status}};
    });
    
    // playback.setLoop
    registerCommand("playback.setLoop", [this](const json& params) {
        if (!params.contains("enabled")) {
            throw std::runtime_error("Missing enabled parameter");
        }
        
        bool enabled = params["enabled"];
        player_->setLoop(enabled);
        
        return json{
            {"success", true},
            {"loop_enabled", enabled}
        };
    });
    
    // playback.setTempo
    registerCommand("playback.setTempo", [this](const json& params) {
        if (!params.contains("tempo_factor")) {
            throw std::runtime_error("Missing tempo_factor parameter");
        }
        
        double tempoFactor = params["tempo_factor"];
        bool success = player_->setTempoFactor(tempoFactor);
        
        return json{
            {"success", success},
            {"tempo_factor", tempoFactor}
        };
    });
    
    // playback.getPosition
    registerCommand("playback.getPosition", [this](const json& params) {
        uint64_t position = player_->getCurrentPosition();
        
        return json{
            {"position_ms", position}
        };
    });
    
    // playback.getDuration
    registerCommand("playback.getDuration", [this](const json& params) {
        uint64_t duration = player_->getDuration();
        
        return json{
            {"duration_ms", duration}
        };
    });
    
    // playback.setVolume
    registerCommand("playback.setVolume", [this](const json& params) {
        if (!params.contains("volume")) {
            throw std::runtime_error("Missing volume parameter");
        }
        
        double volume = params["volume"];
        if (volume < 0.0 || volume > 1.0) {
            throw std::runtime_error("Volume must be between 0.0 and 1.0");
        }
        
        player_->setVolume(volume);
        
        return json{
            {"success", true},
            {"volume", volume}
        };
    });
    
    // playback.mute
    registerCommand("playback.mute", [this](const json& params) {
        if (!params.contains("muted")) {
            throw std::runtime_error("Missing muted parameter");
        }
        
        bool muted = params["muted"];
        player_->setMute(muted);
        
        return json{
            {"success", true},
            {"muted", muted}
        };
    });
    
    // playback.getTrackInfo
    registerCommand("playback.getTrackInfo", [this](const json& params) {
        auto trackInfo = player_->getTrackInfo();
        
        return json{{"track_info", trackInfo}};
    });
    
    // playback.selectTracks
    registerCommand("playback.selectTracks", [this](const json& params) {
        if (!params.contains("track_ids")) {
            throw std::runtime_error("Missing track_ids parameter");
        }
        
        if (!params["track_ids"].is_array()) {
            throw std::runtime_error("track_ids must be an array");
        }
        
        std::vector<int> trackIds;
        for (const auto& id : params["track_ids"]) {
            trackIds.push_back(id.get<int>());
        }
        
        bool success = player_->selectTracks(trackIds);
        
        return json{
            {"success", success},
            {"track_count", trackIds.size()}
        };
    });
    
    Logger::debug("CommandHandler", "✓ Playback commands registered (14 commands)");
}

// ============================================================================
// FILE COMMANDS (11 commands)
// ============================================================================

void CommandHandler::registerFileCommands() {
    if (!fileManager_) {
        Logger::warning("CommandHandler", 
                    "FileManager not available, skipping file commands");
        return;
    }
    
    // files.list
    registerCommand("files.list", [this](const json& params) {
        std::string directory = params.value("directory", "");
        std::string filter = params.value("filter", "*.mid");
        
        auto files = fileManager_->listFiles(directory, filter);
        
        json filesJson = json::array();
        for (const auto& file : files) {
            filesJson.push_back(file);
        }
        
        return json{
            {"files", filesJson},
            {"count", files.size()}
        };
    });
    
    // files.get
    registerCommand("files.get", [this](const json& params) {
        if (!params.contains("file_path")) {
            throw std::runtime_error("Missing file_path parameter");
        }
        
        std::string filePath = params["file_path"];
        auto fileInfo = fileManager_->getFileInfo(filePath);
        
        return json{{"file_info", fileInfo}};
    });
    
    // files.delete
    registerCommand("files.delete", [this](const json& params) {
        if (!params.contains("file_path")) {
            throw std::runtime_error("Missing file_path parameter");
        }
        
        std::string filePath = params["file_path"];
        bool success = fileManager_->deleteFile(filePath);
        
        return json{
            {"success", success},
            {"file_path", filePath}
        };
    });
    
    // files.rename
    registerCommand("files.rename", [this](const json& params) {
        if (!params.contains("old_path") || !params.contains("new_path")) {
            throw std::runtime_error("Missing old_path or new_path parameter");
        }
        
        std::string oldPath = params["old_path"];
        std::string newPath = params["new_path"];
        
        bool success = fileManager_->renameFile(oldPath, newPath);
        
        return json{
            {"success", success},
            {"old_path", oldPath},
            {"new_path", newPath}
        };
    });
    
    // files.copy
    registerCommand("files.copy", [this](const json& params) {
        if (!params.contains("source_path") || !params.contains("dest_path")) {
            throw std::runtime_error("Missing source_path or dest_path parameter");
        }
        
        std::string sourcePath = params["source_path"];
        std::string destPath = params["dest_path"];
        
        bool success = fileManager_->copyFile(sourcePath, destPath);
        
        return json{
            {"success", success},
            {"source_path", sourcePath},
            {"dest_path", destPath}
        };
    });
    
    // files.move
    registerCommand("files.move", [this](const json& params) {
        if (!params.contains("source_path") || !params.contains("dest_path")) {
            throw std::runtime_error("Missing source_path or dest_path parameter");
        }
        
        std::string sourcePath = params["source_path"];
        std::string destPath = params["dest_path"];
        
        bool success = fileManager_->moveFile(sourcePath, destPath);
        
        return json{
            {"success", success},
            {"source_path", sourcePath},
            {"dest_path", destPath}
        };
    });
    
    // files.createDirectory
    registerCommand("files.createDirectory", [this](const json& params) {
        if (!params.contains("directory_path")) {
            throw std::runtime_error("Missing directory_path parameter");
        }
        
        std::string dirPath = params["directory_path"];
        bool success = fileManager_->createDirectory(dirPath);
        
        return json{
            {"success", success},
            {"directory_path", dirPath}
        };
    });
    
    // files.deleteDirectory
    registerCommand("files.deleteDirectory", [this](const json& params) {
        if (!params.contains("directory_path")) {
            throw std::runtime_error("Missing directory_path parameter");
        }
        
        std::string dirPath = params["directory_path"];
        bool recursive = params.value("recursive", false);
        
        bool success = fileManager_->deleteDirectory(dirPath, recursive);
        
        return json{
            {"success", success},
            {"directory_path", dirPath}
        };
    });
    
    // files.exists
    registerCommand("files.exists", [this](const json& params) {
        if (!params.contains("file_path")) {
            throw std::runtime_error("Missing file_path parameter");
        }
        
        std::string filePath = params["file_path"];
        bool exists = fileManager_->fileExists(filePath);
        
        return json{
            {"file_path", filePath},
            {"exists", exists}
        };
    });
    
    // files.validate
    registerCommand("files.validate", [this](const json& params) {
        if (!params.contains("file_path")) {
            throw std::runtime_error("Missing file_path parameter");
        }
        
        std::string filePath = params["file_path"];
        auto validation = fileManager_->validateMidiFile(filePath);
        
        return json{
            {"file_path", filePath},
            {"validation", validation}
        };
    });
    
    // files.getMetadata
    registerCommand("files.getMetadata", [this](const json& params) {
        if (!params.contains("file_path")) {
            throw std::runtime_error("Missing file_path parameter");
        }
        
        std::string filePath = params["file_path"];
        auto metadata = fileManager_->getMetadata(filePath);
        
        return json{
            {"file_path", filePath},
            {"metadata", metadata}
        };
    });
    
    Logger::debug("CommandHandler", "✓ File commands registered (11 commands)");
}

// ============================================================================
// MIDI COMMANDS (11 commands)
// ============================================================================

void CommandHandler::registerMidiCommands() {
    // midi.toJson - Convert MIDI binary to JSON
    registerCommand("midi.toJson", [this](const json& params) {
        if (!params.contains("data") || !params["data"].is_array()) {
            throw std::runtime_error("Missing or invalid 'data' parameter (must be byte array)");
        }
        
        std::vector<uint8_t> midiData;
        for (const auto& byte : params["data"]) {
            if (!byte.is_number_integer() || byte.get<int>() < 0 || byte.get<int>() > 255) {
                throw std::runtime_error("Invalid byte value in MIDI data");
            }
            midiData.push_back(static_cast<uint8_t>(byte.get<int>()));
        }
        
        MidiMessage msg(midiData);
        json result = JsonMidiConverter::midiToJson(msg);
        
        return json{
            {"success", true},
            {"midi_json", result}
        };
    });
    
    // midi.fromJson - Convert JSON to MIDI binary
    registerCommand("midi.fromJson", [this](const json& params) {
        if (!params.contains("midi_json")) {
            throw std::runtime_error("Missing 'midi_json' parameter");
        }
        
        MidiMessage msg = JsonMidiConverter::jsonToMidi(params["midi_json"]);
        const auto& data = msg.getRawData();
        
        json dataArray = json::array();
        for (uint8_t byte : data) {
            dataArray.push_back(static_cast<int>(byte));
        }
        
        return json{
            {"success", true},
            {"data", dataArray}
        };
    });
    
    // midi.validate - Validate MIDI message structure
    registerCommand("midi.validate", [this](const json& params) {
        if (!params.contains("data") || !params["data"].is_array()) {
            throw std::runtime_error("Missing or invalid 'data' parameter");
        }
        
        std::vector<uint8_t> midiData;
        for (const auto& byte : params["data"]) {
            midiData.push_back(static_cast<uint8_t>(byte.get<int>()));
        }
        
        MidiMessage msg(midiData);
        bool isValid = msg.isValid();
        
        return json{
            {"valid", isValid},
            {"message_type", msg.getTypeString()}
        };
    });
    
    // midi.parse - Parse and analyze MIDI message
    registerCommand("midi.parse", [this](const json& params) {
        if (!params.contains("data") || !params["data"].is_array()) {
            throw std::runtime_error("Missing or invalid 'data' parameter");
        }
        
        std::vector<uint8_t> midiData;
        for (const auto& byte : params["data"]) {
            midiData.push_back(static_cast<uint8_t>(byte.get<int>()));
        }
        
        MidiMessage msg(midiData);
        
        return json{
            {"type", msg.getTypeString()},
            {"channel", msg.getChannel()},
            {"data1", msg.getData1()},
            {"data2", msg.getData2()},
            {"raw_data", msg.getRawData()},
            {"is_channel_message", msg.isChannelMessage()},
            {"is_system_message", msg.isSystemMessage()}
        };
    });
    
    // midi.createNoteOn
    registerCommand("midi.createNoteOn", [this](const json& params) {
        if (!params.contains("note") || !params.contains("velocity")) {
            throw std::runtime_error("Missing note or velocity parameter");
        }
        
        uint8_t channel = params.value("channel", 0);
        uint8_t note = params["note"];
        uint8_t velocity = params["velocity"];
        
        std::vector<uint8_t> data = {
            static_cast<uint8_t>(0x90 | (channel & 0x0F)),
            static_cast<uint8_t>(note & 0x7F),
            static_cast<uint8_t>(velocity & 0x7F)
        };
        
        json dataArray = json::array();
        for (uint8_t byte : data) {
            dataArray.push_back(static_cast<int>(byte));
        }
        
        return json{
            {"data", dataArray},
            {"type", "Note On"}
        };
    });
    
    // midi.createNoteOff
    registerCommand("midi.createNoteOff", [this](const json& params) {
        if (!params.contains("note")) {
            throw std::runtime_error("Missing note parameter");
        }
        
        uint8_t channel = params.value("channel", 0);
        uint8_t note = params["note"];
        uint8_t velocity = params.value("velocity", 0);
        
        std::vector<uint8_t> data = {
            static_cast<uint8_t>(0x80 | (channel & 0x0F)),
            static_cast<uint8_t>(note & 0x7F),
            static_cast<uint8_t>(velocity & 0x7F)
        };
        
        json dataArray = json::array();
        for (uint8_t byte : data) {
            dataArray.push_back(static_cast<int>(byte));
        }
        
        return json{
            {"data", dataArray},
            {"type", "Note Off"}
        };
    });
    
    // midi.createControlChange
    registerCommand("midi.createControlChange", [this](const json& params) {
        if (!params.contains("controller") || !params.contains("value")) {
            throw std::runtime_error("Missing controller or value parameter");
        }
        
        uint8_t channel = params.value("channel", 0);
        uint8_t controller = params["controller"];
        uint8_t value = params["value"];
        
        std::vector<uint8_t> data = {
            static_cast<uint8_t>(0xB0 | (channel & 0x0F)),
            static_cast<uint8_t>(controller & 0x7F),
            static_cast<uint8_t>(value & 0x7F)
        };
        
        json dataArray = json::array();
        for (uint8_t byte : data) {
            dataArray.push_back(static_cast<int>(byte));
        }
        
        return json{
            {"data", dataArray},
            {"type", "Control Change"}
        };
    });
    
    // midi.createProgramChange
    registerCommand("midi.createProgramChange", [this](const json& params) {
        if (!params.contains("program")) {
            throw std::runtime_error("Missing program parameter");
        }
        
        uint8_t channel = params.value("channel", 0);
        uint8_t program = params["program"];
        
        std::vector<uint8_t> data = {
            static_cast<uint8_t>(0xC0 | (channel & 0x0F)),
            static_cast<uint8_t>(program & 0x7F)
        };
        
        json dataArray = json::array();
        for (uint8_t byte : data) {
            dataArray.push_back(static_cast<int>(byte));
        }
        
        return json{
            {"data", dataArray},
            {"type", "Program Change"}
        };
    });
    
    // midi.createPitchBend
    registerCommand("midi.createPitchBend", [this](const json& params) {
        if (!params.contains("value")) {
            throw std::runtime_error("Missing value parameter");
        }
        
        uint8_t channel = params.value("channel", 0);
        int value = params["value"];  // -8192 to 8191
        
        // Convert to 14-bit unsigned value
        uint16_t unsignedValue = static_cast<uint16_t>(value + 8192);
        uint8_t lsb = unsignedValue & 0x7F;
        uint8_t msb = (unsignedValue >> 7) & 0x7F;
        
        std::vector<uint8_t> data = {
            static_cast<uint8_t>(0xE0 | (channel & 0x0F)),
            lsb,
            msb
        };
        
        json dataArray = json::array();
        for (uint8_t byte : data) {
            dataArray.push_back(static_cast<int>(byte));
        }
        
        return json{
            {"data", dataArray},
            {"type", "Pitch Bend"}
        };
    });
    
    // midi.sendNoteOn - Envoyer un Note On direct à un device
    registerCommand("midi.sendNoteOn", [this](const json& params) {
        if (!params.contains("device_id") || 
            !params.contains("note") || 
            !params.contains("velocity")) {
            throw std::runtime_error("Missing required parameters: device_id, note, velocity");
        }
        
        std::string deviceId = params["device_id"];
        uint8_t note = params["note"];
        uint8_t velocity = params["velocity"];
        uint8_t channel = params.value("channel", 0);
        
        if (note > 127) {
            throw std::runtime_error("Note must be 0-127");
        }
        if (velocity > 127) {
            throw std::runtime_error("Velocity must be 0-127");
        }
        if (channel > 15) {
            throw std::runtime_error("Channel must be 0-15");
        }
        
        // Créer message MIDI Note On (0x90 + channel)
        std::vector<uint8_t> data = {
            static_cast<uint8_t>(0x90 | channel),
            note,
            velocity
        };
        
        MidiMessage msg(data);
        
        // IMPORTANT: Envoyer directement au device spécifié avec routeTo()
        if (!router_) {
            throw std::runtime_error("Router not available");
        }
        
        router_->routeTo(msg, deviceId);
        
        Logger::debug("CommandHandler", 
            "Sent Note On: device=" + deviceId + 
            ", note=" + std::to_string(note) + 
            ", velocity=" + std::to_string(velocity) +
            ", channel=" + std::to_string(channel));
        
        return json{
            {"success", true},
            {"device_id", deviceId},
            {"note", note},
            {"velocity", velocity},
            {"channel", channel}
        };
    });
    
    // midi.sendNoteOff - Envoyer un Note Off direct à un device
    registerCommand("midi.sendNoteOff", [this](const json& params) {
        if (!params.contains("device_id") || !params.contains("note")) {
            throw std::runtime_error("Missing required parameters: device_id, note");
        }
        
        std::string deviceId = params["device_id"];
        uint8_t note = params["note"];
        uint8_t channel = params.value("channel", 0);
        
        if (note > 127) {
            throw std::runtime_error("Note must be 0-127");
        }
        if (channel > 15) {
            throw std::runtime_error("Channel must be 0-15");
        }
        
        // Créer message MIDI Note Off (0x80 + channel)
        std::vector<uint8_t> data = {
            static_cast<uint8_t>(0x80 | channel),
            note,
            0  // velocity = 0 pour Note Off
        };
        
        MidiMessage msg(data);
        
        // IMPORTANT: Envoyer directement au device spécifié avec routeTo()
        if (!router_) {
            throw std::runtime_error("Router not available");
        }
        
        router_->routeTo(msg, deviceId);
        
        Logger::debug("CommandHandler", 
            "Sent Note Off: device=" + deviceId + 
            ", note=" + std::to_string(note) +
            ", channel=" + std::to_string(channel));
        
        return json{
            {"success", true},
            {"device_id", deviceId},
            {"note", note},
            {"channel", channel}
        };
    });

    Logger::debug("CommandHandler", "✓ MIDI commands registered (11 commands)");
}

// ============================================================================
// PLAYLIST COMMANDS (8 commands)
// ============================================================================

void CommandHandler::registerPlaylistCommands() {
    if (!playlistManager_) {
        Logger::warning("CommandHandler", 
                    "PlaylistManager not available, skipping playlist commands");
        return;
    }
    
    // playlist.create
    registerCommand("playlist.create", [this](const json& params) {
        if (!params.contains("name")) {
            throw std::runtime_error("Missing name parameter");
        }
        
        std::string name = params["name"];
        std::string description = params.value("description", "");
        
        int id = playlistManager_->createPlaylist(name, description);
        
        return json{
            {"success", true},
            {"playlist_id", id},
            {"name", name}
        };
    });
    
    // playlist.delete
    registerCommand("playlist.delete", [this](const json& params) {
        if (!params.contains("playlist_id")) {
            throw std::runtime_error("Missing playlist_id parameter");
        }
        
        int playlistId = params["playlist_id"];
        bool success = playlistManager_->deletePlaylist(playlistId);
        
        return json{
            {"success", success},
            {"playlist_id", playlistId}
        };
    });
    
    // playlist.update
    registerCommand("playlist.update", [this](const json& params) {
        if (!params.contains("playlist_id") || !params.contains("name")) {
            throw std::runtime_error("Missing playlist_id or name parameter");
        }
        
        int playlistId = params["playlist_id"];
        std::string name = params["name"];
        std::string description = params.value("description", "");
        
        bool success = playlistManager_->updatePlaylist(playlistId, name, description);
        
        return json{
            {"success", success},
            {"playlist_id", playlistId}
        };
    });
    
    // playlist.list
    registerCommand("playlist.list", [this](const json& params) {
        auto playlists = playlistManager_->listPlaylists();
        
        json playlistsJson = json::array();
        for (const auto& playlist : playlists) {
            playlistsJson.push_back(playlist.toJson());
        }
        
        return json{
            {"playlists", playlistsJson},
            {"count", playlists.size()}
        };
    });
    
    // playlist.get
    registerCommand("playlist.get", [this](const json& params) {
        if (!params.contains("playlist_id")) {
            throw std::runtime_error("Missing playlist_id parameter");
        }
        
        int playlistId = params["playlist_id"];
        auto playlist = playlistManager_->getPlaylist(playlistId);
        
        return json{
            {"playlist", playlist.toJson()}
        };
    });
    
    // playlist.addItem
    registerCommand("playlist.addItem", [this](const json& params) {
        if (!params.contains("playlist_id") || !params.contains("midi_file_id")) {
            throw std::runtime_error("Missing playlist_id or midi_file_id parameter");
        }
        
        int playlistId = params["playlist_id"];
        int midiFileId = params["midi_file_id"];
        
        bool success = playlistManager_->addItem(playlistId, midiFileId);
        
        return json{
            {"success", success},
            {"playlist_id", playlistId},
            {"midi_file_id", midiFileId}
        };
    });
    
    // playlist.removeItem
    registerCommand("playlist.removeItem", [this](const json& params) {
        if (!params.contains("playlist_id") || !params.contains("item_id")) {
            throw std::runtime_error("Missing playlist_id or item_id parameter");
        }
        
        int playlistId = params["playlist_id"];
        int itemId = params["item_id"];
        
        bool success = playlistManager_->removeItem(playlistId, itemId);
        
        return json{
            {"success", success},
            {"playlist_id", playlistId},
            {"item_id", itemId}
        };
    });
    
    // playlist.reorder
    registerCommand("playlist.reorder", [this](const json& params) {
        if (!params.contains("playlist_id") || !params.contains("item_ids")) {
            throw std::runtime_error("Missing playlist_id or item_ids parameter");
        }
        
        int playlistId = params["playlist_id"];
        
        if (!params["item_ids"].is_array()) {
            throw std::runtime_error("item_ids must be an array");
        }
        
        std::vector<int> itemIds;
        for (const auto& id : params["item_ids"]) {
            itemIds.push_back(id.get<int>());
        }
        
        bool success = playlistManager_->reorderItems(playlistId, itemIds);
        
        return json{
            {"success", success},
            {"playlist_id", playlistId},
            {"item_count", itemIds.size()}
        };
    });
    
    // playlist.setLoop
    registerCommand("playlist.setLoop", [this](const json& params) {
        if (!params.contains("playlist_id") || !params.contains("enabled")) {
            throw std::runtime_error("Missing playlist_id or enabled parameter");
        }
        
        int playlistId = params["playlist_id"];
        bool enabled = params["enabled"];
        
        bool success = playlistManager_->setLoop(playlistId, enabled);
        
        return json{
            {"success", success},
            {"playlist_id", playlistId},
            {"loop", enabled}
        };
    });
    
    Logger::debug("CommandHandler", "✓ Playlist commands registered (8 commands)");
}

// ============================================================================
// SYSTEM COMMANDS (7 commands)
// ============================================================================

void CommandHandler::registerSystemCommands() {
    // system.getInfo
    registerCommand("system.getInfo", [this](const json& params) {
        utsname sysInfo;
        if (uname(&sysInfo) != 0) {
            throw std::runtime_error("Failed to get system information");
        }
        
        return json{
            {"system", sysInfo.sysname},
            {"node", sysInfo.nodename},
            {"release", sysInfo.release},
            {"version", sysInfo.version},
            {"machine", sysInfo.machine}
        };
    });
    
    // system.getCpuInfo
    registerCommand("system.getCpuInfo", [this](const json& params) {
        std::ifstream cpuinfo("/proc/cpuinfo");
        if (!cpuinfo.is_open()) {
            throw std::runtime_error("Failed to open /proc/cpuinfo");
        }
        
        std::string line;
        std::string model;
        int cores = 0;
        
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    model = line.substr(pos + 2);
                }
            } else if (line.find("processor") != std::string::npos) {
                cores++;
            }
        }
        
        return json{
            {"model", model},
            {"cores", cores}
        };
    });
    
    // system.getMemoryInfo
    registerCommand("system.getMemoryInfo", [this](const json& params) {
        std::ifstream meminfo("/proc/meminfo");
        if (!meminfo.is_open()) {
            throw std::runtime_error("Failed to open /proc/meminfo");
        }
        
        std::string line;
        uint64_t totalMem = 0;
        uint64_t freeMem = 0;
        uint64_t availMem = 0;
        
        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            std::string unit;
            
            if (iss >> key >> value >> unit) {
                if (key == "MemTotal:") totalMem = value;
                else if (key == "MemFree:") freeMem = value;
                else if (key == "MemAvailable:") availMem = value;
            }
        }
        
        return json{
            {"total_kb", totalMem},
            {"free_kb", freeMem},
            {"available_kb", availMem},
            {"used_kb", totalMem - freeMem}
        };
    });
    
    // system.getDiskInfo
    registerCommand("system.getDiskInfo", [this](const json& params) {
        std::string path = params.value("path", "/");
        
        struct statvfs stat;
        if (statvfs(path.c_str(), &stat) != 0) {
            throw std::runtime_error("Failed to get disk information");
        }
        
        uint64_t total = stat.f_blocks * stat.f_frsize;
        uint64_t available = stat.f_bavail * stat.f_frsize;
        uint64_t free = stat.f_bfree * stat.f_frsize;
        uint64_t used = total - free;
        
        return json{
            {"path", path},
            {"total_bytes", total},
            {"available_bytes", available},
            {"free_bytes", free},
            {"used_bytes", used}
        };
    });
    
    // system.getUptime
    registerCommand("system.getUptime", [this](const json& params) {
        std::ifstream uptime("/proc/uptime");
        if (!uptime.is_open()) {
            throw std::runtime_error("Failed to open /proc/uptime");
        }
        
        double uptimeSeconds;
        uptime >> uptimeSeconds;
        
        return json{
            {"uptime_seconds", static_cast<uint64_t>(uptimeSeconds)}
        };
    });
    
    // system.getLoadAverage
    registerCommand("system.getLoadAverage", [this](const json& params) {
        std::ifstream loadavg("/proc/loadavg");
        if (!loadavg.is_open()) {
            throw std::runtime_error("Failed to open /proc/loadavg");
        }
        
        double load1, load5, load15;
        loadavg >> load1 >> load5 >> load15;
        
        return json{
            {"load_1min", load1},
            {"load_5min", load5},
            {"load_15min", load15}
        };
    });
    
    // system.getProcessInfo
    registerCommand("system.getProcessInfo", [this](const json& params) {
        pid_t pid = getpid();
        
        std::string statPath = "/proc/" + std::to_string(pid) + "/stat";
        std::ifstream stat(statPath);
        
        if (!stat.is_open()) {
            throw std::runtime_error("Failed to open process stat file");
        }
        
        std::string line;
        std::getline(stat, line);
        
        return json{
            {"pid", pid},
            {"stat", line}
        };
    });
    
    Logger::debug("CommandHandler", "✓ System commands registered (7 commands)");
}

// ============================================================================
// NETWORK COMMANDS (2 commands)
// ============================================================================

void CommandHandler::registerNetworkCommands() {
    // network.getInterfaces
    registerCommand("network.getInterfaces", [this](const json& params) {
        // This would require platform-specific code
        // Placeholder implementation
        return json{
            {"interfaces", json::array()}
        };
    });
    
    // network.ping
    registerCommand("network.ping", [this](const json& params) {
        if (!params.contains("host")) {
            throw std::runtime_error("Missing host parameter");
        }
        
        std::string host = params["host"];
        
        // Simple implementation - would need enhancement for production
        std::string command = "ping -c 1 -W 1 " + host + " > /dev/null 2>&1";
        int result = system(command.c_str());
        
        return json{
            {"host", host},
            {"reachable", result == 0}
        };
    });
    
    Logger::debug("CommandHandler", "✓ Network commands registered (2 commands)");
}

// ============================================================================
// LOGGER COMMANDS (5 commands)
// ============================================================================

void CommandHandler::registerLoggerCommands() {
    // logger.setLevel
    registerCommand("logger.setLevel", [this](const json& params) {
        if (!params.contains("level")) {
            throw std::runtime_error("Missing level parameter");
        }
        
        std::string level = params["level"];
        Logger::setGlobalLevel(level);
        
        return json{
            {"success", true},
            {"level", level}
        };
    });
    
    // logger.getLevel
    registerCommand("logger.getLevel", [this](const json& params) {
        std::string level = Logger::getGlobalLevel();
        
        return json{{"level", level}};
    });
    
    // logger.setModuleLevel
    registerCommand("logger.setModuleLevel", [this](const json& params) {
        if (!params.contains("module") || !params.contains("level")) {
            throw std::runtime_error("Missing module or level parameter");
        }
        
        std::string module = params["module"];
        std::string level = params["level"];
        
        Logger::setModuleLevel(module, level);
        
        return json{
            {"success", true},
            {"module", module},
            {"level", level}
        };
    });
    
    // logger.getModuleLevels
    registerCommand("logger.getModuleLevels", [this](const json& params) {
        auto levels = Logger::getModuleLevels();
        
        return json{{"module_levels", levels}};
    });
    
    // logger.getLogs
    registerCommand("logger.getLogs", [this](const json& params) {
        int count = params.value("count", 100);
        std::string minLevel = params.value("min_level", "DEBUG");
        
        auto logs = Logger::getRecentLogs(count, minLevel);
        
        json logsJson = json::array();
        for (const auto& log : logs) {
            logsJson.push_back(log);
        }
        
        return json{
            {"logs", logsJson},
            {"count", logs.size()}
        };
    });
    
    Logger::debug("CommandHandler", "✓ Logger commands registered (5 commands)");
}

// ============================================================================
// LATENCY COMMANDS (7 commands)
// ============================================================================

void CommandHandler::registerLatencyCommands() {
    if (!compensator_) {
        Logger::warning("CommandHandler", 
                    "LatencyCompensator not available, skipping latency commands");
        return;
    }
    
    // latency.setInstrumentLatency
    registerCommand("latency.setInstrumentLatency", [this](const json& params) {
        if (!params.contains("instrument_id") || !params.contains("latency_ms")) {
            throw std::runtime_error("Missing instrument_id or latency_ms parameter");
        }
        
        std::string instrumentId = params["instrument_id"];
        int latencyMs = params["latency_ms"];
        
        compensator_->setInstrumentLatency(instrumentId, latencyMs);
        
        return json{
            {"success", true},
            {"instrument_id", instrumentId},
            {"latency_ms", latencyMs}
        };
    });
    
    // latency.getInstrumentLatency
    registerCommand("latency.getInstrumentLatency", [this](const json& params) {
        if (!params.contains("instrument_id")) {
            throw std::runtime_error("Missing instrument_id parameter");
        }
        
        std::string instrumentId = params["instrument_id"];
        int latencyMs = compensator_->getInstrumentLatency(instrumentId);
        
        return json{
            {"instrument_id", instrumentId},
            {"latency_ms", latencyMs}
        };
    });
    
    // latency.getAllLatencies
    registerCommand("latency.getAllLatencies", [this](const json& params) {
        auto latencies = compensator_->getAllLatencies();
        
        return json{{"latencies", latencies}};
    });
    
    // latency.clearInstrumentLatency
    registerCommand("latency.clearInstrumentLatency", [this](const json& params) {
        if (!params.contains("instrument_id")) {
            throw std::runtime_error("Missing instrument_id parameter");
        }
        
        std::string instrumentId = params["instrument_id"];
        compensator_->clearInstrumentLatency(instrumentId);
        
        return json{
            {"success", true},
            {"instrument_id", instrumentId}
        };
    });
    
    // latency.clearAllLatencies
    registerCommand("latency.clearAllLatencies", [this](const json& params) {
        compensator_->clearAllLatencies();
        
        return json{{"success", true}};
    });
    
    // latency.enable
    registerCommand("latency.enable", [this](const json& params) {
        if (!params.contains("enabled")) {
            throw std::runtime_error("Missing enabled parameter");
        }
        
        bool enabled = params["enabled"];
        compensator_->setEnabled(enabled);
        
        return json{
            {"success", true},
            {"enabled", enabled}
        };
    });
    
    // latency.getStatus
    registerCommand("latency.getStatus", [this](const json& params) {
        bool enabled = compensator_->isEnabled();
        auto latencies = compensator_->getAllLatencies();
        
        return json{
            {"enabled", enabled},
            {"instrument_count", latencies.size()}
        };
    });
    
    Logger::debug("CommandHandler", "✓ Latency commands registered (7 commands)");
}

// ============================================================================
// PRESET COMMANDS (7 commands)
// ============================================================================

void CommandHandler::registerPresetCommands() {
    if (!presetManager_) {
        Logger::warning("CommandHandler", 
                    "PresetManager not available, skipping preset commands");
        return;
    }
    
    // presets.save
    registerCommand("presets.save", [this](const json& params) {
        if (!params.contains("name") || !params.contains("data")) {
            throw std::runtime_error("Missing name or data parameter");
        }
        
        std::string name = params["name"];
        json data = params["data"];
        std::string description = params.value("description", "");
        
        bool success = presetManager_->savePreset(name, data, description);
        
        return json{
            {"success", success},
            {"name", name}
        };
    });
    
    // presets.load
    registerCommand("presets.load", [this](const json& params) {
        if (!params.contains("name")) {
            throw std::runtime_error("Missing name parameter");
        }
        
        std::string name = params["name"];
        auto preset = presetManager_->loadPreset(name);
        
        return json{
            {"name", name},
            {"data", preset}
        };
    });
    
    // presets.list
    registerCommand("presets.list", [this](const json& params) {
        auto presets = presetManager_->listPresets();
        
        json presetsJson = json::array();
        for (const auto& preset : presets) {
            presetsJson.push_back(preset);
        }
        
        return json{
            {"presets", presetsJson},
            {"count", presets.size()}
        };
    });
    
    // presets.delete
    registerCommand("presets.delete", [this](const json& params) {
        if (!params.contains("name")) {
            throw std::runtime_error("Missing name parameter");
        }
        
        std::string name = params["name"];
        bool success = presetManager_->deletePreset(name);
        
        return json{
            {"success", success},
            {"name", name}
        };
    });
    
    // presets.rename
    registerCommand("presets.rename", [this](const json& params) {
        if (!params.contains("old_name") || !params.contains("new_name")) {
            throw std::runtime_error("Missing old_name or new_name parameter");
        }
        
        std::string oldName = params["old_name"];
        std::string newName = params["new_name"];
        
        bool success = presetManager_->renamePreset(oldName, newName);
        
        return json{
            {"success", success},
            {"old_name", oldName},
            {"new_name", newName}
        };
    });
    
    // presets.update
    registerCommand("presets.update", [this](const json& params) {
        if (!params.contains("name") || !params.contains("data")) {
            throw std::runtime_error("Missing name or data parameter");
        }
        
        std::string name = params["name"];
        json data = params["data"];
        std::string description = params.value("description", "");
        
        bool success = presetManager_->updatePreset(name, data, description);
        
        return json{
            {"success", success},
            {"name", name}
        };
    });
    
    // presets.export
    registerCommand("presets.export", [this](const json& params) {
        if (!params.contains("name") || !params.contains("file_path")) {
            throw std::runtime_error("Missing name or file_path parameter");
        }
        
        std::string name = params["name"];
        std::string filePath = params["file_path"];
        
        bool success = presetManager_->exportPreset(name, filePath);
        
        return json{
            {"success", success},
            {"name", name},
            {"file_path", filePath}
        };
    });
    
    Logger::debug("CommandHandler", "✓ Preset commands registered (7 commands)");
}

// ============================================================================
// HELPER METHODS
// ============================================================================

json CommandHandler::createSuccessResponse(const json& data) const {
    return json{
        {"success", true},
        {"data", data}
    };
}

json CommandHandler::createErrorResponse(const std::string& error, 
                                        const std::string& errorCode) const {
    return json{
        {"success", false},
        {"error", error},
        {"error_code", errorCode}
    };
}

bool CommandHandler::validateCommand(const json& command, std::string& error) const {
    if (!command.is_object()) {
        error = "Command must be a JSON object";
        return false;
    }
    
    if (!command.contains("command")) {
        error = "Missing 'command' field";
        return false;
    }
    
    if (!command["command"].is_string()) {
        error = "'command' field must be a string";
        return false;
    }
    
    if (command.contains("params") && !command["params"].is_object()) {
        error = "'params' field must be an object";
        return false;
    }
    
    return true;
}


}
// ============================================================================
// END OF FILE CommandHandler.cpp
// ============================================================================
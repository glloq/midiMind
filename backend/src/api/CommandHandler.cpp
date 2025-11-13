// ============================================================================
// File: backend/src/api/CommandHandler.cpp
// Version: 4.2.3
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================


// Changes v4.2.3:
//   - FIXED: Removed double wrapping - commands return raw data
//   - ApiServer now handles response envelope creation
//   - Removed createSuccessResponse, createErrorResponse, validateCommand
//
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
                "âœ“ CommandHandler initialized (" + 
                std::to_string(commands_.size()) + " commands)");
}


CommandHandler::~CommandHandler() {
    Logger::info("CommandHandler", "CommandHandler destroyed");
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================

json CommandHandler::processCommand(const json& command) {
    // Validate command structure
    if (!command.is_object()) {
        throw std::runtime_error("Command must be a JSON object");
    }
    
    if (!command.contains("command") || !command["command"].is_string()) {
        throw std::runtime_error("Missing or invalid 'command' field");
    }
    
    std::string commandName = command["command"];
    json params = command.value("params", json::object());
    
    // Find and copy command function under lock
    CommandFunction func;
    {
        std::lock_guard<std::mutex> lock(commandsMutex_);
        
        auto it = commands_.find(commandName);
        if (it == commands_.end()) {
            throw std::runtime_error("Unknown command: " + commandName);
        }
        
        func = it->second;
    }
    
    // Execute command - returns raw data (ApiServer wraps in response envelope)
    return func(params);
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
        for (const auto& deviceInfo : devices) {
            devicesJson.push_back({
                {"id", deviceInfo.id},
                {"name", deviceInfo.name},
                {"type", static_cast<int>(deviceInfo.type)},
                {"status", static_cast<int>(deviceInfo.status)},
                {"available", deviceInfo.available}
            });
        }
        
        return json{{"devices", devicesJson}};
    });
    
    // devices.scan
    registerCommand("devices.scan", [this](const json& params) {
        bool fullScan = params.value("full_scan", false);
        auto devices = deviceManager_->discoverDevices(fullScan);
        
        json devicesJson = json::array();
        for (const auto& deviceInfo : devices) {
            devicesJson.push_back({
                {"id", deviceInfo.id},
                {"name", deviceInfo.name},
                {"type", static_cast<int>(deviceInfo.type)},
                {"status", static_cast<int>(deviceInfo.status)},
                {"available", deviceInfo.available}
            });
        }
        
        return json{
            {"devices", devicesJson},
            {"count", devicesJson.size()}
        };
    });
    
    // devices.connect
    registerCommand("devices.connect", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        bool success = deviceManager_->connect(deviceId);
        
        return json{
            {"connected", success},
            {"device_id", deviceId}
        };
    });
    
    // devices.disconnect
    registerCommand("devices.disconnect", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        deviceManager_->disconnect(deviceId);
        
        return json{
            {"disconnected", true},
            {"device_id", deviceId}
        };
    });
    
    // devices.disconnectAll
    registerCommand("devices.disconnectAll", [this](const json& params) {
        deviceManager_->disconnectAll();
        return json{{"disconnected_all", true}};
    });
    
    // devices.getInfo
    registerCommand("devices.getInfo", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        auto device = deviceManager_->getDevice(deviceId);
        
        if (!device) {
            throw std::runtime_error("Device not found: " + deviceId);
        }
        
        return json{
            {"id", device->getId()},
            {"name", device->getName()},
            {"type", static_cast<int>(device->getType())},
            {"status", static_cast<int>(device->getStatus())},
            {"available", device->isConnected()}
        };
    });
    
    // devices.getConnected
    registerCommand("devices.getConnected", [this](const json& params) {
        auto devices = deviceManager_->getConnectedDevices();
        
        json devicesJson = json::array();
        for (const auto& device : devices) {
            devicesJson.push_back({
                {"id", device->getId()},
                {"name", device->getName()},
                {"type", static_cast<int>(device->getType())},
                {"status", static_cast<int>(device->getStatus())}
            });
        }
        
        return json{
            {"devices", devicesJson},
            {"count", devicesJson.size()}
        };
    });
    
    // devices.startHotPlug
    registerCommand("devices.startHotPlug", [this](const json& params) {
        int intervalMs = params.value("interval_ms", 2000);
        deviceManager_->startHotPlugMonitoring(intervalMs);
        
        return json{
            {"hot_plug_started", true},
            {"interval_ms", intervalMs}
        };
    });
    
    // devices.stopHotPlug
    registerCommand("devices.stopHotPlug", [this](const json& params) {
        deviceManager_->stopHotPlugMonitoring();
        return json{{"hot_plug_stopped", true}};
    });
    
    // devices.getHotPlugStatus
    registerCommand("devices.getHotPlugStatus", [this](const json& params) {
        bool active = deviceManager_->isHotPlugMonitoringActive();
        
        return json{
            {"active", active}
        };
    });
    
    // bluetooth.config
    registerCommand("bluetooth.config", [this](const json& params) {
        bool enabled = params.value("enabled", true);
        int timeout = params.value("scan_timeout", 5);
        
        deviceManager_->setBluetoothEnabled(enabled);
        deviceManager_->setBluetoothScanTimeout(timeout);
        
        return json{
            {"enabled", enabled},
            {"scan_timeout", timeout}
        };
    });
    
    // bluetooth.status
    registerCommand("bluetooth.status", [this](const json& params) {
        bool enabled = deviceManager_->isBluetoothEnabled();
        
        return json{
            {"enabled", enabled}
        };
    });
    
    // bluetooth.scan
    registerCommand("bluetooth.scan", [this](const json& params) {
        int duration = params.value("duration", 5);
        std::string filter = params.value("filter", "");
        
        auto devices = deviceManager_->scanBleDevices(duration, filter);
        
        json devicesJson = json::array();
        for (const auto& deviceInfo : devices) {
            devicesJson.push_back({
                {"id", deviceInfo.id},
                {"name", deviceInfo.name},
                {"address", deviceInfo.bluetoothAddress},
                {"paired", deviceInfo.paired},
                {"signal", deviceInfo.signalStrength},
                {"available", deviceInfo.available}
            });
        }
        
        return json{
            {"devices", devicesJson},
            {"count", devicesJson.size()},
            {"duration", duration}
        };
    });
    
    // bluetooth.pair
    registerCommand("bluetooth.pair", [this](const json& params) {
        if (!params.contains("address")) {
            throw std::runtime_error("Missing address parameter");
        }
        
        std::string address = params["address"];
        std::string pin = params.value("pin", "");
        
        bool success = deviceManager_->pairBleDevice(address, pin);
        
        return json{
            {"paired", success},
            {"address", address}
        };
    });
    
    // bluetooth.unpair
    registerCommand("bluetooth.unpair", [this](const json& params) {
        if (!params.contains("address")) {
            throw std::runtime_error("Missing address parameter");
        }
        
        std::string address = params["address"];
        bool success = deviceManager_->unpairBleDevice(address);
        
        return json{
            {"unpaired", success},
            {"address", address}
        };
    });
    
    // bluetooth.paired
    registerCommand("bluetooth.paired", [this](const json& params) {
        auto devices = deviceManager_->getPairedBleDevices();
        
        json devicesJson = json::array();
        for (const auto& deviceInfo : devices) {
            devicesJson.push_back({
                {"id", deviceInfo.id},
                {"name", deviceInfo.name},
                {"address", deviceInfo.bluetoothAddress},
                {"signal", deviceInfo.signalStrength},
                {"available", deviceInfo.available}
            });
        }
        
        return json{
            {"devices", devicesJson},
            {"count", devicesJson.size()}
        };
    });
    
    // bluetooth.forget
    registerCommand("bluetooth.forget", [this](const json& params) {
        if (!params.contains("address")) {
            throw std::runtime_error("Missing address parameter");
        }
        
        std::string address = params["address"];
        bool success = deviceManager_->forgetBleDevice(address);
        
        return json{
            {"forgotten", success},
            {"address", address}
        };
    });
    
    // bluetooth.signal
    registerCommand("bluetooth.signal", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        int rssi = deviceManager_->getBleDeviceSignal(deviceId);
        
        return json{
            {"device_id", deviceId},
            {"rssi", rssi},
            {"signal_quality", rssi > -70 ? "good" : rssi > -85 ? "fair" : "poor"}
        };
    });
    
    Logger::debug("CommandHandler", "Ã¢Å“â€œ Device commands registered (18 commands)");  
}

// ============================================================================
// ROUTING COMMANDS (6 commands)
// ============================================================================

void CommandHandler::registerRoutingCommands() {
    if (!router_) {
        Logger::warning("CommandHandler", 
                    "Router not available, skipping routing commands");
        return;
    }
    
    // routing.addRoute
    registerCommand("routing.addRoute", [this](const json& params) {
        if (!params.contains("source_id") || !params.contains("destination_id")) {
            throw std::runtime_error("Missing source_id or destination_id");
        }
        
        std::string sourceId = params["source_id"];
        std::string destId = params["destination_id"];
        
        bool success = router_->addRoute(sourceId, destId);
        
        return json{
            {"added", success},
            {"source_id", sourceId},
            {"destination_id", destId}
        };
    });
    
    // routing.removeRoute
    registerCommand("routing.removeRoute", [this](const json& params) {
        if (!params.contains("source_id") || !params.contains("destination_id")) {
            throw std::runtime_error("Missing source_id or destination_id");
        }
        
        std::string sourceId = params["source_id"];
        std::string destId = params["destination_id"];
        
        bool success = router_->removeRoute(sourceId, destId);
        
        return json{
            {"removed", success},
            {"source_id", sourceId},
            {"destination_id", destId}
        };
    });
    
    // routing.clearRoutes
    registerCommand("routing.clearRoutes", [this](const json& params) {
        router_->clearRoutes();
        return json{{"cleared", true}};
    });
    
    // routing.listRoutes
    registerCommand("routing.listRoutes", [this](const json& params) {
        auto routes = router_->getRoutes();
        
        json routesJson = json::array();
        for (const auto& route : routes) {
            json routeObj = {
                {"source_id", route->sourceDeviceId},
                {"destination_id", route->destinationDeviceId},
                {"enabled", route->enabled}
            };
            routesJson.push_back(routeObj);
        }
        
        return json{
            {"routes", routesJson},
            {"count", routesJson.size()}
        };
    });
    
    // routing.enableRoute
    registerCommand("routing.enableRoute", [this](const json& params) {
        if (!params.contains("source_id") || !params.contains("destination_id")) {
            throw std::runtime_error("Missing source_id or destination_id");
        }
        
        std::string sourceId = params["source_id"];
        std::string destId = params["destination_id"];
        
        bool success = router_->enableRoute(sourceId, destId);
        
        return json{
            {"enabled", success},
            {"source_id", sourceId},
            {"destination_id", destId}
        };
    });
    
    // routing.disableRoute
    registerCommand("routing.disableRoute", [this](const json& params) {
        if (!params.contains("source_id") || !params.contains("destination_id")) {
            throw std::runtime_error("Missing source_id or destination_id");
        }
        
        std::string sourceId = params["source_id"];
        std::string destId = params["destination_id"];
        
        bool success = router_->disableRoute(sourceId, destId);
        
        return json{
            {"disabled", success},
            {"source_id", sourceId},
            {"destination_id", destId}
        };
    });
    
    Logger::debug("CommandHandler", "Ã¢Å“â€œ Routing commands registered (6 commands)");
}

// ============================================================================
// PLAYBACK COMMANDS (10 commands)
// ============================================================================

void CommandHandler::registerPlaybackCommands() {
    if (!player_) {
        Logger::warning("CommandHandler", 
                    "Player not available, skipping playback commands");
        return;
    }
    
    // playback.load
    registerCommand("playback.load", [this](const json& params) {
        if (!params.contains("filename")) {
            throw std::runtime_error("Missing filename parameter");
        }
        
        std::string filename = params["filename"];
        bool success = player_->load(filename);
        
        return json{
            {"loaded", success},
            {"filename", filename}
        };
    });
    
    // playback.play
    registerCommand("playback.play", [this](const json& params) {
        bool success = player_->play();
        
        return json{
            {"playing", success}
        };
    });
    
    // playback.pause
    registerCommand("playback.pause", [this](const json& params) {
        player_->pause();
        
        return json{
            {"paused", true}
        };
    });
    
    // playback.stop
    registerCommand("playback.stop", [this](const json& params) {
        player_->stop();
        
        return json{
            {"stopped", true}
        };
    });
    
    // playback.getStatus
    registerCommand("playback.getStatus", [this](const json& params) {
        auto state = player_->getState();
        
        return json{
            {"state", static_cast<int>(state)},
            {"current_time", player_->getCurrentPosition()},
            {"duration", player_->getDuration()},
            {"tempo", player_->getTempo()},
            {"filename", player_->getCurrentFile()}
        };
    });
    
    // playback.seek
    registerCommand("playback.seek", [this](const json& params) {
        if (!params.contains("position")) {
            throw std::runtime_error("Missing position parameter");
        }
        
        double position = params["position"];
        player_->seek(position);
        
        return json{
            {"seeked", true},
            {"position", position}
        };
    });
    
    // playback.setTempo
    registerCommand("playback.setTempo", [this](const json& params) {
        if (!params.contains("tempo")) {
            throw std::runtime_error("Missing tempo parameter");
        }
        
        double tempo = params["tempo"];
        player_->setTempo(tempo);
        
        return json{
            {"tempo", tempo}
        };
    });
    
    // playback.setLoop
    registerCommand("playback.setLoop", [this](const json& params) {
        if (!params.contains("enabled")) {
            throw std::runtime_error("Missing enabled parameter");
        }
        
        bool enabled = params["enabled"];
        player_->setLoop(enabled);
        
        return json{
            {"loop_enabled", enabled}
        };
    });
    
    // playback.getInfo
    registerCommand("playback.getInfo", [this](const json& params) {
        json metadata = player_->getMetadata();
        
        return json{
            {"filename", player_->getCurrentFile()},
            {"duration", player_->getDuration()},
            {"tempo", player_->getTempo()},
            {"metadata", metadata}
        };
    });
    
    // playback.listFiles
    registerCommand("playback.listFiles", [this](const json& params) {
        auto fileInfos = fileManager_->listFiles();  // List all files
        
        json filesJson = json::array();
        for (const auto& info : fileInfos) {
            filesJson.push_back(info.toJson());
        }
        
        return json{
            {"files", filesJson},
            {"count", filesJson.size()}
        };
    });
    
    Logger::debug("CommandHandler", "Ã¢Å“â€œ Playback commands registered (10 commands)");
}

// ============================================================================
// FILE COMMANDS (6 commands)
// ============================================================================

void CommandHandler::registerFileCommands() {
    if (!fileManager_) {
        Logger::warning("CommandHandler", 
                    "FileManager not available, skipping file commands");
        return;
    }
    
    // files.list
    registerCommand("files.list", [this](const json& params) {
        auto fileInfos = fileManager_->listFiles();  // List all files
        
        json filesJson = json::array();
        for (const auto& info : fileInfos) {
            filesJson.push_back(info.toJson());
        }
        
        return json{
            {"files", filesJson},
            {"count", filesJson.size()}
        };
    });
    
    // files.read
    registerCommand("files.read", [this](const json& params) {
        if (!params.contains("filename")) {
            throw std::runtime_error("Missing filename parameter");
        }
        
        std::string filename = params["filename"];
        auto data = fileManager_->downloadFile(filename);
        
        // Convert binary data to base64 string for JSON
        std::string content(data.begin(), data.end());
        
        return json{
            {"filename", filename},
            {"content", content},
            {"size", data.size()}
        };
    });
    
  // files.write - PHASE 1: Support Base64 pour fichiers binaires
    registerCommand("files.write", [this](const json& params) {
        if (!params.contains("filename") || !params.contains("content")) {
            throw std::runtime_error("Missing filename or content parameter");
        }
        
        std::string filename = params["filename"];
        std::string content = params["content"];
        bool isBase64 = params.value("base64", true);  // DÃƒÂ©faut: Base64 activÃƒÂ©
        
        std::vector<uint8_t> data;
        
        if (isBase64) {
            // DÃƒÂ©coder Base64 pour fichiers binaires (MIDI, etc.)
            try {
                data = base64Decode(content);
                Logger::debug("CommandHandler", 
                    "Decoded Base64: " + std::to_string(data.size()) + " bytes");
            } catch (const std::exception& e) {
                throw std::runtime_error("Invalid Base64 data: " + std::string(e.what()));
            }
        } else {
            // Conversion directe pour fichiers texte
            data = std::vector<uint8_t>(content.begin(), content.end());
            Logger::debug("CommandHandler", 
                "Text mode: " + std::to_string(data.size()) + " bytes");
        }
        
        // Validation de la taille
        if (data.empty()) {
            throw std::runtime_error("File content is empty");
        }
        
        // Upload avec validation
        std::string filepath = fileManager_->uploadFile(
            data, filename, DirectoryType::UPLOADS, true
        );
        
        if (filepath.empty()) {
            throw std::runtime_error("Failed to upload file");
        }
        
        Logger::info("CommandHandler", 
            "Ã¢Å“â€œ File uploaded: " + filename + " (" + std::to_string(data.size()) + " bytes)");
        
        return json{
            {"success", true},
            {"filename", filename},
            {"filepath", filepath},
            {"size", data.size()},
            {"base64", isBase64}
        };
    });

    // files.delete
    registerCommand("files.delete", [this](const json& params) {
        if (!params.contains("filename")) {
            throw std::runtime_error("Missing filename parameter");
        }
        
        std::string filename = params["filename"];
        bool success = fileManager_->deleteFile(filename);
        
        return json{
            {"deleted", success},
            {"filename", filename}
        };
    });
    
    // files.exists
    registerCommand("files.exists", [this](const json& params) {
        if (!params.contains("filename")) {
            throw std::runtime_error("Missing filename parameter");
        }
        
        std::string filename = params["filename"];
        auto infoOpt = fileManager_->getFileInfo(filename);
        bool exists = infoOpt.has_value();
        
        return json{
            {"exists", exists},
            {"filename", filename}
        };
    });
    
    // files.getInfo
    registerCommand("files.getInfo", [this](const json& params) {
        if (!params.contains("filename")) {
            throw std::runtime_error("Missing filename parameter");
        }
        
        std::string filename = params["filename"];
        auto infoOpt = fileManager_->getFileInfo(filename);
        
        if (!infoOpt) {
            throw std::runtime_error("File not found: " + filename);
        }
        
        return infoOpt->toJson();
    });
    
    Logger::debug("CommandHandler", "Ã¢Å“â€œ File commands registered (6 commands)");
}

// ============================================================================
// SYSTEM COMMANDS (7 commands)
// ============================================================================

void CommandHandler::registerSystemCommands() {
    // system.ping
    registerCommand("system.ping", [this](const json& params) {
        return json{{"pong", true}};
    });
    
    // system.version
    registerCommand("system.version", [this](const json& params) {
        return json{
            {"version", "4.2.2"},
            {"name", "MidiMind"}
        };
    });
    
    // system.info
    registerCommand("system.info", [this](const json& params) {
        struct utsname systemInfo;
        uname(&systemInfo);
        
        return json{
            {"system", systemInfo.sysname},
            {"node", systemInfo.nodename},
            {"release", systemInfo.release},
            {"version", systemInfo.version},
            {"machine", systemInfo.machine}
        };
    });
    
    // system.uptime
    registerCommand("system.uptime", [this](const json& params) {
        std::ifstream uptimeFile("/proc/uptime");
        double uptime = 0.0;
        
        if (uptimeFile.is_open()) {
            uptimeFile >> uptime;
            uptimeFile.close();
        }
        
        return json{
            {"uptime_seconds", uptime}
        };
    });
    
    // system.memory
    registerCommand("system.memory", [this](const json& params) {
        std::ifstream meminfoFile("/proc/meminfo");
        std::string line;
        long totalMem = 0;
        long availMem = 0;
        
        if (meminfoFile.is_open()) {
            while (std::getline(meminfoFile, line)) {
                if (line.find("MemTotal:") == 0) {
                    std::istringstream iss(line);
                    std::string label;
                    iss >> label >> totalMem;
                }
                else if (line.find("MemAvailable:") == 0) {
                    std::istringstream iss(line);
                    std::string label;
                    iss >> label >> availMem;
                }
            }
            meminfoFile.close();
        }
        
        return json{
            {"total_kb", totalMem},
            {"available_kb", availMem},
            {"used_kb", totalMem - availMem}
        };
    });
    
    // system.disk
    registerCommand("system.disk", [this](const json& params) {
        struct statvfs stat;
        
        if (statvfs("/", &stat) != 0) {
            throw std::runtime_error("Failed to get disk information");
        }
        
        unsigned long long totalSpace = stat.f_blocks * stat.f_frsize;
        unsigned long long freeSpace = stat.f_bfree * stat.f_frsize;
        unsigned long long availSpace = stat.f_bavail * stat.f_frsize;
        
        return json{
            {"total_bytes", totalSpace},
            {"free_bytes", freeSpace},
            {"available_bytes", availSpace},
            {"used_bytes", totalSpace - freeSpace}
        };
    });
    
    // system.commands
    registerCommand("system.commands", [this](const json& params) {
        auto commands = listCommands();
        auto categories = listCommandsByCategory();
        
        return json{
            {"commands", commands},
            {"count", commands.size()},
            {"categories", categories}
        };
    });
    
    Logger::debug("CommandHandler", "Ã¢Å“â€œ System commands registered (7 commands)");
}

// ============================================================================
// NETWORK COMMANDS (3 commands)
// ============================================================================

void CommandHandler::registerNetworkCommands() {
    // network.status
    registerCommand("network.status", [this](const json& params) {
        return json{
            {"connected", true},
            {"type", "websocket"}
        };
    });
    
    // network.interfaces
    registerCommand("network.interfaces", [this](const json& params) {
        std::ifstream routeFile("/proc/net/route");
        std::string line;
        json interfaces = json::array();
        
        if (routeFile.is_open()) {
            std::getline(routeFile, line); // Skip header
            
            while (std::getline(routeFile, line)) {
                std::istringstream iss(line);
                std::string iface;
                iss >> iface;
                
                if (std::find_if(interfaces.begin(), interfaces.end(), 
                    [&iface](const json& j) { return j["name"] == iface; }) == interfaces.end()) {
                    interfaces.push_back({{"name", iface}});
                }
            }
            routeFile.close();
        }
        
        return json{
            {"interfaces", interfaces},
            {"count", interfaces.size()}
        };
    });
    
    // network.stats
    registerCommand("network.stats", [this](const json& params) {
        return json{
            {"active_connections", 1},
            {"messages_sent", 0},
            {"messages_received", 0}
        };
    });
    
    Logger::debug("CommandHandler", "Ã¢Å“â€œ Network commands registered (3 commands)");
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
        
        std::string levelStr = params["level"];
        
        // Convert string to Logger::Level
        Logger::Level level;
        if (levelStr == "DEBUG" || levelStr == "debug" || levelStr == "0") {
            level = Logger::Level::DEBUG;
        } else if (levelStr == "INFO" || levelStr == "info" || levelStr == "1") {
            level = Logger::Level::INFO;
        } else if (levelStr == "WARNING" || levelStr == "warning" || levelStr == "2") {
            level = Logger::Level::WARNING;
        } else if (levelStr == "ERROR" || levelStr == "error" || levelStr == "3") {
            level = Logger::Level::ERROR;
        } else if (levelStr == "CRITICAL" || levelStr == "critical" || levelStr == "4") {
            level = Logger::Level::CRITICAL;
        } else {
            throw std::runtime_error("Invalid log level: " + levelStr);
        }
        
        Logger::setLevel(level);
        
        return json{
            {"level", levelStr}
        };
    });
    
    // logger.getLevel
    registerCommand("logger.getLevel", [this](const json& params) {
        auto level = Logger::getLevel();
        
        // Convert Logger::Level to string
        std::string levelStr;
        switch (level) {
            case Logger::Level::DEBUG:
                levelStr = "DEBUG";
                break;
            case Logger::Level::INFO:
                levelStr = "INFO";
                break;
            case Logger::Level::WARNING:
                levelStr = "WARNING";
                break;
            case Logger::Level::ERROR:
                levelStr = "ERROR";
                break;
            case Logger::Level::CRITICAL:
                levelStr = "CRITICAL";
                break;
            default:
                levelStr = "UNKNOWN";
                break;
        }
        
        return json{
            {"level", levelStr}
        };
    });
    
    // logger.getLogs
    registerCommand("logger.getLogs", [this](const json& params) {
        int count = params.value("count", 100);
        auto logs = Logger::getRecentLogs(count);
        return json{
            {"logs", logs},
            {"count", logs.size()}
        };
    });
    
    // logger.clear
    registerCommand("logger.clear", [this](const json& params) {
        Logger::clearLogs();
        return json{{"cleared", true}};
    });
    
    // logger.export
    registerCommand("logger.export", [this](const json& params) {
        if (!params.contains("filename")) {
            throw std::runtime_error("Missing filename parameter");
        }
        std::string filename = params["filename"];
        bool success = Logger::exportLogs(filename);
        return json{
            {"exported", success},
            {"filename", filename}
        };
    });
    
    Logger::debug("CommandHandler", "Ã¢Å“â€œ Logger commands registered (5 commands)");
}

// ============================================================================
// LATENCY COMMANDS (7 commands)
// ============================================================================

void CommandHandler::registerLatencyCommands() {
    if (!compensator_) {
        Logger::warning("CommandHandler", 
                    "Compensator not available, skipping latency commands");
        return;
    }
    
    // latency.setCompensation
    registerCommand("latency.setCompensation", [this](const json& params) {
        if (!params.contains("instrument_id") || !params.contains("offset_ms")) {
            throw std::runtime_error("Missing instrument_id or offset_ms parameter");
        }
        
        std::string instrumentId = params["instrument_id"];
        double offsetMs = params["offset_ms"];
        int64_t offsetUs = static_cast<int64_t>(offsetMs * 1000.0);
        
        compensator_->setInstrumentCompensation(instrumentId, offsetUs);
        
        return json{
            {"instrument_id", instrumentId},
            {"offset_ms", offsetMs},
            {"offset_us", offsetUs}
        };
    });
    
    // latency.getCompensation
    registerCommand("latency.getCompensation", [this](const json& params) {
        if (!params.contains("instrument_id")) {
            throw std::runtime_error("Missing instrument_id parameter");
        }
        
        std::string instrumentId = params["instrument_id"];
        int64_t offsetUs = compensator_->getInstrumentCompensation(instrumentId);
        double offsetMs = offsetUs / 1000.0;
        
        return json{
            {"instrument_id", instrumentId},
            {"offset_ms", offsetMs},
            {"offset_us", offsetUs}
        };
    });
    
    // latency.enable
    registerCommand("latency.enable", [this](const json& params) {
        compensator_->enable();
        
        return json{{"enabled", true}};
    });
    
    // latency.disable
    registerCommand("latency.disable", [this](const json& params) {
        compensator_->disable();
        
        return json{{"enabled", false}};
    });
    
    // latency.setGlobalOffset
    registerCommand("latency.setGlobalOffset", [this](const json& params) {
        if (!params.contains("offset_ms")) {
            throw std::runtime_error("Missing offset_ms parameter");
        }
        
        double offsetMs = params["offset_ms"];
        compensator_->setGlobalOffset(offsetMs);
        
        return json{
            {"offset_ms", offsetMs}
        };
    });
    
    // latency.getGlobalOffset
    registerCommand("latency.getGlobalOffset", [this](const json& params) {
        double offsetMs = compensator_->getGlobalOffset();
        
        return json{
            {"offset_ms", offsetMs}
        };
    });
    
    // latency.listInstruments
    registerCommand("latency.listInstruments", [this](const json& params) {
        auto profiles = compensator_->getAllInstrumentProfiles();
        
        json instrumentsJson = json::array();
        for (const auto& profile : profiles) {
            json instrumentObj = {
                {"instrument_id", profile.instrumentId},
                {"avg_latency_us", profile.avgLatency},
                {"compensation_offset_us", profile.totalCompensation},
                {"measurement_count", profile.measurementCount},
                {"auto_calibration", profile.autoCalibration}
            };
            instrumentsJson.push_back(instrumentObj);
        }
        
        return json{
            {"instruments", instrumentsJson},
            {"count", instrumentsJson.size()}
        };
    });
    
    Logger::debug("CommandHandler", "Ã¢Å“â€œ Latency commands registered (7 commands)");
}

// ============================================================================
// PRESET COMMANDS (5 commands)
// ============================================================================

void CommandHandler::registerPresetCommands() {
    if (!presetManager_) {
        Logger::warning("CommandHandler", 
                    "PresetManager not available, skipping preset commands");
        return;
    }
    
    // preset.list
    registerCommand("preset.list", [this](const json& params) {
        auto presets = presetManager_->list();
        
        json presetsJson = json::array();
        for (const auto& preset : presets) {
            presetsJson.push_back(preset.toJson());
        }
        
        return json{
            {"presets", presetsJson},
            {"count", presetsJson.size()}
        };
    });
    
    // preset.load
    registerCommand("preset.load", [this](const json& params) {
        if (!params.contains("id")) {
            throw std::runtime_error("Missing id parameter");
        }
        
        int id = params["id"];
        auto preset = presetManager_->load(id);
        
        if (!preset) {
            throw std::runtime_error("Preset not found: " + std::to_string(id));
        }
        
        return preset->toJson();
    });
    
    // preset.save
    registerCommand("preset.save", [this](const json& params) {
        if (!params.contains("preset")) {
            throw std::runtime_error("Missing preset parameter");
        }
        
        json presetJson = params["preset"];
        Preset preset = Preset::fromJson(presetJson);
        
        int id = presetManager_->create(preset);
        
        return json{
            {"saved", true},
            {"id", id},
            {"name", preset.metadata.name}
        };
    });
    
    // preset.delete
    registerCommand("preset.delete", [this](const json& params) {
        if (!params.contains("id")) {
            throw std::runtime_error("Missing id parameter");
        }
        
        int id = params["id"];
        bool deleted = presetManager_->remove(id);
        
        return json{
            {"deleted", deleted},
            {"id", id}
        };
    });
    
    // preset.export
    registerCommand("preset.export", [this](const json& params) {
        if (!params.contains("id")) {
            throw std::runtime_error("Missing id parameter");
        }
        if (!params.contains("filepath")) {
            throw std::runtime_error("Missing filepath parameter");
        }
        
        int id = params["id"];
        std::string filepath = params["filepath"];
        
        bool exported = presetManager_->exportToFile(id, filepath);
        
        return json{
            {"exported", exported},
            {"id", id},
            {"filepath", filepath}
        };
    });
    
    Logger::debug("CommandHandler", "Ã¢Å“â€œ Preset commands registered (5 commands)");
}

// ============================================================================
// HELPERS
// ============================================================================



// ============================================================================
// BASE64 DECODING
// ============================================================================

std::vector<uint8_t> CommandHandler::base64Decode(const std::string& encoded) const {
    // Table Base64 standard
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    // CrÃƒÂ©er table de dÃƒÂ©codage
    std::vector<int> decoding_table(256, -1);
    for (int i = 0; i < 64; i++) {
        decoding_table[static_cast<unsigned char>(base64_chars[i])] = i;
    }
    
    std::vector<uint8_t> decoded;
    decoded.reserve((encoded.size() * 3) / 4);
    
    int val = 0;
    int valb = -8;
    
    for (unsigned char c : encoded) {
        // Ignorer les caractÃƒÂ¨res de padding et whitespace
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') {
            continue;
        }
        
        // VÃƒÂ©rifier que le caractÃƒÂ¨re est valide
        if (decoding_table[c] == -1) {
            Logger::warning("CommandHandler", 
                "Invalid Base64 character: " + std::to_string(static_cast<int>(c)));
            continue;
        }
        
        val = (val << 6) + decoding_table[c];
        valb += 6;
        
        if (valb >= 0) {
            decoded.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded;
}

// ============================================================================
// MIDI COMMANDS (3 commands)
// ============================================================================

void CommandHandler::registerMidiCommands() {
    // midi.convert - Convertir MIDI Ã¢â€ â€™ midiJson
    registerCommand("midi.convert", [this](const json& params) {
        if (!params.contains("filename")) {
            throw std::runtime_error("Missing filename parameter");
        }
        
        std::string filename = params["filename"];
        
        // Construire le chemin complet
        std::string filepath = fileManager_->getDirectoryPath(DirectoryType::UPLOADS) + 
                              "/" + filename;
        
        // VÃƒÂ©rifier que le fichier existe
        std::ifstream testFile(filepath);
        if (!testFile.good()) {
            throw std::runtime_error("File not found: " + filename);
        }
        testFile.close();
        
        // Convertir MIDI Ã¢â€ â€™ JsonMidi
        JsonMidiConverter converter;
        JsonMidi jsonMidi;
        
        try {
            jsonMidi = converter.fromMidiFile(filepath);
        } catch (const std::exception& e) {
            throw std::runtime_error("Conversion failed: " + std::string(e.what()));
        }
        
        Logger::info("CommandHandler", 
            "Ã¢Å“â€œ MIDI converted: " + filename + " -> " + 
            std::to_string(jsonMidi.timeline.size()) + " events");
        
        return json{
            {"success", true},
            {"filename", filename},
            {"midi_json", jsonMidi.toJson()}
        };
    });
    
    // midi.load - Charger midiJson depuis la base de donnÃ©es
    registerCommand("midi.load", [this](const json& params) {
        if (!params.contains("id")) {
            throw std::runtime_error("Missing id parameter");
        }
        
        if (!midiDatabase_) {
            throw std::runtime_error("MidiDatabase not available - requires Phase 4");
        }
        
        int id = params["id"];
        
        // Charger depuis la base (Phase 4)
        auto midiData = midiDatabase_->load(id);
        
        if (!midiData) {
            throw std::runtime_error("MIDI data not found: " + std::to_string(id));
        }
        
        return json{
            {"success", true},
            {"id", id},
            {"midi_json", midiData->midiJson},
            {"metadata", midiData->metadata.toJson()}
        };
    });
    
    // midi.save - Sauvegarder midiJson en base de donnÃ©es
    registerCommand("midi.save", [this](const json& params) {
        if (!params.contains("filename") || !params.contains("midi_json")) {
            throw std::runtime_error("Missing filename or midi_json parameter");
        }
        
        if (!midiDatabase_) {
            throw std::runtime_error("MidiDatabase not available - requires Phase 4");
        }
        
        std::string filename = params["filename"];
        json midiJson = params["midi_json"];
        
        // Sauvegarder en base (Phase 4)
        int id = midiDatabase_->save(filename, midiJson);
        
        return json{
            {"success", true},
            {"id", id},
            {"filename", filename}
        };
    });

    // midi.list - Lister tous les fichiers MIDI de la base de données
    registerCommand("midi.list", [this](const json& params) {
        if (!midiDatabase_) {
            throw std::runtime_error("MidiDatabase not available");
        }

        auto files = midiDatabase_->list();

        json filesJson = json::array();
        for (const auto& metadata : files) {
            filesJson.push_back(metadata.toJson());
        }

        return json{
            {"files", filesJson},
            {"count", files.size()}
        };
    });

    // midi.import - Upload + Convert + Save en une commande (Phase 6)
    registerCommand("midi.import", [this](const json& params) {
        if (!params.contains("filename") || !params.contains("content")) {
            throw std::runtime_error("Missing filename or content parameter");
        }
        
        if (!midiDatabase_) {
            throw std::runtime_error("MidiDatabase not available");
        }
        
        std::string filename = params["filename"];
        std::string content = params["content"];
        bool isBase64 = params.value("base64", true);
        
        Logger::info("CommandHandler", "Importing MIDI file: " + filename);
        
        // 1. Upload le fichier
        std::vector<uint8_t> data;
        if (isBase64) {
            try {
                data = base64Decode(content);
            } catch (const std::exception& e) {
                throw std::runtime_error("Invalid Base64 data: " + std::string(e.what()));
            }
        } else {
            data = std::vector<uint8_t>(content.begin(), content.end());
        }
        
        std::string filepath = fileManager_->uploadFile(
            data, filename, DirectoryType::UPLOADS, true
        );
        
        if (filepath.empty()) {
            throw std::runtime_error("Failed to upload file");
        }
        
        // 2. Convertir MIDI â†’ midiJson
        JsonMidiConverter converter;
        JsonMidi jsonMidi;
        
        try {
            jsonMidi = converter.fromMidiFile(filepath);
        } catch (const std::exception& e) {
            throw std::runtime_error("Conversion failed: " + std::string(e.what()));
        }
        
        // 3. Sauvegarder en base de donnÃ©es
        int midiId = midiDatabase_->save(filename, jsonMidi.toJson());
        
        Logger::info("CommandHandler", 
            "âœ“ MIDI file imported (ID: " + std::to_string(midiId) + 
            ", events: " + std::to_string(jsonMidi.timeline.size()) + ")");
        
        return json{
            {"success", true},
            {"midi_id", midiId},
            {"filename", filename},
            {"filepath", filepath},
            {"midi_json", jsonMidi.toJson()}
        };
    });
    
    // midi.routing.add - Ajouter routing instrument â†’ device
    registerCommand("midi.routing.add", [this](const json& params) {
        if (!params.contains("midi_file_id") || 
            !params.contains("track_id") || 
            !params.contains("device_id")) {
            throw std::runtime_error("Missing required parameters");
        }
        
        if (!midiDatabase_) {
            throw std::runtime_error("MidiDatabase not available");
        }
        
        MidiInstrumentRouting routing;
        routing.midiFileId = params["midi_file_id"];
        routing.trackId = params["track_id"];
        routing.instrumentName = params.value("instrument_name", "");
        routing.deviceId = params["device_id"];
        routing.channel = params.value("channel", 0);
        routing.enabled = params.value("enabled", true);
        
        int id = midiDatabase_->addRouting(routing);
        
        return json{
            {"success", true},
            {"routing_id", id}
        };
    });
    
    // midi.routing.list - Lister les routings d'un fichier MIDI
    registerCommand("midi.routing.list", [this](const json& params) {
        if (!params.contains("midi_file_id")) {
            throw std::runtime_error("Missing midi_file_id parameter");
        }
        
        if (!midiDatabase_) {
            throw std::runtime_error("MidiDatabase not available");
        }
        
        int midiFileId = params["midi_file_id"];
        auto routings = midiDatabase_->getRoutings(midiFileId);
        
        json routingsJson = json::array();
        for (const auto& routing : routings) {
            routingsJson.push_back(routing.toJson());
        }
        
        return json{
            {"routings", routingsJson},
            {"count", routings.size()}
        };
    });
    
    // midi.routing.update - Mettre Ã  jour un routing
    registerCommand("midi.routing.update", [this](const json& params) {
        if (!params.contains("routing_id")) {
            throw std::runtime_error("Missing routing_id parameter");
        }
        
        if (!midiDatabase_) {
            throw std::runtime_error("MidiDatabase not available");
        }
        
        MidiInstrumentRouting routing = MidiInstrumentRouting::fromJson(params);
        bool success = midiDatabase_->updateRouting(routing);
        
        return json{
            {"success", success}
        };
    });
    
    // midi.routing.remove - Supprimer un routing
    registerCommand("midi.routing.remove", [this](const json& params) {
        if (!params.contains("routing_id")) {
            throw std::runtime_error("Missing routing_id parameter");
        }
        
        if (!midiDatabase_) {
            throw std::runtime_error("MidiDatabase not available");
        }
        
        int routingId = params["routing_id"];
        bool success = midiDatabase_->removeRouting(routingId);
        
        return json{
            {"success", success}
        };
    });
    
    // midi.routing.clear - Supprimer tous les routings d'un fichier
    registerCommand("midi.routing.clear", [this](const json& params) {
        if (!params.contains("midi_file_id")) {
            throw std::runtime_error("Missing midi_file_id parameter");
        }
        
        if (!midiDatabase_) {
            throw std::runtime_error("MidiDatabase not available");
        }
        
        int midiFileId = params["midi_file_id"];
        bool success = midiDatabase_->clearRoutings(midiFileId);
        
        return json{
            {"success", success}
        };
    });

  // midi.sendNoteOn - Envoyer un Note On direct
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
        
        // CrÃ©er message MIDI Note On (0x90 + channel)
        std::vector<uint8_t> data = {
            static_cast<uint8_t>(0x90 | channel),
            note,
            velocity
        };
        
        MidiMessage msg(data);
        
        // Envoyer via le router
        if (!router_) {
            throw std::runtime_error("Router not available");
        }
        
        router_->route(msg);
        
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
    
    // midi.sendNoteOff - Envoyer un Note Off direct
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
        
        // CrÃ©er message MIDI Note Off (0x80 + channel)
        std::vector<uint8_t> data = {
            static_cast<uint8_t>(0x80 | channel),
            note,
            0  // velocity = 0 pour Note Off
        };
        
        MidiMessage msg(data);
        
        // Envoyer via le router
        if (!router_) {
            throw std::runtime_error("Router not available");
        }
        
        router_->route(msg);
        
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

    Logger::debug("CommandHandler", "MIDI commands registered (12 commands)");
}

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
    
    Logger::debug("CommandHandler", "âœ“ Playlist commands registered (9 commands)");
}


}
// ============================================================================
// END OF FILE CommandHandler.cpp
// ============================================================================
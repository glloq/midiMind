// ============================================================================
// File: backend/src/api/CommandHandler.cpp
// Version: 4.2.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#include "CommandHandler.h"
#include "../core/Logger.h"
#include "../timing/LatencyCompensator.h"
#include "../storage/InstrumentDatabase.h"
#include "../storage/PresetManager.h"
#include <chrono>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

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
    std::shared_ptr<EventBus> eventBus)
    : deviceManager_(deviceManager)
    , router_(router)
    , player_(player)
    , fileManager_(fileManager)
    , compensator_(compensator)
    , instrumentDb_(instrumentDb)
    , presetManager_(presetManager)
    , eventBus_(eventBus)
{
    Logger::info("CommandHandler", "Initializing CommandHandler v4.2.1...");
    registerAllCommands();
    Logger::info("CommandHandler", 
                "âœ… CommandHandler initialized (" + 
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
        
        // CORRECTION CRITIQUE 1: Validation sÃ©curisÃ©e avant accÃ¨s
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
    
    Logger::debug("CommandHandler", "âœ… Device commands registered (18 commands)");  
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
            routesJson.push_back({
                {"source_id", route.sourceId},
                {"destination_id", route.destinationId},
                {"enabled", route.enabled}
            });
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
    
    Logger::debug("CommandHandler", "âœ… Routing commands registered (6 commands)");
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
        auto status = player_->getStatus();
        
        return json{
            {"state", static_cast<int>(status.state)},
            {"current_time", status.currentTime},
            {"duration", status.duration},
            {"tempo", status.tempo},
            {"filename", status.filename}
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
        auto info = player_->getFileInfo();
        
        return json{
            {"filename", info.filename},
            {"duration", info.duration},
            {"track_count", info.trackCount},
            {"tempo", info.tempo},
            {"time_signature", info.timeSignature},
            {"format", info.format}
        };
    });
    
    // playback.listFiles
    registerCommand("playback.listFiles", [this](const json& params) {
        auto files = player_->listAvailableFiles();
        
        return json{
            {"files", files},
            {"count", files.size()}
        };
    });
    
    Logger::debug("CommandHandler", "âœ… Playback commands registered (10 commands)");
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
        std::string directory = params.value("directory", "");
        auto files = fileManager_->listFiles(directory);
        
        return json{
            {"files", files},
            {"count", files.size()}
        };
    });
    
    // files.read
    registerCommand("files.read", [this](const json& params) {
        if (!params.contains("filename")) {
            throw std::runtime_error("Missing filename parameter");
        }
        
        std::string filename = params["filename"];
        auto content = fileManager_->readFile(filename);
        
        return json{
            {"filename", filename},
            {"content", content},
            {"size", content.size()}
        };
    });
    
    // files.write
    registerCommand("files.write", [this](const json& params) {
        if (!params.contains("filename") || !params.contains("content")) {
            throw std::runtime_error("Missing filename or content parameter");
        }
        
        std::string filename = params["filename"];
        std::string content = params["content"];
        
        bool success = fileManager_->writeFile(filename, content);
        
        return json{
            {"written", success},
            {"filename", filename},
            {"size", content.size()}
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
        bool exists = fileManager_->fileExists(filename);
        
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
        auto info = fileManager_->getFileInfo(filename);
        
        return json{
            {"filename", info.filename},
            {"size", info.size},
            {"modified", info.modifiedTime},
            {"type", info.type}
        };
    });
    
    Logger::debug("CommandHandler", "âœ… File commands registered (6 commands)");
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
            {"version", "4.2.1"},
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
    
    Logger::debug("CommandHandler", "âœ… System commands registered (7 commands)");
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
    
    Logger::debug("CommandHandler", "âœ… Network commands registered (3 commands)");
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
        Logger::setGlobalLogLevel(level);
        
        return json{
            {"level", level}
        };
    });
    
    // logger.getLevel
    registerCommand("logger.getLevel", [this](const json& params) {
        return json{
            {"level", Logger::getGlobalLogLevelString()}
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
    
    Logger::debug("CommandHandler", "âœ… Logger commands registered (5 commands)");
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
            instrumentsJson.push_back({
                {"instrument_id", profile.instrumentId},
                {"avg_latency_us", profile.averageLatency},
                {"compensation_offset_us", profile.compensationOffset},
                {"measurement_count", profile.measurementCount},
                {"auto_calibration", profile.autoCalibration}
            });
        }
        
        return json{
            {"instruments", instrumentsJson},
            {"count", instrumentsJson.size()}
        };
    });
    
    Logger::debug("CommandHandler", "âœ… Latency commands registered (7 commands)");
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
    
    Logger::debug("CommandHandler", "âœ… Preset commands registered (5 commands)");
}

// ============================================================================
// HELPERS
// ============================================================================

json CommandHandler::createSuccessResponse(const json& data) const {
    return json{
        {"success", true},
        {"data", data},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
}

json CommandHandler::createErrorResponse(const std::string& error, 
                                        const std::string& errorCode) const {
    return json{
        {"success", false},
        {"error", error},
        {"error_code", errorCode},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
}

bool CommandHandler::validateCommand(const json& command, 
                                     std::string& error) const {
    if (!command.is_object()) {
        error = "Command must be a JSON object";
        return false;
    }
    
    if (!command.contains("command")) {
        error = "Missing 'command' field";
        return false;
    }
    
    if (!command["command"].is_string()) {
        error = "'command' must be a string";
        return false;
    }
    
    return true;
}

} // namespace midiMind

// ============================================================================
// END OF FILE CommandHandler.cpp
// ============================================================================
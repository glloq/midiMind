// ============================================================================
// File: backend/src/api/CommandHandler.cpp
// Version: 4.1.3 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.3:
//   - Fixed device commands: use getInfo() instead of MidiDeviceInfo
//   - Fixed routing commands: addRoute() returns void, use setRouteEnabled()
//   - Fixed logger commands: use setLevel() and getLevel()
//   - Added status field to device info
//
// ============================================================================

#include "CommandHandler.h"
#include "../core/Logger.h"
#include <chrono>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fstream>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

CommandHandler::CommandHandler(
    std::shared_ptr<MidiDeviceManager> deviceManager,
    std::shared_ptr<MidiRouter> router,
    std::shared_ptr<MidiPlayer> player,
    std::shared_ptr<FileManager> fileManager)
    : deviceManager_(deviceManager)
    , router_(router)
    , player_(player)
    , fileManager_(fileManager)
{
    Logger::info("CommandHandler", "Initializing CommandHandler...");
    
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
        
        std::string commandName = command["command"];
        json params = command.value("params", json::object());
        
        std::lock_guard<std::mutex> lock(commandsMutex_);
        
        auto it = commands_.find(commandName);
        if (it == commands_.end()) {
            return createErrorResponse(
                "Unknown command: " + commandName,
                "UNKNOWN_COMMAND");
        }
        
        try {
            json data = it->second(params);
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
        std::string category = (dotPos != std::string::npos) ?
            name.substr(0, dotPos) : "other";
        
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
// PRIVATE METHODS - HELPERS
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
        error = "'command' field must be a string";
        return false;
    }
    
    if (command.contains("params") && !command["params"].is_object()) {
        error = "'params' field must be an object";
        return false;
    }
    
    return true;
}

// ============================================================================
// PRIVATE METHODS - REGISTRATION
// ============================================================================

void CommandHandler::registerAllCommands() {
    Logger::debug("CommandHandler", "Registering all command categories...");
    
    registerDeviceCommands();
    registerRoutingCommands();
    registerPlaybackCommands();
    registerFileCommands();
    registerSystemCommands();
    registerNetworkCommands();
    registerLoggerCommands();
    
    Logger::debug("CommandHandler", 
                 "✓ All commands registered (" + 
                 std::to_string(commands_.size()) + " total)");
}

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
            {"count", devices.size()}
        };
    });
    
    // devices.connect
    registerCommand("devices.connect", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        bool success = deviceManager_->connect(deviceId);
        
        return json{{"connected", success}};
    });
    
    // devices.disconnect
    registerCommand("devices.disconnect", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        deviceManager_->disconnect(deviceId);
        
        return json{{"disconnected", true}};
    });
    
    // devices.info
    registerCommand("devices.info", [this](const json& params) {
        if (!params.contains("device_id")) {
            throw std::runtime_error("Missing device_id parameter");
        }
        
        std::string deviceId = params["device_id"];
        auto device = deviceManager_->getDevice(deviceId);
        
        if (!device) {
            throw std::runtime_error("Device not found: " + deviceId);
        }
        
        return device->getInfo();
    });
    
    Logger::debug("CommandHandler", "✓ Device commands registered (5 commands)");
}

void CommandHandler::registerRoutingCommands() {
    if (!router_) {
        Logger::warning("CommandHandler", 
                    "Router not available, skipping routing commands");
        return;
    }
    
    // routing.list
    registerCommand("routing.list", [this](const json& params) {
        auto routes = router_->getRoutes();
        
        json routesJson = json::array();
        for (const auto& route : routes) {
            routesJson.push_back(route->toJson());
        }
        
        return json{{"routes", routesJson}};
    });
    
    // routing.add
    registerCommand("routing.add", [this](const json& params) {
        auto route = std::make_shared<MidiRoute>();
        
        route->id = params.value("id", "");
        route->name = params.value("name", "");
        route->sourceDeviceId = params.value("source_device_id", "");
        route->destinationDeviceId = params.value("destination_device_id", "");
        route->enabled = params.value("enabled", true);
        
        router_->addRoute(route);
        
        return json{{"added", true}};
    });
    
    // routing.remove
    registerCommand("routing.remove", [this](const json& params) {
        if (!params.contains("route_id")) {
            throw std::runtime_error("Missing route_id parameter");
        }
        
        std::string routeId = params["route_id"];
        bool success = router_->removeRoute(routeId);
        
        return json{{"removed", success}};
    });
    
    // routing.enable
    registerCommand("routing.enable", [this](const json& params) {
        if (!params.contains("route_id")) {
            throw std::runtime_error("Missing route_id parameter");
        }
        
        std::string routeId = params["route_id"];
        router_->setRouteEnabled(routeId, true);
        
        return json{{"enabled", true}};
    });
    
    // routing.disable
    registerCommand("routing.disable", [this](const json& params) {
        if (!params.contains("route_id")) {
            throw std::runtime_error("Missing route_id parameter");
        }
        
        std::string routeId = params["route_id"];
        router_->setRouteEnabled(routeId, false);
        
        return json{{"disabled", true}};
    });
    
    Logger::debug("CommandHandler", "✓ Routing commands registered (5 commands)");
}

void CommandHandler::registerPlaybackCommands() {
    if (!player_) {
        Logger::warning("CommandHandler", 
                    "Player not available, skipping playback commands");
        return;
    }
    
    // playback.play
    registerCommand("playback.play", [this](const json& params) {
        bool success = player_->play();
        return json{{"playing", success}};
    });
    
    // playback.pause
    registerCommand("playback.pause", [this](const json& params) {
        bool success = player_->pause();
        return json{{"paused", success}};
    });
    
    // playback.stop
    registerCommand("playback.stop", [this](const json& params) {
        player_->stop();
        return json{{"stopped", true}};
    });
    
    // playback.seek
    registerCommand("playback.seek", [this](const json& params) {
        if (!params.contains("position")) {
            throw std::runtime_error("Missing position parameter");
        }
        
        uint64_t position = params["position"];
        player_->seek(position);
        
        return json{{"seeked", true}};
    });
    
    // playback.setTempo
    registerCommand("playback.setTempo", [this](const json& params) {
        if (!params.contains("tempo")) {
            throw std::runtime_error("Missing tempo parameter");
        }
        
        double tempo = params["tempo"];
        player_->setTempo(tempo);
        
        return json{{"tempo", tempo}};
    });
    
    // playback.setVolume
    registerCommand("playback.setVolume", [this](const json& params) {
        if (!params.contains("volume")) {
            throw std::runtime_error("Missing volume parameter");
        }
        
        float volume = params["volume"];
        player_->setVolume(volume);
        
        return json{{"volume", volume}};
    });
    
    // playback.setLoop
    registerCommand("playback.setLoop", [this](const json& params) {
        bool loop = params.value("loop", false);
        player_->setLoop(loop);
        
        return json{{"loop", loop}};
    });
    
    // playback.getStatus
    registerCommand("playback.getStatus", [this](const json& params) {
        return json{
            {"state", static_cast<int>(player_->getState())},
            {"position", player_->getCurrentPosition()},
            {"duration", player_->getDuration()},
            {"tempo", player_->getTempo()},
            {"volume", player_->getVolume()}
        };
    });
    
    Logger::debug("CommandHandler", "✓ Playback commands registered (8 commands)");
}

void CommandHandler::registerFileCommands() {
    Logger::warning("CommandHandler", "File commands not yet implemented");
}

void CommandHandler::registerSystemCommands() {
    // system.info
    registerCommand("system.info", [](const json& params) {
        struct utsname unameData;
        uname(&unameData);
        
        return json{
            {"system", unameData.sysname},
            {"node", unameData.nodename},
            {"release", unameData.release},
            {"version", unameData.version},
            {"machine", unameData.machine}
        };
    });
    
    // system.uptime
    registerCommand("system.uptime", [](const json& params) {
        std::ifstream uptime("/proc/uptime");
        double uptimeSeconds;
        uptime >> uptimeSeconds;
        
        return json{{"uptime_seconds", static_cast<uint64_t>(uptimeSeconds)}};
    });
    
    Logger::debug("CommandHandler", "✓ System commands registered (2 commands)");
}

void CommandHandler::registerNetworkCommands() {
    Logger::warning("CommandHandler", "Network commands not yet implemented");
}

void CommandHandler::registerLoggerCommands() {
    // logger.setLevel
    registerCommand("logger.setLevel", [](const json& params) {
        if (!params.contains("level")) {
            throw std::runtime_error("Missing level parameter");
        }
        
        std::string levelStr = params["level"];
        
        Logger::Level level;
        if (levelStr == "DEBUG") level = Logger::Level::DEBUG;
        else if (levelStr == "INFO") level = Logger::Level::INFO;
        else if (levelStr == "WARNING") level = Logger::Level::WARNING;
        else if (levelStr == "ERROR") level = Logger::Level::ERROR;
        else if (levelStr == "CRITICAL") level = Logger::Level::CRITICAL;
        else throw std::runtime_error("Invalid level: " + levelStr);
        
        Logger::setLevel(level);
        
        return json{{"level", levelStr}};
    });
    
    // logger.getLevel
    registerCommand("logger.getLevel", [](const json& params) {
        Logger::Level level = Logger::getLevel();
        
        std::string levelStr;
        switch (level) {
            case Logger::Level::DEBUG: levelStr = "DEBUG"; break;
            case Logger::Level::INFO: levelStr = "INFO"; break;
            case Logger::Level::WARNING: levelStr = "WARNING"; break;
            case Logger::Level::ERROR: levelStr = "ERROR"; break;
            case Logger::Level::CRITICAL: levelStr = "CRITICAL"; break;
        }
        
        return json{{"level", levelStr}};
    });
    
    Logger::debug("CommandHandler", "✓ Logger commands registered (2 commands)");
}

} // namespace midiMind

// ============================================================================
// END OF FILE CommandHandler.cpp v4.1.3
// ============================================================================
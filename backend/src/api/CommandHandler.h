// ============================================================================
// File: backend/src/api/CommandHandler.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Command handler for processing API commands.
//   Registers and routes commands to appropriate handlers.
//
// Features:
//   - 50+ registered commands
//   - Command factory pattern
//   - Category-based organization
//   - Parameter validation
//   - Error handling
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Renamed from CommandProcessorV2
//   - Enhanced error handling
//   - Better introspection
//
// ============================================================================

#pragma once

#include "../midi/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiFileManager.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @class CommandHandler
 * @brief Central command processing hub
 * 
 * Processes API commands from WebSocket clients.
 * Uses Command Pattern with factory for extensibility.
 * 
 * Command Categories:
 * - devices.*    : MIDI device management (5+ commands)
 * - routing.*    : Message routing (6+ commands)
 * - playback.*   : File playback (11+ commands)
 * - files.*      : File management (7+ commands)
 * - editor.*     : MIDI editing (7+ commands)
 * - network.*    : Network configuration (6+ commands)
 * - system.*     : System information (6+ commands)
 * - logger.*     : Logging control (3+ commands)
 * 
 * Thread Safety: YES
 * 
 * Example:
 * ```cpp
 * CommandHandler handler(deviceManager, router, player, fileManager);
 * 
 * // Process command
 * json command = {
 *     {"command", "devices.list"},
 *     {"params", {}}
 * };
 * 
 * json result = handler.processCommand(command);
 * 
 * if (result["success"]) {
 *     auto devices = result["data"]["devices"];
 * }
 * ```
 */
class CommandHandler {
public:
    // ========================================================================
    // TYPE DEFINITIONS
    // ========================================================================
    
    /**
     * @brief Command function signature
     * 
     * Takes parameters, returns result JSON
     */
    using CommandFunction = std::function<json(const json&)>;
    
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param deviceManager MIDI device manager
     * @param router MIDI router
     * @param player MIDI player
     * @param fileManager File manager
     */
    CommandHandler(
        std::shared_ptr<MidiDeviceManager> deviceManager,
        std::shared_ptr<MidiRouter> router,
        std::shared_ptr<MidiPlayer> player,
        std::shared_ptr<MidiFileManager> fileManager
    );
    
    /**
     * @brief Destructor
     */
    ~CommandHandler();
    
    // Disable copy
    CommandHandler(const CommandHandler&) = delete;
    CommandHandler& operator=(const CommandHandler&) = delete;
    
    // ========================================================================
    // COMMAND PROCESSING
    // ========================================================================
    
    /**
     * @brief Process command
     * @param command Command JSON {"command": "...", "params": {...}}
     * @return json Result JSON {"success": bool, "data": {...}}
     * @note Thread-safe
     */
    json processCommand(const json& command);
    
    /**
     * @brief Process command from string
     * @param jsonString JSON string
     * @return json Result JSON
     */
    json processCommand(const std::string& jsonString);
    
    // ========================================================================
    // COMMAND REGISTRATION
    // ========================================================================
    
    /**
     * @brief Register command handler
     * @param name Command name (e.g., "devices.list")
     * @param function Handler function
     */
    void registerCommand(const std::string& name, CommandFunction function);
    
    /**
     * @brief Unregister command
     * @param name Command name
     * @return true if removed
     */
    bool unregisterCommand(const std::string& name);
    
    // ========================================================================
    // INTROSPECTION
    // ========================================================================
    
    /**
     * @brief Get command count
     * @return size_t Number of registered commands
     */
    size_t getCommandCount() const;
    
    /**
     * @brief List all commands
     * @return std::vector<std::string> Command names
     */
    std::vector<std::string> listCommands() const;
    
    /**
     * @brief List commands by category
     * @return std::unordered_map<category, commands> Commands by category
     */
    std::unordered_map<std::string, std::vector<std::string>> 
    listCommandsByCategory() const;
    
    /**
     * @brief Check if command exists
     * @param name Command name
     * @return true if registered
     */
    bool hasCommand(const std::string& name) const;

private:
    // ========================================================================
    // PRIVATE METHODS - REGISTRATION
    // ========================================================================
    
    /**
     * @brief Register all command categories
     */
    void registerAllCommands();
    
    /**
     * @brief Register device commands (devices.*)
     */
    void registerDeviceCommands();
    
    /**
     * @brief Register routing commands (routing.*)
     */
    void registerRoutingCommands();
    
    /**
     * @brief Register playback commands (playback.*)
     */
    void registerPlaybackCommands();
    
    /**
     * @brief Register file commands (files.*)
     */
    void registerFileCommands();
    
    /**
     * @brief Register system commands (system.*)
     */
    void registerSystemCommands();
    
    /**
     * @brief Register network commands (network.*)
     */
    void registerNetworkCommands();
    
    /**
     * @brief Register logger commands (logger.*)
     */
    void registerLoggerCommands();
	
	/**
     * @brief Register latency management commands
     * @note Commandes pour gestion manuelle de la latence
     */
    void registerLatencyCommands();
    
    // ========================================================================
    // PRIVATE METHODS - HELPERS
    // ========================================================================
    
    /**
     * @brief Create success response
     */
    json createSuccessResponse(const json& data) const;
    
    /**
     * @brief Create error response
     */
    json createErrorResponse(const std::string& error, 
                           const std::string& errorCode = "COMMAND_FAILED") const;
    
    /**
     * @brief Validate command JSON
     */
    bool validateCommand(const json& command, std::string& error) const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Registered commands (name -> function)
    std::unordered_map<std::string, CommandFunction> commands_;
    
    /// Mutex for thread safety
    mutable std::mutex commandsMutex_;
    
    /// Component references
	std::shared_ptr<LatencyCompensator> compensator_;
    std::shared_ptr<InstrumentDatabase> instrumentDb_;
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<MidiPlayer> player_;
    std::shared_ptr<MidiFileManager> fileManager_;
};

} // namespace midiMind
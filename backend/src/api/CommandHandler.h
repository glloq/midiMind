// ============================================================================
// File: backend/src/api/CommandHandler.h
// Version: 4.2.8
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.8:
//   - REMOVED: createSuccessResponse(), createErrorResponse(), validateCommand()
//     (already removed from .cpp in v4.2.3, now removed from header)
//
// ============================================================================

#pragma once

#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/player/MidiPlayer.h"
#include "../storage/FileManager.h"
#include "../timing/LatencyCompensator.h"
#include "../storage/InstrumentDatabase.h"
#include "../storage/PresetManager.h"
#include "../storage/MidiDatabase.h"
#include "../storage/PlaylistManager.h"
#include "../core/EventBus.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

class CommandHandler {
public:
    using CommandFunction = std::function<json(const json&)>;
    
    CommandHandler(
        std::shared_ptr<MidiDeviceManager> deviceManager,
        std::shared_ptr<MidiRouter> router,
        std::shared_ptr<MidiPlayer> player,
        std::shared_ptr<FileManager> fileManager,
        std::shared_ptr<LatencyCompensator> compensator,
        std::shared_ptr<InstrumentDatabase> instrumentDb,
        std::shared_ptr<PresetManager> presetManager,
        std::shared_ptr<EventBus> eventBus,
        std::shared_ptr<MidiDatabase> midiDatabase = nullptr,
        std::shared_ptr<PlaylistManager> playlistManager = nullptr
    );
    
    ~CommandHandler();
    
    CommandHandler(const CommandHandler&) = delete;
    CommandHandler& operator=(const CommandHandler&) = delete;
    
    json processCommand(const json& command);
    
    void registerCommand(const std::string& name, CommandFunction function);
    bool unregisterCommand(const std::string& name);
    
    size_t getCommandCount() const;
    std::vector<std::string> listCommands() const;
    std::unordered_map<std::string, std::vector<std::string>> 
    listCommandsByCategory() const;
    bool hasCommand(const std::string& name) const;

private:
    void registerAllCommands();
    void registerDeviceCommands();
    void registerRoutingCommands();
    void registerPlaybackCommands();
    void registerFileCommands();
    void registerMidiCommands();
    void registerPlaylistCommands();
    void registerSystemCommands();
    void registerNetworkCommands();
    void registerLoggerCommands();
    void registerLatencyCommands();
    void registerPresetCommands();
    
    std::vector<uint8_t> base64Decode(const std::string& encoded) const;
    
    std::unordered_map<std::string, CommandFunction> commands_;
    mutable std::mutex commandsMutex_;
    
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<MidiPlayer> player_;
    std::shared_ptr<FileManager> fileManager_;
    std::shared_ptr<LatencyCompensator> compensator_;
    std::shared_ptr<InstrumentDatabase> instrumentDb_;
    std::shared_ptr<PresetManager> presetManager_;
    std::shared_ptr<EventBus> eventBus_;
    std::shared_ptr<MidiDatabase> midiDatabase_;
    std::shared_ptr<PlaylistManager> playlistManager_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE CommandHandler.h v4.2.8
// ============================================================================
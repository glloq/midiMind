// ============================================================================
// File: backend/src/core/Application.h
// Version: 4.2.5 - Add PlaylistManager support
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <optional>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// Forward declarations
class Database;
class Settings;
class FileManager;
class InstrumentDatabase;
class PresetManager;
class MidiDatabase;
class PlaylistManager;
class LatencyCompensator;
class MidiDeviceManager;
class MidiRouter;
class MidiPlayer;
class ApiServer;
class CommandHandler;
class EventBus;

/**
 * @class Application
 * @brief Main application singleton
 * 
 * Manages complete application lifecycle with 7-phase initialization.
 * 
 * Initialization order:
 *   1. Configuration (PathManager, Config)
 *   2. Database (SQLite + migrations)
 *   3. Storage (Settings, FileManager, InstrumentDB, PresetManager, MidiDatabase, PlaylistManager)
 *   4. EventSystem (EventBus) - AVANT Timing/MIDI/API
 *   5. Timing (LatencyCompensator)
 *   6. MIDI (DeviceManager, Router, Player)
 *   7. API (CommandHandler + ApiServer) - utilise EventBus
 */
class Application {
public:
    // Singleton
    static Application& instance();
    
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    // Lifecycle
    bool initialize(const std::string& configPath = "");
    bool start();
    void run();
    void stop();
    void shutdown() { stop(); }
    
    bool isInitialized() const;
    bool isRunning() const;
    
    // Component access (const references - immutable)
    Database* getDatabase() { return database_; }
    const std::shared_ptr<Settings>& getSettings() const { return settings_; }
    const std::shared_ptr<FileManager>& getFileManager() const { return fileManager_; }
    const std::shared_ptr<InstrumentDatabase>& getInstrumentDatabase() const { return instrumentDatabase_; }
    const std::shared_ptr<PresetManager>& getPresetManager() const { return presetManager_; }
    const std::shared_ptr<MidiDatabase>& getMidiDatabase() const { return midiDatabase_; }
    const std::shared_ptr<PlaylistManager>& getPlaylistManager() const { return playlistManager_; }
    const std::shared_ptr<LatencyCompensator>& getLatencyCompensator() const { return latencyCompensator_; }
    const std::shared_ptr<MidiDeviceManager>& getDeviceManager() const { return deviceManager_; }
    const std::shared_ptr<MidiRouter>& getRouter() const { return router_; }
    const std::shared_ptr<MidiPlayer>& getPlayer() const { return player_; }
    const std::shared_ptr<ApiServer>& getApiServer() const { return apiServer_; }
    const std::shared_ptr<EventBus>& getEventBus() const { return eventBus_; }
    
    // Status
    json getStatus() const;
    int getUptime() const;
    std::string getVersion() const;
    std::string getProtocolVersion() const;

private:
    Application();
    ~Application();
    
    // Initialization phases (7 phases in correct order)
    bool initializeConfiguration(const std::string& configPath);  // Phase 1
    bool initializeDatabase();                                     // Phase 2
    bool initializeStorage();                                      // Phase 3
    bool initializeEventSystem();                                  // Phase 4 - AVANT Timing/MIDI/API
    bool initializeTiming();                                       // Phase 5
    bool initializeMidi();                                         // Phase 6
    bool initializeApi();                                          // Phase 7 - utilise EventBus
    
    // Monitoring
    void startMonitoringThreads();
    void stopMonitoringThreads();
    void broadcastStatus();
    
    // Signal handling
    void setupSignalHandlers();
    
    // Member variables
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> statusBroadcastRunning_{false};
    std::optional<std::chrono::steady_clock::time_point> startTime_;
    
    std::string configPath_;
    json config_;
    std::thread statusBroadcastThread_;
    std::mutex shutdownMutex_;
    std::condition_variable shutdownCv_;
    
    // Core components
    Database* database_;
    std::shared_ptr<Settings> settings_;
    std::shared_ptr<FileManager> fileManager_;
    std::shared_ptr<InstrumentDatabase> instrumentDatabase_;
    std::shared_ptr<PresetManager> presetManager_;
    std::shared_ptr<MidiDatabase> midiDatabase_;
    std::shared_ptr<PlaylistManager> playlistManager_;
    std::shared_ptr<EventBus> eventBus_;
    
    // Timing components
    std::shared_ptr<LatencyCompensator> latencyCompensator_;
    
    // MIDI components
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<MidiPlayer> player_;
    
    // API components
    std::shared_ptr<ApiServer> apiServer_;
    std::shared_ptr<CommandHandler> commandHandler_;
    
    // Signal handling (private)
    static std::atomic<int> signalCount_;
    
    friend void signalHandler(int);
};

} // namespace midiMind

// ============================================================================
// END OF FILE Application.h
// ============================================================================
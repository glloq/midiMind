// ============================================================================
// File: backend/src/core/Application.h
// Version: 4.1.1 - CORRIGÉ COMPLET
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.1:
//   - Fixed member variable names to match .cpp
//   - Fixed pointer types (unique_ptr instead of shared_ptr)
//   - Added missing thread variable
//   - Fixed startTime_ type
//   - Fixed getUptime() return type
//   - Moved misplaced method declarations inside class
//
// ============================================================================

#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// Forward declarations
class Database;
class Settings;
class FileManager;
class PathManager;
class InstrumentDatabase;
class LatencyCompensator;
class MidiDeviceManager;
class MidiRouter;
class MidiPlayer;
class ApiServer;
class CommandHandler;
class EventBus;

// ============================================================================
// CLASS: Application (Singleton)
// ============================================================================

/**
 * @class Application
 * @brief Main application singleton
 * 
 * Manages complete application lifecycle with 7-phase initialization.
 */
class Application {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    /**
     * @brief Get singleton instance
     */
    static Application& instance();
    
    // Disable copy
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    // ========================================================================
    // LIFECYCLE
    // ========================================================================
    
    /**
     * @brief Initialize application
     * @param configPath Path to config.json (optional)
     * @return bool true if successful
     */
    bool initialize(const std::string& configPath = "");
    
    /**
     * @brief Start application
     * @return bool true if successful
     */
    bool start();
    
    /**
     * @brief Run main loop
     */
    void run();
    
    /**
     * @brief Stop application
     */
    void stop();
    
    /**
     * @brief Shutdown application (alias for stop)
     */
    void shutdown() { stop(); }
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const;
    
    /**
     * @brief Check if running
     */
    bool isRunning() const;
    
    // ========================================================================
    // COMPONENT ACCESS
    // ========================================================================
    
    std::unique_ptr<Database>& getDatabase() { return database_; }
    std::unique_ptr<Settings>& getSettings() { return settings_; }
    std::unique_ptr<FileManager>& getFileManager() { return fileManager_; }
    std::unique_ptr<InstrumentDatabase>& getInstrumentDatabase() { return instrumentDatabase_; }
    std::unique_ptr<LatencyCompensator>& getLatencyCompensator() { return latencyCompensator_; }
    std::unique_ptr<MidiDeviceManager>& getDeviceManager() { return deviceManager_; }
    std::unique_ptr<MidiRouter>& getRouter() { return router_; }
    std::unique_ptr<MidiPlayer>& getPlayer() { return player_; }
    std::unique_ptr<ApiServer>& getApiServer() { return apiServer_; }
    std::unique_ptr<EventBus>& getEventBus() { return eventBus_; }
    
    // ========================================================================
    // STATUS
    // ========================================================================
    
    /**
     * @brief Get application status
     * @return json Status information
     */
    json getStatus() const;
    
    /**
     * @brief Get uptime in seconds
     */
    int getUptime() const;
    
    /**
     * @brief Get version string
     */
    std::string getVersion() const;
    
    /**
     * @brief Get protocol version
     */
    std::string getProtocolVersion() const;
    
    // ========================================================================
    // SIGNAL HANDLING (PUBLIC STATIC)
    // ========================================================================
    
    static std::atomic<int> signalCount_;

private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR
    // ========================================================================
    
    Application();
    ~Application();
    
    // ========================================================================
    // INITIALIZATION PHASES
    // ========================================================================
    
    bool initializeConfiguration(const std::string& configPath);
    bool initializeDatabase();
    bool initializeStorage();
    bool initializeTiming();
    bool initializeMidi();
    bool initializeApi();
    bool initializeEventSystem();
    
    // ========================================================================
    // MONITORING
    // ========================================================================
    
    void startMonitoringThreads();
    void stopMonitoringThreads();
    void broadcastStatus();
    
    // ========================================================================
    // SIGNAL HANDLING
    // ========================================================================
    
    void setupSignalHandlers();
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> statusBroadcastRunning_{false};
    std::optional<std::chrono::steady_clock::time_point> startTime_;
    
    // Configuration
    std::string configPath_;
    json config_;
    
    // Threads
    std::thread statusBroadcastThread_;
    
    // Core components
    std::unique_ptr<Database> database_;
    std::unique_ptr<Settings> settings_;
    std::unique_ptr<FileManager> fileManager_;
    std::unique_ptr<PathManager> pathManager_;
    std::unique_ptr<InstrumentDatabase> instrumentDatabase_;
    std::unique_ptr<EventBus> eventBus_;
    
    // Timing components
    std::unique_ptr<LatencyCompensator> latencyCompensator_;
    
    // MIDI components
    std::unique_ptr<MidiDeviceManager> deviceManager_;
    std::unique_ptr<MidiRouter> router_;
    std::unique_ptr<MidiPlayer> player_;
    
    // API components
    std::unique_ptr<ApiServer> apiServer_;
    std::unique_ptr<CommandHandler> commandHandler_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE Application.h
// ============================================================================

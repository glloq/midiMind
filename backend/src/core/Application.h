// ============================================================================
// File: backend/src/core/Application.h
// Version: 4.1.0 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.0:
//   - Added #include <nlohmann/json.hpp> for json type
//
// ============================================================================

#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>  // ✅ AJOUTÉ

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
     * @brief Stop application
     */
    void stop();
    
    /**
     * @brief Shutdown application
     */
    void shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return initialized_.load(); }
    
    /**
     * @brief Check if running
     */
    bool isRunning() const { return running_.load(); }
    
    // ========================================================================
    // COMPONENT ACCESS
    // ========================================================================
    
    std::shared_ptr<Database> getDatabase() const { return database_; }
    std::shared_ptr<Settings> getSettings() const { return settings_; }
    std::shared_ptr<FileManager> getFileManager() const { return fileManager_; }
    std::shared_ptr<InstrumentDatabase> getInstrumentDatabase() const { return instrumentDb_; }
    std::shared_ptr<LatencyCompensator> getLatencyCompensator() const { return compensator_; }
    std::shared_ptr<MidiDeviceManager> getDeviceManager() const { return deviceManager_; }
    std::shared_ptr<MidiRouter> getRouter() const { return router_; }
    std::shared_ptr<MidiPlayer> getPlayer() const { return player_; }
    std::shared_ptr<ApiServer> getApiServer() const { return apiServer_; }
    std::shared_ptr<EventBus> getEventBus() const { return eventBus_; }
    
    // ========================================================================
    // STATUS
    // ========================================================================
    
    /**
     * @brief Get application status
     * @return json Status information
     */
    json getStatus() const;  // ✅ Type 'json' maintenant déclaré
    
    /**
     * @brief Get uptime in seconds
     */
    uint64_t getUptime() const;

private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR
    // ========================================================================
    
    Application();
    ~Application();
    
    // ========================================================================
    // INITIALIZATION PHASES
    // ========================================================================
    
    bool initializePhase1_Config(const std::string& configPath);
    bool initializePhase2_Database();
    bool initializePhase3_Storage();
    bool initializePhase4_Timing();
    bool initializePhase5_MIDI();
    bool initializePhase6_API();
    bool initializePhase7_Monitoring();
    
    // ========================================================================
    // SIGNAL HANDLING
    // ========================================================================
    
    static void signalHandler(int signal);
    static std::atomic<int> signalCount_;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    uint64_t startTime_{0};
    
    // Configuration
    std::string configPath_;
    json config_;
    
    // Core components
    std::shared_ptr<Database> database_;
    std::shared_ptr<Settings> settings_;
    std::shared_ptr<FileManager> fileManager_;
    std::shared_ptr<PathManager> pathManager_;
    std::shared_ptr<InstrumentDatabase> instrumentDb_;
    std::shared_ptr<EventBus> eventBus_;
    
    // Timing components
    std::shared_ptr<LatencyCompensator> compensator_;
    
    // MIDI components
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<MidiPlayer> player_;
    
    // API components
    std::shared_ptr<ApiServer> apiServer_;
    std::shared_ptr<CommandHandler> commandHandler_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE Application.h
// ============================================================================
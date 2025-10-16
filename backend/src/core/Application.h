// ============================================================================
// File: backend/src/core/Application.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Main application class implementing the singleton pattern.
//   Manages the complete lifecycle of the MidiMind backend including
//   initialization, startup, shutdown, and signal handling.
//
// Lifecycle:
//   1. initialize() - 7-phase initialization
//   2. start()      - Start all services
//   3. run()        - Main loop / wait for shutdown
//   4. stop()       - Graceful shutdown
//
// Dependencies:
//   - Logger, Config, Database
//   - MidiDeviceManager, MidiRouter, MidiPlayer
//   - ApiServer, CommandProcessorV2
//   - EventBus
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified initialization (7 phases)
//   - Removed NetworkManager (WiFi/BT for v4.2.0)
//   - Added LatencyCompensator integration
//   - Enhanced signal handling
//   - Thread-safe Meyers singleton
//
// ============================================================================

#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

// Forward declarations
namespace midiMind {
    class Database;
    class Settings;
    class FileManager;
    class InstrumentDatabase;
    class MidiDeviceManager;
    class MidiRouter;
    class MidiPlayer;
    class ApiServer;
    class CommandProcessorV2;
    class EventBus;
    class LatencyCompensator;
    class CalibrationEngine;
}

namespace midiMind {

// ============================================================================
// EXIT CODES
// ============================================================================

enum class ExitCode {
    SUCCESS = 0,
    INITIALIZATION_FAILED = 1,
    START_FAILED = 2,
    RUNTIME_ERROR = 3,
    INVALID_ARGUMENTS = 4
};

// ============================================================================
// CLASS: Application
// ============================================================================

/**
 * @class Application
 * @brief Main application class - Singleton pattern
 * 
 * Manages the complete lifecycle of MidiMind backend:
 * - Configuration loading
 * - Database initialization
 * - MIDI subsystem setup
 * - API server startup
 * - Signal handling
 * - Graceful shutdown
 * 
 * Usage:
 * ```cpp
 * Application& app = Application::instance();
 * if (!app.initialize("/path/to/config.json")) {
 *     return EXIT_FAILURE;
 * }
 * if (!app.start()) {
 *     return EXIT_FAILURE;
 * }
 * app.run(); // Blocks until shutdown
 * app.stop();
 * ```
 * 
 * Thread Safety:
 * - Meyers singleton (thread-safe since C++11)
 * - All public methods are thread-safe
 * - Signal handler interacts safely with atomic flags
 */
class Application {
public:
    // ========================================================================
    // SINGLETON PATTERN
    // ========================================================================
    
    /**
     * @brief Get the singleton instance (thread-safe)
     * @return Reference to the unique instance
     * @note Meyers Singleton - guaranteed thread-safe initialization
     */
    static Application& instance();
    
    // Disable copy and assignment
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    /**
     * @brief Destructor - Clean shutdown
     * 
     * Ensures proper cleanup:
     * - Stops all threads
     * - Disconnects devices
     * - Closes database
     * - Releases resources
     */
    ~Application();
    
    // ========================================================================
    // LIFECYCLE METHODS
    // ========================================================================
    
    /**
     * @brief Initialize the application (7 phases)
     * 
     * Initialization phases:
     * 1. Configuration (Config.h)
     * 2. Database (SQLite + migrations)
     * 3. Storage (Settings, FileManager, InstrumentDatabase)
     * 4. Timing (LatencyCompensator, CalibrationEngine)
     * 5. MIDI Core (DeviceManager, Router, Player)
     * 6. API (ApiServer, CommandProcessor)
     * 7. Event System (EventBus, observers)
     * 
     * @param configPath Path to config.json (default: ./config.json)
     * @return true if initialization successful
     * 
     * @note Logs detailed progress for each phase
     * @note Automatically performs database migrations
     */
    bool initialize(const std::string& configPath = "");
    
    /**
     * @brief Start all services
     * 
     * Actions:
     * - Start API Server (WebSocket on configured port)
     * - Scan and connect MIDI devices
     * - Start hot-plug monitoring thread
     * - Start status broadcast thread
     * - Load saved configuration
     * 
     * @return true if startup successful
     * 
     * @note Must be called after initialize()
     * @note Non-blocking - returns after startup
     */
    bool start();
    
    /**
     * @brief Run the application (blocking)
     * 
     * Blocks until shutdown is requested via:
     * - SIGINT (Ctrl+C)
     * - SIGTERM (kill)
     * - API shutdown command
     * 
     * @note This is the main event loop
     * @note Call stop() to request shutdown
     */
    void run();
    
    /**
     * @brief Stop the application gracefully
     * 
     * Shutdown sequence:
     * 1. Set shutdown flag
     * 2. Stop hot-plug monitoring
     * 3. Stop status broadcast
     * 4. Stop MIDI player
     * 5. Disconnect all devices
     * 6. Stop API server
     * 7. Close database
     * 
     * @note Thread-safe - can be called from signal handler
     * @note Can be called multiple times safely
     */
    void stop();
    
    /**
     * @brief Check if application is running
     * @return true if running, false otherwise
     */
    bool isRunning() const { return running_.load(); }
    
    /**
     * @brief Check if application is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return initialized_.load(); }
    
    // ========================================================================
    // SIGNAL HANDLING
    // ========================================================================
    
    /**
     * @brief Setup signal handlers
     * 
     * Handles:
     * - SIGINT (Ctrl+C)
     * - SIGTERM (kill)
     * - SIGHUP (reload config - future)
     * 
     * @note Automatically called by initialize()
     */
    void setupSignalHandlers();
    
    /**
     * @brief Signal counter for force quit
     * 
     * Tracks number of signals received.
     * After 3 signals, force immediate exit.
     */
    static std::atomic<int> signalCount_;
    
    // ========================================================================
    // STATUS & MONITORING
    // ========================================================================
    
    /**
     * @brief Get application status
     * @return JSON object with status information
     * 
     * Status includes:
     * - Version information
     * - Uptime
     * - Connected devices count
     * - API connection count
     * - Memory usage
     * - Database size
     */
    json getStatus() const;
    
    /**
     * @brief Get application version
     * @return Version string (e.g., "4.1.0")
     */
    static std::string getVersion() { return "4.1.0"; }
    
    /**
     * @brief Get protocol version
     * @return Protocol version string (e.g., "4.0.0")
     */
    static std::string getProtocolVersion() { return "4.0.0"; }
    
    // ========================================================================
    // COMPONENT ACCESS
    // ========================================================================
    
    /**
     * @brief Get database instance
     * @return Reference to Database
     * @throws std::runtime_error if not initialized
     */
    Database& getDatabase();
    
    /**
     * @brief Get MIDI device manager
     * @return Reference to MidiDeviceManager
     * @throws std::runtime_error if not initialized
     */
    MidiDeviceManager& getDeviceManager();
    
    /**
     * @brief Get MIDI router
     * @return Reference to MidiRouter
     * @throws std::runtime_error if not initialized
     */
    MidiRouter& getRouter();
    
    /**
     * @brief Get MIDI player
     * @return Reference to MidiPlayer
     * @throws std::runtime_error if not initialized
     */
    MidiPlayer& getPlayer();
    
    /**
     * @brief Get API server
     * @return Reference to ApiServer
     * @throws std::runtime_error if not initialized
     */
    ApiServer& getApiServer();
    
    /**
     * @brief Get latency compensator
     * @return Reference to LatencyCompensator
     * @throws std::runtime_error if not initialized
     */
    LatencyCompensator& getLatencyCompensator();
    
    /**
     * @brief Get event bus
     * @return Reference to EventBus
     * @throws std::runtime_error if not initialized
     */
    EventBus& getEventBus();
    
private:
    // ========================================================================
    // CONSTRUCTOR (PRIVATE - SINGLETON)
    // ========================================================================
    
    Application();
    
    // ========================================================================
    // INITIALIZATION PHASES (PRIVATE)
    // ========================================================================
    
    /**
     * @brief Phase 1: Load configuration
     * @param configPath Path to config.json
     * @return true if successful
     */
    bool initializeConfiguration(const std::string& configPath);
    
    /**
     * @brief Phase 2: Initialize database
     * @return true if successful
     */
    bool initializeDatabase();
    
    /**
     * @brief Phase 3: Initialize storage subsystems
     * @return true if successful
     */
    bool initializeStorage();
    
    /**
     * @brief Phase 4: Initialize timing subsystems
     * @return true if successful
     */
    bool initializeTiming();
    
    /**
     * @brief Phase 5: Initialize MIDI subsystems
     * @return true if successful
     */
    bool initializeMidi();
    
    /**
     * @brief Phase 6: Initialize API
     * @return true if successful
     */
    bool initializeApi();
    
    /**
     * @brief Phase 7: Setup event system
     * @return true if successful
     */
    bool initializeEventSystem();
    
    // ========================================================================
    // MONITORING THREADS (PRIVATE)
    // ========================================================================
    
    /**
     * @brief Hot-plug monitoring thread function
     * 
     * Monitors ALSA for device connections/disconnections.
     * Automatically connects new devices based on configuration.
     */
    void hotPlugMonitorThread();
    
    /**
     * @brief Status broadcast thread function
     * 
     * Periodically broadcasts system status to connected clients.
     * Default: every 5 seconds
     */
    void statusBroadcastThread();
    
    /**
     * @brief Stop monitoring threads
     */
    void stopMonitoringThreads();
    
    // ========================================================================
    // EVENT HANDLERS (PRIVATE)
    // ========================================================================
    
    /**
     * @brief Setup event observers
     * 
     * Registers callbacks for:
     * - Device connected/disconnected
     * - MIDI message received
     * - Calibration completed
     * - Error events
     */
    void setupEventObservers();
    
    /**
     * @brief Handle device connected event
     */
    void onDeviceConnected(const std::string& deviceId);
    
    /**
     * @brief Handle device disconnected event
     */
    void onDeviceDisconnected(const std::string& deviceId);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    // Lifecycle flags
    std::atomic<bool> initialized_;
    std::atomic<bool> running_;
    std::atomic<bool> shutdownRequested_;
    
    // Monitoring threads
    std::atomic<bool> hotPlugRunning_;
    std::atomic<bool> statusBroadcastRunning_;
    std::thread hotPlugThread_;
    std::thread statusBroadcastThread_;
    
    // Synchronization
    mutable std::mutex mutex_;
    std::condition_variable shutdownCondition_;
    
    // Core components (unique_ptr for RAII)
    std::unique_ptr<Database> database_;
    std::unique_ptr<Settings> settings_;
    std::unique_ptr<FileManager> fileManager_;
    std::unique_ptr<InstrumentDatabase> instrumentDatabase_;
    
    // Timing components
    std::unique_ptr<LatencyCompensator> latencyCompensator_;
    std::unique_ptr<CalibrationEngine> calibrationEngine_;
    
    // MIDI components
    std::unique_ptr<MidiDeviceManager> deviceManager_;
    std::unique_ptr<MidiRouter> router_;
    std::unique_ptr<MidiPlayer> player_;
    
    // API components
    std::unique_ptr<ApiServer> apiServer_;
    std::unique_ptr<CommandProcessorV2> commandProcessor_;
    
    // Event system
    std::unique_ptr<EventBus> eventBus_;
    
    // Runtime info
    std::chrono::steady_clock::time_point startTime_;
};

} // namespace midiMind
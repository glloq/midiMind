// ============================================================================
// File: backend/src/core/Application.cpp
// Version: 4.1.2 - CORRIGÉ API SERVER
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of the main application class.
//   Manages complete lifecycle with 7-phase initialization.
//
//   SIMPLIFICATION v4.1.0:
//   - Removed CalibrationEngine (auto-calibration wizard)
//   - Kept LatencyCompensator (manual compensation only)
//   - Added InstrumentDatabase integration
//
// Dependencies:
//   - All core components (Logger, Config, Database, etc.)
//   - All MIDI components (DeviceManager, Router, Player)
//   - All API components (ApiServer, CommandHandler)
//   - Timing: LatencyCompensator only (no CalibrationEngine)
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Removed NetworkManager (WiFi/BT for v4.2.0)
//   - Removed CalibrationEngine (manual latency only)
//   - Added LatencyCompensator with InstrumentDatabase
//   - Simplified initialization (7 phases)
//   - Enhanced error handling
//
// ============================================================================

#include "Application.h"
#include "Logger.h"
#include "Config.h"
#include "Error.h"
#include "TimeUtils.h"
#include "EventBus.h"
#include "../storage/Database.h"
#include "../storage/Settings.h"
#include "../storage/FileManager.h"
#include "../storage/InstrumentDatabase.h"
#include "../timing/LatencyCompensator.h"
// NOTE: CalibrationEngine.h REMOVED
#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/player/MidiPlayer.h"
#include "../api/ApiServer.h"
#include "../api/CommandHandler.h"

#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>

namespace midiMind {

// ============================================================================
// STATIC MEMBERS
// ============================================================================

std::atomic<int> Application::signalCount_{0};

// Static signal handler flag
static std::atomic<bool> g_shutdownRequested{false};

// ============================================================================
// SIGNAL HANDLER
// ============================================================================

/**
 * @brief Global signal handler
 * 
 * Handles SIGINT (Ctrl+C) and SIGTERM (kill).
 * After 3 signals, forces immediate exit.
 */
void signalHandler(int signal) {
    Application::signalCount_++;
    
    if (Application::signalCount_ == 1) {
        Logger::info("Signal", "Shutdown signal received (Ctrl+C)");
        Logger::info("Signal", "Starting graceful shutdown...");
        g_shutdownRequested = true;
    } else if (Application::signalCount_ == 2) {
        Logger::warning("Signal", "Second signal received. Forcing shutdown...");
        g_shutdownRequested = true;
    } else {
        Logger::critical("Signal", "Third signal received. Immediate exit!");
		std::exit(1);    
	}
}

// ============================================================================
// SINGLETON
// ============================================================================

Application& Application::instance() {
    static Application instance;
    return instance;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

Application::Application()
    : initialized_(false)
    , running_(false)
    , statusBroadcastRunning_(false)
{
    Logger::info("Application", "Application instance created");
}

Application::~Application() {
    Logger::info("Application", "Destroying application...");
    
    // Ensure threads are stopped
    stopMonitoringThreads();
    
    // Release components in reverse order of creation
    Logger::debug("Application", "Releasing components...");
    
    eventBus_.reset();
    commandHandler_.reset();
    apiServer_.reset();
    player_.reset();
    router_.reset();
    deviceManager_.reset();
    // calibrationEngine_.reset();  // REMOVED
    latencyCompensator_.reset();
    instrumentDatabase_.reset();
    fileManager_.reset();
    settings_.reset();
    
    Logger::info("Application", "Application destroyed successfully");
}

bool Application::isInitialized() const {
    return initialized_.load();
}

bool Application::isRunning() const {
    return running_.load();
}

json Application::getStatus() const {
    json status;
    
    status["initialized"] = initialized_.load();
    status["running"] = running_.load();
    status["version"] = getVersion();
    status["protocol_version"] = getProtocolVersion();
    status["uptime"] = getUptime();
    
    status["components"] = {
        {"database", database_ && database_->isConnected()},
        {"settings", settings_ != nullptr},
        {"file_manager", fileManager_ != nullptr},
        {"instrument_database", instrumentDatabase_ != nullptr},
        {"latency_compensator", latencyCompensator_ != nullptr},
        {"device_manager", deviceManager_ != nullptr},
        {"router", router_ != nullptr},
        {"player", player_ != nullptr},
        {"api_server", apiServer_ && apiServer_->isRunning()},
        {"event_bus", eventBus_ != nullptr}
    };
    
    if (deviceManager_) {
        status["midi_devices"] = deviceManager_->getDeviceCount();
    }
    
    return status;
}



// ============================================================================
// LIFECYCLE - INITIALIZE
// ============================================================================

bool Application::initialize(const std::string& configPath) {
    if (initialized_.load()) {
        Logger::warning("Application", "Already initialized");
        return true;
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "╔═══════════════════════════════════════╗");
    Logger::info("Application", "║   MidiMind v4.1.0 Initialization     ║");
    Logger::info("Application", "╚═══════════════════════════════════════╝");
    Logger::info("Application", "");
    Logger::info("Application", "Protocol version: " + getProtocolVersion());
    Logger::info("Application", "Build date: " + std::string(__DATE__) + " " + __TIME__);
    Logger::info("Application", "");
    
    try {
        // Phase 1: Configuration
        if (!initializeConfiguration(configPath)) {
            Logger::error("Application", "Configuration initialization failed");
            return false;
        }
        
        // Phase 2: Database
        if (!initializeDatabase()) {
            Logger::error("Application", "Database initialization failed");
            return false;
        }
        
        // Phase 3: Storage
        if (!initializeStorage()) {
            Logger::error("Application", "Storage initialization failed");
            return false;
        }
        
        // Phase 4: Timing (simplified - no CalibrationEngine)
        if (!initializeTiming()) {
            Logger::error("Application", "Timing initialization failed");
            return false;
        }
        
        // Phase 5: MIDI
        if (!initializeMidi()) {
            Logger::error("Application", "MIDI initialization failed");
            return false;
        }
        
        // Phase 6: API
        if (!initializeApi()) {
            Logger::error("Application", "API initialization failed");
            return false;
        }
        
        // Phase 7: Event System
        if (!initializeEventSystem()) {
            Logger::error("Application", "Event system initialization failed");
            return false;
        }
        
        // Setup signal handlers
        setupSignalHandlers();
        
        initialized_ = true;
        
        Logger::info("Application", "");
        Logger::info("Application", "╔═══════════════════════════════════════╗");
        Logger::info("Application", "║   Initialization Complete ✓          ║");
        Logger::info("Application", "╚═══════════════════════════════════════╝");
        Logger::info("Application", "");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::critical("Application", 
                        "Initialization failed with exception: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// INITIALIZATION PHASES (PRIVATE)
// ============================================================================

bool Application::initializeConfiguration(const std::string& configPath) {
    Logger::info("Application", "┌─── Phase 1/7: Configuration ─────────┐");
    Logger::info("Application", "");
    
    try {
        std::string cfgPath = configPath.empty() ? 
            "/etc/midimind/config.json" : configPath;
        
        Logger::info("Application", "  Loading configuration from: " + cfgPath);
        
        if (!Config::instance().load(cfgPath)) {
            Logger::warning("Application", "  Config file not found, using defaults");
        }
        
        Logger::info("Application", "  ✓ Configuration loaded");
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Configuration failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeDatabase() {
    Logger::info("Application", "┌─── Phase 2/7: Database ──────────────┐");
    Logger::info("Application", "");
    
    try {
        // Initialize PathManager first
        if (!pathManager_) {
            Logger::info("Application", "  Creating PathManager...");
            pathManager_ = std::make_shared<PathManager>();
            std::string rootPath = Config::instance().getString("storage.root", 
                                                                "/opt/midimind");
            pathManager_->setBasePath(rootPath);
            Logger::info("Application", "  ✓ PathManager ready (root: " + rootPath + ")");
        }
        
        std::string dbPath = pathManager_->getDatabasePath();
        
        Logger::info("Application", "  Connecting to database: " + dbPath);
        
        database_ = &Database::instance(); 
        
        if (!database_->connect(dbPath)) {
            Logger::error("Application", "  Failed to connect to database");
            return false;
        }
        
        Logger::info("Application", "  ✓ Database connected");
        
        // Run migrations using PathManager
        Logger::info("Application", "  Running migrations...");
        std::string migrationDir = pathManager_->getMigrationsPath();
        
        if (!database_->runMigrations(migrationDir)) {
            Logger::error("Application", "  Migrations failed");
            return false;
        }
        
        Logger::info("Application", "  ✓ Migrations complete");
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Database failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeStorage() {
    Logger::info("Application", "┌─── Phase 3/7: Storage ───────────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Creating Settings...");
        settings_ = std::make_shared<Settings>(*database_);
        if (!settings_->load()) {
            Logger::warning("Application", "  Settings not found, using defaults");
        }
        Logger::info("Application", "  ✓ Settings ready");
        
        Logger::info("Application", "  Creating FileManager...");
        // Use PathManager instead of hardcoded path
        std::string rootPath = pathManager_->getBasePath();
        fileManager_ = std::make_shared<FileManager>(rootPath);
        
        if (!fileManager_->initializeDirectories()) {
            Logger::error("Application", "  Failed to initialize directories");
            return false;
        }
        Logger::info("Application", "  ✓ FileManager ready");
        
        Logger::info("Application", "  Creating InstrumentDatabase...");
        instrumentDatabase_ = std::make_shared<InstrumentDatabase>(*database_);
        Logger::info("Application", "  ✓ InstrumentDatabase ready");
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Storage failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeTiming() {
    Logger::info("Application", "┌─── Phase 4/7: Timing (Manual Only) ──┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Creating LatencyCompensator...");
        latencyCompensator_ = std::make_shared<LatencyCompensator>(*instrumentDatabase_);
        
        // Load existing profiles from database
        /*if (!latencyCompensator_->loadFromDatabase()) {
            Logger::warning("Application", "  No instrument profiles loaded");
        } else {
            Logger::info("Application", "  ✓ Instrument profiles loaded");
        }*/
        
        Logger::info("Application", "  ✓ LatencyCompensator ready (manual mode)");
        
        // NOTE: CalibrationEngine NOT created (removed)
        Logger::info("Application", "  ℹ Calibration: Manual only (no auto-wizard)");
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Timing failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeMidi() {
    Logger::info("Application", "┌─── Phase 5/7: MIDI ──────────────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Creating MidiDeviceManager...");
        deviceManager_ = std::make_shared<MidiDeviceManager>();
        Logger::info("Application", "  ✓ MidiDeviceManager ready");
        
        Logger::info("Application", "  Creating MidiRouter...");
        router_ = std::make_shared<MidiRouter>(latencyCompensator_.get());
        Logger::info("Application", "  ✓ MidiRouter ready (with latency compensation)");
        
        Logger::info("Application", "  Creating MidiPlayer...");
        player_ = std::make_shared<MidiPlayer>(router_);
        Logger::info("Application", "  ✓ MidiPlayer ready");
        
        // Scan for MIDI devices
        Logger::info("Application", "  Scanning for MIDI devices...");
        auto devices = deviceManager_->discoverDevices(true);
        Logger::info("Application", "  ✓ Found " + std::to_string(devices.size()) + " devices");
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "MIDI failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeApi() {
    Logger::info("Application", "┌─── Phase 6/7: API ───────────────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Creating CommandHandler...");
        
        // Pass latencyCompensator and instrumentDatabase to CommandHandler
        commandHandler_ = std::make_shared<CommandHandler>(
			deviceManager_,
			router_,
			player_,
			fileManager_ 
        );
        
        Logger::info("Application", "  ✓ CommandHandler ready");
        Logger::info("Application", "    - Registered commands: " + 
                    std::to_string(commandHandler_->getCommandCount()));
        
        Logger::info("Application", "  Creating ApiServer...");
        
        apiPort_ = Config::instance().getInt("api.port", 8080);
        apiHost_ = Config::instance().getString("api.host", "0.0.0.0");
        
        apiServer_ = std::make_shared<ApiServer>();
        
        // Register command callback
        apiServer_->setCommandCallback([this](const json& command) {
            return commandHandler_->processCommand(command);
        });
        
        Logger::info("Application", "  ✓ ApiServer ready");
        Logger::info("Application", "    - Port: " + std::to_string(apiPort_));
        Logger::info("Application", "    - Host: " + apiHost_);
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "API failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeEventSystem() {
    Logger::info("Application", "┌─── Phase 7/7: Event System ──────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Creating EventBus...");
        eventBus_ = std::make_shared<EventBus>();
        Logger::info("Application", "  ✓ EventBus ready");
        
        // Subscribe API server to device events
        if (apiServer_ && deviceManager_) {
            // TODO: Setup event subscriptions
            Logger::info("Application", "  ✓ Event subscriptions configured");
        }
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Event system failed: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// LIFECYCLE - START
// ============================================================================

bool Application::start() {
    if (!initialized_.load()) {
        Logger::error("Application", "Cannot start: not initialized");
        return false;
    }
    
    if (running_.load()) {
        Logger::warning("Application", "Already running");
        return true;
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "╔═══════════════════════════════════════╗");
    Logger::info("Application", "║   Starting MidiMind Services         ║");
    Logger::info("Application", "╚═══════════════════════════════════════╝");
    Logger::info("Application", "");
    
    try {
        // Start API server
        // Start API server
        Logger::info("Application", "Starting API server...");
        
        // ✅✅✅ FIXED v4.1.2: Actually start the API server
        if (apiServer_) {
            try {
                apiServer_->start(apiPort_);
                Logger::info("Application", "✓ API server started on port " + std::to_string(apiPort_));
            } catch (const std::exception& e) {
                Logger::error("Application", "Failed to start API server: " + std::string(e.what()));
                return false;
            }
        } else {
            Logger::error("Application", "❌ API server not initialized");
            return false;
        }
        
        // Start monitoring threads
		startMonitoringThreads();
        Logger::info("Application", "✓ Monitoring threads started");
        
        running_ = true;
        startTime_ = std::chrono::steady_clock::now();
        Logger::info("Application", "");
        Logger::info("Application", "╔═══════════════════════════════════════╗");
        Logger::info("Application", "║   MidiMind Ready ✓                   ║");
        Logger::info("Application", "╚═══════════════════════════════════════╝");
        Logger::info("Application", "");
        Logger::info("Application", "Press Ctrl+C to shutdown gracefully");
        Logger::info("Application", "");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Start failed: " + std::string(e.what()));
        running_ = false;
        return false;
    }
}

// ============================================================================
// LIFECYCLE - RUN
// ============================================================================

void Application::run() {
    if (!running_.load()) {
        Logger::error("Application", "Cannot run: not started");
        return;
    }
    
    Logger::info("Application", "Main loop started");
    
    // Main loop: wait for shutdown signal
    while (!g_shutdownRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    Logger::info("Application", "Main loop exited");
}

// ============================================================================
// LIFECYCLE - STOP
// ============================================================================

void Application::stop() {
    if (!running_.load()) {
        Logger::warning("Application", "Not running");
        return;
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "╔═══════════════════════════════════════╗");
    Logger::info("Application", "║   Shutting Down MidiMind             ║");
    Logger::info("Application", "╚═══════════════════════════════════════╝");
    Logger::info("Application", "");
    
    try {
        // Stop monitoring threads
        Logger::info("Application", "Stopping monitoring threads...");
        stopMonitoringThreads();
        Logger::info("Application", "✓ Monitoring stopped");
        
        // Stop API server
        Logger::info("Application", "Stopping API server...");
        if (apiServer_) {
            apiServer_->stop();
        }
        Logger::info("Application", "✓ API server stopped");
        /*
        // Save state
        Logger::info("Application", "Saving state...");
        if (latencyCompensator_) {
            latencyCompensator_->saveToDatabase();
        }
        Logger::info("Application", "✓ State saved");
        */
        running_ = false;
        
        Logger::info("Application", "");
        Logger::info("Application", "╔═══════════════════════════════════════╗");
        Logger::info("Application", "║   Shutdown Complete ✓                ║");
        Logger::info("Application", "╚═══════════════════════════════════════╝");
        Logger::info("Application", "");
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Shutdown error: " + std::string(e.what()));
    }
}

// ============================================================================
// MONITORING
// ============================================================================

void Application::startMonitoringThreads() {
    statusBroadcastRunning_ = true;
    
    // Status broadcast thread
    statusBroadcastThread_ = std::thread([this]() {
        Logger::debug("Application", "Status broadcast thread started");
        
        while (statusBroadcastRunning_.load()) {
            try {
                // Broadcast status every 5 seconds
                broadcastStatus();
                std::this_thread::sleep_for(std::chrono::seconds(5));
                
            } catch (const std::exception& e) {
                Logger::error("Application", 
                            "Status broadcast error: " + std::string(e.what()));
            }
        }
        
        Logger::debug("Application", "Status broadcast thread stopped");
    });
}

void Application::stopMonitoringThreads() {
    statusBroadcastRunning_ = false;
    
    if (statusBroadcastThread_.joinable()) {
        statusBroadcastThread_.join();
    }
}

void Application::broadcastStatus() {
    if (!apiServer_) return;
    
    json status = {
        {"type", "status"},
        {"timestamp",std::chrono::system_clock::now().time_since_epoch().count()},
        {"uptime", getUptime()},
        {"components", {
            {"database", database_ && database_->isConnected()},
            {"api_server", apiServer_ && apiServer_->isRunning()},
            {"midi_devices", deviceManager_ ? deviceManager_->getDeviceCount() : 0},
            {"latency_compensator", latencyCompensator_ != nullptr}
        }}
    };
    auto event = MessageEnvelope::createEvent("system:status", status);
apiServer_->broadcast(event);
}

// ============================================================================
// SIGNAL HANDLING
// ============================================================================

void Application::setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    Logger::debug("Application", "Signal handlers installed");
}

// ============================================================================
// GETTERS
// ============================================================================


std::string Application::getVersion() const {
    return "4.1.0";
}

std::string Application::getProtocolVersion() const {
    return "1.0";
}

int Application::getUptime() const {
    if (!startTime_.has_value()) {
        return 0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - startTime_.value());
    
    return static_cast<int>(duration.count());
}

} // namespace midiMind

// ============================================================================
// END OF FILE Application.cpp
// ============================================================================
// ============================================================================
// File: backend/src/core/Application.cpp
// Version: 4.2.4 - FIX MidiPlayer and ApiServer constructors
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#include "Application.h"
#include "Logger.h"
#include "Config.h"
#include "Error.h"
#include "TimeUtils.h"
#include "EventBus.h"
#include "../storage/Database.h"
#include "../storage/Settings.h"
#include "../storage/PathManager.h"
#include "../storage/FileManager.h"
#include "../storage/InstrumentDatabase.h"
#include "../storage/PresetManager.h"
#include "../timing/LatencyCompensator.h"
#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/player/MidiPlayer.h"
#include "../api/ApiServer.h"
#include "../api/CommandHandler.h"
#include "../api/MessageEnvelope.h"

#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include <future>

namespace midiMind {

// ============================================================================
// STATIC MEMBERS
// ============================================================================

std::atomic<int> Application::signalCount_{0};
static volatile sig_atomic_t g_shutdownRequested = 0;

// ============================================================================
// SIGNAL HANDLER (ASYNC-SIGNAL-SAFE)
// ============================================================================

void signalHandler(int signal) {
    (void)signal;  // Unused parameter
    
    int count = Application::signalCount_.fetch_add(1, std::memory_order_relaxed) + 1;
    
    if (count == 1) {
        g_shutdownRequested = 1;
        const char* msg = "\n[SIGNAL] Shutdown signal received (Ctrl+C)\n";
        (void)write(STDERR_FILENO, msg, strlen(msg));
    } else if (count == 2) {
        g_shutdownRequested = 1;
        const char* msg = "\n[SIGNAL] Second signal - forcing shutdown...\n";
        (void)write(STDERR_FILENO, msg, strlen(msg));
    } else {
        const char* msg = "\n[SIGNAL] Third signal - immediate exit!\n";
        (void)write(STDERR_FILENO, msg, strlen(msg));
        std::_Exit(1);
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
    , database_(nullptr)
{
    Logger::info("Application", "Application instance created");
}

Application::~Application() {
    Logger::info("Application", "Destroying application...");
    
    stopMonitoringThreads();
    
    Logger::debug("Application", "Releasing components...");
    
    eventBus_.reset();
    commandHandler_.reset();
    apiServer_.reset();
    player_.reset();
    router_.reset();
    deviceManager_.reset();
    latencyCompensator_.reset();
    presetManager_.reset();
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
        {"preset_manager", presetManager_ != nullptr},
        {"latency_compensator", latencyCompensator_ != nullptr},
        {"device_manager", deviceManager_ != nullptr},
        {"router", router_ != nullptr},
        {"player", player_ != nullptr},
        {"api_server", apiServer_ != nullptr},
        {"event_bus", eventBus_ != nullptr}
    };
    
    return status;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool Application::initialize(const std::string& configPath) {
    if (initialized_.load()) {
        Logger::warning("Application", "Already initialized");
        return true;
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "╔═══════════════════════════════════════╗");
    Logger::info("Application", "║   MidiMind Initialization v4.2.4     ║");
    Logger::info("Application", "╚═══════════════════════════════════════╝");
    Logger::info("Application", "");
    
    setupSignalHandlers();
    
    if (!initializeConfiguration(configPath)) return false;
    if (!initializeDatabase()) return false;
    if (!initializeStorage()) return false;
    if (!initializeEventSystem()) return false;  // Phase 4: EventBus AVANT Timing
    if (!initializeTiming()) return false;        // Phase 5
    if (!initializeMidi()) return false;          // Phase 6
    if (!initializeApi()) return false;           // Phase 7: API utilise EventBus
    
    initialized_ = true;
    
    Logger::info("Application", "");
    Logger::info("Application", "╔═══════════════════════════════════════╗");
    Logger::info("Application", "║   Initialization Complete ✓          ║");
    Logger::info("Application", "╚═══════════════════════════════════════╝");
    Logger::info("Application", "");
    
    return true;
}

bool Application::initializeConfiguration(const std::string& configPath) {
    Logger::info("Application", "┌─── Phase 1/7: Configuration ─────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Initializing PathManager...");
        PathManager::initialize();
        Logger::info("Application", "  ✓ PathManager ready");
        
        Logger::info("Application", "  Loading configuration...");
        std::string path = configPath.empty() ? "config.json" : configPath;
        
        if (!Config::instance().load(path)) {
            Logger::warning("Application", "  Config not found, using defaults");
            
            // Hardcoded defaults
            Config::instance().set("database.path", "/var/lib/midimind/midimind.db");
            Config::instance().set("database.migrations", "data/migrations");
            Config::instance().set("api.host", "0.0.0.0");
            Config::instance().set("api.port", 8080);
            Config::instance().set("log.level", "info");
            Config::instance().set("log.file", "/var/log/midimind/midimind.log");
            Config::instance().set("midi.scan_on_startup", true);
            Config::instance().set("midi.auto_connect", false);
            
            if (!Config::instance().save(path)) {
                Logger::warning("Application", "  Failed to save default config");
            }
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
        Logger::info("Application", "  Connecting to database...");
        database_ = Database::instance();
        
        std::string dbPath = Config::instance().getString("database.path", 
                                                          "/var/lib/midimind/midimind.db");
        
        if (!database_->connect(dbPath)) {
            Logger::error("Application", "  Database connection failed");
            return false;
        }
        Logger::info("Application", "  ✓ Database connected");
        
        Logger::info("Application", "  Running migrations...");
        std::string migrationsPath = Config::instance().getString("database.migrations", 
                                                                  "data/migrations");
        
        if (!database_->runMigrations(migrationsPath)) {
            Logger::error("Application", "  Migration failed");
            return false;
        }
        Logger::info("Application", "  ✓ Migrations complete");
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Database initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeStorage() {
    Logger::info("Application", "┌─── Phase 3/7: Storage ───────────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Initializing Settings...");
        settings_ = std::make_shared<Settings>();
        if (!settings_->load()) {
            Logger::warning("Application", "  Failed to load settings (using defaults)");
        } else {
            Logger::info("Application", "  ✓ Settings loaded");
        }
        
        Logger::info("Application", "  Initializing FileManager...");
        fileManager_ = std::make_shared<FileManager>();
        Logger::info("Application", "  ✓ FileManager initialized");
        
        Logger::info("Application", "  Initializing InstrumentDatabase...");
        instrumentDatabase_ = std::make_shared<InstrumentDatabase>();
        Logger::info("Application", "  ✓ InstrumentDatabase initialized");
        
        Logger::info("Application", "  Initializing PresetManager...");
        presetManager_ = std::make_shared<PresetManager>(*database_);
        Logger::info("Application", "  ✓ PresetManager initialized");
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Storage initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeEventSystem() {
    Logger::info("Application", "┌─── Phase 4/7: Event System ──────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Initializing EventBus...");
        eventBus_ = std::make_shared<EventBus>();
        Logger::info("Application", "  ✓ EventBus initialized");
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Event system initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeTiming() {
    Logger::info("Application", "┌─── Phase 5/7: Timing ────────────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Initializing LatencyCompensator...");
        latencyCompensator_ = std::make_shared<LatencyCompensator>(*instrumentDatabase_);
        Logger::info("Application", "  ✓ LatencyCompensator initialized");
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Timing initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeMidi() {
    Logger::info("Application", "┌─── Phase 6/7: MIDI ──────────────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Initializing MidiDeviceManager...");
        deviceManager_ = std::make_shared<MidiDeviceManager>();
        Logger::info("Application", "  ✓ MidiDeviceManager initialized");
        
        Logger::info("Application", "  Initializing MidiRouter...");
        router_ = std::make_shared<MidiRouter>();
        Logger::info("Application", "  ✓ MidiRouter initialized");
        
        Logger::info("Application", "  Initializing MidiPlayer...");
        player_ = std::make_shared<MidiPlayer>(router_, eventBus_);
        Logger::info("Application", "  ✓ MidiPlayer initialized");
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "MIDI initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeApi() {
    Logger::info("Application", "┌─── Phase 7/7: API ───────────────────┐");
    Logger::info("Application", "");
    
    try {
        Logger::info("Application", "  Initializing CommandHandler...");
        commandHandler_ = std::make_shared<CommandHandler>(
            deviceManager_,
            router_,
            player_,
            fileManager_,
            latencyCompensator_,
            instrumentDatabase_,
            presetManager_,
            eventBus_
        );
        Logger::info("Application", "  ✓ CommandHandler initialized");
        
        Logger::info("Application", "  Initializing ApiServer...");
        apiServer_ = std::make_shared<ApiServer>(eventBus_);
        Logger::info("Application", "  ✓ ApiServer initialized");
        
        Logger::info("Application", "");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "API initialization failed: " + std::string(e.what()));
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
    
    try {
        int port = Config::instance().getInt("api.port", 8080);
        
        Logger::info("Application", "Starting API server on port " + std::to_string(port) + "...");
        apiServer_->start(port);
        Logger::info("Application", "✓ API server started");
        
        Logger::info("Application", "Starting monitoring threads...");
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
    
    while (!g_shutdownRequested) {
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
        Logger::info("Application", "Stopping monitoring threads...");
        stopMonitoringThreads();
        Logger::info("Application", "✓ Monitoring stopped");
        
        Logger::info("Application", "Stopping API server...");
        if (apiServer_) {
            apiServer_->stop();
        }
        Logger::info("Application", "✓ API server stopped");
        
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
// MONITORING (THREAD-SAFE WITH TIMEOUT)
// ============================================================================

void Application::startMonitoringThreads() {
    statusBroadcastRunning_ = true;
    
    // Capture par copie des shared_ptr pour éviter dangling
    auto apiServerCopy = apiServer_;
    auto deviceManagerCopy = deviceManager_;
    auto latencyCompensatorCopy = latencyCompensator_;
    
    statusBroadcastThread_ = std::thread([this, apiServerCopy, deviceManagerCopy, latencyCompensatorCopy]() {
        Logger::debug("Application", "Status broadcast thread started");
        
        while (statusBroadcastRunning_.load()) {
            try {
                if (apiServerCopy && running_.load()) {
                    auto now = std::chrono::system_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()).count();
                    
                    json status = {
                        {"type", "status"},
                        {"timestamp", ms},
                        {"uptime", getUptime()},
                        {"components", {
                            {"database", database_ && database_->isConnected()},
                            {"api_server", apiServerCopy && apiServerCopy->isRunning()},
                            {"midi_devices", deviceManagerCopy ? deviceManagerCopy->getDeviceCount() : 0},
                            {"latency_compensator", latencyCompensatorCopy != nullptr}
                        }}
                    };
                    
                    auto event = MessageEnvelope::createEvent("system:status", status);
                    apiServerCopy->broadcast(event);
                }
                
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
        auto future = std::async(std::launch::async, [this]() {
            statusBroadcastThread_.join();
        });
        
        if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            Logger::warning("Application", "Status broadcast thread join timeout - detaching");
            statusBroadcastThread_.detach();
        }
    }
}

void Application::broadcastStatus() {
    if (!apiServer_) return;
    
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    json status = {
        {"type", "status"},
        {"timestamp", ms},
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
    return "4.2.4";
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
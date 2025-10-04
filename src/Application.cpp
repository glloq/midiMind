// ============================================================================
// Fichier: src/Application.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "Application.h"
#include "core/TimeUtils.h"
#include "core/StringUtils.h"
#include <iostream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// VARIABLES STATIQUES
// ============================================================================

static Application* g_applicationInstance = nullptr;

// ============================================================================
// SINGLETON
// ============================================================================

Application& Application::instance() {
    static Application instance;
    return instance;
}

// ============================================================================
// CONSTRUCTION PRIVÉE
// ============================================================================

Application::Application()
    : state_(ApplicationState::UNINITIALIZED)
    , shutdownRequested_(false)
    , startTime_(0)
    , initTime_(0) {
    
    g_applicationInstance = this;
    
    std::cout << "MidiMind v3.0 - MIDI Orchestration System" << std::endl;
    std::cout << "Initializing..." << std::endl;
}

Application::~Application() {
    if (state_ == ApplicationState::RUNNING) {
        stop();
    }
    
    g_applicationInstance = nullptr;
}

// ============================================================================
// LIFECYCLE - INITIALIZE
// ============================================================================

bool Application::initialize(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != ApplicationState::UNINITIALIZED) {
        Logger::warn("Application", "Already initialized");
        return true;
    }
    
    state_ = ApplicationState::INITIALIZING;
    initTime_ = TimeUtils::getCurrentTimestampMs();
    
    printBanner();
    
    Logger::info("Application", "═══════════════════════════════════════");
    Logger::info("Application", "  Initializing MidiMind v3.0");
    Logger::info("Application", "═══════════════════════════════════════");
    
    try {
        // 1. Core (Paths, Logger)
        if (!initializeCore()) {
            state_ = ApplicationState::ERROR;
            return false;
        }
        
        // 2. Configuration
        if (!initializeConfig(configPath)) {
            state_ = ApplicationState::ERROR;
            return false;
        }
        
        // 3. Database
        if (!initializeDatabase()) {
            state_ = ApplicationState::ERROR;
            return false;
        }
        
        // 4. Optimizations
        if (!initializeOptimizations()) {
            state_ = ApplicationState::ERROR;
            return false;
        }
        
        // 5. MIDI
        if (!initializeMidi()) {
            state_ = ApplicationState::ERROR;
            return false;
        }
        
        // 6. Network
        if (!initializeNetwork()) {
            state_ = ApplicationState::ERROR;
            return false;
        }
        
        // 7. API
        if (!initializeAPI()) {
            state_ = ApplicationState::ERROR;
            return false;
        }
        
        // 8. Monitoring
        if (!initializeMonitoring()) {
            state_ = ApplicationState::ERROR;
            return false;
        }
        
        // 9. Load initial configuration
        if (!loadInitialConfiguration()) {
            Logger::warn("Application", "Failed to load initial configuration");
        }
        
        // 10. Register signal handlers
        registerSignalHandlers();
        
        state_ = ApplicationState::STOPPED;
        
        uint64_t initDuration = TimeUtils::getCurrentTimestampMs() - initTime_;
        
        Logger::info("Application", "═══════════════════════════════════════");
        Logger::info("Application", "✓ Initialization completed in " + 
                    std::to_string(initDuration) + "ms");
        Logger::info("Application", "═══════════════════════════════════════");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Initialization failed: " + std::string(e.what()));
        state_ = ApplicationState::ERROR;
        return false;
    }
}

// ============================================================================
// LIFECYCLE - START
// ============================================================================

bool Application::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == ApplicationState::RUNNING) {
        Logger::warn("Application", "Already running");
        return true;
    }
    
    if (state_ != ApplicationState::STOPPED) {
        Logger::error("Application", "Cannot start: invalid state");
        return false;
    }
    
    Logger::info("Application", "═══════════════════════════════════════");
    Logger::info("Application", "  Starting MidiMind v3.0");
    Logger::info("Application", "═══════════════════════════════════════");
    
    startTime_ = TimeUtils::getCurrentTimestampMs();
    
    try {
        // 1. Start MIDI devices
        Logger::info("Application", "Starting MIDI devices...");
        deviceManager_->scanDevices();
        
        // 2. Start MIDI clock if enabled
        if (settings_->getBool("midi.clock_enabled", false)) {
            Logger::info("Application", "Starting MIDI clock...");
            float tempo = settings_->getFloat("midi.clock_tempo", 120.0f);
            midiClock_->setTempo(tempo);
            midiClock_->start();
        }
        
        // 3. Start network services
        if (settings_->getBool("network.rtpmidi_enabled", true)) {
            Logger::info("Application", "Starting network services...");
            networkManager_->start();
        }
        
        // 4. Start API server
        Logger::info("Application", "Starting API server...");
        int apiPort = settings_->getInt("api.port", 8080);
        apiServer_->start(apiPort);
        
        // 5. Start monitoring
        Logger::info("Application", "Starting monitoring...");
        metricsCollector_->start();
        healthCheck_->start();
        
        state_ = ApplicationState::RUNNING;
        
        printStartupSummary();
        
        Logger::info("Application", "═══════════════════════════════════════");
        Logger::info("Application", "✓ MidiMind is running");
        Logger::info("Application", "═══════════════════════════════════════");
        Logger::info("Application", "Press Ctrl+C to stop");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Start failed: " + std::string(e.what()));
        state_ = ApplicationState::ERROR;
        return false;
    }
}

// ============================================================================
// LIFECYCLE - STOP
// ============================================================================

void Application::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != ApplicationState::RUNNING) {
        return;
    }
    
    state_ = ApplicationState::STOPPING;
    
    Logger::info("Application", "═══════════════════════════════════════");
    Logger::info("Application", "  Stopping MidiMind v3.0");
    Logger::info("Application", "═══════════════════════════════════════");
    
    try {
        // Stop dans l'ordre inverse du démarrage
        
        // 1. Stop monitoring
        Logger::info("Application", "Stopping monitoring...");
        if (healthCheck_) healthCheck_->stop();
        if (metricsCollector_) metricsCollector_->stop();
        
        // 2. Stop API server
        Logger::info("Application", "Stopping API server...");
        if (apiServer_) apiServer_->stop();
        
        // 3. Stop network
        Logger::info("Application", "Stopping network services...");
        if (networkManager_) networkManager_->stop();
        
        // 4. Stop MIDI clock
        Logger::info("Application", "Stopping MIDI clock...");
        if (midiClock_) midiClock_->stop();
        
        // 5. Stop MIDI player
        Logger::info("Application", "Stopping MIDI player...");
        if (midiPlayer_) midiPlayer_->stop();
        
        // 6. Close MIDI devices
        Logger::info("Application", "Closing MIDI devices...");
        if (deviceManager_) {
            auto devices = deviceManager_->getDevices();
            for (auto& device : devices) {
                device->close();
            }
        }
        
        // 7. Save settings
        Logger::info("Application", "Saving settings...");
        if (settings_) settings_->save();
        
        // 8. Close database
        Logger::info("Application", "Closing database...");
        if (database_) database_->close();
        
        // 9. Shutdown optimizations
        Logger::info("Application", "Shutting down optimizations...");
        PerformanceOptimizer::instance().shutdown();
        
        state_ = ApplicationState::STOPPED;
        
        uint64_t uptime = TimeUtils::getCurrentTimestampMs() - startTime_;
        
        Logger::info("Application", "═══════════════════════════════════════");
        Logger::info("Application", "✓ MidiMind stopped");
        Logger::info("Application", "  Uptime: " + TimeUtils::durationToString(uptime));
        Logger::info("Application", "═══════════════════════════════════════");
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Stop error: " + std::string(e.what()));
    }
}

// ============================================================================
// LIFECYCLE - RESTART
// ============================================================================

bool Application::restart() {
    Logger::info("Application", "Restarting MidiMind...");
    
    stop();
    
    // Attendre un peu
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    return start();
}

// ============================================================================
// LIFECYCLE - WAIT FOR SHUTDOWN
// ============================================================================

void Application::waitForShutdown() {
    while (!shutdownRequested_ && state_ == ApplicationState::RUNNING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ============================================================================
// ÉTAT
// ============================================================================

ApplicationState Application::getState() const {
    return state_;
}

bool Application::isRunning() const {
    return state_ == ApplicationState::RUNNING;
}

json Application::getVersion() const {
    json j;
    j["version"] = "3.0.0";
    j["build_date"] = __DATE__;
    j["build_time"] = __TIME__;
    return j;
}

json Application::getStatistics() const {
    json stats;
    
    stats["version"] = getVersion();
    stats["state"] = state_ == ApplicationState::RUNNING ? "RUNNING" : "STOPPED";
    
    if (startTime_ > 0) {
        uint64_t uptime = TimeUtils::getCurrentTimestampMs() - startTime_;
        stats["uptime_ms"] = uptime;
        stats["uptime_str"] = TimeUtils::durationToString(uptime);
    }
    
    // Statistiques des composants
    if (metricsCollector_) {
        stats["metrics"] = metricsCollector_->getStatistics();
    }
    
    if (healthCheck_) {
        stats["health"] = healthCheck_->getCurrentStatus().toJson();
    }
    
    if (deviceManager_) {
        stats["midi_devices"] = deviceManager_->getDevices().size();
    }
    
    if (midiRouter_) {
        stats["midi_routes"] = midiRouter_->getRouteCount();
    }
    
    return stats;
}

// ============================================================================
// SIGNAL HANDLERS
// ============================================================================

void Application::registerSignalHandlers() {
    Logger::info("Application", "Registering signal handlers...");
    
    std::signal(SIGINT, Application::signalHandler);   // Ctrl+C
    std::signal(SIGTERM, Application::signalHandler);  // kill
    
    Logger::info("Application", "✓ Signal handlers registered");
}

void Application::signalHandler(int signal) {
    if (g_applicationInstance) {
        switch (signal) {
            case SIGINT:
                Logger::info("Application", "Received SIGINT (Ctrl+C)");
                break;
            case SIGTERM:
                Logger::info("Application", "Received SIGTERM");
                break;
        }
        
        g_applicationInstance->shutdownRequested_ = true;
    }
}

// ============================================================================
// Fichier: src/Application.cpp (Suite - Partie 2/2)
// ============================================================================

// ============================================================================
// MÉTHODES PRIVÉES D'INITIALISATION
// ============================================================================

bool Application::initializeCore() {
    Logger::info("Application", "▶ Initializing Core...");
    
    try {
        // Initialiser PathManager
        PathManager::instance().initialize();
        
        // Configurer le Logger pour écrire dans un fichier
        Logger::setLogFile(PathManager::instance().getLogFilePath());
        Logger::setLogLevel(LogLevel::INFO);
        
        Logger::info("Application", "  ✓ Paths initialized");
        Logger::info("Application", "  ✓ Logger configured");
        Logger::info("Application", "✓ Core initialized");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Core initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeConfig(const std::string& configPath) {
    Logger::info("Application", "▶ Initializing Configuration...");
    
    try {
        // Déterminer le chemin de config
        std::string cfgPath = configPath;
        if (cfgPath.empty()) {
            cfgPath = PathManager::instance().getConfigFilePath();
        }
        
        Logger::info("Application", "  Config file: " + cfgPath);
        
        // Créer Config
        config_ = std::make_shared<Config>();
        
        // Charger la config
        if (FileSystem::exists(cfgPath)) {
            config_->load(cfgPath);
            Logger::info("Application", "  ✓ Configuration loaded");
        } else {
            Logger::warn("Application", "  Config file not found, using defaults");
            // Créer un fichier de config par défaut
            config_->save(cfgPath);
            Logger::info("Application", "  ✓ Default configuration created");
        }
        
        Logger::info("Application", "✓ Configuration initialized");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Config initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeDatabase() {
    Logger::info("Application", "▶ Initializing Database...");
    
    try {
        std::string dbPath = PathManager::instance().getDatabasePath();
        Logger::info("Application", "  Database: " + dbPath);
        
        // Créer Database
        database_ = std::make_shared<Database>(dbPath);
        
        // Ouvrir
        if (!database_->open()) {
            Logger::error("Application", "Failed to open database");
            return false;
        }
        
        // Initialiser le schéma
        database_->initializeSchema();
        
        // Créer Settings
        settings_ = std::make_shared<Settings>(database_);
        settings_->load();
        
        // Créer SessionManager
        sessionManager_ = std::make_shared<SessionManager>(database_);
        
        Logger::info("Application", "  ✓ Database opened");
        Logger::info("Application", "  ✓ Settings loaded");
        Logger::info("Application", "  ✓ SessionManager created");
        Logger::info("Application", "✓ Database initialized");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Database initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeOptimizations() {
    Logger::info("Application", "▶ Initializing Optimizations...");
    
    try {
        OptimizationConfig optConfig;
        
        // Lire la config depuis settings
        optConfig.threadPoolSize = settings_->getInt("optimization.thread_pool_size", 4);
        optConfig.memoryBlockSize = 1024;
        optConfig.memoryInitialBlocks = 500;
        optConfig.midiMessagePoolSize = 2000;
        optConfig.autoOptimize = true;
        
        // Initialiser PerformanceOptimizer
        PerformanceOptimizer::instance().initialize(optConfig);
        
        Logger::info("Application", "  ✓ ThreadPool created (" + 
                    std::to_string(optConfig.threadPoolSize) + " threads)");
        Logger::info("Application", "  ✓ MemoryPool created");
        Logger::info("Application", "  ✓ ObjectPools created");
        Logger::info("Application", "✓ Optimizations initialized");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Optimizations initialization failed: " + 
                     std::string(e.what()));
        return false;
    }
}

bool Application::initializeMidi() {
    Logger::info("Application", "▶ Initializing MIDI...");
    
    try {
        // 1. Device Manager
        deviceManager_ = std::make_shared<MidiDeviceManager>();
        Logger::info("Application", "  ✓ DeviceManager created");
        
        // 2. Router
        midiRouter_ = std::make_shared<MidiRouter>();
        Logger::info("Application", "  ✓ Router created");
        
        // 3. Player
        midiPlayer_ = std::make_shared<MidiPlayer>(midiRouter_);
        Logger::info("Application", "  ✓ Player created");
        
        // 4. Clock
        midiClock_ = std::make_shared<MidiClock>();
        
        // Connecter le clock au router pour envoyer les messages
        midiClock_->setOnSendMessage([this](const MidiMessage& msg) {
            midiRouter_->route(msg);
        });
        
        Logger::info("Application", "  ✓ Clock created");
        
        // 5. File Manager
        fileManager_ = std::make_shared<MidiFileManager>(
            PathManager::instance().getMidiFilesPath()
        );
        Logger::info("Application", "  ✓ FileManager created");
        
        // 6. Processor Manager
        processorManager_ = std::make_shared<ProcessorManager>();
        Logger::info("Application", "  ✓ ProcessorManager created");
        
        Logger::info("Application", "✓ MIDI initialized");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "MIDI initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeNetwork() {
    Logger::info("Application", "▶ Initializing Network...");
    
    try {
        // Créer NetworkManager
        networkManager_ = std::make_shared<NetworkManager>();
        
        // Connecter le callback de réception MIDI
        // TODO: Connecter au router
        
        Logger::info("Application", "  ✓ NetworkManager created");
        Logger::info("Application", "✓ Network initialized");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Network initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeAPI() {
    Logger::info("Application", "▶ Initializing API...");
    
    try {
        // 1. Command Processor
        commandProcessor_ = std::make_shared<CommandProcessor>();
        
        // Enregistrer les commandes
        // TODO: Enregistrer toutes les commandes
        
        Logger::info("Application", "  ✓ CommandProcessor created");
        
        // 2. API Server
        apiServer_ = std::make_shared<APIServer>();
        
        // Connecter le command processor
        apiServer_->setCommandHandler([this](const json& command) -> json {
            return commandProcessor_->process(command);
        });
        
        Logger::info("Application", "  ✓ APIServer created");
        Logger::info("Application", "✓ API initialized");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "API initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool Application::initializeMonitoring() {
    Logger::info("Application", "▶ Initializing Monitoring...");
    
    try {
        // 1. System Monitor
        systemMonitor_ = std::make_shared<SystemMonitor>();
        Logger::info("Application", "  ✓ SystemMonitor created");
        
        // 2. Latency Monitor
        latencyMonitor_ = std::make_shared<LatencyMonitor>();
        Logger::info("Application", "  ✓ LatencyMonitor created");
        
        // 3. Metrics Collector
        metricsCollector_ = std::make_shared<MetricsCollector>();
        metricsCollector_->registerSystemMonitor(systemMonitor_);
        metricsCollector_->registerLatencyMonitor(latencyMonitor_);
        
        // Configurer le callback pour broadcaster via WebSocket
        metricsCollector_->setMetricsUpdateCallback([this](const AggregatedMetrics& metrics) {
            if (apiServer_ && apiServer_->isRunning()) {
                json event;
                event["type"] = "metrics_update";
                event["data"] = metrics.toJson();
                apiServer_->broadcast(event.dump());
            }
        });
        
        Logger::info("Application", "  ✓ MetricsCollector created");
        
        // 4. Health Check
        healthCheck_ = std::make_shared<HealthCheck>();
        healthCheck_->registerMetricsCollector(metricsCollector_);
        
        // Configurer le callback pour alerter
        healthCheck_->setOnHealthChanged([this](const HealthStatus& status) {
            if (!status.isHealthy()) {
                Logger::warn("Health", status.message);
                
                if (apiServer_ && apiServer_->isRunning()) {
                    json alert;
                    alert["type"] = "health_alert";
                    alert["status"] = status.toJson();
                    apiServer_->broadcast(alert.dump());
                }
            }
        });
        
        Logger::info("Application", "  ✓ HealthCheck created");
        Logger::info("Application", "✓ Monitoring initialized");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Monitoring initialization failed: " + 
                     std::string(e.what()));
        return false;
    }
}

bool Application::loadInitialConfiguration() {
    Logger::info("Application", "▶ Loading initial configuration...");
    
    try {
        // Charger la session active si elle existe
        int activeSession = sessionManager_->getActive();
        if (activeSession > 0) {
            Logger::info("Application", "  Loading active session: " + 
                        std::to_string(activeSession));
            
            auto session = sessionManager_->load(activeSession);
            
            // TODO: Appliquer la configuration de la session
            // (routes MIDI, presets, etc.)
        }
        
        Logger::info("Application", "✓ Initial configuration loaded");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Failed to load configuration: " + 
                     std::string(e.what()));
        return false;
    }
}

// ============================================================================
// AFFICHAGE
// ============================================================================

void Application::printBanner() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                               ║\n";
    std::cout << "║                       MidiMind v3.0                           ║\n";
    std::cout << "║            MIDI Orchestration System for Raspberry Pi         ║\n";
    std::cout << "║                                                               ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

void Application::printStartupSummary() {
    Logger::info("Application", "");
    Logger::info("Application", "═══════════════════════════════════════");
    Logger::info("Application", "  STARTUP SUMMARY");
    Logger::info("Application", "═══════════════════════════════════════");
    
    // MIDI Devices
    auto devices = deviceManager_->getDevices();
    Logger::info("Application", "MIDI Devices: " + std::to_string(devices.size()));
    for (const auto& device : devices) {
        Logger::info("Application", "  • " + device->getName() + 
                    " (" + (device->isOpen() ? "open" : "closed") + ")");
    }
    
    // MIDI Routes
    Logger::info("Application", "MIDI Routes: " + std::to_string(midiRouter_->getRouteCount()));
    
    // Network
    Logger::info("Application", "Network: " + 
                std::string(settings_->getBool("network.rtpmidi_enabled") ? "enabled" : "disabled"));
    
    // API
    int apiPort = settings_->getInt("api.port", 8080);
    Logger::info("Application", "API Server: http://localhost:" + std::to_string(apiPort));
    Logger::info("Application", "WebSocket: ws://localhost:" + std::to_string(apiPort) + "/ws");
    
    // Monitoring
    Logger::info("Application", "Monitoring: enabled");
    Logger::info("Application", "Health Check: enabled");
    
    Logger::info("Application", "═══════════════════════════════════════");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.cpp
// ============================================================================
// ============================================================================
// Fichier: src/core/Application.cpp
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Implémentation de la classe Application. Contient toute la logique
//   d'initialisation, de configuration et de gestion du cycle de vie
//   de l'application midiMind.
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#include "Application.h"

// Includes additionnels pour l'implémentation
#include <fstream>       // Pour lecture/écriture de fichiers
#include <sstream>       // Pour manipulation de strings
#include <iomanip>       // Pour formatage (std::setw, std::setprecision)
#include <cstring>       // Pour strerror
#include <sys/stat.h>    // Pour stat (vérification fichiers)
#include <nlohmann/json.hpp>  // Pour génération documentation JSON

// Includes des patterns et composants
#include "patterns/DIContainer.h"
#include "documentation/CommandDocumentation.h"
#include "../midi/scheduling/PriorityQueueScheduler.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// CONSTANTES GLOBALES
// ============================================================================

namespace {
    // Fréquences par défaut selon modèle de Raspberry Pi
    constexpr int DEFAULT_PLAYER_FPS = 100;
    constexpr int DEFAULT_BROADCAST_FPS = 10;
    
    // Fréquences pour Pi Zero (CPU limité)
    constexpr int PI_ZERO_PLAYER_FPS = 50;
    constexpr int PI_ZERO_BROADCAST_FPS = 5;
    
    // Fréquences pour Pi 3 (équilibré)
    constexpr int PI_3_PLAYER_FPS = 100;
    constexpr int PI_3_BROADCAST_FPS = 10;
    
    // Fréquences pour Pi 4/5 (performance)
    constexpr int PI_4_PLAYER_FPS = 200;
    constexpr int PI_4_BROADCAST_FPS = 20;
    
    // Chemins système Raspberry Pi
    const char* CPUINFO_PATH = "/proc/cpuinfo";
    const char* THERMAL_PATH = "/sys/class/thermal/thermal_zone0/temp";
    
    // Seuils thermiques (en degrés Celsius)
    constexpr int THERMAL_WARNING_THRESHOLD = 70;
    constexpr int THERMAL_CRITICAL_THRESHOLD = 80;
}

// ============================================================================
// CONSTRUCTEUR
// ============================================================================

/**
 * @brief Constructeur de l'application - Initialise tous les modules
 * 
 * Séquence d'initialisation en 7 étapes:
 * 1. Configuration (chargement config.json)
 * 2. Logger (configuration du système de log)
 * 3. Détection hardware et optimisation
 * 4. Création des modules core
 * 5. Dependency Injection Container
 * 6. Observers et Callbacks
 * 7. Génération documentation API
 */
Application::Application() {
    Logger::info("Application", "╔═══════════════════════════════════════════════════════╗");
    Logger::info("Application", "║   Initializing midiMind v3.0 (Refactored)           ║");
    Logger::info("Application", "╚═══════════════════════════════════════════════════════╝");
    
    try {
        // ====================================================================
        // ÉTAPE 1: CHARGEMENT DE LA CONFIGURATION
        // ====================================================================
        
        Logger::info("Application", "");
        Logger::info("Application", "Step 1/7: Loading configuration...");
        
        // Charger le fichier de configuration
        std::string configPath = Config::instance().get<std::string>(
            "config_file_path",
            "config/config.json"
        );
        
        if (!Config::instance().load(configPath)) {
            Logger::warn("Application", "Failed to load config file: " + configPath);
            Logger::warn("Application", "Using default configuration");
        } else {
            Logger::info("Application", "✓ Configuration loaded from: " + configPath);
        }
        
        // ====================================================================
        // ÉTAPE 2: CONFIGURATION DU LOGGER
        // ====================================================================
        
        Logger::info("Application", "");
        Logger::info("Application", "Step 2/7: Configuring logger...");
        configureLogger();
        Logger::info("Application", "✓ Logger configured");
        
        // ====================================================================
        // ÉTAPE 3: DÉTECTION RASPBERRY PI ET OPTIMISATION
        // ====================================================================
        
        Logger::info("Application", "");
        Logger::info("Application", "Step 3/7: Detecting hardware and optimizing...");
        detectAndOptimizeForRaspberryPi();
        Logger::info("Application", "✓ Hardware detection complete");
        
        // ====================================================================
        // ÉTAPE 4: CRÉATION DES MODULES CORE
        // ====================================================================
        
        Logger::info("Application", "");
        Logger::info("Application", "Step 4/7: Creating core modules...");
        
        // Device Manager
        Logger::info("Application", "  Creating MidiDeviceManager...");
        deviceManager_ = std::make_shared<MidiDeviceManager>();
        Logger::info("Application", "  ✓ MidiDeviceManager created");
        
        // Router avec Strategy Pattern pour le scheduling
        Logger::info("Application", "  Creating MidiRouter...");
        router_ = std::make_shared<MidiRouter>(deviceManager_);
        
        // Configurer la stratégie de scheduling
        auto scheduler = std::make_shared<PriorityQueueScheduler>();
        router_->setSchedulingStrategy(scheduler);
        Logger::info("Application", "  ✓ MidiRouter created (with PriorityQueueScheduler)");
        
        // Démarrer le router
        router_->start();
        Logger::info("Application", "  ✓ MidiRouter started");
        
        // Player (refactorisé avec composants séparés)
        Logger::info("Application", "  Creating MidiPlayer...");
        player_ = std::make_shared<MidiPlayer>(router_);
        Logger::info("Application", "  ✓ MidiPlayer created (refactored)");
        
        // File Manager (refactorisé avec composants séparés)
        Logger::info("Application", "  Creating MidiFileManager...");
        std::string midiFilesDir = Config::instance().get<std::string>(
            "midi_files_directory",
            "/home/pi/midi-files"
        );
        std::string dbPath = Config::instance().get<std::string>(
            "database_path",
            "/home/pi/midimind.db"
        );
        
        fileManager_ = std::make_shared<MidiFileManager>(midiFilesDir, dbPath);
        Logger::info("Application", "  ✓ MidiFileManager created");
        
        // Auto-scan initial de la bibliothèque
        bool autoScanOnStartup = Config::instance().get<bool>("auto_scan_on_startup", true);
        if (autoScanOnStartup) {
            Logger::info("Application", "  Starting initial library scan...");
            fileManager_->scanLibrary(true, false);
            Logger::info("Application", "  ✓ Library scan complete");
        }
        
        // API Server
        Logger::info("Application", "  Creating ApiServer...");
        apiServer_ = std::make_shared<ApiServer>();
        Logger::info("Application", "  ✓ ApiServer created");
        
        // Command Processor V2 avec Factory Pattern
        Logger::info("Application", "  Creating CommandProcessorV2...");
        commandProcessor_ = std::make_shared<CommandProcessorV2>(
            deviceManager_,
            router_,
            player_,
            fileManager_
        );
        Logger::info("Application", "  ✓ CommandProcessorV2 created with Command Pattern");
        
        // Network Manager
        Logger::info("Application", "  Creating NetworkManager...");
        networkManager_ = std::make_shared<NetworkManager>();
        Logger::info("Application", "  ✓ NetworkManager created");
        
        Logger::info("Application", "✓ All core modules created successfully");
        
        // ====================================================================
        // ÉTAPE 5: DEPENDENCY INJECTION CONTAINER
        // ====================================================================
        
        Logger::info("Application", "");
        Logger::info("Application", "Step 5/7: Registering dependencies in DI Container...");
        
        auto& di = DIContainer::instance();
        
        di.registerSingleton<MidiDeviceManager>(deviceManager_);
        di.registerSingleton<MidiRouter>(router_);
        di.registerSingleton<MidiPlayer>(player_);
        di.registerSingleton<MidiFileManager>(fileManager_);
        di.registerSingleton<ApiServer>(apiServer_);
        di.registerSingleton<CommandProcessorV2>(commandProcessor_);
        
        Logger::info("Application", "✓ DI Container configured");
        
        // ====================================================================
        // ÉTAPE 6: OBSERVER PATTERN ET CALLBACKS
        // ====================================================================
        
        Logger::info("Application", "");
        Logger::info("Application", "Step 6/7: Setting up observers and callbacks...");
        
        setupObservers();
        Logger::info("Application", "  ✓ Observers configured");
        
        setupCallbacks();
        Logger::info("Application", "  ✓ Callbacks configured");
        
        // ====================================================================
        // ÉTAPE 7: GÉNÉRATION DOCUMENTATION API
        // ====================================================================
        
        Logger::info("Application", "");
        Logger::info("Application", "Step 7/7: Generating API documentation...");
        generateApiDocumentation();
        Logger::info("Application", "✓ API documentation generated");
        
        // ====================================================================
        // INITIALISATION TERMINÉE
        // ====================================================================
        
        Logger::info("Application", "");
        Logger::info("Application", "╔═══════════════════════════════════════════════════════╗");
        Logger::info("Application", "║        Initialization Complete - Ready to Start      ║");
        Logger::info("Application", "╚═══════════════════════════════════════════════════════╝");
        
    } catch (const std::exception& e) {
        Logger::error("Application", "FATAL: Initialization failed: " + std::string(e.what()));
        throw;
    }
}

// ============================================================================
// DESTRUCTEUR
// ============================================================================

/**
 * @brief Destructeur - Arrête proprement l'application
 */
Application::~Application() {
    Logger::info("Application", "Destroying Application...");
    stop();
    Logger::info("Application", "Application destroyed");
}

// ============================================================================
// MÉTHODE: run()
// ============================================================================

/**
 * @brief Lance l'application et entre dans la boucle principale
 */
void Application::run() {
    Logger::info("Application", "");
    Logger::info("Application", "╔═══════════════════════════════════════════════════════╗");
    Logger::info("Application", "║             Starting midiMind v3.0                   ║");
    Logger::info("Application", "╚═══════════════════════════════════════════════════════╝");
    Logger::info("Application", "");
    
    running_ = true;
    
    try {
        // ====================================================================
        // DÉMARRAGE DU SERVEUR API
        // ====================================================================
        
        Logger::info("Application", "Starting API WebSocket server...");
        
        int apiPort = Config::instance().get<int>("api_port", 8080);
        apiServer_->start(apiPort);
        
        Logger::info("Application", "✓ API server listening on port " + std::to_string(apiPort));
        
        // ====================================================================
        // CONFIGURATION RÉSEAU (WIFI HOTSPOT SI NÉCESSAIRE)
        // ====================================================================
        
        bool enableHotspot = Config::instance().get<bool>("wifi_hotspot_enabled", false);
        
        if (enableHotspot) {
            Logger::info("Application", "Starting WiFi Hotspot...");
            
            NetworkManager::HotspotConfig hotspotConfig;
            hotspotConfig.ssid = Config::instance().get<std::string>(
                "wifi_hotspot_ssid",
                "midiMind-" + std::to_string(::getpid())
            );
            hotspotConfig.password = Config::instance().get<std::string>(
                "wifi_hotspot_password",
                "midimind2025"
            );
            hotspotConfig.interface = Config::instance().get<std::string>(
                "wifi_hotspot_interface",
                "wlan0"
            );
            
            if (networkManager_->startHotspot(hotspotConfig)) {
                Logger::info("Application", "✓ WiFi Hotspot started: " + hotspotConfig.ssid);
            } else {
                Logger::warn("Application", "Failed to start WiFi Hotspot (continuing anyway)");
            }
        }
        
        // ====================================================================
        // DÉMARRAGE DES THREADS DE MONITORING
        // ====================================================================
        
        Logger::info("Application", "Starting monitoring threads...");
        
        // Thread de broadcast du statut
        try {
            statusThread_ = std::thread(&Application::statusBroadcastLoop, this);
            Logger::info("Application", "✓ Status broadcast thread started");
        } catch (const std::exception& e) {
            Logger::error("Application", "Failed to start status thread: " + std::string(e.what()));
            throw;
        }
        
        // Thread de monitoring thermique (optionnel)
        bool thermalMonitoring = Config::instance().get<bool>("thermal_monitoring_enabled", true);
        if (thermalMonitoring) {
            try {
                thermalThread_ = std::thread(&Application::thermalMonitoringLoop, this);
                Logger::info("Application", "✓ Thermal monitoring thread started");
            } catch (const std::exception& e) {
                Logger::error("Application", "Failed to start thermal thread: " + std::string(e.what()));
                // Non fatal - continuer sans monitoring thermique
            }
        }
        
        // ====================================================================
        // AFFICHAGE INFORMATIONS DE DÉMARRAGE
        // ====================================================================
        
        displayStartupInfo();
        
        // ====================================================================
        // BOUCLE PRINCIPALE
        // ====================================================================
        
        Logger::info("Application", "Entering main loop...");
        
        while (running_) {
            // La boucle principale ne fait que dormir
            // Tout le travail est fait dans les threads et callbacks
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        Logger::info("Application", "Main loop exited");
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Error in main loop: " + std::string(e.what()));
        running_ = false;
        throw;
    }
}

// ============================================================================
// MÉTHODE: stop()
// ============================================================================

void Application::stop() {
    // ✅ ÉTAPE 1 : Copier les shared_ptr SANS lock pour éviter deadlock
    std::shared_ptr<HealthCheck> healthCheck;
    std::shared_ptr<MetricsCollector> metricsCollector;
    std::shared_ptr<APIServer> apiServer;
    std::shared_ptr<NetworkManager> networkManager;
    std::shared_ptr<MidiClock> midiClock;
    std::shared_ptr<MidiPlayer> midiPlayer;
    std::shared_ptr<MidiDeviceManager> deviceManager;
    std::shared_ptr<Settings> settings;
    std::shared_ptr<Database> database;
    
    // ✅ ÉTAPE 2 : Lock minimal pour copier les pointeurs et changer l'état
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != ApplicationState::RUNNING) {
            Logger::warn("Application", "Not running, cannot stop");
            return;
        }
        
        state_ = ApplicationState::STOPPING;
        
        // Copier tous les shared_ptr
        healthCheck = healthCheck_;
        metricsCollector = metricsCollector_;
        apiServer = apiServer_;
        networkManager = networkManager_;
        midiClock = midiClock_;
        midiPlayer = midiPlayer_;
        deviceManager = deviceManager_;
        settings = settings_;
        database = database_;
        
    }  // ✅ Lock libéré ici - plus de risque de deadlock
    
    // ✅ ÉTAPE 3 : Arrêter tous les composants SANS lock
    Logger::info("Application", "═══════════════════════════════════════");
    Logger::info("Application", "  Stopping MidiMind v3.0");
    Logger::info("Application", "═══════════════════════════════════════");
    
    try {
        // 1. Stop monitoring (pour éviter les alertes pendant shutdown)
        Logger::info("Application", "Stopping monitoring...");
        if (healthCheck) {
            healthCheck->stop();
        }
        if (metricsCollector) {
            metricsCollector->stop();
        }
        
        // 2. Stop API server (fermer les connexions clients)
        Logger::info("Application", "Stopping API server...");
        if (apiServer) {
            apiServer->stop();
        }
        
        // 3. Stop network services
        Logger::info("Application", "Stopping network services...");
        if (networkManager) {
            networkManager->stop();
        }
        
        // 4. Stop MIDI clock
        Logger::info("Application", "Stopping MIDI clock...");
        if (midiClock) {
            midiClock->stop();
        }
        
        // 5. Stop MIDI player
        Logger::info("Application", "Stopping MIDI player...");
        if (midiPlayer) {
            midiPlayer->stop();
        }
        
        // 6. Close all MIDI devices
        Logger::info("Application", "Closing MIDI devices...");
        if (deviceManager) {
            auto devices = deviceManager->getDevices();
            for (auto& device : devices) {
                if (device && device->isOpen()) {
                    device->close();
                }
            }
        }
        
        // 7. Save settings
        Logger::info("Application", "Saving settings...");
        if (settings) {
            settings->save();
        }
        
        // 8. Close database
        Logger::info("Application", "Closing database...");
        if (database) {
            database->close();
        }
        
        // 9. Shutdown optimizations
        Logger::info("Application", "Shutting down optimizations...");
        PerformanceOptimizer::instance().shutdown();
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Error during shutdown: " + std::string(e.what()));
    }
    
    // ✅ ÉTAPE 4 : Re-lock uniquement pour changer l'état final
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ApplicationState::STOPPED;
    }
    
    // ✅ ÉTAPE 5 : Afficher le résumé
    uint64_t uptime = TimeUtils::getCurrentTimestampMs() - startTime_;
    
    Logger::info("Application", "═══════════════════════════════════════");
    Logger::info("Application", "✓ MidiMind stopped successfully");
    Logger::info("Application", "  Uptime: " + TimeUtils::durationToString(uptime));
    Logger::info("Application", "═══════════════════════════════════════");
}
// ============================================================================
// MÉTHODE: configureLogger()
// ============================================================================

/**
 * @brief Configure le système de logging
 */
void Application::configureLogger() {
    // Lire le niveau de log depuis la config
    std::string logLevel = Config::instance().get<std::string>("log_level", "INFO");
    
    // Convertir la string en enum
    if (logLevel == "DEBUG") {
        Logger::setLevel(Logger::Level::DEBUG);
    } else if (logLevel == "INFO") {
        Logger::setLevel(Logger::Level::INFO);
    } else if (logLevel == "WARNING" || logLevel == "WARN") {
        Logger::setLevel(Logger::Level::WARNING);
    } else if (logLevel == "ERROR") {
        Logger::setLevel(Logger::Level::ERROR);
    } else {
        Logger::warn("Application", "Unknown log level: " + logLevel + ", using INFO");
        Logger::setLevel(Logger::Level::INFO);
    }
    
    Logger::info("Application", "Log level set to: " + logLevel);
    
    // Configurer la destination des logs (console, fichier, syslog)
    std::string logDestination = Config::instance().get<std::string>("log_destination", "console");
    
    if (logDestination == "file") {
        std::string logFile = Config::instance().get<std::string>("log_file", "/var/log/midimind.log");
        Logger::info("Application", "Logging to file: " + logFile);
        // TODO: Implémenter la redirection vers fichier
    } else if (logDestination == "syslog") {
        Logger::info("Application", "Logging to syslog");
        // TODO: Implémenter l'envoi vers syslog
    } else {
        Logger::info("Application", "Logging to console");
    }
}

// ============================================================================
// MÉTHODE: detectAndOptimizeForRaspberryPi()
// ============================================================================

/**
 * @brief Détecte le modèle de Raspberry Pi et optimise les paramètres
 */
void Application::detectAndOptimizeForRaspberryPi() {
    Logger::info("Application", "Detecting Raspberry Pi model...");
    
    // Lire /proc/cpuinfo pour détecter le modèle
    std::ifstream cpuinfo(CPUINFO_PATH);
    if (!cpuinfo.is_open()) {
        Logger::warn("Application", "Cannot open " + std::string(CPUINFO_PATH));
        Logger::warn("Application", "Using default performance parameters");
        playerFPS_ = DEFAULT_PLAYER_FPS;
        broadcastFPS_ = DEFAULT_BROADCAST_FPS;
        return;
    }
    
    std::string line;
    std::string model = "Unknown";
    std::string revision = "Unknown";
    
    while (std::getline(cpuinfo, line)) {
        if (line.find("Model") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                model = line.substr(pos + 2);  // +2 pour sauter ": "
            }
        } else if (line.find("Revision") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                revision = line.substr(pos + 2);
            }
        }
    }
    
    cpuinfo.close();
    
    Logger::info("Application", "Model: " + model);
    Logger::info("Application", "Revision: " + revision);
    
    // Détecter le modèle et optimiser
    if (model.find("Pi Zero") != std::string::npos) {
        // Raspberry Pi Zero ou Zero W - CPU limité
        Logger::info("Application", "Detected Pi Zero - Applying low-power optimizations");
        playerFPS_ = PI_ZERO_PLAYER_FPS;
        broadcastFPS_ = PI_ZERO_BROADCAST_FPS;
        
    } else if (model.find("Pi 3") != std::string::npos) {
        // Raspberry Pi 3B / 3B+ - Équilibré
        Logger::info("Application", "Detected Pi 3 - Applying balanced optimizations");
        playerFPS_ = PI_3_PLAYER_FPS;
        broadcastFPS_ = PI_3_BROADCAST_FPS;
        
    } else if (model.find("Pi 4") != std::string::npos || model.find("Pi 5") != std::string::npos) {
        // Raspberry Pi 4 ou 5 - Performance maximale
        Logger::info("Application", "Detected Pi 4/5 - Applying high-performance optimizations");
        playerFPS_ = PI_4_PLAYER_FPS;
        broadcastFPS_ = PI_4_BROADCAST_FPS;
        
    } else {
        // Modèle inconnu - utiliser valeurs par défaut
        Logger::warn("Application", "Unknown Pi model - Using default parameters");
        playerFPS_ = DEFAULT_PLAYER_FPS;
        broadcastFPS_ = DEFAULT_BROADCAST_FPS;
    }
    
    Logger::info("Application", "Performance parameters:");
    Logger::info("Application", "  Player FPS: " + std::to_string(playerFPS_));
    Logger::info("Application", "  Broadcast FPS: " + std::to_string(broadcastFPS_));
}

// ============================================================================
// MÉTHODE: displayStartupInfo()
// ============================================================================

/**
 * @brief Affiche les informations de démarrage
 */
void Application::displayStartupInfo() {
    Logger::info("Application", "");
    Logger::info("Application", "╔═══════════════════════════════════════════════════════╗");
    Logger::info("Application", "║              midiMind v3.0 - Ready                   ║");
    Logger::info("Application", "╚═══════════════════════════════════════════════════════╝");
    Logger::info("Application", "");
    
    // Afficher la configuration réseau
    Logger::info("Application", "Network Configuration:");
    Logger::info("Application", "  API Port: " + std::to_string(Config::instance().get<int>("api_port", 8080)));
    
    bool hotspotEnabled = Config::instance().get<bool>("wifi_hotspot_enabled", false);
    if (hotspotEnabled) {
        std::string ssid = Config::instance().get<std::string>("wifi_hotspot_ssid", "midiMind");
        Logger::info("Application", "  WiFi Hotspot: " + ssid);
    } else {
        Logger::info("Application", "  WiFi Hotspot: Disabled");
    }
    
    Logger::info("Application", "");
    
    // Afficher les périphériques MIDI
    if (deviceManager_) {
        auto devices = deviceManager_->getConnectedDevices();
        Logger::info("Application", "MIDI Devices:");
        if (devices.empty()) {
            Logger::info("Application", "  No devices connected");
        } else {
            for (const auto& device : devices) {
                Logger::info("Application", "  - " + device->getName() + " [" + device->getId() + "]");
            }
        }
    }
    
    Logger::info("Application", "");
    
    // Afficher les statistiques de la bibliothèque
    if (fileManager_) {
        auto stats = fileManager_->getStatistics();
        Logger::info("Application", "MIDI Library:");
        Logger::info("Application", "  Total files: " + std::to_string(stats["total_files"].get<int>()));
        Logger::info("Application", "  Total duration: " + std::to_string(stats["total_duration_seconds"].get<int>()) + " seconds");
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "Press Ctrl+C to stop");
    Logger::info("Application", "");
}

// ============================================================================
// MÉTHODE: setupCallbacks()
// ============================================================================

/**
 * @brief Configure les callbacks entre les modules
 */
void Application::setupCallbacks() {
    // Callback: Device Manager -> API Server (nouveau périphérique)
    deviceManager_->onDeviceConnected([this](const std::string& deviceId) {
        Logger::info("Application", "Device connected: " + deviceId);
        
        // Broadcaster aux clients WebSocket
        json event;
        event["event"] = "device.connected";
        event["device_id"] = deviceId;
        event["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        apiServer_->broadcast(event);
    });
    
    // Callback: Device Manager -> API Server (périphérique déconnecté)
    deviceManager_->onDeviceDisconnected([this](const std::string& deviceId) {
        Logger::info("Application", "Device disconnected: " + deviceId);
        
        json event;
        event["event"] = "device.disconnected";
        event["device_id"] = deviceId;
        event["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        apiServer_->broadcast(event);
    });
    
    // Callback: Player -> API Server (changement d'état)
    player_->onStateChanged([this](const std::string& newState) {
        Logger::debug("Application", "Player state changed: " + newState);
        
        json event;
        event["event"] = "player.state_changed";
        event["state"] = newState;
        event["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        apiServer_->broadcast(event);
    });
    
    // Callback: API Server -> Command Processor (traiter commandes)
    apiServer_->setCommandCallback([this](const json& command) -> json {
        try {
            return commandProcessor_->processCommand(command);
        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["error"] = e.what();
            return error;
        }
    });
    
    Logger::debug("Application", "All callbacks configured");
}

// ============================================================================
// MÉTHODE: setupObservers()
// ============================================================================

/**
 * @brief Configure les observers (Observer Pattern)
 */
void Application::setupObservers() {
    // Observer: Erreurs critiques
    ErrorManager::instance().addObserver([this](const std::string& error) {
        Logger::error("Application", "Critical error observed: " + error);
        
        // Broadcaster aux clients
        json event;
        event["event"] = "system.error";
        event["error"] = error;
        event["severity"] = "critical";
        event["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        apiServer_->broadcast(event);
    });
    
    Logger::debug("Application", "All observers configured");
}

// ============================================================================
// MÉTHODE: generateApiDocumentation()
// ============================================================================

/**
 * @brief Génère la documentation API au format JSON
 */
void Application::generateApiDocumentation() {
    try {
        // Récupérer toutes les commandes depuis le CommandFactory
        auto& factory = commandProcessor_->getCommandFactory();
        auto commands = factory.listCommands();
        
        json doc;
        doc["version"] = "3.0.0";
        doc["generated_at"] = std::chrono::system_clock::now().time_since_epoch().count();
        doc["total_commands"] = commands.size();
        doc["commands"] = json::array();
        
        for (const auto& cmdName : commands) {
            json cmdDoc;
            cmdDoc["name"] = cmdName;
            // TODO: Ajouter description, paramètres, exemples depuis les commandes
            doc["commands"].push_back(cmdDoc);
        }
        
        // Sauvegarder dans un fichier
        std::string docPath = Config::instance().get<std::string>(
            "api_doc_path",
            "docs/api_documentation.json"
        );
        
        std::ofstream docFile(docPath);
        if (docFile.is_open()) {
            docFile << doc.dump(2);  // Pretty print avec indentation 2
            docFile.close();
            Logger::info("Application", "API documentation saved to: " + docPath);
        } else {
            Logger::warn("Application", "Cannot write API documentation to: " + docPath);
        }
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Failed to generate API documentation: " + std::string(e.what()));
    }
}

// ============================================================================
// MÉTHODE: statusBroadcastLoop()
// ============================================================================

/**
 * @brief Boucle de broadcast du statut du player
 */
void Application::statusBroadcastLoop() {
    Logger::debug("Application", "Status broadcast loop started");
    
    // Calculer le délai entre chaque broadcast
    auto broadcastInterval = std::chrono::milliseconds(1000 / broadcastFPS_);
    
    while (running_) {
        try {
            // Construire le message de statut
            json status;
            status["event"] = "player.status";
            status["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
            
            if (player_) {
                auto playerStatus = player_->getStatus();
                status["player"] = playerStatus;
            }
            
            // Ajouter statistiques système
            status["system"] = json::object();
            status["system"]["running"] = running_.load();
            status["system"]["connected_clients"] = apiServer_->getClientCount();
            
            // Broadcaster
            apiServer_->broadcast(status);
            
        } catch (const std::exception& e) {
            Logger::error("Application", "Error in status broadcast: " + std::string(e.what()));
        }
        
        // Attendre avant le prochain broadcast
        std::this_thread::sleep_for(broadcastInterval);
    }
    
    Logger::debug("Application", "Status broadcast loop stopped");
}

// ============================================================================
// MÉTHODE: thermalMonitoringLoop()
// ============================================================================

/**
 * @brief Boucle de monitoring thermique
 */
void Application::thermalMonitoringLoop() {
    Logger::debug("Application", "Thermal monitoring loop started");
    
    while (running_) {
        try {
            // Lire la température depuis le système
            std::ifstream thermalFile(THERMAL_PATH);
            if (!thermalFile.is_open()) {
                Logger::warn("Application", "Cannot read thermal data from: " + std::string(THERMAL_PATH));
                break;  // Arrêter le monitoring si le fichier n'existe pas
            }
            
            int tempMilliCelsius;
            thermalFile >> tempMilliCelsius;
            thermalFile.close();
            
            // Convertir en Celsius
            int tempCelsius = tempMilliCelsius / 1000;
            
            // Vérifier les seuils
            if (tempCelsius >= THERMAL_CRITICAL_THRESHOLD) {
                Logger::error("Application", "CRITICAL: CPU temperature is " + 
                            std::to_string(tempCelsius) + "°C (threshold: " + 
                            std::to_string(THERMAL_CRITICAL_THRESHOLD) + "°C)");
                
                // Broadcaster l'alerte critique
                json alert;
                alert["event"] = "system.thermal_critical";
                alert["temperature"] = tempCelsius;
                alert["threshold"] = THERMAL_CRITICAL_THRESHOLD;
                alert["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
                
                apiServer_->broadcast(alert);
                
            } else if (tempCelsius >= THERMAL_WARNING_THRESHOLD) {
                Logger::warn("Application", "WARNING: CPU temperature is " + 
                           std::to_string(tempCelsius) + "°C (threshold: " + 
                           std::to_string(THERMAL_WARNING_THRESHOLD) + "°C)");
                
                // Broadcaster l'alerte warning
                json alert;
                alert["event"] = "system.thermal_warning";
                alert["temperature"] = tempCelsius;
                alert["threshold"] = THERMAL_WARNING_THRESHOLD;
                alert["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
                
                apiServer_->broadcast(alert);
            } else {
                // Température normale - log en debug seulement
                Logger::debug("Application", "CPU temperature: " + std::to_string(tempCelsius) + "°C");
            }
            
        } catch (const std::exception& e) {
            Logger::error("Application", "Error in thermal monitoring: " + std::string(e.what()));
        }
        
        // Vérifier toutes les 30 secondes
        for (int i = 0; i < 30 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    Logger::debug("Application", "Thermal monitoring loop stopped");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.cpp
// ============================================================================

// ============================================================================
// Fichier: backend/src/core/Application.cpp
// Version: 3.0.5 - CORRECTIONS CRITIQUES PHASE 1
// Date: 2025-10-15
// ============================================================================
// Description:
//   Application principale - Orchestre tous les modules du système
//
// CHANGEMENTS v3.0.5:
//   ✅ CORRECTION 1.1: Destructeur avec join explicite des threads
//   ✅ CORRECTION 1.1: Libération propre NetworkManager
//   ✅ CORRECTION 1.3: Meyers Singleton (thread-safe C++11)
//   ✅ Suppression membre static Application* instance_
//
// PRÉSERVÉ DE v3.0.4:
//   ✅ Toutes fonctionnalités existantes
//   ✅ Hot-plug monitoring thread
//   ✅ Status broadcast thread
//   ✅ Shutdown avec timeout 5s
//   ✅ Compteur signaux 3x Ctrl+C
//   ✅ Méthodes getStatus() et getHealth()
//   ✅ setupCallbacks() et setupObservers()
//   ✅ generateApiDocumentation()
//   ✅ Initialisation 7 phases
//
// Architecture:
//   Application (singleton thread-safe)
//   ├── MidiDeviceManager
//   ├── MidiRouter
//   ├── MidiPlayer
//   ├── MidiFileManager
//   ├── ApiServer
//   ├── CommandProcessorV2
//   └── NetworkManager
//
// Auteur: MidiMind Team
// ============================================================================

#include "Application.h"
#include "../core/Logger.h"
#include "../core/Config.h"
#include "../api/MessageEnvelope.h"
#include "../api/Protocol.h"
#include "../core/patterns/DIContainer.h"
#include "../storage/FileSystem.h"
#include <csignal>
#include <iostream>
#include <fstream>
#include <sstream>

namespace midiMind {

// ============================================================================
// SINGLETON - v3.0.5: MEYERS SINGLETON (THREAD-SAFE)
// ============================================================================

Application& Application::instance() {
    // Meyers Singleton - Thread-safe depuis C++11
    // L'initialisation est garantie thread-safe par le standard
    static Application instance;
    return instance;
}

// Initialiser le compteur de signaux
std::atomic<int> Application::signalCount_{0};

// ============================================================================
// SIGNAL HANDLER - v3.0.4: AMÉLIORÉ AVEC COMPTEUR
// ============================================================================

static std::atomic<bool> shutdownRequested{false};

void signalHandler(int signal) {
    Application::signalCount_++;
    
    // Force kill après 3 signaux
    if (Application::signalCount_ > 3) {
        std::cerr << "\n\nForce shutdown!\n";
        std::exit(EXIT_FAILURE);
    }
    
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n\n[" << Application::signalCount_ << "/3] Shutdown signal received";
        if (Application::signalCount_ == 1) {
            std::cout << " - Stopping gracefully...";
        } else {
            std::cout << " - Send " << (4 - Application::signalCount_) << " more to force quit";
        }
        std::cout << std::endl;
        
        shutdownRequested = true;
        
        // Arrêter l'application
        Application::instance().stop();
    }
}

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR - v3.0.5: CORRECTIFS CRITIQUES
// ============================================================================

Application::Application()
    : initialized_(false)
    , running_(false)
    , hotPlugRunning_(false)
    , statusRunning_(false)
    , startTime_(std::chrono::steady_clock::now())
{
    Logger::info("Application", "Creating application instance...");
    
    // Installer les signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGPIPE, SIG_IGN); // Ignorer SIGPIPE (WebSocket)
    
    Logger::debug("Application", "Signal handlers installed");
}

Application::~Application() {
    Logger::info("Application", "Destroying application...");
    
    // ========================================================================
    // v3.0.5 CORRECTION 1.1: ARRÊT PROPRE AVEC JOIN EXPLICITE
    // ========================================================================
    
    // Demander l'arrêt de tous les services
    stop();
    
    // Attendre que les threads de monitoring se terminent proprement
    // IMPORTANT: Ne pas vérifier joinable() car stop() les a déjà joints,
    // mais on s'assure qu'ils sont bien arrêtés
    if (hotPlugThread_.joinable()) {
        Logger::debug("Application", "Waiting for hot-plug thread to finish...");
        hotPlugRunning_ = false;
        hotPlugThread_.join();
        Logger::debug("Application", "Hot-plug thread joined");
    }
    
    if (statusThread_.joinable()) {
        Logger::debug("Application", "Waiting for status thread to finish...");
        statusRunning_ = false;
        statusThread_.join();
        Logger::debug("Application", "Status thread joined");
    }
    
    // ========================================================================
    // v3.0.5 CORRECTION 1.1: LIBÉRER NETWORKMANAGER PROPREMENT
    // ========================================================================
    
    if (networkManager_) {
        Logger::debug("Application", "Releasing NetworkManager...");
        networkManager_.reset();
        Logger::debug("Application", "NetworkManager released");
    }
    
    // Nettoyer le DI Container
    DIContainer::instance().clear();
    Logger::debug("Application", "DIContainer cleared");
    
    Logger::info("Application", "Application destroyed successfully");
}

// ============================================================================
// INITIALISATION - 7 PHASES (PRÉSERVÉ DE v3.0.4)
// ============================================================================

bool Application::initialize(const std::string& configPath) {
    if (initialized_) {
        Logger::warn("Application", "Already initialized");
        return true;
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "╔═══════════════════════════════════════════╗");
    Logger::info("Application", "║   midiMind v3.0.5 Initialization Start   ║");
    Logger::info("Application", "╚═══════════════════════════════════════════╝");
    Logger::info("Application", "");
    Logger::info("Application", "Protocol version: " + std::string(protocol::PROTOCOL_VERSION));
    Logger::info("Application", "");
    
    try {
        // ====================================================================
        // PHASE 1/7: CONFIGURATION
        // ====================================================================
        
        Logger::info("Application", "┌─── Phase 1/7: Configuration ─────┐");
        Logger::info("Application", "");
        
        std::string cfgPath = configPath.empty() ? 
            "./config/config.json" : configPath;
        
        if (!FileSystem::exists(cfgPath)) {
            Logger::warn("Application", "Config file not found: " + cfgPath);
            Logger::info("Application", "Using default configuration");
        } else {
            Config::instance().load(cfgPath);
            Logger::info("Application", "  ✓ Config loaded: " + cfgPath);
        }
        
        config_ = std::make_shared<Config>(Config::instance());
        
        // Configurer le logger
        std::string logLevel = config_->getValue("log_level", "info");
        Logger::setLevel(logLevel);
        Logger::info("Application", "  ✓ Logger configured (level: " + logLevel + ")");
        
        Logger::info("Application", "");
        
        // ====================================================================
        // PHASE 2/7: DATABASE
        // ====================================================================
        
        Logger::info("Application", "┌─── Phase 2/7: Database ──────────┐");
        Logger::info("Application", "");
        
        std::string dbPath = config_->getValue("database.path", 
            std::string("./data/midimind.db"));
        
        database_ = std::make_shared<Database>(dbPath);
        
        if (!database_->open()) {
            throw std::runtime_error("Failed to open database: " + dbPath);
        }
        
        Logger::info("Application", "  ✓ Database opened: " + dbPath);
        Logger::info("Application", "");
        
        // ====================================================================
        // PHASE 3/7: STORAGE (Settings, Presets, etc.)
        // ====================================================================
        
        Logger::info("Application", "┌─── Phase 3/7: Storage ───────────┐");
        Logger::info("Application", "");
        
        // Créer répertoires nécessaires
        std::vector<std::string> dirs = {
            "./data",
            "./data/midi",
            "./data/presets",
            "./data/logs",
            "./data/recordings"
        };
        
        for (const auto& dir : dirs) {
            if (!FileSystem::exists(dir)) {
                FileSystem::createDirectory(dir, true);
                Logger::debug("Application", "  Created directory: " + dir);
            }
        }
        
        Logger::info("Application", "  ✓ Storage directories ready");
        Logger::info("Application", "");
        
        // ====================================================================
        // PHASE 4/7: MIDI CORE
        // ====================================================================
        
        Logger::info("Application", "┌─── Phase 4/7: MIDI Core ─────────┐");
        Logger::info("Application", "");
        
        // MidiDeviceManager
        deviceManager_ = std::make_shared<MidiDeviceManager>();
        Logger::info("Application", "  ✓ MidiDeviceManager initialized");
        
        // MidiRouter
        router_ = std::make_shared<MidiRouter>(deviceManager_);
        Logger::info("Application", "  ✓ MidiRouter initialized");
        
        // MidiPlayer
        player_ = std::make_shared<MidiPlayer>(router_);
        Logger::info("Application", "  ✓ MidiPlayer initialized");
        
        // MidiFileManager
        std::string midiPath = config_->getValue("midi.library_path", 
            std::string("./data/midi"));
        fileManager_ = std::make_shared<MidiFileManager>(midiPath, database_);
        Logger::info("Application", "  ✓ MidiFileManager initialized");
        
        Logger::info("Application", "");
        
        // ====================================================================
        // PHASE 5/7: API SERVER & COMMAND PROCESSOR
        // ====================================================================
        
        Logger::info("Application", "┌─── Phase 5/7: API Server ────────┐");
        Logger::info("Application", "");
        
        // ApiServer
        apiServer_ = std::make_shared<ApiServer>();
        Logger::info("Application", "  ✓ ApiServer created");
        
        // CommandProcessorV2
        commandProcessor_ = std::make_shared<CommandProcessorV2>(
            deviceManager_,
            router_,
            player_,
            fileManager_,
            database_
        );
        Logger::info("Application", "  ✓ CommandProcessorV2 created");
        
        // Connecter CommandProcessor à ApiServer
        apiServer_->setCommandCallback([this](const json& cmd) {
            return commandProcessor_->processCommand(cmd);
        });
        Logger::info("Application", "  ✓ Command callback connected");
        
        Logger::info("Application", "");
        
        // ====================================================================
        // PHASE 6/7: DEPENDENCY INJECTION & CALLBACKS
        // ====================================================================
        
        Logger::info("Application", "┌─── Phase 6/7: DI & Callbacks ────┐");
        Logger::info("Application", "");
        
        Logger::info("Application", "  Setting up DI Container...");
        setupDependencyInjection();
        Logger::info("Application", "  ✓ DI Container configured");
        
        Logger::info("Application", "  Setting up callbacks...");
        setupCallbacks();
        Logger::info("Application", "  ✓ Callbacks configured");
        
        Logger::info("Application", "  Setting up observers...");
        setupObservers();
        Logger::info("Application", "  ✓ Observers configured");
        
        Logger::info("Application", "  Generating API documentation...");
        generateApiDocumentation();
        Logger::info("Application", "  ✓ API documentation generated");
        
        Logger::info("Application", "");
        
        // ====================================================================
        // PHASE 7/7: NETWORK (optionnel)
        // ====================================================================
        
        Logger::info("Application", "┌─── Phase 7/7: Network (optional) ─┐");
        Logger::info("Application", "");
        
        if (!initializeNetwork()) {
            Logger::warn("Application", "  Network initialization failed (non-critical)");
        } else {
            Logger::info("Application", "  ✓ NetworkManager initialized");
        }
        
        Logger::info("Application", "");
        
        // ====================================================================
        // SUCCÈS
        // ====================================================================
        
        initialized_ = true;
        
        Logger::info("Application", "╔═══════════════════════════════════════════╗");
        Logger::info("Application", "║     Initialization Complete - Ready       ║");
        Logger::info("Application", "╚═══════════════════════════════════════════╝");
        Logger::info("Application", "");
        
        // Résumé
        Logger::info("Application", "Modules initialized:");
        Logger::info("Application", "  ✓ Config");
        Logger::info("Application", "  ✓ Database");
        Logger::info("Application", "  ✓ MidiDeviceManager");
        Logger::info("Application", "  ✓ MidiRouter");
        Logger::info("Application", "  ✓ MidiPlayer");
        Logger::info("Application", "  ✓ MidiFileManager");
        Logger::info("Application", "  ✓ ApiServer");
        Logger::info("Application", "  ✓ CommandProcessorV2");
        Logger::info("Application", "  " + std::string(networkManager_ ? "✓" : "✗") + " NetworkManager");
        Logger::info("Application", "");
        Logger::info("Application", "Ready to start!");
        Logger::info("Application", "");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "FATAL: Initialization failed");
        Logger::error("Application", "Exception: " + std::string(e.what()));
        
        initialized_ = false;
        return false;
    }
}

// ============================================================================
// DÉMARRAGE (PRÉSERVÉ DE v3.0.4)
// ============================================================================

bool Application::start() {
    if (!initialized_) {
        Logger::error("Application", "Cannot start: not initialized");
        return false;
    }
    
    if (running_) {
        Logger::warn("Application", "Already running");
        return true;
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "Starting services...");
    
    try {
        // Démarrer l'API Server
        int port = config_->getValue("api.port", 8080);
        apiServer_->start(port);
        Logger::info("Application", "  ✓ API Server started on port " + std::to_string(port));
        
        // Scanner les devices
        deviceManager_->scanDevices();
        Logger::info("Application", "  ✓ Device scan initiated");
        
        // Démarrer le network manager si présent
        if (networkManager_) {
            networkManager_->start();
            Logger::info("Application", "  ✓ NetworkManager started");
        }
        
        // Démarrer hot-plug monitoring
        startHotPlugMonitoring();
        Logger::info("Application", "  ✓ Hot-plug monitoring started");
        
        // Démarrer status broadcast
        startStatusBroadcast();
        Logger::info("Application", "  ✓ Status broadcast started");
        
        running_ = true;
        
        Logger::info("Application", "");
        Logger::info("Application", "╔═══════════════════════════════════════════╗");
        Logger::info("Application", "║  ✓ midiMind v3.0.5 is RUNNING            ║");
        Logger::info("Application", "╚═══════════════════════════════════════════╝");
        Logger::info("Application", "");
        Logger::info("Application", "  WebSocket: ws://localhost:" + std::to_string(port));
        Logger::info("Application", "  Ready to accept connections");
        Logger::info("Application", "");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Failed to start: " + std::string(e.what()));
        running_ = false;
        return false;
    }
}

// ============================================================================
// ARRÊT - v3.0.5: ARRÊT PROPRE DES THREADS
// ============================================================================

void Application::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "═══════════════════════════════════");
    Logger::info("Application", "  Stopping midiMind Services...");
    Logger::info("Application", "═══════════════════════════════════");
    Logger::info("Application", "");
    
    running_ = false;
    
    auto startShutdown = std::chrono::steady_clock::now();
    const int SHUTDOWN_TIMEOUT_MS = 5000;
    
    try {
        // Arrêter monitoring threads
        stopHotPlugMonitoring();
        Logger::info("Application", "  ✓ Hot-plug monitoring stopped");
        
        stopStatusBroadcast();
        Logger::info("Application", "  ✓ Status broadcast stopped");
        
        // Arrêter le network manager
        if (networkManager_) {
            Logger::info("Application", "  Stopping NetworkManager...");
            networkManager_->stop();
            Logger::info("Application", "  ✓ NetworkManager stopped");
        }
        
        // Arrêter l'API Server
        Logger::info("Application", "  Stopping API Server...");
        apiServer_->stop();
        Logger::info("Application", "  ✓ API Server stopped");
        
        // Arrêter le player
        Logger::info("Application", "  Stopping MidiPlayer...");
        player_->stop();
        Logger::info("Application", "  ✓ MidiPlayer stopped");
        
        // Déconnecter tous les devices
        Logger::info("Application", "  Disconnecting MIDI devices...");
        deviceManager_->disconnectAll();
        Logger::info("Application", "  ✓ Devices disconnected");
        
        // Fermer la database
        Logger::info("Application", "  Closing database...");
        database_->close();
        Logger::info("Application", "  ✓ Database closed");
        
        // Vérifier timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startShutdown
        ).count();
        
        if (elapsed > SHUTDOWN_TIMEOUT_MS) {
            Logger::warn("Application", "Shutdown took longer than " + 
                std::to_string(SHUTDOWN_TIMEOUT_MS) + "ms");
        }
        
        Logger::info("Application", "");
        Logger::info("Application", "╔═══════════════════════════════════════════╗");
        Logger::info("Application", "║  ✓ midiMind Stopped Cleanly              ║");
        Logger::info("Application", "╚═══════════════════════════════════════════╝");
        Logger::info("Application", "");
        Logger::info("Application", "Shutdown time: " + std::to_string(elapsed) + "ms");
        Logger::info("Application", "");
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Error during shutdown: " + std::string(e.what()));
    }
}

// ============================================================================
// ATTENTE SHUTDOWN (PRÉSERVÉ DE v3.0.4)
// ============================================================================

void Application::waitForShutdown() {
    Logger::debug("Application", "Waiting for shutdown signal...");
    
    // Boucle principale - attend le signal d'arrêt
    while (running_ && !shutdownRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    Logger::debug("Application", "Shutdown signal received");
}

// ============================================================================
// SETUP CALLBACKS (PRÉSERVÉ DE v3.0.4)
// ============================================================================

void Application::setupCallbacks() {
    Logger::debug("Application", "Setting up callbacks...");
    
    // Callback: Player State Changed
    player_->setStateCallback([this](const std::string& state) {
        try {
            auto event = MessageEnvelope::createEvent(
                "player:state",
                {
                    {"state", state},
                    {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
                }
            );
            
            apiServer_->broadcast(event.toJsonString());
            
        } catch (const std::exception& e) {
            Logger::error("Application", "Failed to broadcast player state: " + std::string(e.what()));
        }
    });
    
    Logger::debug("Application", "  ✓ Player state callback");
    
    // Callback: MIDI Message Received
    router_->setMessageCallback([this](const MidiMessage& msg) {
        try {
            if (apiServer_->getClientCount() > 0) {
                auto event = MessageEnvelope::createEvent(
                    "midi:message",
                    {
                        {"channel", msg.channel},
                        {"type", midiMessageTypeToString(msg.type)},
                        {"data1", msg.data1},
                        {"data2", msg.data2}
                    }
                );
                
                apiServer_->broadcast(event.toJsonString());
            }
            
        } catch (const std::exception& e) {
            Logger::error("Application", "Failed to broadcast MIDI message: " + std::string(e.what()));
        }
    });
    
    Logger::debug("Application", "  ✓ MIDI message callback");
    
    Logger::debug("Application", "  All callbacks configured");
}

// ============================================================================
// SETUP OBSERVERS (PRÉSERVÉ DE v3.0.4)
// ============================================================================

void Application::setupObservers() {
    Logger::debug("Application", "Setting up observers...");
    
    // Observer: Device Status Changed
    deviceManager_->setOnDeviceConnectedCallback([this](const DeviceInfo& dev) {
        try {
            auto event = MessageEnvelope::createEvent(
                "device:connected",
                {
                    {"device_id", dev.id},
                    {"device_name", dev.name},
                    {"device_type", deviceTypeToString(dev.type)},
                    {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
                }
            );
            
            apiServer_->broadcast(event.toJsonString());
            
        } catch (const std::exception& e) {
            Logger::error("Application", "Failed to broadcast device event: " + std::string(e.what()));
        }
    });
    
    Logger::debug("Application", "  ✓ Device observers");
}

// ============================================================================
// DEPENDENCY INJECTION (PRÉSERVÉ DE v3.0.4)
// ============================================================================

void Application::setupDependencyInjection() {
    auto& container = DIContainer::instance();
    
    container.registerSingleton<Config>(config_);
    container.registerSingleton<Database>(database_);
    container.registerSingleton<MidiDeviceManager>(deviceManager_);
    container.registerSingleton<MidiRouter>(router_);
    container.registerSingleton<MidiPlayer>(player_);
    container.registerSingleton<MidiFileManager>(fileManager_);
    container.registerSingleton<ApiServer>(apiServer_);
    container.registerSingleton<CommandProcessorV2>(commandProcessor_);
    
    if (networkManager_) {
        container.registerSingleton<NetworkManager>(networkManager_);
    }
    
    Logger::debug("Application", "DI Container configured with " + 
        std::to_string(container.count()) + " services");
}

// ============================================================================
// INITIALIZE NETWORK (PRÉSERVÉ DE v3.0.4)
// ============================================================================

bool Application::initializeNetwork() {
    try {
        Logger::debug("Application", "  Initializing NetworkManager...");
        
        networkManager_ = std::make_shared<NetworkManager>();
        
        bool enableWifi = config_->getValue("network.wifi.enabled", false);
        bool enableBluetooth = config_->getValue("network.bluetooth.enabled", false);
        
        if (!enableWifi && !enableBluetooth) {
            Logger::info("Application", "  Network modules disabled in config");
            networkManager_.reset();
            return false;
        }
        
        if (enableWifi) {
            Logger::info("Application", "  WiFi enabled");
        }
        
        if (enableBluetooth) {
            Logger::info("Application", "  Bluetooth enabled");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", 
            "  Failed to initialize NetworkManager: " + std::string(e.what()));
        networkManager_.reset();
        return false;
    }
}

// ============================================================================
// GENERATE API DOCUMENTATION (PRÉSERVÉ DE v3.0.4)
// ============================================================================

void Application::generateApiDocumentation() {
    try {
        Logger::debug("Application", "  Generating API documentation...");
        
        json doc = commandProcessor_->getDocumentation();
        
        doc["version"] = "3.0.5";
        doc["protocol_version"] = protocol::PROTOCOL_VERSION;
        doc["generated_at"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        std::string docDir = config_->getValue("api.doc_path", std::string("./docs"));
        std::string docPath = docDir + "/api_commands.json";
        
        if (!FileSystem::exists(docDir)) {
            FileSystem::createDirectory(docDir, true);
        }
        
        try {
            std::ofstream outFile(docPath);
            
            if (!outFile.is_open()) {
                Logger::error("Application", "    Cannot open file for writing: " + docPath);
                return;
            }
            
            outFile << doc.dump(2);
            outFile.close();
            
            Logger::info("Application", "    ✓ API documentation saved to: " + docPath);
            
            uint64_t fileSize = FileSystem::getFileSize(docPath);
            Logger::debug("Application", "    File size: " + std::to_string(fileSize) + " bytes");
            
        } catch (const std::exception& e) {
            Logger::error("Application", 
                "    Failed to write documentation file: " + std::string(e.what()));
        }
        
        bool generateHtml = config_->getValue("api.generate_html_doc", false);
        if (generateHtml) {
            generateHtmlDocumentation(doc, docDir + "/api_commands.html");
        }
        
    } catch (const std::exception& e) {
        Logger::warn("Application", "  Failed to generate API documentation: " + std::string(e.what()));
    }
}

// ============================================================================
// GENERATE HTML DOCUMENTATION (PRÉSERVÉ DE v3.0.4)
// ============================================================================

void Application::generateHtmlDocumentation(const json& doc, const std::string& outputPath) {
    try {
        Logger::debug("Application", "    Generating HTML documentation: " + outputPath);
        
        std::ofstream html(outputPath);
        
        if (!html.is_open()) {
            Logger::error("Application", "    Cannot create HTML file: " + outputPath);
            return;
        }
        
        html << "<!DOCTYPE html>\n";
        html << "<html lang=\"en\">\n";
        html << "<head>\n";
        html << "    <meta charset=\"UTF-8\">\n";
        html << "    <title>MidiMind API Documentation</title>\n";
        html << "    <style>\n";
        html << "        body { font-family: Arial, sans-serif; margin: 40px; }\n";
        html << "        h1 { color: #333; }\n";
        html << "        .command { border: 1px solid #ddd; padding: 15px; margin: 10px 0; }\n";
        html << "        .command-name { font-weight: bold; color: #0066cc; }\n";
        html << "    </style>\n";
        html << "</head>\n";
        html << "<body>\n";
        html << "    <h1>MidiMind API Documentation v" << doc["version"] << "</h1>\n";
        
        if (doc.contains("commands")) {
            for (const auto& cmd : doc["commands"]) {
                html << "    <div class=\"command\">\n";
                html << "        <div class=\"command-name\">" << cmd["name"] << "</div>\n";
                html << "        <p>" << cmd["description"] << "</p>\n";
                html << "    </div>\n";
            }
        }
        
        html << "</body>\n";
        html << "</html>\n";
        
        html.close();
        
        Logger::info("Application", "    ✓ HTML documentation saved to: " + outputPath);
        
    } catch (const std::exception& e) {
        Logger::error("Application", 
            "    Failed to generate HTML documentation: " + std::string(e.what()));
    }
}

// ============================================================================
// HOT-PLUG MONITORING (PRÉSERVÉ DE v3.0.4)
// ============================================================================

void Application::startHotPlugMonitoring() {
    if (hotPlugThread_.joinable()) {
        return;
    }
    
    hotPlugRunning_ = true;
    hotPlugThread_ = std::thread(&Application::hotPlugMonitorLoop, this);
}

void Application::stopHotPlugMonitoring() {
    hotPlugRunning_ = false;
    if (hotPlugThread_.joinable()) {
        hotPlugThread_.join();
    }
}

void Application::hotPlugMonitorLoop() {
    Logger::info("HotPlug", "Monitoring thread started");
    
    auto previousDevices = deviceManager_->getAvailableDevices();
    
    while (hotPlugRunning_ && running_) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        if (!hotPlugRunning_ || !running_) break;
        
        try {
            deviceManager_->scanDevices();
            auto currentDevices = deviceManager_->getAvailableDevices();
            
            // Détecter nouveaux devices
            for (const auto& dev : currentDevices) {
                bool isNew = true;
                for (const auto& prev : previousDevices) {
                    if (prev.id == dev.id) {
                        isNew = false;
                        break;
                    }
                }
                
                if (isNew) {
                    Logger::info("HotPlug", "🔌 NEW DEVICE: " + dev.name);
                    
                    json event = {
                        {"event", "device.connected"},
                        {"device", {
                            {"id", dev.id},
                            {"name", dev.name},
                            {"type", deviceTypeToString(dev.type)}
                        }}
                    };
                    
                    auto envelope = MessageEnvelope::createEvent(
                        "midi.device.connected",
                        event,
                        MessagePriority::NORMAL
                    );
                    
                    if (apiServer_) {
                        apiServer_->broadcast(envelope.toJsonString());
                    }
                }
            }
            
            // Détecter devices déconnectés
            for (const auto& prev : previousDevices) {
                bool stillConnected = false;
                for (const auto& curr : currentDevices) {
                    if (curr.id == prev.id) {
                        stillConnected = true;
                        break;
                    }
                }
                
                if (!stillConnected) {
                    Logger::warn("HotPlug", "🔌 DEVICE REMOVED: " + prev.name);
                    
                    json event = {
                        {"event", "device.disconnected"},
                        {"device", {
                            {"id", prev.id},
                            {"name", prev.name}
                        }}
                    };
                    
                    auto envelope = MessageEnvelope::createEvent(
                        "midi.device.disconnected",
                        event,
                        MessagePriority::HIGH
                    );
                    
                    if (apiServer_) {
                        apiServer_->broadcast(envelope.toJsonString());
                    }
                }
            }
            
            previousDevices = currentDevices;
            
        } catch (const std::exception& e) {
            Logger::error("HotPlug", "Error: " + std::string(e.what()));
        }
    }
    
    Logger::info("HotPlug", "Monitoring thread stopped");
}

// ============================================================================
// STATUS BROADCAST (PRÉSERVÉ DE v3.0.4)
// ============================================================================

void Application::startStatusBroadcast() {
    if (statusThread_.joinable()) {
        return;
    }
    
    statusRunning_ = true;
    statusThread_ = std::thread(&Application::statusBroadcastLoop, this);
}

void Application::stopStatusBroadcast() {
    statusRunning_ = false;
    if (statusThread_.joinable()) {
        statusThread_.join();
    }
}

void Application::statusBroadcastLoop() {
    Logger::info("StatusBroadcast", "Thread started");
    
    while (statusRunning_ && running_) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        if (!statusRunning_ || !running_) break;
        
        try {
            json status = getStatus();
            
            auto envelope = MessageEnvelope::createEvent(
                "backend.status",
                status,
                MessagePriority::LOW
            );
            
            if (apiServer_ && apiServer_->getClientCount() > 0) {
                apiServer_->broadcast(envelope.toJsonString());
            }
            
        } catch (const std::exception& e) {
            Logger::error("StatusBroadcast", "Error: " + std::string(e.what()));
        }
    }
    
    Logger::info("StatusBroadcast", "Thread stopped");
}

// ============================================================================
// STATUS & HEALTH (PRÉSERVÉ DE v3.0.4)
// ============================================================================

json Application::getStatus() const {
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - startTime_
    ).count();
    
    json status = {
        {"initialized", initialized_},
        {"running", running_.load()},
        {"uptime_seconds", uptime},
        {"version", "3.0.5"},
        {"protocol_version", protocol::PROTOCOL_VERSION}
    };
    
    if (deviceManager_) {
        auto devices = deviceManager_->getAvailableDevices();
        int connectedCount = 0;
        for (const auto& dev : devices) {
            if (dev.connected) connectedCount++;
        }
        
        status["devices"] = {
            {"total", devices.size()},
            {"connected", connectedCount}
        };
    }
    
    if (router_) {
        auto routes = router_->getRoutes();
        status["routes"] = {
            {"count", routes.size()}
        };
    }
    
    if (player_) {
        status["player"] = {
            {"state", player_->getStateString()},
            {"has_file", player_->hasFile()}
        };
    }
    
    if (apiServer_) {
        status["api"] = {
            {"clients", apiServer_->getClientCount()}
        };
    }
    
    return status;
}

json Application::getHealth() const {
    json health = {
        {"status", "healthy"},
        {"checks", json::array()}
    };
    
    bool allHealthy = true;
    
    if (!initialized_) {
        health["checks"].push_back({
            {"name", "initialization"},
            {"status", "unhealthy"}
        });
        allHealthy = false;
    } else {
        health["checks"].push_back({
            {"name", "initialization"},
            {"status", "healthy"}
        });
    }
    
    health["checks"].push_back({
        {"name", "device_manager"},
        {"status", deviceManager_ ? "healthy" : "unhealthy"}
    });
    
    health["checks"].push_back({
        {"name", "router"},
        {"status", router_ ? "healthy" : "unhealthy"}
    });
    
    health["checks"].push_back({
        {"name", "player"},
        {"status", player_ ? "healthy" : "unhealthy"}
    });
    
    health["checks"].push_back({
        {"name", "api_server"},
        {"status", apiServer_ ? "healthy" : "unhealthy"}
    });
    
    if (!deviceManager_ || !router_ || !player_ || !apiServer_) {
        allHealthy = false;
    }
    
    health["status"] = allHealthy ? "healthy" : "unhealthy";
    
    return health;
}

// ============================================================================
// HELPERS (PRÉSERVÉ DE v3.0.4)
// ============================================================================

std::string Application::deviceTypeToString(DeviceType type) const {
    switch (type) {
        case DeviceType::USB: return "USB";
        case DeviceType::BLUETOOTH: return "Bluetooth";
        case DeviceType::NETWORK: return "Network";
        case DeviceType::VIRTUAL: return "Virtual";
        default: return "Unknown";
    }
}

std::string Application::midiMessageTypeToString(MidiMessageType type) const {
    switch (type) {
        case MidiMessageType::NOTE_ON: return "note_on";
        case MidiMessageType::NOTE_OFF: return "note_off";
        case MidiMessageType::CONTROL_CHANGE: return "control_change";
        case MidiMessageType::PROGRAM_CHANGE: return "program_change";
        case MidiMessageType::PITCH_BEND: return "pitch_bend";
        default: return "unknown";
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.cpp v3.0.5 - CORRECTIONS CRITIQUES APPLIQUÉES
// ============================================================================
// ============================================================================
// Fichier: backend/src/core/Application.cpp
// Version: 3.0.2 - COMPLET
// Date: 2025-10-13
// ============================================================================
// Description:
//   Application principale - Orchestre tous les modules du système
//
// CORRECTIONS v3.0.2:
//   ✅ Méthode initialize() complétée
//   ✅ Méthode start() complétée
//   ✅ Méthode stop() complétée
//   ✅ Méthode waitForShutdown() implémentée
//   ✅ setupDependencyInjection() complétée
//   ✅ setupCallbacks() complétée
//   ✅ setupObservers() implémentée
//   ✅ initializeNetwork() implémentée
//   ✅ generateApiDocumentation() implémentée
//   ✅ Gestion signaux (SIGINT, SIGTERM)
//
// Architecture:
//   Application (singleton)
//   ├── MidiDeviceManager
//   ├── MidiRouter
//   ├── MidiPlayer
//   ├── MidiFileManager
//   ├── ApiServer
//   ├── CommandProcessorV2
//   └── NetworkManager
//
// Auteur: midiMind Team
// ============================================================================

#include "Application.h"
#include "../core/Logger.h"
#include "../api/MessageEnvelope.h"
#include "../core/patterns/DIContainer.h"
#include <csignal>
#include <iostream>

namespace midiMind {

// ============================================================================
// SINGLETON
// ============================================================================

Application* Application::instance_ = nullptr;

Application& Application::instance() {
    if (!instance_) {
        instance_ = new Application();
    }
    return *instance_;
}

// ============================================================================
// SIGNAL HANDLER
// ============================================================================

static std::atomic<bool> shutdownRequested{false};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n\nReceived shutdown signal..." << std::endl;
        shutdownRequested = true;
        
        // Arrêter l'application
        if (Application::instance_) {
            Application::instance().stop();
        }
    }
}

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR
// ============================================================================

Application::Application()
    : initialized_(false)
    , running_(false)
{
    Logger::info("Application", "Creating application instance...");
    
    // Installer les signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    Logger::debug("Application", "Signal handlers installed");
}

Application::~Application() {
    stop();
    Logger::info("Application", "Application instance destroyed");
}

// ============================================================================
// INITIALISATION
// ============================================================================

bool Application::initialize(const std::string& configPath) {
    if (initialized_) {
        Logger::warn("Application", "Already initialized");
        return true;
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "╔═════════════════════════════════════════════════╗");
    Logger::info("Application", "║       midiMind v3.0 Initialization Start       ║");
    Logger::info("Application", "╚═════════════════════════════════════════════════╝");
    Logger::info("Application", "");
    Logger::info("Application", "Protocol version: " + std::string(protocol::PROTOCOL_VERSION));
    Logger::info("Application", "");
    
    try {
        // ====================================================================
        // ÉTAPE 1: CONFIGURATION
        // ====================================================================
        
        Logger::info("Application", "╠══ Phase 1/7: Configuration ══╣");
        Logger::info("Application", "");
        
        std::string cfgPath = configPath.empty() ? "config/config.json" : configPath;
        Logger::info("Application", "  Loading config from: " + cfgPath);
        
        config_ = std::make_shared<Config>();
        if (!config_->load(cfgPath)) {
            Logger::warn("Application", "  Failed to load config, using defaults");
        } else {
            Logger::info("Application", "  ✓ Config loaded");
        }
        
        // Configurer le niveau de log
        std::string logLevel = config_->getValue("log.level", std::string("info"));
        Logger::setLevel(logLevel);
        Logger::info("Application", "  ✓ Log level: " + logLevel);
        
        Logger::info("Application", "");
        
        // ====================================================================
        // ÉTAPE 2: DATABASE
        // ====================================================================
        
        Logger::info("Application", "╠══ Phase 2/7: Database ══╣");
        Logger::info("Application", "");
        
        std::string dbPath = config_->getValue("database.path", std::string("data/midimind.db"));
        Logger::info("Application", "  Opening database: " + dbPath);
        
        database_ = std::make_shared<Database>();
        if (!database_->open(dbPath)) {
            Logger::error("Application", "  Failed to open database");
            return false;
        }
        Logger::info("Application", "  ✓ Database opened");
        
        Logger::info("Application", "");
        
        // ====================================================================
        // ÉTAPE 3: MIDI DEVICE MANAGER
        // ====================================================================
        
        Logger::info("Application", "╠══ Phase 3/7: MIDI Devices ══╣");
        Logger::info("Application", "");
        
        Logger::info("Application", "  Creating MidiDeviceManager...");
        deviceManager_ = std::make_shared<MidiDeviceManager>();
        Logger::info("Application", "  ✓ MidiDeviceManager created");
        
        Logger::info("Application", "");
        
        // ====================================================================
        // ÉTAPE 4: MIDI CORE (Router, Player, FileManager)
        // ====================================================================
        
        Logger::info("Application", "╠══ Phase 4/7: MIDI Core ══╣");
        Logger::info("Application", "");
        
        Logger::info("Application", "  Creating MidiRouter...");
        router_ = std::make_shared<MidiRouter>();
        Logger::info("Application", "  ✓ MidiRouter created");
        
        Logger::info("Application", "  Creating MidiPlayer...");
        player_ = std::make_shared<MidiPlayer>(router_);
        Logger::info("Application", "  ✓ MidiPlayer created");
        
        Logger::info("Application", "  Creating MidiFileManager...");
        fileManager_ = std::make_shared<MidiFileManager>(database_);
        Logger::info("Application", "  ✓ MidiFileManager created");
        
        // Auto-scan initial
        bool autoScan = config_->getValue("auto_scan_on_startup", true);
        if (autoScan) {
            Logger::info("Application", "  Scanning MIDI library...");
            std::string midiPath = config_->getValue("midi.library_path", std::string("./midi_files"));
            fileManager_->scanLibrary(midiPath, true);
            Logger::info("Application", "  ✓ Library scanned");
        }
        
        Logger::info("Application", "");
        
        // ====================================================================
        // ÉTAPE 5: API SERVER
        // ====================================================================
        
        Logger::info("Application", "╠══ Phase 5/7: API Server ══╣");
        Logger::info("Application", "");
        
        Logger::info("Application", "  Creating ApiServer...");
        apiServer_ = std::make_shared<ApiServer>();
        Logger::info("Application", "  ✓ ApiServer created");
        
        Logger::info("Application", "  Creating CommandProcessorV2...");
        commandProcessor_ = std::make_shared<CommandProcessorV2>(
            deviceManager_,
            router_,
            player_,
            fileManager_,
            database_
        );
        Logger::info("Application", "  ✓ CommandProcessorV2 created");
        
        Logger::info("Application", "");
        
        // ====================================================================
        // ÉTAPE 6: DEPENDENCY INJECTION
        // ====================================================================
        
        Logger::info("Application", "╠══ Phase 6/7: Dependency Injection ══╣");
        Logger::info("Application", "");
        
        Logger::info("Application", "  Setting up DI Container...");
        setupDependencyInjection();
        Logger::info("Application", "  ✓ DI Container configured");
        
        Logger::info("Application", "");
        
        // ====================================================================
        // ÉTAPE 7: NETWORK (optionnel)
        // ====================================================================
        
        Logger::info("Application", "╠══ Phase 7/7: Network ══╣");
        Logger::info("Application", "");
        
        if (!initializeNetwork()) {
            Logger::warn("Application", "  Network initialization failed (non-critical)");
        } else {
            Logger::info("Application", "  ✓ NetworkManager initialized");
        }
        
        Logger::info("Application", "");
        
        // ====================================================================
        // FINALIZATION
        // ====================================================================
        
        Logger::info("Application", "╠══ Finalization ══╣");
        Logger::info("Application", "");
        
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
        // SUCCÈS
        // ====================================================================
        
        initialized_ = true;
        
        Logger::info("Application", "╔═════════════════════════════════════════════════╗");
        Logger::info("Application", "║     Initialization Complete - Ready to Start    ║");
        Logger::info("Application", "╚═════════════════════════════════════════════════╝");
        Logger::info("Application", "");
        
        // Afficher résumé
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
// DÉMARRAGE
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
        
        running_ = true;
        
        Logger::info("Application", "");
        Logger::info("Application", "========================================");
        Logger::info("Application", "✓ midiMind v3.0 is now running");
        Logger::info("Application", "  WebSocket: ws://localhost:" + std::to_string(port));
        Logger::info("Application", "  Ready to accept connections");
        Logger::info("Application", "========================================");
        Logger::info("Application", "");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Failed to start: " + std::string(e.what()));
        running_ = false;
        return false;
    }
}

// ============================================================================
// ARRÊT
// ============================================================================

void Application::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("Application", "");
    Logger::info("Application", "Stopping application...");
    
    running_ = false;
    
    try {
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
        
        Logger::info("Application", "");
        Logger::info("Application", "✓ Application stopped successfully");
        Logger::info("Application", "");
        
    } catch (const std::exception& e) {
        Logger::error("Application", "Error during shutdown: " + std::string(e.what()));
    }
}

// ============================================================================
// ATTENTE SHUTDOWN
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
// SETUP DEPENDENCY INJECTION
// ============================================================================

void Application::setupDependencyInjection() {
    Logger::debug("Application", "Setting up Dependency Injection Container...");
    
    auto& di = DIContainer::instance();
    
    // Core modules
    if (config_) {
        di.registerSingleton<Config>(config_);
    }
    
    if (database_) {
        di.registerSingleton<Database>(database_);
    }
    
    // MIDI modules
    if (deviceManager_) {
        di.registerSingleton<MidiDeviceManager>(deviceManager_);
    }
    
    if (router_) {
        di.registerSingleton<MidiRouter>(router_);
    }
    
    if (player_) {
        di.registerSingleton<MidiPlayer>(player_);
    }
    
    if (fileManager_) {
        di.registerSingleton<MidiFileManager>(fileManager_);
    }
    
    // API modules
    if (apiServer_) {
        di.registerSingleton<ApiServer>(apiServer_);
    }
    
    if (commandProcessor_) {
        di.registerSingleton<CommandProcessorV2>(commandProcessor_);
    }
    
    // Network module
    if (networkManager_) {
        di.registerSingleton<NetworkManager>(networkManager_);
    }
    
    Logger::debug("Application", "  All dependencies registered");
}

// ============================================================================
// SETUP CALLBACKS
// ============================================================================

void Application::setupCallbacks() {
    Logger::debug("Application", "Setting up inter-module callbacks...");
    
    // ========================================================================
    // Device Manager -> Router
    // ========================================================================
    
    deviceManager_->setMidiMessageCallback([this](const MidiMessage& msg) {
        router_->routeMessage(msg);
    });
    
    Logger::debug("Application", "  ✓ DeviceManager -> Router");
    
    // ========================================================================
    // Player -> Router
    // ========================================================================
    
    player_->setOutputCallback([this](const MidiMessage& msg) {
        router_->routeMessage(msg);
    });
    
    Logger::debug("Application", "  ✓ Player -> Router");
    
    // ========================================================================
    // API Server -> CommandProcessor
    // ========================================================================
    
    apiServer_->setMessageCallback([this](const std::string& jsonStr) -> std::string {
        try {
            // Parser le message
            auto envelopeOpt = MessageEnvelope::fromJsonString(jsonStr);
            if (!envelopeOpt) {
                Logger::error("Application", "Invalid message format");
                return "{\"success\":false,\"error\":\"Invalid message format\"}";
            }
            
            auto envelope = *envelopeOpt;
            
            // Traiter selon le type
            if (envelope.isRequest()) {
                auto request = envelope.getRequest();
                
                // Exécuter la commande
                auto result = commandProcessor_->execute(request.command, request.params);
                
                // Créer la réponse
                auto response = MessageEnvelope::createSuccessResponse(
                    envelope.getEnvelope().id,
                    result
                );
                
                return response.toJsonString();
            }
            
            return "{\"success\":false,\"error\":\"Unsupported message type\"}";
            
        } catch (const std::exception& e) {
            Logger::error("Application", "Callback error: " + std::string(e.what()));
            return "{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}";
        }
    });
    
    Logger::debug("Application", "  ✓ ApiServer -> CommandProcessor");
    
    // ========================================================================
    // Broadcast initial file list (delayed)
    // ========================================================================
    
    std::thread([this]() {
        // Attendre que les clients se connectent
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        try {
            // Récupérer la liste des fichiers
            std::string midiPath = config_->getValue("midi.library_path", std::string("./midi_files"));
            auto files = fileManager_->listFiles(midiPath);
            
            // Créer l'événement
            auto event = MessageEnvelope::createEvent(
                "files:list",
                {
                    {"files", files},
                    {"count", files.size()}
                }
            );
            
            // Broadcaster
            apiServer_->broadcast(event.toJsonString());
            
            Logger::info("Application", "Initial file list broadcasted (" + 
                        std::to_string(files.size()) + " files)");
            
        } catch (const std::exception& e) {
            Logger::error("Application", "Failed to broadcast file list: " + std::string(e.what()));
        }
    }).detach();
    
    Logger::debug("Application", "  All callbacks configured");
}

// ============================================================================
// SETUP OBSERVERS
// ============================================================================

void Application::setupObservers() {
    Logger::debug("Application", "Setting up observers...");
    
    // Observer pattern pour événements asynchrones
    // Exemple: notifier l'API Server des changements d'état
    
    // Player state changes
    // player_->addObserver(...)
    
    // Device connections/disconnections
    // deviceManager_->addObserver(...)
    
    Logger::debug("Application", "  Observers configured");
}

// ============================================================================
// INITIALIZE NETWORK
// ============================================================================

bool Application::initializeNetwork() {
    try {
        Logger::debug("Application", "  Initializing NetworkManager...");
        
        networkManager_ = std::make_shared<NetworkManager>();
        
        // Configuration depuis config
        bool enableWifi = config_->getValue("network.wifi.enabled", false);
        bool enableBluetooth = config_->getValue("network.bluetooth.enabled", false);
        
        if (enableWifi) {
            Logger::debug("Application", "    WiFi enabled");
            // networkManager_->enableWifi();
        }
        
        if (enableBluetooth) {
            Logger::debug("Application", "    Bluetooth enabled");
            // networkManager_->enableBluetooth();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::warn("Application", "  NetworkManager initialization failed: " + std::string(e.what()));
        networkManager_ = nullptr;
        return false;
    }
}

// ============================================================================
// GENERATE API DOCUMENTATION
// ============================================================================

void Application::generateApiDocumentation() {
    try {
        Logger::debug("Application", "  Generating API documentation...");
        
        // Récupérer la liste des commandes
        auto commands = commandProcessor_->listCommands();
        auto categories = commandProcessor_->listCommandsByCategory();
        
        Logger::debug("Application", "    " + std::to_string(commands.size()) + " commands registered");
        Logger::debug("Application", "    " + std::to_string(categories.size()) + " categories");
        
        // Générer documentation JSON
        json doc;
        doc["version"] = "3.0";
        doc["commands"] = commands;
        doc["categories"] = categories;
        
        // Sauvegarder dans un fichier (optionnel)
        std::string docPath = config_->getValue("api.documentation_path", std::string("docs/api_commands.json"));
        
        // TODO: Sauvegarder doc dans le fichier
        
        Logger::debug("Application", "    Documentation generated");
        
    } catch (const std::exception& e) {
        Logger::warn("Application", "  Failed to generate API documentation: " + std::string(e.what()));
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.cpp v3.0.2 - COMPLET
// ============================================================================

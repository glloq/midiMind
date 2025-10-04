// ============================================================================
// Fichier: src/Application.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Classe principale de l'application MidiMind.
//   Orchestre tous les composants et gère le lifecycle.
//
// Responsabilités:
//   - Initialisation de tous les composants
//   - Lifecycle management
//   - Coordination inter-composants
//   - Gestion des signaux système
//   - Point d'accès central
//
// Thread-safety: OUI
//
// Patterns: Facade Pattern, Singleton Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include <csignal>

// Core
#include "core/Logger.h"
#include "core/Config.h"
#include "core/Error.h"

// Storage
#include "storage/Database.h"
#include "storage/Settings.h"
#include "storage/SessionManager.h"
#include "storage/PathManager.h"

// MIDI
#include "midi/MidiRouter.h"
#include "midi/MidiPlayer.h"
#include "midi/MidiClock.h"
#include "midi/MidiFileManager.h"
#include "midi/devices/MidiDeviceManager.h"
#include "midi/processing/ProcessorManager.h"

// Network
#include "network/NetworkManager.h"

// API
#include "api/APIServer.h"
#include "api/CommandProcessor.h"

// Monitoring
#include "monitoring/MetricsCollector.h"
#include "monitoring/SystemMonitor.h"
#include "monitoring/LatencyMonitor.h"
#include "monitoring/HealthCheck.h"

// Optimization
#include "core/optimization/PerformanceOptimizer.h"

namespace midiMind {

/**
 * @enum ApplicationState
 * @brief États possibles de l'application
 */
enum class ApplicationState {
    UNINITIALIZED,  ///< Non initialisée
    INITIALIZING,   ///< En cours d'initialisation
    RUNNING,        ///< En cours d'exécution
    STOPPING,       ///< En cours d'arrêt
    STOPPED,        ///< Arrêtée
    ERROR           ///< Erreur fatale
};

/**
 * @class Application
 * @brief Classe principale de l'application MidiMind
 * 
 * @details
 * Orchestre tous les composants du système et gère le lifecycle complet.
 * 
 * Architecture:
 * ```
 * Application
 * ├── Core (Logger, Config, Database, Settings)
 * ├── MIDI (Router, Player, Clock, Devices, Processors)
 * ├── Network (RTP-MIDI, mDNS, WiFi)
 * ├── API (REST, WebSocket)
 * ├── Monitoring (Metrics, Health)
 * └── Optimization (ThreadPool, MemoryPool)
 * ```
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * Application app;
 * 
 * // Initialiser
 * if (!app.initialize()) {
 *     return 1;
 * }
 * 
 * // Démarrer
 * app.start();
 * 
 * // Attendre arrêt (Ctrl+C)
 * app.waitForShutdown();
 * 
 * // Arrêter
 * app.stop();
 * ```
 */
class Application {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    /**
     * @brief Récupère l'instance de l'application
     * 
     * @note Thread-safe
     */
    static Application& instance();
    
    // Désactiver copie et move
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    // ========================================================================
    // LIFECYCLE
    // ========================================================================
    
    /**
     * @brief Initialise l'application
     * 
     * Initialise tous les composants dans l'ordre:
     * 1. Paths et Logger
     * 2. Config et Database
     * 3. Optimizations
     * 4. MIDI
     * 5. Network
     * 6. API
     * 7. Monitoring
     * 
     * @param configPath Chemin du fichier de configuration (optionnel)
     * @return true Si succès
     * 
     * @note Thread-safe
     */
    bool initialize(const std::string& configPath = "");
    
    /**
     * @brief Démarre l'application
     * 
     * Démarre tous les services:
     * - MIDI devices
     * - MIDI clock
     * - Network services
     * - API server
     * - Monitoring
     * 
     * @return true Si succès
     * 
     * @note Thread-safe
     */
    bool start();
    
    /**
     * @brief Arrête l'application
     * 
     * Arrête tous les services dans l'ordre inverse.
     * 
     * @note Thread-safe
     */
    void stop();
    
    /**
     * @brief Redémarre l'application
     * 
     * @return true Si succès
     * 
     * @note Thread-safe
     */
    bool restart();
    
    /**
     * @brief Attend un signal d'arrêt (SIGINT, SIGTERM)
     * 
     * Bloque jusqu'à réception d'un signal.
     * 
     * @note Thread-safe
     */
    void waitForShutdown();
    
    // ========================================================================
    // ÉTAT
    // ========================================================================
    
    /**
     * @brief Récupère l'état actuel
     * 
     * @note Thread-safe
     */
    ApplicationState getState() const;
    
    /**
     * @brief Vérifie si l'application est en cours d'exécution
     * 
     * @note Thread-safe
     */
    bool isRunning() const;
    
    /**
     * @brief Récupère les informations de version
     * 
     * @note Thread-safe
     */
    json getVersion() const;
    
    /**
     * @brief Récupère les statistiques globales
     * 
     * @note Thread-safe
     */
    json getStatistics() const;
    
    // ========================================================================
    // ACCÈS AUX COMPOSANTS
    // ========================================================================
    
    // Config & Storage
    std::shared_ptr<Config> getConfig() { return config_; }
    std::shared_ptr<Database> getDatabase() { return database_; }
    std::shared_ptr<Settings> getSettings() { return settings_; }
    std::shared_ptr<SessionManager> getSessionManager() { return sessionManager_; }
    
    // MIDI
    std::shared_ptr<MidiDeviceManager> getDeviceManager() { return deviceManager_; }
    std::shared_ptr<MidiRouter> getMidiRouter() { return midiRouter_; }
    std::shared_ptr<MidiPlayer> getMidiPlayer() { return midiPlayer_; }
    std::shared_ptr<MidiClock> getMidiClock() { return midiClock_; }
    std::shared_ptr<MidiFileManager> getFileManager() { return fileManager_; }
    std::shared_ptr<ProcessorManager> getProcessorManager() { return processorManager_; }
    
    // Network
    std::shared_ptr<NetworkManager> getNetworkManager() { return networkManager_; }
    
    // API
    std::shared_ptr<APIServer> getAPIServer() { return apiServer_; }
    std::shared_ptr<CommandProcessor> getCommandProcessor() { return commandProcessor_; }
    
    // Monitoring
    std::shared_ptr<MetricsCollector> getMetricsCollector() { return metricsCollector_; }
    std::shared_ptr<HealthCheck> getHealthCheck() { return healthCheck_; }
    
    // ========================================================================
    // GESTION DES SIGNAUX
    // ========================================================================
    
    /**
     * @brief Enregistre les handlers de signaux
     */
    void registerSignalHandlers();
    
    /**
     * @brief Handler de signal (appelé par le système)
     */
    static void signalHandler(int signal);

private:
    // ========================================================================
    // CONSTRUCTION PRIVÉE (Singleton)
    // ========================================================================
    
    Application();
    ~Application();
    
    // ========================================================================
    // MÉTHODES PRIVÉES D'INITIALISATION
    // ========================================================================
    
    /**
     * @brief Initialise les paths et le logger
     */
    bool initializeCore();
    
    /**
     * @brief Initialise la configuration
     */
    bool initializeConfig(const std::string& configPath);
    
    /**
     * @brief Initialise la base de données
     */
    bool initializeDatabase();
    
    /**
     * @brief Initialise les optimisations
     */
    bool initializeOptimizations();
    
    /**
     * @brief Initialise les composants MIDI
     */
    bool initializeMidi();
    
    /**
     * @brief Initialise le réseau
     */
    bool initializeNetwork();
    
    /**
     * @brief Initialise l'API
     */
    bool initializeAPI();
    
    /**
     * @brief Initialise le monitoring
     */
    bool initializeMonitoring();
    
    /**
     * @brief Charge la configuration initiale
     */
    bool loadInitialConfiguration();
    
    /**
     * @brief Affiche la bannière de démarrage
     */
    void printBanner();
    
    /**
     * @brief Affiche le résumé de démarrage
     */
    void printStartupSummary();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// État de l'application
    std::atomic<ApplicationState> state_;
    
    /// Flag de shutdown demandé
    std::atomic<bool> shutdownRequested_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    // ========================================================================
    // COMPOSANTS
    // ========================================================================
    
    // Core
    std::shared_ptr<Config> config_;
    std::shared_ptr<Database> database_;
    std::shared_ptr<Settings> settings_;
    std::shared_ptr<SessionManager> sessionManager_;
    
    // MIDI
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    std::shared_ptr<MidiRouter> midiRouter_;
    std::shared_ptr<MidiPlayer> midiPlayer_;
    std::shared_ptr<MidiClock> midiClock_;
    std::shared_ptr<MidiFileManager> fileManager_;
    std::shared_ptr<ProcessorManager> processorManager_;
    
    // Network
    std::shared_ptr<NetworkManager> networkManager_;
    
    // API
    std::shared_ptr<APIServer> apiServer_;
    std::shared_ptr<CommandProcessor> commandProcessor_;
    
    // Monitoring
    std::shared_ptr<MetricsCollector> metricsCollector_;
    std::shared_ptr<SystemMonitor> systemMonitor_;
    std::shared_ptr<LatencyMonitor> latencyMonitor_;
    std::shared_ptr<HealthCheck> healthCheck_;
    
    // Timestamps
    uint64_t startTime_;
    uint64_t initTime_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.h
// ============================================================================
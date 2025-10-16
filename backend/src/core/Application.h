// ============================================================================
// Fichier: backend/src/core/Application.h
// Version: 3.0.6 - CORRECTIONS CHEMINS D'INCLUSION
// Date: 2025-10-15
// ============================================================================
// CORRECTIONS v3.0.6:
//   ✅ FIX #1: Correction chemin MidiDeviceManager.h
//   ✅ FIX #2: Vérification chemins des autres includes
//
// Description:
//   Classe principale de l'application MidiMind
// ============================================================================

#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <string>

// Core
#include "Logger.h"
#include "Config.h"

// MIDI - ✅ FIX #1: Chemin corrigé depuis src/core
#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/player/MidiPlayer.h"
#include "../midi/processing/ProcessorManager.h"

// API
#include "../api/ApiServer.h"
#include "../api/CommandProcessorV2.h"

// Storage
#include "../storage/Database.h"
#include "../storage/FileManager.h"
#include "../storage/PresetManager.h"
#include "../storage/SessionManager.h"
#include "../storage/Settings.h"
#include "../storage/PathManager.h"

// Network
#include "../network/NetworkManager.h"
#include "../network/WiFiHotspot.h"

// Monitoring
#include "../monitoring/PerformanceMetrics.h"
#include "../monitoring/SystemMonitor.h"
#include "../monitoring/LatencyMonitor.h"
#include "../monitoring/MetricsCollector.h"

namespace midiMind {

/**
 * @class Application
 * @brief Classe principale de l'application MidiMind
 * 
 * Point d'entrée et orchestrateur central de l'application.
 * Gère l'initialisation, le cycle de vie et l'arrêt de tous les composants.
 * 
 * @version 3.0.6
 * @note Thread-safe
 */
class Application {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    Application();
    
    /**
     * @brief Destructeur
     */
    ~Application();
    
    // Désactiver copie et move
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    // ========================================================================
    // CYCLE DE VIE
    // ========================================================================
    
    /**
     * @brief Initialise l'application
     * 
     * @param configPath Chemin du fichier de configuration
     * @return bool True si succès
     */
    bool initialize(const std::string& configPath = "");
    
    /**
     * @brief Lance l'application
     * 
     * @note Bloquant - retourne quand l'application s'arrête
     */
    void run();
    
    /**
     * @brief Arrête l'application
     * 
     * @note Thread-safe - peut être appelé depuis n'importe quel thread
     */
    void shutdown();
    
    /**
     * @brief Vérifie si l'application est en cours d'exécution
     * 
     * @return bool True si running
     */
    bool isRunning() const { return running_.load(); }
    
    // ========================================================================
    // ACCÈS AUX COMPOSANTS
    // ========================================================================
    
    std::shared_ptr<MidiDeviceManager> getDeviceManager() const { 
        return deviceManager_; 
    }
    
    std::shared_ptr<MidiRouter> getRouter() const { 
        return router_; 
    }
    
    std::shared_ptr<MidiPlayer> getPlayer() const { 
        return player_; 
    }
    
    std::shared_ptr<ProcessorManager> getProcessorManager() const { 
        return processorManager_; 
    }
    
    std::shared_ptr<ApiServer> getApiServer() const { 
        return apiServer_; 
    }
    
    std::shared_ptr<Database> getDatabase() const { 
        return database_; 
    }
    
    std::shared_ptr<MidiFileManager> getFileManager() const { 
        return fileManager_; 
    }
    
    std::shared_ptr<PresetManager> getPresetManager() const { 
        return presetManager_; 
    }
    
    std::shared_ptr<SessionManager> getSessionManager() const { 
        return sessionManager_; 
    }
    
    std::shared_ptr<Settings> getSettings() const { 
        return settings_; 
    }
    
    std::shared_ptr<NetworkManager> getNetworkManager() const { 
        return networkManager_; 
    }
    
    std::shared_ptr<WiFiHotspot> getWiFiHotspot() const { 
        return wifiHotspot_; 
    }
    
    std::shared_ptr<MetricsCollector> getMetricsCollector() const { 
        return metricsCollector_; 
    }
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES - INITIALISATION
    // ========================================================================
    
    /**
     * @brief Initialise le système de chemins
     */
    bool initializePaths();
    
    /**
     * @brief Initialise la base de données
     */
    bool initializeDatabase();
    
    /**
     * @brief Initialise le système MIDI
     */
    bool initializeMidi();
    
    /**
     * @brief Initialise l'API
     */
    bool initializeApi();
    
    /**
     * @brief Initialise le réseau
     */
    bool initializeNetwork();
    
    /**
     * @brief Initialise le monitoring
     */
    bool initializeMonitoring();
    
    /**
     * @brief Charge la configuration
     */
    bool loadConfiguration(const std::string& configPath);
    
    // ========================================================================
    // MEMBRES PRIVÉS - ÉTAT
    // ========================================================================
    
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - COMPOSANTS CORE
    // ========================================================================
    
    // Storage
    std::shared_ptr<Database> database_;
    std::shared_ptr<MidiFileManager> fileManager_;
    std::shared_ptr<PresetManager> presetManager_;
    std::shared_ptr<SessionManager> sessionManager_;
    std::shared_ptr<Settings> settings_;
    
    // MIDI
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<MidiPlayer> player_;
    std::shared_ptr<ProcessorManager> processorManager_;
    
    // API
    std::shared_ptr<ApiServer> apiServer_;
    std::shared_ptr<CommandProcessorV2> commandProcessor_;
    
    // Network
    std::shared_ptr<NetworkManager> networkManager_;
    std::shared_ptr<WiFiHotspot> wifiHotspot_;
    
    // Monitoring
    std::shared_ptr<SystemMonitor> systemMonitor_;
    std::shared_ptr<LatencyMonitor> latencyMonitor_;
    std::shared_ptr<MetricsCollector> metricsCollector_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - THREADS
    // ========================================================================
    
    std::thread mainLoopThread_;
    
    // ========================================================================
    // MÉTHODES PRIVÉES - BOUCLE PRINCIPALE
    // ========================================================================
    
    /**
     * @brief Boucle principale de l'application
     */
    void mainLoop();
    
    /**
     * @brief Traite les événements
     */
    void processEvents();
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.h v3.0.6 - CORRIGÉ
// ============================================================================

// ============================================================================
// Fichier: backend/src/core/Application.h
// Version: 3.0.5 - CORRECTIONS CRITIQUES PHASE 1
// Date: 2025-10-15
// ============================================================================
// Description:
//   Header de la classe Application - Orchestrateur principal
//
// CHANGEMENTS v3.0.5:
//   ✅ CORRECTION 1.1: Destructeur thread-safe avec join explicite
//   ✅ CORRECTION 1.3: Singleton thread-safe (Meyers Singleton)
//   ✅ Suppression static Application* instance_ (remplacé par Meyers)
//   ✅ Ajout statusRunning_ et hotPlugRunning_ atomiques
//
// PRÉSERVÉ DE v3.0.4:
//   ✅ Toutes les fonctionnalités existantes
//   ✅ Hot-plug monitoring
//   ✅ Status broadcast
//   ✅ Gestion signaux
//   ✅ getStatus() et getHealth()
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

#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#include "../midi/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiFileManager.h"
#include "../api/ApiServer.h"
#include "../api/commands/CommandProcessorV2.h"
#include "../network/NetworkManager.h"
#include "../storage/Database.h"

namespace midiMind {

using json = nlohmann::json;

/**
 * @class Application
 * @brief Orchestrateur principal de l'application MidiMind
 * 
 * Responsabilités:
 * - Initialisation séquentielle (7 phases)
 * - Gestion cycle de vie (initialize/start/stop/waitForShutdown)
 * - Hot-plug monitoring des devices MIDI
 * - Status broadcast via WebSocket
 * - Gestion signaux système (SIGINT, SIGTERM)
 * - Dependency Injection Container
 * 
 * Scénarios supportés:
 * 1. ✅ Startup complet - Initialisation → API ready
 * 2. ✅ Hot-plug device - USB keyboard → Détection → Routes
 * 3. ✅ Playback fichier - Charger MIDI → Play → MIDI out
 * 4. ✅ WebSocket live - Frontend → Backend → Response
 * 5. ✅ Shutdown propre - SIGTERM → Cleanup → Exit
 * 
 * Pattern: Meyers Singleton (thread-safe C++11)
 * Thread-safety: Tous les membres atomiques ou protégés par mutex
 * 
 * @example Utilisation
 * ```cpp
 * auto& app = Application::instance();
 * if (!app.initialize("./config.json")) {
 *     return EXIT_FAILURE;
 * }
 * if (!app.start()) {
 *     return EXIT_FAILURE;
 * }
 * app.waitForShutdown();
 * app.stop();
 * ```
 */
class Application {
public:
    // ========================================================================
    // SINGLETON - v3.0.5: MEYERS SINGLETON (THREAD-SAFE)
    // ========================================================================
    
    /**
     * @brief Récupère l'instance unique (thread-safe depuis C++11)
     * @return Référence à l'instance
     * @note Meyers Singleton - Initialisation garantie thread-safe
     */
    static Application& instance();
    
    // Désactiver copie et assignation
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    /**
     * @brief Destructeur - Arrête proprement l'application
     * 
     * v3.0.5: Join explicite des threads avant destruction
     * - Attend hotPlugThread_ si actif
     * - Attend statusThread_ si actif
     * - Libère NetworkManager proprement
     * - Nettoie DIContainer
     */
    ~Application();
    
    // ========================================================================
    // CYCLE DE VIE
    // ========================================================================
    
    /**
     * @brief Initialise l'application (7 phases)
     * 
     * Phases:
     * 1. Configuration (Config.h)
     * 2. Database (SQLite)
     * 3. Storage (Settings, Presets)
     * 4. MIDI Core (DeviceManager, Router, Player)
     * 5. API (ApiServer, CommandProcessor)
     * 6. DI Container
     * 7. Network (optionnel)
     * 
     * @param configPath Chemin vers config.json
     * @return true Si succès
     */
    bool initialize(const std::string& configPath = "");
    
    /**
     * @brief Démarre tous les services
     * 
     * Actions:
     * - Démarre API Server (WebSocket)
     * - Scan initial devices
     * - Démarre hot-plug monitoring
     * - Démarre status broadcast
     * - Démarre NetworkManager (si activé)
     * 
     * @return true Si succès
     */
    bool start();
    
    /**
     * @brief Arrête proprement tous les services
     * 
     * Actions (ordre):
     * 1. Arrêt hot-plug monitoring
     * 2. Arrêt status broadcast
     * 3. Arrêt MidiPlayer
     * 4. Déconnexion devices
     * 5. Arrêt API Server
     * 6. Fermeture Database
     * 7. Arrêt NetworkManager
     * 
     * Timeout: 5 secondes max
     */
    void stop();
    
    /**
     * @brief Attend un signal de shutdown
     * 
     * Bloque jusqu'à :
     * - SIGINT (Ctrl+C)
     * - SIGTERM (kill)
     * - Appel stop()
     */
    void waitForShutdown();
    
    // ========================================================================
    // STATUS & HEALTH
    // ========================================================================
    
    /**
     * @brief Récupère le status global
     * 
     * Inclut:
     * - État (initialized, running)
     * - Uptime (secondes)
     * - Version
     * - Statistiques devices
     * - Statistiques routes
     * - Statistiques player
     * - Statistiques API
     * 
     * @return json Status complet
     * @note Thread-safe
     */
    json getStatus() const;
    
    /**
     * @brief Vérifie la santé de l'application
     * 
     * Retourne health checks de tous les modules
     * 
     * @return json Health status avec checks détaillés
     * @note Thread-safe
     */
    json getHealth() const;
    
    /**
     * @brief Vérifie si initialisé
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief Vérifie si en cours d'exécution
     */
    bool isRunning() const { return running_.load(); }
    
    // ========================================================================
    // ACCÈS AUX MODULES
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
    
    std::shared_ptr<MidiFileManager> getFileManager() const {
        return fileManager_;
    }
    
    std::shared_ptr<ApiServer> getApiServer() const {
        return apiServer_;
    }
    
    std::shared_ptr<CommandProcessorV2> getCommandProcessor() const {
        return commandProcessor_;
    }
    
    std::shared_ptr<NetworkManager> getNetworkManager() const {
        return networkManager_;
    }
    
    std::shared_ptr<Database> getDatabase() const {
        return database_;
    }
    
    // ========================================================================
    // COMPTEUR SIGNAUX (PUBLIC POUR SIGNAL HANDLER)
    // ========================================================================
    
    static std::atomic<int> signalCount_; // v3.0.4: Compteur signaux
    
private:
    // ========================================================================
    // CONSTRUCTEUR PRIVÉ (SINGLETON)
    // ========================================================================
    
    Application();
    
    // ========================================================================
    // MÉTHODES PRIVÉES - INITIALISATION
    // ========================================================================
    
    /**
     * @brief Configure les callbacks entre modules
     */
    void setupCallbacks();
    
    /**
     * @brief Configure les observers (pattern Observer)
     */
    void setupObservers();
    
    /**
     * @brief Configure le Dependency Injection Container
     */
    void setupDependencyInjection();
    
    /**
     * @brief Initialise le NetworkManager (optionnel)
     */
    bool initializeNetwork();
    
    /**
     * @brief Génère la documentation API (JSON)
     */
    void generateApiDocumentation();
    
    /**
     * @brief Génère la documentation API (HTML)
     */
    void generateHtmlDocumentation(const json& doc, const std::string& outputPath);
    
    // ========================================================================
    // MÉTHODES PRIVÉES - HOT-PLUG MONITORING
    // ========================================================================
    
    /**
     * @brief Démarre le thread de monitoring hot-plug
     */
    void startHotPlugMonitoring();
    
    /**
     * @brief Arrête le thread de monitoring hot-plug
     */
    void stopHotPlugMonitoring();
    
    /**
     * @brief Boucle du thread hot-plug (scan toutes les 2s)
     * 
     * Détecte:
     * - Nouveaux devices connectés
     * - Devices déconnectés
     * 
     * Broadcast events via WebSocket
     */
    void hotPlugMonitorLoop();
    
    // ========================================================================
    // MÉTHODES PRIVÉES - STATUS BROADCAST
    // ========================================================================
    
    /**
     * @brief Démarre le thread de broadcast status
     */
    void startStatusBroadcast();
    
    /**
     * @brief Arrête le thread de broadcast status
     */
    void stopStatusBroadcast();
    
    /**
     * @brief Boucle du thread status (broadcast toutes les 5s)
     * 
     * Envoie status complet à tous les clients WebSocket
     */
    void statusBroadcastLoop();
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * @brief Convertit DeviceType en string
     */
    std::string deviceTypeToString(DeviceType type) const;
    
    /**
     * @brief Convertit MidiMessageType en string
     */
    std::string midiMessageTypeToString(MidiMessageType type) const;
    
    // ========================================================================
    // MEMBRES PRIVÉS - ÉTAT
    // ========================================================================
    
    /// Application initialisée
    bool initialized_;
    
    /// Application en cours d'exécution
    std::atomic<bool> running_;
    
    /// Timestamp de démarrage (pour uptime)
    std::chrono::steady_clock::time_point startTime_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - MONITORING & THREADS
    // ========================================================================
    
    /// Thread hot-plug monitoring
    std::thread hotPlugThread_;
    
    /// Flag running pour hot-plug (atomique pour thread-safety)
    std::atomic<bool> hotPlugRunning_;
    
    /// Thread status broadcast
    std::thread statusThread_;
    
    /// Flag running pour status (atomique pour thread-safety)
    std::atomic<bool> statusRunning_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - MODULES CORE
    // ========================================================================
    
    /// Configuration
    std::shared_ptr<Config> config_;
    
    /// Base de données SQLite
    std::shared_ptr<Database> database_;
    
    /// Gestionnaire de devices MIDI
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    
    /// Routeur MIDI
    std::shared_ptr<MidiRouter> router_;
    
    /// Lecteur de fichiers MIDI
    std::shared_ptr<MidiPlayer> player_;
    
    /// Gestionnaire de fichiers MIDI
    std::shared_ptr<MidiFileManager> fileManager_;
    
    /// Serveur WebSocket API
    std::shared_ptr<ApiServer> apiServer_;
    
    /// Processeur de commandes
    std::shared_ptr<CommandProcessorV2> commandProcessor_;
    
    /// Gestionnaire réseau (optionnel)
    std::shared_ptr<NetworkManager> networkManager_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.h v3.0.5
// ============================================================================
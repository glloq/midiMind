// ============================================================================
// Fichier: backend/src/core/Application.h
// Version: 3.0.1
// ============================================================================
// Description:
//   Classe principale de l'application midiMind. Gère le cycle de vie complet
//   de l'application : initialisation, configuration, exécution et arrêt.
//   Coordonne tous les sous-systèmes (MIDI, API, réseau, fichiers).
//
// Responsabilités:
//   - Initialiser tous les modules (Device Manager, Router, Player, API, etc.)
//   - Configurer les callbacks et observers entre modules
//   - Gérer les threads de monitoring (status broadcast, thermal monitoring)
//   - Détecter le modèle de Raspberry Pi et optimiser les paramètres
//   - Assurer un arrêt propre de tous les composants
//
// Architecture:
//   Application (cette classe)
//   ├── MidiDeviceManager    : Gestion des périphériques MIDI (USB/WiFi/BT)
//   ├── MidiRouter           : Routage MIDI avec strategies de scheduling
//   ├── MidiPlayer           : Lecture de fichiers MIDI
//   ├── MidiFileManager      : Bibliothèque et playlists
//   ├── ApiServer            : Serveur WebSocket pour API
//   ├── CommandProcessorV2   : Traitement des commandes API (Command Pattern)
//   ├── SysExHandler         : Gestion des messages SysEx
//   └── NetworkManager       : Gestion WiFi Hotspot et Bluetooth
//
// Design Patterns Utilisés:
//   - Singleton (via instance unique dans main.cpp)
//   - Observer Pattern (pour notifications entre modules)
//   - Dependency Injection (via DIContainer)
//   - Strategy Pattern (pour scheduling MIDI)
//   - Command Pattern (pour commandes API)
//
// Auteur: midiMind Team
// Date: 2025-10-09
// Version: 3.0.1
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES SYSTÈME
// ============================================================================
#include <memory>        // Pour std::shared_ptr, std::unique_ptr
#include <thread>        // Pour std::thread
#include <chrono>        // Pour std::chrono (timing)
#include <atomic>        // Pour std::atomic (thread-safe flags)
#include <string>        // Pour std::string
#include <unistd.h>      // Pour geteuid() (vérification root)

// ============================================================================
// INCLUDES PROJET - CORE
// ============================================================================
#include "Config.h"              // Configuration globale
#include "Logger.h"              // Système de logging
#include "ErrorManager.h"        // Gestion des erreurs

// ============================================================================
// INCLUDES PROJET - MIDI
// ============================================================================
#include "../midi/devices/MidiDeviceManager.h"  // Gestion des périphériques MIDI
#include "../midi/MidiRouter.h"                 // Routage des messages MIDI
#include "../midi/MidiPlayer.h"                 // Lecture de fichiers MIDI
#include "../midi/MidiFileManager.h"            // Bibliothèque de fichiers MIDI
#include "../midi/sysex/SysExHandler.h"         // Gestion des messages SysEx

// ============================================================================
// INCLUDES PROJET - API
// ============================================================================
#include "../api/ApiServer.h"                   // Serveur WebSocket
#include "../api/CommandProcessorV2.h"          // Processeur de commandes V2

// ============================================================================
// INCLUDES PROJET - NETWORK
// ============================================================================
#include "../network/NetworkManager.h"          // Gestion réseau (WiFi/BT)

// ============================================================================
// NAMESPACE
// ============================================================================
namespace midiMind {

// ============================================================================
// CLASSE: Application
// ============================================================================

/**
 * @class Application
 * @brief Classe principale de l'application midiMind
 * 
 * Gère l'initialisation, la configuration et le cycle de vie complet
 * de tous les composants du système MIDI. Coordonne les interactions
 * entre les différents modules et assure la cohérence globale.
 * 
 * @details
 * Cette classe implémente le pattern Facade pour simplifier l'interface
 * avec les nombreux sous-systèmes de l'application. Elle gère également
 * le cycle de vie des threads de monitoring et broadcasting.
 * 
 * Cycle de vie:
 * 1. Constructeur → Initialise tous les composants
 * 2. run() → Lance la boucle principale
 * 3. stop() → Arrête proprement tous les threads
 * 4. Destructeur → Libère les ressources
 * 
 * @note Thread-safe : Les méthodes publiques peuvent être appelées
 *       depuis différents threads
 * 
 * @example Utilisation:
 * @code
 * int main() {
 *     try {
 *         Application app;
 *         app.run();  // Bloque jusqu'à stop()
 *     } catch (const std::exception& e) {
 *         std::cerr << "Error: " << e.what() << std::endl;
 *         return 1;
 *     }
 *     return 0;
 * }
 * @endcode
 */
class Application {
public:
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur - Initialise tous les modules
     * 
     * @throws std::runtime_error si l'initialisation échoue
     * 
     * @details
     * Initialise dans l'ordre:
     * 1. Core (Config, Logger, ErrorManager)
     * 2. MIDI (DeviceManager, Router, Player, FileManager)
     * 3. SysEx Handler
     * 4. API (CommandProcessor, ApiServer)
     * 5. Network (NetworkManager)
     * 6. Callbacks entre modules
     */
    Application();
    
    /**
     * @brief Destructeur - Arrête proprement l'application
     * 
     * @details
     * Arrête tous les threads actifs, ferme toutes les connexions,
     * et libère toutes les ressources. Garantit un arrêt propre
     * même en cas d'erreur.
     */
    ~Application();
    
    // Désactiver copie et assignation
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Lance l'application et entre dans la boucle principale
     * 
     * @details
     * Cette méthode bloque jusqu'à ce que stop() soit appelé.
     * Configure le réseau, démarre tous les services, et gère
     * les threads de monitoring.
     * 
     * Séquence d'exécution:
     * 1. Détection du modèle de Raspberry Pi
     * 2. Configuration du réseau (WiFi/BT)
     * 3. Démarrage de l'API Server
     * 4. Lancement des threads de monitoring
     * 5. Boucle principale (attend le signal stop)
     */
    void run();
    
    /**
     * @brief Arrête proprement l'application
     * 
     * @details
     * Arrête tous les threads, ferme les connexions,
     * et nettoie les ressources. Peut être appelé depuis
     * n'importe quel thread (signal handler, API, etc.)
     * 
     * Séquence d'arrêt:
     * 1. Positionne le flag running_ à false
     * 2. Attend la fin des threads de monitoring
     * 3. Arrête l'API Server
     * 4. Arrête le player
     * 5. Déconnecte tous les devices
     */
    void stop();
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - ÉTAT
    // ========================================================================
    
    /**
     * @brief Vérifie si l'application est en cours d'exécution
     * 
     * @return true Si l'application tourne
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Vérifie si l'initialisation est complète
     * 
     * @return true Si tous les modules sont initialisés
     */
    bool isInitialized() const { return initialized_; }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - ACCESSEURS
    // ========================================================================
    
    /**
     * @brief Récupère le serveur API
     * @return std::shared_ptr<ApiServer>
     */
    std::shared_ptr<ApiServer> getApiServer();
    
    /**
     * @brief Récupère le routeur MIDI
     * @return std::shared_ptr<MidiRouter>
     */
    std::shared_ptr<MidiRouter> getMidiRouter();
    
    /**
     * @brief Récupère le lecteur MIDI
     * @return std::shared_ptr<MidiPlayer>
     */
    std::shared_ptr<MidiPlayer> getMidiPlayer();
    
    /**
     * @brief Récupère le gestionnaire de périphériques
     * @return std::shared_ptr<MidiDeviceManager>
     */
    std::shared_ptr<MidiDeviceManager> getDeviceManager();
    
    /**
     * @brief Récupère le processeur de commandes
     * @return std::shared_ptr<CommandProcessorV2>
     */
    std::shared_ptr<CommandProcessorV2> getCommandProcessor();

private:
    // ========================================================================
    // MÉTHODES PRIVÉES - INITIALISATION
    // ========================================================================
    
    /**
     * @brief Configure les callbacks entre les modules
     * 
     * @details
     * Établit les connexions entre Device Manager, Router, Player, etc.
     * Configure les observers pour les événements:
     * - Connexion/déconnexion de devices
     * - Changements d'état du player
     * - Messages MIDI
     * - Événements SysEx
     */
    void setupCallbacks();
    
    /**
     * @brief Détecte le modèle de Raspberry Pi
     * 
     * @return std::string Modèle détecté (ex: "Pi 4 Model B")
     * 
     * @details
     * Lit /proc/device-tree/model et /proc/cpuinfo pour identifier
     * le modèle précis. Ajuste ensuite les paramètres de performance
     * (FPS player, broadcast rate) en fonction.
     */
    std::string detectRaspberryPiModel();
    
    /**
     * @brief Ajuste les paramètres selon le modèle de Pi
     * 
     * @param model Modèle détecté
     * 
     * @details
     * - Pi Zero : 50 FPS player, 5 FPS broadcast
     * - Pi 3 : 100 FPS player, 10 FPS broadcast
     * - Pi 4/5 : 200 FPS player, 20 FPS broadcast
     */
    void adjustPerformanceSettings(const std::string& model);
    
    // ========================================================================
    // MÉTHODES PRIVÉES - THREADS
    // ========================================================================
    
    /**
     * @brief Boucle de broadcast du statut du player
     * 
     * @details
     * Envoie périodiquement le statut du player aux clients WebSocket.
     * Fréquence ajustée selon le modèle de Raspberry Pi détecté.
     * 
     * Informations broadcastées:
     * - État (playing/paused/stopped)
     * - Position et durée
     * - Tempo et transposition
     * - Pistes (mute/solo)
     */
    void statusBroadcastLoop();
    
    /**
     * @brief Boucle de monitoring thermique
     * 
     * @details
     * Surveille la température du CPU via /sys/class/thermal.
     * Émet des alertes si surchauffe détectée.
     * Ajuste automatiquement les performances si nécessaire.
     */
    void thermalMonitoringLoop();
    
    // ========================================================================
    // MÉTHODES PRIVÉES - BROADCAST
    // ========================================================================
    
    /**
     * @brief Broadcast un message MIDI à tous les clients
     * 
     * @param msg Message MIDI à broadcaster
     */
    void broadcastMidiMessage(const MidiMessage& msg);
    
    /**
     * @brief Broadcast l'état du player
     * 
     * @param state État ("playing", "paused", "stopped")
     * @param position Position actuelle en ms
     */
    void broadcastPlaybackState(const std::string& state, double position);
    
    /**
     * @brief Broadcast un événement de device
     * 
     * @param event Type d'événement ("connected", "disconnected")
     * @param deviceId ID du device
     */
    void broadcastDeviceEvent(const std::string& event, 
                             const std::string& deviceId);
       void setupLogging();  // Existant

// ============================================================================
// AJOUT v3.1.1: Méthode pour appliquer config logger à chaud
// ==========================================================================
    /**
     * @brief Applique une nouvelle configuration logger à chaud
     * @param newConfig Nouvelle configuration
     * @return true Si succès
     * @version 3.1.1
     */
    bool applyLoggerConfig(const Config::LoggerConfig& newConfig);  
    // ========================================================================
    // MEMBRES PRIVÉS - COMPOSANTS PRINCIPAUX
    // ========================================================================
    
    /// Gestionnaire de périphériques MIDI (USB, WiFi, Bluetooth)
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    
    /// Routeur MIDI - gère les routes canal → device
    std::shared_ptr<MidiRouter> router_;
    
    /// Lecteur de fichiers MIDI
    std::shared_ptr<MidiPlayer> player_;
    
    /// Gestionnaire de bibliothèque de fichiers MIDI et playlists
    std::shared_ptr<MidiFileManager> fileManager_;
    
    /// Gestionnaire de messages SysEx
    std::shared_ptr<SysExHandler> sysexHandler_;
    
    /// Serveur API WebSocket
    std::shared_ptr<ApiServer> apiServer_;
    
    /// Processeur de commandes API (v2)
    std::shared_ptr<CommandProcessorV2> commandProcessor_;
    
    /// Gestionnaire réseau (WiFi Hotspot, Bluetooth)
    std::shared_ptr<NetworkManager> networkManager_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - THREADS
    // ========================================================================
    
    /// Thread de broadcast du statut
    std::thread statusThread_;
    
    /// Thread de monitoring thermique
    std::thread thermalThread_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - CONFIGURATION DYNAMIQUE
    // ========================================================================
    
    /// Fréquence de traitement du player (images/sec)
    /// Ajustée selon le modèle de Raspberry Pi:
    /// - Pi Zero: 50 FPS
    /// - Pi 3: 100 FPS  
    /// - Pi 4/5: 200 FPS
    int playerFPS_ = 100;
    
    /// Fréquence de broadcast WebSocket (images/sec)
    /// Ajustée selon le modèle de Raspberry Pi:
    /// - Pi Zero: 5 FPS
    /// - Pi 3: 10 FPS
    /// - Pi 4/5: 20 FPS
    int broadcastFPS_ = 10;
    
    // ========================================================================
    // MEMBRES PRIVÉS - ÉTAT
    // ========================================================================
    
    /// Flag indiquant si l'application est en cours d'exécution
    std::atomic<bool> running_{false};
    
    /// Flag indiquant si l'initialisation est complète
    std::atomic<bool> initialized_{false};
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.h
// ============================================================================

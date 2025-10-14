// ============================================================================
// Fichier: src/core/Application.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
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
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once
// ============================================================================
// INCLUDES SYSTÈME
// ============================================================================
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <unistd.h>

// ============================================================================
// INCLUDES PROJET - CORE
// ============================================================================
#include "Config.h"
#include "Logger.h"
#include "ErrorManager.h"

// ============================================================================
// INCLUDES PROJET - MIDI
// ============================================================================
#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiFileManager.h"
#include "../midi/sysex/SysExHandler.h"

// ============================================================================
// INCLUDES PROJET - API
// ============================================================================
#include "../api/ApiServer.h"
#include "../api/CommandProcessorV2.h"

// ============================================================================
// INCLUDES PROJET - NETWORK
// ============================================================================
#include "../network/NetworkManager.h"

// ============================================================================
// ✅ NOUVEAU - INCLUDES PROJET - STORAGE
// ============================================================================
#include "../storage/Database.h"
#include "../storage/PresetManager.h"    
#include "../storage/FileManager.h"      
#include "../storage/SessionManager.h"
#include "../storage/Settings.h"

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
 * 1. Construction : Initialisation de tous les modules
 * 2. run()        : Démarrage de tous les services et boucle principale
 * 3. stop()       : Arrêt propre de tous les services
 * 4. Destruction  : Nettoyage final des ressources
 * 
 * @note Cette classe n'est pas thread-safe pour la création/destruction,
 *       mais les opérations run() et stop() peuvent être appelées depuis
 *       différents threads (typiquement: main thread et signal handler).
 * 
 * @warning Cette classe ne doit avoir qu'une seule instance dans l'application.
 *          L'instanciation multiple n'est pas supportée et causerait des conflits.
 * 
 * @example Utilisation typique:
 * @code
 * int main() {
 *     try {
 *         auto app = std::make_unique<Application>();
 *         app->run();  // Bloque jusqu'à l'arrêt
 *         return 0;
 *     } catch (const std::exception& e) {
 *         std::cerr << "Error: " << e.what() << std::endl;
 *         return 1;
 *     }
 * }
 * @endcode
 */
class Application {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur - Initialise tous les modules
     * 
     * Étapes d'initialisation:
     * 1. Chargement de la configuration (Config::instance())
     * 2. Configuration du logger
     * 3. Détection du modèle de Raspberry Pi et optimisation
     * 4. Création des modules core (Device Manager, Router, Player, etc.)
     * 5. Configuration du Dependency Injection Container
     * 6. Setup des observers et callbacks
     * 7. Génération de la documentation API
     * 
     * @throws std::runtime_error Si l'initialisation échoue
     * @throws std::bad_alloc Si allocation mémoire échoue
     * 
     * @note Le constructeur peut prendre quelques secondes sur Raspberry Pi Zero
     *       en raison de la détection hardware et de l'initialisation ALSA.
     * 
     * @see Config::load()
     * @see detectAndOptimizeForRaspberryPi()
     */
    Application();
    
    /**
     * @brief Destructeur - Arrête proprement l'application
     * 
     * Assure que tous les threads sont arrêtés et toutes les ressources
     * sont libérées. Appelle automatiquement stop() si nécessaire.
     * 
     * @note Le destructeur ne lance pas d'exceptions (noexcept implicite)
     */
    ~Application();
    
    // ========================================================================
    // DÉSACTIVATION COPIE ET ASSIGNATION
    // ========================================================================
    
    /**
     * @brief Constructeur de copie supprimé
     * 
     * L'application gère des ressources uniques (threads, connexions réseau,
     * périphériques MIDI) qui ne peuvent pas être copiées.
     */
    Application(const Application&) = delete;
    
    /**
     * @brief Opérateur d'assignation supprimé
     * 
     * Pour les mêmes raisons que le constructeur de copie.
     */
    Application& operator=(const Application&) = delete;
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - CONTRÔLE DU CYCLE DE VIE
    // ========================================================================
    
    /**
     * @brief Lance l'application et entre dans la boucle principale
     * 
     * Cette méthode:
     * 1. Démarre le serveur API WebSocket
     * 2. Configure le réseau (WiFi Hotspot si nécessaire)
     * 3. Démarre les threads de monitoring (status, thermal)
     * 4. Affiche les informations de démarrage
     * 5. Entre dans la boucle principale (bloquante)
     * 
     * La méthode bloque jusqu'à ce que stop() soit appelé (typiquement
     * par un signal handler en réponse à Ctrl+C ou SIGTERM).
     * 
     * @throws std::runtime_error Si le démarrage échoue
     * 
     * @note Cette méthode est bloquante et ne retourne que lors de l'arrêt
     * 
     * @see stop()
     * @see statusBroadcastLoop()
     * @see thermalMonitoringLoop()
     * 
     * @example
     * @code
     * Application app;
     * app.run();  // Bloque ici jusqu'à l'arrêt
     * // Code après arrêt...
     * @endcode
     */
    void run();
    
    /**
     * @brief Arrête proprement l'application
     * 
     * Séquence d'arrêt:
     * 1. Positionne le flag running_ à false
     * 2. Arrête les threads de monitoring
     * 3. Arrête le serveur API (ferme les connexions WebSocket)
     * 4. Arrête le player MIDI
     * 5. Arrête le router MIDI
     * 6. Déconnecte tous les périphériques MIDI
     * 7. Arrête le réseau (désactive hotspot si actif)
     * 
     * @note Cette méthode est thread-safe et peut être appelée depuis
     *       n'importe quel thread (typiquement depuis un signal handler)
     * 
     * @note Idempotente : peut être appelée plusieurs fois sans danger
     * 
     * @see run()
     */
    void stop();
    
    /**
     * @brief Vérifie si l'application est en cours d'exécution
     * 
     * @return true si l'application tourne (run() a été appelé et stop() pas encore)
     * @return false si l'application est arrêtée ou en cours d'arrêt
     * 
     * @note Thread-safe (utilise std::atomic)
     */
    bool isRunning() const { return running_; }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES - INITIALISATION
    // ========================================================================
    
    /**
     * @brief Configure le système de logging
     * 
     * Lit la configuration de logging depuis Config et applique les paramètres:
     * - Niveau de log (DEBUG, INFO, WARNING, ERROR)
     * - Destination (console, fichier, syslog)
     * - Format des messages
     * - Rotation des logs si fichier
     * 
     * @note Appelé automatiquement par le constructeur
     */
    void configureLogger();
    
    /**
     * @brief Détecte le modèle de Raspberry Pi et optimise les paramètres
     * 
     * Lit /proc/cpuinfo pour identifier le modèle de Pi et ajuste:
     * - playerFPS_ (fréquence de traitement MIDI)
     * - broadcastFPS_ (fréquence de broadcast WebSocket)
     * - Taille des buffers
     * - Stratégie de scheduling
     * 
     * Optimisations par modèle:
     * - Pi Zero/Zero W  : FPS réduits (50/5), buffers petits
     * - Pi 3B/3B+       : FPS moyens (100/10), buffers moyens
     * - Pi 4/Pi 5       : FPS élevés (200/20), buffers grands
     * 
     * @note Appelé automatiquement par le constructeur
     * @see playerFPS_
     * @see broadcastFPS_
     */
    void detectAndOptimizeForRaspberryPi();
    
    /**
     * @brief Affiche les informations de démarrage dans la console
     * 
     * Affiche:
     * - Version de l'application
     * - Modèle de Raspberry Pi détecté
     * - Configuration réseau (IP, port API)
     * - Liste des périphériques MIDI détectés
     * - Nombre de fichiers MIDI dans la bibliothèque
     * - Paramètres de performance (FPS)
     * 
     * @note Appelé automatiquement par run()
     */
    void displayStartupInfo();
    
    // ========================================================================
    // MÉTHODES PRIVÉES - CONFIGURATION DES MODULES
    // ========================================================================
    
    /**
     * @brief Configure les callbacks entre les modules
     * 
     * Établit les connexions entre les différents modules:
     * - Device Manager → Router (nouveaux périphériques)
     * - Player → Router (messages MIDI à envoyer)
     * - Player → API (changements d'état pour broadcast)
     * - Router → Devices (messages MIDI sortants)
     * 
     * Utilise des lambdas et std::bind pour éviter le couplage fort.
     * 
     * @note Appelé automatiquement par le constructeur
     */
    void setupCallbacks();
    
    /**
     * @brief Configure les observers (Observer Pattern)
     * 
     * Enregistre les observateurs pour les événements système:
     * - Changements d'état du player (play, pause, stop)
     * - Connexion/déconnexion de périphériques MIDI
     * - Erreurs critiques nécessitant un broadcast
     * - Changements de configuration
     * 
     * Les observers sont notifiés via l'ApiServer pour broadcast
     * aux clients WebSocket connectés.
     * 
     * @note Appelé automatiquement par le constructeur
     */
    void setupObservers();
    
    /**
     * @brief Génère la documentation API au format JSON
     * 
     * Génère un fichier JSON contenant:
     * - Liste de toutes les commandes API disponibles
     * - Paramètres requis et optionnels pour chaque commande
     * - Types de paramètres et validation
     * - Exemples d'utilisation
     * - Réponses attendues
     * 
     * Le fichier est sauvegardé dans docs/api_documentation.json
     * et peut être servi aux clients WebSocket via la commande "api.doc".
     * 
     * @note Appelé automatiquement par le constructeur
     * @see CommandFactory::listCommands()
     */
    void generateApiDocumentation();
    
    // ========================================================================
    // MÉTHODES PRIVÉES - THREADS DE MONITORING
    // ========================================================================
    
    /**
     * @brief Boucle de broadcast du statut du player
     * 
     * Thread qui broadcast périodiquement le statut du player via WebSocket:
     * - Position actuelle (ms)
     * - Durée totale (ms)
     * - État (playing, paused, stopped)
     * - Tempo actuel
     * - Transposition
     * - Liste des pistes avec leur état (mute/solo)
     * 
     * Fréquence ajustée selon le modèle de Raspberry Pi (broadcastFPS_).
     * 
     * @note Tourne dans son propre thread (statusThread_)
     * @note S'arrête automatiquement quand running_ devient false
     * 
     * @see broadcastFPS_
     * @see statusThread_
     */
    void statusBroadcastLoop();
    
    /**
     * @brief Boucle de monitoring thermique
     * 
     * Thread qui surveille la température du CPU et alerte si nécessaire:
     * - Lit /sys/class/thermal/thermal_zone0/temp
     * - Log un warning si > 70°C
     * - Log une erreur critique si > 80°C
     * - Broadcast aux clients WebSocket pour affichage
     * - Peut déclencher throttling automatique si config activée
     * 
     * Vérifie toutes les 30 secondes (configurable).
     * 
     * @note Tourne dans son propre thread (thermalThread_)
     * @note S'arrête automatiquement quand running_ devient false
     * @note Peut être désactivé via config (thermal_monitoring_enabled)
     * 
     * @see thermalThread_
     */
    void thermalMonitoringLoop();
    
    // ========================================================================
    // MEMBRES PRIVÉS - COMPOSANTS PRINCIPAUX
    // ========================================================================
    
    /**
     * @brief Gestionnaire de périphériques MIDI (USB, WiFi, Bluetooth)
     * 
     * Responsable de:
     * - Détection des périphériques MIDI disponibles
     * - Connexion/déconnexion dynamique
     * - Gestion du hot-plug USB
     * - Support des périphériques réseau (RTP-MIDI, WiFi MIDI)
     * - Support des périphériques Bluetooth MIDI
     */
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    
    /**
     * @brief Routeur MIDI - gère les routes canal → device
     * 
     * Responsable de:
     * - Router les messages MIDI vers les bons périphériques
     * - Gérer les mappings canal → device
     * - Appliquer les transformations (transpose, velocity curve)
     * - Gérer mute/solo par canal
     * - Implémenter la stratégie de scheduling (FIFO, Priority Queue)
     */
    std::shared_ptr<MidiRouter> router_;
    
    /**
     * @brief Lecteur de fichiers MIDI
     * 
     * Responsable de:
     * - Charger et parser les fichiers MIDI
     * - Lire et envoyer les événements MIDI en temps réel
     * - Gérer play/pause/stop/seek
     * - Appliquer tempo et transposition globale
     * - Gérer mute/solo par piste
     */
    std::shared_ptr<MidiPlayer> player_;
    
    /**
     * @brief Gestionnaire de bibliothèque de fichiers MIDI et playlists
     * 
     * Responsable de:
     * - Scanner le système de fichiers pour trouver les .mid/.midi
     * - Indexer les métadonnées (titre, compositeur, durée, etc.)
     * - Stocker les infos dans SQLite
     * - Gérer les playlists
     * - Fournir des fonctions de recherche
     */
    std::shared_ptr<MidiFileManager> fileManager_;
    
    /**
     * @brief Serveur API WebSocket
     * 
     * Responsable de:
     * - Gérer les connexions WebSocket des clients
     * - Recevoir et router les commandes API
     * - Broadcaster les événements et le statut
     * - Gérer l'authentification (si activée)
     */
    std::shared_ptr<ApiServer> apiServer_;
    
    /**
     * @brief Processeur de commandes API (Command Pattern + Factory)
     * 
     * Responsable de:
     * - Parser les commandes JSON reçues via WebSocket
     * - Instancier les commandes appropriées via CommandFactory
     * - Exécuter les commandes
     * - Retourner les réponses JSON
     * - Gérer les erreurs et validations
     * 
     * @note Version 2 utilise le Command Pattern pour meilleure extensibilité
     */
    std::shared_ptr<CommandProcessorV2> commandProcessor_;
    
    /**
     * @brief Gestionnaire réseau (WiFi Hotspot, Bluetooth)
     * 
     * Responsable de:
     * - Configurer et démarrer le WiFi Hotspot (mode AP)
     * - Gérer le Bluetooth (découverte, pairing)
     * - Configurer DHCP (dnsmasq)
     * - Gérer le routage réseau
     */
    std::shared_ptr<NetworkManager> networkManager_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - THREADS
    // ========================================================================
    
    /**
     * @brief Thread de broadcast du statut
     * 
     * Broadcast périodique via WebSocket:
     * - État du player (position, durée, tempo, etc.)
     * - Niveau de chaque piste (VU-meter)
     * - Statistiques système (CPU, RAM, latence MIDI)
     * 
     * @see statusBroadcastLoop()
     */
    std::thread statusThread_;
    
    /**
     * @brief Thread de monitoring thermique
     * 
     * Surveillance continue de la température CPU pour éviter
     * la surchauffe et le throttling sur Raspberry Pi.
     * 
     * @see thermalMonitoringLoop()
     */
    std::thread thermalThread_;
    
	
	
	
	
	
	std::shared_ptr<PresetManager> presetManager_;
std::shared_ptr<FileManager> fileManager_;
    // ========================================================================
    // MEMBRES PRIVÉS - CONFIGURATION DYNAMIQUE
    // ========================================================================
    
    /**
     * @brief Fréquence de traitement du player (images/seconde)
     * 
     * Ajustée automatiquement selon le modèle de Raspberry Pi:
     * - Pi Zero/Zero W : 50 FPS (limite CPU)
     * - Pi 3B/3B+      : 100 FPS (équilibré)
     * - Pi 4/Pi 5      : 200 FPS (performance max)
     * 
     * Plus la fréquence est élevée, plus la précision temporelle est bonne,
     * mais plus la charge CPU est importante.
     * 
     * @see detectAndOptimizeForRaspberryPi()
     */
    int playerFPS_ = 100;
    
    /**
     * @brief Fréquence de broadcast WebSocket (images/seconde)
     * 
     * Ajustée automatiquement selon le modèle de Raspberry Pi:
     * - Pi Zero/Zero W : 5 FPS (économie bande passante)
     * - Pi 3B/3B+      : 10 FPS (équilibré)
     * - Pi 4/Pi 5      : 20 FPS (réactivité max)
     * 
     * Impact direct sur la fluidité de l'interface web et la charge réseau.
     * 
     * @see detectAndOptimizeForRaspberryPi()
     * @see statusBroadcastLoop()
     */
    int broadcastFPS_ = 10;
    
    // ========================================================================
    // MEMBRES PRIVÉS - ÉTAT
    // ========================================================================
    
    /**
     * @brief Flag indiquant si l'application est en cours d'exécution
     * 
     * Utilisé pour:
     * - Contrôler les boucles des threads de monitoring
     * - Éviter les double-stops
     * - Synchroniser l'arrêt entre les threads
     * 
     * @note Thread-safe via std::atomic
     * @note Positionné à true par run(), à false par stop()
     */
    std::atomic<bool> running_{false};
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Application.h
// ============================================================================

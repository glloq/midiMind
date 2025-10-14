// ============================================================================
// src/core/Application.h - VERSION COMPLÈTE CORRIGÉE v2.0
// ============================================================================
#pragma once

#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <unistd.h>  // Pour geteuid()

#include "Config.h"
#include "Logger.h"
#include "ErrorManager.h"
#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiFileManager.h"
#include "../api/ApiServer.h"
#include "../api/CommandProcessorV2.h"
#include "../network/NetworkManager.h"

namespace midiMind {

/**
 * @brief Classe principale de l'application midiMind
 * 
 * Gère l'initialisation, la configuration et le cycle de vie
 * de tous les composants du système MIDI.
 */
class Application {
public:
    /**
     * @brief Constructeur - Initialise tous les modules
     * @throws std::runtime_error si l'initialisation échoue
     */
    Application();
    
    /**
     * @brief Destructeur - Arrête proprement l'application
     */
    ~Application();
    
    // Désactiver copie et assignation
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    /**
     * @brief Lance l'application et entre dans la boucle principale
     * 
     * Cette méthode bloque jusqu'à ce que stop() soit appelé.
     * Configure le réseau, démarre tous les services, et gère
     * les threads de monitoring.
     */
    void run();
    
    /**
     * @brief Arrête proprement l'application
     * 
     * Arrête tous les threads, ferme les connexions,
     * et nettoie les ressources.
     */
    void stop();
    
    /**
     * @brief Vérifie si l'application est en cours d'exécution
     * @return true si l'application tourne
     */
    bool isRunning() const { return running_; }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Configure les callbacks entre les modules
     * 
     * Établit les connexions entre Device Manager, Router, Player, etc.
     */
    void setupCallbacks();
    
    /**
     * @brief Boucle de broadcast du statut du player
     * 
     * Envoie périodiquement le statut du player aux clients WebSocket.
     * Fréquence ajustée selon le modèle de Raspberry Pi détecté.
     */
    void statusBroadcastLoop();
    
    /**
     * @brief Détecte le modèle de Raspberry Pi et optimise les paramètres
     * 
     * Lit /proc/device-tree/model pour identifier le Pi et ajuste:
     * - Fréquence de traitement du player (playerFPS_)
     * - Fréquence de broadcast WebSocket (broadcastFPS_)
     * - Options de compilation CPU
     */
    void detectAndOptimizeForRaspberryPi();
    
    /**
     * @brief Boucle de monitoring thermique
     * 
     * Surveille la température du CPU via /sys/class/thermal/thermal_zone0/temp
     * et envoie des alertes si dépassement de seuils (70°C, 80°C).
     */
    void thermalMonitoringLoop();
    
    // ========================================================================
    // COMPOSANTS PRINCIPAUX
    // ========================================================================
    
    /// Gestionnaire de périphériques MIDI (USB, WiFi, Bluetooth)
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    
    /// Routeur MIDI - gère les routes canal → device
    std::shared_ptr<MidiRouter> router_;
    
    /// Lecteur de fichiers MIDI
    std::shared_ptr<MidiPlayer> player_;
    
    /// Gestionnaire de bibliothèque de fichiers MIDI et playlists
    std::shared_ptr<MidiFileManager> fileManager_;
    
    /// Serveur API WebSocket
    std::shared_ptr<ApiServer> apiServer_;
    
    /// Processeur de commandes API
    std::shared_ptr<CommandProcessor> commandProcessor_;
    
    /// Gestionnaire réseau (WiFi Hotspot, Bluetooth)
    std::shared_ptr<NetworkManager> networkManager_;
    
    // ========================================================================
    // THREADS
    // ========================================================================
    
    /// Thread de broadcast du statut
    std::thread statusThread_;
    
    /// Thread de monitoring thermique
    std::thread thermalThread_;
    
    // ========================================================================
    // CONFIGURATION DYNAMIQUE
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
    // ÉTAT
    // ========================================================================
    
    /// Flag indiquant si l'application est en cours d'exécution
    std::atomic<bool> running_{true};
};

} // namespace midiMind

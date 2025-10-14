// ============================================================================
// Fichier: backend/src/midi/devices/UsbMidiDevice.h
// Version: 1.0.0
// Projet: midiMind - Système d'Orchestration MIDI
// Description: Implémentation USB MIDI via ALSA sequencer
// ============================================================================

#pragma once

#include "MidiDevice.h"
#include "../../core/Logger.h"
#include <alsa/asoundlib.h>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace midiMind {

/**
 * @class UsbMidiDevice
 * @brief Périphérique MIDI USB utilisant ALSA sequencer
 * 
 * @details
 * Implémentation complète d'un device USB MIDI via ALSA (snd_seq).
 * 
 * Fonctionnalités:
 * - Connexion/déconnexion ALSA
 * - Envoi de messages MIDI
 * - Réception de messages MIDI (thread dédié)
 * - Reconnexion automatique en cas d'erreur
 * - Buffer de messages pour retry
 * - Gestion complète des erreurs ALSA
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe
 * 
 * @example Utilisation
 * ```cpp
 * auto device = std::make_shared<UsbMidiDevice>(
 *     "usb_128_0", "Arturia KeyStep", 128, 0
 * );
 * 
 * if (device->connect()) {
 *     // Envoyer un message
 *     auto msg = MidiMessage::noteOn(0, 60, 100);
 *     device->sendMessage(msg);
 *     
 *     // Recevoir des messages via callback
 *     device->setMessageCallback([](const MidiMessage& msg) {
 *         Logger::info("MIDI", "Received: " + msg.toString());
 *     });
 * }
 * ```
 */
class UsbMidiDevice : public MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param id ID unique du device (format: "usb_CLIENT_PORT")
     * @param name Nom du device
     * @param alsaClient Numéro de client ALSA
     * @param alsaPort Numéro de port ALSA
     */
    UsbMidiDevice(const std::string& id, 
                  const std::string& name,
                  int alsaClient, 
                  int alsaPort);
    
    /**
     * @brief Destructeur
     * 
     * Déconnecte proprement et arrête le thread de réception
     */
    ~UsbMidiDevice() override;
    
    // Désactiver copie
    UsbMidiDevice(const UsbMidiDevice&) = delete;
    UsbMidiDevice& operator=(const UsbMidiDevice&) = delete;
    
    // ========================================================================
    // MÉTHODES VIRTUELLES (MidiDevice)
    // ========================================================================
    
    /**
     * @brief Connecte le device USB via ALSA
     * 
     * Séquence:
     * 1. Ouvre le sequencer ALSA
     * 2. Crée un port d'entrée/sortie
     * 3. Connecte au device cible (alsaClient:alsaPort)
     * 4. Démarre le thread de réception
     * 
     * @return true Si connexion réussie
     * @return false Si erreur ALSA
     * 
     * @note Thread-safe
     */
    bool connect() override;
    
    /**
     * @brief Déconnecte le device
     * 
     * Séquence:
     * 1. Arrête le thread de réception
     * 2. Déconnecte les ports ALSA
     * 3. Ferme le sequencer
     * 
     * @note Thread-safe
     */
    void disconnect() override;
    
    /**
     * @brief Envoie un message MIDI
     * 
     * @param msg Message MIDI à envoyer
     * @return true Si envoi réussi
     * @return false Si erreur ou device déconnecté
     * 
     * @note Thread-safe
     * @note Buffer le message si device déconnecté (retry automatique)
     */
    bool sendMessage(const MidiMessage& msg) override;
    
    /**
     * @brief Vérifie si des messages sont disponibles
     * 
     * @return true Si queue de réception non vide
     * 
     * @note Thread-safe
     */
    bool hasMessages() const override;
    
    /**
     * @brief Récupère le prochain message reçu
     * 
     * @return MidiMessage Message reçu, ou message vide si queue vide
     * 
     * @note Thread-safe
     */
    MidiMessage receive() override;
    
    /**
     * @brief Récupère le port ALSA
     * 
     * @return string Format "CLIENT:PORT"
     */
    std::string getPort() const override;
    
    /**
     * @brief Récupère les informations du device
     * 
     * @return json Informations complètes (+ statistiques ALSA)
     */
    json getInfo() const override;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de réception de messages
     * 
     * @param callback Fonction appelée pour chaque message reçu
     * 
     * @note Thread-safe
     * @note Appelé depuis le thread de réception
     */
    void setMessageCallback(std::function<void(const MidiMessage&)> callback);
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Active/désactive la reconnexion automatique
     * 
     * @param enabled true pour activer
     */
    void setAutoReconnect(bool enabled);
    
    /**
     * @brief Définit le nombre maximum de tentatives de reconnexion
     * 
     * @param maxRetries Nombre max (défaut: 3)
     */
    void setMaxRetries(int maxRetries);
    
    /**
     * @brief Définit le délai entre les tentatives de reconnexion
     * 
     * @param delayMs Délai en millisecondes (défaut: 1000)
     */
    void setRetryDelay(int delayMs);

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Ouvre le sequencer ALSA
     * 
     * @return true Si succès
     */
    bool openSequencer();
    
    /**
     * @brief Ferme le sequencer ALSA
     */
    void closeSequencer();
    
    /**
     * @brief Crée les ports ALSA d'entrée/sortie
     * 
     * @return true Si succès
     */
    bool createPorts();
    
    /**
     * @brief Connecte aux ports du device cible
     * 
     * @return true Si succès
     */
    bool connectToPorts();
    
    /**
     * @brief Déconnecte des ports
     */
    void disconnectFromPorts();
    
    /**
     * @brief Thread de réception de messages
     * 
     * Boucle principale:
     * 1. Attend événements ALSA (poll)
     * 2. Lit les événements
     * 3. Convertit en MidiMessage
     * 4. Appelle callback ou met en queue
     */
    void receiveThreadFunc();
    
    /**
     * @brief Convertit un événement ALSA en MidiMessage
     * 
     * @param ev Événement ALSA
     * @return MidiMessage Message MIDI correspondant
     */
    MidiMessage alsaEventToMidiMessage(const snd_seq_event_t* ev);
    
    /**
     * @brief Convertit un MidiMessage en événement ALSA
     * 
     * @param msg Message MIDI
     * @param ev Événement ALSA (output)
     */
    void midiMessageToAlsaEvent(const MidiMessage& msg, snd_seq_event_t* ev);
    
    /**
     * @brief Tente une reconnexion automatique
     * 
     * @return true Si reconnexion réussie
     */
    bool attemptReconnect();
    
    /**
     * @brief Vide le buffer de messages en attente
     * 
     * Appelé après reconnexion réussie
     */
    void flushMessageBuffer();
    
    /**
     * @brief Vérifie si la connexion est toujours active
     * 
     * @return true Si device répond
     */
    bool validateConnection();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    // ALSA
    snd_seq_t* alsaSeq_;                  ///< Handle sequencer ALSA
    int alsaClient_;                      ///< Numéro de client ALSA du device cible
    int alsaPort_;                        ///< Numéro de port ALSA du device cible
    int myPort_;                          ///< Notre port ALSA créé
    snd_seq_addr_t destAddr_;             ///< Adresse destination ALSA
    
    // Thread de réception
    std::thread receiveThread_;           ///< Thread de réception
    std::atomic<bool> shouldStop_;        ///< Flag d'arrêt du thread
    
    // Queue de messages reçus
    std::queue<MidiMessage> receiveQueue_;  ///< Queue des messages reçus
    mutable std::mutex receiveMutex_;       ///< Mutex pour receiveQueue_
    std::condition_variable receiveCv_;     ///< CV pour notification messages
    
    // Buffer d'envoi (pour retry)
    std::queue<MidiMessage> sendBuffer_;    ///< Buffer messages à renvoyer
    mutable std::mutex sendMutex_;          ///< Mutex pour sendBuffer_
    static constexpr size_t MAX_BUFFER_SIZE = 1000;  ///< Taille max buffer
    
    // Callback
    std::function<void(const MidiMessage&)> messageCallback_;
    std::mutex callbackMutex_;              ///< Mutex pour callback
    
    // Reconnexion
    std::atomic<bool> autoReconnect_;       ///< Reconnexion automatique activée
    std::atomic<int> retryCount_;           ///< Compteur tentatives
    int maxRetries_;                        ///< Max tentatives
    int retryDelayMs_;                      ///< Délai entre tentatives
    std::atomic_flag reconnecting_;         ///< Flag reconnexion en cours
    
    // Statistiques
    std::atomic<uint64_t> alsaEventsReceived_;  ///< Événements ALSA reçus
    std::atomic<uint64_t> alsaEventsSent_;      ///< Événements ALSA envoyés
    std::atomic<uint64_t> alsaErrors_;          ///< Erreurs ALSA
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER UsbMidiDevice.h
// ============================================================================
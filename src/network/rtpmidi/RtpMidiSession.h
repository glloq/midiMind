// ============================================================================
// Fichier: src/network/rtpmidi/RtpMidiSession.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestion d'une session RTP-MIDI individuelle avec un client.
//   Maintient l'état de la connexion, gère la synchronisation temporelle,
//   et le recovery des paquets perdus.
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <asio.hpp>

#include "../../core/Logger.h"
#include "../../midi/MidiMessage.h"
#include "RtpPacket.h"

namespace midiMind {

/**
 * @enum SessionState
 * @brief État d'une session RTP-MIDI
 */
enum class SessionState {
    DISCONNECTED,       ///< Déconnecté
    CONNECTING,         ///< En cours de connexion
    CONNECTED,          ///< Connecté
    SYNCHRONIZING,      ///< Synchronisation en cours
    SYNCHRONIZED,       ///< Synchronisé
    CLOSING             ///< Fermeture en cours
};

/**
 * @class RtpMidiSession
 * @brief Session RTP-MIDI avec un client
 * 
 * @details
 * Gère une session RTP-MIDI complète:
 * - Handshake initial (invitation/acceptation)
 * - Synchronisation temporelle (clock sync)
 * - Transmission de données MIDI
 * - Recovery des paquets perdus
 * - Maintien de la connexion (keep-alive)
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 */
class RtpMidiSession : public std::enable_shared_from_this<RtpMidiSession> {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    using MidiReceivedCallback = std::function<void(const MidiMessage&)>;
    using StateChangedCallback = std::function<void(SessionState)>;
    
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param sessionId ID unique de la session
     * @param controlSocket Socket TCP de contrôle
     * @param dataSocket Socket UDP de données (partagé avec le serveur)
     * @param clientEndpoint Endpoint UDP du client
     */
    RtpMidiSession(const std::string& sessionId,
                   std::shared_ptr<asio::ip::tcp::socket> controlSocket,
                   std::shared_ptr<asio::ip::udp::socket> dataSocket,
                   const asio::ip::udp::endpoint& clientEndpoint);
    
    /**
     * @brief Destructeur
     */
    ~RtpMidiSession();
    
    // ========================================================================
    // CONTRÔLE DE LA SESSION
    // ========================================================================
    
    /**
     * @brief Démarre la session
     * 
     * Effectue le handshake et démarre la synchronisation.
     * 
     * @return true Si le démarrage a réussi
     */
    bool start();
    
    /**
     * @brief Ferme la session proprement
     */
    void close();
    
    /**
     * @brief Vérifie si la session est active
     */
    bool isActive() const;
    
    /**
     * @brief Récupère l'état actuel
     */
    SessionState getState() const;
    
    // ========================================================================
    // ENVOI/RÉCEPTION MIDI
    // ========================================================================
    
    /**
     * @brief Envoie un message MIDI
     * 
     * @param message Message à envoyer
     * @return true Si l'envoi a réussi
     */
    bool sendMidi(const MidiMessage& message);
    
    /**
     * @brief Définit le callback de réception MIDI
     */
    void setOnMidiReceived(MidiReceivedCallback callback);
    
    /**
     * @brief Définit le callback de changement d'état
     */
    void setOnStateChanged(StateChangedCallback callback);
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère l'ID de la session
     */
    std::string getId() const { return sessionId_; }
    
    /**
     * @brief Récupère le nom du client
     */
    std::string getClientName() const { return clientName_; }
    
    /**
     * @brief Récupère l'adresse du client
     */
    std::string getClientAddress() const {
        return clientEndpoint_.address().to_string();
    }
    
    /**
     * @brief Récupère les statistiques de la session
     */
    json getStatistics() const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread de lecture du socket de contrôle
     */
    void controlReadLoop();
    
    /**
     * @brief Thread de réception des paquets UDP
     */
    void dataReadLoop();
    
    /**
     * @brief Gère un paquet de contrôle reçu
     */
    void handleControlPacket(const ControlPacket& packet, const std::string& deviceName);
    
    /**
     * @brief Gère un paquet de données reçu
     */
    void handleDataPacket(const uint8_t* data, size_t size);
    
    /**
     * @brief Envoie un paquet de contrôle
     */
    bool sendControlPacket(uint16_t command, const std::string& name = "");
    
    /**
     * @brief Thread de synchronisation temporelle
     */
    void syncLoop();
    
    /**
     * @brief Effectue une synchronisation
     */
    void performSync();
    
    /**
     * @brief Change l'état de la session
     */
    void setState(SessionState newState);
    
    /**
     * @brief Récupère le timestamp actuel en microsecondes
     */
    uint64_t getCurrentTimestamp() const;
    
    /**
     * @brief Génère un SSRC aléatoire
     */
    static uint32_t generateSSRC();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Identifiant de la session
    std::string sessionId_;
    
    /// État de la session
    std::atomic<SessionState> state_;
    
    /// Sockets
    std::shared_ptr<asio::ip::tcp::socket> controlSocket_;
    std::shared_ptr<asio::ip::udp::socket> dataSocket_;
    asio::ip::udp::endpoint clientEndpoint_;
    
    /// SSRC (Synchronization Source)
    uint32_t ssrc_;
    uint32_t clientSSRC_;
    
    /// Token d'initiateur
    uint32_t initiatorToken_;
    
    /// Nom du client
    std::string clientName_;
    
    /// Threads
    std::thread controlThread_;
    std::thread syncThread_;
    
    /// Flag d'arrêt
    std::atomic<bool> running_;
    
    /// Constructeur de paquets
    std::unique_ptr<RtpPacketBuilder> packetBuilder_;
    
    /// Séquence numbers
    uint16_t lastReceivedSeq_;
    
    /// Callbacks
    MidiReceivedCallback onMidiReceived_;
    StateChangedCallback onStateChanged_;
    
    /// Statistiques
    std::atomic<uint64_t> packetsReceived_;
    std::atomic<uint64_t> packetsSent_;
    std::atomic<uint64_t> bytesReceived_;
    std::atomic<uint64_t> bytesSent_;
    std::atomic<uint32_t> packetsLost_;
    
    /// Timestamps de synchronisation
    uint64_t lastSyncTimestamp_;
    int64_t clockOffset_;
    bool synchronized_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER RtpMidiSession.h
// ============================================================================
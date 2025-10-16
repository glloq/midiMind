// ============================================================================
// Fichier: src/network/rtpmidi/RtpMidiSession.h
// Version: 3.0.1 - CORRECTION INCLUDES ASIO
// Date: 2025-10-16
// ============================================================================
// CORRECTIONS v3.0.1:
//   ✅ Ajout de tous les includes STL nécessaires AVANT asio.hpp
//   ✅ Ordre correct des includes
//   ✅ Defines ASIO standalone
// ============================================================================

#pragma once

// Includes STL AVANT asio.hpp
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>

// Defines ASIO standalone
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#ifndef ASIO_NO_DEPRECATED
#define ASIO_NO_DEPRECATED
#endif

// ASIO avec tous les types disponibles
#include <asio.hpp>

// Includes projet
#include "../../core/Logger.h"
#include "../../midi/MidiMessage.h"
#include "RtpPacket.h"

// Forward declare json
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace midiMind {

// Forward declaration
class RtpPacketBuilder;

/**
 * @enum SessionState
 * @brief États d'une session RTP-MIDI
 */
enum class SessionState {
    DISCONNECTED = 0,    ///< Pas de connexion
    CONNECTING = 1,      ///< Connexion en cours
    CONNECTED = 2,       ///< Connexion établie
    SYNCHRONIZED = 3,    ///< Synchronisation OK
    ERROR_STATE = 4      ///< Erreur
};

/**
 * @class RtpMidiSession
 * @brief Session RTP-MIDI individuelle avec un client
 * 
 * @details
 * Gère une session RTP-MIDI complète avec un client connecté.
 * Inclut:
 * - Handshake de connexion
 * - Synchronisation d'horloge
 * - Transmission bidirectionnelle de MIDI
 * - Gestion des paquets perdus
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 */
class RtpMidiSession {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback pour messages MIDI reçus
     */
    using MidiReceivedCallback = std::function<void(const MidiMessage&)>;
    
    /**
     * @brief Callback pour changement d'état
     */
    using StateChangedCallback = std::function<void(SessionState)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param sessionId ID unique de la session
     * @param controlSocket Socket TCP de contrôle
     * @param dataSocket Socket UDP de données
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
    
    // Interdire copie et déplacement
    RtpMidiSession(const RtpMidiSession&) = delete;
    RtpMidiSession& operator=(const RtpMidiSession&) = delete;
    RtpMidiSession(RtpMidiSession&&) = delete;
    RtpMidiSession& operator=(RtpMidiSession&&) = delete;
    
    // ========================================================================
    // CONTRÔLE DE SESSION
    // ========================================================================
    
    /**
     * @brief Démarre la session
     * 
     * @return true si démarrage réussi
     */
    bool start();
    
    /**
     * @brief Ferme la session
     */
    void close();
    
    /**
     * @brief Vérifie si la session est active
     * 
     * @return true si session active (connectée ou synchronisée)
     */
    bool isActive() const;
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Envoie un message MIDI
     * 
     * @param message Le message MIDI à envoyer
     * @return true si envoyé avec succès
     */
    bool sendMidi(const MidiMessage& message);
    
    // ========================================================================
    // ACCESSEURS
    // ========================================================================
    
    /**
     * @brief Récupère l'ID de la session
     */
    std::string getId() const { return sessionId_; }
    
    /**
     * @brief Récupère l'état actuel
     */
    SessionState getState() const { return state_.load(); }
    
    /**
     * @brief Récupère le nom du client
     */
    std::string getClientName() const;
    
    /**
     * @brief Récupère l'adresse IP du client
     */
    std::string getClientAddress() const {
        return clientEndpoint_.address().to_string();
    }
    
    /**
     * @brief Récupère les statistiques de la session
     * 
     * @return JSON contenant les statistiques
     */
    json getStatistics() const;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback pour messages MIDI reçus
     */
    void setOnMidiReceived(MidiReceivedCallback callback);
    
    /**
     * @brief Définit le callback pour changement d'état
     */
    void setOnStateChanged(StateChangedCallback callback);
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread de réception sur le control socket
     */
    void controlLoop();
    
    /**
     * @brief Thread de synchronisation d'horloge
     */
    void syncLoop();
    
    /**
     * @brief Traite un message de contrôle
     */
    void handleControlMessage(const std::vector<uint8_t>& data);
    
    /**
     * @brief Traite un paquet RTP de données
     */
    void handleDataPacket(const std::vector<uint8_t>& data);
    
    /**
     * @brief Envoie un paquet de synchronisation
     */
    void sendSyncPacket();
    
    /**
     * @brief Change l'état de la session
     */
    void setState(SessionState newState);
    
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
// FIN DU FICHIER RtpMidiSession.h v3.0.1
// ============================================================================

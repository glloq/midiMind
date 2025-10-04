// ============================================================================
// Fichier: src/network/rtpmidi/RtpMidiServer.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Serveur RTP-MIDI (Apple MIDI Network Protocol - RFC 6295).
//   Permet de recevoir et envoyer des messages MIDI via le réseau IP.
//   Supporte la découverte automatique via mDNS.
//
// Responsabilités:
//   - Écouter les connexions RTP-MIDI entrantes
//   - Gérer les sessions RTP-MIDI
//   - Encoder/décoder les paquets RTP-MIDI
//   - Gérer le timing et la synchronisation
//   - Recovery des paquets perdus
//
// Protocole RTP-MIDI:
//   - Control Port: 5004 (TCP pour handshake)
//   - Data Port: 5005 (UDP pour données MIDI)
//   - Format: RTP header + MIDI payload
//
// Thread-safety: OUI - I/O asio sur threads séparés
//
// Patterns: Observer Pattern pour notifications
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
#include <functional>
#include <asio.hpp>

#include "../../core/Logger.h"
#include "../../midi/MidiMessage.h"
#include "RtpMidiSession.h"

namespace midiMind {

/**
 * @class RtpMidiServer
 * @brief Serveur RTP-MIDI (RFC 6295)
 * 
 * @details
 * Implémente le protocole Apple MIDI Network (RTP-MIDI).
 * Permet à des clients (Logic Pro, Ableton, etc.) de se connecter
 * via le réseau IP et d'échanger des messages MIDI.
 * 
 * Architecture:
 * - Control Port (TCP 5004): Handshake et contrôle de session
 * - Data Port (UDP 5005): Transmission des données MIDI
 * 
 * Thread-safety: Les I/O sont gérés par asio sur des threads séparés.
 * 
 * @example Utilisation
 * ```cpp
 * RtpMidiServer server;
 * 
 * // Callback pour messages MIDI reçus
 * server.setOnMidiReceived([](const MidiMessage& msg, const std::string& sessionId) {
 *     Logger::info("RTP", "Received MIDI from " + sessionId);
 * });
 * 
 * // Démarrer le serveur
 * server.start(5004, "MidiMind");
 * ```
 */
class RtpMidiServer {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors de la réception d'un message MIDI
     * @param message Message MIDI reçu
     * @param sessionId ID de la session RTP-MIDI
     */
    using MidiReceivedCallback = std::function<void(const MidiMessage&, const std::string&)>;
    
    /**
     * @brief Callback appelé lors de la connexion d'un client
     * @param sessionId ID de la session
     * @param clientName Nom du client
     */
    using ClientConnectedCallback = std::function<void(const std::string&, const std::string&)>;
    
    /**
     * @brief Callback appelé lors de la déconnexion d'un client
     * @param sessionId ID de la session
     */
    using ClientDisconnectedCallback = std::function<void(const std::string&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    RtpMidiServer();
    
    /**
     * @brief Destructeur
     * 
     * @note Arrête le serveur proprement
     */
    ~RtpMidiServer();
    
    // Désactiver copie
    RtpMidiServer(const RtpMidiServer&) = delete;
    RtpMidiServer& operator=(const RtpMidiServer&) = delete;
    
    // ========================================================================
    // CONTRÔLE DU SERVEUR
    // ========================================================================
    
    /**
     * @brief Démarre le serveur RTP-MIDI
     * 
     * @param controlPort Port de contrôle (défaut: 5004)
     * @param serviceName Nom du service mDNS
     * @return true Si le serveur a démarré
     * 
     * @note Thread-safe
     */
    bool start(uint16_t controlPort = 5004, const std::string& serviceName = "MidiMind");
    
    /**
     * @brief Arrête le serveur
     * 
     * @note Thread-safe. Ferme toutes les sessions actives.
     */
    void stop();
    
    /**
     * @brief Vérifie si le serveur est actif
     * 
     * @return true Si le serveur est démarré
     * 
     * @note Thread-safe
     */
    bool isRunning() const;
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Envoie un message MIDI à tous les clients connectés
     * 
     * @param message Message MIDI à envoyer
     * 
     * @note Thread-safe
     */
    void sendToAll(const MidiMessage& message);
    
    /**
     * @brief Envoie un message MIDI à une session spécifique
     * 
     * @param message Message MIDI à envoyer
     * @param sessionId ID de la session cible
     * @return true Si l'envoi a réussi
     * 
     * @note Thread-safe
     */
    bool sendToSession(const MidiMessage& message, const std::string& sessionId);
    
    // ========================================================================
    // GESTION DES SESSIONS
    // ========================================================================
    
    /**
     * @brief Liste toutes les sessions actives
     * 
     * @return std::vector<std::string> IDs des sessions
     * 
     * @note Thread-safe
     */
    std::vector<std::string> listSessions() const;
    
    /**
     * @brief Récupère les informations d'une session
     * 
     * @param sessionId ID de la session
     * @return json Informations de la session ou objet vide
     * 
     * @note Thread-safe
     */
    json getSessionInfo(const std::string& sessionId) const;
    
    /**
     * @brief Ferme une session
     * 
     * @param sessionId ID de la session à fermer
     * @return true Si la fermeture a réussi
     * 
     * @note Thread-safe
     */
    bool closeSession(const std::string& sessionId);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de réception MIDI
     */
    void setOnMidiReceived(MidiReceivedCallback callback);
    
    /**
     * @brief Définit le callback de connexion client
     */
    void setOnClientConnected(ClientConnectedCallback callback);
    
    /**
     * @brief Définit le callback de déconnexion client
     */
    void setOnClientDisconnected(ClientDisconnectedCallback callback);
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques du serveur
     * 
     * @return json Statistiques
     * 
     * Format:
     * ```json
     * {
     *   "active_sessions": 2,
     *   "packets_received": 15420,
     *   "packets_sent": 8935,
     *   "bytes_received": 185040,
     *   "bytes_sent": 107220,
     *   "packet_loss_rate": 0.02
     * }
     * ```
     */
    json getStatistics() const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread d'acceptation des connexions
     */
    void acceptLoop();
    
    /**
     * @brief Gère une nouvelle connexion
     */
    void handleNewConnection(std::shared_ptr<asio::ip::tcp::socket> socket);
    
    /**
     * @brief Thread de réception des paquets UDP
     */
    void receiveLoop();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// ASIO context
    asio::io_context ioContext_;
    
    /// Accepteur TCP (control port)
    std::unique_ptr<asio::ip::tcp::acceptor> controlAcceptor_;
    
    /// Socket UDP (data port)
    std::unique_ptr<asio::ip::udp::socket> dataSocket_;
    
    /// Sessions actives
    std::vector<std::shared_ptr<RtpMidiSession>> sessions_;
    
    /// Threads I/O
    std::vector<std::thread> ioThreads_;
    
    /// État du serveur
    std::atomic<bool> running_;
    
    /// Configuration
    uint16_t controlPort_;
    uint16_t dataPort_;
    std::string serviceName_;
    
    /// Callbacks
    MidiReceivedCallback onMidiReceived_;
    ClientConnectedCallback onClientConnected_;
    ClientDisconnectedCallback onClientDisconnected_;
    
    /// Statistiques
    std::atomic<uint64_t> packetsReceived_;
    std::atomic<uint64_t> packetsSent_;
    std::atomic<uint64_t> bytesReceived_;
    std::atomic<uint64_t> bytesSent_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER RtpMidiServer.h
// ============================================================================
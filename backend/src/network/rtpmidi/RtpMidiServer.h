// ============================================================================
// Fichier: src/network/rtpmidi/RtpMidiServer.h
// Version: 3.0.1 - CORRECTION INCLUDES ASIO
// Date: 2025-10-16
// ============================================================================
// CORRECTIONS v3.0.1:
//   ✅ Ajout de tous les includes STL nécessaires AVANT asio.hpp
//   ✅ Ordre correct des includes pour éviter les erreurs de compilation
//   ✅ Ajout de <atomic>, <thread>, <chrono>, <cstdint>
//   ✅ Defines ASIO pour standalone mode
// ============================================================================

#pragma once

// IMPORTANT: Includes STL AVANT asio.hpp pour éviter les erreurs de compilation
#include <memory>           // Pour std::shared_ptr, std::unique_ptr
#include <string>           // Pour std::string
#include <vector>           // Pour std::vector
#include <mutex>            // Pour std::mutex
#include <functional>       // Pour std::function
#include <atomic>           // Pour std::atomic
#include <thread>           // Pour std::thread
#include <chrono>           // Pour std::chrono
#include <cstdint>          // Pour uint16_t, uint32_t, uint64_t
#include <algorithm>        // Pour std::find_if

// Defines pour ASIO standalone (sans Boost)
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#ifndef ASIO_NO_DEPRECATED
#define ASIO_NO_DEPRECATED
#endif

// Maintenant ASIO avec tous les types nécessaires disponibles
#include <asio.hpp>

// Includes projet
#include "../../core/Logger.h"
#include "../../midi/MidiMessage.h"
#include "RtpMidiSession.h"

// Forward declare json (nlohmann)
#include <nlohmann/json.hpp>
using json = nlohmann::json;

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
 * @example Utilisation simple
 * ```cpp
 * RtpMidiServer server;
 * 
 * // Définir les callbacks
 * server.setOnMidiReceived([](const MidiMessage& msg, const std::string& sessionId) {
 *     std::cout << "MIDI received: " << msg.toString() << std::endl;
 * });
 * 
 * // Démarrer le serveur
 * if (server.start(5004, "MidiMind RTP")) {
 *     std::cout << "Server started on port 5004" << std::endl;
 * }
 * 
 * // Envoyer un message à tous les clients
 * MidiMessage noteOn(0x90, 60, 100);
 * server.sendToAll(noteOn);
 * 
 * // Arrêter
 * server.stop();
 * ```
 */
class RtpMidiServer {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback pour messages MIDI reçus
     * 
     * @param message Le message MIDI reçu
     * @param sessionId ID de la session qui a envoyé le message
     */
    using MidiReceivedCallback = std::function<void(const MidiMessage&, const std::string&)>;
    
    /**
     * @brief Callback pour connexion de client
     * 
     * @param sessionId ID de la nouvelle session
     */
    using ClientConnectedCallback = std::function<void(const std::string&)>;
    
    /**
     * @brief Callback pour déconnexion de client
     * 
     * @param sessionId ID de la session déconnectée
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
     * Arrête automatiquement le serveur si en cours d'exécution
     */
    ~RtpMidiServer();
    
    // Interdire copie et déplacement
    RtpMidiServer(const RtpMidiServer&) = delete;
    RtpMidiServer& operator=(const RtpMidiServer&) = delete;
    RtpMidiServer(RtpMidiServer&&) = delete;
    RtpMidiServer& operator=(RtpMidiServer&&) = delete;
    
    // ========================================================================
    // CONTRÔLE DU SERVEUR
    // ========================================================================
    
    /**
     * @brief Démarre le serveur RTP-MIDI
     * 
     * @param controlPort Port TCP pour les connexions de contrôle (défaut: 5004)
     * @param serviceName Nom du service (visible dans les clients MIDI)
     * @return true si démarrage réussi, false sinon
     * 
     * @note Le data port (UDP) sera automatiquement controlPort + 1
     * 
     * @example
     * ```cpp
     * if (!server.start(5004, "Mon Raspberry Pi")) {
     *     std::cerr << "Échec du démarrage" << std::endl;
     * }
     * ```
     */
    bool start(uint16_t controlPort = 5004, const std::string& serviceName = "MidiMind RTP");
    
    /**
     * @brief Arrête le serveur RTP-MIDI
     * 
     * Ferme toutes les sessions actives et libère les ressources
     */
    void stop();
    
    /**
     * @brief Vérifie si le serveur est en cours d'exécution
     * 
     * @return true si le serveur est actif
     */
    bool isRunning() const;
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Envoie un message MIDI à tous les clients connectés
     * 
     * @param message Le message MIDI à envoyer
     * 
     * @example
     * ```cpp
     * MidiMessage noteOn(0x90, 60, 100); // Note On, C4, vélocité 100
     * server.sendToAll(noteOn);
     * ```
     */
    void sendToAll(const MidiMessage& message);
    
    /**
     * @brief Envoie un message MIDI à une session spécifique
     * 
     * @param message Le message MIDI à envoyer
     * @param sessionId ID de la session cible
     * @return true si envoyé avec succès, false si session inexistante
     */
    bool sendToSession(const MidiMessage& message, const std::string& sessionId);
    
    // ========================================================================
    // GESTION DES SESSIONS
    // ========================================================================
    
    /**
     * @brief Liste toutes les sessions actives
     * 
     * @return Vector des IDs de sessions
     */
    std::vector<std::string> listSessions() const;
    
    /**
     * @brief Récupère les informations d'une session
     * 
     * @param sessionId ID de la session
     * @return JSON contenant les infos de session (nom client, stats, etc.)
     */
    json getSessionInfo(const std::string& sessionId) const;
    
    /**
     * @brief Ferme une session spécifique
     * 
     * @param sessionId ID de la session à fermer
     * @return true si fermée avec succès
     */
    bool closeSession(const std::string& sessionId);
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques du serveur
     * 
     * @return JSON contenant:
     *   - activeSessions: nombre de sessions actives
     *   - packetsReceived: nombre total de paquets reçus
     *   - packetsSent: nombre total de paquets envoyés
     *   - bytesReceived: nombre total d'octets reçus
     *   - bytesSent: nombre total d'octets envoyés
     *   - packetsLost: nombre de paquets perdus détectés
     *   - uptime: temps depuis le démarrage (secondes)
     */
    json getStatistics() const;
    
    /**
     * @brief Réinitialise les compteurs de statistiques
     */
    void resetStatistics();
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback pour messages MIDI reçus
     * 
     * @param callback Fonction appelée lors de la réception d'un message
     */
    void setOnMidiReceived(MidiReceivedCallback callback);
    
    /**
     * @brief Définit le callback pour nouvelle connexion client
     * 
     * @param callback Fonction appelée lors d'une nouvelle connexion
     */
    void setOnClientConnected(ClientConnectedCallback callback);
    
    /**
     * @brief Définit le callback pour déconnexion client
     * 
     * @param callback Fonction appelée lors d'une déconnexion
     */
    void setOnClientDisconnected(ClientDisconnectedCallback callback);
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread d'acceptation des connexions TCP
     */
    void acceptLoop();
    
    /**
     * @brief Gère une nouvelle connexion
     * 
     * @param socket Socket TCP de la nouvelle connexion
     */
    void handleNewConnection(std::shared_ptr<asio::ip::tcp::socket> socket);
    
    /**
     * @brief Thread de réception des paquets UDP
     */
    void receiveLoop();
    
    /**
     * @brief Traite un paquet RTP entrant
     * 
     * @param data Buffer contenant le paquet RTP
     * @param length Taille du buffer
     * @param sessionId ID de la session source
     */
    void processIncomingPacket(const uint8_t* data, 
                              size_t length, 
                              const std::string& sessionId);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// ASIO context pour I/O asynchrone
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
    std::atomic<uint64_t> packetsLost_;
    std::atomic<uint64_t> expectedSequenceNumber_;
    
    /// Mutex pour les sequence numbers
    mutable std::mutex sequenceMutex_;
    
    /// Timestamp de démarrage
    std::chrono::steady_clock::time_point startTime_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER RtpMidiServer.h v3.0.1
// ============================================================================

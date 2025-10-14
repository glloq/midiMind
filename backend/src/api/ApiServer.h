// ============================================================================
// Fichier: backend/src/api/ApiServer.h
// Version: 3.0.0-refonte
// Date: 2025-10-09
// ============================================================================
// Description:
//   Serveur WebSocket avec protocole unifié.
//   Support complet des enveloppes, requestId, et gestion asynchrone.
// ============================================================================

#pragma once

#include "MessageEnvelope.h"
#include "../midi/sysex/SysExHandler.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @brief Serveur WebSocket pour communication frontend/backend
 * 
 * Gère la communication bidirectionnelle avec le frontend via WebSocket.
 * Utilise le protocole unifié défini dans Protocol.h.
 * 
 * Features:
 * - Support protocole v3.0 avec enveloppes
 * - Gestion requestId pour promesses asynchrones
 * - Broadcast d'événements (MIDI, système, instruments)
 * - Heartbeat automatique
 * - Reconnexion automatique clients
 * - Statistiques temps réel
 * 
 * @note Thread-safe : toutes les méthodes publiques peuvent être appelées
 *       depuis n'importe quel thread.
 * 
 * @example Usage basique
 * ```cpp
 * ApiServer server;
 * server.setCommandCallback([](const json& cmd) {
 *     // Traiter commande
 *     return result;
 * });
 * server.start(8080);
 * 
 * // Broadcast événement
 * auto event = MessageEnvelope::createEvent(
 *     "midi:message", midiData, protocol::EventPriority::HIGH
 * );
 * server.broadcast(event);
 * ```
 */
class ApiServer {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    using WebSocketServer = websocketpp::server<websocketpp::config::asio>;
    using connection_hdl = websocketpp::connection_hdl;
    using message_ptr = WebSocketServer::message_ptr;
    
    /**
     * @brief Callback pour traiter les commandes
     * 
     * Reçoit une commande au format legacy et retourne un résultat JSON.
     * Le serveur se charge de l'envelopper dans le nouveau protocole.
     * 
     * @param command Commande au format {"command": "...", "params": {...}}
     * @return Résultat au format {"success": bool, "data": {...}}
     */
    using CommandCallback = std::function<json(const json&)>;
    
    /**
     * @brief Statistiques du serveur
     */
    struct Stats {
        std::chrono::steady_clock::time_point startTime;
        size_t activeConnections;
        size_t totalConnections;
        size_t messagesSent;
        size_t messagesReceived;
        size_t errorCount;
    };
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    ApiServer();
    
    /**
     * @brief Destructeur
     */
    ~ApiServer();
    
    // Interdire copie et assignation
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;
    
    // ========================================================================
    // GESTION SERVEUR
    // ========================================================================
    
    /**
     * @brief Démarre le serveur WebSocket
     * 
     * @param port Port d'écoute (défaut: 8080)
     * 
     * @throws std::runtime_error Si le serveur ne peut pas démarrer
     * 
     * @note Thread-safe
     * @note Bloquant jusqu'à ce que le serveur soit prêt (100ms max)
     */
    void start(int port = 8080);
    
    /**
     * @brief Arrête le serveur WebSocket
     * 
     * Ferme toutes les connexions proprement et arrête le thread serveur.
     * 
     * @note Thread-safe
     * @note Bloquant jusqu'à arrêt complet
     */
    void stop();
    
    /**
     * @brief Vérifie si le serveur est en cours d'exécution
     * 
     * @return true si le serveur tourne, false sinon
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Récupère le nombre de clients connectés
     * 
     * @return Nombre de connexions actives
     * 
     * @note Thread-safe
     */
    size_t getClientCount() const;
    
    /**
     * @brief Récupère les statistiques du serveur
     * 
     * @return Structure Stats
     * 
     * @note Thread-safe
     */
    Stats getStats() const;
    
    // ========================================================================
    // COMMUNICATION
    // ========================================================================
    
    /**
     * @brief Envoie un message à un client spécifique
     * 
     * @param hdl Handle de connexion
     * @param message Enveloppe à envoyer
     * 
     * @note Thread-safe
     */
    void sendTo(connection_hdl hdl, const MessageEnvelope& message);
    
    /**
     * @brief Broadcast un message à tous les clients
     * 
     * @param message Enveloppe à broadcaster
     * 
     * @note Thread-safe
     * @note Utilisé pour événements (MIDI, système, etc.)
     * 
     * @example
     * ```cpp
     * auto event = MessageEnvelope::createEvent(
     *     "playback:state", 
     *     {{"playing", true}, {"position", 1.5}},
     *     protocol::EventPriority::NORMAL
     * );
     * server.broadcast(event);
     * ```
     */
    void broadcast(const MessageEnvelope& message);
    
    /**
     * @brief Broadcast au format legacy (rétrocompatibilité)
     * 
     * @param message JSON brut (sera enveloppé automatiquement)
     * 
     * @deprecated Utiliser broadcast(MessageEnvelope) à la place
     */
    void broadcast(const json& message);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Enregistre le callback de traitement des commandes
     * 
     * @param callback Fonction de traitement
     * 
     * @note Doit être appelé AVANT start()
     * @note Thread-safe
     */
    void setCommandCallback(CommandCallback callback);
    
    /**
     * @brief Configure le handler SysEx (pour événements instruments)
     * 
     * @param handler Handler SysEx partagé
     * 
     * @note Optionnel - utilisé pour broadcast auto des messages SysEx
     */
    void setSysExHandler(std::shared_ptr<SysExHandler> handler);
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Nettoie les connexions mortes
     * 
     * Parcourt toutes les connexions et supprime celles qui sont fermées.
     * 
     * @note Thread-safe
     * @note Appelé automatiquement périodiquement
     */
    void cleanupDisconnectedClients();
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES - WEBSOCKET HANDLERS
    // ========================================================================
    
    /**
     * @brief Thread principal du serveur
     */
    void serverThread();
    
    /**
     * @brief Handler: nouvelle connexion
     */
    void onOpen(connection_hdl hdl);
    
    /**
     * @brief Handler: connexion fermée
     */
    void onClose(connection_hdl hdl);
    
    /**
     * @brief Handler: message reçu
     */
    void onMessage(connection_hdl hdl, message_ptr msg);
    
    /**
     * @brief Handler: erreur WebSocket
     */
    void onError(connection_hdl hdl, const std::error_code& ec);
    
    // ========================================================================
    // MÉTHODES PRIVÉES - TRAITEMENT MESSAGES
    // ========================================================================
    
    /**
     * @brief Traite une requête (type REQUEST)
     * 
     * @param hdl Handle de connexion
     * @param envelope Enveloppe de la requête
     */
    void handleRequest(connection_hdl hdl, const MessageEnvelope& envelope);
    
    /**
     * @brief Traite un événement client (type EVENT)
     * 
     * @param hdl Handle de connexion
     * @param envelope Enveloppe de l'événement
     * 
     * @note Les clients peuvent envoyer des événements (ex: ping)
     */
    void handleClientEvent(connection_hdl hdl, const MessageEnvelope& envelope);
    
    /**
     * @brief Envoi sécurisé (avec vérification état connexion)
     * 
     * @param hdl Handle de connexion
     * @param message String JSON à envoyer
     * @return true si envoyé, false sinon
     */
    bool safeSend(connection_hdl hdl, const std::string& message);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    // Serveur WebSocket
    WebSocketServer server_;
    std::thread serverThread_;
    std::atomic<bool> running_;
    int port_;
    
    // Connexions
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections_;
    mutable std::mutex connectionsMutex_;
    
    // Callback commandes
    CommandCallback commandCallback_;
    std::mutex callbackMutex_;
    
    // SysEx handler (optionnel)
    std::shared_ptr<SysExHandler> sysexHandler_;
    
    // Statistiques
    Stats stats_;
    mutable std::mutex statsMutex_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ApiServer.h
// ============================================================================

// ============================================================================
// Fichier: backend/src/api/ApiServer.h
// Version: 3.1.0-corrections
// Date: 2025-10-15
// ============================================================================
// Description:
//   Header du serveur WebSocket avec protocole unifié - CORRECTIONS
//
// CORRECTIONS v3.1.0:
//   ✅ Ajout méthode sendError() pour helper d'envoi d'erreurs
//   ✅ Ajout méthode onFail() pour gérer les échecs de connexion
//   ✅ Documentation améliorée
//   ✅ Stats accessibles publiquement
//
// Modifications par rapport à v3.0.0:
//   - Nouvelle méthode publique: sendError()
//   - Nouveau handler privé: onFail()
//   - Méthode publique: getStats()
// ============================================================================

#pragma once

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <set>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

#include "Protocol.h"

namespace midiMind {

// Forward declarations
class MessageEnvelope;

using json = nlohmann::json;
using websocketpp::connection_hdl;

/**
 * @class ApiServer
 * @brief Serveur WebSocket pour l'API MidiMind
 * 
 * Gère les connexions WebSocket, parse les messages entrants au format
 * MessageEnvelope, route vers CommandProcessor, et envoie les réponses.
 * 
 * Architecture:
 * - Thread principal: Serveur WebSocket (listen + accept)
 * - Callbacks: onOpen, onClose, onMessage, onFail
 * - CommandCallback: Injection du processeur de commandes
 * - Thread-safe: Mutex pour connections et stats
 * 
 * Flux de message:
 * 1. Frontend envoie REQUEST via WebSocket
 * 2. onMessage() parse MessageEnvelope
 * 3. Appel commandCallback_ avec command JSON
 * 4. Création RESPONSE avec même requestId
 * 5. Envoi réponse au client
 * 
 * @example Utilisation
 * ```cpp
 * ApiServer server;
 * 
 * // Enregistrer le callback
 * server.setCommandCallback([processor](const json& cmd) {
 *     return processor->processCommand(cmd);
 * });
 * 
 * // Démarrer
 * server.start(8080);
 * 
 * // Broadcast event
 * auto event = MessageEnvelope::createEvent("midi.message", data, HIGH);
 * server.broadcast(event);
 * 
 * // Arrêter
 * server.stop();
 * ```
 */
class ApiServer {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    using server_t = websocketpp::server<websocketpp::config::asio>;
    using message_ptr = server_t::message_ptr;
    
    /**
     * @brief Callback pour traitement de commandes
     * 
     * Prend un objet JSON {"command": "...", "params": {...}}
     * Retourne un objet JSON {"success": bool, "data": {...}}
     * 
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
     * 
     * Initialise le serveur WebSocket et configure les handlers.
     */
    ApiServer();
    
    /**
     * @brief Destructeur
     * 
     * Arrête le serveur proprement si encore en cours d'exécution.
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
     * @note Peut prendre quelques secondes
     */
    void stop();
    
    /**
     * @brief Retourne le nombre de clients connectés
     * 
     * @return Nombre de connexions actives
     * 
     * @note Thread-safe
     */
    size_t getClientCount() const;
    
    /**
     * @brief Obtient les statistiques du serveur
     * 
     * @return Structure Stats avec métriques
     * 
     * @note Thread-safe
     */
    Stats getStats() const;
    
    // ========================================================================
    // ENVOI DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Envoie un message à un client spécifique
     * 
     * @param hdl Handle de connexion du client
     * @param message Message à envoyer (format MessageEnvelope)
     * 
     * @return true si envoi réussi
     * 
     * @note Thread-safe
     * @note Incrémente stats.messagesSent
     */
    bool sendTo(connection_hdl hdl, const MessageEnvelope& message);
    
    /**
     * @brief ✅ NOUVEAU: Envoie une erreur formatée à un client
     * 
     * Helper pour envoyer rapidement une erreur au format MessageEnvelope.
     * 
     * @param hdl Handle de connexion du client
     * @param requestId ID de la requête d'origine (vide si non applicable)
     * @param code Code d'erreur (ErrorCode enum)
     * @param message Message d'erreur lisible
     * @param details Détails supplémentaires (optionnel)
     * 
     * @return true si envoi réussi
     * 
     * @note Thread-safe
     * @note Incrémente stats.errorCount
     * 
     * @example
     * ```cpp
     * sendError(hdl, "req-123", ErrorCode::INVALID_PARAMS, 
     *          "Missing required field 'name'", 
     *          json{{"field", "name"}});
     * ```
     */
    bool sendError(connection_hdl hdl,
                   const std::string& requestId,
                   protocol::ErrorCode code,
                   const std::string& message,
                   const json& details = json::object());
    
    /**
     * @brief Broadcast un message à tous les clients
     * 
     * @param message Message à broadcaster (format MessageEnvelope)
     * 
     * @note Thread-safe
     * @note Incrémente stats.messagesSent pour chaque client
     */
    void broadcast(const MessageEnvelope& message);
    
    /**
     * @brief Broadcast un message legacy (rétrocompatibilité)
     * 
     * @param legacyJson Message au format ancien
     * 
     * @deprecated Utiliser broadcast(MessageEnvelope) à la place
     */
    void broadcast(const json& legacyJson);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Enregistre le callback de traitement de commandes
     * 
     * @param callback Fonction callback à appeler pour chaque commande
     * 
     * @note Doit être appelé AVANT start()
     * @warning Appeler après start() génère un warning
     */
    void setCommandCallback(CommandCallback callback);
    
private:
    // ========================================================================
    // HANDLERS WEBSOCKET (privés)
    // ========================================================================
    
    /**
     * @brief Handler: nouvelle connexion
     * 
     * - Ajoute la connexion à l'ensemble
     * - Incrémente stats
     * - Envoie message de bienvenue
     * 
     * @param hdl Handle de la connexion
     */
    void onOpen(connection_hdl hdl);
    
    /**
     * @brief Handler: connexion fermée
     * 
     * - Retire la connexion de l'ensemble
     * - Met à jour stats
     * 
     * @param hdl Handle de la connexion
     */
    void onClose(connection_hdl hdl);
    
    /**
     * @brief ✅ CORRIGÉ: Handler message reçu - IMPLÉMENTATION COMPLÈTE
     * 
     * Flux complet:
     * 1. Parse MessageEnvelope depuis JSON
     * 2. Valide le message
     * 3. Vérifie que c'est une REQUEST
     * 4. Extrait requestId
     * 5. Appelle commandCallback_
     * 6. Crée RESPONSE avec même requestId
     * 7. Envoie réponse au client
     * 8. Gère toutes les erreurs avec sendError()
     * 
     * @param hdl Handle de la connexion
     * @param msg Message WebSocket reçu
     */
    void onMessage(connection_hdl hdl, message_ptr msg);
    
    /**
     * @brief ✅ NOUVEAU: Handler échec de connexion
     * 
     * @param hdl Handle de la connexion
     */
    void onFail(connection_hdl hdl);
    
    // ========================================================================
    // THREAD SERVEUR
    // ========================================================================
    
    /**
     * @brief Thread principal du serveur
     * 
     * Lance le serveur WebSocket et traite les événements.
     */
    void serverThread();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    // Serveur WebSocket
    server_t server_;
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections_;
    std::thread serverThread_;
    
    // État
    bool running_;
    int port_;
    
    // Callback
    CommandCallback commandCallback_;
    
    // Thread-safety
    mutable std::mutex connectionsMutex_;
    mutable std::mutex statsMutex_;
    
    // Statistiques
    Stats stats_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ApiServer.h v3.1.0-corrections
// ============================================================================
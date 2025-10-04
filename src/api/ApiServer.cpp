// ============================================================================
// Fichier: src/api/ApiServer.cpp
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Implémentation du serveur WebSocket API. Gère les connexions des clients,
//   reçoit et route les commandes, et broadcaste les événements et le statut.
//
// Architecture:
//   - WebSocketpp pour le serveur WebSocket
//   - Thread dédié pour le serveur (asynchrone)
//   - Communication via JSON (nlohmann/json)
//   - Thread-safe pour les opérations de broadcast
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#include "ApiServer.h"
#include "../core/Logger.h"

// Includes additionnels
#include <algorithm>  // Pour std::find_if

namespace midiMind {

// ============================================================================
// CONSTRUCTEUR
// ============================================================================

/**
 * @brief Constructeur - Initialise le serveur WebSocket
 * 
 * Configure les handlers WebSocketpp et initialise les statistiques.
 */
ApiServer::ApiServer() {
    Logger::debug("ApiServer", "Constructing ApiServer...");
    
    try {
        // ====================================================================
        // CONFIGURATION DU SERVEUR WEBSOCKETPP
        // ====================================================================
        
        // Désactiver les logs internes de WebSocketpp (trop verbeux)
        server_.clear_access_channels(websocketpp::log::alevel::all);
        server_.clear_error_channels(websocketpp::log::elevel::all);
        
        // Initialiser Asio (moteur réseau)
        server_.init_asio();
        
        // Désactiver le mode de réutilisation d'adresse (évite les conflits)
        server_.set_reuse_addr(true);
        
        // ====================================================================
        // ENREGISTREMENT DES HANDLERS
        // ====================================================================
        
        // Handler: Nouvelle connexion
        server_.set_open_handler([this](connection_hdl hdl) {
            this->onOpen(hdl);
        });
        
        // Handler: Connexion fermée
        server_.set_close_handler([this](connection_hdl hdl) {
            this->onClose(hdl);
        });
        
        // Handler: Message reçu
        server_.set_message_handler([this](connection_hdl hdl, WebSocketServer::message_ptr msg) {
            this->onMessage(hdl, msg);
        });
        
        // Handler: Erreur
        server_.set_fail_handler([this](connection_hdl hdl) {
            this->onError(hdl, std::error_code());
        });
        
        Logger::info("ApiServer", "ApiServer constructed successfully");
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", "Failed to construct ApiServer: " + std::string(e.what()));
        throw;
    }
}

// ============================================================================
// DESTRUCTEUR
// ============================================================================

/**
 * @brief Destructeur - Arrête proprement le serveur
 */
ApiServer::~ApiServer() {
    Logger::debug("ApiServer", "Destroying ApiServer...");
    
    try {
        stop();
    } catch (const std::exception& e) {
        Logger::error("ApiServer", "Error in destructor: " + std::string(e.what()));
    }
    
    Logger::debug("ApiServer", "ApiServer destroyed");
}

// ============================================================================
// MÉTHODE: start()
// ============================================================================

/**
 * @brief Démarre le serveur WebSocket
 * 
 * @param port Port d'écoute (typiquement 8080)
 * @throws std::runtime_error Si le démarrage échoue
 */
void ApiServer::start(int port) {
    // Vérifier si déjà démarré
    if (running_) {
        Logger::warn("ApiServer", "Server already running");
        return;
    }
    
    Logger::info("ApiServer", "Starting WebSocket server on port " + std::to_string(port));
    
    try {
        port_ = port;
        running_ = true;
        
        // Lancer le thread du serveur
        serverThread_ = std::thread(&ApiServer::serverThread, this);
        
        // Attendre un peu pour que le serveur démarre
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        Logger::info("ApiServer", "✓ WebSocket server started successfully");
        
    } catch (const std::exception& e) {
        running_ = false;
        Logger::error("ApiServer", "Failed to start server: " + std::string(e.what()));
        throw std::runtime_error("Failed to start ApiServer: " + std::string(e.what()));
    }
}

// ============================================================================
// MÉTHODE: stop()
// ============================================================================

/**
 * @brief Arrête proprement le serveur WebSocket
 */
void ApiServer::stop() {
    // Vérifier si déjà arrêté
    if (!running_) {
        Logger::debug("ApiServer", "Server already stopped");
        return;
    }
    
    Logger::info("ApiServer", "Stopping WebSocket server...");
    
    running_ = false;
    
    try {
        // Fermer toutes les connexions
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            
            Logger::info("ApiServer", "Closing " + std::to_string(connections_.size()) + " connection(s)...");
            
            for (auto& hdl : connections_) {
                try {
                    server_.close(hdl, websocketpp::close::status::going_away, "Server shutting down");
                } catch (const std::exception& e) {
                    Logger::warn("ApiServer", "Error closing connection: " + std::string(e.what()));
                }
            }
            
            connections_.clear();
        }
        
        // Arrêter le serveur
        try {
            server_.stop_listening();
            server_.stop();
        } catch (const std::exception& e) {
            Logger::warn("ApiServer", "Error stopping server: " + std::string(e.what()));
        }
        
        // Attendre que le thread se termine
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        
        Logger::info("ApiServer", "✓ WebSocket server stopped");
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", "Error during server stop: " + std::string(e.what()));
    }
}

// ============================================================================
// MÉTHODE: getClientCount()
// ============================================================================

/**
 * @brief Récupère le nombre de clients connectés
 * 
 * @return size_t Nombre de clients
 */
size_t ApiServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return connections_.size();
}

// ============================================================================
// MÉTHODE: setCommandCallback()
// ============================================================================

/**
 * @brief Définit le callback de traitement des commandes
 * 
 * @param callback Fonction qui traite les commandes JSON
 */
void ApiServer::setCommandCallback(CommandCallback callback) {
    commandCallback_ = callback;
    Logger::debug("ApiServer", "Command callback registered");
}

// ============================================================================
// MÉTHODE: broadcast()
// ============================================================================

/**
 * @brief Broadcaste un message JSON à tous les clients connectés
 * 
 * @param message Message JSON à broadcaster
 */
void APIServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    // Nettoyer les clients déconnectés AVANT broadcast
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [](const auto& client) {
                return !client->isConnected();
            }),
        clients_.end()
    );
    
    // Broadcast aux clients connectés
    for (auto& client : clients_) {
        try {
            if (client->isConnected()) {
                client->send(message);
            }
        } catch (const std::exception& e) {
            Logger::warn("APIServer", "Broadcast failed: " + std::string(e.what()));
            // Le client sera retiré au prochain nettoyage
        }
    }
}






// Ajouter aussi un nettoyage périodique
void APIServer::cleanupDisconnectedClients() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    size_t before = clients_.size();
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [](const auto& client) { return !client->isConnected(); }),
        clients_.end()
    );
    
    size_t removed = before - clients_.size();
    if (removed > 0) {
        Logger::debug("APIServer", "Cleaned up " + std::to_string(removed) + " disconnected clients");
    }
}
// ============================================================================
// MÉTHODE: sendTo()
// ============================================================================

/**
 * @brief Envoie un message JSON à un client spécifique
 * 
 * @param hdl Handle de connexion du client
 * @param message Message JSON à envoyer
 */
void ApiServer::sendTo(connection_hdl hdl, const json& message) {
    std::string messageStr = message.dump();
    
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.messagesSent++;
    }
    
    safeSend(hdl, messageStr);
}

// ============================================================================
// MÉTHODE: sendToAll() (alias de broadcast)
// ============================================================================

/**
 * @brief Alias de broadcast() pour compatibilité
 * 
 * @param message Message JSON à broadcaster
 */
void ApiServer::sendToAll(const json& message) {
    broadcast(message);
}

// ============================================================================
// MÉTHODE: getStats()
// ============================================================================

/**
 * @brief Récupère les statistiques du serveur
 * 
 * @return Stats Structure contenant les statistiques
 */
ApiServer::Stats ApiServer::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

// ============================================================================
// HANDLER: onOpen()
// ============================================================================

/**
 * @brief Handler appelé lors d'une nouvelle connexion
 * 
 * @param hdl Handle de la nouvelle connexion
 */
void ApiServer::onOpen(connection_hdl hdl) {
    Logger::info("ApiServer", "New client connected");
    
    // Ajouter à la liste des connexions
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.insert(hdl);
    }
    
    // Mettre à jour les statistiques
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.totalConnections++;
        stats_.activeConnections = connections_.size();
    }
    
    // Envoyer un message de bienvenue
    try {
        json welcome;
        welcome["event"] = "connection.established";
        welcome["message"] = "Welcome to midiMind v3.0 API";
        welcome["api_version"] = "3.0.0";
        welcome["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        sendTo(hdl, welcome);
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", "Error sending welcome message: " + std::string(e.what()));
    }
    
    Logger::info("ApiServer", "Total clients: " + std::to_string(connections_.size()));
}

// ============================================================================
// HANDLER: onClose()
// ============================================================================

/**
 * @brief Handler appelé lors de la fermeture d'une connexion
 * 
 * @param hdl Handle de la connexion fermée
 */
void ApiServer::onClose(connection_hdl hdl) {
    Logger::info("ApiServer", "Client disconnected");
    
    // Retirer de la liste des connexions
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.erase(hdl);
    }
    
    // Mettre à jour les statistiques
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.activeConnections = connections_.size();
    }
    
    Logger::info("ApiServer", "Total clients: " + std::to_string(connections_.size()));
}

// ============================================================================
// HANDLER: onMessage()
// ============================================================================

/**
 * @brief Handler appelé lors de la réception d'un message
 * 
 * @param hdl Handle de la connexion
 * @param msg Pointeur vers le message reçu
 */
void ApiServer::onMessage(connection_hdl hdl, WebSocketServer::message_ptr msg) {
    // Récupérer le contenu du message
    std::string payload = msg->get_payload();
    
    Logger::debug("ApiServer", "Received message: " + payload);
    
    // Mettre à jour les statistiques
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.messagesReceived++;
    }
    
    try {
        // Parser le JSON
        json command = json::parse(payload);
        
        // Vérifier si un callback est défini
        if (!commandCallback_) {
            Logger::warn("ApiServer", "No command callback registered");
            
            json error;
            error["success"] = false;
            error["error"] = "Server not ready - no command processor available";
            
            sendTo(hdl, error);
            return;
        }
        
        // Traiter la commande via le callback
        json response = commandCallback_(command);
        
        // Envoyer la réponse
        sendTo(hdl, response);
        
    } catch (const json::parse_error& e) {
        // Erreur de parsing JSON
        Logger::error("ApiServer", "JSON parse error: " + std::string(e.what()));
        
        json error;
        error["success"] = false;
        error["error"] = "Invalid JSON";
        error["details"] = e.what();
        
        sendTo(hdl, error);
        
        // Mettre à jour les statistiques
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.errorCount++;
        }
        
    } catch (const std::exception& e) {
        // Autre erreur
        Logger::error("ApiServer", "Error processing message: " + std::string(e.what()));
        
        json error;
        error["success"] = false;
        error["error"] = "Internal server error";
        error["details"] = e.what();
        
        sendTo(hdl, error);
        
        // Mettre à jour les statistiques
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.errorCount++;
        }
    }
}

// ============================================================================
// HANDLER: onError()
// ============================================================================

/**
 * @brief Handler appelé lors d'une erreur WebSocket
 * 
 * @param hdl Handle de la connexion
 * @param ec Code d'erreur
 */
void ApiServer::onError(connection_hdl hdl, const std::error_code& ec) {
    if (ec) {
        Logger::error("ApiServer", "WebSocket error: " + ec.message());
    } else {
        Logger::error("ApiServer", "WebSocket error (unknown)");
    }
    
    // Mettre à jour les statistiques
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.errorCount++;
    }
}

// ============================================================================
// MÉTHODE PRIVÉE: safeSend()
// ============================================================================

/**
 * @brief Envoie un message de manière sécurisée (avec gestion d'erreurs)
 * 
 * @param hdl Handle de la connexion
 * @param message Message à envoyer (string)
 * @return true Si l'envoi a réussi
 * @return false Si l'envoi a échoué
 */
bool ApiServer::safeSend(connection_hdl hdl, const std::string& message) {
    try {
        server_.send(hdl, message, websocketpp::frame::opcode::text);
        return true;
        
    } catch (const websocketpp::exception& e) {
        Logger::error("ApiServer", "WebSocket exception: " + std::string(e.what()));
        return false;
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", "Failed to send message: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// MÉTHODE PRIVÉE: serverThread()
// ============================================================================

/**
 * @brief Thread du serveur WebSocket (boucle principale)
 * 
 * Cette méthode tourne dans un thread séparé et gère la boucle d'événements
 * du serveur WebSocket.
 */
void ApiServer::serverThread() {
    Logger::debug("ApiServer", "Server thread started");
    
    try {
        // Commencer à écouter sur le port
        server_.listen(port_);
        
        // Commencer à accepter les connexions
        server_.start_accept();
        
        Logger::info("ApiServer", "WebSocket server listening on port " + std::to_string(port_));
        
        // Entrer dans la boucle d'événements (bloquante)
        // Cette méthode ne retourne que lorsque server_.stop() est appelé
        server_.run();
        
    } catch (const websocketpp::exception& e) {
        Logger::error("ApiServer", "WebSocket exception in server thread: " + std::string(e.what()));
        running_ = false;
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", "Server thread error: " + std::string(e.what()));
        running_ = false;
        
    } catch (...) {
        Logger::error("ApiServer", "Unknown error in server thread");
        running_ = false;
    }
    
    Logger::debug("ApiServer", "Server thread stopped");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ApiServer.cpp
// ============================================================================

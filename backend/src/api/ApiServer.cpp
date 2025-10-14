// ============================================================================
// Fichier: backend/src/api/ApiServer.cpp
// Version: 3.0.0-refonte
// Date: 2025-10-09
// ============================================================================
// Description:
//   Serveur WebSocket avec protocole unifié.
//   Support complet du nouveau format d'enveloppe avec requestId.
//   Gestion des requêtes, réponses, événements et erreurs.
// ============================================================================

#include "ApiServer.h"
#include "MessageEnvelope.h"
#include "../core/Logger.h"
#include <functional>

namespace midiMind {

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR
// ============================================================================

ApiServer::ApiServer()
    : running_(false)
    , port_(8080)
{
    Logger::info("ApiServer", "Creating WebSocket server...");
    
    // Configuration WebSocket
    server_.set_access_channels(websocketpp::log::alevel::none);
    server_.set_error_channels(websocketpp::log::elevel::none);
    
    server_.init_asio();
    server_.set_reuse_addr(true);
    
    // Handlers
    server_.set_open_handler(
        std::bind(&ApiServer::onOpen, this, std::placeholders::_1)
    );
    
    server_.set_close_handler(
        std::bind(&ApiServer::onClose, this, std::placeholders::_1)
    );
    
    server_.set_message_handler(
        std::bind(&ApiServer::onMessage, this, 
                 std::placeholders::_1, std::placeholders::_2)
    );
    
    server_.set_fail_handler(
        std::bind(&ApiServer::onError, this, 
                 std::placeholders::_1, std::placeholders::_2)
    );
    
    // Statistiques
    stats_.startTime = std::chrono::steady_clock::now();
    stats_.activeConnections = 0;
    stats_.totalConnections = 0;
    stats_.messagesSent = 0;
    stats_.messagesReceived = 0;
    stats_.errorCount = 0;
    
    Logger::info("ApiServer", "✓ WebSocket server created");
}

ApiServer::~ApiServer() {
    stop();
}

// ============================================================================
// THREAD SERVEUR
// ============================================================================

void ApiServer::serverThread() {
    try {
        Logger::info("ApiServer", "Server thread starting...");
        
        server_.listen(port_);
        server_.start_accept();
        
        Logger::info("ApiServer", "✓ Listening on port " + std::to_string(port_));
        
        server_.run();
        
        Logger::info("ApiServer", "Server thread stopped");
    }
    catch (const std::exception& e) {
        Logger::error("ApiServer", "Server thread error: " + std::string(e.what()));
        running_ = false;
    }
}

// ============================================================================
// HANDLERS WEBSOCKET
// ============================================================================

void ApiServer::onOpen(connection_hdl hdl) {
    Logger::info("ApiServer", "New client connected");
    
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.insert(hdl);
        
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.activeConnections = connections_.size();
        stats_.totalConnections++;
    }
    
    // Envoyer message de bienvenue avec nouveau protocole
    try {
        auto welcome = MessageEnvelope::createEvent(
            "server:welcome",
            {
                {"version", protocol::PROTOCOL_VERSION},
                {"server", "midiMind v3.0"},
                {"uptime", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - stats_.startTime
                ).count()}
            },
            protocol::EventPriority::NORMAL
        );
        
        sendTo(hdl, welcome);
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
            "Error sending welcome message: " + std::string(e.what()));
    }
    
    Logger::info("ApiServer", "Total clients: " + std::to_string(connections_.size()));
}

void ApiServer::onClose(connection_hdl hdl) {
    Logger::info("ApiServer", "Client disconnected");
    
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.erase(hdl);
    }
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.activeConnections = connections_.size();
    }
    
    Logger::info("ApiServer", "Total clients: " + std::to_string(connections_.size()));
}

void ApiServer::onMessage(connection_hdl hdl, WebSocketServer::message_ptr msg) {
    std::string payload = msg->get_payload();
    
    Logger::debug("ApiServer", "Received: " + payload.substr(0, 100) + 
                 (payload.length() > 100 ? "..." : ""));
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.messagesReceived++;
    }
    
    // Parser le message avec le nouveau protocole
    auto envelopeOpt = MessageEnvelope::fromJsonString(payload);
    
    if (!envelopeOpt) {
        Logger::error("ApiServer", "Failed to parse message");
        
        auto errorMsg = MessageEnvelope::createError(
            protocol::ErrorCode::INVALID_FORMAT,
            "Invalid message format",
            {{"received", payload.substr(0, 100)}}
        );
        
        sendTo(hdl, errorMsg);
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.errorCount++;
        return;
    }
    
    auto envelope = *envelopeOpt;
    
    // Traiter selon le type
    if (envelope.isRequest()) {
        handleRequest(hdl, envelope);
    }
    else if (envelope.isEvent()) {
        // Les clients peuvent envoyer des events (ex: heartbeat)
        handleClientEvent(hdl, envelope);
    }
    else {
        Logger::warn("ApiServer", "Unexpected message type from client");
    }
}

void ApiServer::onError(connection_hdl hdl, const std::error_code& ec) {
    if (ec) {
        Logger::error("ApiServer", "WebSocket error: " + ec.message());
    } else {
        Logger::error("ApiServer", "WebSocket error (unknown)");
    }
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.errorCount++;
    }
}

// ============================================================================
// TRAITEMENT REQUÊTES (NOUVEAU PROTOCOLE)
// ============================================================================

void ApiServer::handleRequest(connection_hdl hdl, const MessageEnvelope& envelope) {
    try {
        const auto& request = envelope.getRequest();
        const std::string& requestId = envelope.getId();
        
        Logger::debug("ApiServer", 
            "Processing request: " + request.command + " (id: " + requestId + ")");
        
        // Vérifier callback
        if (!commandCallback_) {
            Logger::warn("ApiServer", "No command callback registered");
            
            auto errorMsg = MessageEnvelope::createErrorResponse(
                requestId,
                protocol::ErrorCode::SERVICE_UNAVAILABLE,
                "Server not ready - no command processor available",
                json::object(),
                true
            );
            
            sendTo(hdl, errorMsg);
            return;
        }
        
        // Construire commande pour le callback (format legacy)
        json legacyCommand;
        legacyCommand["command"] = request.command;
        legacyCommand["params"] = request.params;
        
        // Exécuter la commande
        json result = commandCallback_(legacyCommand);
        
        // Calculer latence
        int latency = envelope.getLatencySinceCreation();
        
        // Créer réponse
        MessageEnvelope response;
        
        if (result.value("success", false)) {
            response = MessageEnvelope::createSuccessResponse(
                requestId,
                result.value("data", json::object()),
                latency
            );
        } else {
            // Mapper l'erreur
            protocol::ErrorCode errorCode = protocol::ErrorCode::COMMAND_FAILED;
            
            if (result.contains("error_code")) {
                std::string codeStr = result["error_code"];
                // TODO: Mapper les codes d'erreur legacy vers nouveaux codes
            }
            
            response = MessageEnvelope::createErrorResponse(
                requestId,
                errorCode,
                result.value("error", "Command failed"),
                result.value("details", json::object()),
                false
            );
        }
        
        sendTo(hdl, response);
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
            "Error processing request: " + std::string(e.what()));
        
        auto errorMsg = MessageEnvelope::createErrorResponse(
            envelope.getId(),
            protocol::ErrorCode::INTERNAL_ERROR,
            "Internal server error",
            {{"details", e.what()}},
            false
        );
        
        sendTo(hdl, errorMsg);
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.errorCount++;
    }
}

void ApiServer::handleClientEvent(connection_hdl hdl, const MessageEnvelope& envelope) {
    try {
        const auto& event = envelope.getEvent();
        
        Logger::debug("ApiServer", "Received event from client: " + event.name);
        
        // Traiter les événements spéciaux
        if (event.name == "ping" || event.name == "heartbeat") {
            // Répondre avec pong
            auto pong = MessageEnvelope::createEvent(
                "pong",
                {
                    {"timestamp", envelope.getTimestamp()},
                    {"server_time", protocol::Envelope::getCurrentTimestamp()}
                },
                protocol::EventPriority::HIGH
            );
            
            sendTo(hdl, pong);
        }
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
            "Error handling client event: " + std::string(e.what()));
    }
}

// ============================================================================
// ENVOI DE MESSAGES
// ============================================================================

void ApiServer::sendTo(connection_hdl hdl, const MessageEnvelope& message) {
    try {
        std::string jsonStr = message.toJsonString();
        
        if (safeSend(hdl, jsonStr)) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.messagesSent++;
        }
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
            "Error serializing message: " + std::string(e.what()));
    }
}

bool ApiServer::safeSend(connection_hdl hdl, const std::string& message) {
    try {
        auto con = server_.get_con_from_hdl(hdl);
        
        if (!con) {
            Logger::warn("ApiServer", "Invalid connection handle");
            return false;
        }
        
        if (con->get_state() != websocketpp::session::state::open) {
            Logger::warn("ApiServer", "Connection not open");
            return false;
        }
        
        server_.send(hdl, message, websocketpp::frame::opcode::text);
        return true;
        
    } catch (const websocketpp::exception& e) {
        Logger::error("ApiServer", "WebSocket send error: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        Logger::error("ApiServer", "Error sending message: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// BROADCAST
// ============================================================================

void ApiServer::broadcast(const MessageEnvelope& message) {
    std::string jsonStr = message.toJsonString();
    
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    for (auto& hdl : connections_) {
        safeSend(hdl, jsonStr);
    }
    
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.messagesSent += connections_.size();
    }
}

void ApiServer::broadcast(const json& legacyJson) {
    // Support du format legacy pour rétrocompatibilité
    auto event = MessageEnvelope::createEvent(
        "legacy",
        legacyJson,
        protocol::EventPriority::NORMAL
    );
    
    broadcast(event);
}

// ============================================================================
// GESTION SERVEUR
// ============================================================================

void ApiServer::start(int port) {
    if (running_) {
        Logger::warn("ApiServer", "Server already running");
        return;
    }
    
    Logger::info("ApiServer", 
        "Starting WebSocket server on port " + std::to_string(port));
    
    try {
        port_ = port;
        running_ = true;
        
        serverThread_ = std::thread(&ApiServer::serverThread, this);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        Logger::info("ApiServer", "✓ WebSocket server started successfully");
        
    } catch (const std::exception& e) {
        running_ = false;
        Logger::error("ApiServer", 
            "Failed to start server: " + std::string(e.what()));
        throw std::runtime_error(
            "Failed to start ApiServer: " + std::string(e.what()));
    }
}

void ApiServer::stop() {
    if (!running_) {
        Logger::debug("ApiServer", "Server already stopped");
        return;
    }
    
    Logger::info("ApiServer", "Stopping WebSocket server...");
    
    running_ = false;
    
    try {
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            
            Logger::info("ApiServer", 
                "Closing " + std::to_string(connections_.size()) + " connection(s)...");
            
            for (auto& hdl : connections_) {
                try {
                    server_.close(hdl, websocketpp::close::status::going_away, 
                                 "Server shutting down");
                } catch (const std::exception& e) {
                    Logger::warn("ApiServer", 
                        "Error closing connection: " + std::string(e.what()));
                }
            }
            
            connections_.clear();
        }
        
        try {
            server_.stop_listening();
            server_.stop();
        } catch (const std::exception& e) {
            Logger::warn("ApiServer", 
                "Error stopping server: " + std::string(e.what()));
        }
        
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        
        Logger::info("ApiServer", "✓ WebSocket server stopped");
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
            "Error during server stop: " + std::string(e.what()));
    }
}

size_t ApiServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return connections_.size();
}

// ============================================================================
// CALLBACKS
// ============================================================================

void ApiServer::setCommandCallback(CommandCallback callback) {
    if (running_) {
        Logger::warn("ApiServer", "Setting callback while server is running!");
    }
    
    commandCallback_ = callback;
    Logger::debug("ApiServer", "Command callback registered");
}

// ============================================================================
// UTILITAIRES
// ============================================================================

void ApiServer::cleanupDisconnectedClients() {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    size_t before = connections_.size();
    
    for (auto it = connections_.begin(); it != connections_.end(); ) {
        try {
            auto con = server_.get_con_from_hdl(*it);
            if (!con || con->get_state() != websocketpp::session::state::open) {
                it = connections_.erase(it);
            } else {
                ++it;
            }
        } catch (...) {
            it = connections_.erase(it);
        }
    }
    
    size_t removed = before - connections_.size();
    if (removed > 0) {
        Logger::debug("ApiServer", 
            "Cleaned up " + std::to_string(removed) + " disconnected clients");
    }
}

ApiServer::Stats ApiServer::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ApiServer.cpp
// ============================================================================

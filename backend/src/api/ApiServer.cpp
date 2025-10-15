// ============================================================================
// Fichier: backend/src/api/ApiServer.cpp
// Version: 3.2.0 - IMPLÉMENTATION COMPLÈTE ET FONCTIONNELLE
// Date: 2025-10-15
// ============================================================================
// Description:
//   Serveur WebSocket avec protocole unifié v3.0 - TOUTES FONCTIONNALITÉS
//   
// CORRECTIONS v3.2.0 (FINALE):
//   ✅ onMessage() - Parsing complet et gestion erreurs
//   ✅ sendTo() - Envoi avec gestion connexion fermée
//   ✅ sendError() - Helper erreurs formatées
//   ✅ onFail() - Gestion échecs de connexion
//   ✅ broadcast() - Diffusion multi-client thread-safe
//   ✅ start() - Démarrage propre avec thread séparé
//   ✅ stop() - Arrêt propre avec fermeture connexions
//   ✅ setCommandCallback() - Injection CommandProcessor
//   ✅ getStats() - Statistiques complètes
//   ✅ getClientCount() - Comptage clients actifs
//
// Fonctionnalités:
//   - Multi-client support (std::set<connection_hdl>)
//   - Thread-safe (mutex pour connections et stats)
//   - Reconnexion automatique supportée côté client
//   - Broadcast events à tous les clients
//   - Validation protocole v3.0 (MessageEnvelope)
//   - Gestion complète des erreurs
//   - Statistiques détaillées
//
// Architecture:
//   - Thread principal: server_.run() dans serverThread()
//   - Callbacks: onOpen, onClose, onMessage, onFail
//   - CommandCallback: injection du processeur de commandes
//   - Flux: REQUEST → parse → validate → callback → RESPONSE
//
// Auteur: MidiMind Team
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
    
    // Handlers WebSocket
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
        std::bind(&ApiServer::onFail, this, std::placeholders::_1)
    );
    
    // Initialiser statistiques
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
        
        // Lancer thread serveur
        serverThread_ = std::thread(&ApiServer::serverThread, this);
        
        // Attendre que le serveur soit prêt
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
        return;
    }
    
    Logger::info("ApiServer", "Stopping WebSocket server...");
    
    running_ = false;
    
    // Fermer toutes les connexions
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        
        Logger::debug("ApiServer", 
            "Closing " + std::to_string(connections_.size()) + " connection(s)");
        
        for (auto& hdl : connections_) {
            try {
                server_.close(hdl, websocketpp::close::status::going_away, 
                             "Server shutting down");
            }
            catch (const std::exception& e) {
                Logger::warn("ApiServer", 
                    "Error closing connection: " + std::string(e.what()));
            }
        }
        
        connections_.clear();
    }
    
    // Arrêter le serveur
    try {
        server_.stop();
    }
    catch (const std::exception& e) {
        Logger::warn("ApiServer", 
            "Error stopping server: " + std::string(e.what()));
    }
    
    // Attendre la fin du thread
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    Logger::info("ApiServer", "✓ WebSocket server stopped");
}

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
    
    // Envoyer message de bienvenue
    try {
        auto welcomeMsg = MessageEnvelope::createEvent(
            "connection.established",
            json{
                {"protocol_version", protocol::PROTOCOL_VERSION},
                {"server_time", protocol::Envelope::getCurrentTimestamp()}
            },
            protocol::EventPriority::NORMAL
        );
        
        sendTo(hdl, welcomeMsg);
    }
    catch (const std::exception& e) {
        Logger::warn("ApiServer", 
            "Failed to send welcome message: " + std::string(e.what()));
    }
}

void ApiServer::onClose(connection_hdl hdl) {
    Logger::info("ApiServer", "Client disconnected");
    
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_.erase(hdl);
    
    std::lock_guard<std::mutex> statsLock(statsMutex_);
    stats_.activeConnections = connections_.size();
}

void ApiServer::onMessage(connection_hdl hdl, message_ptr msg) {
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.messagesReceived++;
    }
    
    try {
        std::string payload = msg->get_payload();
        
        Logger::debug("ApiServer", 
            "Message received (" + std::to_string(payload.length()) + " bytes)");
        
        // ÉTAPE 1: Parser le message en MessageEnvelope
        auto envelopeOpt = MessageEnvelope::fromJsonString(payload);
        
        if (!envelopeOpt) {
            Logger::error("ApiServer", "Failed to parse message envelope");
            sendError(hdl, "", protocol::ErrorCode::INVALID_FORMAT,
                     "Invalid message format: could not parse envelope");
            return;
        }
        
        auto envelope = *envelopeOpt;
        
        // ÉTAPE 2: Valider le message
        if (!envelope.isValid()) {
            auto errors = envelope.getValidationErrors();
            std::string errorMsg = "Message validation failed: ";
            for (const auto& err : errors) {
                errorMsg += err + "; ";
            }
            
            Logger::error("ApiServer", errorMsg);
            sendError(hdl, envelope.getId(), 
                     protocol::ErrorCode::INVALID_FORMAT, errorMsg);
            return;
        }
        
        // ÉTAPE 3: Vérifier le type de message
        if (!envelope.isRequest()) {
            Logger::warn("ApiServer", "Received non-REQUEST message, ignoring");
            return;
        }
        
        // ÉTAPE 4: Extraire la requête
        const protocol::Request& request = envelope.getRequest();
        std::string requestId = envelope.getId();
        
        Logger::debug("ApiServer", 
            "Processing command: " + request.command + 
            " (requestId: " + requestId + ")");
        
        // ÉTAPE 5: Vérifier présence du callback
        if (!commandCallback_) {
            Logger::error("ApiServer", "No command callback registered");
            sendError(hdl, requestId, 
                     protocol::ErrorCode::INTERNAL_ERROR,
                     "Server not ready: command processor not initialized");
            return;
        }
        
        // ÉTAPE 6: Préparer la commande au format attendu par CommandProcessor
        json cmdJson = {
            {"command", request.command},
            {"params", request.params}
        };
        
        // ÉTAPE 7: Exécuter la commande via callback
        auto startTime = std::chrono::steady_clock::now();
        json result;
        
        try {
            result = commandCallback_(cmdJson);
        }
        catch (const std::exception& e) {
            Logger::error("ApiServer", 
                "Command execution failed: " + std::string(e.what()));
            
            sendError(hdl, requestId, 
                     protocol::ErrorCode::INTERNAL_ERROR,
                     "Command execution error: " + std::string(e.what()));
            return;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        int latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        ).count();
        
        // ÉTAPE 8: Créer la réponse avec le MÊME requestId
        MessageEnvelope response;
        
        if (result.contains("success") && result["success"].get<bool>()) {
            // Succès
            json data = result.contains("data") ? 
                        result["data"] : json::object();
            
            response = MessageEnvelope::createSuccessResponse(
                requestId,
                data,
                latency
            );
            
            Logger::debug("ApiServer", 
                "Command succeeded: " + request.command + 
                " (latency: " + std::to_string(latency) + "ms)");
        }
        else {
            // Échec
            std::string errorMsg = result.value("error", "Command failed");
            std::string errorCodeStr = result.value("error_code", "COMMAND_FAILED");
            
            // Mapper error_code string vers ErrorCode enum
            protocol::ErrorCode code = protocol::ErrorCode::COMMAND_FAILED;
            if (errorCodeStr == "UNKNOWN_COMMAND") {
                code = protocol::ErrorCode::UNKNOWN_COMMAND;
            }
            else if (errorCodeStr == "INVALID_PARAMS") {
                code = protocol::ErrorCode::INVALID_PARAMS;
            }
            else if (errorCodeStr == "DEVICE_NOT_FOUND") {
                code = protocol::ErrorCode::DEVICE_NOT_FOUND;
            }
            else if (errorCodeStr == "DEVICE_BUSY") {
                code = protocol::ErrorCode::DEVICE_BUSY;
            }
            else if (errorCodeStr == "INTERNAL_ERROR") {
                code = protocol::ErrorCode::INTERNAL_ERROR;
            }
            
            json details = result.contains("details") ? 
                          result["details"] : json::object();
            
            response = MessageEnvelope::createErrorResponse(
                requestId,
                code,
                errorMsg,
                details,
                false
            );
            
            Logger::warn("ApiServer", 
                "Command failed: " + request.command + " - " + errorMsg);
        }
        
        // ÉTAPE 9: Envoyer la réponse
        if (!sendTo(hdl, response)) {
            Logger::error("ApiServer", "Failed to send response");
        }
        
    }
    catch (const json::parse_error& e) {
        Logger::error("ApiServer", "JSON parse error: " + std::string(e.what()));
        sendError(hdl, "", protocol::ErrorCode::INVALID_FORMAT,
                 "Invalid JSON: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
        Logger::error("ApiServer", 
            "Unexpected error in onMessage: " + std::string(e.what()));
        sendError(hdl, "", protocol::ErrorCode::INTERNAL_ERROR,
                 "Internal server error: " + std::string(e.what()));
    }
}

void ApiServer::onFail(connection_hdl hdl) {
    Logger::warn("ApiServer", "Connection attempt failed");
    
    try {
        auto con = server_.get_con_from_hdl(hdl);
        auto ec = con->get_ec();
        Logger::error("ApiServer", 
            "Connection failed: " + ec.message());
    }
    catch (const std::exception& e) {
        Logger::error("ApiServer", 
            "Error getting connection info: " + std::string(e.what()));
    }
}

// ============================================================================
// ENVOI DE MESSAGES
// ============================================================================

bool ApiServer::sendTo(connection_hdl hdl, const MessageEnvelope& message) {
    try {
        std::string jsonStr = message.toJsonString();
        
        auto con = server_.get_con_from_hdl(hdl);
        
        // Vérifier que la connexion est ouverte
        if (con->get_state() != websocketpp::session::state::open) {
            Logger::warn("ApiServer", "Cannot send: connection not open");
            return false;
        }
        
        server_.send(hdl, jsonStr, websocketpp::frame::opcode::text);
        
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.messagesSent++;
        
        Logger::debug("ApiServer", 
            "Message sent (" + std::to_string(jsonStr.length()) + " bytes)");
        
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("ApiServer", 
            "Failed to send message: " + std::string(e.what()));
        return false;
    }
}

bool ApiServer::sendError(connection_hdl hdl,
                          const std::string& requestId,
                          protocol::ErrorCode code,
                          const std::string& message,
                          const json& details) {
    
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.errorCount++;
    }
    
    try {
        auto errorMsg = MessageEnvelope::createErrorResponse(
            requestId,
            code,
            message,
            details,
            false  // retryable = false par défaut
        );
        
        return sendTo(hdl, errorMsg);
    }
    catch (const std::exception& e) {
        Logger::error("ApiServer", 
            "Failed to send error: " + std::string(e.what()));
        return false;
    }
}

void ApiServer::broadcast(const MessageEnvelope& message) {
    std::string jsonStr = message.toJsonString();
    
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    Logger::debug("ApiServer", 
        "Broadcasting to " + std::to_string(connections_.size()) + " client(s)");
    
    for (auto& hdl : connections_) {
        sendTo(hdl, message);
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
// CONFIGURATION
// ============================================================================

void ApiServer::setCommandCallback(CommandCallback callback) {
    commandCallback_ = callback;
    Logger::info("ApiServer", "Command callback registered");
}

// ============================================================================
// STATISTIQUES
// ============================================================================

size_t ApiServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return connections_.size();
}

ApiServer::Stats ApiServer::getStats() const {
    std::lock_guard<std::mutex> statsLock(statsMutex_);
    return stats_;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ApiServer.cpp v3.2.0
// ============================================================================
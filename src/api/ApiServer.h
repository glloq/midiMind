// ============================================================================
// src/api/ApiServer.h - WebSocket API Server
// ============================================================================
#pragma once

#include <memory>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using websocketpp::connection_hdl;

namespace midiMind {

typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;

class ApiServer {
public:
    // Callback pour traiter les commandes
    using CommandCallback = std::function<json(const json&)>;
    
    ApiServer();
    ~ApiServer();
    
    // Désactiver copie et assignation
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;

    // Démarrage/Arrêt du serveur
    void start(int port);
    void stop();
    
    bool isRunning() const { return running_; }
    int getPort() const { return port_; }
    size_t getClientCount() const;

    // Définir le callback de traitement des commandes
    void setCommandCallback(CommandCallback callback);

    // Envoi de messages
    void broadcast(const json& message);
    void sendTo(connection_hdl hdl, const json& message);
    void sendToAll(const json& message); // Alias de broadcast
    void cleanupDisconnectedClients(); // un nettoyage périodique

    // Statistiques
    struct Stats {
        size_t totalConnections = 0;
        size_t activeConnections = 0;
        size_t messagesReceived = 0;
        size_t messagesSent = 0;
        size_t errorCount = 0;
    };
    
    Stats getStats() const;

private:
    // Handlers WebSocket
    void onOpen(connection_hdl hdl);
    void onClose(connection_hdl hdl);
    void onMessage(connection_hdl hdl, WebSocketServer::message_ptr msg);
    void onError(connection_hdl hdl, const std::error_code& ec);

    // Envoi sécurisé
    bool safeSend(connection_hdl hdl, const std::string& message);

    // Thread du serveur
    void serverThread();

    // Membres privés
    WebSocketServer server_;
    std::thread serverThread_;
    
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections_;
    mutable std::mutex connectionsMutex_;
    
    CommandCallback commandCallback_;
    std::atomic<bool> running_{false};
    std::atomic<int> port_{0};
    
    mutable std::mutex statsMutex_;
    Stats stats_;
};

} // namespace midiMind
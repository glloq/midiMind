// ============================================================================
// File: backend/src/api/ApiServer.h
// Version: 4.2.7
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.7:
//   - FIXED: start() signature matches implementation (bool return + uint16_t)
//   - FIXED: getClientCount() → getConnectionCount() 
//   - FIXED: Stats struct adds uptime field
//
// ============================================================================

#pragma once

#include "MessageEnvelope.h"
#include "../core/EventBus.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <set>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <vector>
#include <memory>

namespace midiMind {

class ApiServer {
public:
    using server_t = websocketpp::server<websocketpp::config::asio>;
    using connection_hdl = websocketpp::connection_hdl;
    using message_ptr = server_t::message_ptr;
    using CommandCallback = std::function<json(const json&)>;
    using Subscription = std::function<void()>;
    
    struct Stats {
        std::chrono::steady_clock::time_point startTime;
        size_t activeConnections;
        size_t totalConnections;
        size_t messagesSent;
        size_t messagesReceived;
        size_t errorCount;
        int64_t uptime;
    };
    
    explicit ApiServer(std::shared_ptr<EventBus> eventBus = nullptr);
    ~ApiServer();
    
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;
    
    // Server management
    bool start(uint16_t port = 8080);
    void stop();
    bool isRunning() const { return running_; }
    size_t getConnectionCount() const;
    Stats getStats() const;
    
    // Message sending
    bool sendTo(connection_hdl hdl, const MessageEnvelope& message);
    bool sendError(connection_hdl hdl,
                   const std::string& requestId,
                   protocol::ErrorCode code,
                   const std::string& message,
                   const json& details = json::object());
    void broadcast(const MessageEnvelope& message);
    void broadcastEvent(const std::string& name,
                       const json& data,
                       protocol::EventPriority priority = protocol::EventPriority::NORMAL);
    
    // Callback registration
    void setCommandCallback(CommandCallback callback) {
        commandCallback_ = callback;
    }
    
    // EventBus configuration
    void setEventBus(std::shared_ptr<EventBus> eventBus);

private:
    void onOpen(connection_hdl hdl);
    void onClose(connection_hdl hdl);
    void onMessage(connection_hdl hdl, message_ptr msg);
    void onFail(connection_hdl hdl);
    bool onPing(connection_hdl hdl, std::string payload);
    void onPongTimeout(connection_hdl hdl, std::string payload);
    
    void serverThread();
    void processRequest(connection_hdl hdl, const MessageEnvelope& message);
    void setupEventSubscriptions();
    
    server_t server_;
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections_;
    std::thread serverThread_;
    std::atomic<bool> running_;
    int port_;
    CommandCallback commandCallback_;
    
    std::shared_ptr<EventBus> eventBus_;
    std::vector<Subscription> eventSubscriptions_;
    
    mutable std::mutex connectionsMutex_;
    mutable std::mutex statsMutex_;
    Stats stats_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE ApiServer.h v4.2.7
// ============================================================================
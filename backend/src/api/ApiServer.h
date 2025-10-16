// ============================================================================
// File: backend/src/api/ApiServer.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   WebSocket API server for frontend communication.
//   Handles client connections, message routing, and event broadcasting.
//
// Features:
//   - Multi-client support
//   - Request/Response pattern
//   - Event broadcasting
//   - Thread-safe operations
//   - Statistics tracking
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Enhanced error handling
//   - Better connection management
//   - Improved statistics
//
// ============================================================================

#pragma once

#include "MessageEnvelope.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <set>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>

namespace midiMind {

/**
 * @class ApiServer
 * @brief WebSocket server for API communication
 * 
 * Provides WebSocket server functionality for frontend communication.
 * Handles multiple concurrent connections and message routing.
 * 
 * Architecture:
 * - Main thread: WebSocket server event loop
 * - Callbacks: onOpen, onClose, onMessage, onFail
 * - Command processing: Delegates to registered callback
 * - Broadcasting: Sends events to all connected clients
 * 
 * Thread Safety: YES (mutex protected)
 * 
 * Example:
 * ```cpp
 * ApiServer server;
 * 
 * // Set command handler
 * server.setCommandCallback([](const json& cmd) {
 *     std::string command = cmd["command"];
 *     json params = cmd["params"];
 *     
 *     // Process command
 *     json result = processCommand(command, params);
 *     
 *     return result;
 * });
 * 
 * // Start server
 * server.start(8080);
 * 
 * // Broadcast event
 * auto event = MessageEnvelope::createEvent(
 *     "midi:message",
 *     {{"note", 60}, {"velocity", 100}}
 * );
 * server.broadcast(event);
 * 
 * // Stop server
 * server.stop();
 * ```
 */
class ApiServer {
public:
    // ========================================================================
    // TYPE DEFINITIONS
    // ========================================================================
    
    /// WebSocket server type
    using server_t = websocketpp::server<websocketpp::config::asio>;
    
    /// Connection handle type
    using connection_hdl = websocketpp::connection_hdl;
    
    /// Message pointer type
    using message_ptr = server_t::message_ptr;
    
    /**
     * @brief Command callback function
     * 
     * Takes command JSON: {"command": "...", "params": {...}}
     * Returns result JSON: {"success": bool, "data": {...}}
     */
    using CommandCallback = std::function<json(const json&)>;
    
    /**
     * @brief Server statistics
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
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     */
    ApiServer();
    
    /**
     * @brief Destructor
     */
    ~ApiServer();
    
    // Disable copy
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;
    
    // ========================================================================
    // SERVER MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Start WebSocket server
     * @param port Port number (default: 8080)
     * @throws std::runtime_error if server fails to start
     * @note Thread-safe
     */
    void start(int port = 8080);
    
    /**
     * @brief Stop WebSocket server
     * @note Closes all connections gracefully
     * @note Thread-safe
     */
    void stop();
    
    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Get active connection count
     * @return Number of connected clients
     * @note Thread-safe
     */
    size_t getClientCount() const;
    
    /**
     * @brief Get server statistics
     * @return Stats Server statistics
     * @note Thread-safe
     */
    Stats getStats() const;
    
    // ========================================================================
    // MESSAGE SENDING
    // ========================================================================
    
    /**
     * @brief Send message to specific client
     * @param hdl Connection handle
     * @param message Message envelope
     * @return true if successful
     * @note Thread-safe
     */
    bool sendTo(connection_hdl hdl, const MessageEnvelope& message);
    
    /**
     * @brief Send error to specific client
     * @param hdl Connection handle
     * @param requestId Original request ID
     * @param code Error code
     * @param message Error message
     * @param details Additional details
     * @return true if successful
     */
    bool sendError(connection_hdl hdl,
                   const std::string& requestId,
                   protocol::ErrorCode code,
                   const std::string& message,
                   const json& details = json::object());
    
    /**
     * @brief Broadcast message to all clients
     * @param message Message envelope
     * @note Thread-safe
     */
    void broadcast(const MessageEnvelope& message);
    
    /**
     * @brief Broadcast event to all clients
     * @param name Event name
     * @param data Event data
     * @param priority Event priority
     */
    void broadcastEvent(const std::string& name,
                       const json& data,
                       protocol::EventPriority priority = protocol::EventPriority::NORMAL);
    
    // ========================================================================
    // CALLBACK REGISTRATION
    // ========================================================================
    
    /**
     * @brief Set command callback
     * @param callback Function to handle commands
     */
    void setCommandCallback(CommandCallback callback) {
        commandCallback_ = callback;
    }

private:
    // ========================================================================
    // WEBSOCKET HANDLERS
    // ========================================================================
    
    /**
     * @brief Handle new connection
     */
    void onOpen(connection_hdl hdl);
    
    /**
     * @brief Handle connection close
     */
    void onClose(connection_hdl hdl);
    
    /**
     * @brief Handle received message
     */
    void onMessage(connection_hdl hdl, message_ptr msg);
    
    /**
     * @brief Handle connection failure
     */
    void onFail(connection_hdl hdl);
    
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Server thread function
     */
    void serverThread();
    
    /**
     * @brief Process request message
     */
    void processRequest(connection_hdl hdl, const MessageEnvelope& message);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// WebSocket server instance
    server_t server_;
    
    /// Active connections
    std::set<connection_hdl, std::owner_less<connection_hdl>> connections_;
    
    /// Server thread
    std::thread serverThread_;
    
    /// Running flag
    std::atomic<bool> running_;
    
    /// Port number
    int port_;
    
    /// Command callback
    CommandCallback commandCallback_;
    
    /// Mutex for connections
    mutable std::mutex connectionsMutex_;
    
    /// Mutex for statistics
    mutable std::mutex statsMutex_;
    
    /// Server statistics
    Stats stats_;
};

} // namespace midiMind
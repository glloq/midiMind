// ============================================================================
// File: backend/src/api/ApiServer.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of ApiServer.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Complete WebSocket implementation
//   - Enhanced error handling
//   - Better connection management
//
// ============================================================================

#include "ApiServer.h"
#include "../core/Logger.h"
#include <functional>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

ApiServer::ApiServer()
    : running_(false)
    , port_(8080)
{
    Logger::info("ApiServer", "Creating WebSocket server...");
    
    // Configure WebSocket server
    server_.set_access_channels(websocketpp::log::alevel::none);
    server_.set_error_channels(websocketpp::log::elevel::none);
    
    server_.init_asio();
    server_.set_reuse_addr(true);
    
    // Set handlers
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
    
    // Initialize statistics
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
// SERVER MANAGEMENT
// ============================================================================

void ApiServer::start(int port) {
    if (running_) {
        Logger::warning("ApiServer", "Server already running");
        return;
    }
    
    Logger::info("ApiServer", 
                "Starting WebSocket server on port " + std::to_string(port));
    
    try {
        port_ = port;
        running_ = true;
        
        // Start server thread
        serverThread_ = std::thread(&ApiServer::serverThread, this);
        
        // Wait for server to be ready
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
    
    try {
        // Close all connections
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            
            for (auto& hdl : connections_) {
                try {
                    server_.close(hdl, websocketpp::close::status::going_away, 
                                 "Server shutting down");
                } catch (const std::exception& e) {
                    Logger::warning("ApiServer", 
                               "Error closing connection: " + std::string(e.what()));
                }
            }
            
            connections_.clear();
        }
        
        // Stop server
        server_.stop();
        
        // Wait for server thread
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        
        Logger::info("ApiServer", "✓ WebSocket server stopped");
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
                     "Error stopping server: " + std::string(e.what()));
    }
}

size_t ApiServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return connections_.size();
}

ApiServer::Stats ApiServer::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

// ============================================================================
// MESSAGE SENDING
// ============================================================================

bool ApiServer::sendTo(connection_hdl hdl, const MessageEnvelope& message) {
    try {
        std::string jsonStr = message.toJsonString();
        
        server_.send(hdl, jsonStr, websocketpp::frame::opcode::text);
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.messagesSent++;
        }
        
        return true;
        
    } catch (const websocketpp::exception& e) {
        Logger::error("ApiServer", 
                     "WebSocket error sending message: " + std::string(e.what()));
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.errorCount++;
        
        return false;
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
                     "Error sending message: " + std::string(e.what()));
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.errorCount++;
        
        return false;
    }
}

bool ApiServer::sendError(connection_hdl hdl,
                         const std::string& requestId,
                         protocol::ErrorCode code,
                         const std::string& message,
                         const json& details) {
    
    auto errorMsg = MessageEnvelope::createErrorResponse(
        requestId, code, message, details);
    
    return sendTo(hdl, errorMsg);
}

void ApiServer::broadcast(const MessageEnvelope& message) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    std::string jsonStr = message.toJsonString();
    
    for (auto& hdl : connections_) {
        try {
            server_.send(hdl, jsonStr, websocketpp::frame::opcode::text);
            
            std::lock_guard<std::mutex> statsLock(statsMutex_);
            stats_.messagesSent++;
            
        } catch (const std::exception& e) {
            Logger::warning("ApiServer", 
                        "Error broadcasting to client: " + std::string(e.what()));
            
            std::lock_guard<std::mutex> statsLock(statsMutex_);
            stats_.errorCount++;
        }
    }
}

void ApiServer::broadcastEvent(const std::string& name,
                              const json& data,
                              protocol::EventPriority priority) {
    
    auto event = MessageEnvelope::createEvent(name, data, priority);
    broadcast(event);
}

// ============================================================================
// WEBSOCKET HANDLERS
// ============================================================================

void ApiServer::onOpen(connection_hdl hdl) {
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.insert(hdl);
    }
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.activeConnections = connections_.size();
        stats_.totalConnections++;
    }
    
    // Get client info
    try {
        auto con = server_.get_con_from_hdl(hdl);
        std::string remote = con->get_remote_endpoint();
        
        Logger::info("ApiServer", "Client connected: " + remote);
        
    } catch (const std::exception& e) {
        Logger::warning("ApiServer", "Client connected (unknown address)");
    }
}

void ApiServer::onClose(connection_hdl hdl) {
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.erase(hdl);
    }
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.activeConnections = connections_.size();
    }
    
    Logger::info("ApiServer", "Client disconnected");
}

void ApiServer::onMessage(connection_hdl hdl, message_ptr msg) {
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.messagesReceived++;
    }
    
    try {
        std::string payload = msg->get_payload();
        
        Logger::debug("ApiServer", "Received message: " + 
                     payload.substr(0, 100) + (payload.size() > 100 ? "..." : ""));
        
        // Parse message
        auto messageOpt = MessageEnvelope::fromJsonString(payload);
        
        if (!messageOpt) {
            Logger::error("ApiServer", "Failed to parse message");
            
            sendError(hdl, "", 
                     protocol::ErrorCode::INVALID_MESSAGE,
                     "Failed to parse message");
            return;
        }
        
        auto& message = *messageOpt;
        
        // Validate message
        if (!message.isValid()) {
            Logger::error("ApiServer", "Invalid message");
            
            auto errors = message.getValidationErrors();
            json errorDetails;
            errorDetails["validation_errors"] = errors;
            
            sendError(hdl, message.getId(),
                     protocol::ErrorCode::INVALID_MESSAGE,
                     "Message validation failed",
                     errorDetails);
            return;
        }
        
        // Process based on type
        if (message.isRequest()) {
            processRequest(hdl, message);
            
        } else {
            Logger::warning("ApiServer", "Received non-request message, ignoring");
        }
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
                     "Error processing message: " + std::string(e.what()));
        
        sendError(hdl, "",
                 protocol::ErrorCode::INTERNAL_ERROR,
                 "Internal server error",
                 {{"error", e.what()}});
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.errorCount++;
    }
}

void ApiServer::onFail(connection_hdl hdl) {
    Logger::warning("ApiServer", "Connection failed");
    
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.erase(hdl);
    }
    
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.activeConnections = connections_.size();
        stats_.errorCount++;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void ApiServer::serverThread() {
    Logger::debug("ApiServer", "Server thread started");
    
    try {
        // Listen
        server_.listen(port_);
        
        // Start accepting connections
        server_.start_accept();
        
        Logger::info("ApiServer", "WebSocket server listening on port " + 
                    std::to_string(port_));
        
        // Run event loop
        server_.run();
        
    } catch (const websocketpp::exception& e) {
        Logger::error("ApiServer", 
                     "WebSocket error: " + std::string(e.what()));
        running_ = false;
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
                     "Server thread error: " + std::string(e.what()));
        running_ = false;
    }
    
    Logger::debug("ApiServer", "Server thread stopped");
}

void ApiServer::processRequest(connection_hdl hdl, const MessageEnvelope& message) {
    auto startTime = std::chrono::steady_clock::now();
    
    const protocol::Request& request = message.getRequest();
    
    Logger::debug("ApiServer", "Processing request: " + request.command + 
                 " (ID: " + request.id + ")");
    
    // Check if callback is registered
    if (!commandCallback_) {
        Logger::error("ApiServer", "No command callback registered");
        
        sendError(hdl, request.id,
                 protocol::ErrorCode::INTERNAL_ERROR,
                 "Command handler not initialized");
        return;
    }
    
    try {
        // Build command JSON for callback
        json commandJson = {
            {"command", request.command},
            {"params", request.params}
        };
        
        // Call command handler
        json result = commandCallback_(commandJson);
        
        // Calculate latency
        auto endTime = std::chrono::steady_clock::now();
        int latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        // Create response based on result
        MessageEnvelope response;
        
        if (result.value("success", false)) {
            // Success response
            json data = result.contains("data") ? result["data"] : json::object();
            
            response = MessageEnvelope::createSuccessResponse(
                request.id, data, latency);
            
            Logger::debug("ApiServer", 
                         "Command succeeded: " + request.command + 
                         " (latency: " + std::to_string(latency) + "ms)");
        } else {
            // Error response
            std::string errorMsg = result.value("error", "Command failed");
            std::string errorCodeStr = result.value("error_code", "COMMAND_FAILED");
            
            // Map error code string to enum
            protocol::ErrorCode code = protocol::ErrorCode::COMMAND_FAILED;
            
            if (errorCodeStr == "UNKNOWN_COMMAND") {
                code = protocol::ErrorCode::UNKNOWN_COMMAND;
            } else if (errorCodeStr == "INVALID_PARAMS") {
                code = protocol::ErrorCode::INVALID_PARAMS;
            } else if (errorCodeStr == "DEVICE_NOT_FOUND") {
                code = protocol::ErrorCode::DEVICE_NOT_FOUND;
            } else if (errorCodeStr == "DEVICE_BUSY") {
                code = protocol::ErrorCode::DEVICE_BUSY;
            } else if (errorCodeStr == "INTERNAL_ERROR") {
                code = protocol::ErrorCode::INTERNAL_ERROR;
            }
            
            json details = result.contains("details") ? 
                          result["details"] : json::object();
            
            response = MessageEnvelope::createErrorResponse(
                request.id, code, errorMsg, details);
            
            Logger::warning("ApiServer", 
                        "Command failed: " + request.command + 
                        " - " + errorMsg);
        }
        
        // Send response
        sendTo(hdl, response);
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
                     "Error executing command: " + std::string(e.what()));
        
        sendError(hdl, request.id,
                 protocol::ErrorCode::INTERNAL_ERROR,
                 "Command execution failed",
                 {{"error", e.what()}});
    }
}

} // namespace midiMind

// ============================================================================
// END OF FILE ApiServer.cpp
// ============================================================================
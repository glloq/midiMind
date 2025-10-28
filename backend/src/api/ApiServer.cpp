// ============================================================================
// File: backend/src/api/ApiServer.cpp
// Version: 4.2.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.0:
//   - Added EventBus integration
//   - Implemented event subscriptions
//   - Broadcasting events to WebSocket clients
//
// ============================================================================

#include "ApiServer.h"
#include "../core/Logger.h"
#include "../core/TimeUtils.h"
#include "../events/Events.h"
#include <functional>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

ApiServer::ApiServer(std::shared_ptr<EventBus> eventBus)
    : running_(false)
    , port_(8080)
    , eventBus_(eventBus)
{
    Logger::info("ApiServer", "Creating WebSocket server...");
    
    server_.set_access_channels(websocketpp::log::alevel::none);
    server_.set_error_channels(websocketpp::log::elevel::none);
    
    server_.init_asio();
    server_.set_reuse_addr(true);
    
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
    
    stats_.startTime = std::chrono::steady_clock::now();
    stats_.activeConnections = 0;
    stats_.totalConnections = 0;
    stats_.messagesSent = 0;
    stats_.messagesReceived = 0;
    stats_.errorCount = 0;
    
    if (eventBus_) {
        setupEventSubscriptions();
    }
    
    Logger::info("ApiServer", "✓ WebSocket server created");
}

ApiServer::~ApiServer() {
    stop();
}

// ============================================================================
// EVENT BUS
// ============================================================================

void ApiServer::setEventBus(std::shared_ptr<EventBus> eventBus) {
    eventBus_ = eventBus;
    eventSubscriptions_.clear();
    
    if (eventBus_) {
        setupEventSubscriptions();
    }
    
    Logger::info("ApiServer", "EventBus configured");
}

void ApiServer::setupEventSubscriptions() {
    if (!eventBus_) return;
    
    Logger::info("ApiServer", "Setting up event subscriptions...");
    
    // 1. MIDI Message Received
    eventSubscriptions_.push_back(
        eventBus_->subscribe<events::MidiMessageReceivedEvent>(
            [this](const auto& event) {
                json data = {
                    {"device_id", event.deviceId},
                    {"device_name", event.deviceName},
                    {"message", {
                        {"status", event.message.getStatus()},
                        {"data1", event.message.getData1()},
                        {"data2", event.message.getData2()}
                    }},
                    {"timestamp", event.timestamp}
                };
                auto envelope = MessageEnvelope::createEvent("midi:message:received", data);
                broadcast(envelope);
            }
        )
    );
    
    // 2. Device Connected
    eventSubscriptions_.push_back(
        eventBus_->subscribe<events::DeviceConnectedEvent>(
            [this](const auto& event) {
                json data = {
                    {"device_id", event.deviceId},
                    {"device_name", event.deviceName},
                    {"device_type", event.deviceType},
                    {"timestamp", event.timestamp}
                };
                auto envelope = MessageEnvelope::createEvent("device:connected", data);
                broadcast(envelope);
            }
        )
    );
    
    // 3. Device Disconnected
    eventSubscriptions_.push_back(
        eventBus_->subscribe<events::DeviceDisconnectedEvent>(
            [this](const auto& event) {
                json data = {
                    {"device_id", event.deviceId},
                    {"device_name", event.deviceName},
                    {"reason", event.reason},
                    {"timestamp", event.timestamp}
                };
                auto envelope = MessageEnvelope::createEvent("device:disconnected", data);
                broadcast(envelope);
            }
        )
    );
    
    // 4. Playback State Changed
    eventSubscriptions_.push_back(
        eventBus_->subscribe<events::PlaybackStateChangedEvent>(
            [this](const auto& event) {
                std::string stateStr;
                switch (event.state) {
                    case events::PlaybackStateChangedEvent::State::PLAYING:
                        stateStr = "playing";
                        break;
                    case events::PlaybackStateChangedEvent::State::PAUSED:
                        stateStr = "paused";
                        break;
                    case events::PlaybackStateChangedEvent::State::STOPPED:
                    default:
                        stateStr = "stopped";
                        break;
                }
                
                json data = {
                    {"state", stateStr},
                    {"filepath", event.filepath},
                    {"position", event.position},
                    {"timestamp", event.timestamp}
                };
                auto envelope = MessageEnvelope::createEvent("playback:state", data);
                broadcast(envelope);
            }
        )
    );
    
    // 5. Playback Progress
    eventSubscriptions_.push_back(
        eventBus_->subscribe<events::PlaybackProgressEvent>(
            [this](const auto& event) {
                json data = {
                    {"position", event.position},
                    {"duration", event.duration},
                    {"percentage", event.percentage},
                    {"timestamp", event.timestamp}
                };
                auto envelope = MessageEnvelope::createEvent("playback:progress", data);
                broadcast(envelope);
            }
        )
    );
    
    // 6. Route Added
    eventSubscriptions_.push_back(
        eventBus_->subscribe<events::RouteAddedEvent>(
            [this](const auto& event) {
                json data = {
                    {"source", event.source},
                    {"destination", event.destination},
                    {"timestamp", event.timestamp}
                };
                auto envelope = MessageEnvelope::createEvent("route:added", data);
                broadcast(envelope);
            }
        )
    );
    
    // 7. Route Removed
    eventSubscriptions_.push_back(
        eventBus_->subscribe<events::RouteRemovedEvent>(
            [this](const auto& event) {
                json data = {
                    {"source", event.source},
                    {"destination", event.destination},
                    {"timestamp", event.timestamp}
                };
                auto envelope = MessageEnvelope::createEvent("route:removed", data);
                broadcast(envelope);
            }
        )
    );
    
    Logger::info("ApiServer", "✓ Event subscriptions configured (" + 
                std::to_string(eventSubscriptions_.size()) + " events)");
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
        
        serverThread_ = std::thread(&ApiServer::serverThread, this);
        
        Logger::info("ApiServer", "✓ WebSocket server started");
        
    } catch (const std::exception& e) {
        running_ = false;
        Logger::error("ApiServer", 
                     "Failed to start server: " + std::string(e.what()));
        throw;
    }
}

void ApiServer::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("ApiServer", "Stopping WebSocket server...");
    
    running_ = false;
    
    try {
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
        
        server_.stop();
        
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
    Stats copy = stats_;
    copy.activeConnections = getClientCount();
    return copy;
}

// ============================================================================
// MESSAGE SENDING
// ============================================================================

bool ApiServer::sendTo(connection_hdl hdl, const MessageEnvelope& message) {
    try {
        std::string payload = message.toString();
        server_.send(hdl, payload, websocketpp::frame::opcode::text);
        
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.messagesSent++;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
                     "Failed to send message: " + std::string(e.what()));
        
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.errorCount++;
        }
        
        return false;
    }
}

bool ApiServer::sendError(connection_hdl hdl,
                          const std::string& requestId,
                          protocol::ErrorCode code,
                          const std::string& message,
                          const json& details) {
    auto envelope = MessageEnvelope::createErrorResponse(requestId, code, message, details);
    return sendTo(hdl, envelope);
}

void ApiServer::broadcast(const MessageEnvelope& message) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    std::string payload = message.toString();
    
    for (auto& hdl : connections_) {
        try {
            server_.send(hdl, payload, websocketpp::frame::opcode::text);
            
            {
                std::lock_guard<std::mutex> statsLock(statsMutex_);
                stats_.messagesSent++;
            }
            
        } catch (const std::exception& e) {
            Logger::warning("ApiServer", 
                          "Failed to broadcast to client: " + std::string(e.what()));
            
            {
                std::lock_guard<std::mutex> statsLock(statsMutex_);
                stats_.errorCount++;
            }
        }
    }
}

void ApiServer::broadcastEvent(const std::string& name,
                               const json& data,
                               protocol::EventPriority priority) {
    auto envelope = MessageEnvelope::createEvent(name, data, priority);
    broadcast(envelope);
}

// ============================================================================
// WEBSOCKET HANDLERS
// ============================================================================

void ApiServer::onOpen(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_.insert(hdl);
    
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.totalConnections++;
    }
    
    Logger::info("ApiServer", 
                "Client connected (total: " + 
                std::to_string(connections_.size()) + ")");
}

void ApiServer::onClose(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_.erase(hdl);
    
    Logger::info("ApiServer", 
                "Client disconnected (remaining: " + 
                std::to_string(connections_.size()) + ")");
}

void ApiServer::onMessage(connection_hdl hdl, message_ptr msg) {
    try {
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.messagesReceived++;
        }
        
        std::string payload = msg->get_payload();
        auto envelopeOpt = MessageEnvelope::fromString(payload);
        
        if (!envelopeOpt) {
            Logger::warning("ApiServer", "Failed to parse message");
            return;
        }
        
        const auto& envelope = *envelopeOpt;
        
        if (envelope.isRequest()) {
            processRequest(hdl, envelope);
        }
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
                     "Error processing message: " + std::string(e.what()));
        
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.errorCount++;
        }
    }
}

void ApiServer::onFail(connection_hdl hdl) {
    Logger::warning("ApiServer", "Connection failed");
    
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_.erase(hdl);
    
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.errorCount++;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void ApiServer::serverThread() {
    Logger::info("ApiServer", "Server thread started");
    
    try {
        server_.listen(port_);
        server_.start_accept();
        server_.run();
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
                     "Server thread error: " + std::string(e.what()));
    }
    
    Logger::info("ApiServer", "Server thread stopped");
}

void ApiServer::processRequest(connection_hdl hdl, const MessageEnvelope& message) {
    if (!commandCallback_) {
        sendError(hdl, message.getRequest().id, 
                 protocol::ErrorCode::INTERNAL_ERROR,
                 "No command handler configured");
        return;
    }
    
    try {
        const auto& request = message.getRequest();
        json result = commandCallback_(request.params);
        
        auto response = MessageEnvelope::createSuccessResponse(
            request.id,
            result
        );
        
        sendTo(hdl, response);
        
    } catch (const std::exception& e) {
        Logger::error("ApiServer", 
                     "Command processing error: " + std::string(e.what()));
        
        sendError(hdl, message.getRequest().id,
                 protocol::ErrorCode::COMMAND_FAILED,
                 "Command execution failed",
                 {{"error", e.what()}});
    }
}

} // namespace midiMind
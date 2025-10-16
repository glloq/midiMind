// ============================================================================
// File: backend/src/api/MessageEnvelope.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Message envelope wrapper for WebSocket protocol.
//   Provides type-safe message creation, validation, and parsing.
//
// Features:
//   - Request/Response/Event/Error messages
//   - JSON serialization/deserialization
//   - Message validation
//   - Factory methods
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Enhanced validation
//   - Better error handling
//   - Improved type safety
//
// ============================================================================

#pragma once

#include "Protocol.h"
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @class MessageEnvelope
 * @brief Wrapper for WebSocket messages
 * 
 * Encapsulates all WebSocket messages according to the protocol.
 * Provides type-safe creation and parsing of messages.
 * 
 * Thread Safety: YES (immutable after creation)
 * 
 * Example:
 * ```cpp
 * // Create request
 * auto msg = MessageEnvelope::createRequest("files.list", 
 *     {{"directory", "/midi"}});
 * 
 * // Serialize
 * std::string json = msg.toJsonString();
 * 
 * // Parse received message
 * auto received = MessageEnvelope::fromJsonString(json);
 * if (received && received->isRequest()) {
 *     auto& req = received->getRequest();
 *     std::cout << "Command: " << req.command << "\n";
 * }
 * 
 * // Create response
 * auto response = MessageEnvelope::createSuccessResponse(
 *     req.id, {{"files", fileList}}, 15);
 * 
 * // Create event
 * auto event = MessageEnvelope::createEvent(
 *     "midi:message", 
 *     {{"note", 60}, {"velocity", 100}},
 *     protocol::EventPriority::HIGH);
 * ```
 */
class MessageEnvelope {
public:
    // ========================================================================
    // CONSTRUCTORS
    // ========================================================================
    
    /**
     * @brief Default constructor
     */
    MessageEnvelope();
    
    /**
     * @brief Constructor with type
     * @param type Message type
     */
    explicit MessageEnvelope(protocol::MessageType type);
    
    // ========================================================================
    // FACTORY METHODS - REQUEST
    // ========================================================================
    
    /**
     * @brief Create request message
     * @param command Command name
     * @param params Parameters (JSON object)
     * @return MessageEnvelope Request message
     */
    static MessageEnvelope createRequest(
        const std::string& command,
        const json& params = json::object()
    );
    
    // ========================================================================
    // FACTORY METHODS - RESPONSE
    // ========================================================================
    
    /**
     * @brief Create success response
     * @param requestId Original request ID
     * @param data Response data
     * @param latency Latency in milliseconds
     * @return MessageEnvelope Success response
     */
    static MessageEnvelope createSuccessResponse(
        const std::string& requestId,
        const json& data,
        int latency = 0
    );
    
    /**
     * @brief Create error response
     * @param requestId Original request ID
     * @param code Error code
     * @param message Error message
     * @param details Additional details
     * @param retryable Can retry?
     * @return MessageEnvelope Error response
     */
    static MessageEnvelope createErrorResponse(
        const std::string& requestId,
        protocol::ErrorCode code,
        const std::string& message,
        const json& details = json::object(),
        bool retryable = false
    );
    
    // ========================================================================
    // FACTORY METHODS - EVENT
    // ========================================================================
    
    /**
     * @brief Create event message
     * @param name Event name
     * @param data Event data
     * @param priority Event priority
     * @return MessageEnvelope Event message
     */
    static MessageEnvelope createEvent(
        const std::string& name,
        const json& data,
        protocol::EventPriority priority = protocol::EventPriority::NORMAL
    );
    
    // ========================================================================
    // FACTORY METHODS - ERROR
    // ========================================================================
    
    /**
     * @brief Create standalone error (not linked to request)
     * @param code Error code
     * @param message Error message
     * @param details Additional details
     * @return MessageEnvelope Error message
     */
    static MessageEnvelope createError(
        protocol::ErrorCode code,
        const std::string& message,
        const json& details = json::object()
    );
    
    // ========================================================================
    // PARSING
    // ========================================================================
    
    /**
     * @brief Parse message from JSON string
     * @param jsonStr JSON string
     * @return std::optional<MessageEnvelope> Parsed message or nullopt
     */
    static std::optional<MessageEnvelope> fromJsonString(const std::string& jsonStr);
    
    /**
     * @brief Parse message from JSON object
     * @param j JSON object
     * @return std::optional<MessageEnvelope> Parsed message or nullopt
     */
    static std::optional<MessageEnvelope> fromJson(const json& j);
    
    // ========================================================================
    // SERIALIZATION
    // ========================================================================
    
    /**
     * @brief Convert to JSON object
     * @return json JSON representation
     */
    json toJson() const;
    
    /**
     * @brief Convert to JSON string
     * @param indent Indentation level (-1 = compact)
     * @return std::string JSON string
     */
    std::string toJsonString(int indent = -1) const;
    
    // ========================================================================
    // TYPE CHECKING
    // ========================================================================
    
    /**
     * @brief Check if request message
     */
    bool isRequest() const { return envelope_.type == protocol::MessageType::REQUEST; }
    
    /**
     * @brief Check if response message
     */
    bool isResponse() const { return envelope_.type == protocol::MessageType::RESPONSE; }
    
    /**
     * @brief Check if event message
     */
    bool isEvent() const { return envelope_.type == protocol::MessageType::EVENT; }
    
    /**
     * @brief Check if error message
     */
    bool isError() const { return envelope_.type == protocol::MessageType::ERROR; }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * @brief Get envelope
     */
    const protocol::Envelope& getEnvelope() const { return envelope_; }
    
    /**
     * @brief Get request (throws if not request)
     */
    const protocol::Request& getRequest() const;
    
    /**
     * @brief Get response (throws if not response)
     */
    const protocol::Response& getResponse() const;
    
    /**
     * @brief Get event (throws if not event)
     */
    const protocol::Event& getEvent() const;
    
    /**
     * @brief Get error (throws if not error)
     */
    const protocol::Error& getError() const;
    
    /**
     * @brief Get message ID
     */
    std::string getId() const { return envelope_.id; }
    
    /**
     * @brief Get message type
     */
    protocol::MessageType getType() const { return envelope_.type; }
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Validate message
     * @return true if valid
     */
    bool isValid() const;
    
    /**
     * @brief Get validation errors
     * @return std::vector<std::string> List of errors
     */
    std::vector<std::string> getValidationErrors() const;

private:
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Message envelope (header)
    protocol::Envelope envelope_;
    
    /// Request payload (if type == REQUEST)
    std::optional<protocol::Request> request_;
    
    /// Response payload (if type == RESPONSE)
    std::optional<protocol::Response> response_;
    
    /// Event payload (if type == EVENT)
    std::optional<protocol::Event> event_;
    
    /// Error payload (if type == ERROR)
    std::optional<protocol::Error> error_;
};

} // namespace midiMind
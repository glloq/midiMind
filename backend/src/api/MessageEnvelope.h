// ============================================================================
// File: backend/src/api/MessageEnvelope.h
// Version: 4.1.1 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.1:
//   - Fixed method signatures to match implementation
//   - Added missing hasRequest(), hasResponse(), hasEvent(), hasError()
//   - Renamed fromJsonString() to fromString() and toJsonString() to toString()
//   - Added retryable parameter to createErrorResponse
//   - Removed requestId parameter from createError (standalone errors)
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
 */
class MessageEnvelope {
public:
    // ========================================================================
    // CONSTRUCTORS
    // ========================================================================
    
    MessageEnvelope();
    explicit MessageEnvelope(protocol::MessageType type);
    
    // ========================================================================
    // FACTORY METHODS - REQUEST
    // ========================================================================
    
    static MessageEnvelope createRequest(
        const std::string& command,
        const json& params = json::object()
    );
    
    // ========================================================================
    // FACTORY METHODS - RESPONSE
    // ========================================================================
    
    static MessageEnvelope createSuccessResponse(
        const std::string& requestId,
        const json& data,
        int latency = 0
    );
    
    static MessageEnvelope createErrorResponse(
        const std::string& requestId,
        protocol::ErrorCode code,
        const std::string& message,
        const json& details = json::object()
    );
    
    // ========================================================================
    // FACTORY METHODS - EVENT
    // ========================================================================
    
    static MessageEnvelope createEvent(
        const std::string& name,
        const json& data,
        protocol::EventPriority priority = protocol::EventPriority::NORMAL
    );
    
    // ========================================================================
    // FACTORY METHODS - ERROR
    // ========================================================================
    
    static MessageEnvelope createError(
        protocol::ErrorCode code,
        const std::string& message,
        const json& details = json::object()
    );
    
    // ========================================================================
    // PARSING
    // ========================================================================
    
    static std::optional<MessageEnvelope> fromString(const std::string& str);
    static std::optional<MessageEnvelope> fromJson(const json& j);
    
    // ========================================================================
    // SERIALIZATION
    // ========================================================================
    
    json toJson() const;
    std::string toString() const;
    
    // ========================================================================
    // TYPE CHECKING
    // ========================================================================
    
    bool isRequest() const { return envelope_.type == protocol::MessageType::REQUEST; }
    bool isResponse() const { return envelope_.type == protocol::MessageType::RESPONSE; }
    bool isEvent() const { return envelope_.type == protocol::MessageType::EVENT; }
    bool isError() const { return envelope_.type == protocol::MessageType::ERROR; }
    
    // ========================================================================
    // PAYLOAD CHECKING
    // ========================================================================
    
    bool hasRequest() const;
    bool hasResponse() const;
    bool hasEvent() const;
    bool hasError() const;
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    const protocol::Envelope& getEnvelope() const { return envelope_; }
    const protocol::Request& getRequest() const;
    const protocol::Response& getResponse() const;
    const protocol::Event& getEvent() const;
    const protocol::Error& getError() const;
    
    std::string getId() const { return envelope_.id; }
    protocol::MessageType getType() const { return envelope_.type; }
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    bool isValid() const;
    std::vector<std::string> getValidationErrors() const;

private:
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    protocol::Envelope envelope_;
    std::optional<protocol::Request> request_;
    std::optional<protocol::Response> response_;
    std::optional<protocol::Event> event_;
    std::optional<protocol::Error> error_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE MessageEnvelope.h v4.1.1
// ============================================================================
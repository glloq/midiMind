// ============================================================================
// File: backend/src/api/MessageEnvelope.h
// Version: 4.1.2 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.2:
//   - Added noexcept to type checking methods
//   - Added @throws documentation to getters
//   - Added noexcept to payload checking methods
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
    
    bool isRequest() const noexcept { return envelope_.type == protocol::MessageType::REQUEST; }
    bool isResponse() const noexcept { return envelope_.type == protocol::MessageType::RESPONSE; }
    bool isEvent() const noexcept { return envelope_.type == protocol::MessageType::EVENT; }
    bool isError() const noexcept { return envelope_.type == protocol::MessageType::ERROR; }
    
    // ========================================================================
    // PAYLOAD CHECKING
    // ========================================================================
    
    bool hasRequest() const noexcept;
    bool hasResponse() const noexcept;
    bool hasEvent() const noexcept;
    bool hasError() const noexcept;
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    const protocol::Envelope& getEnvelope() const noexcept { return envelope_; }
    
    /**
     * @brief Get request payload
     * @return Request payload
     * @throws std::runtime_error if message does not contain a request
     */
    const protocol::Request& getRequest() const;
    
    /**
     * @brief Get response payload
     * @return Response payload
     * @throws std::runtime_error if message does not contain a response
     */
    const protocol::Response& getResponse() const;
    
    /**
     * @brief Get event payload
     * @return Event payload
     * @throws std::runtime_error if message does not contain an event
     */
    const protocol::Event& getEvent() const;
    
    /**
     * @brief Get error payload
     * @return Error payload
     * @throws std::runtime_error if message does not contain an error
     */
    const protocol::Error& getError() const;
    
    std::string getId() const noexcept { return envelope_.id; }
    protocol::MessageType getType() const noexcept { return envelope_.type; }
    
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
// END OF FILE MessageEnvelope.h v4.1.2
// ============================================================================
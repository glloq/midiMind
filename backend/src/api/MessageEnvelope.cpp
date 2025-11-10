// ============================================================================
// File: backend/src/api/MessageEnvelope.cpp
// Version: 4.2.3
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.3:
//   - FIXED: Request ID synchronization - envelope.id copied to request.id if empty
//
// Changes v4.1.3:
//   - Format unique plat: {id, type, timestamp, version, payload}
//   - Suppression du format nested legacy
//   - Added noexcept to payload checking methods
//
// ============================================================================

#include "MessageEnvelope.h"
#include "../core/Logger.h"

namespace midiMind {

// ============================================================================
// CONSTRUCTORS
// ============================================================================

MessageEnvelope::MessageEnvelope()
    : MessageEnvelope(protocol::MessageType::REQUEST)
{
}

MessageEnvelope::MessageEnvelope(protocol::MessageType type) {
    envelope_.type = type;
    envelope_.id = protocol::generateUUID();
    envelope_.timestamp = protocol::getISO8601Timestamp();
}

// ============================================================================
// FACTORY METHODS - REQUEST
// ============================================================================

MessageEnvelope MessageEnvelope::createRequest(const std::string& command,
                                              const json& params) {
    MessageEnvelope msg(protocol::MessageType::REQUEST);
    
    protocol::Request request;
    request.id = msg.envelope_.id;
    request.command = command;
    request.params = params;
    
    msg.request_ = request;
    
    return msg;
}

// ============================================================================
// FACTORY METHODS - RESPONSE
// ============================================================================

MessageEnvelope MessageEnvelope::createSuccessResponse(const std::string& requestId,
                                                       const json& data,
                                                       int latency) {
    MessageEnvelope msg(protocol::MessageType::RESPONSE);
    
    protocol::Response response;
    response.requestId = requestId;
    response.success = true;
    response.data = data;
    response.latency = latency;
    
    msg.response_ = response;
    
    return msg;
}

MessageEnvelope MessageEnvelope::createErrorResponse(const std::string& requestId,
                                                     protocol::ErrorCode code,
                                                     const std::string& message,
                                                     const json& details) {
    MessageEnvelope msg(protocol::MessageType::RESPONSE);
    
    protocol::Response response;
    response.requestId = requestId;
    response.success = false;
    response.errorCode = code;
    response.errorMessage = message;
    response.data = details;
    
    msg.response_ = response;
    
    return msg;
}

// ============================================================================
// FACTORY METHODS - EVENT
// ============================================================================

MessageEnvelope MessageEnvelope::createEvent(const std::string& name,
                                            const json& data,
                                            protocol::EventPriority priority) {
    MessageEnvelope msg(protocol::MessageType::EVENT);
    
    protocol::Event event;
    event.name = name;
    event.data = data;
    event.priority = priority;
    
    msg.event_ = event;
    
    return msg;
}

// ============================================================================
// FACTORY METHODS - ERROR
// ============================================================================

MessageEnvelope MessageEnvelope::createError(protocol::ErrorCode code,
                                            const std::string& message,
                                            const json& details) {
    MessageEnvelope msg(protocol::MessageType::ERROR);
    
    protocol::Error error;
    error.code = code;
    error.message = message;
    error.details = details;
    
    msg.error_ = error;
    
    return msg;
}

// ============================================================================
// GETTERS
// ============================================================================

const protocol::Request& MessageEnvelope::getRequest() const {
    if (!request_.has_value()) {
        throw std::runtime_error("Message does not contain a request");
    }
    return *request_;
}

const protocol::Response& MessageEnvelope::getResponse() const {
    if (!response_.has_value()) {
        throw std::runtime_error("Message does not contain a response");
    }
    return *response_;
}

const protocol::Event& MessageEnvelope::getEvent() const {
    if (!event_.has_value()) {
        throw std::runtime_error("Message does not contain an event");
    }
    return *event_;
}

const protocol::Error& MessageEnvelope::getError() const {
    if (!error_.has_value()) {
        throw std::runtime_error("Message does not contain an error");
    }
    return *error_;
}

bool MessageEnvelope::hasRequest() const noexcept {
    return request_.has_value();
}

bool MessageEnvelope::hasResponse() const noexcept {
    return response_.has_value();
}

bool MessageEnvelope::hasEvent() const noexcept {
    return event_.has_value();
}

bool MessageEnvelope::hasError() const noexcept {
    return error_.has_value();
}

// ============================================================================
// SERIALIZATION - FORMAT PLAT UNIQUE
// ============================================================================

std::optional<MessageEnvelope> MessageEnvelope::fromJson(const json& j) {
    try {
        if (!j.is_object()) {
            Logger::error("MessageEnvelope", "JSON is not an object");
            return std::nullopt;
        }
        
        // Format plat: {id, type, timestamp, version, payload}
        if (!j.contains("id") || !j.contains("type") || !j.contains("payload")) {
            Logger::error("MessageEnvelope", "Invalid message format (missing Envelope fields)");
            return std::nullopt;
        }
        
        MessageEnvelope msg;
        
        msg.envelope_.id = j.value("id", "");
        msg.envelope_.type = protocol::stringToMessageType(j.value("type", "request"));
        msg.envelope_.timestamp = j.value("timestamp", "");
        msg.envelope_.version = j.value("version", "1.0");
        
        const json& payload = j["payload"];
        
        // Parse payload based on type
        switch (msg.envelope_.type) {
            case protocol::MessageType::REQUEST:
                msg.request_ = protocol::Request::fromJson(payload);
                // CRITICAL FIX: Synchronize request ID with envelope ID
                if (msg.request_->id.empty()) {
                    msg.request_->id = msg.envelope_.id;
                }
                break;
                
            case protocol::MessageType::RESPONSE:
                msg.response_ = protocol::Response::fromJson(payload);
                break;
                
            case protocol::MessageType::EVENT:
                msg.event_ = protocol::Event::fromJson(payload);
                break;
                
            case protocol::MessageType::ERROR:
                msg.error_ = protocol::Error::fromJson(payload);
                break;
        }
        
        return msg;
        
    } catch (const std::exception& e) {
        Logger::error("MessageEnvelope", 
                     "Failed to parse JSON: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<MessageEnvelope> MessageEnvelope::fromString(const std::string& str) {
    try {
        json j = json::parse(str);
        return fromJson(j);
        
    } catch (const json::parse_error& e) {
        Logger::error("MessageEnvelope", 
                     "JSON parse error: " + std::string(e.what()));
        return std::nullopt;
    }
}

json MessageEnvelope::toJson() const {
    json j;
    
    // Format plat: {id, type, timestamp, version, payload}
    j["id"] = envelope_.id;
    j["type"] = protocol::messageTypeToString(envelope_.type);
    j["timestamp"] = envelope_.timestamp;
    j["version"] = envelope_.version;
    
    // Add payload based on type
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            if (request_.has_value()) {
                j["payload"] = request_->toJson();
            }
            break;
            
        case protocol::MessageType::RESPONSE:
            if (response_.has_value()) {
                j["payload"] = response_->toJson();
            }
            break;
            
        case protocol::MessageType::EVENT:
            if (event_.has_value()) {
                j["payload"] = event_->toJson();
            }
            break;
            
        case protocol::MessageType::ERROR:
            if (error_.has_value()) {
                j["payload"] = error_->toJson();
            }
            break;
    }
    
    return j;
}

std::string MessageEnvelope::toString() const {
    try {
        return toJson().dump();
    } catch (const std::exception& e) {
        Logger::error("MessageEnvelope", 
                     "Failed to serialize to string: " + std::string(e.what()));
        return "{}";
    }
}

// ============================================================================
// VALIDATION
// ============================================================================

bool MessageEnvelope::isValid() const {
    // Check envelope
    if (envelope_.id.empty()) {
        return false;
    }
    
    if (envelope_.timestamp.empty()) {
        return false;
    }
    
    // Check payload exists for type
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            return request_.has_value();
            
        case protocol::MessageType::RESPONSE:
            return response_.has_value();
            
        case protocol::MessageType::EVENT:
            return event_.has_value();
            
        case protocol::MessageType::ERROR:
            return error_.has_value();
    }
    
    return false;
}

std::vector<std::string> MessageEnvelope::getValidationErrors() const {
    std::vector<std::string> errors;
    
    // Validate envelope
    if (envelope_.id.empty()) {
        errors.push_back("Envelope ID is empty");
    }
    
    if (envelope_.timestamp.empty()) {
        errors.push_back("Envelope timestamp is empty");
    }
    
    if (envelope_.version.empty()) {
        errors.push_back("Envelope version is empty");
    }
    
    // Validate payload
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            if (!request_.has_value()) {
                errors.push_back("REQUEST type but no request payload");
            } else {
                if (request_->command.empty()) {
                    errors.push_back("Request command is empty");
                }
                if (!request_->params.is_object()) {
                    errors.push_back("Request params must be an object");
                }
            }
            break;
            
        case protocol::MessageType::RESPONSE:
            if (!response_.has_value()) {
                errors.push_back("RESPONSE type but no response payload");
            } else {
                if (response_->requestId.empty()) {
                    errors.push_back("Response requestId is empty");
                }
            }
            break;
            
        case protocol::MessageType::EVENT:
            if (!event_.has_value()) {
                errors.push_back("EVENT type but no event payload");
            } else {
                if (event_->name.empty()) {
                    errors.push_back("Event name is empty");
                }
                if (!event_->data.is_object()) {
                    errors.push_back("Event data must be an object");
                }
            }
            break;
            
        case protocol::MessageType::ERROR:
            if (!error_.has_value()) {
                errors.push_back("ERROR type but no error payload");
            } else {
                if (error_->message.empty()) {
                    errors.push_back("Error message is empty");
                }
            }
            break;
    }
    
    return errors;
}

} // namespace midiMind

// ============================================================================
// END OF FILE MessageEnvelope.cpp v4.2.3
// ============================================================================
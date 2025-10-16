// ============================================================================
// File: backend/src/api/MessageEnvelope.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of MessageEnvelope.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Complete implementation
//   - Enhanced validation
//   - Better error handling
//
// ============================================================================

#include "MessageEnvelope.h"
#include "../core/Logger.h"

namespace midiMind {

// ============================================================================
// CONSTRUCTORS
// ============================================================================

MessageEnvelope::MessageEnvelope()
    : envelope_()
{
    envelope_.type = protocol::MessageType::REQUEST;
}

MessageEnvelope::MessageEnvelope(protocol::MessageType type)
    : envelope_()
{
    envelope_.type = type;
    envelope_.id = protocol::Envelope::generateUUID();
    envelope_.timestamp = std::chrono::system_clock::now();
    envelope_.version = "1.0";
}

// ============================================================================
// FACTORY METHODS - REQUEST
// ============================================================================

MessageEnvelope MessageEnvelope::createRequest(
    const std::string& command,
    const json& params)
{
    MessageEnvelope msg(protocol::MessageType::REQUEST);
    
    protocol::Request req;
    req.id = msg.envelope_.id;
    req.command = command;
    req.params = params;
    
    msg.request_ = req;
    
    return msg;
}

// ============================================================================
// FACTORY METHODS - RESPONSE
// ============================================================================

MessageEnvelope MessageEnvelope::createSuccessResponse(
    const std::string& requestId,
    const json& data,
    int latency)
{
    MessageEnvelope msg(protocol::MessageType::RESPONSE);
    
    protocol::Response resp;
    resp.requestId = requestId;
    resp.success = true;
    resp.data = data;
    resp.latency = latency;
    
    msg.response_ = resp;
    
    return msg;
}

MessageEnvelope MessageEnvelope::createErrorResponse(
    const std::string& requestId,
    protocol::ErrorCode code,
    const std::string& message,
    const json& details,
    bool retryable)
{
    MessageEnvelope msg(protocol::MessageType::ERROR);
    
    protocol::Error err;
    err.requestId = requestId;
    err.code = code;
    err.message = message;
    err.details = details;
    err.retryable = retryable;
    
    msg.error_ = err;
    
    return msg;
}

// ============================================================================
// FACTORY METHODS - EVENT
// ============================================================================

MessageEnvelope MessageEnvelope::createEvent(
    const std::string& name,
    const json& data,
    protocol::EventPriority priority)
{
    MessageEnvelope msg(protocol::MessageType::EVENT);
    
    protocol::Event evt;
    evt.name = name;
    evt.data = data;
    evt.priority = priority;
    
    msg.event_ = evt;
    
    return msg;
}

// ============================================================================
// FACTORY METHODS - ERROR
// ============================================================================

MessageEnvelope MessageEnvelope::createError(
    protocol::ErrorCode code,
    const std::string& message,
    const json& details)
{
    MessageEnvelope msg(protocol::MessageType::ERROR);
    
    protocol::Error err;
    err.code = code;
    err.message = message;
    err.details = details;
    err.retryable = false;
    
    msg.error_ = err;
    
    return msg;
}

// ============================================================================
// PARSING
// ============================================================================

std::optional<MessageEnvelope> MessageEnvelope::fromJsonString(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        return fromJson(j);
    }
    catch (const json::parse_error& e) {
        Logger::error("MessageEnvelope", "JSON parse error: " + std::string(e.what()));
        return std::nullopt;
    }
    catch (const std::exception& e) {
        Logger::error("MessageEnvelope", "Error parsing message: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<MessageEnvelope> MessageEnvelope::fromJson(const json& j) {
    try {
        // Check for required envelope field
        if (!j.contains("envelope")) {
            Logger::error("MessageEnvelope", "Missing 'envelope' field");
            return std::nullopt;
        }
        
        // Parse envelope
        MessageEnvelope msg;
        msg.envelope_ = protocol::Envelope::fromJson(j["envelope"]);
        
        // Parse content based on type
        switch (msg.envelope_.type) {
            case protocol::MessageType::REQUEST:
                if (!j.contains("request")) {
                    Logger::error("MessageEnvelope", "Missing 'request' field for REQUEST type");
                    return std::nullopt;
                }
                msg.request_ = protocol::Request::fromJson(j["request"]);
                break;
                
            case protocol::MessageType::RESPONSE:
                if (!j.contains("response")) {
                    Logger::error("MessageEnvelope", "Missing 'response' field for RESPONSE type");
                    return std::nullopt;
                }
                msg.response_ = protocol::Response::fromJson(j["response"]);
                break;
                
            case protocol::MessageType::EVENT:
                if (!j.contains("event")) {
                    Logger::error("MessageEnvelope", "Missing 'event' field for EVENT type");
                    return std::nullopt;
                }
                msg.event_ = protocol::Event::fromJson(j["event"]);
                break;
                
            case protocol::MessageType::ERROR:
                if (!j.contains("error")) {
                    Logger::error("MessageEnvelope", "Missing 'error' field for ERROR type");
                    return std::nullopt;
                }
                msg.error_ = protocol::Error::fromJson(j["error"]);
                break;
        }
        
        // Validate
        if (!msg.isValid()) {
            Logger::error("MessageEnvelope", "Message validation failed");
            auto errors = msg.getValidationErrors();
            for (const auto& err : errors) {
                Logger::error("MessageEnvelope", "  - " + err);
            }
            return std::nullopt;
        }
        
        return msg;
    }
    catch (const std::exception& e) {
        Logger::error("MessageEnvelope", 
            "Error creating message from JSON: " + std::string(e.what()));
        return std::nullopt;
    }
}

// ============================================================================
// SERIALIZATION
// ============================================================================

json MessageEnvelope::toJson() const {
    json j;
    
    // Envelope
    j["envelope"] = envelope_.toJson();
    
    // Content based on type
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            if (request_) {
                j["request"] = request_->toJson();
            }
            break;
            
        case protocol::MessageType::RESPONSE:
            if (response_) {
                j["response"] = response_->toJson();
            }
            break;
            
        case protocol::MessageType::EVENT:
            if (event_) {
                j["event"] = event_->toJson();
            }
            break;
            
        case protocol::MessageType::ERROR:
            if (error_) {
                j["error"] = error_->toJson();
            }
            break;
    }
    
    return j;
}

std::string MessageEnvelope::toJsonString(int indent) const {
    return toJson().dump(indent);
}

// ============================================================================
// GETTERS
// ============================================================================

const protocol::Request& MessageEnvelope::getRequest() const {
    if (!request_) {
        throw std::runtime_error("Message is not a REQUEST");
    }
    return *request_;
}

const protocol::Response& MessageEnvelope::getResponse() const {
    if (!response_) {
        throw std::runtime_error("Message is not a RESPONSE");
    }
    return *response_;
}

const protocol::Event& MessageEnvelope::getEvent() const {
    if (!event_) {
        throw std::runtime_error("Message is not an EVENT");
    }
    return *event_;
}

const protocol::Error& MessageEnvelope::getError() const {
    if (!error_) {
        throw std::runtime_error("Message is not an ERROR");
    }
    return *error_;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool MessageEnvelope::isValid() const {
    // Check envelope
    if (envelope_.id.empty()) {
        return false;
    }
    
    if (envelope_.version.empty()) {
        return false;
    }
    
    // Check content based on type
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            if (!request_) {
                return false;
            }
            if (request_->command.empty()) {
                return false;
            }
            break;
            
        case protocol::MessageType::RESPONSE:
            if (!response_) {
                return false;
            }
            if (response_->requestId.empty()) {
                return false;
            }
            break;
            
        case protocol::MessageType::EVENT:
            if (!event_) {
                return false;
            }
            if (event_->name.empty()) {
                return false;
            }
            break;
            
        case protocol::MessageType::ERROR:
            if (!error_) {
                return false;
            }
            if (error_->message.empty()) {
                return false;
            }
            break;
    }
    
    return true;
}

std::vector<std::string> MessageEnvelope::getValidationErrors() const {
    std::vector<std::string> errors;
    
    // Validate envelope
    if (envelope_.id.empty()) {
        errors.push_back("Envelope ID is empty");
    }
    
    if (envelope_.version.empty()) {
        errors.push_back("Envelope version is empty");
    }
    
    // Validate content based on type
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            if (!request_) {
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
            if (!response_) {
                errors.push_back("RESPONSE type but no response payload");
            } else {
                if (response_->requestId.empty()) {
                    errors.push_back("Response requestId is empty");
                }
                if (response_->success && !response_->data.is_object()) {
                    errors.push_back("Response data must be an object for success responses");
                }
            }
            break;
            
        case protocol::MessageType::EVENT:
            if (!event_) {
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
            if (!error_) {
                errors.push_back("ERROR type but no error payload");
            } else {
                if (error_->message.empty()) {
                    errors.push_back("Error message is empty");
                }
                if (!error_->details.is_object()) {
                    errors.push_back("Error details must be an object");
                }
            }
            break;
    }
    
    return errors;
}

} // namespace midiMind

// ============================================================================
// END OF FILE MessageEnvelope.cpp
// ============================================================================
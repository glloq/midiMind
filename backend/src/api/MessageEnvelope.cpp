// ============================================================================
// Fichier: backend/src/api/MessageEnvelope.cpp
// Version: 3.0.2 - CORRECTION DES MÉTHODES
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - Implémentation de toJsonString(int indent)
// - Implémentation de validate(std::vector<std::string>& errors)
// - Utilisation des méthodes publiques de Protocol.h
// ============================================================================

#include "MessageEnvelope.h"
#include "../core/Logger.h"

namespace midiMind {

// ========================================================================
// CONSTRUCTEURS
// ========================================================================

MessageEnvelope::MessageEnvelope()
    : validationCached_(false)
    , validationResult_(false) {
}

MessageEnvelope::MessageEnvelope(protocol::MessageType type)
    : validationCached_(false)
    , validationResult_(false) {
    envelope_.type = type;
}

// ========================================================================
// FACTORY METHODS
// ========================================================================

MessageEnvelope MessageEnvelope::createRequest(
    const std::string& command,
    const json& params)
{
    MessageEnvelope msg(protocol::MessageType::REQUEST);
    
    protocol::Request req;
    req.command = command;
    req.params = params;
    
    msg.request_ = req;
    
    return msg;
}

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

// ========================================================================
// PARSING
// ========================================================================

std::optional<MessageEnvelope> MessageEnvelope::fromJson(const json& j) {
    try {
        MessageEnvelope msg;
        
        if (!j.contains("envelope")) {
            Logger::error("MessageEnvelope", "Missing 'envelope' field");
            return std::nullopt;
        }
        
        msg.envelope_ = protocol::Envelope::fromJson(j["envelope"]);
        
        // ✅ UTILISATION CORRECTE: méthode publique
        std::string typeStr = j["envelope"].value("type", "request");
        protocol::MessageType type = protocol::Envelope::stringToMessageType(typeStr);
        msg.envelope_.type = type;
        
        switch (type) {
            case protocol::MessageType::REQUEST:
                if (j.contains("request")) {
                    msg.request_ = protocol::Request::fromJson(j["request"]);
                }
                break;
                
            case protocol::MessageType::RESPONSE:
                if (j.contains("response")) {
                    msg.response_ = protocol::Response::fromJson(j["response"]);
                }
                break;
                
            case protocol::MessageType::EVENT:
                if (j.contains("event")) {
                    auto evt = protocol::Event::fromJson(j["event"]);
                    // ✅ UTILISATION CORRECTE: méthode publique
                    if (j["event"].contains("priority")) {
                        std::string priorityStr = j["event"]["priority"];
                        evt.priority = protocol::Event::stringToPriority(priorityStr);
                    }
                    msg.event_ = evt;
                }
                break;
                
            case protocol::MessageType::ERROR:
                if (j.contains("error")) {
                    msg.error_ = protocol::Error::fromJson(j["error"]);
                }
                break;
        }
        
        return msg;
        
    } catch (const std::exception& e) {
        Logger::error("MessageEnvelope", "Parse failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<MessageEnvelope> MessageEnvelope::fromJsonString(const std::string& str) {
    try {
        json j = json::parse(str);
        return fromJson(j);
    } catch (const std::exception& e) {
        Logger::error("MessageEnvelope", "JSON parse failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

// ========================================================================
// SERIALIZATION
// ========================================================================

json MessageEnvelope::toJson() const {
    json j;
    
    // Enveloppe
    j["envelope"] = {
        {"version", envelope_.version},
        {"id", envelope_.id},
        {"timestamp", envelope_.timestamp},
        // ✅ UTILISATION CORRECTE: méthode publique
        {"type", protocol::Envelope::messageTypeToString(envelope_.type)}
    };
    
    // Contenu selon le type
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
                json eventJson = event_->toJson();
                // ✅ UTILISATION CORRECTE: méthode publique
                eventJson["priority"] = protocol::Event::priorityToString(event_->priority);
                j["event"] = eventJson;
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

/**
 * @brief ✅ IMPLÉMENTATION: toJsonString avec paramètre indent
 */
std::string MessageEnvelope::toJsonString(int indent) const {
    json j = toJson();
    return j.dump(indent);
}

// ========================================================================
// VALIDATION
// ========================================================================

/**
 * @brief ✅ IMPLÉMENTATION: validate avec vecteur d'erreurs
 */
bool MessageEnvelope::validate(std::vector<std::string>& errors) const {
    errors.clear();
    
    // Valider l'enveloppe
    if (envelope_.version.empty()) {
        errors.push_back("Version is empty");
    }
    
    if (envelope_.id.empty()) {
        errors.push_back("ID is empty");
    }
    
    if (envelope_.timestamp <= 0) {
        errors.push_back("Timestamp is invalid");
    }
    
    // Valider le contenu selon le type
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            if (!request_) {
                errors.push_back("Request content is missing");
            } else if (request_->command.empty()) {
                errors.push_back("Command is empty");
            }
            break;
            
        case protocol::MessageType::RESPONSE:
            if (!response_) {
                errors.push_back("Response content is missing");
            } else if (response_->requestId.empty()) {
                errors.push_back("RequestId is empty");
            }
            break;
            
        case protocol::MessageType::EVENT:
            if (!event_) {
                errors.push_back("Event content is missing");
            } else if (event_->name.empty()) {
                errors.push_back("Event name is empty");
            }
            break;
            
        case protocol::MessageType::ERROR:
            if (!error_) {
                errors.push_back("Error content is missing");
            } else if (error_->message.empty()) {
                errors.push_back("Error message is empty");
            }
            break;
    }
    
    return errors.empty();
}

bool MessageEnvelope::isValid() const {
    if (!validationCached_) {
        std::vector<std::string> errors;
        validationResult_ = validate(errors);
        validationErrors_ = errors;
        validationCached_ = true;
    }
    return validationResult_;
}

std::vector<std::string> MessageEnvelope::getValidationErrors() const {
    if (!validationCached_) {
        isValid(); // Forcer la validation
    }
    return validationErrors_;
}

// ========================================================================
// ACCESSEURS TYPE
// ========================================================================

bool MessageEnvelope::isRequest() const {
    return envelope_.type == protocol::MessageType::REQUEST && request_.has_value();
}

bool MessageEnvelope::isResponse() const {
    return envelope_.type == protocol::MessageType::RESPONSE && response_.has_value();
}

bool MessageEnvelope::isEvent() const {
    return envelope_.type == protocol::MessageType::EVENT && event_.has_value();
}

bool MessageEnvelope::isError() const {
    return envelope_.type == protocol::MessageType::ERROR && error_.has_value();
}

// ========================================================================
// ACCESSEURS CONTENU
// ========================================================================

const protocol::Request& MessageEnvelope::getRequest() const {
    if (!request_) {
        throw std::runtime_error("Not a request message");
    }
    return *request_;
}

const protocol::Response& MessageEnvelope::getResponse() const {
    if (!response_) {
        throw std::runtime_error("Not a response message");
    }
    return *response_;
}

const protocol::Event& MessageEnvelope::getEvent() const {
    if (!event_) {
        throw std::runtime_error("Not an event message");
    }
    return *event_;
}

const protocol::Error& MessageEnvelope::getError() const {
    if (!error_) {
        throw std::runtime_error("Not an error message");
    }
    return *error_;
}

// ========================================================================
// HELPERS
// ========================================================================

int MessageEnvelope::getLatencySinceCreation() const {
    auto now = std::chrono::system_clock::now();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    return static_cast<int>(nowMs - envelope_.timestamp);
}

// ========================================================================
// MÉTHODES PRIVÉES
// ========================================================================

bool MessageEnvelope::validateEnvelope() const {
    return !envelope_.version.empty() &&
           !envelope_.id.empty() &&
           envelope_.timestamp > 0;
}

bool MessageEnvelope::validateContent() const {
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            return request_ && !request_->command.empty();
            
        case protocol::MessageType::RESPONSE:
            return response_ && !response_->requestId.empty();
            
        case protocol::MessageType::EVENT:
            return event_ && !event_->name.empty();
            
        case protocol::MessageType::ERROR:
            return error_ && !error_->message.empty();
            
        default:
            return false;
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MessageEnvelope.cpp
// ============================================================================

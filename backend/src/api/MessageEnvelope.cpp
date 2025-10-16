// ============================================================================
// Fichier: backend/src/api/MessageEnvelope.cpp
// Version: 3.1.1 - CORRECTION REDÉFINITIONS
// Date: 2025-10-16
// ============================================================================
// Description:
//   Implémentation de la classe MessageEnvelope.
//   Gère création, validation et parsing des messages WebSocket.
//   Protocole v3.0 avec support REQUEST/RESPONSE/EVENT/ERROR
//
// CORRECTIONS v3.1.1:
//   ✅ SUPPRESSION des 4 méthodes redéfinies (déjà inline dans .h):
//      - getEnvelope() const
//      - getId() const
//      - getTimestamp() const
//      - getType() const
//   ✅ Conservation de toutes les autres fonctionnalités
//
// Format MessageEnvelope:
//   {
//     "envelope": {
//       "version": "3.0",
//       "id": "uuid",
//       "timestamp": 1696435200000,
//       "type": "request|response|event|error"
//     },
//     "request": {...},    // Si type = request
//     "response": {...},   // Si type = response
//     "event": {...},      // Si type = event
//     "error": {...}       // Si type = error
//   }
//
// Auteur: MidiMind Team
// ============================================================================

#include "MessageEnvelope.h"
#include "../core/Logger.h"
#include <stdexcept>

namespace midiMind {

// ============================================================================
// CONSTRUCTEURS
// ============================================================================

MessageEnvelope::MessageEnvelope()
    : envelope_(protocol::MessageType::REQUEST)
    , validationCached_(false)
    , validationResult_(false)
{}

MessageEnvelope::MessageEnvelope(protocol::MessageType type)
    : envelope_(type)
    , validationCached_(false)
    , validationResult_(false)
{}

// ============================================================================
// FACTORY METHODS
// ============================================================================

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

// ============================================================================
// PARSING
// ============================================================================

std::optional<MessageEnvelope> MessageEnvelope::fromJsonString(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        return fromJson(j);
        
    } catch (const json::parse_error& e) {
        Logger::error("MessageEnvelope", "JSON parse error: " + std::string(e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        Logger::error("MessageEnvelope", "Unexpected error: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<MessageEnvelope> MessageEnvelope::fromJson(const json& j) {
    try {
        // Vérifier présence de l'envelope
        if (!j.contains("envelope")) {
            Logger::error("MessageEnvelope", "Missing 'envelope' field");
            return std::nullopt;
        }
        
        const auto& env = j["envelope"];
        
        // Parser le type
        if (!env.contains("type")) {
            Logger::error("MessageEnvelope", "Missing 'type' in envelope");
            return std::nullopt;
        }
        
        std::string typeStr = env["type"];
        protocol::MessageType type = protocol::Envelope::stringToMessageType(typeStr);
        
        // Créer le message
        MessageEnvelope msg(type);
        
        // Parser l'envelope
        msg.envelope_.version = env.value("version", "3.0");
        msg.envelope_.id = env.value("id", "");
        msg.envelope_.timestamp = env.value("timestamp", protocol::Envelope::getCurrentTimestamp());
        msg.envelope_.type = type;
        
        // Parser le contenu selon le type
        switch (type) {
            case protocol::MessageType::REQUEST:
                if (j.contains("request")) {
                    protocol::Request req;
                    req.command = j["request"].value("command", "");
                    req.params = j["request"].value("params", json::object());
                    msg.request_ = req;
                } else {
                    Logger::error("MessageEnvelope", "Missing 'request' field for REQUEST type");
                    return std::nullopt;
                }
                break;
                
            case protocol::MessageType::RESPONSE:
                if (j.contains("response")) {
                    protocol::Response resp;
                    resp.requestId = j["response"].value("requestId", "");
                    resp.success = j["response"].value("success", false);
                    resp.data = j["response"].value("data", json::object());
                    resp.latency = j["response"].value("latency", 0);
                    msg.response_ = resp;
                } else {
                    Logger::error("MessageEnvelope", "Missing 'response' field for RESPONSE type");
                    return std::nullopt;
                }
                break;
                
            case protocol::MessageType::EVENT:
                if (j.contains("event")) {
                    protocol::Event evt;
                    evt.name = j["event"].value("name", "");
                    evt.data = j["event"].value("data", json::object());
                    std::string priorityStr = j["event"].value("priority", "normal");
                    evt.priority = protocol::Event::stringToPriority(priorityStr);
                    msg.event_ = evt;
                } else {
                    Logger::error("MessageEnvelope", "Missing 'event' field for EVENT type");
                    return std::nullopt;
                }
                break;
                
            case protocol::MessageType::ERROR:
                if (j.contains("error")) {
                    protocol::Error err;
                    err.code = static_cast<protocol::ErrorCode>(
                        j["error"].value("code", 1000));
                    err.message = j["error"].value("message", "");
                    err.details = j["error"].value("details", json::object());
                    err.retryable = j["error"].value("retryable", false);
                    err.requestId = j["error"].value("requestId", "");
                    msg.error_ = err;
                } else {
                    Logger::error("MessageEnvelope", "Missing 'error' field for ERROR type");
                    return std::nullopt;
                }
                break;
        }
        
        return msg;
        
    } catch (const json::exception& e) {
        Logger::error("MessageEnvelope", "JSON exception: " + std::string(e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        Logger::error("MessageEnvelope", "Unexpected exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

// ============================================================================
// SÉRIALISATION
// ============================================================================

json MessageEnvelope::toJson() const {
    json j;
    
    // Envelope
    j["envelope"] = {
        {"version", envelope_.version},
        {"id", envelope_.id},
        {"timestamp", envelope_.timestamp},
        {"type", protocol::Envelope::messageTypeToString(envelope_.type)}
    };
    
    // Contenu selon le type
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            if (request_) {
                j["request"] = {
                    {"command", request_->command},
                    {"params", request_->params}
                };
            }
            break;
            
        case protocol::MessageType::RESPONSE:
            if (response_) {
                j["response"] = {
                    {"requestId", response_->requestId},
                    {"success", response_->success},
                    {"data", response_->data},
                    {"latency", response_->latency}
                };
            }
            break;
            
        case protocol::MessageType::EVENT:
            if (event_) {
                j["event"] = {
                    {"name", event_->name},
                    {"data", event_->data},
                    {"priority", protocol::Event::priorityToString(event_->priority)}
                };
            }
            break;
            
        case protocol::MessageType::ERROR:
            if (error_) {
                j["error"] = {
                    {"code", static_cast<int>(error_->code)},
                    {"message", error_->message},
                    {"details", error_->details},
                    {"retryable", error_->retryable}
                };
                if (!error_->requestId.empty()) {
                    j["error"]["requestId"] = error_->requestId;
                }
            }
            break;
    }
    
    return j;
}

std::string MessageEnvelope::toJsonString(int indent) const {
    return toJson().dump(indent);
}

// ============================================================================
// VALIDATION
// ============================================================================

bool MessageEnvelope::validate(std::vector<std::string>& errors) const {
    // Utiliser le cache si disponible
    if (validationCached_) {
        errors = validationErrors_;
        return validationResult_;
    }
    
    errors.clear();
    bool valid = true;
    
    // Valider l'envelope
    if (!validateEnvelope()) {
        valid = false;
    }
    
    // Valider le contenu
    if (!validateContent()) {
        valid = false;
    }
    
    // Mettre en cache
    validationCached_ = true;
    validationResult_ = valid;
    validationErrors_ = errors;
    
    return valid;
}

bool MessageEnvelope::validateEnvelope() const {
    bool valid = true;
    
    // Version
    if (envelope_.version.empty()) {
        validationErrors_.push_back("Envelope version is empty");
        valid = false;
    }
    
    // ID
    if (envelope_.id.empty()) {
        validationErrors_.push_back("Envelope id is empty");
        valid = false;
    }
    
    // Timestamp
    if (envelope_.timestamp <= 0) {
        validationErrors_.push_back("Envelope timestamp is invalid");
        valid = false;
    }
    
    return valid;
}

bool MessageEnvelope::validateContent() const {
    bool valid = true;
    
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            if (!request_) {
                validationErrors_.push_back("REQUEST message has no request content");
                valid = false;
            } else {
                if (request_->command.empty()) {
                    validationErrors_.push_back("Request command is empty");
                    valid = false;
                }
            }
            break;
            
        case protocol::MessageType::RESPONSE:
            if (!response_) {
                validationErrors_.push_back("RESPONSE message has no response content");
                valid = false;
            } else {
                if (response_->requestId.empty()) {
                    validationErrors_.push_back("Response requestId is empty");
                    valid = false;
                }
            }
            break;
            
        case protocol::MessageType::EVENT:
            if (!event_) {
                validationErrors_.push_back("EVENT message has no event content");
                valid = false;
            } else {
                if (event_->name.empty()) {
                    validationErrors_.push_back("Event name is empty");
                    valid = false;
                }
            }
            break;
            
        case protocol::MessageType::ERROR:
            if (!error_) {
                validationErrors_.push_back("ERROR message has no error content");
                valid = false;
            } else {
                if (error_->message.empty()) {
                    validationErrors_.push_back("Error message is empty");
                    valid = false;
                }
            }
            break;
    }
    
    return valid;
}

bool MessageEnvelope::isValid() const {
    std::vector<std::string> errors;
    return validate(errors);
}

// ============================================================================
// ACCESSEURS TYPE
// ============================================================================

bool MessageEnvelope::isRequest() const {
    return envelope_.type == protocol::MessageType::REQUEST;
}

bool MessageEnvelope::isResponse() const {
    return envelope_.type == protocol::MessageType::RESPONSE;
}

bool MessageEnvelope::isEvent() const {
    return envelope_.type == protocol::MessageType::EVENT;
}

bool MessageEnvelope::isError() const {
    return envelope_.type == protocol::MessageType::ERROR;
}

// ============================================================================
// ACCESSEURS CONTENU
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
// ACCESSEURS ENVELOPE - SUPPRIMÉS (déjà inline dans .h)
// ============================================================================
// Les 4 méthodes suivantes sont supprimées car elles sont déjà définies
// inline dans MessageEnvelope.h:
//   - getEnvelope() const
//   - getId() const
//   - getTimestamp() const
//   - getType() const
//
// RAISON: Éviter l'erreur de redéfinition lors de la compilation.
// ============================================================================

// ============================================================================
// HELPERS
// ============================================================================

int MessageEnvelope::getLatencySinceCreation() const {
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    return static_cast<int>(nowMs - envelope_.timestamp);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MessageEnvelope.cpp v3.1.1
// ============================================================================

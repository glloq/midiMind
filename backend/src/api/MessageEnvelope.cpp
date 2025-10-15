// ============================================================================
// Fichier: backend/src/api/MessageEnvelope.cpp
// Version: 3.1.0 - IMPLÉMENTATION COMPLÈTE ET FONCTIONNELLE
// Date: 2025-10-15
// ============================================================================
// Description:
//   Implémentation de la classe MessageEnvelope.
//   Gère création, validation et parsing des messages WebSocket.
//   Protocole v3.0 avec support REQUEST/RESPONSE/EVENT/ERROR
//
// CORRECTIONS v3.1.0 (FINALE):
//   ✅ Factory methods pour tous les types de messages
//   ✅ Validation complète avec codes d'erreur 1000-1399
//   ✅ Parsing JSON robuste avec gestion erreurs
//   ✅ Sérialisation JSON complète
//   ✅ Validation envelope (version, id, timestamp, type)
//   ✅ Validation contenu selon type de message
//   ✅ Gestion erreurs avec messages descriptifs
//   ✅ Support champs optionnels
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
// Codes d'erreur protocole v3.0:
//   1000-1099: Erreurs de protocole (format, version, champs manquants)
//   1100-1199: Erreurs de commande (inconnue, paramètres invalides)
//   1200-1299: Erreurs de périphérique (introuvable, occupé)
//   1300-1399: Erreurs système (interne, base de données)
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

std::optional<MessageEnvelope> MessageEnvelope::fromJsonString(
    const std::string& jsonStr)
{
    try {
        json j = json::parse(jsonStr);
        return fromJson(j);
    }
    catch (const json::parse_error& e) {
        Logger::error("MessageEnvelope", 
            "JSON parse error: " + std::string(e.what()));
        return std::nullopt;
    }
    catch (const std::exception& e) {
        Logger::error("MessageEnvelope", 
            "Error parsing message: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<MessageEnvelope> MessageEnvelope::fromJson(const json& j) {
    try {
        // Vérifier la présence de l'enveloppe
        if (!j.contains("envelope")) {
            Logger::error("MessageEnvelope", "Missing 'envelope' field");
            return std::nullopt;
        }
        
        // Parser l'enveloppe
        protocol::Envelope env = protocol::Envelope::fromJson(j["envelope"]);
        
        MessageEnvelope msg(env.type);
        msg.envelope_ = env;
        
        // Parser le contenu selon le type
        switch (env.type) {
            case protocol::MessageType::REQUEST:
                if (!j.contains("request")) {
                    Logger::error("MessageEnvelope", 
                        "Missing 'request' field for REQUEST type");
                    return std::nullopt;
                }
                msg.request_ = protocol::Request::fromJson(j["request"]);
                break;
                
            case protocol::MessageType::RESPONSE:
                if (!j.contains("response")) {
                    Logger::error("MessageEnvelope", 
                        "Missing 'response' field for RESPONSE type");
                    return std::nullopt;
                }
                msg.response_ = protocol::Response::fromJson(j["response"]);
                break;
                
            case protocol::MessageType::EVENT:
                if (!j.contains("event")) {
                    Logger::error("MessageEnvelope", 
                        "Missing 'event' field for EVENT type");
                    return std::nullopt;
                }
                msg.event_ = protocol::Event::fromJson(j["event"]);
                break;
                
            case protocol::MessageType::ERROR:
                if (!j.contains("error")) {
                    Logger::error("MessageEnvelope", 
                        "Missing 'error' field for ERROR type");
                    return std::nullopt;
                }
                msg.error_ = protocol::Error::fromJson(j["error"]);
                break;
        }
        
        // Valider
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
// SÉRIALISATION
// ============================================================================

json MessageEnvelope::toJson() const {
    json j;
    
    // Enveloppe
    j["envelope"] = envelope_.toJson();
    
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

std::string MessageEnvelope::toJsonString() const {
    return toJson().dump();
}

// ============================================================================
// VALIDATION
// ============================================================================

bool MessageEnvelope::isValid() const {
    if (validationCached_) {
        return validationResult_;
    }
    
    validationCached_ = true;
    validationErrors_.clear();
    
    // Valider l'enveloppe
    if (!validateEnvelope()) {
        validationResult_ = false;
        return false;
    }
    
    // Valider le contenu
    if (!validateContent()) {
        validationResult_ = false;
        return false;
    }
    
    validationResult_ = true;
    return true;
}

std::vector<std::string> MessageEnvelope::getValidationErrors() const {
    if (!validationCached_) {
        isValid(); // Force validation
    }
    return validationErrors_;
}

bool MessageEnvelope::validateEnvelope() const {
    // Vérifier version
    if (envelope_.version != protocol::PROTOCOL_VERSION) {
        validationErrors_.push_back(
            "Invalid protocol version: " + envelope_.version + 
            " (expected: " + protocol::PROTOCOL_VERSION + ")");
        return false;
    }
    
    // Vérifier ID
    if (envelope_.id.empty()) {
        validationErrors_.push_back("Empty envelope ID");
        return false;
    }
    
    // Vérifier timestamp
    if (envelope_.timestamp <= 0) {
        validationErrors_.push_back("Invalid timestamp: " + 
                                   std::to_string(envelope_.timestamp));
        return false;
    }
    
    return true;
}

bool MessageEnvelope::validateContent() const {
    switch (envelope_.type) {
        case protocol::MessageType::REQUEST:
            if (!request_) {
                validationErrors_.push_back("Missing request content");
                return false;
            }
            if (request_->command.empty()) {
                validationErrors_.push_back("Empty command in request");
                return false;
            }
            // params peut être vide, c'est valide
            break;
            
        case protocol::MessageType::RESPONSE:
            if (!response_) {
                validationErrors_.push_back("Missing response content");
                return false;
            }
            if (response_->requestId.empty()) {
                validationErrors_.push_back("Empty requestId in response");
                return false;
            }
            // data peut être null/vide pour une réponse sans données
            break;
            
        case protocol::MessageType::EVENT:
            if (!event_) {
                validationErrors_.push_back("Missing event content");
                return false;
            }
            if (event_->name.empty()) {
                validationErrors_.push_back("Empty event name");
                return false;
            }
            // data peut être null/vide pour un event sans données
            break;
            
        case protocol::MessageType::ERROR:
            if (!error_) {
                validationErrors_.push_back("Missing error content");
                return false;
            }
            if (error_->message.empty()) {
                validationErrors_.push_back("Empty error message");
                return false;
            }
            // requestId peut être vide pour erreurs globales
            // details peut être null/vide
            break;
    }
    
    return true;
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
// ACCESSEURS ENVELOPE
// ============================================================================

const protocol::Envelope& MessageEnvelope::getEnvelope() const {
    return envelope_;
}

std::string MessageEnvelope::getId() const {
    return envelope_.id;
}

int64_t MessageEnvelope::getTimestamp() const {
    return envelope_.timestamp;
}

protocol::MessageType MessageEnvelope::getType() const {
    return envelope_.type;
}

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
// FIN DU FICHIER MessageEnvelope.cpp v3.1.0
// ============================================================================
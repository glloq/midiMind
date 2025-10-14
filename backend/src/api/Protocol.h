// ============================================================================
// Fichier: backend/src/api/Protocol.h
// Version: 3.0.0-refonte
// Date: 2025-10-09
// ============================================================================
// Description:
//   Définition du protocole WebSocket unifié entre frontend et backend.
//   Structure stricte pour garantir fiabilité et traçabilité.
//
// Format Enveloppe:
//   {
//     "envelope": {
//       "version": "3.0",
//       "id": "uuid",
//       "timestamp": 1234567890,
//       "type": "request|response|event|error"
//     },
//     "request": {...},      // Si type = request
//     "response": {...},     // Si type = response
//     "event": {...},        // Si type = event
//     "error": {...}         // Si type = error
//   }
// ============================================================================

#pragma once

#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {
namespace protocol {

// ============================================================================
// CONSTANTES
// ============================================================================

constexpr const char* PROTOCOL_VERSION = "3.0";

// Types de messages
enum class MessageType {
    REQUEST,    // Frontend → Backend : demande d'action
    RESPONSE,   // Backend → Frontend : réponse à une requête
    EVENT,      // Backend → Frontend : notification asynchrone
    ERROR       // Backend → Frontend : erreur
};

// Priorités d'événements
enum class EventPriority {
    LOW,        // Événements informatifs (stats, logs)
    NORMAL,     // Événements standards (status updates)
    HIGH        // Événements critiques (MIDI messages, erreurs)
};

// Codes d'erreur standardisés
enum class ErrorCode {
    // Erreurs de protocole (1000-1099)
    INVALID_FORMAT = 1000,
    INVALID_VERSION = 1001,
    MISSING_FIELD = 1002,
    INVALID_TYPE = 1003,
    
    // Erreurs de commande (1100-1199)
    UNKNOWN_COMMAND = 1100,
    INVALID_PARAMS = 1101,
    COMMAND_FAILED = 1102,
    TIMEOUT = 1103,
    
    // Erreurs de validation (1200-1299)
    VALIDATION_ERROR = 1200,
    FILE_NOT_FOUND = 1201,
    PATH_TRAVERSAL = 1202,
    PERMISSION_DENIED = 1203,
    
    // Erreurs MIDI (1300-1399)
    DEVICE_NOT_FOUND = 1300,
    DEVICE_NOT_CONNECTED = 1301,
    ROUTE_NOT_FOUND = 1302,
    INVALID_MIDI_MESSAGE = 1303,
    
    // Erreurs système (1400-1499)
    INTERNAL_ERROR = 1400,
    NOT_IMPLEMENTED = 1401,
    SERVICE_UNAVAILABLE = 1402,
    DATABASE_ERROR = 1403
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @brief Entête commune à tous les messages
 */
struct Envelope {
    std::string version;        // Version du protocole
    std::string id;             // ID unique du message (UUID v4)
    int64_t timestamp;          // Timestamp en millisecondes (epoch)
    MessageType type;           // Type de message
    
    // Constructeur par défaut
    Envelope()
        : version(PROTOCOL_VERSION)
        , id("")
        , timestamp(0)
        , type(MessageType::REQUEST)
    {}
    
    // Constructeur avec génération auto timestamp
    Envelope(MessageType t)
        : version(PROTOCOL_VERSION)
        , id(generateUUID())
        , timestamp(getCurrentTimestamp())
        , type(t)
    {}
    
    // Conversion vers JSON
    json toJson() const {
        json j;
        j["version"] = version;
        j["id"] = id;
        j["timestamp"] = timestamp;
        j["type"] = messageTypeToString(type);
        return j;
    }
    
    // Création depuis JSON
    static Envelope fromJson(const json& j) {
        Envelope env;
        env.version = j.value("version", PROTOCOL_VERSION);
        env.id = j.value("id", "");
        env.timestamp = j.value("timestamp", getCurrentTimestamp());
        env.type = stringToMessageType(j.value("type", "request"));
        return env;
    }
    
private:
    static std::string generateUUID();
    static int64_t getCurrentTimestamp();
    static std::string messageTypeToString(MessageType type);
    static MessageType stringToMessageType(const std::string& str);
};

/**
 * @brief Structure d'une requête (frontend → backend)
 */
struct Request {
    std::string command;        // Nom de la commande (ex: "files.list")
    json params;                // Paramètres (objet JSON)
    
    Request() : params(json::object()) {}
    
    json toJson() const {
        json j;
        j["command"] = command;
        j["params"] = params;
        return j;
    }
    
    static Request fromJson(const json& j) {
        Request req;
        req.command = j.value("command", "");
        req.params = j.value("params", json::object());
        return req;
    }
};

/**
 * @brief Structure d'une réponse (backend → frontend)
 */
struct Response {
    std::string requestId;      // ID de la requête d'origine
    bool success;               // Succès ou échec
    json data;                  // Données de la réponse
    int latency;                // Latence en ms (optionnel)
    
    Response()
        : success(false)
        , data(json::object())
        , latency(0)
    {}
    
    json toJson() const {
        json j;
        j["requestId"] = requestId;
        j["success"] = success;
        j["data"] = data;
        if (latency > 0) {
            j["latency"] = latency;
        }
        return j;
    }
    
    static Response fromJson(const json& j) {
        Response resp;
        resp.requestId = j.value("requestId", "");
        resp.success = j.value("success", false);
        resp.data = j.value("data", json::object());
        resp.latency = j.value("latency", 0);
        return resp;
    }
};

/**
 * @brief Structure d'un événement (backend → frontend)
 */
struct Event {
    std::string name;           // Nom de l'événement (ex: "midi:message")
    json data;                  // Données de l'événement
    EventPriority priority;     // Priorité
    
    Event()
        : data(json::object())
        , priority(EventPriority::NORMAL)
    {}
    
    json toJson() const {
        json j;
        j["name"] = name;
        j["data"] = data;
        j["priority"] = priorityToString(priority);
        return j;
    }
    
    static Event fromJson(const json& j) {
        Event evt;
        evt.name = j.value("name", "");
        evt.data = j.value("data", json::object());
        evt.priority = stringToPriority(j.value("priority", "normal"));
        return evt;
    }
    
private:
    static std::string priorityToString(EventPriority p);
    static EventPriority stringToPriority(const std::string& str);
};

/**
 * @brief Structure d'une erreur (backend → frontend)
 */
struct Error {
    std::string requestId;      // ID de la requête (si applicable)
    ErrorCode code;             // Code d'erreur
    std::string message;        // Message lisible
    json details;               // Détails supplémentaires
    bool retryable;             // Le client peut-il réessayer ?
    
    Error()
        : code(ErrorCode::INTERNAL_ERROR)
        , details(json::object())
        , retryable(false)
    {}
    
    json toJson() const {
        json j;
        if (!requestId.empty()) {
            j["requestId"] = requestId;
        }
        j["code"] = errorCodeToString(code);
        j["codeValue"] = static_cast<int>(code);
        j["message"] = message;
        j["details"] = details;
        j["retryable"] = retryable;
        return j;
    }
    
    static Error fromJson(const json& j) {
        Error err;
        err.requestId = j.value("requestId", "");
        err.code = stringToErrorCode(j.value("code", "INTERNAL_ERROR"));
        err.message = j.value("message", "");
        err.details = j.value("details", json::object());
        err.retryable = j.value("retryable", false);
        return err;
    }
    
private:
    static std::string errorCodeToString(ErrorCode code);
    static ErrorCode stringToErrorCode(const std::string& str);
};

// ============================================================================
// HELPERS - IMPLÉMENTATION INLINE
// ============================================================================

// Génération UUID v4 simplifiée
inline std::string Envelope::generateUUID() {
    // Version simplifiée pour l'exemple
    // En production, utiliser une vraie lib UUID (ex: boost::uuids)
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);
    
    char buf[37];
    snprintf(buf, sizeof(buf),
             "%08x-%04x-%04x-%04x-%012llx",
             (uint32_t)(part1 >> 32),
             (uint16_t)(part1 >> 16),
             (uint16_t)(0x4000 | (part1 & 0x0FFF)),
             (uint16_t)(0x8000 | (part2 >> 48)),
             part2 & 0xFFFFFFFFFFFFULL);
    
    return std::string(buf);
}

inline int64_t Envelope::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
}

inline std::string Envelope::messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::REQUEST:  return "request";
        case MessageType::RESPONSE: return "response";
        case MessageType::EVENT:    return "event";
        case MessageType::ERROR:    return "error";
        default:                    return "unknown";
    }
}

inline MessageType Envelope::stringToMessageType(const std::string& str) {
    if (str == "request")  return MessageType::REQUEST;
    if (str == "response") return MessageType::RESPONSE;
    if (str == "event")    return MessageType::EVENT;
    if (str == "error")    return MessageType::ERROR;
    return MessageType::REQUEST;
}

inline std::string Event::priorityToString(EventPriority p) {
    switch (p) {
        case EventPriority::LOW:    return "low";
        case EventPriority::NORMAL: return "normal";
        case EventPriority::HIGH:   return "high";
        default:                    return "normal";
    }
}

inline EventPriority Event::stringToPriority(const std::string& str) {
    if (str == "low")  return EventPriority::LOW;
    if (str == "high") return EventPriority::HIGH;
    return EventPriority::NORMAL;
}

inline std::string Error::errorCodeToString(ErrorCode code) {
    switch (code) {
        // Protocole
        case ErrorCode::INVALID_FORMAT:    return "INVALID_FORMAT";
        case ErrorCode::INVALID_VERSION:   return "INVALID_VERSION";
        case ErrorCode::MISSING_FIELD:     return "MISSING_FIELD";
        case ErrorCode::INVALID_TYPE:      return "INVALID_TYPE";
        
        // Commandes
        case ErrorCode::UNKNOWN_COMMAND:   return "UNKNOWN_COMMAND";
        case ErrorCode::INVALID_PARAMS:    return "INVALID_PARAMS";
        case ErrorCode::COMMAND_FAILED:    return "COMMAND_FAILED";
        case ErrorCode::TIMEOUT:           return "TIMEOUT";
        
        // Validation
        case ErrorCode::VALIDATION_ERROR:  return "VALIDATION_ERROR";
        case ErrorCode::FILE_NOT_FOUND:    return "FILE_NOT_FOUND";
        case ErrorCode::PATH_TRAVERSAL:    return "PATH_TRAVERSAL";
        case ErrorCode::PERMISSION_DENIED: return "PERMISSION_DENIED";
        
        // MIDI
        case ErrorCode::DEVICE_NOT_FOUND:      return "DEVICE_NOT_FOUND";
        case ErrorCode::DEVICE_NOT_CONNECTED:  return "DEVICE_NOT_CONNECTED";
        case ErrorCode::ROUTE_NOT_FOUND:       return "ROUTE_NOT_FOUND";
        case ErrorCode::INVALID_MIDI_MESSAGE:  return "INVALID_MIDI_MESSAGE";
        
        // Système
        case ErrorCode::INTERNAL_ERROR:        return "INTERNAL_ERROR";
        case ErrorCode::NOT_IMPLEMENTED:       return "NOT_IMPLEMENTED";
        case ErrorCode::SERVICE_UNAVAILABLE:   return "SERVICE_UNAVAILABLE";
        case ErrorCode::DATABASE_ERROR:        return "DATABASE_ERROR";
        
        default:                               return "UNKNOWN_ERROR";
    }
}

inline ErrorCode Error::stringToErrorCode(const std::string& str) {
    // Map string → ErrorCode
    static const std::map<std::string, ErrorCode> codeMap = {
        {"INVALID_FORMAT", ErrorCode::INVALID_FORMAT},
        {"INVALID_VERSION", ErrorCode::INVALID_VERSION},
        {"MISSING_FIELD", ErrorCode::MISSING_FIELD},
        {"INVALID_TYPE", ErrorCode::INVALID_TYPE},
        {"UNKNOWN_COMMAND", ErrorCode::UNKNOWN_COMMAND},
        {"INVALID_PARAMS", ErrorCode::INVALID_PARAMS},
        {"COMMAND_FAILED", ErrorCode::COMMAND_FAILED},
        {"TIMEOUT", ErrorCode::TIMEOUT},
        {"VALIDATION_ERROR", ErrorCode::VALIDATION_ERROR},
        {"FILE_NOT_FOUND", ErrorCode::FILE_NOT_FOUND},
        {"PATH_TRAVERSAL", ErrorCode::PATH_TRAVERSAL},
        {"PERMISSION_DENIED", ErrorCode::PERMISSION_DENIED},
        {"DEVICE_NOT_FOUND", ErrorCode::DEVICE_NOT_FOUND},
        {"DEVICE_NOT_CONNECTED", ErrorCode::DEVICE_NOT_CONNECTED},
        {"ROUTE_NOT_FOUND", ErrorCode::ROUTE_NOT_FOUND},
        {"INVALID_MIDI_MESSAGE", ErrorCode::INVALID_MIDI_MESSAGE},
        {"INTERNAL_ERROR", ErrorCode::INTERNAL_ERROR},
        {"NOT_IMPLEMENTED", ErrorCode::NOT_IMPLEMENTED},
        {"SERVICE_UNAVAILABLE", ErrorCode::SERVICE_UNAVAILABLE},
        {"DATABASE_ERROR", ErrorCode::DATABASE_ERROR}
    };
    
    auto it = codeMap.find(str);
    return (it != codeMap.end()) ? it->second : ErrorCode::INTERNAL_ERROR;
}

} // namespace protocol
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Protocol.h
// ============================================================================

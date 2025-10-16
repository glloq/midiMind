// ============================================================================
// Fichier: backend/src/api/Protocol.h
// Version: 3.0.2 - AJOUT STRUCTURES REQUEST/RESPONSE/EVENT
// Date: 2025-10-16
// ============================================================================
// CORRECTIONS v3.0.2:
//   ✅ Ajout struct Request (manquante)
//   ✅ Ajout struct Response (manquante)
//   ✅ Ajout struct Event (manquante)
//   ✅ Conserve toutes les corrections de v3.0.1
//
// Description:
//   Protocole WebSocket complet pour communication client-serveur
// ============================================================================

#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <chrono>
#include <map>
#include <random>

using json = nlohmann::json;

namespace midiMind {
namespace protocol {

// ============================================================================
// CONSTANTES
// ============================================================================

constexpr const char* PROTOCOL_VERSION = "3.0";

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

/**
 * @brief Type de message
 */
enum class MessageType {
    REQUEST,        // Requête client → serveur
    RESPONSE,       // Réponse serveur → client
    EVENT,          // Événement serveur → client (broadcast)
    ERROR           // Erreur
};

/**
 * @brief Priorité d'événement
 */
enum class EventPriority {
    LOW,            // Basse priorité (statistiques, etc.)
    NORMAL,         // Priorité normale (la plupart des événements)
    HIGH            // Haute priorité (alertes, erreurs critiques)
};

/**
 * @brief Codes d'erreur
 */
enum class ErrorCode {
    // Erreurs de protocole (1000-1099)
    INVALID_FORMAT = 1000,
    INVALID_VERSION = 1001,
    MISSING_FIELD = 1002,
    INVALID_TYPE = 1003,
    
    // Erreurs de commandes (1100-1199)
    UNKNOWN_COMMAND = 1100,
    INVALID_PARAMS = 1101,
    COMMAND_FAILED = 1102,
    TIMEOUT = 1103,
    DEVICE_BUSY = 1104,
    
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
        env.timestamp = j.value("timestamp", 0);
        env.type = stringToMessageType(j.value("type", "request"));
        return env;
    }
    
    // Méthodes utilitaires publiques
    static std::string generateUUID();
    static int64_t getCurrentTimestamp();
    
private:
    static std::string messageTypeToString(MessageType type) {
        switch (type) {
            case MessageType::REQUEST:  return "request";
            case MessageType::RESPONSE: return "response";
            case MessageType::EVENT:    return "event";
            case MessageType::ERROR:    return "error";
            default:                    return "unknown";
        }
    }
    
    static MessageType stringToMessageType(const std::string& str) {
        if (str == "request")  return MessageType::REQUEST;
        if (str == "response") return MessageType::RESPONSE;
        if (str == "event")    return MessageType::EVENT;
        if (str == "error")    return MessageType::ERROR;
        return MessageType::REQUEST;
    }
};

/**
 * @brief Structure de requête (client → serveur)
 */
struct Request {
    std::string command;        // Nom de la commande
    json params;                // Paramètres de la commande
    int timeout;                // Timeout en ms (0 = aucun)
    
    Request()
        : params(json::object())
        , timeout(0)
    {}
    
    json toJson() const {
        json j;
        j["command"] = command;
        j["params"] = params;
        if (timeout > 0) {
            j["timeout"] = timeout;
        }
        return j;
    }
    
    static Request fromJson(const json& j) {
        Request req;
        req.command = j.value("command", "");
        req.params = j.value("params", json::object());
        req.timeout = j.value("timeout", 0);
        return req;
    }
};

/**
 * @brief Structure de réponse (serveur → client)
 */
struct Response {
    std::string requestId;      // ID de la requête d'origine
    bool success;               // Succès ou échec
    json data;                  // Données de réponse
    int latency;                // Latence en ms
    
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
 * @brief Structure d'événement (serveur → client, broadcast)
 */
struct Event {
    std::string name;           // Nom de l'événement
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
};

/**
 * @brief Structure d'erreur
 */
struct Error {
    std::string requestId;      // ID de la requête en erreur
    ErrorCode code;             // Code d'erreur
    std::string message;        // Message d'erreur
    json details;               // Détails additionnels
    bool retryable;             // Peut être réessayé ?
    
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

inline int64_t Envelope::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

inline std::string Envelope::generateUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);
    
    char buf[37];
    snprintf(buf, sizeof(buf),
        "%08lx-%04lx-%04lx-%04lx-%012lx",
        (part1 >> 32) & 0xFFFFFFFF,
        (part1 >> 16) & 0xFFFF,
        part1 & 0xFFFF,
        (part2 >> 48) & 0xFFFF,
        part2 & 0xFFFFFFFFFFFF
    );
    
    return std::string(buf);
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
        case ErrorCode::DEVICE_BUSY:       return "DEVICE_BUSY";
        
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
    static const std::map<std::string, ErrorCode> codeMap = {
        {"INVALID_FORMAT", ErrorCode::INVALID_FORMAT},
        {"INVALID_VERSION", ErrorCode::INVALID_VERSION},
        {"MISSING_FIELD", ErrorCode::MISSING_FIELD},
        {"INVALID_TYPE", ErrorCode::INVALID_TYPE},
        {"UNKNOWN_COMMAND", ErrorCode::UNKNOWN_COMMAND},
        {"INVALID_PARAMS", ErrorCode::INVALID_PARAMS},
        {"COMMAND_FAILED", ErrorCode::COMMAND_FAILED},
        {"TIMEOUT", ErrorCode::TIMEOUT},
        {"DEVICE_BUSY", ErrorCode::DEVICE_BUSY},
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

inline std::string priorityToString(EventPriority priority) {
    switch (priority) {
        case EventPriority::LOW:    return "low";
        case EventPriority::NORMAL: return "normal";
        case EventPriority::HIGH:   return "high";
        default:                    return "normal";
    }
}

inline EventPriority stringToPriority(const std::string& str) {
    if (str == "low")  return EventPriority::LOW;
    if (str == "high") return EventPriority::HIGH;
    return EventPriority::NORMAL;
}

} // namespace protocol
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Protocol.h v3.0.2 - COMPLET
// ============================================================================

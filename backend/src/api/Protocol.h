// ============================================================================
// Fichier: backend/src/api/Protocol.h
// Version: 3.0.1 - CORRECTIONS CRITIQUES
// Date: 2025-10-15
// ============================================================================
// CORRECTIONS v3.0.1:
//   ✅ FIX #1: Ajout #include <random> pour random_device et mt19937_64
//   ✅ FIX #2: getCurrentTimestamp() déplacé en public
//   ✅ FIX #3: Ajout ErrorCode::DEVICE_BUSY
//
// Description:
//   Protocole WebSocket pour communication client-serveur
// ============================================================================

#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <chrono>
#include <map>
#include <random>  // ✅ FIX #1: Ajouté pour random_device et mt19937_64

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
    DEVICE_BUSY = 1104,  // ✅ FIX #3: Ajouté
    
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
    
    // ✅ FIX #2: Méthodes utilitaires publiques
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

// ✅ FIX #2: Implémentation publique de getCurrentTimestamp()
inline int64_t Envelope::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// Génération UUID v4 simplifiée
inline std::string Envelope::generateUUID() {
    // ✅ FIX #1: random_device et mt19937_64 maintenant disponibles
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
        case ErrorCode::DEVICE_BUSY:       return "DEVICE_BUSY";  // ✅ FIX #3
        
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
        {"DEVICE_BUSY", ErrorCode::DEVICE_BUSY},  // ✅ FIX #3
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
// FIN DU FICHIER Protocol.h v3.0.1 - CORRIGÉ
// ============================================================================

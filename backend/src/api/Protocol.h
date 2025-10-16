// ============================================================================
// Fichier: backend/src/api/Protocol.h
// Version: 3.0.2 - CORRECTION VISIBILITÉ DES MÉTHODES
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - messageTypeToString() et stringToMessageType() rendues publiques
// - priorityToString() et stringToPriority() rendues publiques
// ============================================================================

#pragma once

#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {
namespace protocol {

// ========================================================================
// CONSTANTES
// ========================================================================

constexpr const char* PROTOCOL_VERSION = "3.0";

// ========================================================================
// ÉNUMÉRATIONS
// ========================================================================

enum class MessageType {
    REQUEST,
    RESPONSE,
    EVENT,
    ERROR
};

enum class EventPriority {
    LOW,
    NORMAL,
    HIGH
};

enum class ErrorCode {
    // Format et validation
    INVALID_FORMAT = 1000,
    INVALID_VERSION = 1001,
    MISSING_FIELD = 1002,
    INVALID_TYPE = 1003,
    
    // Commandes
    UNKNOWN_COMMAND = 2000,
    INVALID_PARAMS = 2001,
    COMMAND_FAILED = 2002,
    TIMEOUT = 2003,
    VALIDATION_ERROR = 2004,
    
    // Fichiers
    FILE_NOT_FOUND = 3000,
    PATH_TRAVERSAL = 3001,
    PERMISSION_DENIED = 3002,
    
    // MIDI
    DEVICE_NOT_FOUND = 4000,
    DEVICE_NOT_CONNECTED = 4001,
    ROUTE_NOT_FOUND = 4002,
    INVALID_MIDI_MESSAGE = 4003,
    
    // Système
    INTERNAL_ERROR = 5000,
    NOT_IMPLEMENTED = 5001,
    SERVICE_UNAVAILABLE = 5002,
    DATABASE_ERROR = 5003
};

// ========================================================================
// STRUCTURE: ENVELOPE
// ========================================================================

struct Envelope {
    std::string version;
    std::string id;
    int64_t timestamp;
    MessageType type;
    
    Envelope()
        : version(PROTOCOL_VERSION)
        , id(generateUUID())
        , timestamp(getCurrentTimestamp())
        , type(MessageType::REQUEST)
    {}
    
    json toJson() const {
        json j;
        j["version"] = version;
        j["id"] = id;
        j["timestamp"] = timestamp;
        j["type"] = messageTypeToString(type);
        return j;
    }
    
    static Envelope fromJson(const json& j) {
        Envelope env;
        env.version = j.value("version", PROTOCOL_VERSION);
        env.id = j.value("id", "");
        env.timestamp = j.value("timestamp", getCurrentTimestamp());
        env.type = stringToMessageType(j.value("type", "request"));
        return env;
    }
    
    // ✅ CORRECTION: Méthodes rendues PUBLIQUES
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
    
private:
    static std::string generateUUID();
    static int64_t getCurrentTimestamp();
};

// ========================================================================
// STRUCTURE: REQUEST
// ========================================================================

struct Request {
    std::string command;
    json params;
    
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

// ========================================================================
// STRUCTURE: RESPONSE
// ========================================================================

struct Response {
    std::string requestId;
    bool success;
    json data;
    int latency;
    
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

// ========================================================================
// STRUCTURE: EVENT
// ========================================================================

struct Event {
    std::string name;
    json data;
    EventPriority priority;
    
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
    
    // ✅ CORRECTION: Méthodes rendues PUBLIQUES
    static std::string priorityToString(EventPriority p) {
        switch (p) {
            case EventPriority::LOW:    return "low";
            case EventPriority::NORMAL: return "normal";
            case EventPriority::HIGH:   return "high";
            default:                    return "normal";
        }
    }
    
    static EventPriority stringToPriority(const std::string& str) {
        if (str == "low")    return EventPriority::LOW;
        if (str == "normal") return EventPriority::NORMAL;
        if (str == "high")   return EventPriority::HIGH;
        return EventPriority::NORMAL;
    }
};

// ========================================================================
// STRUCTURE: ERROR
// ========================================================================

struct Error {
    std::string requestId;
    ErrorCode code;
    std::string message;
    json details;
    bool retryable;
    
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
        j["code"] = static_cast<int>(code);
        j["codeName"] = errorCodeToString(code);
        j["message"] = message;
        j["details"] = details;
        j["retryable"] = retryable;
        return j;
    }
    
    static Error fromJson(const json& j) {
        Error err;
        err.requestId = j.value("requestId", "");
        err.code = static_cast<ErrorCode>(j.value("code", 5000));
        err.message = j.value("message", "");
        err.details = j.value("details", json::object());
        err.retryable = j.value("retryable", false);
        return err;
    }
    
    static std::string errorCodeToString(ErrorCode code) {
        switch (code) {
            case ErrorCode::INVALID_FORMAT:        return "INVALID_FORMAT";
            case ErrorCode::INVALID_VERSION:       return "INVALID_VERSION";
            case ErrorCode::MISSING_FIELD:         return "MISSING_FIELD";
            case ErrorCode::INVALID_TYPE:          return "INVALID_TYPE";
            case ErrorCode::UNKNOWN_COMMAND:       return "UNKNOWN_COMMAND";
            case ErrorCode::INVALID_PARAMS:        return "INVALID_PARAMS";
            case ErrorCode::COMMAND_FAILED:        return "COMMAND_FAILED";
            case ErrorCode::TIMEOUT:               return "TIMEOUT";
            case ErrorCode::VALIDATION_ERROR:      return "VALIDATION_ERROR";
            case ErrorCode::FILE_NOT_FOUND:        return "FILE_NOT_FOUND";
            case ErrorCode::PATH_TRAVERSAL:        return "PATH_TRAVERSAL";
            case ErrorCode::PERMISSION_DENIED:     return "PERMISSION_DENIED";
            case ErrorCode::DEVICE_NOT_FOUND:      return "DEVICE_NOT_FOUND";
            case ErrorCode::DEVICE_NOT_CONNECTED:  return "DEVICE_NOT_CONNECTED";
            case ErrorCode::ROUTE_NOT_FOUND:       return "ROUTE_NOT_FOUND";
            case ErrorCode::INVALID_MIDI_MESSAGE:  return "INVALID_MIDI_MESSAGE";
            case ErrorCode::INTERNAL_ERROR:        return "INTERNAL_ERROR";
            case ErrorCode::NOT_IMPLEMENTED:       return "NOT_IMPLEMENTED";
            case ErrorCode::SERVICE_UNAVAILABLE:   return "SERVICE_UNAVAILABLE";
            case ErrorCode::DATABASE_ERROR:        return "DATABASE_ERROR";
            default:                               return "UNKNOWN_ERROR";
        }
    }
    
    static ErrorCode stringToErrorCode(const std::string& str);
};

// ========================================================================
// IMPLÉMENTATIONS INLINE
// ========================================================================

inline std::string Envelope::generateUUID() {
    auto now = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    
    uint64_t part1 = static_cast<uint64_t>(nanos);
    uint64_t part2 = static_cast<uint64_t>(std::hash<uint64_t>{}(part1));
    
    char buf[64];
    std::snprintf(buf, sizeof(buf),
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

inline ErrorCode Error::stringToErrorCode(const std::string& str) {
    // Map basique
    if (str == "INVALID_FORMAT") return ErrorCode::INVALID_FORMAT;
    if (str == "UNKNOWN_COMMAND") return ErrorCode::UNKNOWN_COMMAND;
    if (str == "COMMAND_FAILED") return ErrorCode::COMMAND_FAILED;
    if (str == "FILE_NOT_FOUND") return ErrorCode::FILE_NOT_FOUND;
    if (str == "DEVICE_NOT_FOUND") return ErrorCode::DEVICE_NOT_FOUND;
    
    return ErrorCode::INTERNAL_ERROR;
}

} // namespace protocol
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Protocol.h
// ============================================================================

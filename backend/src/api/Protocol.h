// ============================================================================
// File: backend/src/api/Protocol.h
// Version: 4.1.4
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   WebSocket protocol - CORRECT ORDER (functions before structures)
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// Changes v4.1.4:
//   - FIXED: Thread-safe generateUUID() with thread_local
//   - FIXED: Thread-safe getISO8601Timestamp() using localtime_r/gmtime_r
//   - FIXED: All struct constructors initialize all members
//
// Changes v4.1.3:
//   - FIXED: Functions declared BEFORE structures that use them
//   - FIXED: Namespace collision with midiMind::ErrorCode
//   - Use protocol::ErrorCode everywhere
//
// ============================================================================

#pragma once

#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {
namespace protocol {

// ============================================================================
// ENUMERATIONS (must be first)
// ============================================================================

enum class MessageType {
    REQUEST,
    RESPONSE,
    EVENT,
    ERROR
};

enum class ErrorCode {
    UNKNOWN = 0,
    INVALID_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    TIMEOUT = 408,
    INTERNAL_ERROR = 500,
    SERVICE_UNAVAILABLE = 503,
    PARSE_ERROR = 1000,
    INVALID_COMMAND = 1001,
    INVALID_PARAMS = 1002,
    INVALID_MESSAGE = 1003,
    COMMAND_FAILED = 1004,
    UNKNOWN_COMMAND = 1005,
    DEVICE_NOT_FOUND = 2001,
    DEVICE_BUSY = 2002,
    MIDI_ERROR = 2000,
    FILE_ERROR = 3000,
    SYSTEM_ERROR = 4000
};

enum class EventPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// ============================================================================
// CONVERSION FUNCTIONS (must be before structures)
// ============================================================================

inline std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::REQUEST:  return "request";
        case MessageType::RESPONSE: return "response";
        case MessageType::EVENT:    return "event";
        case MessageType::ERROR:    return "error";
        default:                    return "unknown";
    }
}

inline MessageType stringToMessageType(const std::string& str) {
    if (str == "request")  return MessageType::REQUEST;
    if (str == "response") return MessageType::RESPONSE;
    if (str == "event")    return MessageType::EVENT;
    if (str == "error")    return MessageType::ERROR;
    return MessageType::REQUEST;
}

inline std::string errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::INVALID_REQUEST:      return "INVALID_REQUEST";
        case ErrorCode::UNAUTHORIZED:         return "UNAUTHORIZED";
        case ErrorCode::FORBIDDEN:            return "FORBIDDEN";
        case ErrorCode::NOT_FOUND:            return "NOT_FOUND";
        case ErrorCode::TIMEOUT:              return "TIMEOUT";
        case ErrorCode::INTERNAL_ERROR:       return "INTERNAL_ERROR";
        case ErrorCode::SERVICE_UNAVAILABLE:  return "SERVICE_UNAVAILABLE";
        case ErrorCode::PARSE_ERROR:          return "PARSE_ERROR";
        case ErrorCode::INVALID_COMMAND:      return "INVALID_COMMAND";
        case ErrorCode::INVALID_PARAMS:       return "INVALID_PARAMS";
        case ErrorCode::INVALID_MESSAGE:      return "INVALID_MESSAGE";
        case ErrorCode::COMMAND_FAILED:       return "COMMAND_FAILED";
        case ErrorCode::UNKNOWN_COMMAND:      return "UNKNOWN_COMMAND";
        case ErrorCode::DEVICE_NOT_FOUND:     return "DEVICE_NOT_FOUND";
        case ErrorCode::DEVICE_BUSY:          return "DEVICE_BUSY";
        case ErrorCode::MIDI_ERROR:           return "MIDI_ERROR";
        case ErrorCode::FILE_ERROR:           return "FILE_ERROR";
        case ErrorCode::SYSTEM_ERROR:         return "SYSTEM_ERROR";
        default:                              return "UNKNOWN";
    }
}

inline std::string eventPriorityToString(EventPriority priority) {
    switch (priority) {
        case EventPriority::LOW:      return "low";
        case EventPriority::NORMAL:   return "normal";
        case EventPriority::HIGH:     return "high";
        case EventPriority::CRITICAL: return "critical";
        default:                      return "normal";
    }
}

inline EventPriority stringToEventPriority(const std::string& str) {
    if (str == "low")      return EventPriority::LOW;
    if (str == "normal")   return EventPriority::NORMAL;
    if (str == "high")     return EventPriority::HIGH;
    if (str == "critical") return EventPriority::CRITICAL;
    return EventPriority::NORMAL;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Generate UUID (thread-safe)
 * @note Uses thread_local for thread safety
 */
inline std::string generateUUID() {
    // Thread-local random generators for thread safety
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_int_distribution<> dis(0, 15);
    thread_local std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    
    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    
    return ss.str();
}

/**
 * @brief Get ISO 8601 timestamp (thread-safe)
 * @note Uses gmtime_r on POSIX systems for thread safety
 */
inline std::string getISO8601Timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef _WIN32
    // Windows: gmtime_s
    gmtime_s(&tm_buf, &time_t_now);
#else
    // POSIX: gmtime_r (thread-safe)
    gmtime_r(&time_t_now, &tm_buf);
#endif
    
    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return ss.str();
}

// ============================================================================
// STRUCTURES (after all conversion functions)
// ============================================================================

struct Envelope {
    std::string id;
    MessageType type;
    std::string timestamp;
    std::string version;
    
    Envelope() 
        : id("")
        , type(MessageType::REQUEST)
        , timestamp("")
        , version("1.0") {}
    
    json toJson() const {
        json j;
        j["id"] = id;
        j["type"] = messageTypeToString(type);
        j["timestamp"] = timestamp;
        j["version"] = version;
        return j;
    }
    
    static Envelope fromJson(const json& j) {
        Envelope env;
        env.id = j.value("id", "");
        env.type = stringToMessageType(j.value("type", "request"));
        env.timestamp = j.value("timestamp", "");
        env.version = j.value("version", "1.0");
        return env;
    }
};

struct Request {
    std::string id;
    std::string command;
    json params;
    int timeout;
    
    Request() 
        : id("")
        , command("")
        , params(json::object())
        , timeout(0) {}
    
    json toJson() const {
        json j;
        j["id"] = id;
        j["command"] = command;
        j["params"] = params;
        j["timeout"] = timeout;
        return j;
    }
    
    static Request fromJson(const json& j) {
        Request req;
        req.id = j.value("id", "");
        req.command = j.value("command", "");
        req.params = j.value("params", json::object());
        req.timeout = j.value("timeout", 0);
        return req;
    }
};

struct Response {
    std::string requestId;
    bool success;
    json data;
    std::string errorMessage;
    ErrorCode errorCode;
    int latency;
    
    Response() 
        : requestId("")
        , success(true)
        , data(json::object())
        , errorMessage("")
        , errorCode(ErrorCode::UNKNOWN)
        , latency(0) {}
    
    json toJson() const {
        json j;
        j["request_id"] = requestId;
        j["success"] = success;
        j["latency"] = latency;
        
        if (success) {
            j["data"] = data;
        } else {
            j["error_message"] = errorMessage;
            j["error_code"] = errorCodeToString(errorCode);
        }
        
        return j;
    }
    
    static Response fromJson(const json& j) {
        Response resp;
        resp.requestId = j.value("request_id", "");
        resp.success = j.value("success", true);
        resp.latency = j.value("latency", 0);
        
        if (resp.success) {
            resp.data = j.value("data", json::object());
        } else {
            resp.errorMessage = j.value("error_message", "");
            resp.errorCode = ErrorCode::UNKNOWN;
        }
        
        return resp;
    }
};

struct Event {
    std::string name;
    json data;
    EventPriority priority;
    std::string source;
    
    Event() 
        : name("")
        , data(json::object())
        , priority(EventPriority::NORMAL)
        , source("") {}
    
    json toJson() const {
        json j;
        j["name"] = name;
        j["data"] = data;
        j["priority"] = eventPriorityToString(priority);
        j["source"] = source;
        return j;
    }
    
    static Event fromJson(const json& j) {
        Event evt;
        evt.name = j.value("name", "");
        evt.data = j.value("data", json::object());
        evt.priority = stringToEventPriority(j.value("priority", "normal"));
        evt.source = j.value("source", "");
        return evt;
    }
};

struct Error {
    ErrorCode code;
    std::string message;
    json details;
    bool retryable;
    std::string requestId;
    
    Error() 
        : code(ErrorCode::UNKNOWN)
        , message("")
        , details(json::object())
        , retryable(false)
        , requestId("") {}
    
    json toJson() const {
        json j;
        j["code"] = errorCodeToString(code);
        j["message"] = message;
        j["details"] = details;
        j["retryable"] = retryable;
        j["request_id"] = requestId;
        return j;
    }
    
    static Error fromJson(const json& j) {
        Error err;
        err.code = ErrorCode::UNKNOWN;
        err.message = j.value("message", "");
        err.details = j.value("details", json::object());
        err.retryable = j.value("retryable", false);
        err.requestId = j.value("request_id", "");
        return err;
    }
};

} // namespace protocol
} // namespace midiMind

// ============================================================================
// END OF FILE Protocol.h v4.1.4
// ============================================================================
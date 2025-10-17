// ============================================================================
// File: backend/src/api/Protocol.h
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   WebSocket protocol definitions (fixed with missing error codes)
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// Changes v4.1.1:
//   - Added INVALID_MESSAGE error code
//   - Added COMMAND_FAILED error code
//   - Added UNKNOWN_COMMAND error code
//   - Added DEVICE_NOT_FOUND error code
//   - Added DEVICE_BUSY error code
//   - Added id field to Request structure
//
// ============================================================================

#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {
namespace protocol {

// ============================================================================
// ENUMERATIONS
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
    INVALID_MESSAGE = 1003,        // NEW
    COMMAND_FAILED = 1004,         // NEW
    UNKNOWN_COMMAND = 1005,        // NEW
    DEVICE_NOT_FOUND = 2001,       // NEW
    DEVICE_BUSY = 2002,            // NEW
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
// STRUCTURES
// ============================================================================

struct Envelope {
    std::string id;
    MessageType type;
    std::string timestamp;
    std::string version;
    
    Envelope() 
        : type(MessageType::REQUEST)
        , version("1.0") {}
};

struct Request {
    std::string id;            // NEW - Added request ID
    std::string command;
    json params;
    int timeout;
    
    Request() 
        : params(json::object())
        , timeout(0) {}
};

struct Response {
    std::string requestId;
    bool success;
    json data;
    std::string errorMessage;
    ErrorCode errorCode;
    int latency;
    
    Response() 
        : success(true)
        , data(json::object())
        , errorCode(ErrorCode::UNKNOWN)
        , latency(0) {}
};

struct Event {
    std::string name;
    json data;
    EventPriority priority;
    std::string source;
    
    Event() 
        : data(json::object())
        , priority(EventPriority::NORMAL) {}
};

struct Error {
    ErrorCode code;
    std::string message;
    json details;
    bool retryable;
    std::string requestId;
    
    Error() 
        : code(ErrorCode::UNKNOWN)
        , details(json::object())
        , retryable(false) {}
};

// ============================================================================
// CONVERSION FUNCTIONS
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

} // namespace protocol
} // namespace midiMind

// ============================================================================
// END OF FILE Protocol.h v4.1.1
// ============================================================================
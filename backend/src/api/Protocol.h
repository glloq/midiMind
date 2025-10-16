// ============================================================================
// File: backend/src/api/Protocol.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   WebSocket protocol definitions and structures.
//   Defines message types, envelopes, and protocol structures.
//
// Author: MidiMind Team
// Date: 2025-10-16
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

/**
 * @enum MessageType
 * @brief Types of WebSocket messages
 */
enum class MessageType {
    REQUEST,   ///< Client request
    RESPONSE,  ///< Server response
    EVENT,     ///< Server event notification
    ERROR      ///< Error message
};

/**
 * @enum ErrorCode
 * @brief Protocol error codes
 */
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
    MIDI_ERROR = 2000,
    FILE_ERROR = 3000,
    SYSTEM_ERROR = 4000
};

/**
 * @enum EventPriority
 * @brief Event priority levels
 */
enum class EventPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct Envelope
 * @brief Message envelope (header)
 */
struct Envelope {
    std::string id;           ///< Unique message ID
    MessageType type;         ///< Message type
    std::string timestamp;    ///< ISO 8601 timestamp
    std::string version;      ///< Protocol version
    
    Envelope() 
        : type(MessageType::REQUEST)
        , version("1.0") {}
};

/**
 * @struct Request
 * @brief Client request message
 */
struct Request {
    std::string command;      ///< Command name (e.g., "files.list")
    json params;              ///< Command parameters
    int timeout;              ///< Timeout in milliseconds (0 = no timeout)
    
    Request() 
        : params(json::object())
        , timeout(0) {}
};

/**
 * @struct Response
 * @brief Server response message
 */
struct Response {
    std::string requestId;    ///< Original request ID
    bool success;             ///< Success flag
    json data;                ///< Response data (if success)
    std::string errorMessage; ///< Error message (if !success)
    ErrorCode errorCode;      ///< Error code (if !success)
    int latency;              ///< Response time in milliseconds
    
    Response() 
        : success(true)
        , data(json::object())
        , errorCode(ErrorCode::UNKNOWN)
        , latency(0) {}
};

/**
 * @struct Event
 * @brief Server event notification
 */
struct Event {
    std::string name;         ///< Event name (e.g., "midi:message")
    json data;                ///< Event data
    EventPriority priority;   ///< Event priority
    std::string source;       ///< Event source
    
    Event() 
        : data(json::object())
        , priority(EventPriority::NORMAL) {}
};

/**
 * @struct Error
 * @brief Standalone error message
 */
struct Error {
    ErrorCode code;           ///< Error code
    std::string message;      ///< Error message
    json details;             ///< Additional details
    bool retryable;           ///< Can retry?
    std::string requestId;    ///< Related request ID (optional)
    
    Error() 
        : code(ErrorCode::UNKNOWN)
        , details(json::object())
        , retryable(false) {}
};

// ============================================================================
// CONVERSION FUNCTIONS
// ============================================================================

/**
 * @brief Convert MessageType to string
 */
inline std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::REQUEST:  return "request";
        case MessageType::RESPONSE: return "response";
        case MessageType::EVENT:    return "event";
        case MessageType::ERROR:    return "error";
        default:                    return "unknown";
    }
}

/**
 * @brief Convert string to MessageType
 */
inline MessageType stringToMessageType(const std::string& str) {
    if (str == "request")  return MessageType::REQUEST;
    if (str == "response") return MessageType::RESPONSE;
    if (str == "event")    return MessageType::EVENT;
    if (str == "error")    return MessageType::ERROR;
    return MessageType::REQUEST;
}

/**
 * @brief Convert ErrorCode to string
 */
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
        case ErrorCode::MIDI_ERROR:           return "MIDI_ERROR";
        case ErrorCode::FILE_ERROR:           return "FILE_ERROR";
        case ErrorCode::SYSTEM_ERROR:         return "SYSTEM_ERROR";
        default:                              return "UNKNOWN";
    }
}

/**
 * @brief Convert EventPriority to string
 */
inline std::string eventPriorityToString(EventPriority priority) {
    switch (priority) {
        case EventPriority::LOW:      return "low";
        case EventPriority::NORMAL:   return "normal";
        case EventPriority::HIGH:     return "high";
        case EventPriority::CRITICAL: return "critical";
        default:                      return "normal";
    }
}

/**
 * @brief Convert string to EventPriority
 */
inline EventPriority stringToEventPriority(const std::string& str) {
    if (str == "low")      return EventPriority::LOW;
    if (str == "normal")   return EventPriority::NORMAL;
    if (str == "high")     return EventPriority::HIGH;
    if (str == "critical") return EventPriority::CRITICAL;
    return EventPriority::NORMAL;
}

} // namespace protocol
} // namespace midiMind

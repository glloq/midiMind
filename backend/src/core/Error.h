// ============================================================================
// File: backend/src/core/Error.h
// Version: 4.1.0 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Error handling system with error codes and exception class.
//   Header-only for inline performance.
//
// Changes v4.1.0:
//   - Added VALIDATION_FAILED error code
//
// ============================================================================

#pragma once

#include <string>
#include <exception>
#include <stdexcept>

namespace midiMind {

// ============================================================================
// ERROR CODES
// ============================================================================

/**
 * @enum ErrorCode
 * @brief Error codes for MidiMind exceptions
 */
enum class ErrorCode {
    // General
    SUCCESS = 0,
    UNKNOWN_ERROR,
    NOT_IMPLEMENTED,
    INVALID_ARGUMENT,
    OUT_OF_RANGE,
    NULL_POINTER,
    
    // Initialization
    INITIALIZATION_FAILED,
    ALREADY_INITIALIZED,
    NOT_INITIALIZED,
    
    // Configuration
    CONFIG_ERROR,
    CONFIG_PARSE_ERROR,
    CONFIG_NOT_FOUND,
    INVALID_CONFIG,
    
    // Database
    DATABASE_ERROR,
    DATABASE_CONNECTION_FAILED,
    DATABASE_QUERY_FAILED,
    DATABASE_NOT_FOUND,
    DATABASE_NOT_CONNECTED = 1518,
	VALIDATION_ERROR = 1100,
	
	
    // File System
    FILE_ERROR,
    FILE_NOT_FOUND,
    FILE_READ_ERROR,
    FILE_WRITE_ERROR,
    FILE_PERMISSION_ERROR,
    PATH_NOT_FOUND,
    DIRECTORY_ERROR,
	STORAGE_FILE_EXISTS = 1501,
	STORAGE_IO_ERROR = 1504,
    
    // MIDI
    MIDI_ERROR,
    MIDI_DEVICE_ERROR,
    MIDI_DEVICE_NOT_FOUND,
    MIDI_CONNECTION_ERROR,
    MIDI_SEND_ERROR,
    MIDI_RECEIVE_ERROR,
    MIDI_INVALID_MESSAGE,
    MIDI_PORT_ERROR,
    
    // MIDI File
    MIDI_FILE_ERROR,
    MIDI_FILE_PARSE_ERROR,
    MIDI_FILE_INVALID_FORMAT,
    MIDI_FILE_CORRUPTED,
    
    // Network
    NETWORK_ERROR,
    NETWORK_CONNECTION_FAILED,
    NETWORK_TIMEOUT,
    NETWORK_SEND_ERROR,
    NETWORK_RECEIVE_ERROR,
    
    // API
    API_ERROR,
    COMMAND_ERROR,
    COMMAND_NOT_FOUND,
    INVALID_COMMAND,
    COMMAND_EXECUTION_FAILED,
    
    // Validation
    VALIDATION_FAILED,          // ✅ AJOUTÉ
    INVALID_JSON,
    SCHEMA_ERROR,
    
    // Threading
    THREAD_ERROR,
    MUTEX_ERROR,
    DEADLOCK_ERROR,
    
    // Resources
    RESOURCE_ERROR,
    RESOURCE_NOT_FOUND,
    RESOURCE_EXHAUSTED,
    OUT_OF_MEMORY,
    
    // Timing
    TIMEOUT_ERROR,
    LATENCY_ERROR,
    TIMING_ERROR,
    
    // State
    INVALID_STATE,
    OPERATION_NOT_ALLOWED,
    ALREADY_EXISTS,
    
    // Hardware
    HARDWARE_ERROR,
    DEVICE_ERROR,
    DRIVER_ERROR
};

// ============================================================================
// ERROR CODE TO STRING
// ============================================================================

/**
 * @brief Convert error code to string
 * @param code Error code
 * @return const char* Error code name
 */
inline const char* errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "SUCCESS";
        case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
        case ErrorCode::NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case ErrorCode::OUT_OF_RANGE: return "OUT_OF_RANGE";
        case ErrorCode::NULL_POINTER: return "NULL_POINTER";
        
        case ErrorCode::INITIALIZATION_FAILED: return "INITIALIZATION_FAILED";
        case ErrorCode::ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
        case ErrorCode::NOT_INITIALIZED: return "NOT_INITIALIZED";
        
        case ErrorCode::CONFIG_ERROR: return "CONFIG_ERROR";
        case ErrorCode::CONFIG_PARSE_ERROR: return "CONFIG_PARSE_ERROR";
        case ErrorCode::CONFIG_NOT_FOUND: return "CONFIG_NOT_FOUND";
        case ErrorCode::INVALID_CONFIG: return "INVALID_CONFIG";
        
        case ErrorCode::DATABASE_ERROR: return "DATABASE_ERROR";
        case ErrorCode::DATABASE_CONNECTION_FAILED: return "DATABASE_CONNECTION_FAILED";
        case ErrorCode::DATABASE_QUERY_FAILED: return "DATABASE_QUERY_FAILED";
        case ErrorCode::DATABASE_NOT_FOUND: return "DATABASE_NOT_FOUND";
        
        case ErrorCode::FILE_ERROR: return "FILE_ERROR";
        case ErrorCode::FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case ErrorCode::FILE_READ_ERROR: return "FILE_READ_ERROR";
        case ErrorCode::FILE_WRITE_ERROR: return "FILE_WRITE_ERROR";
        case ErrorCode::FILE_PERMISSION_ERROR: return "FILE_PERMISSION_ERROR";
        case ErrorCode::PATH_NOT_FOUND: return "PATH_NOT_FOUND";
        case ErrorCode::DIRECTORY_ERROR: return "DIRECTORY_ERROR";
        
        case ErrorCode::MIDI_ERROR: return "MIDI_ERROR";
        case ErrorCode::MIDI_DEVICE_ERROR: return "MIDI_DEVICE_ERROR";
        case ErrorCode::MIDI_DEVICE_NOT_FOUND: return "MIDI_DEVICE_NOT_FOUND";
        case ErrorCode::MIDI_CONNECTION_ERROR: return "MIDI_CONNECTION_ERROR";
        case ErrorCode::MIDI_SEND_ERROR: return "MIDI_SEND_ERROR";
        case ErrorCode::MIDI_RECEIVE_ERROR: return "MIDI_RECEIVE_ERROR";
        case ErrorCode::MIDI_INVALID_MESSAGE: return "MIDI_INVALID_MESSAGE";
        case ErrorCode::MIDI_PORT_ERROR: return "MIDI_PORT_ERROR";
        
        case ErrorCode::MIDI_FILE_ERROR: return "MIDI_FILE_ERROR";
        case ErrorCode::MIDI_FILE_PARSE_ERROR: return "MIDI_FILE_PARSE_ERROR";
        case ErrorCode::MIDI_FILE_INVALID_FORMAT: return "MIDI_FILE_INVALID_FORMAT";
        case ErrorCode::MIDI_FILE_CORRUPTED: return "MIDI_FILE_CORRUPTED";
        
        case ErrorCode::NETWORK_ERROR: return "NETWORK_ERROR";
        case ErrorCode::NETWORK_CONNECTION_FAILED: return "NETWORK_CONNECTION_FAILED";
        case ErrorCode::NETWORK_TIMEOUT: return "NETWORK_TIMEOUT";
        case ErrorCode::NETWORK_SEND_ERROR: return "NETWORK_SEND_ERROR";
        case ErrorCode::NETWORK_RECEIVE_ERROR: return "NETWORK_RECEIVE_ERROR";
        
        case ErrorCode::API_ERROR: return "API_ERROR";
        case ErrorCode::COMMAND_ERROR: return "COMMAND_ERROR";
        case ErrorCode::COMMAND_NOT_FOUND: return "COMMAND_NOT_FOUND";
        case ErrorCode::INVALID_COMMAND: return "INVALID_COMMAND";
        case ErrorCode::COMMAND_EXECUTION_FAILED: return "COMMAND_EXECUTION_FAILED";
        
        case ErrorCode::VALIDATION_FAILED: return "VALIDATION_FAILED";  // ✅ AJOUTÉ
        case ErrorCode::INVALID_JSON: return "INVALID_JSON";
        case ErrorCode::SCHEMA_ERROR: return "SCHEMA_ERROR";
        
        case ErrorCode::THREAD_ERROR: return "THREAD_ERROR";
        case ErrorCode::MUTEX_ERROR: return "MUTEX_ERROR";
        case ErrorCode::DEADLOCK_ERROR: return "DEADLOCK_ERROR";
        
        case ErrorCode::RESOURCE_ERROR: return "RESOURCE_ERROR";
        case ErrorCode::RESOURCE_NOT_FOUND: return "RESOURCE_NOT_FOUND";
        case ErrorCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
        case ErrorCode::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        
        case ErrorCode::TIMEOUT_ERROR: return "TIMEOUT_ERROR";
        case ErrorCode::LATENCY_ERROR: return "LATENCY_ERROR";
        case ErrorCode::TIMING_ERROR: return "TIMING_ERROR";
        
        case ErrorCode::INVALID_STATE: return "INVALID_STATE";
        case ErrorCode::OPERATION_NOT_ALLOWED: return "OPERATION_NOT_ALLOWED";
        case ErrorCode::ALREADY_EXISTS: return "ALREADY_EXISTS";
        
        case ErrorCode::HARDWARE_ERROR: return "HARDWARE_ERROR";
        case ErrorCode::DEVICE_ERROR: return "DEVICE_ERROR";
        case ErrorCode::DRIVER_ERROR: return "DRIVER_ERROR";
        
        default: return "UNKNOWN_ERROR_CODE";
    }
}

// ============================================================================
// EXCEPTION CLASS
// ============================================================================

/**
 * @class MidiMindException
 * @brief Custom exception class with error code
 */
class MidiMindException : public std::runtime_error {
public:
    /**
     * @brief Constructor
     * @param code Error code
     * @param message Error message
     */
    MidiMindException(ErrorCode code, const std::string& message)
        : std::runtime_error(message)
        , code_(code) {}
    
    /**
     * @brief Get error code
     * @return ErrorCode Error code
     */
    ErrorCode getCode() const { return code_; }
    
    /**
     * @brief Get error code as string
     * @return const char* Error code name
     */
    const char* getCodeString() const {
        return errorCodeToString(code_);
    }

private:
    ErrorCode code_;
};

// ============================================================================
// MACROS
// ============================================================================

/**
 * @brief Throw MidiMindException
 * @param code Error code
 * @param message Error message
 */
#define THROW_ERROR(code, message) \
    throw midiMind::MidiMindException(code, message)

/**
 * @brief Assert condition or throw
 * @param condition Condition to check
 * @param code Error code if false
 * @param message Error message if false
 */
#define ASSERT_OR_THROW(condition, code, message) \
    if (!(condition)) { \
        THROW_ERROR(code, message); \
    }

} // namespace midiMind

// ============================================================================
// END OF FILE Error.h
// ============================================================================
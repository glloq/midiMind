// ============================================================================
// File: backend/src/core/Error.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Comprehensive error code definitions for the entire MidiMind system.
//   Organized by module for clarity. Uses enum class for type safety.
//
// Features:
//   - Error codes organized by module (Core, MIDI, Storage, API, etc.)
//   - Type-safe enum class
//   - Helper functions for string conversion
//   - Exception class with error code support
//   - Macros for convenient error throwing
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Added Timing module error codes (calibration, latency)
//   - Added InstrumentDatabase error codes
//   - Reorganized by module for better clarity
//   - Improved documentation
//
// ============================================================================

#pragma once

#include <string>
#include <exception>
#include <sstream>

namespace midiMind {

// ============================================================================
// ERROR CODES
// ============================================================================

/**
 * @enum ErrorCode
 * @brief System-wide error codes organized by module
 * 
 * @details
 * Error code ranges:
 * - 0:          Success
 * - 1000-1099:  Core/General errors
 * - 1100-1199:  MIDI device errors
 * - 1200-1299:  MIDI file errors
 * - 1300-1399:  Network errors
 * - 1400-1499:  API errors
 * - 1500-1599:  Storage/Database errors
 * - 1600-1699:  Timing/Latency errors (NEW in v4.1.0)
 * - 1700-1799:  Processing errors
 * - 1800-1899:  System errors
 */
enum class ErrorCode {
    // ========================================================================
    // SUCCESS (0)
    // ========================================================================
    SUCCESS = 0,
    
    // ========================================================================
    // CORE/GENERAL ERRORS (1000-1099)
    // ========================================================================
    UNKNOWN_ERROR = 1000,           ///< Unknown/unspecified error
    INVALID_ARGUMENT = 1001,        ///< Invalid function argument
    NULL_POINTER = 1002,            ///< Null pointer encountered
    OUT_OF_MEMORY = 1003,           ///< Memory allocation failed
    NOT_INITIALIZED = 1004,         ///< Component not initialized
    ALREADY_INITIALIZED = 1005,     ///< Component already initialized
    TIMEOUT = 1006,                 ///< Operation timeout
    ABORTED = 1007,                 ///< Operation aborted
    INVALID_STATE = 1008,           ///< Invalid state for operation
    NOT_SUPPORTED = 1009,           ///< Feature not supported
    
    // ========================================================================
    // MIDI DEVICE ERRORS (1100-1199)
    // ========================================================================
    MIDI_DEVICE_NOT_FOUND = 1100,       ///< MIDI device not found
    MIDI_DEVICE_OPEN_FAILED = 1101,     ///< Failed to open device
    MIDI_DEVICE_CLOSE_FAILED = 1102,    ///< Failed to close device
    MIDI_DEVICE_DISCONNECTED = 1103,    ///< Device disconnected
    MIDI_INVALID_MESSAGE = 1104,        ///< Invalid MIDI message
    MIDI_SEND_FAILED = 1105,            ///< Failed to send message
    MIDI_RECEIVE_FAILED = 1106,         ///< Failed to receive message
    MIDI_BUFFER_OVERFLOW = 1107,        ///< Buffer overflow
    MIDI_INVALID_CHANNEL = 1108,        ///< Invalid MIDI channel (0-15)
    MIDI_INVALID_NOTE = 1109,           ///< Invalid note number (0-127)
    MIDI_INVALID_VELOCITY = 1110,       ///< Invalid velocity (0-127)
    MIDI_INVALID_CC = 1111,             ///< Invalid control change
    MIDI_ROUTE_NOT_FOUND = 1112,        ///< Route not found
    MIDI_ROUTE_ALREADY_EXISTS = 1113,   ///< Route already exists
    
    // ========================================================================
    // MIDI FILE ERRORS (1200-1299)
    // ========================================================================
    MIDI_FILE_NOT_FOUND = 1200,         ///< MIDI file not found
    MIDI_FILE_OPEN_FAILED = 1201,       ///< Failed to open file
    MIDI_FILE_READ_FAILED = 1202,       ///< Failed to read file
    MIDI_FILE_WRITE_FAILED = 1203,      ///< Failed to write file
    MIDI_FILE_INVALID_FORMAT = 1204,    ///< Invalid file format
    MIDI_FILE_CORRUPTED = 1205,         ///< File corrupted
    MIDI_FILE_TOO_LARGE = 1206,         ///< File too large
    MIDI_FILE_UNSUPPORTED_TYPE = 1207,  ///< Unsupported MIDI type
    
    // ========================================================================
    // NETWORK ERRORS (1300-1399)
    // ========================================================================
    NETWORK_NOT_AVAILABLE = 1300,       ///< Network not available
    NETWORK_CONNECTION_FAILED = 1301,   ///< Connection failed
    NETWORK_DISCONNECTED = 1302,        ///< Disconnected
    NETWORK_TIMEOUT = 1303,             ///< Network timeout
    NETWORK_INVALID_ADDRESS = 1304,     ///< Invalid address/hostname
    NETWORK_SEND_FAILED = 1305,         ///< Send failed
    NETWORK_RECEIVE_FAILED = 1306,      ///< Receive failed
    NETWORK_PROTOCOL_ERROR = 1307,      ///< Protocol error
    
    // ========================================================================
    // API ERRORS (1400-1499)
    // ========================================================================
    API_INVALID_COMMAND = 1400,         ///< Invalid command
    API_INVALID_PARAMETERS = 1401,      ///< Invalid parameters
    API_UNAUTHORIZED = 1402,            ///< Unauthorized access
    API_FORBIDDEN = 1403,               ///< Forbidden
    API_NOT_FOUND = 1404,               ///< Resource not found
    API_CONFLICT = 1405,                ///< Resource conflict
    API_RATE_LIMITED = 1406,            ///< Rate limit exceeded
    API_SERVER_ERROR = 1407,            ///< Internal server error
    API_SERVICE_UNAVAILABLE = 1408,     ///< Service unavailable
    API_INVALID_JSON = 1409,            ///< Invalid JSON format
    API_MISSING_FIELD = 1410,           ///< Required field missing
    
    // ========================================================================
    // STORAGE/DATABASE ERRORS (1500-1599)
    // ========================================================================
    STORAGE_FILE_NOT_FOUND = 1500,      ///< File not found
    STORAGE_FILE_EXISTS = 1501,         ///< File already exists
    STORAGE_PERMISSION_DENIED = 1502,   ///< Permission denied
    STORAGE_DISK_FULL = 1503,           ///< Disk full
    STORAGE_IO_ERROR = 1504,            ///< I/O error
    DATABASE_OPEN_FAILED = 1510,        ///< Failed to open database
    DATABASE_QUERY_FAILED = 1511,       ///< Query failed
    DATABASE_INSERT_FAILED = 1512,      ///< Insert failed
    DATABASE_UPDATE_FAILED = 1513,      ///< Update failed
    DATABASE_DELETE_FAILED = 1514,      ///< Delete failed
    DATABASE_NOT_FOUND = 1515,          ///< Record not found
    DATABASE_CONSTRAINT_VIOLATION = 1516, ///< Constraint violation
    DATABASE_CORRUPTED = 1517,          ///< Database corrupted
    
    // ========================================================================
    // TIMING/LATENCY ERRORS (1600-1699) - NEW in v4.1.0
    // ========================================================================
    TIMING_COMPENSATION_FAILED = 1600,  ///< Latency compensation failed
    TIMING_CALIBRATION_FAILED = 1601,   ///< Calibration failed
    TIMING_INVALID_PROFILE = 1602,      ///< Invalid latency profile
    TIMING_PROFILE_NOT_FOUND = 1603,    ///< Profile not found
    TIMING_MEASUREMENT_FAILED = 1604,   ///< Measurement failed
    TIMING_SYNC_LOST = 1605,            ///< Synchronization lost
    TIMING_DRIFT_TOO_HIGH = 1606,       ///< Clock drift too high
    INSTRUMENT_NOT_FOUND = 1610,        ///< Instrument not found (DB)
    INSTRUMENT_ALREADY_EXISTS = 1611,   ///< Instrument already exists
    INSTRUMENT_UPDATE_FAILED = 1612,    ///< Instrument update failed
    INSTRUMENT_DELETE_FAILED = 1613,    ///< Instrument delete failed
    
    // ========================================================================
    // PROCESSING ERRORS (1700-1799)
    // ========================================================================
    PROCESSOR_NOT_FOUND = 1700,         ///< Processor not found
    PROCESSOR_CREATE_FAILED = 1701,     ///< Failed to create processor
    PROCESSOR_INVALID_CONFIG = 1702,    ///< Invalid configuration
    PROCESSOR_CHAIN_FULL = 1703,        ///< Processor chain full
    PROCESSOR_EXECUTION_FAILED = 1704,  ///< Execution failed
    
    // ========================================================================
    // SYSTEM ERRORS (1800-1899)
    // ========================================================================
    SYSTEM_RESOURCE_EXHAUSTED = 1800,   ///< System resources exhausted
    SYSTEM_PERMISSION_DENIED = 1801,    ///< Permission denied
    SYSTEM_NOT_SUPPORTED = 1802,        ///< Not supported on this system
    SYSTEM_HARDWARE_ERROR = 1803,       ///< Hardware error
    SYSTEM_CONFIG_ERROR = 1804,         ///< Configuration error
    SYSTEM_CRITICAL_ERROR = 1805        ///< Critical system error
};

// ============================================================================
// EXCEPTION CLASS
// ============================================================================

/**
 * @class MidiMindException
 * @brief Standard exception class with error code support
 * 
 * @details
 * All exceptions thrown by MidiMind components should use this class.
 * Provides error code, message, and formatted error string.
 * 
 * @example
 * @code
 * if (!device) {
 *     throw MidiMindException(ErrorCode::MIDI_DEVICE_NOT_FOUND, 
 *                            "Device with ID 'usb_123' not found");
 * }
 * @endcode
 */
class MidiMindException : public std::exception {
public:
    /**
     * @brief Constructor
     * @param code Error code
     * @param message Error message
     */
    MidiMindException(ErrorCode code, const std::string& message)
        : code_(code), message_(message) {
        formattedMessage_ = formatMessage();
    }
    
    /**
     * @brief Get error code
     * @return Error code
     */
    ErrorCode code() const noexcept {
        return code_;
    }
    
    /**
     * @brief Get error message
     * @return Error message
     */
    const std::string& message() const noexcept {
        return message_;
    }
    
    /**
     * @brief Get formatted error string
     * @return Formatted error (for display)
     */
    const char* what() const noexcept override {
        return formattedMessage_.c_str();
    }
    
    /**
     * @brief Convert to string
     * @return String representation
     */
    std::string toString() const {
        return formattedMessage_;
    }

private:
    /**
     * @brief Format error message
     * @return Formatted string: [ERROR_NAME (1234)] Message
     */
    std::string formatMessage() const {
        std::ostringstream oss;
        oss << "[" << getErrorName(code_) 
            << " (" << static_cast<int>(code_) << ")] "
            << message_;
        return oss.str();
    }
    
    /**
     * @brief Get error code name
     * @param code Error code
     * @return String name of error code
     */
    static std::string getErrorName(ErrorCode code) {
        switch (code) {
            case ErrorCode::SUCCESS: return "SUCCESS";
            
            // Core
            case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
            case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
            case ErrorCode::NULL_POINTER: return "NULL_POINTER";
            case ErrorCode::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
            case ErrorCode::NOT_INITIALIZED: return "NOT_INITIALIZED";
            case ErrorCode::ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
            case ErrorCode::TIMEOUT: return "TIMEOUT";
            case ErrorCode::ABORTED: return "ABORTED";
            case ErrorCode::INVALID_STATE: return "INVALID_STATE";
            case ErrorCode::NOT_SUPPORTED: return "NOT_SUPPORTED";
            
            // MIDI Device
            case ErrorCode::MIDI_DEVICE_NOT_FOUND: return "MIDI_DEVICE_NOT_FOUND";
            case ErrorCode::MIDI_DEVICE_OPEN_FAILED: return "MIDI_DEVICE_OPEN_FAILED";
            case ErrorCode::MIDI_DEVICE_CLOSE_FAILED: return "MIDI_DEVICE_CLOSE_FAILED";
            case ErrorCode::MIDI_DEVICE_DISCONNECTED: return "MIDI_DEVICE_DISCONNECTED";
            case ErrorCode::MIDI_INVALID_MESSAGE: return "MIDI_INVALID_MESSAGE";
            case ErrorCode::MIDI_SEND_FAILED: return "MIDI_SEND_FAILED";
            case ErrorCode::MIDI_RECEIVE_FAILED: return "MIDI_RECEIVE_FAILED";
            case ErrorCode::MIDI_BUFFER_OVERFLOW: return "MIDI_BUFFER_OVERFLOW";
            case ErrorCode::MIDI_INVALID_CHANNEL: return "MIDI_INVALID_CHANNEL";
            case ErrorCode::MIDI_INVALID_NOTE: return "MIDI_INVALID_NOTE";
            case ErrorCode::MIDI_INVALID_VELOCITY: return "MIDI_INVALID_VELOCITY";
            case ErrorCode::MIDI_INVALID_CC: return "MIDI_INVALID_CC";
            case ErrorCode::MIDI_ROUTE_NOT_FOUND: return "MIDI_ROUTE_NOT_FOUND";
            case ErrorCode::MIDI_ROUTE_ALREADY_EXISTS: return "MIDI_ROUTE_ALREADY_EXISTS";
            
            // MIDI File
            case ErrorCode::MIDI_FILE_NOT_FOUND: return "MIDI_FILE_NOT_FOUND";
            case ErrorCode::MIDI_FILE_OPEN_FAILED: return "MIDI_FILE_OPEN_FAILED";
            case ErrorCode::MIDI_FILE_READ_FAILED: return "MIDI_FILE_READ_FAILED";
            case ErrorCode::MIDI_FILE_WRITE_FAILED: return "MIDI_FILE_WRITE_FAILED";
            case ErrorCode::MIDI_FILE_INVALID_FORMAT: return "MIDI_FILE_INVALID_FORMAT";
            case ErrorCode::MIDI_FILE_CORRUPTED: return "MIDI_FILE_CORRUPTED";
            case ErrorCode::MIDI_FILE_TOO_LARGE: return "MIDI_FILE_TOO_LARGE";
            case ErrorCode::MIDI_FILE_UNSUPPORTED_TYPE: return "MIDI_FILE_UNSUPPORTED_TYPE";
            
            // Network
            case ErrorCode::NETWORK_NOT_AVAILABLE: return "NETWORK_NOT_AVAILABLE";
            case ErrorCode::NETWORK_CONNECTION_FAILED: return "NETWORK_CONNECTION_FAILED";
            case ErrorCode::NETWORK_DISCONNECTED: return "NETWORK_DISCONNECTED";
            case ErrorCode::NETWORK_TIMEOUT: return "NETWORK_TIMEOUT";
            case ErrorCode::NETWORK_INVALID_ADDRESS: return "NETWORK_INVALID_ADDRESS";
            case ErrorCode::NETWORK_SEND_FAILED: return "NETWORK_SEND_FAILED";
            case ErrorCode::NETWORK_RECEIVE_FAILED: return "NETWORK_RECEIVE_FAILED";
            case ErrorCode::NETWORK_PROTOCOL_ERROR: return "NETWORK_PROTOCOL_ERROR";
            
            // API
            case ErrorCode::API_INVALID_COMMAND: return "API_INVALID_COMMAND";
            case ErrorCode::API_INVALID_PARAMETERS: return "API_INVALID_PARAMETERS";
            case ErrorCode::API_UNAUTHORIZED: return "API_UNAUTHORIZED";
            case ErrorCode::API_FORBIDDEN: return "API_FORBIDDEN";
            case ErrorCode::API_NOT_FOUND: return "API_NOT_FOUND";
            case ErrorCode::API_CONFLICT: return "API_CONFLICT";
            case ErrorCode::API_RATE_LIMITED: return "API_RATE_LIMITED";
            case ErrorCode::API_SERVER_ERROR: return "API_SERVER_ERROR";
            case ErrorCode::API_SERVICE_UNAVAILABLE: return "API_SERVICE_UNAVAILABLE";
            case ErrorCode::API_INVALID_JSON: return "API_INVALID_JSON";
            case ErrorCode::API_MISSING_FIELD: return "API_MISSING_FIELD";
            
            // Storage/Database
            case ErrorCode::STORAGE_FILE_NOT_FOUND: return "STORAGE_FILE_NOT_FOUND";
            case ErrorCode::STORAGE_FILE_EXISTS: return "STORAGE_FILE_EXISTS";
            case ErrorCode::STORAGE_PERMISSION_DENIED: return "STORAGE_PERMISSION_DENIED";
            case ErrorCode::STORAGE_DISK_FULL: return "STORAGE_DISK_FULL";
            case ErrorCode::STORAGE_IO_ERROR: return "STORAGE_IO_ERROR";
            case ErrorCode::DATABASE_OPEN_FAILED: return "DATABASE_OPEN_FAILED";
            case ErrorCode::DATABASE_QUERY_FAILED: return "DATABASE_QUERY_FAILED";
            case ErrorCode::DATABASE_INSERT_FAILED: return "DATABASE_INSERT_FAILED";
            case ErrorCode::DATABASE_UPDATE_FAILED: return "DATABASE_UPDATE_FAILED";
            case ErrorCode::DATABASE_DELETE_FAILED: return "DATABASE_DELETE_FAILED";
            case ErrorCode::DATABASE_NOT_FOUND: return "DATABASE_NOT_FOUND";
            case ErrorCode::DATABASE_CONSTRAINT_VIOLATION: return "DATABASE_CONSTRAINT_VIOLATION";
            case ErrorCode::DATABASE_CORRUPTED: return "DATABASE_CORRUPTED";
            
            // Timing/Latency (NEW v4.1.0)
            case ErrorCode::TIMING_COMPENSATION_FAILED: return "TIMING_COMPENSATION_FAILED";
            case ErrorCode::TIMING_CALIBRATION_FAILED: return "TIMING_CALIBRATION_FAILED";
            case ErrorCode::TIMING_INVALID_PROFILE: return "TIMING_INVALID_PROFILE";
            case ErrorCode::TIMING_PROFILE_NOT_FOUND: return "TIMING_PROFILE_NOT_FOUND";
            case ErrorCode::TIMING_MEASUREMENT_FAILED: return "TIMING_MEASUREMENT_FAILED";
            case ErrorCode::TIMING_SYNC_LOST: return "TIMING_SYNC_LOST";
            case ErrorCode::TIMING_DRIFT_TOO_HIGH: return "TIMING_DRIFT_TOO_HIGH";
            case ErrorCode::INSTRUMENT_NOT_FOUND: return "INSTRUMENT_NOT_FOUND";
            case ErrorCode::INSTRUMENT_ALREADY_EXISTS: return "INSTRUMENT_ALREADY_EXISTS";
            case ErrorCode::INSTRUMENT_UPDATE_FAILED: return "INSTRUMENT_UPDATE_FAILED";
            case ErrorCode::INSTRUMENT_DELETE_FAILED: return "INSTRUMENT_DELETE_FAILED";
            
            // Processing
            case ErrorCode::PROCESSOR_NOT_FOUND: return "PROCESSOR_NOT_FOUND";
            case ErrorCode::PROCESSOR_CREATE_FAILED: return "PROCESSOR_CREATE_FAILED";
            case ErrorCode::PROCESSOR_INVALID_CONFIG: return "PROCESSOR_INVALID_CONFIG";
            case ErrorCode::PROCESSOR_CHAIN_FULL: return "PROCESSOR_CHAIN_FULL";
            case ErrorCode::PROCESSOR_EXECUTION_FAILED: return "PROCESSOR_EXECUTION_FAILED";
            
            // System
            case ErrorCode::SYSTEM_RESOURCE_EXHAUSTED: return "SYSTEM_RESOURCE_EXHAUSTED";
            case ErrorCode::SYSTEM_PERMISSION_DENIED: return "SYSTEM_PERMISSION_DENIED";
            case ErrorCode::SYSTEM_NOT_SUPPORTED: return "SYSTEM_NOT_SUPPORTED";
            case ErrorCode::SYSTEM_HARDWARE_ERROR: return "SYSTEM_HARDWARE_ERROR";
            case ErrorCode::SYSTEM_CONFIG_ERROR: return "SYSTEM_CONFIG_ERROR";
            case ErrorCode::SYSTEM_CRITICAL_ERROR: return "SYSTEM_CRITICAL_ERROR";
            
            default: return "UNKNOWN";
        }
    }
    
    ErrorCode code_;
    std::string message_;
    std::string formattedMessage_;
};

// ============================================================================
// CONVENIENCE MACROS
// ============================================================================

/**
 * @brief Throw MidiMindException with error code and message
 * @param code ErrorCode
 * @param message Error message
 * 
 * @example
 * @code
 * if (!device) {
 *     THROW_ERROR(ErrorCode::MIDI_DEVICE_NOT_FOUND, "Device not found");
 * }
 * @endcode
 */
#define THROW_ERROR(code, message) \
    throw midiMind::MidiMindException(code, message)

/**
 * @brief Check condition and throw if false
 * @param condition Condition to check
 * @param code ErrorCode to throw if false
 * @param message Error message
 * 
 * @example
 * @code
 * CHECK(deviceId.length() > 0, ErrorCode::INVALID_ARGUMENT, 
 *       "Device ID cannot be empty");
 * @endcode
 */
#define CHECK(condition, code, message) \
    if (!(condition)) { \
        THROW_ERROR(code, message); \
    }

/**
 * @brief Check pointer is not null
 * @param ptr Pointer to check
 * @param message Error message if null
 * 
 * @example
 * @code
 * CHECK_NOT_NULL(device, "Device pointer is null");
 * @endcode
 */
#define CHECK_NOT_NULL(ptr, message) \
    CHECK(ptr != nullptr, midiMind::ErrorCode::NULL_POINTER, message)

} // namespace midiMind

// ============================================================================
// END OF FILE Error.h v4.1.0
// ============================================================================


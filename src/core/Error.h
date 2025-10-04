// ============================================================================
// Fichier: src/core/Error.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Codes d'erreur et gestion des exceptions du système.
//   Définit tous les codes d'erreur possibles et les exceptions associées.
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <exception>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @enum ErrorCode
 * @brief Codes d'erreur système
 */
enum class ErrorCode {
    // Succès
    SUCCESS = 0,
    
    // Erreurs générales (1000-1099)
    UNKNOWN_ERROR = 1000,
    INVALID_ARGUMENT = 1001,
    NULL_POINTER = 1002,
    OUT_OF_MEMORY = 1003,
    NOT_INITIALIZED = 1004,
    ALREADY_INITIALIZED = 1005,
    TIMEOUT = 1006,
    ABORTED = 1007,
    
    // Erreurs MIDI (1100-1199)
    MIDI_DEVICE_NOT_FOUND = 1100,
    MIDI_DEVICE_OPEN_FAILED = 1101,
    MIDI_DEVICE_CLOSE_FAILED = 1102,
    MIDI_INVALID_MESSAGE = 1103,
    MIDI_SEND_FAILED = 1104,
    MIDI_RECEIVE_FAILED = 1105,
    MIDI_BUFFER_OVERFLOW = 1106,
    MIDI_INVALID_CHANNEL = 1107,
    MIDI_INVALID_NOTE = 1108,
    MIDI_INVALID_VELOCITY = 1109,
    
    // Erreurs fichiers MIDI (1200-1299)
    MIDI_FILE_NOT_FOUND = 1200,
    MIDI_FILE_OPEN_FAILED = 1201,
    MIDI_FILE_READ_FAILED = 1202,
    MIDI_FILE_WRITE_FAILED = 1203,
    MIDI_FILE_INVALID_FORMAT = 1204,
    MIDI_FILE_CORRUPTED = 1205,
    MIDI_FILE_TOO_LARGE = 1206,
    
    // Erreurs réseau (1300-1399)
    NETWORK_NOT_AVAILABLE = 1300,
    NETWORK_CONNECTION_FAILED = 1301,
    NETWORK_DISCONNECTED = 1302,
    NETWORK_TIMEOUT = 1303,
    NETWORK_INVALID_ADDRESS = 1304,
    NETWORK_SEND_FAILED = 1305,
    NETWORK_RECEIVE_FAILED = 1306,
    
    // Erreurs API (1400-1499)
    API_INVALID_COMMAND = 1400,
    API_INVALID_PARAMETERS = 1401,
    API_UNAUTHORIZED = 1402,
    API_FORBIDDEN = 1403,
    API_NOT_FOUND = 1404,
    API_INTERNAL_ERROR = 1405,
    
    // Erreurs configuration (1500-1599)
    CONFIG_FILE_NOT_FOUND = 1500,
    CONFIG_PARSE_ERROR = 1501,
    CONFIG_INVALID_VALUE = 1502,
    CONFIG_SAVE_FAILED = 1503,
    
    // Erreurs base de données (1600-1699)
    DATABASE_OPEN_FAILED = 1600,
    DATABASE_QUERY_FAILED = 1601,
    DATABASE_INSERT_FAILED = 1602,
    DATABASE_UPDATE_FAILED = 1603,
    DATABASE_DELETE_FAILED = 1604,
    DATABASE_NOT_FOUND = 1605,
    
    // Erreurs processing (1700-1799)
    PROCESSOR_NOT_FOUND = 1700,
    PROCESSOR_CREATE_FAILED = 1701,
    PROCESSOR_INVALID_CONFIG = 1702,
    CHAIN_FULL = 1703,
    
    // Erreurs système (1800-1899)
    SYSTEM_RESOURCE_EXHAUSTED = 1800,
    SYSTEM_PERMISSION_DENIED = 1801,
    SYSTEM_NOT_SUPPORTED = 1802,
    SYSTEM_HARDWARE_ERROR = 1803
};

/**
 * @class MidiMindException
 * @brief Exception de base pour MidiMind
 */
class MidiMindException : public std::exception {
public:
    /**
     * @brief Constructeur
     */
    MidiMindException(ErrorCode code, const std::string& message)
        : code_(code)
        , message_(message)
        , fullMessage_(formatMessage()) {}
    
    /**
     * @brief Récupère le code d'erreur
     */
    ErrorCode getCode() const noexcept {
        return code_;
    }
    
    /**
     * @brief Récupère le message d'erreur
     */
    const std::string& getMessage() const noexcept {
        return message_;
    }
    
    /**
     * @brief Implémentation de std::exception
     */
    const char* what() const noexcept override {
        return fullMessage_.c_str();
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["code"] = static_cast<int>(code_);
        j["name"] = getErrorName(code_);
        j["message"] = message_;
        return j;
    }
    
    /**
     * @brief Récupère le nom du code d'erreur
     */
    static std::string getErrorName(ErrorCode code) {
        switch (code) {
            case ErrorCode::SUCCESS: return "SUCCESS";
            case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
            case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
            case ErrorCode::NULL_POINTER: return "NULL_POINTER";
            case ErrorCode::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
            case ErrorCode::NOT_INITIALIZED: return "NOT_INITIALIZED";
            case ErrorCode::ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
            case ErrorCode::TIMEOUT: return "TIMEOUT";
            case ErrorCode::ABORTED: return "ABORTED";
            
            case ErrorCode::MIDI_DEVICE_NOT_FOUND: return "MIDI_DEVICE_NOT_FOUND";
            case ErrorCode::MIDI_DEVICE_OPEN_FAILED: return "MIDI_DEVICE_OPEN_FAILED";
            case ErrorCode::MIDI_DEVICE_CLOSE_FAILED: return "MIDI_DEVICE_CLOSE_FAILED";
            case ErrorCode::MIDI_INVALID_MESSAGE: return "MIDI_INVALID_MESSAGE";
            case ErrorCode::MIDI_SEND_FAILED: return "MIDI_SEND_FAILED";
            case ErrorCode::MIDI_RECEIVE_FAILED: return "MIDI_RECEIVE_FAILED";
            case ErrorCode::MIDI_BUFFER_OVERFLOW: return "MIDI_BUFFER_OVERFLOW";
            case ErrorCode::MIDI_INVALID_CHANNEL: return "MIDI_INVALID_CHANNEL";
            case ErrorCode::MIDI_INVALID_NOTE: return "MIDI_INVALID_NOTE";
            case ErrorCode::MIDI_INVALID_VELOCITY: return "MIDI_INVALID_VELOCITY";
            
            case ErrorCode::MIDI_FILE_NOT_FOUND: return "MIDI_FILE_NOT_FOUND";
            case ErrorCode::MIDI_FILE_OPEN_FAILED: return "MIDI_FILE_OPEN_FAILED";
            case ErrorCode::MIDI_FILE_READ_FAILED: return "MIDI_FILE_READ_FAILED";
            case ErrorCode::MIDI_FILE_WRITE_FAILED: return "MIDI_FILE_WRITE_FAILED";
            case ErrorCode::MIDI_FILE_INVALID_FORMAT: return "MIDI_FILE_INVALID_FORMAT";
            case ErrorCode::MIDI_FILE_CORRUPTED: return "MIDI_FILE_CORRUPTED";
            case ErrorCode::MIDI_FILE_TOO_LARGE: return "MIDI_FILE_TOO_LARGE";
            
            case ErrorCode::NETWORK_NOT_AVAILABLE: return "NETWORK_NOT_AVAILABLE";
            case ErrorCode::NETWORK_CONNECTION_FAILED: return "NETWORK_CONNECTION_FAILED";
            case ErrorCode::NETWORK_DISCONNECTED: return "NETWORK_DISCONNECTED";
            case ErrorCode::NETWORK_TIMEOUT: return "NETWORK_TIMEOUT";
            case ErrorCode::NETWORK_INVALID_ADDRESS: return "NETWORK_INVALID_ADDRESS";
            case ErrorCode::NETWORK_SEND_FAILED: return "NETWORK_SEND_FAILED";
            case ErrorCode::NETWORK_RECEIVE_FAILED: return "NETWORK_RECEIVE_FAILED";
            
            case ErrorCode::API_INVALID_COMMAND: return "API_INVALID_COMMAND";
            case ErrorCode::API_INVALID_PARAMETERS: return "API_INVALID_PARAMETERS";
            case ErrorCode::API_UNAUTHORIZED: return "API_UNAUTHORIZED";
            case ErrorCode::API_FORBIDDEN: return "API_FORBIDDEN";
            case ErrorCode::API_NOT_FOUND: return "API_NOT_FOUND";
            case ErrorCode::API_INTERNAL_ERROR: return "API_INTERNAL_ERROR";
            
            case ErrorCode::CONFIG_FILE_NOT_FOUND: return "CONFIG_FILE_NOT_FOUND";
            case ErrorCode::CONFIG_PARSE_ERROR: return "CONFIG_PARSE_ERROR";
            case ErrorCode::CONFIG_INVALID_VALUE: return "CONFIG_INVALID_VALUE";
            case ErrorCode::CONFIG_SAVE_FAILED: return "CONFIG_SAVE_FAILED";
            
            case ErrorCode::DATABASE_OPEN_FAILED: return "DATABASE_OPEN_FAILED";
            case ErrorCode::DATABASE_QUERY_FAILED: return "DATABASE_QUERY_FAILED";
            case ErrorCode::DATABASE_INSERT_FAILED: return "DATABASE_INSERT_FAILED";
            case ErrorCode::DATABASE_UPDATE_FAILED: return "DATABASE_UPDATE_FAILED";
            case ErrorCode::DATABASE_DELETE_FAILED: return "DATABASE_DELETE_FAILED";
            case ErrorCode::DATABASE_NOT_FOUND: return "DATABASE_NOT_FOUND";
            
            case ErrorCode::PROCESSOR_NOT_FOUND: return "PROCESSOR_NOT_FOUND";
            case ErrorCode::PROCESSOR_CREATE_FAILED: return "PROCESSOR_CREATE_FAILED";
            case ErrorCode::PROCESSOR_INVALID_CONFIG: return "PROCESSOR_INVALID_CONFIG";
            case ErrorCode::CHAIN_FULL: return "CHAIN_FULL";
            
            case ErrorCode::SYSTEM_RESOURCE_EXHAUSTED: return "SYSTEM_RESOURCE_EXHAUSTED";
            case ErrorCode::SYSTEM_PERMISSION_DENIED: return "SYSTEM_PERMISSION_DENIED";
            case ErrorCode::SYSTEM_NOT_SUPPORTED: return "SYSTEM_NOT_SUPPORTED";
            case ErrorCode::SYSTEM_HARDWARE_ERROR: return "SYSTEM_HARDWARE_ERROR";
            
            default: return "UNKNOWN";
        }
    }

private:
    std::string formatMessage() const {
        return "[" + getErrorName(code_) + " (" + 
               std::to_string(static_cast<int>(code_)) + ")] " + message_;
    }
    
    ErrorCode code_;
    std::string message_;
    std::string fullMessage_;
};

/**
 * @brief Macro pour lancer une exception
 */
#define THROW_ERROR(code, message) \
    throw midiMind::MidiMindException(code, message)

/**
 * @brief Macro pour vérifier une condition
 */
#define CHECK(condition, code, message) \
    if (!(condition)) { \
        THROW_ERROR(code, message); \
    }

/**
 * @brief Macro pour vérifier un pointeur non-null
 */
#define CHECK_NOT_NULL(ptr, message) \
    CHECK(ptr != nullptr, midiMind::ErrorCode::NULL_POINTER, message)

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Error.h
// ============================================================================
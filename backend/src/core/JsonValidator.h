// ============================================================================
// File: backend/src/core/JsonValidator.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Complete JSON validator with security checks and MIDI-specific validation.
//   Provides static methods for validating JSON data with type checking,
//   range validation, format validation, and security checks.
//
// Features:
//   - Basic type validation (string, int, bool, array, object)
//   - Range and length constraints
//   - Format validation (email, URL, IPv4, device ID)
//   - Security validation (SQL injection, XSS, path traversal)
//   - MIDI-specific validation (channel, note, velocity, CC)
//   - Schema-based validation with builder pattern
//
// Dependencies:
//   - nlohmann/json (JSON library)
//   - Logger (for error reporting)
//   - Error (for exceptions)
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Unified validation interface
//   - Enhanced security checks
//   - MIDI validation methods
//   - Schema builder pattern
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <regex>
#include <memory>
#include <nlohmann/json.hpp>
#include "Logger.h"
#include "Error.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ENUMS: JSON Types
// ============================================================================

enum class JsonType {
    STRING,
    NUMBER,
    INTEGER,
    BOOLEAN,
    ARRAY,
    OBJECT,
    NULL_TYPE,
    ANY
};

// ============================================================================
// STRUCTURE: Field Schema
// ============================================================================

struct FieldSchema {
    std::string name;
    JsonType type;
    bool required;
    
    // Numeric constraints
    int minValue;
    int maxValue;
    
    // String constraints
    size_t minLength;
    size_t maxLength;
    std::string pattern;
    
    // Array constraints
    size_t minSize;
    size_t maxSize;
    
    // Enum validation
    std::vector<std::string> enumValues;
    
    // Custom validator
    std::function<bool(const json&, std::string&)> customValidator;
    
    // Constructor with defaults
    FieldSchema(const std::string& fieldName, JsonType fieldType, bool isRequired = false)
        : name(fieldName)
        , type(fieldType)
        , required(isRequired)
        , minValue(INT_MIN)
        , maxValue(INT_MAX)
        , minLength(0)
        , maxLength(SIZE_MAX)
        , pattern("")
        , minSize(0)
        , maxSize(SIZE_MAX)
        , enumValues()
        , customValidator(nullptr)
    {}
};

// ============================================================================
// CLASS: JsonValidator
// ============================================================================

/**
 * @class JsonValidator
 * @brief Complete JSON validator with security and MIDI validation
 * 
 * Provides both static methods for quick validation and schema-based
 * validation for complex JSON structures. Thread-safe for static methods.
 */
class JsonValidator {
public:
    // ========================================================================
    // CONSTRUCTORS
    // ========================================================================
    
    JsonValidator() = default;
    ~JsonValidator() = default;
    
    // ========================================================================
    // BASIC FIELD VALIDATION (STATIC)
    // ========================================================================
    
    /**
     * @brief Check if a field exists and is not null
     * @param obj JSON object to check
     * @param field Field name
     * @return true if field exists and is not null
     */
    static bool hasField(const json& obj, const std::string& field) {
        return obj.contains(field) && !obj[field].is_null();
    }
    
    /**
     * @brief Require a field to exist
     * @param obj JSON object
     * @param field Field name
     * @param error Error message output
     * @return true if field exists, false otherwise
     */
    static bool requireField(const json& obj, const std::string& field, std::string& error) {
        if (!hasField(obj, field)) {
            error = "Missing required field: " + field;
            Logger::warning("JsonValidator", error);
            return false;
        }
        return true;
    }
    
    // ========================================================================
    // TYPE VALIDATION (STATIC)
    // ========================================================================
    
    /**
     * @brief Validate and extract string field
     * @param obj JSON object
     * @param field Field name
     * @param result Output string value
     * @param maxLength Maximum allowed length (0 = no limit)
     * @param error Error message output
     * @return true if valid
     */
    static bool validateString(const json& obj, const std::string& field,
                              std::string& result, size_t maxLength = 0,
                              std::string& error) {
        if (!hasField(obj, field)) {
            error = "Missing field: " + field;
            return false;
        }
        
        if (!obj[field].is_string()) {
            error = "Field must be string: " + field;
            return false;
        }
        
        result = obj[field].get<std::string>();
        
        if (maxLength > 0 && result.length() > maxLength) {
            error = "String too long (max " + std::to_string(maxLength) + "): " + field;
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate and extract integer field
     * @param obj JSON object
     * @param field Field name
     * @param result Output integer value
     * @param error Error message output
     * @return true if valid
     */
    static bool validateInt(const json& obj, const std::string& field,
                           int& result, std::string& error) {
        if (!hasField(obj, field)) {
            error = "Missing field: " + field;
            return false;
        }
        
        if (!obj[field].is_number_integer()) {
            error = "Field must be integer: " + field;
            return false;
        }
        
        result = obj[field].get<int>();
        return true;
    }
    
    /**
     * @brief Validate integer field with range
     * @param obj JSON object
     * @param field Field name
     * @param result Output integer value
     * @param minValue Minimum allowed value
     * @param maxValue Maximum allowed value
     * @param error Error message output
     * @return true if valid and in range
     */
    static bool validateRange(const json& obj, const std::string& field,
                             int& result, int minValue, int maxValue,
                             std::string& error) {
        if (!validateInt(obj, field, result, error)) {
            return false;
        }
        
        if (result < minValue || result > maxValue) {
            error = "Value out of range [" + std::to_string(minValue) + 
                   ", " + std::to_string(maxValue) + "]: " + field;
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate and extract boolean field
     * @param obj JSON object
     * @param field Field name
     * @param result Output boolean value
     * @param error Error message output
     * @return true if valid
     */
    static bool validateBool(const json& obj, const std::string& field,
                            bool& result, std::string& error) {
        if (!hasField(obj, field)) {
            error = "Missing field: " + field;
            return false;
        }
        
        if (!obj[field].is_boolean()) {
            error = "Field must be boolean: " + field;
            return false;
        }
        
        result = obj[field].get<bool>();
        return true;
    }
    
    /**
     * @brief Validate and extract array field
     * @param obj JSON object
     * @param field Field name
     * @param result Output array
     * @param minSize Minimum array size (0 = no limit)
     * @param maxSize Maximum array size (0 = no limit)
     * @param error Error message output
     * @return true if valid
     */
    static bool validateArray(const json& obj, const std::string& field,
                             json& result, size_t minSize = 0,
                             size_t maxSize = 0, std::string& error) {
        if (!hasField(obj, field)) {
            error = "Missing field: " + field;
            return false;
        }
        
        if (!obj[field].is_array()) {
            error = "Field must be array: " + field;
            return false;
        }
        
        result = obj[field];
        size_t size = result.size();
        
        if (minSize > 0 && size < minSize) {
            error = "Array too small (min " + std::to_string(minSize) + "): " + field;
            return false;
        }
        
        if (maxSize > 0 && size > maxSize) {
            error = "Array too large (max " + std::to_string(maxSize) + "): " + field;
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate and extract object field
     * @param obj JSON object
     * @param field Field name
     * @param result Output object
     * @param error Error message output
     * @return true if valid
     */
    static bool validateObject(const json& obj, const std::string& field,
                               json& result, std::string& error) {
        if (!hasField(obj, field)) {
            error = "Missing field: " + field;
            return false;
        }
        
        if (!obj[field].is_object()) {
            error = "Field must be object: " + field;
            return false;
        }
        
        result = obj[field];
        return true;
    }
    
    // ========================================================================
    // FORMAT VALIDATION (STATIC)
    // ========================================================================
    
    /**
     * @brief Validate email format
     * @param email Email string to validate
     * @param error Error message output
     * @return true if valid email format
     */
    static bool validateEmail(const std::string& email, std::string& error) {
        static const std::regex emailRegex(
            R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)"
        );
        
        if (!std::regex_match(email, emailRegex)) {
            error = "Invalid email format: " + email;
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate URL format
     * @param url URL string to validate
     * @param error Error message output
     * @return true if valid URL format
     */
    static bool validateUrl(const std::string& url, std::string& error) {
        static const std::regex urlRegex(
            R"(^https?://[a-zA-Z0-9\-\.]+\.[a-zA-Z]{2,}(/.*)?$)"
        );
        
        if (!std::regex_match(url, urlRegex)) {
            error = "Invalid URL format: " + url;
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate IPv4 address
     * @param ip IP address string to validate
     * @param error Error message output
     * @return true if valid IPv4 address
     */
    static bool validateIPv4(const std::string& ip, std::string& error) {
        static const std::regex ipRegex(
            R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)"
        );
        
        std::smatch match;
        if (!std::regex_match(ip, match, ipRegex)) {
            error = "Invalid IPv4 format: " + ip;
            return false;
        }
        
        // Check each octet is 0-255
        for (int i = 1; i <= 4; i++) {
            int octet = std::stoi(match[i].str());
            if (octet < 0 || octet > 255) {
                error = "Invalid IPv4 octet (must be 0-255): " + ip;
                return false;
            }
        }
        
        return true;
    }
    
    /**
     * @brief Validate device ID format
     * @param deviceId Device ID to validate
     * @param error Error message output
     * @return true if valid device ID format
     */
    static bool validateDeviceId(const std::string& deviceId, std::string& error) {
        // Format: device_type_client_port (e.g., "device_usb_128_0")
        static const std::regex deviceIdRegex(
            R"(^device_(usb|wifi|bt|virtual)_\d+_\d+$)"
        );
        
        if (!std::regex_match(deviceId, deviceIdRegex)) {
            error = "Invalid device ID format: " + deviceId;
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate file path (no traversal attacks)
     * @param path File path to validate
     * @param error Error message output
     * @return true if valid and safe path
     */
    static bool validatePath(const std::string& path, std::string& error) {
        // Check for path traversal attempts
        if (path.find("..") != std::string::npos) {
            error = "Path traversal not allowed: " + path;
            return false;
        }
        
        // Check for absolute paths (should be relative)
        if (!path.empty() && path[0] == '/') {
            error = "Absolute paths not allowed: " + path;
            return false;
        }
        
        // Check for dangerous characters
        static const std::string dangerousChars = "<>:|\"?*";
        for (char c : dangerousChars) {
            if (path.find(c) != std::string::npos) {
                error = "Invalid character in path: " + std::string(1, c);
                return false;
            }
        }
        
        return true;
    }
    
    // ========================================================================
    // SECURITY VALIDATION (STATIC)
    // ========================================================================
    
    /**
     * @brief Check for SQL injection patterns
     * @param input Input string to check
     * @param error Error message output
     * @return true if safe, false if suspicious
     */
    static bool validateSqlInjection(const std::string& input, std::string& error) {
        // Common SQL injection patterns
        static const std::vector<std::string> sqlPatterns = {
            "' OR '1'='1",
            "'; DROP TABLE",
            "'; DELETE FROM",
            "' UNION SELECT",
            "' OR 1=1",
            "--",
            "/*",
            "*/"
        };
        
        std::string lowerInput = input;
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        
        for (const auto& pattern : sqlPatterns) {
            std::string lowerPattern = pattern;
            std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
            
            if (lowerInput.find(lowerPattern) != std::string::npos) {
                error = "Potential SQL injection detected: " + pattern;
                return false;
            }
        }
        
        return true;
    }
    
    /**
     * @brief Check for XSS (Cross-Site Scripting) patterns
     * @param input Input string to check
     * @param error Error message output
     * @return true if safe, false if suspicious
     */
    static bool validateXss(const std::string& input, std::string& error) {
        // Common XSS patterns
        static const std::vector<std::string> xssPatterns = {
            "<script",
            "</script>",
            "javascript:",
            "onerror=",
            "onload=",
            "<iframe"
        };
        
        std::string lowerInput = input;
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        
        for (const auto& pattern : xssPatterns) {
            if (lowerInput.find(pattern) != std::string::npos) {
                error = "Potential XSS detected: " + pattern;
                return false;
            }
        }
        
        return true;
    }
    
    // ========================================================================
    // MIDI VALIDATION (STATIC)
    // ========================================================================
    
    /**
     * @brief Validate MIDI channel (0-15)
     * @param channel MIDI channel to validate
     * @param error Error message output
     * @return true if valid MIDI channel
     */
    static bool validateMidiChannel(int channel, std::string& error) {
        if (channel < 0 || channel > 15) {
            error = "Invalid MIDI channel (must be 0-15): " + std::to_string(channel);
            return false;
        }
        return true;
    }
    
    /**
     * @brief Validate MIDI note (0-127)
     * @param note MIDI note to validate
     * @param error Error message output
     * @return true if valid MIDI note
     */
    static bool validateMidiNote(int note, std::string& error) {
        if (note < 0 || note > 127) {
            error = "Invalid MIDI note (must be 0-127): " + std::to_string(note);
            return false;
        }
        return true;
    }
    
    /**
     * @brief Validate MIDI velocity (0-127)
     * @param velocity MIDI velocity to validate
     * @param error Error message output
     * @return true if valid MIDI velocity
     */
    static bool validateMidiVelocity(int velocity, std::string& error) {
        if (velocity < 0 || velocity > 127) {
            error = "Invalid MIDI velocity (must be 0-127): " + std::to_string(velocity);
            return false;
        }
        return true;
    }
    
    /**
     * @brief Validate MIDI CC (Control Change) number (0-127)
     * @param cc MIDI CC number to validate
     * @param error Error message output
     * @return true if valid MIDI CC number
     */
    static bool validateMidiCC(int cc, std::string& error) {
        if (cc < 0 || cc > 127) {
            error = "Invalid MIDI CC (must be 0-127): " + std::to_string(cc);
            return false;
        }
        return true;
    }
    
    /**
     * @brief Validate MIDI program number (0-127)
     * @param program MIDI program number to validate
     * @param error Error message output
     * @return true if valid MIDI program number
     */
    static bool validateMidiProgram(int program, std::string& error) {
        if (program < 0 || program > 127) {
            error = "Invalid MIDI program (must be 0-127): " + std::to_string(program);
            return false;
        }
        return true;
    }
    
    // ========================================================================
    // SCHEMA-BASED VALIDATION
    // ========================================================================
    
    /**
     * @brief Add a field schema
     * @param schema Field schema to add
     * @return Reference to this validator (for chaining)
     */
    JsonValidator& addField(const FieldSchema& schema) {
        fields_.push_back(schema);
        return *this;
    }
    
    /**
     * @brief Validate JSON data against defined schema
     * @param data JSON data to validate
     * @param errorMessage Output error message
     * @return true if validation passed
     */
    bool validate(const json& data, std::string& errorMessage) const {
        errorMessage.clear();
        
        // Check root is object
        if (!data.is_object()) {
            errorMessage = "Root element must be an object";
            return false;
        }
        
        // Validate each field in schema
        for (const auto& field : fields_) {
            bool fieldExists = data.contains(field.name);
            
            // Check required fields
            if (field.required && !fieldExists) {
                errorMessage = "Required field '" + field.name + "' is missing";
                return false;
            }
            
            // Skip optional missing fields
            if (!fieldExists) {
                continue;
            }
            
            const json& value = data[field.name];
            
            // Validate type
            if (!validateType(value, field, errorMessage)) {
                errorMessage = "Field '" + field.name + "': " + errorMessage;
                return false;
            }
            
            // Validate constraints
            if (!validateConstraints(value, field, errorMessage)) {
                errorMessage = "Field '" + field.name + "': " + errorMessage;
                return false;
            }
        }
        
        return true;
    }
    
    /**
     * @brief Validate and throw exception on failure
     * @param data JSON data to validate
     * @throws std::runtime_error if validation fails
     */
    void validateOrThrow(const json& data) const {
        std::string errorMessage;
        
        if (!validate(data, errorMessage)) {
            THROW_ERROR(ErrorCode::VALIDATION_FAILED, 
                       "JSON validation failed: " + errorMessage);
        }
    }
    
private:
    std::vector<FieldSchema> fields_;
    
    // ========================================================================
    // PRIVATE VALIDATION HELPERS
    // ========================================================================
    
    /**
     * @brief Validate field type
     */
    bool validateType(const json& value, const FieldSchema& field, 
                     std::string& errorMessage) const {
        switch (field.type) {
            case JsonType::STRING:
                if (!value.is_string()) {
                    errorMessage = "Expected string";
                    return false;
                }
                break;
                
            case JsonType::NUMBER:
                if (!value.is_number()) {
                    errorMessage = "Expected number";
                    return false;
                }
                break;
                
            case JsonType::INTEGER:
                if (!value.is_number_integer()) {
                    errorMessage = "Expected integer";
                    return false;
                }
                break;
                
            case JsonType::BOOLEAN:
                if (!value.is_boolean()) {
                    errorMessage = "Expected boolean";
                    return false;
                }
                break;
                
            case JsonType::ARRAY:
                if (!value.is_array()) {
                    errorMessage = "Expected array";
                    return false;
                }
                break;
                
            case JsonType::OBJECT:
                if (!value.is_object()) {
                    errorMessage = "Expected object";
                    return false;
                }
                break;
                
            case JsonType::NULL_TYPE:
                if (!value.is_null()) {
                    errorMessage = "Expected null";
                    return false;
                }
                break;
                
            case JsonType::ANY:
                // Any type is valid
                break;
        }
        
        return true;
    }
    
    /**
     * @brief Validate field constraints
     */
    bool validateConstraints(const json& value, const FieldSchema& field,
                            std::string& errorMessage) const {
        // Numeric range validation
        if (value.is_number_integer()) {
            int intValue = value.get<int>();
            if (intValue < field.minValue || intValue > field.maxValue) {
                errorMessage = "Value out of range [" + 
                             std::to_string(field.minValue) + ", " + 
                             std::to_string(field.maxValue) + "]";
                return false;
            }
        }
        
        // String length and pattern validation
        if (value.is_string()) {
            std::string strValue = value.get<std::string>();
            
            if (strValue.length() < field.minLength) {
                errorMessage = "String too short (min " + 
                             std::to_string(field.minLength) + ")";
                return false;
            }
            
            if (strValue.length() > field.maxLength) {
                errorMessage = "String too long (max " + 
                             std::to_string(field.maxLength) + ")";
                return false;
            }
            
            if (!field.pattern.empty()) {
                std::regex pattern(field.pattern);
                if (!std::regex_match(strValue, pattern)) {
                    errorMessage = "String does not match pattern";
                    return false;
                }
            }
        }
        
        // Array size validation
        if (value.is_array()) {
            size_t size = value.size();
            
            if (size < field.minSize) {
                errorMessage = "Array too small (min " + 
                             std::to_string(field.minSize) + ")";
                return false;
            }
            
            if (size > field.maxSize) {
                errorMessage = "Array too large (max " + 
                             std::to_string(field.maxSize) + ")";
                return false;
            }
        }
        
        // Enum validation
        if (!field.enumValues.empty() && value.is_string()) {
            std::string strValue = value.get<std::string>();
            bool found = false;
            
            for (const auto& enumValue : field.enumValues) {
                if (strValue == enumValue) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                errorMessage = "Value not in enum list";
                return false;
            }
        }
        
        // Custom validator
        if (field.customValidator) {
            if (!field.customValidator(value, errorMessage)) {
                return false;
            }
        }
        
        return true;
    }
};

// ============================================================================
// PREDEFINED SCHEMAS
// ============================================================================

/**
 * @brief Create validator for MIDI message command
 */
inline JsonValidator createMidiMessageValidator() {
    JsonValidator validator;
    
    validator.addField(FieldSchema("type", JsonType::STRING, true))
             .addField(FieldSchema("channel", JsonType::INTEGER, true))
             .addField(FieldSchema("note", JsonType::INTEGER, false))
             .addField(FieldSchema("velocity", JsonType::INTEGER, false))
             .addField(FieldSchema("cc", JsonType::INTEGER, false))
             .addField(FieldSchema("value", JsonType::INTEGER, false));
    
    return validator;
}

/**
 * @brief Create validator for device connection command
 */
inline JsonValidator createDeviceConnectValidator() {
    JsonValidator validator;
    
    validator.addField(FieldSchema("deviceId", JsonType::STRING, true))
             .addField(FieldSchema("timeout", JsonType::INTEGER, false));
    
    return validator;
}

} // namespace midiMind
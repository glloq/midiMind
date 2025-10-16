// ============================================================================
// File: backend/src/core/JsonValidator.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of the JSON validator with complete validation rules.
//   Since most methods in JsonValidator.h are static and inline, this file
//   contains only the schema-based validation implementation.
//
// Dependencies:
//   - JsonValidator.h
//   - Logger.h
//   - Error.h
//   - nlohmann/json
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified implementation (most methods are static inline in header)
//   - Focus on schema-based validation
//   - Enhanced error reporting
//
// ============================================================================

#include "JsonValidator.h"
#include <algorithm>
#include <cctype>

namespace midiMind {

// ============================================================================
// NOTE: Static validation methods are implemented inline in the header
// ============================================================================
//
// The following methods are implemented in JsonValidator.h as static inline:
// - hasField()
// - requireField()
// - validateString()
// - validateInt()
// - validateRange()
// - validateBool()
// - validateArray()
// - validateObject()
// - validateEmail()
// - validateUrl()
// - validateIPv4()
// - validateDeviceId()
// - validatePath()
// - validateSqlInjection()
// - validateXss()
// - validateMidiChannel()
// - validateMidiNote()
// - validateMidiVelocity()
// - validateMidiCC()
// - validateMidiProgram()
//
// This keeps them header-only for performance and simplicity.
// Only non-static methods and complex logic are implemented here.
// ============================================================================

// ============================================================================
// SCHEMA-BASED VALIDATION IMPLEMENTATION
// ============================================================================

/**
 * @brief Validate JSON data against defined schema
 * 
 * This method validates a JSON object against all fields defined in the schema.
 * It checks:
 * 1. Root element is an object
 * 2. All required fields are present
 * 3. All present fields have correct types
 * 4. All present fields satisfy their constraints
 * 
 * @param data JSON data to validate
 * @param errorMessage Output error message if validation fails
 * @return true if validation passed, false otherwise
 */
bool JsonValidator::validate(const json& data, std::string& errorMessage) const {
    errorMessage.clear();
    
    // Check root is object
    if (!data.is_object()) {
        errorMessage = "Root element must be an object";
        Logger::warning("JsonValidator", errorMessage);
        return false;
    }
    
    // Validate each field in schema
    for (const auto& field : fields_) {
        bool fieldExists = data.contains(field.name);
        
        // Check required fields
        if (field.required && !fieldExists) {
            errorMessage = "Required field '" + field.name + "' is missing";
            Logger::warning("JsonValidator", errorMessage);
            return false;
        }
        
        // Skip optional missing fields
        if (!fieldExists) {
            continue;
        }
        
        const json& value = data[field.name];
        
        // Validate type
        std::string typeError;
        if (!validateType(value, field, typeError)) {
            errorMessage = "Field '" + field.name + "': " + typeError;
            Logger::warning("JsonValidator", errorMessage);
            return false;
        }
        
        // Validate constraints
        std::string constraintError;
        if (!validateConstraints(value, field, constraintError)) {
            errorMessage = "Field '" + field.name + "': " + constraintError;
            Logger::warning("JsonValidator", errorMessage);
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Validate and throw exception on failure
 * 
 * Convenience method that validates JSON and throws an exception
 * if validation fails. Useful for cases where validation failure
 * should stop execution.
 * 
 * @param data JSON data to validate
 * @throws std::runtime_error if validation fails
 */
void JsonValidator::validateOrThrow(const json& data) const {
    std::string errorMessage;
    
    if (!validate(data, errorMessage)) {
        Logger::error("JsonValidator", "Validation failed: " + errorMessage);
        THROW_ERROR(ErrorCode::VALIDATION_FAILED, 
                   "JSON validation failed: " + errorMessage);
    }
}

// ============================================================================
// PRIVATE VALIDATION HELPERS
// ============================================================================

/**
 * @brief Validate field type
 * 
 * Checks if the JSON value matches the expected type defined in the schema.
 * Supports all JSON types including ANY for polymorphic fields.
 * 
 * @param value JSON value to validate
 * @param field Field schema with expected type
 * @param errorMessage Output error message if validation fails
 * @return true if type matches, false otherwise
 */
bool JsonValidator::validateType(const json& value, const FieldSchema& field, 
                                 std::string& errorMessage) const {
    switch (field.type) {
        case JsonType::STRING:
            if (!value.is_string()) {
                errorMessage = "Expected string, got " + std::string(value.type_name());
                return false;
            }
            break;
            
        case JsonType::NUMBER:
            if (!value.is_number()) {
                errorMessage = "Expected number, got " + std::string(value.type_name());
                return false;
            }
            break;
            
        case JsonType::INTEGER:
            if (!value.is_number_integer()) {
                errorMessage = "Expected integer, got " + std::string(value.type_name());
                return false;
            }
            break;
            
        case JsonType::BOOLEAN:
            if (!value.is_boolean()) {
                errorMessage = "Expected boolean, got " + std::string(value.type_name());
                return false;
            }
            break;
            
        case JsonType::ARRAY:
            if (!value.is_array()) {
                errorMessage = "Expected array, got " + std::string(value.type_name());
                return false;
            }
            break;
            
        case JsonType::OBJECT:
            if (!value.is_object()) {
                errorMessage = "Expected object, got " + std::string(value.type_name());
                return false;
            }
            break;
            
        case JsonType::NULL_TYPE:
            if (!value.is_null()) {
                errorMessage = "Expected null, got " + std::string(value.type_name());
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
 * 
 * Validates type-specific constraints such as:
 * - Numeric: min/max range
 * - String: min/max length, pattern, enum
 * - Array: min/max size
 * - Custom: user-defined validator function
 * 
 * @param value JSON value to validate
 * @param field Field schema with constraints
 * @param errorMessage Output error message if validation fails
 * @return true if all constraints satisfied, false otherwise
 */
bool JsonValidator::validateConstraints(const json& value, const FieldSchema& field,
                                        std::string& errorMessage) const {
    // Numeric range validation
    if (value.is_number_integer()) {
        int intValue = value.get<int>();
        
        if (intValue < field.minValue) {
            errorMessage = "Value too small (min: " + std::to_string(field.minValue) + 
                         ", got: " + std::to_string(intValue) + ")";
            return false;
        }
        
        if (intValue > field.maxValue) {
            errorMessage = "Value too large (max: " + std::to_string(field.maxValue) + 
                         ", got: " + std::to_string(intValue) + ")";
            return false;
        }
    }
    
    // String validation
    if (value.is_string()) {
        std::string strValue = value.get<std::string>();
        
        // Length constraints
        if (field.minLength > 0 && strValue.length() < field.minLength) {
            errorMessage = "String too short (min: " + std::to_string(field.minLength) + 
                         ", got: " + std::to_string(strValue.length()) + ")";
            return false;
        }
        
        if (field.maxLength < SIZE_MAX && strValue.length() > field.maxLength) {
            errorMessage = "String too long (max: " + std::to_string(field.maxLength) + 
                         ", got: " + std::to_string(strValue.length()) + ")";
            return false;
        }
        
        // Pattern validation
        if (!field.pattern.empty()) {
            try {
                std::regex pattern(field.pattern);
                if (!std::regex_match(strValue, pattern)) {
                    errorMessage = "String does not match pattern: " + field.pattern;
                    return false;
                }
            } catch (const std::regex_error& e) {
                Logger::error("JsonValidator", 
                            "Invalid regex pattern '" + field.pattern + "': " + e.what());
                errorMessage = "Internal error: invalid pattern";
                return false;
            }
        }
        
        // Enum validation
        if (!field.enumValues.empty()) {
            bool found = false;
            for (const auto& enumValue : field.enumValues) {
                if (strValue == enumValue) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                errorMessage = "Value '" + strValue + "' not in allowed list";
                return false;
            }
        }
    }
    
    // Array size validation
    if (value.is_array()) {
        size_t size = value.size();
        
        if (field.minSize > 0 && size < field.minSize) {
            errorMessage = "Array too small (min: " + std::to_string(field.minSize) + 
                         ", got: " + std::to_string(size) + ")";
            return false;
        }
        
        if (field.maxSize < SIZE_MAX && size > field.maxSize) {
            errorMessage = "Array too large (max: " + std::to_string(field.maxSize) + 
                         ", got: " + std::to_string(size) + ")";
            return false;
        }
    }
    
    // Custom validator
    if (field.customValidator) {
        std::string customError;
        if (!field.customValidator(value, customError)) {
            errorMessage = customError.empty() ? "Custom validation failed" : customError;
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// PREDEFINED SCHEMAS IMPLEMENTATION
// ============================================================================
//
// Note: createMidiMessageValidator() and createDeviceConnectValidator()
// are implemented inline in the header as they are simple factory functions.
// Additional complex schemas can be implemented here if needed.
//
// ============================================================================

} // namespace midiMind

// ============================================================================
// END OF FILE JsonValidator.cpp
// ============================================================================
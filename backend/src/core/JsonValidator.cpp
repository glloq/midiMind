// ============================================================================
// File: backend/src/core/JsonValidator.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of JsonValidator utility functions.
//
// Changes v4.1.0:
//   - Moved validate() from inline to .cpp
//   - Optimized field checking with unordered_set (O(1) instead of O(n²))
//   - Added error logging in validateRegexPattern()
//   - Extended int validation to int64_t range
//
// ============================================================================

#include "JsonValidator.h"
#include "Logger.h"
#include <regex>

namespace midiMind {

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::string jsonTypeToString(JsonType type) {
    switch (type) {
        case JsonType::OBJECT: return "object";
        case JsonType::ARRAY: return "array";
        case JsonType::STRING: return "string";
        case JsonType::NUMBER: return "number";
        case JsonType::INTEGER: return "integer";
        case JsonType::BOOLEAN: return "boolean";
        case JsonType::NULL_TYPE: return "null";
        case JsonType::ANY: return "any";
    }
    
    // Should never reach here, but handle gracefully
    Logger::warning("JsonValidator", "Unknown JsonType: " + std::to_string(static_cast<int>(type)));
    return "unknown";
}

bool validateRegexPattern(const std::string& value, const std::string& pattern) {
    try {
        std::regex re(pattern);
        return std::regex_match(value, re);
    } catch (const std::regex_error& e) {
        Logger::warning("JsonValidator", 
                       "Regex validation failed for pattern '" + pattern + "': " + e.what());
        return false;
    }
}

// ============================================================================
// JsonValidator IMPLEMENTATION
// ============================================================================

JsonValidator::JsonValidator(const ObjectSchema& schema)
    : schema_(schema) {
    // Pre-populate field names for O(1) lookup
    for (const auto& field : schema_.fields) {
        fieldNames_.insert(field.name);
    }
}

bool JsonValidator::validate(const json& data, std::string& errorMessage) const {
    if (!data.is_object()) {
        errorMessage = "Root must be an object";
        return false;
    }
    
    // Check required fields and validate existing fields
    for (const auto& field : schema_.fields) {
        if (field.required && !data.contains(field.name)) {
            errorMessage = "Missing required field: " + field.name;
            return false;
        }
        
        if (data.contains(field.name)) {
            if (!validateType(data[field.name], field, errorMessage)) {
                return false;
            }
            
            if (!validateConstraints(data[field.name], field, errorMessage)) {
                return false;
            }
        }
    }
    
    // Check additional fields (optimized with unordered_set - O(1) lookup)
    if (!schema_.allowAdditionalFields) {
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (fieldNames_.find(it.key()) == fieldNames_.end()) {
                errorMessage = "Unknown field: " + it.key();
                return false;
            }
        }
    }
    
    return true;
}

void JsonValidator::validateOrThrow(const json& data) const {
    std::string error;
    if (!validate(data, error)) {
        THROW_ERROR(ErrorCode::VALIDATION_FAILED, error);
    }
}

// ============================================================================
// STATIC VALIDATORS
// ============================================================================

bool JsonValidator::validateString(const json& data, 
                                   const std::string& fieldName,
                                   std::string& result, 
                                   size_t maxLength,
                                   std::string& error) {
    if (!data.contains(fieldName)) {
        error = "Missing field: " + fieldName;
        return false;
    }
    
    if (!data[fieldName].is_string()) {
        error = "Field " + fieldName + " must be string";
        return false;
    }
    
    result = data[fieldName].get<std::string>();
    
    if (maxLength > 0 && result.length() > maxLength) {
        error = "Field " + fieldName + " exceeds max length";
        return false;
    }
    
    return true;
}

bool JsonValidator::validateInt(const json& data,
                                const std::string& fieldName,
                                int& result,
                                int minValue,
                                int maxValue,
                                std::string& error) {
    if (!data.contains(fieldName)) {
        error = "Missing field: " + fieldName;
        return false;
    }
    
    if (!data[fieldName].is_number_integer()) {
        error = "Field " + fieldName + " must be integer";
        return false;
    }
    
    result = data[fieldName].get<int>();
    
    if (result < minValue || result > maxValue) {
        error = "Field " + fieldName + " out of range";
        return false;
    }
    
    return true;
}

bool JsonValidator::validateBool(const json& data,
                                 const std::string& fieldName,
                                 bool& result,
                                 std::string& error) {
    if (!data.contains(fieldName)) {
        error = "Missing field: " + fieldName;
        return false;
    }
    
    if (!data[fieldName].is_boolean()) {
        error = "Field " + fieldName + " must be boolean";
        return false;
    }
    
    result = data[fieldName].get<bool>();
    return true;
}

bool JsonValidator::validateArray(const json& data,
                                  const std::string& fieldName,
                                  json& result,
                                  size_t minSize,
                                  size_t maxSize,
                                  std::string& error) {
    if (!data.contains(fieldName)) {
        error = "Missing field: " + fieldName;
        return false;
    }
    
    if (!data[fieldName].is_array()) {
        error = "Field " + fieldName + " must be array";
        return false;
    }
    
    result = data[fieldName];
    size_t size = result.size();
    
    if (size < minSize) {
        error = "Field " + fieldName + " array too small";
        return false;
    }
    
    if (maxSize > 0 && size > maxSize) {
        error = "Field " + fieldName + " array too large";
        return false;
    }
    
    return true;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool JsonValidator::validateType(const json& value, const FieldSchema& field,
                                 std::string& error) const {
    switch (field.type) {
        case JsonType::STRING:
            if (!value.is_string()) {
                error = "Field " + field.name + " must be string";
                return false;
            }
            break;
        
        case JsonType::NUMBER:
            if (!value.is_number()) {
                error = "Field " + field.name + " must be number";
                return false;
            }
            break;
        
        case JsonType::INTEGER:
            if (!value.is_number_integer()) {
                error = "Field " + field.name + " must be integer";
                return false;
            }
            break;
        
        case JsonType::BOOLEAN:
            if (!value.is_boolean()) {
                error = "Field " + field.name + " must be boolean";
                return false;
            }
            break;
        
        case JsonType::OBJECT:
            if (!value.is_object()) {
                error = "Field " + field.name + " must be object";
                return false;
            }
            break;
        
        case JsonType::ARRAY:
            if (!value.is_array()) {
                error = "Field " + field.name + " must be array";
                return false;
            }
            break;
        
        case JsonType::NULL_TYPE:
            if (!value.is_null()) {
                error = "Field " + field.name + " must be null";
                return false;
            }
            break;
        
        case JsonType::ANY:
            // Any type is valid
            break;
    }
    
    return true;
}

bool JsonValidator::validateConstraints(const json& value, const FieldSchema& field,
                                        std::string& error) const {
    // Integer constraints (supports int64_t range)
    if (field.type == JsonType::INTEGER && value.is_number_integer()) {
        int64_t val = value.get<int64_t>();
        if (val < field.minValue || val > field.maxValue) {
            error = "Field " + field.name + " out of range [" + 
                    std::to_string(field.minValue) + ", " + 
                    std::to_string(field.maxValue) + "]";
            return false;
        }
    }
    
    // String constraints
    if (field.type == JsonType::STRING && value.is_string()) {
        std::string str = value.get<std::string>();
        
        if (str.length() < field.minLength || str.length() > field.maxLength) {
            error = "Field " + field.name + " invalid length";
            return false;
        }
        
        if (!field.allowedValues.empty()) {
            bool found = false;
            for (const auto& allowed : field.allowedValues) {
                if (str == allowed) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                error = "Field " + field.name + " has invalid value";
                return false;
            }
        }
        
        // Regex pattern validation
        if (!field.pattern.empty()) {
            if (!validateRegexPattern(str, field.pattern)) {
                error = "Field " + field.name + " does not match pattern";
                return false;
            }
        }
    }
    
    return true;
}

} // namespace midiMind

// ============================================================================
// END OF FILE JsonValidator.cpp
// ============================================================================
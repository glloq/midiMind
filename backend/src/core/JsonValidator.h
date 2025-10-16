// ============================================================================
// File: backend/src/core/JsonValidator.h
// Version: 4.1.0 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.0:
//   - Added #include <climits> for INT_MIN/INT_MAX
//   - Fixed default parameter placement
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <climits>  // ✅ AJOUTÉ pour INT_MIN/INT_MAX
#include "Error.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum JsonType
 * @brief JSON value types
 */
enum class JsonType {
    OBJECT,
    ARRAY,
    STRING,
    NUMBER,
    INTEGER,
    BOOLEAN,
    NULL_TYPE,
    ANY
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct FieldSchema
 * @brief Schema for a single field
 */
struct FieldSchema {
    std::string name;
    JsonType type;
    bool required;
    
    // Constraints
    int minValue;
    int maxValue;
    size_t minLength;
    size_t maxLength;
    std::vector<std::string> allowedValues;
    std::string pattern;  // Regex pattern
    
    // Nested schemas
    std::shared_ptr<struct ObjectSchema> objectSchema;
    std::shared_ptr<FieldSchema> arrayItemSchema;
    
    // Constructor
    FieldSchema(const std::string& fieldName, JsonType fieldType, bool isRequired = true)
        : name(fieldName)
        , type(fieldType)
        , required(isRequired)
        , minValue(INT_MIN)  // ✅ Maintenant défini
        , maxValue(INT_MAX)  // ✅ Maintenant défini
        , minLength(0)
        , maxLength(SIZE_MAX)
        , objectSchema(nullptr)
        , arrayItemSchema(nullptr) {}
};

/**
 * @struct ObjectSchema
 * @brief Schema for JSON object
 */
struct ObjectSchema {
    std::vector<FieldSchema> fields;
    bool allowAdditionalFields;
    
    ObjectSchema() : allowAdditionalFields(false) {}
};

// ============================================================================
// CLASS: JsonValidator
// ============================================================================

/**
 * @class JsonValidator
 * @brief JSON schema validator
 */
class JsonValidator {
public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param schema Object schema
     */
    explicit JsonValidator(const ObjectSchema& schema)
        : schema_(schema) {}
    
    // ========================================================================
    // VALIDATION (INLINE)
    // ========================================================================
    
    /**
     * @brief Validate JSON data
     * @param data JSON to validate
     * @param errorMessage Error message output
     * @return bool true if valid
     */
    inline bool validate(const json& data, std::string& errorMessage) const {
        if (!data.is_object()) {
            errorMessage = "Root must be an object";
            return false;
        }
        
        // Check required fields
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
        
        // Check additional fields
        if (!schema_.allowAdditionalFields) {
            for (auto it = data.begin(); it != data.end(); ++it) {
                bool found = false;
                for (const auto& field : schema_.fields) {
                    if (field.name == it.key()) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    errorMessage = "Unknown field: " + it.key();
                    return false;
                }
            }
        }
        
        return true;
    }
    
    /**
     * @brief Validate or throw exception
     * @param data JSON to validate
     * @throws MidiMindException if validation fails
     */
    inline void validateOrThrow(const json& data) const {
        std::string error;
        if (!validate(data, error)) {
            THROW_ERROR(ErrorCode::VALIDATION_FAILED, error);
        }
    }
    
    // ========================================================================
    // STATIC VALIDATORS
    // ========================================================================
    
    /**
     * @brief Validate string field
     * @param data JSON data
     * @param fieldName Field name
     * @param result Output string
     * @param maxLength Max length (0 = no limit)
     * @param error Error message
     * @return bool true if valid
     */
    static inline bool validateString(const json& data, 
                                     const std::string& fieldName,
                                     std::string& result, 
                                     size_t maxLength,  // ✅ Pas de défaut ici
                                     std::string& error) {  // ✅ Paramètre final
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
    
    /**
     * @brief Validate integer field
     * @param data JSON data
     * @param fieldName Field name
     * @param result Output integer
     * @param minValue Min value
     * @param maxValue Max value
     * @param error Error message
     * @return bool true if valid
     */
    static inline bool validateInt(const json& data,
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
    
    /**
     * @brief Validate boolean field
     * @param data JSON data
     * @param fieldName Field name
     * @param result Output boolean
     * @param error Error message
     * @return bool true if valid
     */
    static inline bool validateBool(const json& data,
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
    
    /**
     * @brief Validate array field
     * @param data JSON data
     * @param fieldName Field name
     * @param result Output array
     * @param minSize Min size
     * @param maxSize Max size (0 = no limit)
     * @param error Error message
     * @return bool true if valid
     */
    static inline bool validateArray(const json& data,
                                    const std::string& fieldName,
                                    json& result,
                                    size_t minSize,  // ✅ Pas de défaut ici
                                    size_t maxSize,  // ✅ Pas de défaut ici
                                    std::string& error) {  // ✅ Paramètre final
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

private:
    // ========================================================================
    // PRIVATE METHODS (INLINE)
    // ========================================================================
    
    inline bool validateType(const json& value, const FieldSchema& field,
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
    
    inline bool validateConstraints(const json& value, const FieldSchema& field,
                                   std::string& error) const {
        // Integer constraints
        if (field.type == JsonType::INTEGER && value.is_number_integer()) {
            int val = value.get<int>();
            if (val < field.minValue || val > field.maxValue) {
                error = "Field " + field.name + " out of range";
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
        }
        
        return true;
    }
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    ObjectSchema schema_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE JsonValidator.h
// ============================================================================
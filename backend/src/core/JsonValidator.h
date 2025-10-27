// ============================================================================
// File: backend/src/core/JsonValidator.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   JSON schema validator for API request/response validation.
//
// Changes v4.1.0:
//   - objectSchema/arrayItemSchema: shared_ptr → unique_ptr
//   - minValue/maxValue: int → int64_t
//   - validate() moved to .cpp (no longer inline)
//   - Optimized field checking with unordered_set
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <climits>
#include <nlohmann/json.hpp>
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
    int64_t minValue;
    int64_t maxValue;
    size_t minLength;
    size_t maxLength;
    std::vector<std::string> allowedValues;
    std::string pattern;  // Regex pattern
    
    // Nested schemas (use unique_ptr for ownership)
    std::unique_ptr<struct ObjectSchema> objectSchema;
    std::unique_ptr<FieldSchema> arrayItemSchema;
    
    // Constructor
    FieldSchema(const std::string& fieldName, JsonType fieldType, bool isRequired = true)
        : name(fieldName)
        , type(fieldType)
        , required(isRequired)
        , minValue(INT64_MIN)
        , maxValue(INT64_MAX)
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
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Convert JsonType to string
 * @param type JsonType
 * @return std::string Type name
 */
std::string jsonTypeToString(JsonType type);

/**
 * @brief Validate regex pattern
 * @param value String to validate
 * @param pattern Regex pattern
 * @return bool true if matches
 */
bool validateRegexPattern(const std::string& value, const std::string& pattern);

// ============================================================================
// CLASS: JsonValidator
// ============================================================================

/**
 * @class JsonValidator
 * @brief JSON schema validator
 * 
 * Validates JSON data against a defined schema with type checking,
 * constraints, and nested object/array validation.
 * 
 * Thread Safety: YES (read-only after construction)
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
    explicit JsonValidator(const ObjectSchema& schema);
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Validate JSON data
     * @param data JSON to validate
     * @param errorMessage Error message output
     * @return bool true if valid
     */
    bool validate(const json& data, std::string& errorMessage) const;
    
    /**
     * @brief Validate or throw exception
     * @param data JSON to validate
     * @throws MidiMindException if validation fails
     */
    void validateOrThrow(const json& data) const;
    
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
    static bool validateString(const json& data, 
                              const std::string& fieldName,
                              std::string& result, 
                              size_t maxLength,
                              std::string& error);
    
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
    static bool validateInt(const json& data,
                           const std::string& fieldName,
                           int& result,
                           int minValue,
                           int maxValue,
                           std::string& error);
    
    /**
     * @brief Validate boolean field
     * @param data JSON data
     * @param fieldName Field name
     * @param result Output boolean
     * @param error Error message
     * @return bool true if valid
     */
    static bool validateBool(const json& data,
                            const std::string& fieldName,
                            bool& result,
                            std::string& error);
    
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
    static bool validateArray(const json& data,
                             const std::string& fieldName,
                             json& result,
                             size_t minSize,
                             size_t maxSize,
                             std::string& error);

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Validate type of JSON value
     * @param value Value to validate
     * @param field Field schema
     * @param error Error message output
     * @return bool true if valid
     */
    bool validateType(const json& value, const FieldSchema& field,
                     std::string& error) const;
    
    /**
     * @brief Validate constraints on JSON value
     * @param value Value to validate
     * @param field Field schema
     * @param error Error message output
     * @return bool true if valid
     */
    bool validateConstraints(const json& value, const FieldSchema& field,
                            std::string& error) const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    ObjectSchema schema_;
    std::unordered_set<std::string> fieldNames_;  // For O(1) field lookup
};

} // namespace midiMind

// ============================================================================
// END OF FILE JsonValidator.h
// ============================================================================
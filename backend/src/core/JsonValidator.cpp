// ============================================================================
// File: backend/src/core/JsonValidator.cpp
// Version: 4.1.0 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of JsonValidator utility functions.
//   Main validation methods are inline in header.
//
// Changes v4.1.0:
//   - Removed duplicate implementations (already inline in .h)
//   - Kept only non-inline utility functions
//
// ============================================================================

#include "JsonValidator.h"
#include <regex>

namespace midiMind {

// ============================================================================
// UTILITY FUNCTIONS (NON-INLINE)
// ============================================================================

/**
 * @brief Convert JsonType to string
 * @param type JsonType
 * @return std::string Type name
 */
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
        default: return "unknown";
    }
}

/**
 * @brief Validate regex pattern
 * @param value String to validate
 * @param pattern Regex pattern
 * @return bool true if matches
 */
bool validateRegexPattern(const std::string& value, const std::string& pattern) {
    try {
        std::regex re(pattern);
        return std::regex_match(value, re);
    } catch (const std::regex_error& e) {
        return false;
    }
}

// ============================================================================
// FONCTIONS INLINE DÉJÀ DÉFINIES DANS .h - NE PAS DUPLIQUER
// ============================================================================

// Les fonctions suivantes sont définies inline dans JsonValidator.h
// et ne doivent PAS être réimplémentées ici :
//
// - bool JsonValidator::validate(const json& data, std::string& errorMessage) const
// - void JsonValidator::validateOrThrow(const json& data) const
// - bool JsonValidator::validateType(const json& value, const FieldSchema& field, std::string& error) const
// - bool JsonValidator::validateConstraints(const json& value, const FieldSchema& field, std::string& error) const
//
// Si ces fonctions étaient présentes ici, elles ont été supprimées
// pour éviter les erreurs de "redéfinition" lors de la compilation.

} // namespace midiMind

// ============================================================================
// END OF FILE JsonValidator.cpp
// ============================================================================
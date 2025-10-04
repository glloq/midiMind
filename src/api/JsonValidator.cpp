// ============================================================================
// Fichier: src/api/JsonValidator.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "JsonValidator.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

JsonValidator::JsonValidator() {
    Logger::debug("JsonValidator", "JsonValidator created");
}

JsonValidator::~JsonValidator() {
    Logger::debug("JsonValidator", "JsonValidator destroyed");
}

// ============================================================================
// DÉFINITION DU SCHÉMA
// ============================================================================

void JsonValidator::addField(const FieldSchema& field) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Retirer le champ existant si présent
    fields_.erase(
        std::remove_if(fields_.begin(), fields_.end(),
            [&field](const FieldSchema& f) { return f.name == field.name; }),
        fields_.end()
    );
    
    fields_.push_back(field);
}

void JsonValidator::removeField(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    fields_.erase(
        std::remove_if(fields_.begin(), fields_.end(),
            [&name](const FieldSchema& f) { return f.name == name; }),
        fields_.end()
    );
}

void JsonValidator::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    fields_.clear();
}

// ============================================================================
// VALIDATION
// ============================================================================

ValidationResult JsonValidator::validate(const json& data) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ValidationResult result;
    
    // Vérifier que c'est un objet
    if (!data.is_object()) {
        result.addError("Root must be an object");
        return result;
    }
    
    // Valider chaque champ du schéma
    for (const auto& field : fields_) {
        // Vérifier si le champ est présent
        if (!data.contains(field.name)) {
            if (field.required) {
                result.addError("Missing required field: " + field.name);
            }
            continue;
        }
        
        const json& value = data[field.name];
        
        // Valider le type
        if (!validateType(field, value, result)) {
            continue;
        }
        
        // Valider les contraintes
        validateConstraints(field, value, result);
    }
    
    return result;
}

void JsonValidator::validateOrThrow(const json& data) const {
    auto result = validate(data);
    
    if (!result.valid) {
        THROW_ERROR(ErrorCode::API_INVALID_PARAMETERS, result.getMessage());
    }
}

// ============================================================================
// BUILDERS FLUENT API
// ============================================================================

JsonValidator::StringFieldBuilder JsonValidator::string(const std::string& name) {
    return StringFieldBuilder(this, name);
}

JsonValidator::NumberFieldBuilder JsonValidator::number(const std::string& name) {
    return NumberFieldBuilder(this, name);
}

JsonValidator::IntegerFieldBuilder JsonValidator::integer(const std::string& name) {
    return IntegerFieldBuilder(this, name);
}

JsonValidator::BooleanFieldBuilder JsonValidator::boolean(const std::string& name) {
    return BooleanFieldBuilder(this, name);
}

JsonValidator::ArrayFieldBuilder JsonValidator::array(const std::string& name) {
    return ArrayFieldBuilder(this, name);
}

JsonValidator::ObjectFieldBuilder JsonValidator::object(const std::string& name) {
    return ObjectFieldBuilder(this, name);
}

// ============================================================================
// SCHÉMAS PRÉDÉFINIS
// ============================================================================

JsonValidator JsonValidator::createCommandSchema() {
    JsonValidator validator;
    
    validator.string("command").required();
    validator.object("params");
    
    return validator;
}

JsonValidator JsonValidator::createMidiMessageSchema() {
    JsonValidator validator;
    
    validator.integer("type").required().min(0).max(255);
    validator.integer("channel").min(1).max(16);
    validator.integer("note").min(0).max(127);
    validator.integer("velocity").min(0).max(127);
    
    return validator;
}

JsonValidator JsonValidator::createPresetSchema() {
    JsonValidator validator;
    
    validator.string("name").required().minLength(1).maxLength(100);
    validator.string("category").maxLength(50);
    validator.object("data").required();
    
    return validator;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

bool JsonValidator::validateType(const FieldSchema& field, const json& value,
                                 ValidationResult& result) const {
    bool typeValid = false;
    
    switch (field.type) {
        case JsonType::STRING:
            typeValid = value.is_string();
            break;
        case JsonType::NUMBER:
            typeValid = value.is_number();
            break;
        case JsonType::INTEGER:
            typeValid = value.is_number_integer();
            break;
        case JsonType::BOOLEAN:
            typeValid = value.is_boolean();
            break;
        case JsonType::OBJECT:
            typeValid = value.is_object();
            break;
        case JsonType::ARRAY:
            typeValid = value.is_array();
            break;
        case JsonType::NULL_TYPE:
            typeValid = value.is_null();
            break;
        case JsonType::ANY:
            typeValid = true;
            break;
    }
    
    if (!typeValid) {
        result.addError("Field '" + field.name + "' has invalid type, expected: " +
                       jsonTypeToString(field.type));
        return false;
    }
    
    return true;
}

bool JsonValidator::validateConstraints(const FieldSchema& field, const json& value,
                                        ValidationResult& result) const {
    // Validateur personnalisé
    if (field.validator && !field.validator(value)) {
        result.addError("Field '" + field.name + "' failed custom validation");
        return false;
    }
    
    // Contraintes pour les strings
    if (field.type == JsonType::STRING) {
        std::string str = value.get<std::string>();
        
        if (field.hasMinLength && str.length() < field.minLength) {
            result.addError("Field '" + field.name + "' is too short (min: " +
                           std::to_string(field.minLength) + ")");
        }
        
        if (field.hasMaxLength && str.length() > field.maxLength) {
            result.addError("Field '" + field.name + "' is too long (max: " +
                           std::to_string(field.maxLength) + ")");
        }
        
        if (!field.enumValues.empty()) {
            bool found = std::find(field.enumValues.begin(), field.enumValues.end(), str) 
                        != field.enumValues.end();
            if (!found) {
                result.addError("Field '" + field.name + "' has invalid value");
            }
        }
    }
    
    // Contraintes pour les nombres
    if (field.type == JsonType::NUMBER || field.type == JsonType::INTEGER) {
        double num = value.get<double>();
        
        if (field.hasMin && num < field.minValue) {
            result.addError("Field '" + field.name + "' is too small (min: " +
                           std::to_string(field.minValue) + ")");
        }
        
        if (field.hasMax && num > field.maxValue) {
            result.addError("Field '" + field.name + "' is too large (max: " +
                           std::to_string(field.maxValue) + ")");
        }
    }
    
    // Contraintes pour les arrays
    if (field.type == JsonType::ARRAY) {
        size_t size = value.size();
        
        if (field.hasMinItems && size < field.minItems) {
            result.addError("Field '" + field.name + "' has too few items (min: " +
                           std::to_string(field.minItems) + ")");
        }
        
        if (field.hasMaxItems && size > field.maxItems) {
            result.addError("Field '" + field.name + "' has too many items (max: " +
                           std::to_string(field.maxItems) + ")");
        }
    }
    
    return result.valid;
}

std::string JsonValidator::jsonTypeToString(JsonType type) {
    switch (type) {
        case JsonType::STRING: return "string";
        case JsonType::NUMBER: return "number";
        case JsonType::INTEGER: return "integer";
        case JsonType::BOOLEAN: return "boolean";
        case JsonType::OBJECT: return "object";
        case JsonType::ARRAY: return "array";
        case JsonType::NULL_TYPE: return "null";
        case JsonType::ANY: return "any";
        default: return "unknown";
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER JsonValidator.cpp
// ============================================================================
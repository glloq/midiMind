// ============================================================================
// Fichier: src/core/utils/JsonValidator.cpp
// Version: 3.0.1
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Implémentation du validateur JSON avec règles de validation complètes.
//   Supporte tous les types JSON et permet une validation fluide via builders.
//
// Fonctionnalités:
//   - Validation de types (string, number, boolean, array, object)
//   - Validation de contraintes (min/max, length, pattern, enum)
//   - Champs requis/optionnels
//   - Messages d'erreur détaillés
//   - Schémas prédéfinis (commandes, MIDI, presets)
//
// Architecture:
//   - Builder pattern pour configuration fluide
//   - Validation récursive pour objets imbriqués
//   - Gestion d'erreurs avec exceptions ou retour booléen
//
// Auteur: MidiMind Team
// Date: 2025-10-09
// ============================================================================

#include "JsonValidator.h"
#include "../Error.h"
#include "../Logger.h"
#include <regex>

namespace midiMind {

// ============================================================================
// MÉTHODE: validate()
// Valide un objet JSON selon le schéma défini
// ============================================================================

bool JsonValidator::validate(const json& data, std::string& errorMessage) const {
    errorMessage.clear();
    
    // Vérifier que c'est un objet
    if (!data.is_object()) {
        errorMessage = "Root element must be an object";
        return false;
    }
    
    // Valider chaque champ défini dans le schéma
    for (const auto& field : fields_) {
        // Vérifier si le champ existe
        bool fieldExists = data.contains(field.name);
        
        // Champ requis manquant
        if (field.required && !fieldExists) {
            errorMessage = "Required field '" + field.name + "' is missing";
            return false;
        }
        
        // Champ optionnel manquant - passer
        if (!fieldExists) {
            continue;
        }
        
        const json& value = data[field.name];
        
        // Valider le type
        if (!validateType(value, field, errorMessage)) {
            errorMessage = "Field '" + field.name + "': " + errorMessage;
            return false;
        }
        
        // Valider les contraintes spécifiques au type
        if (!validateConstraints(value, field, errorMessage)) {
            errorMessage = "Field '" + field.name + "': " + errorMessage;
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// MÉTHODE: validateOrThrow()
// Valide et lève une exception si invalide
// ============================================================================

void JsonValidator::validateOrThrow(const json& data) const {
    std::string errorMessage;
    
    if (!validate(data, errorMessage)) {
        THROW_ERROR(ErrorCode::VALIDATION_FAILED, 
                   "JSON validation failed: " + errorMessage);
    }
}

// ============================================================================
// MÉTHODE PRIVÉE: validateType()
// Valide le type d'une valeur JSON
// ============================================================================

bool JsonValidator::validateType(
    const json& value,
    const FieldSchema& field,
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
    }
    
    return true;
}

// ============================================================================
// MÉTHODE PRIVÉE: validateConstraints()
// Valide les contraintes spécifiques d'un champ
// ============================================================================

bool JsonValidator::validateConstraints(
    const json& value,
    const FieldSchema& field,
    std::string& errorMessage) const {
    
    switch (field.type) {
        case JsonType::STRING:
            return validateStringConstraints(value, field, errorMessage);
            
        case JsonType::NUMBER:
        case JsonType::INTEGER:
            return validateNumberConstraints(value, field, errorMessage);
            
        case JsonType::ARRAY:
            return validateArrayConstraints(value, field, errorMessage);
            
        case JsonType::BOOLEAN:
        case JsonType::OBJECT:
            // Pas de contraintes supplémentaires pour boolean et object (pour l'instant)
            return true;
    }
    
    return true;
}

// ============================================================================
// MÉTHODE PRIVÉE: validateStringConstraints()
// Valide les contraintes d'une string
// ============================================================================

bool JsonValidator::validateStringConstraints(
    const json& value,
    const FieldSchema& field,
    std::string& errorMessage) const {
    
    std::string str = value.get<std::string>();
    
    // Longueur minimum
    if (field.hasMinLength && str.length() < field.minLength) {
        errorMessage = "String too short (min: " + std::to_string(field.minLength) + 
                      ", got: " + std::to_string(str.length()) + ")";
        return false;
    }
    
    // Longueur maximum
    if (field.hasMaxLength && str.length() > field.maxLength) {
        errorMessage = "String too long (max: " + std::to_string(field.maxLength) + 
                      ", got: " + std::to_string(str.length()) + ")";
        return false;
    }
    
    // Pattern regex
    if (field.hasPattern) {
        try {
            std::regex pattern(field.pattern);
            if (!std::regex_match(str, pattern)) {
                errorMessage = "String does not match pattern: " + field.pattern;
                return false;
            }
        } catch (const std::regex_error& e) {
            Logger::error("JsonValidator", "Invalid regex pattern: " + field.pattern);
            errorMessage = "Internal error: invalid pattern";
            return false;
        }
    }
    
    // Enum values
    if (!field.enumValues.empty()) {
        bool found = false;
        for (const auto& allowed : field.enumValues) {
            if (allowed == str) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            errorMessage = "Value not in allowed list";
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// MÉTHODE PRIVÉE: validateNumberConstraints()
// Valide les contraintes d'un nombre
// ============================================================================

bool JsonValidator::validateNumberConstraints(
    const json& value,
    const FieldSchema& field,
    std::string& errorMessage) const {
    
    double num = value.get<double>();
    
    // Minimum
    if (field.hasMin && num < field.min) {
        errorMessage = "Value too small (min: " + std::to_string(field.min) + 
                      ", got: " + std::to_string(num) + ")";
        return false;
    }
    
    // Maximum
    if (field.hasMax && num > field.max) {
        errorMessage = "Value too large (max: " + std::to_string(field.max) + 
                      ", got: " + std::to_string(num) + ")";
        return false;
    }
    
    return true;
}

// ============================================================================
// MÉTHODE PRIVÉE: validateArrayConstraints()
// Valide les contraintes d'un array
// ============================================================================

bool JsonValidator::validateArrayConstraints(
    const json& value,
    const FieldSchema& field,
    std::string& errorMessage) const {
    
    size_t size = value.size();
    
    // Minimum d'éléments
    if (field.hasMinItems && size < field.minItems) {
        errorMessage = "Array too small (min items: " + std::to_string(field.minItems) + 
                      ", got: " + std::to_string(size) + ")";
        return false;
    }
    
    // Maximum d'éléments
    if (field.hasMaxItems && size > field.maxItems) {
        errorMessage = "Array too large (max items: " + std::to_string(field.maxItems) + 
                      ", got: " + std::to_string(size) + ")";
        return false;
    }
    
    return true;
}

// ============================================================================
// MÉTHODES PUBLIQUES: Builders
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
// MÉTHODES: Schémas prédéfinis
// ============================================================================

JsonValidator JsonValidator::createCommandSchema() {
    JsonValidator validator;
    
    validator.string("command")
        .required()
        .minLength(3)
        .maxLength(100)
        .pattern("^[a-z][a-z0-9_]*(\\.[a-z][a-z0-9_]*)*$");
    
    validator.object("params");
    
    return validator;
}

JsonValidator JsonValidator::createMidiMessageSchema() {
    JsonValidator validator;
    
    validator.string("type")
        .required()
        .enumValues({
            "noteOn", "noteOff", "controlChange", 
            "programChange", "pitchBend", "aftertouch"
        });
    
    validator.integer("channel")
        .required()
        .min(0)
        .max(15);
    
    validator.integer("data1")
        .min(0)
        .max(127);
    
    validator.integer("data2")
        .min(0)
        .max(127);
    
    validator.integer("timestamp");
    
    return validator;
}

JsonValidator JsonValidator::createPresetSchema() {
    JsonValidator validator;
    
    validator.string("name")
        .required()
        .minLength(1)
        .maxLength(100);
    
    validator.string("description")
        .maxLength(500);
    
    validator.string("category")
        .enumValues({"routing", "processing", "playback", "system"});
    
    validator.object("config")
        .required();
    
    validator.array("tags")
        .maxItems(10);
    
    validator.integer("version")
        .min(1)
        .defaultValue(1);
    
    return validator;
}

// ============================================================================
// MÉTHODE PRIVÉE: addField()
// Ajoute un champ au schéma (utilisé par les builders)
// ============================================================================

void JsonValidator::addField(const FieldSchema& field) {
    // Vérifier si le champ existe déjà
    for (auto& existingField : fields_) {
        if (existingField.name == field.name) {
            Logger::warn("JsonValidator", 
                "Field '" + field.name + "' already exists, replacing");
            existingField = field;
            return;
        }
    }
    
    fields_.push_back(field);
}

// ============================================================================
// IMPLEMENTATIONS DES BUILDERS
// ============================================================================

// StringFieldBuilder

JsonValidator::StringFieldBuilder& 
JsonValidator::StringFieldBuilder::required() {
    field_.required = true;
    return *this;
}

JsonValidator::StringFieldBuilder& 
JsonValidator::StringFieldBuilder::minLength(size_t len) {
    field_.hasMinLength = true;
    field_.minLength = len;
    return *this;
}

JsonValidator::StringFieldBuilder& 
JsonValidator::StringFieldBuilder::maxLength(size_t len) {
    field_.hasMaxLength = true;
    field_.maxLength = len;
    return *this;
}

JsonValidator::StringFieldBuilder& 
JsonValidator::StringFieldBuilder::pattern(const std::string& regex) {
    field_.hasPattern = true;
    field_.pattern = regex;
    return *this;
}

JsonValidator::StringFieldBuilder& 
JsonValidator::StringFieldBuilder::enumValues(const std::vector<std::string>& values) {
    field_.enumValues = values;
    return *this;
}

JsonValidator::StringFieldBuilder& 
JsonValidator::StringFieldBuilder::defaultValue(const std::string& value) {
    field_.defaultValue = value;
    return *this;
}

// NumberFieldBuilder

JsonValidator::NumberFieldBuilder& 
JsonValidator::NumberFieldBuilder::required() {
    field_.required = true;
    return *this;
}

JsonValidator::NumberFieldBuilder& 
JsonValidator::NumberFieldBuilder::min(double value) {
    field_.hasMin = true;
    field_.min = value;
    return *this;
}

JsonValidator::NumberFieldBuilder& 
JsonValidator::NumberFieldBuilder::max(double value) {
    field_.hasMax = true;
    field_.max = value;
    return *this;
}

JsonValidator::NumberFieldBuilder& 
JsonValidator::NumberFieldBuilder::defaultValue(double value) {
    field_.defaultValue = value;
    return *this;
}

// IntegerFieldBuilder

JsonValidator::IntegerFieldBuilder& 
JsonValidator::IntegerFieldBuilder::required() {
    field_.required = true;
    return *this;
}

JsonValidator::IntegerFieldBuilder& 
JsonValidator::IntegerFieldBuilder::min(int value) {
    field_.hasMin = true;
    field_.min = value;
    return *this;
}

JsonValidator::IntegerFieldBuilder& 
JsonValidator::IntegerFieldBuilder::max(int value) {
    field_.hasMax = true;
    field_.max = value;
    return *this;
}

JsonValidator::IntegerFieldBuilder& 
JsonValidator::IntegerFieldBuilder::defaultValue(int value) {
    field_.defaultValue = value;
    return *this;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER JsonValidator.cpp
// ============================================================================

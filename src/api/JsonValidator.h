// ============================================================================
// Fichier: src/api/JsonValidator.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Validateur de schémas JSON.
//   Vérifie que les données JSON respectent un schéma défini.
//
// Responsabilités:
//   - Validation de schémas
//   - Vérification des types
//   - Validation des valeurs requises
//   - Messages d'erreur détaillés
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

#include "../core/Logger.h"
#include "../core/Error.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @enum JsonType
 * @brief Types JSON supportés
 */
enum class JsonType {
    STRING,
    NUMBER,
    INTEGER,
    BOOLEAN,
    OBJECT,
    ARRAY,
    NULL_TYPE,
    ANY
};

/**
 * @struct FieldSchema
 * @brief Schéma pour un champ JSON
 */
struct FieldSchema {
    std::string name;               ///< Nom du champ
    JsonType type;                  ///< Type attendu
    bool required;                  ///< Champ obligatoire ?
    json defaultValue;              ///< Valeur par défaut
    
    // Validation des valeurs
    std::function<bool(const json&)> validator;  ///< Validateur personnalisé
    
    // Pour les nombres
    bool hasMin;
    bool hasMax;
    double minValue;
    double maxValue;
    
    // Pour les strings
    bool hasMinLength;
    bool hasMaxLength;
    size_t minLength;
    size_t maxLength;
    std::vector<std::string> enumValues;  ///< Valeurs autorisées
    
    // Pour les arrays
    bool hasMinItems;
    bool hasMaxItems;
    size_t minItems;
    size_t maxItems;
    
    FieldSchema()
        : type(JsonType::ANY)
        , required(false)
        , hasMin(false)
        , hasMax(false)
        , minValue(0)
        , maxValue(0)
        , hasMinLength(false)
        , hasMaxLength(false)
        , minLength(0)
        , maxLength(0)
        , hasMinItems(false)
        , hasMaxItems(false)
        , minItems(0)
        , maxItems(0) {}
};

/**
 * @struct ValidationResult
 * @brief Résultat de validation
 */
struct ValidationResult {
    bool valid;                     ///< Validation réussie ?
    std::vector<std::string> errors; ///< Erreurs de validation
    
    ValidationResult() : valid(true) {}
    
    void addError(const std::string& error) {
        valid = false;
        errors.push_back(error);
    }
    
    std::string getMessage() const {
        if (valid) return "Valid";
        
        std::string msg = "Validation failed:\n";
        for (const auto& error : errors) {
            msg += "  - " + error + "\n";
        }
        return msg;
    }
};

/**
 * @class JsonValidator
 * @brief Validateur de schémas JSON
 * 
 * @details
 * Valide des objets JSON contre des schémas définis.
 * 
 * Fonctionnalités:
 * - Vérification des types
 * - Champs requis
 * - Validation des valeurs (min/max, length, enum)
 * - Validateurs personnalisés
 * 
 * Thread-safety: Toutes les méthodes sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * JsonValidator validator;
 * 
 * // Définir un schéma
 * FieldSchema nameField;
 * nameField.name = "name";
 * nameField.type = JsonType::STRING;
 * nameField.required = true;
 * nameField.hasMinLength = true;
 * nameField.minLength = 3;
 * 
 * FieldSchema ageField;
 * ageField.name = "age";
 * ageField.type = JsonType::INTEGER;
 * ageField.hasMin = true;
 * ageField.minValue = 0;
 * 
 * validator.addField(nameField);
 * validator.addField(ageField);
 * 
 * // Valider
 * json data = {{"name", "John"}, {"age", 30}};
 * auto result = validator.validate(data);
 * 
 * if (!result.valid) {
 *     Logger::error("Validation", result.getMessage());
 * }
 * ```
 */
class JsonValidator {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    JsonValidator();
    
    /**
     * @brief Destructeur
     */
    ~JsonValidator();
    
    // ========================================================================
    // DÉFINITION DU SCHÉMA
    // ========================================================================
    
    /**
     * @brief Ajoute un champ au schéma
     * 
     * @param field Définition du champ
     * 
     * @note Thread-safe
     */
    void addField(const FieldSchema& field);
    
    /**
     * @brief Retire un champ du schéma
     * 
     * @param name Nom du champ
     * 
     * @note Thread-safe
     */
    void removeField(const std::string& name);
    
    /**
     * @brief Efface tous les champs
     * 
     * @note Thread-safe
     */
    void clear();
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Valide un objet JSON
     * 
     * @param data Objet JSON à valider
     * @return ValidationResult Résultat de validation
     * 
     * @note Thread-safe
     */
    ValidationResult validate(const json& data) const;
    
    /**
     * @brief Valide et lève une exception si invalide
     * 
     * @param data Objet JSON à valider
     * 
     * @throws MidiMindException Si validation échoue
     * 
     * @note Thread-safe
     */
    void validateOrThrow(const json& data) const;
    
    // ========================================================================
    // BUILDERS FLUENT API
    // ========================================================================
    
    /**
     * @brief Builder pour ajouter un champ string
     * 
     * @example
     * ```cpp
     * validator.string("name").required().minLength(3).maxLength(50);
     * ```
     */
    class StringFieldBuilder;
    StringFieldBuilder string(const std::string& name);
    
    /**
     * @brief Builder pour ajouter un champ number
     */
    class NumberFieldBuilder;
    NumberFieldBuilder number(const std::string& name);
    
    /**
     * @brief Builder pour ajouter un champ integer
     */
    class IntegerFieldBuilder;
    IntegerFieldBuilder integer(const std::string& name);
    
    /**
     * @brief Builder pour ajouter un champ boolean
     */
    class BooleanFieldBuilder;
    BooleanFieldBuilder boolean(const std::string& name);
    
    /**
     * @brief Builder pour ajouter un champ array
     */
    class ArrayFieldBuilder;
    ArrayFieldBuilder array(const std::string& name);
    
    /**
     * @brief Builder pour ajouter un champ object
     */
    class ObjectFieldBuilder;
    ObjectFieldBuilder object(const std::string& name);
    
    // ========================================================================
    // SCHÉMAS PRÉDÉFINIS
    // ========================================================================
    
    /**
     * @brief Crée un schéma pour les commandes API
     */
    static JsonValidator createCommandSchema();
    
    /**
     * @brief Crée un schéma pour les messages MIDI
     */
    static JsonValidator createMidiMessageSchema();
    
    /**
     * @brief Crée un schéma pour les presets
     */
    static JsonValidator createPresetSchema();

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Valide le type d'un champ
     */
    bool validateType(const FieldSchema& field, const json& value, 
                     ValidationResult& result) const;
    
    /**
     * @brief Valide les contraintes d'un champ
     */
    bool validateConstraints(const FieldSchema& field, const json& value,
                            ValidationResult& result) const;
    
    /**
     * @brief Convertit JsonType en string
     */
    static std::string jsonTypeToString(JsonType type);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Schéma des champs
    std::vector<FieldSchema> fields_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
};

// ============================================================================
// BUILDERS (FLUENT API)
// ============================================================================

/**
 * @class StringFieldBuilder
 * @brief Builder pour champs string
 */
class JsonValidator::StringFieldBuilder {
public:
    StringFieldBuilder(JsonValidator* validator, const std::string& name)
        : validator_(validator), field_() {
        field_.name = name;
        field_.type = JsonType::STRING;
    }
    
    StringFieldBuilder& required() {
        field_.required = true;
        return *this;
    }
    
    StringFieldBuilder& defaultValue(const std::string& value) {
        field_.defaultValue = value;
        return *this;
    }
    
    StringFieldBuilder& minLength(size_t length) {
        field_.hasMinLength = true;
        field_.minLength = length;
        return *this;
    }
    
    StringFieldBuilder& maxLength(size_t length) {
        field_.hasMaxLength = true;
        field_.maxLength = length;
        return *this;
    }
    
    StringFieldBuilder& enumValues(const std::vector<std::string>& values) {
        field_.enumValues = values;
        return *this;
    }
    
    ~StringFieldBuilder() {
        validator_->addField(field_);
    }

private:
    JsonValidator* validator_;
    FieldSchema field_;
};

/**
 * @class NumberFieldBuilder
 * @brief Builder pour champs number
 */
class JsonValidator::NumberFieldBuilder {
public:
    NumberFieldBuilder(JsonValidator* validator, const std::string& name)
        : validator_(validator), field_() {
        field_.name = name;
        field_.type = JsonType::NUMBER;
    }
    
    NumberFieldBuilder& required() {
        field_.required = true;
        return *this;
    }
    
    NumberFieldBuilder& defaultValue(double value) {
        field_.defaultValue = value;
        return *this;
    }
    
    NumberFieldBuilder& min(double value) {
        field_.hasMin = true;
        field_.minValue = value;
        return *this;
    }
    
    NumberFieldBuilder& max(double value) {
        field_.hasMax = true;
        field_.maxValue = value;
        return *this;
    }
    
    ~NumberFieldBuilder() {
        validator_->addField(field_);
    }

private:
    JsonValidator* validator_;
    FieldSchema field_;
};

/**
 * @class IntegerFieldBuilder
 * @brief Builder pour champs integer
 */
class JsonValidator::IntegerFieldBuilder {
public:
    IntegerFieldBuilder(JsonValidator* validator, const std::string& name)
        : validator_(validator), field_() {
        field_.name = name;
        field_.type = JsonType::INTEGER;
    }
    
    IntegerFieldBuilder& required() {
        field_.required = true;
        return *this;
    }
    
    IntegerFieldBuilder& defaultValue(int value) {
        field_.defaultValue = value;
        return *this;
    }
    
    IntegerFieldBuilder& min(int value) {
        field_.hasMin = true;
        field_.minValue = value;
        return *this;
    }
    
    IntegerFieldBuilder& max(int value) {
        field_.hasMax = true;
        field_.maxValue = value;
        return *this;
    }
    
    ~IntegerFieldBuilder() {
        validator_->addField(field_);
    }

private:
    JsonValidator* validator_;
    FieldSchema field_;
};

/**
 * @class BooleanFieldBuilder
 * @brief Builder pour champs boolean
 */
class JsonValidator::BooleanFieldBuilder {
public:
    BooleanFieldBuilder(JsonValidator* validator, const std::string& name)
        : validator_(validator), field_() {
        field_.name = name;
        field_.type = JsonType::BOOLEAN;
    }
    
    BooleanFieldBuilder& required() {
        field_.required = true;
        return *this;
    }
    
    BooleanFieldBuilder& defaultValue(bool value) {
        field_.defaultValue = value;
        return *this;
    }
    
    ~BooleanFieldBuilder() {
        validator_->addField(field_);
    }

private:
    JsonValidator* validator_;
    FieldSchema field_;
};

/**
 * @class ArrayFieldBuilder
 * @brief Builder pour champs array
 */
class JsonValidator::ArrayFieldBuilder {
public:
    ArrayFieldBuilder(JsonValidator* validator, const std::string& name)
        : validator_(validator), field_() {
        field_.name = name;
        field_.type = JsonType::ARRAY;
    }
    
    ArrayFieldBuilder& required() {
        field_.required = true;
        return *this;
    }
    
    ArrayFieldBuilder& minItems(size_t count) {
        field_.hasMinItems = true;
        field_.minItems = count;
        return *this;
    }
    
    ArrayFieldBuilder& maxItems(size_t count) {
        field_.hasMaxItems = true;
        field_.maxItems = count;
        return *this;
    }
    
    ~ArrayFieldBuilder() {
        validator_->addField(field_);
    }

private:
    JsonValidator* validator_;
    FieldSchema field_;
};

/**
 * @class ObjectFieldBuilder
 * @brief Builder pour champs object
 */
class JsonValidator::ObjectFieldBuilder {
public:
    ObjectFieldBuilder(JsonValidator* validator, const std::string& name)
        : validator_(validator), field_() {
        field_.name = name;
        field_.type = JsonType::OBJECT;
    }
    
    ObjectFieldBuilder& required() {
        field_.required = true;
        return *this;
    }
    
    ~ObjectFieldBuilder() {
        validator_->addField(field_);
    }

private:
    JsonValidator* validator_;
    FieldSchema field_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER JsonValidator.h
// ============================================================================
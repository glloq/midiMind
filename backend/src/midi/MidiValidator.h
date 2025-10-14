// ============================================================================
// Fichier: backend/src/midi/MidiValidator.h
// Version: 3.1.0
// Date: 2025-10-10
// Projet: MidiMind v3.1 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Validateur pour données MIDI et JsonMidi.
//   Vérifie l'intégrité, la validité des valeurs MIDI,
//   et détecte les problèmes courants.
//
// Fonctionnalités:
//   - Validation header JsonMidi (format, tracks, ppq)
//   - Validation events (types, ranges MIDI 0-127)
//   - Détection overlaps de notes
//   - Vérification tri temporel
//   - Validation références (track, channel)
//   - Rapport d'erreurs et warnings détaillé
//
// Utilisation:
//   MidiValidator validator;
//   ValidationResult result = validator.validate(jsonMidi);
//   if (!result.isValid) {
//     for (const auto& error : result.errors) {
//       Logger::error("Validator", error);
//     }
//   }
//
// Auteur: MidiMind Team
// Statut: ✅ PHASE 1 - COMPLET
// ============================================================================

#ifndef MIDIMIND_MIDI_VALIDATOR_H
#define MIDIMIND_MIDI_VALIDATOR_H

#include <string>
#include <vector>
#include <set>
#include "../core/json.hpp"

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// ENUMS: Niveaux de sévérité
// ============================================================================

enum class ValidationSeverity {
    ERROR,      // Erreur bloquante (données invalides)
    WARNING,    // Avertissement (données valides mais suspectes)
    INFO        // Information (suggestion d'amélioration)
};

// ============================================================================
// STRUCTURE: ValidationIssue
// Représente un problème de validation
// ============================================================================
struct ValidationIssue {
    ValidationSeverity severity;
    std::string category;           // "header", "event", "note", "timeline"
    std::string message;            // Description du problème
    std::string location;           // Localisation (ex: "track 0, event 15")
    json context;                   // Données contextuelles (optionnel)
    
    ValidationIssue(ValidationSeverity sev, 
                   const std::string& cat,
                   const std::string& msg,
                   const std::string& loc = "",
                   const json& ctx = json::object())
        : severity(sev)
        , category(cat)
        , message(msg)
        , location(loc)
        , context(ctx)
    {}
    
    /**
     * @brief Convertit en JSON pour rapport
     */
    json toJson() const {
        return {
            {"severity", severityToString()},
            {"category", category},
            {"message", message},
            {"location", location},
            {"context", context}
        };
    }
    
    /**
     * @brief Convertit la sévérité en string
     */
    std::string severityToString() const {
        switch (severity) {
            case ValidationSeverity::ERROR:   return "ERROR";
            case ValidationSeverity::WARNING: return "WARNING";
            case ValidationSeverity::INFO:    return "INFO";
            default: return "UNKNOWN";
        }
    }
};

// ============================================================================
// STRUCTURE: ValidationResult
// Résultat de validation
// ============================================================================
struct ValidationResult {
    bool isValid;                       // true si aucune erreur
    std::vector<ValidationIssue> errors;    // Erreurs bloquantes
    std::vector<ValidationIssue> warnings;  // Avertissements
    std::vector<ValidationIssue> infos;     // Informations
    
    ValidationResult() : isValid(true) {}
    
    /**
     * @brief Ajoute une issue
     */
    void addIssue(const ValidationIssue& issue) {
        switch (issue.severity) {
            case ValidationSeverity::ERROR:
                errors.push_back(issue);
                isValid = false;
                break;
            case ValidationSeverity::WARNING:
                warnings.push_back(issue);
                break;
            case ValidationSeverity::INFO:
                infos.push_back(issue);
                break;
        }
    }
    
    /**
     * @brief Compte total des issues
     */
    size_t getTotalIssues() const {
        return errors.size() + warnings.size() + infos.size();
    }
    
    /**
     * @brief Convertit en JSON pour rapport
     */
    json toJson() const {
        json result = {
            {"valid", isValid},
            {"errorCount", errors.size()},
            {"warningCount", warnings.size()},
            {"infoCount", infos.size()}
        };
        
        if (!errors.empty()) {
            result["errors"] = json::array();
            for (const auto& error : errors) {
                result["errors"].push_back(error.toJson());
            }
        }
        
        if (!warnings.empty()) {
            result["warnings"] = json::array();
            for (const auto& warning : warnings) {
                result["warnings"].push_back(warning.toJson());
            }
        }
        
        if (!infos.empty()) {
            result["infos"] = json::array();
            for (const auto& info : infos) {
                result["infos"].push_back(info.toJson());
            }
        }
        
        return result;
    }
};

// ============================================================================
// CLASSE: MidiValidator
// Validateur de données MIDI
// ============================================================================
class MidiValidator {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    MidiValidator();
    ~MidiValidator() = default;
    
    // ========================================================================
    // VALIDATION PRINCIPALE
    // ========================================================================
    
    /**
     * @brief Valide un JsonMidi complet
     * 
     * @param jsonMidi Données JsonMidi à valider
     * @return ValidationResult Résultat avec erreurs/warnings
     * 
     * @note Valide header, tracks, events, timeline
     */
    ValidationResult validate(const json& jsonMidi) const;
    
    /**
     * @brief Valide uniquement le header
     * 
     * @param header Header JsonMidi
     * @return ValidationResult Résultat
     */
    ValidationResult validateHeader(const json& header) const;
    
    /**
     * @brief Valide une piste
     * 
     * @param track Piste JsonMidi
     * @param trackIndex Index de la piste (pour messages)
     * @return ValidationResult Résultat
     */
    ValidationResult validateTrack(const json& track, int trackIndex) const;
    
    /**
     * @brief Valide un event MIDI
     * 
     * @param event Event JsonMidi
     * @param location Localisation pour messages
     * @return ValidationResult Résultat
     */
    ValidationResult validateEvent(const json& event, const std::string& location) const;
    
    // ========================================================================
    // VALIDATION SPÉCIFIQUE
    // ========================================================================
    
    /**
     * @brief Valide une note MIDI
     * 
     * @param note Event de type note
     * @param location Localisation
     * @return ValidationResult Résultat
     */
    ValidationResult validateNote(const json& note, const std::string& location) const;
    
    /**
     * @brief Valide un Control Change
     * 
     * @param cc Event de type cc
     * @param location Localisation
     * @return ValidationResult Résultat
     */
    ValidationResult validateCC(const json& cc, const std::string& location) const;
    
    /**
     * @brief Détecte les overlaps de notes
     * 
     * @param track Piste à analyser
     * @param trackIndex Index de la piste
     * @return ValidationResult Résultat avec warnings si overlaps
     */
    ValidationResult detectNoteOverlaps(const json& track, int trackIndex) const;
    
    /**
     * @brief Vérifie le tri temporel de la timeline
     * 
     * @param track Piste à analyser
     * @param trackIndex Index de la piste
     * @return ValidationResult Résultat avec erreurs si non trié
     */
    ValidationResult validateTimelineSorting(const json& track, int trackIndex) const;
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Vérifie si une valeur MIDI est valide (0-127)
     */
    static bool isValidMidiValue(int value);
    
    /**
     * @brief Vérifie si un channel MIDI est valide (0-15)
     */
    static bool isValidMidiChannel(int channel);
    
    /**
     * @brief Vérifie si un type d'event est connu
     */
    static bool isValidEventType(const std::string& type);
    
    /**
     * @brief Liste des types d'events valides
     */
    static const std::set<std::string>& getValidEventTypes();
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Valide la structure de base d'un objet JSON
     */
    bool hasRequiredFields(const json& obj, 
                          const std::vector<std::string>& fields,
                          ValidationResult& result,
                          const std::string& location) const;
    
    /**
     * @brief Ajoute une erreur au résultat
     */
    void addError(ValidationResult& result,
                 const std::string& category,
                 const std::string& message,
                 const std::string& location = "",
                 const json& context = json::object()) const;
    
    /**
     * @brief Ajoute un warning au résultat
     */
    void addWarning(ValidationResult& result,
                   const std::string& category,
                   const std::string& message,
                   const std::string& location = "",
                   const json& context = json::object()) const;
    
    /**
     * @brief Ajoute une info au résultat
     */
    void addInfo(ValidationResult& result,
                const std::string& category,
                const std::string& message,
                const std::string& location = "",
                const json& context = json::object()) const;
    
    // ========================================================================
    // TYPES D'EVENTS VALIDES
    // ========================================================================
    
    static const std::set<std::string> VALID_EVENT_TYPES;
};

} // namespace midiMind

#endif // MIDIMIND_MIDI_VALIDATOR_H

// ============================================================================
// FIN DU FICHIER MidiValidator.h
// ============================================================================

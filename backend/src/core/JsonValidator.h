// ============================================================================
// Fichier: backend/src/core/JsonValidator.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0 - 2025-10-09
// ============================================================================
// Description:
//   Validateur JSON pour sécuriser les entrées API
//   Vérifie types, plages, formats et sécurité
//
// Fonctionnalités:
//   - Validation types de base (string, int, bool, array, object)
//   - Validation plages numériques
//   - Validation formats (email, URL, path, device ID)
//   - Validation sécurité (injection SQL, XSS, path traversal)
//   - Validation MIDI (canal, note, velocity, CC)
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <regex>
#include <nlohmann/json.hpp>
#include "Logger.h"
#include "Error.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @class JsonValidator
 * @brief Validateur JSON complet et sécurisé
 * 
 * Fournit des méthodes statiques pour valider les données JSON
 * avec vérification de types, plages, formats et sécurité.
 */
class JsonValidator {
public:

    // ========================================================================
    // VALIDATION DE BASE
    // ========================================================================

    /**
     * @brief Vérifie si un champ existe
     */
    static bool hasField(const json& obj, const std::string& field) {
        return obj.contains(field) && !obj[field].is_null();
    }

    /**
     * @brief Vérifie si un champ est requis
     */
    static bool requireField(const json& obj, 
                            const std::string& field, 
                            std::string& error) {
        if (!hasField(obj, field)) {
            error = "Missing required field: " + field;
            return false;
        }
        return true;
    }

    /**
     * @brief Valide un champ string
     */
    static bool validateString(const json& obj,
                              const std::string& field,
                              std::string& result,
                              size_t maxLength = 0,
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
     * @brief Valide un champ int
     */
    static bool validateInt(const json& obj,
                           const std::string& field,
                           int& result,
                           std::string& error) {
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
     * @brief Valide un champ int avec plage
     */
    static bool validateRange(const json& obj,
                             const std::string& field,
                             int& result,
                             int minValue,
                             int maxValue,
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
     * @brief Valide un champ bool
     */
    static bool validateBool(const json& obj,
                            const std::string& field,
                            bool& result,
                            std::string& error) {
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
     * @brief Valide un champ array
     */
    static bool validateArray(const json& obj,
                             const std::string& field,
                             json& result,
                             size_t minSize = 0,
                             size_t maxSize = 0,
                             std::string& error) {
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
     * @brief Valide un champ object
     */
    static bool validateObject(const json& obj,
                               const std::string& field,
                               json& result,
                               std::string& error) {
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
    // VALIDATION FORMATS
    // ========================================================================

    /**
     * @brief Valide un email
     */
    static bool validateEmail(const std::string& email, std::string& error) {
        static const std::regex emailRegex(
            R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)"
        );
        
        if (!std::regex_match(email, emailRegex)) {
            error = "Invalid email format";
            return false;
        }
        
        return true;
    }

    /**
     * @brief Valide une URL
     */
    static bool validateUrl(const std::string& url, std::string& error) {
        static const std::regex urlRegex(
            R"(^https?://[a-zA-Z0-9\-\.]+\.[a-zA-Z]{2,}(/.*)?$)"
        );
        
        if (!std::regex_match(url, urlRegex)) {
            error = "Invalid URL format";
            return false;
        }
        
        return true;
    }

    /**
     * @brief Valide une IPv4
     */
    static bool validateIPv4(const std::string& ip, std::string& error) {
        static const std::regex ipRegex(
            R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)"
        );
        
        std::smatch match;
        if (!std::regex_match(ip, match, ipRegex)) {
            error = "Invalid IPv4 format";
            return false;
        }
        
        // Vérifier chaque octet
        for (int i = 1; i <= 4; i++) {
            int octet = std::stoi(match[i]);
            if (octet < 0 || octet > 255) {
                error = "Invalid IPv4 octet: " + std::to_string(octet);
                return false;
            }
        }
        
        return true;
    }

    /**
     * @brief Valide un UUID
     */
    static bool validateUuid(const std::string& uuid, std::string& error) {
        static const std::regex uuidRegex(
            R"(^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$)",
            std::regex::icase
        );
        
        if (!std::regex_match(uuid, uuidRegex)) {
            error = "Invalid UUID format";
            return false;
        }
        
        return true;
    }

    // ========================================================================
    // VALIDATION SÉCURITÉ
    // ========================================================================

    /**
     * @brief Vérifie les tentatives d'injection SQL
     */
    static bool checkSqlInjection(const std::string& input, std::string& error) {
        // Patterns suspects
        static const std::vector<std::regex> sqlPatterns = {
            std::regex(R"(\b(SELECT|INSERT|UPDATE|DELETE|DROP|CREATE|ALTER|EXEC|UNION)\b)", 
                      std::regex::icase),
            std::regex(R"(--|\#|\/\*|\*\/)"),
            std::regex(R"(\bOR\b.*=.*\bOR\b)", std::regex::icase),
            std::regex(R"(;.*\bDROP\b)", std::regex::icase)
        };
        
        for (const auto& pattern : sqlPatterns) {
            if (std::regex_search(input, pattern)) {
                error = "Potential SQL injection detected";
                Logger::warn("JsonValidator", "SQL injection attempt: " + input);
                return false;
            }
        }
        
        return true;
    }

    /**
     * @brief Vérifie les tentatives XSS
     */
    static bool checkXss(const std::string& input, std::string& error) {
        static const std::vector<std::regex> xssPatterns = {
            std::regex(R"(<script)", std::regex::icase),
            std::regex(R"(javascript:)", std::regex::icase),
            std::regex(R"(on\w+\s*=)", std::regex::icase),
            std::regex(R"(<iframe)", std::regex::icase)
        };
        
        for (const auto& pattern : xssPatterns) {
            if (std::regex_search(input, pattern)) {
                error = "Potential XSS attack detected";
                Logger::warn("JsonValidator", "XSS attempt: " + input);
                return false;
            }
        }
        
        return true;
    }

    /**
     * @brief Vérifie les path traversal
     */
    static bool checkPathTraversal(const std::string& path, std::string& error) {
        // Interdire ".." dans les chemins
        if (path.find("..") != std::string::npos) {
            error = "Path traversal attempt detected";
            Logger::warn("JsonValidator", "Path traversal: " + path);
            return false;
        }
        
        // Interdire chemins absolus non autorisés
        if (path[0] == '/' && path.find("/etc") == 0) {
            error = "Access to /etc forbidden";
            return false;
        }
        
        return true;
    }

    /**
     * @brief Valide un chemin de fichier sécurisé
     */
    static bool validateFilePath(const std::string& path,
                                 const std::string& baseDir,
                                 std::string& error) {
        // Vérifier path traversal
        if (!checkPathTraversal(path, error)) {
            return false;
        }
        
        // Vérifier que le chemin est dans le répertoire autorisé
        if (!path.empty() && path[0] == '/') {
            if (path.find(baseDir) != 0) {
                error = "Path outside allowed directory";
                return false;
            }
        }
        
        // Vérifier caractères dangereux
        static const std::regex dangerousChars(R"([;&|`$\(\)<>])");
        if (std::regex_search(path, dangerousChars)) {
            error = "Dangerous characters in path";
            return false;
        }
        
        return true;
    }

    // ========================================================================
    // VALIDATION MIDI
    // ========================================================================

    /**
     * @brief Valide un canal MIDI (0-15)
     */
    static bool validateMidiChannel(int channel, std::string& error) {
        if (channel < 0 || channel > 15) {
            error = "Invalid MIDI channel (must be 0-15): " + std::to_string(channel);
            return false;
        }
        return true;
    }

    /**
     * @brief Valide une note MIDI (0-127)
     */
    static bool validateMidiNote(int note, std::string& error) {
        if (note < 0 || note > 127) {
            error = "Invalid MIDI note (must be 0-127): " + std::to_string(note);
            return false;
        }
        return true;
    }

    /**
     * @brief Valide une vélocité MIDI (0-127)
     */
    static bool validateMidiVelocity(int velocity, std::string& error) {
        if (velocity < 0 || velocity > 127) {
            error = "Invalid MIDI velocity (must be 0-127): " + std::to_string(velocity);
            return false;
        }
        return true;
    }

    /**
     * @brief Valide un contrôleur MIDI (0-127)
     */
    static bool validateMidiController(int controller, std::string& error) {
        if (controller < 0 || controller > 127) {
            error = "Invalid MIDI controller (must be 0-127): " + std::to_string(controller);
            return false;
        }
        return true;
    }

    /**
     * @brief Valide un program MIDI (0-127)
     */
    static bool validateMidiProgram(int program, std::string& error) {
        if (program < 0 || program > 127) {
            error = "Invalid MIDI program (must be 0-127): " + std::to_string(program);
            return false;
        }
        return true;
    }

    /**
     * @brief Valide un pitch bend MIDI (0-16383)
     */
    static bool validateMidiPitchBend(int value, std::string& error) {
        if (value < 0 || value > 16383) {
            error = "Invalid MIDI pitch bend (must be 0-16383): " + std::to_string(value);
            return false;
        }
        return true;
    }

    /**
     * @brief Valide un device ID MIDI
     */
    static bool validateDeviceId(const std::string& deviceId, std::string& error) {
        // Format attendu: "type_identifier"
        // Exemples: "usb_0", "wifi_192.168.1.100_5004", "bt_00:11:22:33:44:55"
        
        // Vérifier longueur
        if (deviceId.empty() || deviceId.length() > 100) {
            error = "Invalid device ID length";
            return false;
        }
        
        // Vérifier injection
        if (!checkSqlInjection(deviceId, error)) {
            return false;
        }
        
        if (!checkPathTraversal(deviceId, error)) {
            return false;
        }
        
        // Vérifier format
        static const std::regex deviceIdRegex(
            R"(^(usb|wifi|bt|virtual)_[a-zA-Z0-9\.\:_\-]+$)"
        );
        
        if (!std::regex_match(deviceId, deviceIdRegex)) {
            error = "Invalid device ID format";
            return false;
        }
        
        return true;
    }

    // ========================================================================
    // VALIDATION COMPOSITE
    // ========================================================================

    /**
     * @brief Valide un objet MIDI note event
     */
    static bool validateMidiNoteEvent(const json& event, std::string& error) {
        int channel, note, velocity;
        
        if (!validateRange(event, "channel", channel, 0, 15, error)) {
            return false;
        }
        
        if (!validateRange(event, "note", note, 0, 127, error)) {
            return false;
        }
        
        if (!validateRange(event, "velocity", velocity, 0, 127, error)) {
            return false;
        }
        
        return true;
    }

    /**
     * @brief Valide un objet MIDI CC event
     */
    static bool validateMidiCcEvent(const json& event, std::string& error) {
        int channel, controller, value;
        
        if (!validateRange(event, "channel", channel, 0, 15, error)) {
            return false;
        }
        
        if (!validateRange(event, "controller", controller, 0, 127, error)) {
            return false;
        }
        
        if (!validateRange(event, "value", value, 0, 127, error)) {
            return false;
        }
        
        return true;
    }

    /**
     * @brief Valide une configuration de routage
     */
    static bool validateRoutingConfig(const json& config, std::string& error) {
        // Valider source
        std::string source;
        if (!validateString(config, "source", source, 100, error)) {
            return false;
        }
        
        if (!validateDeviceId(source, error)) {
            return false;
        }
        
        // Valider destination
        std::string destination;
        if (!validateString(config, "destination", destination, 100, error)) {
            return false;
        }
        
        if (!validateDeviceId(destination, error)) {
            return false;
        }
        
        // Valider channel (optionnel)
        if (hasField(config, "channel")) {
            int channel;
            if (!validateRange(config, "channel", channel, 0, 15, error)) {
                return false;
            }
        }
        
        return true;
    }

    // ========================================================================
    // HELPERS
    // ========================================================================

    /**
     * @brief Sanitize une string pour l'affichage
     */
    static std::string sanitizeForDisplay(const std::string& input) {
        std::string result;
        
        for (char c : input) {
            if (c >= 32 && c <= 126) {
                result += c;
            } else {
                result += "?";
            }
        }
        
        return result;
    }

    /**
     * @brief Tronque une string à une longueur maximale
     */
    static std::string truncate(const std::string& input, size_t maxLength) {
        if (input.length() <= maxLength) {
            return input;
        }
        
        return input.substr(0, maxLength - 3) + "...";
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER JsonValidator.h
// ============================================================================

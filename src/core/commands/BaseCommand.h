// ============================================================================
// Fichier: src/core/commands/BaseCommand.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Classe de base pour toutes les commandes. Fournit des implémentations
//   par défaut et des helpers pour validation, logging, et création de réponses.
//
// Fonctionnalités:
//   - Implémentations par défaut des méthodes ICommand
//   - Helpers de validation (string, int, bool, range, pattern, array)
//   - Helpers de création de réponses JSON (success, error)
//   - Accès facile aux paramètres
//   - Logging standardisé
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include "interfaces/ICommand.h"
#include "../Logger.h"
#include <regex>         // Pour std::regex (validation patterns)
#include <algorithm>     // Pour std::clamp
#include <chrono>        // Pour timestamps

namespace midiMind {

// ============================================================================
// CLASSE: BaseCommand
// ============================================================================

/**
 * @class BaseCommand
 * @brief Classe de base pour toutes les commandes concrètes
 * 
 * Fournit des implémentations par défaut et des méthodes utilitaires
 * pour simplifier l'écriture des commandes concrètes.
 * 
 * @details
 * Les commandes concrètes héritent de cette classe et:
 * - Implémentent getName() et execute()
 * - Utilisent les helpers de validation
 * - Utilisent les helpers de réponse JSON
 * 
 * @note Cette classe stocke les paramètres dans params_
 * 
 * @example Commande minimale:
 * @code
 * class MyCommand : public BaseCommand {
 * public:
 *     MyCommand(const json& params) : BaseCommand(params) {}
 *     
 *     std::string getName() const override { 
 *         return "my.command"; 
 *     }
 *     
 *     bool validate(std::string& error) const override {
 *         return validateRequired("param1", error);
 *     }
 *     
 *     json execute() override {
 *         std::string value = params_["param1"];
 *         // ... logique
 *         return jsonSuccess("Operation completed");
 *     }
 * };
 * @endcode
 */
class BaseCommand : public ICommand {
public:
    // ========================================================================
    // CONSTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param params Paramètres de la commande (JSON)
     */
    explicit BaseCommand(const json& params) : params_(params) {}
    
    /**
     * @brief Destructeur virtuel
     */
    virtual ~BaseCommand() = default;
    
    // ========================================================================
    // IMPLÉMENTATIONS PAR DÉFAUT (ICommand)
    // ========================================================================
    
    /**
     * @brief Log l'exécution de la commande
     * 
     * Implémentation par défaut: log INFO avec le nom de la commande.
     */
    void logExecution() const override {
        Logger::info("Command", "Executing: " + getName());
    }
    
    /**
     * @brief Extrait la catégorie depuis le nom
     * 
     * Pour "devices.list" → retourne "devices"
     * Pour "player.play" → retourne "player"
     * 
     * @return std::string Catégorie ou "unknown" si format invalide
     */
    std::string getCategory() const override {
        std::string name = getName();
        size_t dotPos = name.find('.');
        if (dotPos != std::string::npos) {
            return name.substr(0, dotPos);
        }
        return "unknown";
    }
    
    // ========================================================================
    // HELPERS - ACCÈS AUX PARAMÈTRES
    // ========================================================================
    
    /**
     * @brief Récupère un paramètre avec valeur par défaut
     * 
     * @tparam T Type du paramètre
     * @param key Clé du paramètre
     * @param defaultValue Valeur par défaut si absent
     * @return T Valeur du paramètre ou valeur par défaut
     * 
     * @note Thread-safe si params_ n'est pas modifié
     * 
     * @example
     * @code
     * int timeout = getOptional("timeout", 5000);
     * bool verbose = getOptional("verbose", false);
     * std::string mode = getOptional("mode", std::string("auto"));
     * @endcode
     */
    template<typename T>
    T getOptional(const std::string& key, const T& defaultValue) const {
        if (!params_.contains(key)) {
            return defaultValue;
        }
        
        try {
            return params_[key].get<T>();
        } catch (const json::exception&) {
            return defaultValue;
        }
    }
    
    /**
     * @brief Récupère un paramètre requis
     * 
     * @tparam T Type du paramètre
     * @param key Clé du paramètre
     * @return T Valeur du paramètre
     * 
     * @throws std::runtime_error Si le paramètre n'existe pas ou type invalide
     * 
     * @example
     * @code
     * try {
     *     std::string deviceId = getRequired<std::string>("device_id");
     *     int channel = getRequired<int>("channel");
     * } catch (const std::runtime_error& e) {
     *     return jsonError(e.what());
     * }
     * @endcode
     */
    template<typename T>
    T getRequired(const std::string& key) const {
        if (!params_.contains(key)) {
            throw std::runtime_error("Missing required parameter: " + key);
        }
        
        try {
            return params_[key].get<T>();
        } catch (const json::exception& e) {
            throw std::runtime_error("Invalid type for parameter '" + key + "'");
        }
    }
    
    // ========================================================================
    // HELPERS - VALIDATION
    // ========================================================================
    
    /**
     * @brief Valide qu'un champ requis existe
     * 
     * @param field Nom du champ
     * @param error Message d'erreur (rempli si échec)
     * @return true Si le champ existe
     * @return false Si le champ est absent
     * 
     * @example
     * @code
     * bool validate(std::string& error) const override {
     *     if (!validateRequired("device_id", error)) return false;
     *     if (!validateRequired("channel", error)) return false;
     *     return true;
     * }
     * @endcode
     */
    bool validateRequired(const std::string& field, std::string& error) const {
        if (!params_.contains(field)) {
            error = "Missing required parameter: " + field;
            return false;
        }
        return true;
    }
    
    /**
     * @brief Valide un entier avec bornes optionnelles
     * 
     * @param field Nom du champ
     * @param minValue Valeur minimale (incluse)
     * @param maxValue Valeur maximale (incluse)
     * @param error Message d'erreur (rempli si échec)
     * @return true Si validation réussie
     * @return false Si validation échouée
     * 
     * @example
     * @code
     * // Valider un canal MIDI (1-16)
     * if (!validateInteger("channel", 1, 16, error)) {
     *     return false;
     * }
     * 
     * // Valider une vélocité (0-127)
     * if (!validateInteger("velocity", 0, 127, error)) {
     *     return false;
     * }
     * @endcode
     */
    bool validateInteger(const std::string& field, 
                        int minValue, 
                        int maxValue, 
                        std::string& error) const {
        if (!validateRequired(field, error)) {
            return false;
        }
        
        try {
            int value = params_[field].get<int>();
            
            if (value < minValue || value > maxValue) {
                error = "Field '" + field + "' must be between " + 
                       std::to_string(minValue) + " and " + std::to_string(maxValue) +
                       " (got " + std::to_string(value) + ")";
                return false;
            }
            
            return true;
            
        } catch (const json::exception& e) {
            error = "Field '" + field + "' must be an integer";
            return false;
        }
    }
    
    /**
     * @brief Valide une chaîne de caractères avec longueur max
     * 
     * @param field Nom du champ
     * @param maxLength Longueur maximale (0 = illimitée)
     * @param error Message d'erreur (rempli si échec)
     * @return true Si validation réussie
     * @return false Si validation échouée
     * 
     * @example
     * @code
     * // Nom de device max 64 caractères
     * if (!validateString("device_name", 64, error)) {
     *     return false;
     * }
     * @endcode
     */
    bool validateString(const std::string& field, 
                       size_t maxLength, 
                       std::string& error) const {
        if (!validateRequired(field, error)) {
            return false;
        }
        
        try {
            std::string value = params_[field].get<std::string>();
            
            if (maxLength > 0 && value.length() > maxLength) {
                error = "Field '" + field + "' exceeds maximum length of " + 
                       std::to_string(maxLength) + " characters";
                return false;
            }
            
            return true;
            
        } catch (const json::exception& e) {
            error = "Field '" + field + "' must be a string";
            return false;
        }
    }
    
    /**
     * @brief Valide une chaîne contre un pattern regex
     * 
     * @param field Nom du champ
     * @param pattern Pattern regex à matcher
     * @param patternDesc Description du pattern (pour message d'erreur)
     * @param error Message d'erreur (rempli si échec)
     * @return true Si validation réussie
     * @return false Si validation échouée
     * 
     * @example
     * @code
     * // Valider un ID device (alphanumeric + underscores)
     * if (!validatePattern("device_id", "^[a-zA-Z0-9_]+$", 
     *                     "alphanumeric with underscores", error)) {
     *     return false;
     * }
     * 
     * // Valider une adresse IP
     * if (!validatePattern("ip_address", 
     *                     "^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$",
     *                     "valid IP address", error)) {
     *     return false;
     * }
     * @endcode
     */
    bool validatePattern(const std::string& field,
                        const std::string& pattern,
                        const std::string& patternDesc,
                        std::string& error) const {
        if (!validateRequired(field, error)) {
            return false;
        }
        
        try {
            std::string value = params_[field].get<std::string>();
            std::regex regex(pattern);
            
            if (!std::regex_match(value, regex)) {
                error = "Field '" + field + "' has invalid format. " +
                       "Expected: " + patternDesc;
                return false;
            }
            
            return true;
            
        } catch (const json::exception& e) {
            error = "Field '" + field + "' must be a string";
            return false;
        }
    }
    
    /**
     * @brief Valide un booléen
     * 
     * @param field Nom du champ
     * @param error Message d'erreur (rempli si échec)
     * @return true Si validation réussie
     * @return false Si validation échouée
     * 
     * @example
     * @code
     * if (!validateBoolean("auto_reconnect", error)) {
     *     return false;
     * }
     * @endcode
     */
    bool validateBoolean(const std::string& field, std::string& error) const {
        if (!validateRequired(field, error)) {
            return false;
        }
        
        try {
            params_[field].get<bool>();
            return true;
            
        } catch (const json::exception& e) {
            error = "Field '" + field + "' must be a boolean (true/false)";
            return false;
        }
    }
    
    /**
     * @brief Valide qu'un champ est un tableau
     * 
     * @param field Nom du champ
     * @param minSize Taille minimale du tableau (0 = pas de minimum)
     * @param maxSize Taille maximale du tableau (0 = illimitée)
     * @param error Message d'erreur (rempli si échec)
     * @return true Si validation réussie
     * @return false Si validation échouée
     * 
     * @example
     * @code
     * // Valider une liste de device IDs (au moins 1)
     * if (!validateArray("device_ids", 1, 0, error)) {
     *     return false;
     * }
     * 
     * // Valider une liste de pistes (max 16)
     * if (!validateArray("tracks", 0, 16, error)) {
     *     return false;
     * }
     * @endcode
     */
    bool validateArray(const std::string& field, 
                      size_t minSize = 0,
                      size_t maxSize = 0,
                      std::string& error) const {
        if (!validateRequired(field, error)) {
            return false;
        }
        
        if (!params_[field].is_array()) {
            error = "Field '" + field + "' must be an array";
            return false;
        }
        
        size_t size = params_[field].size();
        
        if (size < minSize) {
            error = "Field '" + field + "' must contain at least " + 
                   std::to_string(minSize) + " element(s)";
            return false;
        }
        
        if (maxSize > 0 && size > maxSize) {
            error = "Field '" + field + "' must contain at most " + 
                   std::to_string(maxSize) + " element(s)";
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Valide qu'une valeur fait partie d'un ensemble
     * 
     * @tparam T Type de la valeur
     * @param field Nom du champ
     * @param allowedValues Valeurs autorisées
     * @param error Message d'erreur (rempli si échec)
     * @return true Si validation réussie
     * @return false Si validation échouée
     * 
     * @example
     * @code
     * // Valider un mode (parmi: "auto", "manual", "disabled")
     * if (!validateEnum("mode", {"auto", "manual", "disabled"}, error)) {
     *     return false;
     * }
     * @endcode
     */
    template<typename T>
    bool validateEnum(const std::string& field,
                     const std::vector<T>& allowedValues,
                     std::string& error) const {
        if (!validateRequired(field, error)) {
            return false;
        }
        
        try {
            T value = params_[field].get<T>();
            
            if (std::find(allowedValues.begin(), allowedValues.end(), value) == allowedValues.end()) {
                error = "Field '" + field + "' has invalid value. Allowed: ";
                for (size_t i = 0; i < allowedValues.size(); ++i) {
                    if (i > 0) error += ", ";
                    error += std::to_string(allowedValues[i]);
                }
                return false;
            }
            
            return true;
            
        } catch (const json::exception& e) {
            error = "Field '" + field + "' has invalid type";
            return false;
        }
    }
    
    // Spécialisation pour strings
    bool validateEnum(const std::string& field,
                     const std::vector<std::string>& allowedValues,
                     std::string& error) const {
        if (!validateRequired(field, error)) {
            return false;
        }
        
        try {
            std::string value = params_[field].get<std::string>();
            
            if (std::find(allowedValues.begin(), allowedValues.end(), value) == allowedValues.end()) {
                error = "Field '" + field + "' has invalid value. Allowed: ";
                for (size_t i = 0; i < allowedValues.size(); ++i) {
                    if (i > 0) error += ", ";
                    error += "'" + allowedValues[i] + "'";
                }
                return false;
            }
            
            return true;
            
        } catch (const json::exception& e) {
            error = "Field '" + field + "' must be a string";
            return false;
        }
    }
    
    // ========================================================================
    // HELPERS - CRÉATION DE RÉPONSES JSON
    // ========================================================================
    
    /**
     * @brief Crée une réponse de succès
     * 
     * @param message Message optionnel
     * @return json Objet JSON de succès
     * 
     * @example
     * @code
     * return jsonSuccess("Device connected successfully");
     * @endcode
     */
    json jsonSuccess(const std::string& message = "") const {
        json response;
        response["success"] = true;
        
        if (!message.empty()) {
            response["message"] = message;
        }
        
        response["timestamp"] = getCurrentTimestamp();
        
        return response;
    }
    
    /**
     * @brief Crée une réponse d'erreur
     * 
     * @param error Message d'erreur
     * @return json Objet JSON d'erreur
     * 
     * @example
     * @code
     * return jsonError("Device not found");
     * return jsonError("Invalid parameter: " + validationError);
     * @endcode
     */
    json jsonError(const std::string& error) const {
        json response;
        response["success"] = false;
        response["error"] = error;
        response["timestamp"] = getCurrentTimestamp();
        
        return response;
    }
    
    /**
     * @brief Récupère le timestamp actuel (millisecondes depuis epoch)
     * 
     * @return int64_t Timestamp en millisecondes
     */
    int64_t getCurrentTimestamp() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

protected:
    // ========================================================================
    // MEMBRES PROTÉGÉS
    // ========================================================================
    
    /**
     * @brief Paramètres de la commande (JSON)
     * 
     * Accessible aux classes dérivées pour lire les paramètres.
     */
    json params_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER BaseCommand.h
// ============================================================================

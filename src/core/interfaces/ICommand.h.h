// ============================================================================
// src/core/interfaces/ICommand.h
// Interface pour le pattern Command - Base de toutes les commandes API
// ============================================================================
#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @brief Interface pour le pattern Command
 * 
 * Toutes les commandes API doivent implémenter cette interface.
 * 
 * Avantages:
 * - Testabilité: chaque commande est testable indépendamment
 * - Extensibilité: ajouter des commandes sans modifier CommandProcessor
 * - Maintenabilité: logique organisée et modulaire
 * - Traçabilité: logging et audit automatiques
 * 
 * @example
 * ```cpp
 * class MyCommand : public ICommand {
 * public:
 *     std::string getName() const override { return "my.command"; }
 *     
 *     bool validate(std::string& error) const override {
 *         // Valider les paramètres
 *         return true;
 *     }
 *     
 *     json execute() override {
 *         // Exécuter la logique
 *         return {{"success", true}};
 *     }
 * };
 * ```
 */
class ICommand {
public:
    virtual ~ICommand() = default;
    
    /**
     * @brief Exécute la commande
     * 
     * Cette méthode contient la logique principale de la commande.
     * Elle doit retourner un JSON avec au minimum un champ "success".
     * 
     * @return Réponse JSON structurée:
     *         - success (bool): true si succès, false si erreur
     *         - data (object, optionnel): données de retour
     *         - error (string, optionnel): message d'erreur si échec
     * 
     * @throws std::exception En cas d'erreur critique
     * 
     * @example Réponse succès:
     * ```json
     * {
     *   "success": true,
     *   "data": {
     *     "devices": [...]
     *   }
     * }
     * ```
     * 
     * @example Réponse erreur:
     * ```json
     * {
     *   "success": false,
     *   "error": "Device not found"
     * }
     * ```
     */
    virtual json execute() = 0;
    
    /**
     * @brief Retourne le nom de la commande
     * 
     * Le nom doit suivre le format: "category.action"
     * 
     * @return Nom complet de la commande
     * 
     * @example
     * - "devices.list"
     * - "player.play"
     * - "routes.add"
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Valide les paramètres de la commande
     * 
     * Cette méthode est appelée AVANT execute() pour vérifier
     * que tous les paramètres requis sont présents et valides.
     * 
     * @param error Message d'erreur détaillé en cas d'échec
     * @return true si la validation réussit, false sinon
     * 
     * @note Si cette méthode retourne false, execute() ne sera pas appelée
     * 
     * @example
     * ```cpp
     * bool validate(std::string& error) const override {
     *     if (!params.contains("device_id")) {
     *         error = "Missing required parameter: device_id";
     *         return false;
     *     }
     *     return true;
     * }
     * ```
     */
    virtual bool validate(std::string& error) const = 0;
    
    /**
     * @brief Log l'exécution de la commande
     * 
     * Appelée automatiquement avant execute() pour tracer
     * l'utilisation de l'API.
     * 
     * @note Implémentation par défaut dans BaseCommand
     */
    virtual void logExecution() const = 0;
    
    /**
     * @brief Retourne la catégorie de la commande
     * 
     * Extrait automatiquement depuis le nom si format "category.action"
     * 
     * @return Catégorie (devices, player, routes, etc.)
     * 
     * @example
     * - getName() = "devices.list" → getCategory() = "devices"
     * - getName() = "player.play" → getCategory() = "player"
     */
    virtual std::string getCategory() const = 0;
    
    /**
     * @brief Retourne une description courte de la commande
     * 
     * Utilisée pour la documentation automatique de l'API.
     * 
     * @return Description en une phrase
     * 
     * @example
     * - "List all available MIDI devices"
     * - "Start playback of the loaded MIDI file"
     */
    virtual std::string getDescription() const = 0;
    
    /**
     * @brief Retourne les paramètres requis (optionnel)
     * 
     * Utilisé pour générer la documentation API automatiquement.
     * 
     * @return Tableau JSON décrivant les paramètres
     * 
     * @example
     * ```json
     * [
     *   {
     *     "name": "device_id",
     *     "type": "string",
     *     "required": true,
     *     "description": "ID of the device to connect"
     *   },
     *   {
     *     "name": "timeout_ms",
     *     "type": "integer",
     *     "required": false,
     *     "default": 5000,
     *     "description": "Connection timeout in milliseconds"
     *   }
     * ]
     * ```
     */
    virtual json getParameterSpec() const {
        // Implémentation par défaut: aucun paramètre documenté
        return json::array();
    }
    
    /**
     * @brief Retourne un exemple de requête (optionnel)
     * 
     * Utilisé pour la documentation et les tests.
     * 
     * @return JSON d'exemple de requête
     */
    virtual json getExampleRequest() const {
        return {{"command", getName()}};
    }
    
    /**
     * @brief Retourne un exemple de réponse (optionnel)
     * 
     * Utilisé pour la documentation.
     * 
     * @return JSON d'exemple de réponse
     */
    virtual json getExampleResponse() const {
        return {{"success", true}};
    }
};

} // namespace midiMind

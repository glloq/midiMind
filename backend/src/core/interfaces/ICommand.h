// ============================================================================
// Fichier: backend/src/core/interfaces/ICommand.h
// Version: 1.0.0
// Date: 2025-10-15
// ============================================================================
// Description:
//   Interface de base pour le Command Pattern utilisé dans CommandProcessorV2
//   Toutes les commandes doivent implémenter cette interface
// ============================================================================

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <memory>

using json = nlohmann::json;

namespace midiMind {

/**
 * @interface ICommand
 * @brief Interface pour les commandes du Command Pattern
 * 
 * Chaque commande implémente cette interface et définit son comportement
 * dans la méthode execute()
 */
class ICommand {
public:
    /**
     * @brief Destructeur virtuel
     */
    virtual ~ICommand() = default;
    
    /**
     * @brief Exécute la commande avec les paramètres fournis
     * 
     * @param params Paramètres JSON de la commande
     * @return json Résultat de l'exécution (format: {"success": bool, "data": {...}})
     * 
     * @throws std::exception En cas d'erreur d'exécution
     */
    virtual json execute(const json& params) = 0;
    
    /**
     * @brief Retourne le nom de la commande
     * @return std::string Nom de la commande
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Retourne la description de la commande
     * @return std::string Description
     */
    virtual std::string getDescription() const {
        return "No description available";
    }
    
    /**
     * @brief Valide les paramètres avant exécution
     * 
     * @param params Paramètres à valider
     * @return bool True si valide, false sinon
     */
    virtual bool validateParams(const json& params) const {
        return true;  // Par défaut, accepte tous les paramètres
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ICommand.h
// ============================================================================

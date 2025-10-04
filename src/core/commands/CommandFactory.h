// ============================================================================
// Fichier: src/core/commands/CommandFactory.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Factory pour la création dynamique des commandes. Utilise le pattern
//   Factory pour créer les bonnes instances de commandes à partir de leur nom.
//
// Design Pattern:
//   - Factory Pattern (création d'objets)
//   - Registry Pattern (enregistrement des créateurs)
//   - Singleton (instance unique de la factory)
//
// Fonctionnalités:
//   - Enregistrement de créateurs de commandes
//   - Création dynamique par nom
//   - Introspection (liste des commandes disponibles)
//   - Thread-safe
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <string>              // Pour std::string
#include <map>                 // Pour std::map
#include <memory>              // Pour std::unique_ptr
#include <functional>          // Pour std::function
#include <mutex>               // Pour std::mutex
#include <vector>              // Pour std::vector
#include <algorithm>           // Pour std::sort
#include <nlohmann/json.hpp>   // Pour json

#include "interfaces/ICommand.h"  // Interface de base
#include "../Logger.h"            // Pour logging

// Alias
using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// CLASSE: CommandFactory
// ============================================================================

/**
 * @class CommandFactory
 * @brief Factory pour créer dynamiquement des commandes
 * 
 * Cette classe permet d'enregistrer des fonctions de création (créateurs)
 * pour chaque type de commande, puis de créer des instances à la demande
 * en fonction du nom de la commande.
 * 
 * @details
 * Le pattern Factory permet de:
 * - Centraliser la création des objets
 * - Découpler la création de l'utilisation
 * - Faciliter l'ajout de nouvelles commandes
 * - Supporter la création dynamique depuis des strings
 * 
 * Architecture:
 * - Factory stocke des lambdas de création (CommandCreator)
 * - Chaque lambda capture les dépendances nécessaires
 * - La création est déclenchée par le nom de la commande
 * - Retourne un unique_ptr<ICommand>
 * 
 * @note Thread-safe : peut être utilisé depuis plusieurs threads
 * @note Les commandes créées sont uniques (unique_ptr)
 * 
 * @example Enregistrement de commandes:
 * @code
 * CommandFactory factory;
 * 
 * // Enregistrer une commande simple
 * factory.registerCommand("system.ping", [](const json& params) {
 *     return std::make_unique<SystemPingCommand>(params);
 * });
 * 
 * // Enregistrer une commande avec dépendances
 * auto deviceMgr = std::make_shared<MidiDeviceManager>();
 * factory.registerCommand("devices.list", [deviceMgr](const json& params) {
 *     return std::make_unique<DeviceListCommand>(params, deviceMgr);
 * });
 * @endcode
 * 
 * @example Création et exécution:
 * @code
 * // Créer une commande
 * auto cmd = factory.create("devices.list", {});
 * if (!cmd) {
 *     std::cerr << "Unknown command" << std::endl;
 *     return;
 * }
 * 
 * // Exécuter
 * json response = cmd->execute();
 * std::cout << response.dump(2) << std::endl;
 * @endcode
 */
class CommandFactory {
public:
    // ========================================================================
    // TYPE: Créateur de Commande
    // ========================================================================
    
    /**
     * @typedef CommandCreator
     * @brief Type de fonction pour créer une commande
     * 
     * Signature: prend des paramètres JSON, retourne unique_ptr<ICommand>
     * 
     * @param params Paramètres de la commande (JSON)
     * @return std::unique_ptr<ICommand> Instance de la commande
     */
    using CommandCreator = std::function<std::unique_ptr<ICommand>(const json&)>;
    
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur par défaut
     */
    CommandFactory() = default;
    
    /**
     * @brief Destructeur
     */
    ~CommandFactory() = default;
    
    // Désactiver copie et assignation
    CommandFactory(const CommandFactory&) = delete;
    CommandFactory& operator=(const CommandFactory&) = delete;
    
    // ========================================================================
    // ENREGISTREMENT DE COMMANDES
    // ========================================================================
    
    /**
     * @brief Enregistre une commande dans la factory
     * 
     * Associe un nom de commande à une fonction de création.
     * Si une commande du même nom existe déjà, elle est remplacée.
     * 
     * @param name Nom de la commande (ex: "devices.list")
     * @param creator Fonction de création (lambda ou functor)
     * 
     * @note Thread-safe
     * @note Remplace silencieusement si le nom existe déjà
     * 
     * @example
     * @code
     * factory.registerCommand("player.play", [player](const json& params) {
     *     return std::make_unique<PlayerPlayCommand>(params, player);
     * });
     * @endcode
     */
    void registerCommand(const std::string& name, CommandCreator creator) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Vérifier si déjà enregistré
        if (creators_.find(name) != creators_.end()) {
            Logger::warn("CommandFactory", 
                "Command '" + name + "' already registered, replacing");
        }
        
        creators_[name] = creator;
        
        Logger::debug("CommandFactory", "Registered command: " + name);
    }
    
    /**
     * @brief Enregistre plusieurs commandes en une fois
     * 
     * Helper pour enregistrer un batch de commandes.
     * 
     * @param commands Map de (nom -> créateur)
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * std::map<std::string, CommandCreator> commands = {
     *     {"devices.list", [](const json& p) { return std::make_unique<...>(...); }},
     *     {"devices.connect", [](const json& p) { return std::make_unique<...>(...); }}
     * };
     * factory.registerCommands(commands);
     * @endcode
     */
    void registerCommands(const std::map<std::string, CommandCreator>& commands) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [name, creator] : commands) {
            creators_[name] = creator;
            Logger::debug("CommandFactory", "Registered command: " + name);
        }
        
        Logger::info("CommandFactory", 
            "Registered " + std::to_string(commands.size()) + " commands");
    }
    
    /**
     * @brief Désenregistre une commande
     * 
     * @param name Nom de la commande à retirer
     * @return true Si la commande a été retirée
     * @return false Si la commande n'existait pas
     * 
     * @note Thread-safe
     */
    bool unregisterCommand(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = creators_.find(name);
        if (it == creators_.end()) {
            return false;
        }
        
        creators_.erase(it);
        Logger::debug("CommandFactory", "Unregistered command: " + name);
        
        return true;
    }
    
    /**
     * @brief Efface toutes les commandes enregistrées
     * 
     * @note Thread-safe
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = creators_.size();
        creators_.clear();
        
        Logger::info("CommandFactory", 
            "Cleared " + std::to_string(count) + " commands");
    }
    
    // ========================================================================
    // CRÉATION DE COMMANDES
    // ========================================================================
    
    /**
     * @brief Crée une instance de commande
     * 
     * @param name Nom de la commande
     * @param params Paramètres de la commande
     * @return std::unique_ptr<ICommand> Instance, ou nullptr si non trouvée
     * 
     * @note Thread-safe
     * @note Retourne nullptr si la commande n'existe pas
     * 
     * @example
     * @code
     * auto cmd = factory.create("devices.list", {});
     * if (!cmd) {
     *     std::cerr << "Unknown command" << std::endl;
     *     return;
     * }
     * 
     * auto response = cmd->execute();
     * @endcode
     */
    std::unique_ptr<ICommand> create(const std::string& name, 
                                     const json& params) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = creators_.find(name);
        if (it == creators_.end()) {
            Logger::debug("CommandFactory", "Command not found: " + name);
            return nullptr;
        }
        
        try {
            return it->second(params);
            
        } catch (const std::exception& e) {
            Logger::error("CommandFactory", 
                "Failed to create command '" + name + "': " + e.what());
            return nullptr;
        }
    }
    
    /**
     * @brief Crée une instance et exécute immédiatement
     * 
     * Helper pour créer + exécuter en une seule ligne.
     * 
     * @param name Nom de la commande
     * @param params Paramètres de la commande
     * @param response Réponse JSON (remplie si succès)
     * @return true Si la commande a été créée et exécutée
     * @return false Si la commande n'existe pas ou a échoué
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * json response;
     * if (factory.createAndExecute("devices.list", {}, response)) {
     *     std::cout << response.dump(2) << std::endl;
     * }
     * @endcode
     */
    bool createAndExecute(const std::string& name, 
                         const json& params,
                         json& response) const {
        auto cmd = create(name, params);
        if (!cmd) {
            response = {
                {"success", false},
                {"error", "Unknown command: " + name}
            };
            return false;
        }
        
        try {
            response = cmd->execute();
            return true;
            
        } catch (const std::exception& e) {
            response = {
                {"success", false},
                {"error", "Execution failed: " + std::string(e.what())}
            };
            return false;
        }
    }
    
    // ========================================================================
    // INTROSPECTION
    // ========================================================================
    
    /**
     * @brief Vérifie si une commande est enregistrée
     * 
     * @param name Nom de la commande
     * @return true Si la commande existe
     * @return false Si la commande n'existe pas
     * 
     * @note Thread-safe
     */
    bool has(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return creators_.find(name) != creators_.end();
    }
    
    /**
     * @brief Compte le nombre de commandes enregistrées
     * 
     * @return size_t Nombre de commandes
     * 
     * @note Thread-safe
     */
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return creators_.size();
    }
    
    /**
     * @brief Liste toutes les commandes enregistrées
     * 
     * @return std::vector<std::string> Noms des commandes (triés)
     * 
     * @note Thread-safe
     * @note Retourne une copie triée alphabétiquement
     * 
     * @example
     * @code
     * auto commands = factory.listCommands();
     * for (const auto& name : commands) {
     *     std::cout << "- " << name << std::endl;
     * }
     * @endcode
     */
    std::vector<std::string> listCommands() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> names;
        names.reserve(creators_.size());
        
        for (const auto& [name, _] : creators_) {
            names.push_back(name);
        }
        
        // Trier alphabétiquement
        std::sort(names.begin(), names.end());
        
        return names;
    }
    
    /**
     * @brief Liste les commandes par catégorie
     * 
     * Regroupe les commandes par préfixe (avant le point).
     * 
     * @return std::map<std::string, std::vector<std::string>> 
     *         Map de (catégorie -> liste de commandes)
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * auto byCategory = factory.listCommandsByCategory();
     * // {
     * //   "devices": ["devices.list", "devices.connect", ...],
     * //   "player": ["player.play", "player.pause", ...],
     * //   ...
     * // }
     * @endcode
     */
    std::map<std::string, std::vector<std::string>> listCommandsByCategory() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::map<std::string, std::vector<std::string>> byCategory;
        
        for (const auto& [name, _] : creators_) {
            // Extraire catégorie depuis le nom
            size_t dotPos = name.find('.');
            std::string category = (dotPos != std::string::npos) 
                                  ? name.substr(0, dotPos) 
                                  : "other";
            
            byCategory[category].push_back(name);
        }
        
        // Trier les commandes dans chaque catégorie
        for (auto& [category, commands] : byCategory) {
            std::sort(commands.begin(), commands.end());
        }
        
        return byCategory;
    }
    
    /**
     * @brief Récupère des statistiques sur les commandes
     * 
     * @return json Objet JSON avec statistiques
     * 
     * @note Thread-safe
     * 
     * @example Résultat:
     * @code{.json}
     * {
     *   "total": 42,
     *   "categories": {
     *     "devices": 5,
     *     "player": 8,
     *     "routes": 7,
     *     ...
     *   },
     *   "commands": ["devices.list", "devices.connect", ...]
     * }
     * @endcode
     */
    json getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        json stats;
        stats["total"] = creators_.size();
        
        // Compter par catégorie
        std::map<std::string, int> categoryCount;
        for (const auto& [name, _] : creators_) {
            size_t dotPos = name.find('.');
            std::string category = (dotPos != std::string::npos) 
                                  ? name.substr(0, dotPos) 
                                  : "other";
            categoryCount[category]++;
        }
        
        stats["categories"] = categoryCount;
        
        // Liste complète (triée)
        auto commands = listCommands();
        stats["commands"] = commands;
        
        return stats;
    }
    
    /**
     * @brief Affiche les commandes dans la console (debug)
     * 
     * @note Thread-safe
     */
    void printCommands() const {
        auto byCategory = listCommandsByCategory();
        
        Logger::info("CommandFactory", "=== Registered Commands ===");
        
        for (const auto& [category, commands] : byCategory) {
            Logger::info("CommandFactory", "Category: " + category);
            
            for (const auto& cmd : commands) {
                Logger::info("CommandFactory", "  - " + cmd);
            }
        }
        
        Logger::info("CommandFactory", 
            "Total: " + std::to_string(count()) + " commands");
    }

private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /**
     * @brief Map des créateurs de commandes
     * 
     * Key: nom de la commande
     * Value: fonction créatrice
     */
    std::map<std::string, CommandCreator> creators_;
    
    /**
     * @brief Mutex pour thread-safety
     * 
     * Protège l'accès concurrent à creators_.
     * Mutable pour permettre le lock dans les méthodes const.
     */
    mutable std::mutex mutex_;
};

} // namespace midiMind

// ============================================================================
// EXEMPLES D'UTILISATION AVANCÉS
// ============================================================================

/**
 * @example Configuration complète de la factory
 * @code
 * void setupCommandFactory(CommandFactory& factory,
 *                         std::shared_ptr<MidiDeviceManager> deviceMgr,
 *                         std::shared_ptr<MidiRouter> router,
 *                         std::shared_ptr<MidiPlayer> player,
 *                         std::shared_ptr<MidiFileManager> fileMgr) {
 *     
 *     // ========================================================================
 *     // COMMANDES DEVICES
 *     // ========================================================================
 *     
 *     factory.registerCommand("devices.list", 
 *         [deviceMgr](const json& params) {
 *             return std::make_unique<DeviceListCommand>(params, deviceMgr);
 *         });
 *     
 *     factory.registerCommand("devices.connect", 
 *         [deviceMgr](const json& params) {
 *             return std::make_unique<DeviceConnectCommand>(params, deviceMgr);
 *         });
 *     
 *     factory.registerCommand("devices.disconnect", 
 *         [deviceMgr](const json& params) {
 *             return std::make_unique<DeviceDisconnectCommand>(params, deviceMgr);
 *         });
 *     
 *     // ========================================================================
 *     // COMMANDES PLAYER
 *     // ========================================================================
 *     
 *     factory.registerCommand("player.play", 
 *         [player](const json& params) {
 *             return std::make_unique<PlayerPlayCommand>(params, player);
 *         });
 *     
 *     factory.registerCommand("player.pause", 
 *         [player](const json& params) {
 *             return std::make_unique<PlayerPauseCommand>(params, player);
 *         });
 *     
 *     factory.registerCommand("player.stop", 
 *         [player](const json& params) {
 *             return std::make_unique<PlayerStopCommand>(params, player);
 *         });
 *     
 *     // ... etc pour toutes les commandes
 *     
 *     Logger::info("CommandFactory", 
 *         "Registered " + std::to_string(factory.count()) + " commands");
 * }
 * @endcode
 */

// ============================================================================
// FIN DU FICHIER CommandFactory.h
// ============================================================================

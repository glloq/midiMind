// ============================================================================
// Fichier: backend/src/core/commands/CommandFactory.h
// Version: 3.0.1 - COMPLET ET FONCTIONNEL
// Date: 2025-10-15
// ============================================================================
// Description:
//   Factory pour enregistrement et exécution de commandes via lambdas
//   Pattern: Command Pattern avec Factory
//
// Architecture:
//   CommandFactory stocke des lambdas (json -> json)
//   Utilisé par CommandProcessorV2 pour router les commandes
//
// Thread-safety: OUI (mutex sur toutes les opérations)
//
// Auteur: MidiMind Team
// ============================================================================

#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <mutex>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "../Logger.h"
#include "../interfaces/ICommand.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @class CommandFactory
 * @brief Factory pour enregistrement et exécution de commandes
 * 
 * Permet d'enregistrer des commandes sous forme de lambdas.
 * Alternative légère à ICommand pour les cas simples.
 * 
 * Signature des lambdas: (const json& params) -> json
 */
class CommandFactory {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @typedef CommandExecutor
     * @brief Type de fonction pour exécuter une commande
     * 
     * Signature: prend des paramètres JSON, retourne une réponse JSON
     */
    using CommandExecutor = std::function<json(const json&)>;
    
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    CommandFactory() {
        Logger::debug("CommandFactory", "CommandFactory constructed");
    }
    
    ~CommandFactory() {
        Logger::debug("CommandFactory", 
            "CommandFactory destroyed (" + std::to_string(executors_.size()) + " commands)");
    }
    
    // Désactiver copie et assignation
    CommandFactory(const CommandFactory&) = delete;
    CommandFactory& operator=(const CommandFactory&) = delete;
    
    // ========================================================================
    // ENREGISTREMENT DE COMMANDES
    // ========================================================================
    
    /**
     * @brief Enregistre une lambda d'exécution
     * 
     * @param name Nom de la commande (ex: "devices.list")
     * @param executor Lambda (json) -> json
     */
    void registerCommand(const std::string& name, CommandExecutor executor) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (executors_.find(name) != executors_.end()) {
            Logger::warn("CommandFactory", 
                "Command '" + name + "' already registered, replacing");
        }
        
        executors_[name] = executor;
        Logger::debug("CommandFactory", "✓ Registered command: " + name);
    }
    
    /**
     * @brief Enregistre une commande ICommand
     * 
     * @param name Nom de la commande
     * @param command Pointeur partagé vers ICommand
     */
    void registerCommand(const std::string& name, std::shared_ptr<ICommand> command) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (executors_.find(name) != executors_.end()) {
            Logger::warn("CommandFactory", 
                "Command '" + name + "' already registered, replacing");
        }
        
        // Wrapper: ICommand -> lambda
        executors_[name] = [command](const json& params) -> json {
            return command->execute(params);
        };
        
        Logger::debug("CommandFactory", "✓ Registered ICommand: " + name);
    }
    
    // ========================================================================
    // EXÉCUTION
    // ========================================================================
    
    /**
     * @brief Exécute directement une commande
     * 
     * @param name Nom de la commande
     * @param params Paramètres JSON
     * @return json Réponse de la commande
     * @throws std::runtime_error si la commande n'existe pas
     */
    json execute(const std::string& name, const json& params) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = executors_.find(name);
        if (it == executors_.end()) {
            throw std::runtime_error("Unknown command: " + name);
        }
        
        try {
            Logger::debug("CommandFactory", "Executing command: " + name);
            return it->second(params);
            
        } catch (const std::exception& e) {
            Logger::error("CommandFactory", 
                "Error executing '" + name + "': " + e.what());
            throw;
        }
    }
    
    // ========================================================================
    // INTROSPECTION
    // ========================================================================
    
    /**
     * @brief Vérifie si une commande existe
     */
    bool exists(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return executors_.find(name) != executors_.end();
    }
    
    /**
     * @brief Compte total de commandes
     */
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return executors_.size();
    }
    
    /**
     * @brief Liste toutes les commandes
     */
    std::vector<std::string> listCommands() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> commands;
        commands.reserve(executors_.size());
        
        for (const auto& [name, _] : executors_) {
            commands.push_back(name);
        }
        
        std::sort(commands.begin(), commands.end());
        return commands;
    }
    
    /**
     * @brief Liste les commandes par catégorie
     */
    std::map<std::string, std::vector<std::string>> listCommandsByCategory() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::map<std::string, std::vector<std::string>> byCategory;
        
        for (const auto& [name, _] : executors_) {
            std::string category = extractCategory(name);
            byCategory[category].push_back(name);
        }
        
        return byCategory;
    }
    
    /**
     * @brief Liste les catégories
     */
    std::vector<std::string> listCategories() const {
        auto byCategory = listCommandsByCategory();
        
        std::vector<std::string> categories;
        categories.reserve(byCategory.size());
        
        for (const auto& [category, _] : byCategory) {
            categories.push_back(category);
        }
        
        return categories;
    }
    
    /**
     * @brief Compte par catégorie
     */
    std::map<std::string, size_t> countByCategory() const {
        auto byCategory = listCommandsByCategory();
        
        std::map<std::string, size_t> counts;
        for (const auto& [category, commands] : byCategory) {
            counts[category] = commands.size();
        }
        
        return counts;
    }
    
    /**
     * @brief Vérifie si vide
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return executors_.empty();
    }
    
    /**
     * @brief Efface toutes les commandes
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        executors_.clear();
        Logger::info("CommandFactory", "All commands cleared");
    }
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Extrait la catégorie d'un nom de commande
     * 
     * @param name Nom complet (ex: "devices.list")
     * @return string Catégorie (ex: "devices")
     */
    static std::string extractCategory(const std::string& name) {
        size_t dotPos = name.find('.');
        if (dotPos == std::string::npos) {
            return "uncategorized";
        }
        return name.substr(0, dotPos);
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    mutable std::mutex mutex_;  ///< Mutex pour thread-safety
    std::map<std::string, CommandExecutor> executors_;  ///< Map nom -> executor
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CommandFactory.h v3.0.1 - COMPLET
// ============================================================================

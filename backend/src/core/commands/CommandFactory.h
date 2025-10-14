// ============================================================================
// Fichier: src/core/commands/CommandFactory.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Factory Pattern pour la création dynamique de commandes API.
//   Permet d'enregistrer, créer et gérer les commandes de manière centralisée.
//
// Fonctionnalités:
//   - Enregistrement dynamique de commandes
//   - Création thread-safe d'instances de commandes
//   - Introspection (liste, compte, vérification existence)
//   - Support pour documentation automatique
//   - Gestion par catégorie
//
// Auteur: midiMind Team
// Date: 2025-10-05
// Version: 3.0.0 - MISE À JOUR COMPLÈTE
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <string>              // Pour std::string
#include <memory>              // Pour std::unique_ptr, std::shared_ptr
#include <functional>          // Pour std::function
#include <unordered_map>       // Pour std::unordered_map
#include <map>                 // Pour std::map
#include <vector>              // Pour std::vector
#include <mutex>               // Pour std::mutex
#include <algorithm>           // Pour std::sort
#include <nlohmann/json.hpp>   // Pour json

#include "../interfaces/ICommand.h"
#include "../Logger.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @class CommandFactory
 * @brief Factory pour lambdas d'exécution directe
 * 
 * NOUVELLE APPROCHE: Les lambdas retournent directement du JSON,
 * sans passer par des objets Command intermédiaires.
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
    
    // ========================================================================
    // DEBUG / LOGGING
    // ========================================================================
    
    /**
     * @brief Log toutes les commandes enregistrées
     */
    void logRegisteredCommands(const std::string& logLevel = "INFO") const {
        auto byCategory = listCommandsByCategory();
        
        Logger::log(logLevel, "CommandFactory", 
            "=== Registered Commands (" + std::to_string(count()) + " total) ===");
        
        for (const auto& [category, commands] : byCategory) {
            Logger::log(logLevel, "CommandFactory", 
                "  [" + category + "] (" + std::to_string(commands.size()) + " commands)");
            
            for (const auto& cmd : commands) {
                Logger::log(logLevel, "CommandFactory", "    - " + cmd);
            }
        }
        
        Logger::log(logLevel, "CommandFactory", "======================================");
    }
    
    /**
     * @brief Statistiques
     */
    json getStatistics() const {
        json stats;
        stats["total_commands"] = count();
        stats["by_category"] = countByCategory();
        stats["categories"] = listCategories();
        stats["is_empty"] = empty();
        
        return stats;
    }

private:
    // ========================================================================
    // HELPERS PRIVÉS
    // ========================================================================
    
    /**
     * @brief Extrait la catégorie depuis le nom
     * 
     * "devices.list" -> "devices"
     * "player.play" -> "player"
     */
    std::string extractCategory(const std::string& name) const {
        size_t dotPos = name.find('.');
        if (dotPos != std::string::npos) {
            return name.substr(0, dotPos);
        }
        return "other";
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    std::unordered_map<std::string, CommandExecutor> executors_;
    mutable std::mutex mutex_;
};

} // namespace midiMind
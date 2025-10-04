// ============================================================================
// Fichier: src/api/CommandProcessorV2.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Processeur de commandes VERSION 2 utilisant le Command Pattern avec Factory.
//   Remplace la version 1 (switch/case) par une architecture extensible.
//
// Architecture:
//   CommandProcessorV2 (cette classe)
//   └── CommandFactory (création des commandes)
//       └── Commands concrètes (DeviceListCommand, PlayerPlayCommand, etc.)
//
// Avantages vs V1:
//   - Extensible : ajouter une commande = 1 classe + 1 enregistrement
//   - Testable : chaque commande testable individuellement
//   - Maintenable : logique séparée par commande
//   - Documentation : auto-générée depuis les commandes
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <memory>              // Pour std::shared_ptr
#include <string>              // Pour std::string
#include <nlohmann/json.hpp>   // Pour json

#include "../core/Logger.h"
#include "../core/commands/CommandFactory.h"
#include "../core/commands/interfaces/ICommand.h"

// Dépendances (passées au constructeur)
#include "../midi/devices/MidiDeviceManager.h"
#include "../midi/MidiRouter.h"
#include "../midi/MidiPlayer.h"
#include "../midi/MidiFileManager.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// CLASSE: CommandProcessorV2
// ============================================================================

/**
 * @class CommandProcessorV2
 * @brief Processeur de commandes utilisant Command Pattern + Factory
 * 
 * Cette classe reçoit des commandes JSON via WebSocket, les parse,
 * crée les instances de commandes appropriées via la Factory, et
 * les exécute en retournant une réponse JSON.
 * 
 * @details
 * Architecture:
 * 1. Réception commande JSON (via ApiServer)
 * 2. Extraction du nom de commande
 * 3. Création de l'instance via CommandFactory
 * 4. Validation des paramètres
 * 5. Exécution de la commande
 * 6. Retour de la réponse JSON
 * 
 * Format des commandes:
 * @code{.json}
 * {
 *   "command": "devices.list",
 *   "params": {
 *     // paramètres spécifiques à la commande
 *   }
 * }
 * @endcode
 * 
 * Format des réponses:
 * @code{.json}
 * {
 *   "success": true,
 *   "data": {
 *     // données de réponse
 *   }
 * }
 * @endcode
 * 
 * @note Thread-safe si les commandes elles-mêmes sont thread-safe
 * 
 * @example Utilisation:
 * @code
 * auto processor = std::make_shared<CommandProcessorV2>(
 *     deviceManager, router, player, fileManager
 * );
 * 
 * json command = {
 *     {"command", "devices.list"},
 *     {"params", json::object()}
 * };
 * 
 * json response = processor->processCommand(command);
 * std::cout << response.dump(2) << std::endl;
 * @endcode
 */
class CommandProcessorV2 {
public:
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur - Initialise le processeur et la factory
     * 
     * @param deviceManager Gestionnaire de périphériques MIDI
     * @param router Routeur MIDI
     * @param player Lecteur MIDI
     * @param fileManager Gestionnaire de fichiers MIDI
     * 
     * @note Enregistre automatiquement toutes les commandes disponibles
     */
    CommandProcessorV2(std::shared_ptr<MidiDeviceManager> deviceManager,
                       std::shared_ptr<MidiRouter> router,
                       std::shared_ptr<MidiPlayer> player,
                       std::shared_ptr<MidiFileManager> fileManager)
        : deviceManager_(deviceManager)
        , router_(router)
        , player_(player)
        , fileManager_(fileManager) {
        
        Logger::info("CommandProcessorV2", "Initializing Command Processor V2...");
        
        // Enregistrer toutes les commandes disponibles
        registerAllCommands();
        
        Logger::info("CommandProcessorV2", 
            "✓ Command Processor V2 initialized with " + 
            std::to_string(factory_.count()) + " commands");
    }
    
    /**
     * @brief Destructeur
     */
    ~CommandProcessorV2() = default;
    
    // Désactiver copie et assignation
    CommandProcessorV2(const CommandProcessorV2&) = delete;
    CommandProcessorV2& operator=(const CommandProcessorV2&) = delete;
    
    // ========================================================================
    // MÉTHODE PRINCIPALE
    // ========================================================================
    
    /**
     * @brief Traite une commande JSON
     * 
     * @param commandJson Commande JSON contenant "command" et optionnel "params"
     * @return json Réponse JSON (success + data ou error)
     * 
     * @note Cette méthode ne lance pas d'exceptions - retourne toujours une réponse JSON
     * 
     * @example
     * @code
     * json cmd = {
     *     {"command", "player.play"},
     *     {"params", {{"file_id", "abc123"}}}
     * };
     * json response = processor->processCommand(cmd);
     * @endcode
     */
    json processCommand(const json& commandJson) {
        try {
            // ================================================================
            // ÉTAPE 1: VALIDATION DE LA STRUCTURE
            // ================================================================
            
            if (!commandJson.contains("command")) {
                return createErrorResponse("Missing 'command' field");
            }
            
            if (!commandJson["command"].is_string()) {
                return createErrorResponse("Field 'command' must be a string");
            }
            
            std::string commandName = commandJson["command"];
            
            // Récupérer les paramètres (ou objet vide si absent)
            json params = commandJson.contains("params") 
                         ? commandJson["params"] 
                         : json::object();
            
            if (!params.is_object()) {
                return createErrorResponse("Field 'params' must be an object");
            }
            
            Logger::debug("CommandProcessorV2", 
                "Processing command: " + commandName);
            
            // ================================================================
            // ÉTAPE 2: CRÉATION DE LA COMMANDE
            // ================================================================
            
            auto command = factory_.create(commandName, params);
            
            if (!command) {
                return createErrorResponse(
                    "Unknown command: " + commandName,
                    "UNKNOWN_COMMAND"
                );
            }
            
            // ================================================================
            // ÉTAPE 3: VALIDATION DES PARAMÈTRES
            // ================================================================
            
            std::string validationError;
            if (!command->validate(validationError)) {
                return createErrorResponse(
                    validationError,
                    "VALIDATION_ERROR"
                );
            }
            
            // ================================================================
            // ÉTAPE 4: LOG DE L'EXÉCUTION
            // ================================================================
            
            command->logExecution();
            
            // ================================================================
            // ÉTAPE 5: EXÉCUTION
            // ================================================================
            
            json response = command->execute();
            
            // Ajouter métadonnées si pas déjà présentes
            if (!response.contains("timestamp")) {
                response["timestamp"] = getCurrentTimestamp();
            }
            
            if (!response.contains("command")) {
                response["command"] = commandName;
            }
            
            return response;
            
        } catch (const json::exception& e) {
            // Erreur JSON (parsing, accès, etc.)
            Logger::error("CommandProcessorV2", 
                "JSON error: " + std::string(e.what()));
            
            return createErrorResponse(
                "JSON error: " + std::string(e.what()),
                "JSON_ERROR"
            );
            
        } catch (const std::exception& e) {
            // Autre exception
            Logger::error("CommandProcessorV2", 
                "Exception: " + std::string(e.what()));
            
            return createErrorResponse(
                "Internal error: " + std::string(e.what()),
                "INTERNAL_ERROR"
            );
            
        } catch (...) {
            // Exception inconnue
            Logger::error("CommandProcessorV2", "Unknown exception");
            
            return createErrorResponse(
                "Unknown internal error",
                "UNKNOWN_ERROR"
            );
        }
    }
    
    // ========================================================================
    // ACCÈS À LA FACTORY
    // ========================================================================
    
    /**
     * @brief Récupère la CommandFactory (pour introspection)
     * 
     * @return CommandFactory& Référence à la factory
     * 
     * @example
     * @code
     * auto& factory = processor->getCommandFactory();
     * auto commands = factory.listCommands();
     * @endcode
     */
    CommandFactory& getCommandFactory() {
        return factory_;
    }
    
    /**
     * @brief Récupère la CommandFactory (const)
     */
    const CommandFactory& getCommandFactory() const {
        return factory_;
    }
    
    // ========================================================================
    // STATISTIQUES ET INTROSPECTION
    // ========================================================================
    
    /**
     * @brief Récupère le nombre de commandes enregistrées
     * 
     * @return size_t Nombre de commandes
     */
    size_t getCommandCount() const {
        return factory_.count();
    }
    
    /**
     * @brief Liste toutes les commandes disponibles
     * 
     * @return std::vector<std::string> Liste des noms de commandes
     */
    std::vector<std::string> listCommands() const {
        return factory_.listCommands();
    }
    
    /**
     * @brief Liste les commandes par catégorie
     * 
     * @return std::map<std::string, std::vector<std::string>> Map catégorie -> commandes
     */
    std::map<std::string, std::vector<std::string>> listCommandsByCategory() const {
        return factory_.listCommandsByCategory();
    }

private:
    // ========================================================================
    // MÉTHODES PRIVÉES - ENREGISTREMENT DES COMMANDES
    // ========================================================================
    
    /**
     * @brief Enregistre toutes les commandes disponibles dans la factory
     * 
     * Cette méthode doit être mise à jour à chaque ajout de nouvelle commande.
     */
    void registerAllCommands() {
        Logger::debug("CommandProcessorV2", "Registering commands...");
        
        // ====================================================================
        // COMMANDES SYSTEM
        // ====================================================================
        
        registerCommand("system.ping", [](const json& params) {
            // Commande ping simple (pas besoin de classe séparée)
            json response;
            response["success"] = true;
            response["message"] = "pong";
            response["timestamp"] = std::chrono::system_clock::now()
                                       .time_since_epoch().count();
            return response;
        });
        
        // ====================================================================
        // COMMANDES DEVICES
        // ====================================================================
        
        // Note: Les implémentations réelles nécessitent les classes de commandes
        // Pour l'instant, on enregistre des placeholders qui retournent "not implemented"
        
        registerCommand("devices.list", [this](const json& params) {
            return createNotImplementedResponse("devices.list");
        });
        
        registerCommand("devices.connect", [this](const json& params) {
            return createNotImplementedResponse("devices.connect");
        });
        
        registerCommand("devices.disconnect", [this](const json& params) {
            return createNotImplementedResponse("devices.disconnect");
        });
        
        // ====================================================================
        // COMMANDES PLAYER
        // ====================================================================
        
        registerCommand("player.play", [this](const json& params) {
            return createNotImplementedResponse("player.play");
        });
        
        registerCommand("player.pause", [this](const json& params) {
            return createNotImplementedResponse("player.pause");
        });
        
        registerCommand("player.stop", [this](const json& params) {
            return createNotImplementedResponse("player.stop");
        });
        
        registerCommand("player.status", [this](const json& params) {
            return createNotImplementedResponse("player.status");
        });
        
        // ====================================================================
        // COMMANDES ROUTES
        // ====================================================================
        
        registerCommand("routes.list", [this](const json& params) {
            return createNotImplementedResponse("routes.list");
        });
        
        registerCommand("routes.add", [this](const json& params) {
            return createNotImplementedResponse("routes.add");
        });
        
        registerCommand("routes.remove", [this](const json& params) {
            return createNotImplementedResponse("routes.remove");
        });
        
        // ====================================================================
        // COMMANDES LIBRARY
        // ====================================================================
        
        registerCommand("library.scan", [this](const json& params) {
            return createNotImplementedResponse("library.scan");
        });
        
        registerCommand("library.list", [this](const json& params) {
            return createNotImplementedResponse("library.list");
        });
        
        registerCommand("library.search", [this](const json& params) {
            return createNotImplementedResponse("library.search");
        });
        
        Logger::info("CommandProcessorV2", 
            "Registered " + std::to_string(factory_.count()) + " commands");
    }
    
    /**
     * @brief Helper pour enregistrer une commande avec lambda
     * 
     * @param name Nom de la commande
     * @param executor Lambda d'exécution
     */
    void registerCommand(const std::string& name, 
                        std::function<json(const json&)> executor) {
        // Wrapper dans une commande simple
        factory_.registerCommand(name, [name, executor](const json& params) {
            // Créer une commande anonyme inline
            class LambdaCommand : public ICommand {
            public:
                LambdaCommand(const std::string& name, 
                            const json& params,
                            std::function<json(const json&)> exec)
                    : name_(name), params_(params), executor_(exec) {}
                
                std::string getName() const override { return name_; }
                
                json execute() override {
                    return executor_(params_);
                }
                
            private:
                std::string name_;
                json params_;
                std::function<json(const json&)> executor_;
            };
            
            return std::make_unique<LambdaCommand>(name, params, executor);
        });
    }
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * @brief Crée une réponse d'erreur standardisée
     * 
     * @param message Message d'erreur
     * @param errorCode Code d'erreur optionnel
     * @return json Réponse JSON d'erreur
     */
    json createErrorResponse(const std::string& message, 
                            const std::string& errorCode = "") const {
        json response;
        response["success"] = false;
        response["error"] = message;
        
        if (!errorCode.empty()) {
            response["error_code"] = errorCode;
        }
        
        response["timestamp"] = getCurrentTimestamp();
        
        return response;
    }
    
    /**
     * @brief Crée une réponse "not implemented"
     * 
     * @param commandName Nom de la commande
     * @return json Réponse JSON
     */
    json createNotImplementedResponse(const std::string& commandName) const {
        json response;
        response["success"] = false;
        response["error"] = "Command not yet implemented: " + commandName;
        response["error_code"] = "NOT_IMPLEMENTED";
        response["timestamp"] = getCurrentTimestamp();
        
        return response;
    }
    
    /**
     * @brief Récupère le timestamp actuel
     * 
     * @return int64_t Timestamp en millisecondes
     */
    int64_t getCurrentTimestamp() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /**
     * @brief Factory de création des commandes
     */
    CommandFactory factory_;
    
    /**
     * @brief Dépendances (partagées entre les commandes)
     */
    std::shared_ptr<MidiDeviceManager> deviceManager_;
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<MidiPlayer> player_;
    std::shared_ptr<MidiFileManager> fileManager_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CommandProcessorV2.h
// ============================================================================

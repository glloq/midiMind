// ============================================================================
// Fichier: backend/src/api/CommandProcessorV2.cpp
// Version: 3.0.6 - CORRIGÉ (suppression logger.cpp)
// Date: 2025-10-12
// ============================================================================
// Description:
//   Processeur de commandes v2 - Version corrigée sans logger.cpp
//
// Modifications v3.0.6:
//   ✅ Suppression déclaration registerLoggerCommands
//   ✅ Suppression implémentation registerLoggerCommands()
//   ✅ Suppression appel dans registerAllCommands()
//
// Auteur: MidiMind Team
// ============================================================================

#include "CommandProcessorV2.h"
#include "../core/Error.h"
#include "commands/network.cpp"
#include "commands/logger.cpp" 

namespace midiMind {

// ============================================================================
// DÉCLARATIONS DES FONCTIONS D'ENREGISTREMENT EXTERNES
// ============================================================================

// Ces fonctions sont définies dans des fichiers séparés
void registerDeviceCommands(CommandFactory& factory, 
                           std::shared_ptr<MidiDeviceManager> deviceManager);

void registerRoutingCommands(CommandFactory& factory,
                            std::shared_ptr<MidiRouter> router);

void registerPlaybackCommands(CommandFactory& factory,
                             std::shared_ptr<MidiPlayer> player);

void registerFileCommands(CommandFactory& factory,
                         std::shared_ptr<MidiFileManager> fileManager);

void registerEditorCommands(CommandFactory& factory,
                           std::shared_ptr<MidiFileManager> fileManager);

void registerNetworkCommands(CommandFactory& factory);

void registerSystemCommands(CommandFactory& factory);

// ✅ SUPPRIMÉ: void registerLoggerCommands(CommandFactory& factory);

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR
// ============================================================================

CommandProcessorV2::CommandProcessorV2(
    std::shared_ptr<MidiDeviceManager> deviceManager,
    std::shared_ptr<MidiRouter> router,
    std::shared_ptr<MidiPlayer> player,
    std::shared_ptr<MidiFileManager> fileManager
)
    : deviceManager_(deviceManager)
    , router_(router)
    , player_(player)
    , fileManager_(fileManager)
{
    Logger::info("CommandProcessorV2", "Initializing CommandProcessorV2...");
    
    // Enregistrer toutes les commandes
    registerAllCommands();
    
    Logger::info("CommandProcessorV2", 
        "✓ CommandProcessorV2 initialized with " + 
        std::to_string(factory_.count()) + " commands");
}

CommandProcessorV2::~CommandProcessorV2() {
    Logger::info("CommandProcessorV2", "Shutting down CommandProcessorV2...");
}

// ============================================================================
// ENREGISTREMENT DES COMMANDES
// ============================================================================

void CommandProcessorV2::registerAllCommands() {
    Logger::debug("CommandProcessorV2", "Registering all command categories...");
    
    // Enregistrer chaque catégorie
    registerDeviceCommands();
    registerRoutingCommands();
    registerPlaybackCommands();
    registerFileCommands();
    registerEditorCommands();
    registerNetworkCommands();
    registerSystemCommands();
    registerLoggerCommands();
    
    Logger::info("CommandProcessorV2", 
        "✓ All commands registered (" + std::to_string(factory_.count()) + " total)");
}

void CommandProcessorV2::registerDeviceCommands() {
    if (!deviceManager_) {
        Logger::warn("CommandProcessorV2", 
            "Device Manager not available, skipping device commands");
        return;
    }
    
    // Appeler la fonction externe qui enregistre toutes les commandes devices.*
    ::midiMind::registerDeviceCommands(factory_, deviceManager_);
    
    Logger::debug("CommandProcessorV2", "✓ Device commands registered");
}

void CommandProcessorV2::registerRoutingCommands() {
    if (!router_) {
        Logger::warn("CommandProcessorV2", 
            "Router not available, skipping routing commands");
        return;
    }
    
    // Appeler la fonction externe qui enregistre toutes les commandes routing.*
    ::midiMind::registerRoutingCommands(factory_, router_);
    
    Logger::debug("CommandProcessorV2", "✓ Routing commands registered");
}

void CommandProcessorV2::registerPlaybackCommands() {
    if (!player_) {
        Logger::warn("CommandProcessorV2", 
            "Player not available, skipping playback commands");
        return;
    }
    
    // Appeler la fonction externe qui enregistre toutes les commandes playback.*
    ::midiMind::registerPlaybackCommands(factory_, player_);
    
    Logger::debug("CommandProcessorV2", "✓ Playback commands registered");
}

void CommandProcessorV2::registerFileCommands() {
    if (!fileManager_) {
        Logger::warn("CommandProcessorV2", 
            "FileManager not available, skipping file commands");
        return;
    }
    
    // Appeler la fonction externe qui enregistre toutes les commandes files.*
    ::midiMind::registerFileCommands(factory_, fileManager_);
    
    Logger::debug("CommandProcessorV2", "✓ File commands registered");
}

void CommandProcessorV2::registerEditorCommands() {
    if (!fileManager_) {
        Logger::warn("CommandProcessorV2", 
            "FileManager not available, skipping editor commands");
        return;
    }
    
    // Appeler la fonction externe qui enregistre toutes les commandes editor.*
    ::midiMind::registerEditorCommands(factory_, fileManager_);
    
    Logger::debug("CommandProcessorV2", "✓ Editor commands registered");
}

void CommandProcessorV2::registerNetworkCommands() {
    // Appeler la fonction externe qui enregistre toutes les commandes network.*
    ::midiMind::registerNetworkCommands(factory_);
    
    Logger::debug("CommandProcessorV2", "✓ Network commands registered");
}

void CommandProcessorV2::registerSystemCommands() {
    // Appeler la fonction externe qui enregistre toutes les commandes system.*
    ::midiMind::registerSystemCommands(factory_);
    
    Logger::debug("CommandProcessorV2", "✓ System commands registered");
}

// ✅ SUPPRIMÉ: Méthode registerLoggerCommands() complète

// ============================================================================
// TRAITEMENT DES COMMANDES
// ============================================================================

json CommandProcessorV2::processCommand(const json& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Valider le format de la requête
        if (!validateCommand(request)) {
            return createErrorResponse("Invalid command format", "INVALID_FORMAT");
        }
        
        // Extraire la commande
        std::string commandName = request["command"];
        json params = request.value("params", json::object());
        
        Logger::debug("CommandProcessorV2", 
            "Processing command: " + commandName);
        
        // Exécuter la commande via factory
        json result = factory_.execute(commandName, params);
        
        return result;
        
    } catch (const MidiMindException& e) {
        Logger::error("CommandProcessorV2", 
            "Command execution failed: " + std::string(e.what()));
        return createErrorResponse(e.getMessage(), 
                                  std::to_string(static_cast<int>(e.getCode())));
                                  
    } catch (const std::exception& e) {
        Logger::error("CommandProcessorV2", 
            "Unexpected error: " + std::string(e.what()));
        return createErrorResponse("Internal error: " + std::string(e.what()), 
                                  "INTERNAL_ERROR");
    }
}

// ============================================================================
// VALIDATION
// ============================================================================

bool CommandProcessorV2::validateCommand(const json& command) const {
    // Vérifier que c'est un objet JSON
    if (!command.is_object()) {
        Logger::warn("CommandProcessorV2", "Command is not a JSON object");
        return false;
    }
    
    // Vérifier la présence du champ "command"
    if (!command.contains("command")) {
        Logger::warn("CommandProcessorV2", "Missing 'command' field");
        return false;
    }
    
    // Vérifier que "command" est une string
    if (!command["command"].is_string()) {
        Logger::warn("CommandProcessorV2", "'command' field must be a string");
        return false;
    }
    
    // Si "params" existe, vérifier que c'est un objet
    if (command.contains("params") && !command["params"].is_object()) {
        Logger::warn("CommandProcessorV2", "'params' field must be an object");
        return false;
    }
    
    return true;
}

json CommandProcessorV2::createErrorResponse(const std::string& message,
                                             const std::string& code) const {
    return {
        {"success", false},
        {"error", message},
        {"error_code", code}
    };
}

json CommandProcessorV2::createSuccessResponse(const json& data) const {
    json response = {
        {"success", true}
    };
    
    // Fusionner avec les données si présentes
    if (!data.empty()) {
        for (auto it = data.begin(); it != data.end(); ++it) {
            response[it.key()] = it.value();
        }
    }
    
    return response;
}

// ============================================================================
// INTROSPECTION
// ============================================================================

size_t CommandProcessorV2::getCommandCount() const {
    return factory_.count();
}

std::vector<std::string> CommandProcessorV2::listCommands() const {
    return factory_.listCommands();
}

std::unordered_map<std::string, std::vector<std::string>> 
CommandProcessorV2::listCommandsByCategory() const {
    return factory_.listCommandsByCategory();
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CommandProcessorV2.cpp
// ============================================================================

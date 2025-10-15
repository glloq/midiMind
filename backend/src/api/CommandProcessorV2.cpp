// ============================================================================
// Fichier: backend/src/api/CommandProcessorV2.cpp
// Version: 3.1.1 - CORRECTIONS CRITIQUES PHASE 1
// Date: 2025-10-15
// ============================================================================
// CORRECTIFS v3.1.1 (PHASE 1 - CRITIQUES):
//   ✅ 1.4 registerProcessingCommands() - Check nullptr processorManager_
//   ✅ Ajout paramètre processorManager au constructeur
//   ✅ Gestion gracieuse si ProcessorManager absent
//   ✅ Preservation TOTALE des fonctionnalités v3.1.0
//
// Description:
//   Processeur de commandes v2 - TOUTES COMMANDES ENREGISTRÉES
//   50+ commandes dans 11 catégories
//
// Responsabilités:
//   - Inclure TOUS les fichiers de commandes
//   - Initialiser CommandFactory avec toutes les commandes
//   - Router les requêtes vers les bonnes commandes
//   - Valider les entrées JSON
//   - Gérer les erreurs et exceptions
//   - Retourner réponses formatées
//
// Catégories de commandes:
//   1. devices.* - Gestion périphériques MIDI
//   2. routing.* - Routage des messages MIDI
//   3. playback.* - Lecture de fichiers MIDI
//   4. files.* - Gestion bibliothèque de fichiers
//   5. editor.* - Édition de fichiers MIDI
//   6. processing.* - Processeurs d'effets MIDI
//   7. network.* - Configuration réseau
//   8. system.* - Informations système
//   9. logger.* - Configuration du logging
//   10. loops.* - Gestion des boucles
//   11. instruments.* - Profils d'instruments
//
// Auteur: MidiMind Team
// ============================================================================

#include "CommandProcessorV2.h"
#include "../core/Error.h"

// ============================================================================
// INCLUDES DES FICHIERS DE COMMANDES - COMPLETS
// ============================================================================

// Catégories CORE (obligatoires)
#include "commands/devices.cpp"      // ✅ devices.*
#include "commands/routing.cpp"      // ✅ routing.*
#include "commands/playback.cpp"     // ✅ playback.*
#include "commands/files.cpp"        // ✅ files.*
#include "commands/system.cpp"       // ✅ system.*
#include "commands/editor.cpp"       // ✅ editor.*
#include "commands/network.cpp"      // ✅ network.*
#include "commands/processing.cpp"   // ✅ processing.*

// Catégories MONITORING/LOGGING
#include "commands/logger.cpp"       // ✅ logger.*

// Catégories OPTIONNELLES
#include "commands/loops.cpp"        // ✅ loops.*
#include "commands/instruments.cpp"  // ✅ instruments.*

namespace midiMind {

// ============================================================================
// DÉCLARATIONS DES FONCTIONS D'ENREGISTREMENT EXTERNES
// ============================================================================

// CORE Commands
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

void registerSystemCommands(CommandFactory& factory);

void registerNetworkCommands(CommandFactory& factory);

// ✅ CORRECTIF 1.4: Ajout paramètre ProcessorManager
void registerProcessingCommands(CommandFactory& factory,
                               std::shared_ptr<ProcessorManager> processorManager);

// MONITORING Commands
void registerLoggerCommands(CommandFactory& factory);

// OPTIONAL Commands
void registerLoopCommands(CommandFactory& factory);

void registerInstrumentCommands(CommandFactory& factory,
                               std::shared_ptr<MidiDeviceManager> deviceManager,
                               std::shared_ptr<SysExHandler> sysExHandler);

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR - CORRECTIF 1.4
// ============================================================================

CommandProcessorV2::CommandProcessorV2(
    std::shared_ptr<MidiDeviceManager> deviceManager,
    std::shared_ptr<MidiRouter> router,
    std::shared_ptr<MidiPlayer> player,
    std::shared_ptr<MidiFileManager> fileManager,
    std::shared_ptr<SysExHandler> sysExHandler,
    std::shared_ptr<ProcessorManager> processorManager  // ✅ NOUVEAU paramètre
)
    : deviceManager_(deviceManager)
    , router_(router)
    , player_(player)
    , fileManager_(fileManager)
    , sysExHandler_(sysExHandler)
    , processorManager_(processorManager)  // ✅ NOUVEAU membre
{
    Logger::info("CommandProcessorV2", "Initializing CommandProcessorV2...");
    
    // Enregistrer toutes les commandes
    registerAllCommands();
    
    Logger::info("CommandProcessorV2", 
        "✅ CommandProcessorV2 initialized with " + 
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
    registerDeviceCommands();      // devices.* (5+ commandes)
    registerRoutingCommands();     // routing.* (6+ commandes)
    registerPlaybackCommands();    // playback.* (11+ commandes)
    registerFileCommands();        // files.* (7+ commandes)
    registerEditorCommands();      // editor.* (7+ commandes)
    registerProcessingCommands();  // processing.* (N commandes) ✅ CORRECTIF 1.4
    registerNetworkCommands();     // network.* (6+ commandes)
    registerSystemCommands();      // system.* (6+ commandes)
    registerLoggerCommands();      // logger.* (N commandes)
    
    // CATÉGORIES OPTIONNELLES
    registerLoopCommands();        // ✅ loops.* (6 commandes)
    registerInstrumentCommands();  // ✅ instruments.* (8 commandes)
    
    Logger::info("CommandProcessorV2", 
        "✅ All commands registered (" + std::to_string(factory_.count()) + " total)");
}

// ============================================================================
// ENREGISTREMENT PAR CATÉGORIE
// ============================================================================

void CommandProcessorV2::registerDeviceCommands() {
    if (!deviceManager_) {
        Logger::warn("CommandProcessorV2", 
            "DeviceManager not available, skipping device commands");
        return;
    }
    
    ::midiMind::registerDeviceCommands(factory_, deviceManager_);
    
    Logger::debug("CommandProcessorV2", "✅ Device commands registered");
}

void CommandProcessorV2::registerRoutingCommands() {
    if (!router_) {
        Logger::warn("CommandProcessorV2", 
            "Router not available, skipping routing commands");
        return;
    }
    
    ::midiMind::registerRoutingCommands(factory_, router_);
    
    Logger::debug("CommandProcessorV2", "✅ Routing commands registered");
}

void CommandProcessorV2::registerPlaybackCommands() {
    if (!player_) {
        Logger::warn("CommandProcessorV2", 
            "Player not available, skipping playback commands");
        return;
    }
    
    ::midiMind::registerPlaybackCommands(factory_, player_);
    
    Logger::debug("CommandProcessorV2", "✅ Playback commands registered");
}

void CommandProcessorV2::registerFileCommands() {
    if (!fileManager_) {
        Logger::warn("CommandProcessorV2", 
            "FileManager not available, skipping file commands");
        return;
    }
    
    ::midiMind::registerFileCommands(factory_, fileManager_);
    
    Logger::debug("CommandProcessorV2", "✅ File commands registered");
}

void CommandProcessorV2::registerEditorCommands() {
    if (!fileManager_) {
        Logger::warn("CommandProcessorV2", 
            "FileManager not available, skipping editor commands");
        return;
    }
    
    ::midiMind::registerEditorCommands(factory_, fileManager_);
    
    Logger::debug("CommandProcessorV2", "✅ Editor commands registered");
}

// ✅ CORRECTIF 1.4: Check nullptr processorManager_
void CommandProcessorV2::registerProcessingCommands() {
    if (!processorManager_) {
        Logger::warn("CommandProcessorV2", 
            "ProcessorManager not available, skipping processing commands");
        return;
    }
    
    ::midiMind::registerProcessingCommands(factory_, processorManager_);
    
    Logger::debug("CommandProcessorV2", "✅ Processing commands registered");
}

void CommandProcessorV2::registerNetworkCommands() {
    ::midiMind::registerNetworkCommands(factory_);
    
    Logger::debug("CommandProcessorV2", "✅ Network commands registered");
}

void CommandProcessorV2::registerSystemCommands() {
    ::midiMind::registerSystemCommands(factory_);
    
    Logger::debug("CommandProcessorV2", "✅ System commands registered");
}

void CommandProcessorV2::registerLoggerCommands() {
    ::midiMind::registerLoggerCommands(factory_);
    
    Logger::debug("CommandProcessorV2", "✅ Logger commands registered");
}

void CommandProcessorV2::registerLoopCommands() {
    ::midiMind::registerLoopCommands(factory_);
    
    Logger::debug("CommandProcessorV2", "✅ Loop commands registered");
}

void CommandProcessorV2::registerInstrumentCommands() {
    if (!deviceManager_ || !sysExHandler_) {
        Logger::warn("CommandProcessorV2", 
            "DeviceManager or SysExHandler not available, skipping instrument commands");
        return;
    }
    
    ::midiMind::registerInstrumentCommands(factory_, deviceManager_, sysExHandler_);
    
    Logger::debug("CommandProcessorV2", "✅ Instrument commands registered");
}

// ============================================================================
// TRAITEMENT DES COMMANDES
// ============================================================================

json CommandProcessorV2::processCommand(const std::string& jsonString) {
    try {
        // Parser le JSON
        json request = json::parse(jsonString);
        
        // Valider format
        if (!request.contains("command")) {
            return {
                {"success", false},
                {"error", "Missing 'command' field"},
                {"error_code", "INVALID_FORMAT"}
            };
        }
        
        std::string commandName = request["command"];
        
        Logger::debug("CommandProcessorV2", "Processing command: " + commandName);
        
        // Vérifier que la commande existe
        if (!factory_.hasCommand(commandName)) {
            return {
                {"success", false},
                {"error", "Unknown command: " + commandName},
                {"error_code", "UNKNOWN_COMMAND"},
                {"available_commands", factory_.listCommands()}
            };
        }
        
        try {
            // Exécuter la commande
            auto result = factory_.execute(commandName, request);
            
            // Ajouter success si pas présent
            if (!result.contains("success")) {
                result["success"] = true;
            }
            
            Logger::debug("CommandProcessorV2", 
                "Command '" + commandName + "' executed (success: " + 
                (result["success"].get<bool>() ? "true" : "false") + ")");
            
            return result;
        }
        catch (const CommandException& e) {
            Logger::error("CommandProcessorV2", 
                "Command execution failed: " + std::string(e.what()));
            
            return {
                {"success", false},
                {"error", e.what()},
                {"error_code", e.getCode()}
            };
        }
        catch (const std::exception& e) {
            Logger::error("CommandProcessorV2", 
                "Command execution exception: " + std::string(e.what()));
            
            return {
                {"success", false},
                {"error", "Command execution failed: " + std::string(e.what())},
                {"error_code", "INTERNAL_ERROR"}
            };
        }
    }
    catch (const json::exception& e) {
        Logger::error("CommandProcessorV2", 
            "JSON parsing error: " + std::string(e.what()));
        
        return {
            {"success", false},
            {"error", "Invalid JSON: " + std::string(e.what())},
            {"error_code", "INVALID_FORMAT"}
        };
    }
    catch (const std::exception& e) {
        Logger::error("CommandProcessorV2", 
            "Unexpected error: " + std::string(e.what()));
        
        return {
            {"success", false},
            {"error", "Internal error: " + std::string(e.what())},
            {"error_code", "INTERNAL_ERROR"}
        };
    }
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
    std::unordered_map<std::string, std::vector<std::string>> result;
    
    auto allCommands = factory_.listCommands();
    
    for (const auto& cmd : allCommands) {
        // Extraire la catégorie (avant le '.')
        size_t dotPos = cmd.find('.');
        if (dotPos != std::string::npos) {
            std::string category = cmd.substr(0, dotPos);
            result[category].push_back(cmd);
        }
        else {
            result["uncategorized"].push_back(cmd);
        }
    }
    
    return result;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CommandProcessorV2.cpp v3.1.1 - CORRECTIONS PHASE 1 COMPLÈTES
// ============================================================================

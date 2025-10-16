// ============================================================================
// Fichier: backend/src/api/CommandProcessorV2.cpp
// Version: 3.1.2 - CORRECTIONS COMPLÈTES
// Date: 2025-10-16
// ============================================================================
// CORRECTIFS v3.1.2:
//   ✅ hasCommand() → exists() (ligne ~299)
//   ✅ CommandException → MidiMindException (ligne ~323)
//   ✅ Gestion correcte des error_code
//   ✅ Preservation TOTALE des fonctionnalités v3.1.1
//   ✅ Check nullptr processorManager_
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
                         std::shared_ptr<MidiFileManager> fileManager,
                         std::shared_ptr<Database> database);

void registerEditorCommands(CommandFactory& factory,
                           std::shared_ptr<MidiFileManager> fileManager);

void registerSystemCommands(CommandFactory& factory);

void registerNetworkCommands(CommandFactory& factory);

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
// CONSTRUCTEUR / DESTRUCTEUR
// ============================================================================

CommandProcessorV2::CommandProcessorV2(
    std::shared_ptr<MidiDeviceManager> deviceManager,
    std::shared_ptr<MidiRouter> router,
    std::shared_ptr<MidiPlayer> player,
    std::shared_ptr<MidiFileManager> fileManager,
    std::shared_ptr<SysExHandler> sysExHandler,
    std::shared_ptr<ProcessorManager> processorManager,
    std::shared_ptr<Database> database
)
    : deviceManager_(deviceManager)
    , router_(router)
    , player_(player)
    , fileManager_(fileManager)
    , sysExHandler_(sysExHandler)
    , processorManager_(processorManager)
    , database_(database)
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
    registerFileCommands();        // files.* (12+ commandes)
    registerEditorCommands();      // editor.* (7+ commandes)
    registerProcessingCommands();  // processing.* (N commandes)
    registerNetworkCommands();     // network.* (6+ commandes)
    registerSystemCommands();      // system.* (6+ commandes)
    registerLoggerCommands();      // logger.* (N commandes)
    
    // CATÉGORIES OPTIONNELLES
    registerLoopCommands();        // loops.* (6 commandes)
    registerInstrumentCommands();  // instruments.* (8 commandes)
    
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
    
    // ✅ CORRECTION: Passer database_ en paramètre
    ::midiMind::registerFileCommands(factory_, fileManager_, database_);
    
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
// TRAITEMENT DES COMMANDES - MÉTHODE CORRIGÉE
// ============================================================================

json CommandProcessorV2::processCommand(const std::string& jsonString) {
    try {
        Logger::debug("CommandProcessorV2", "Processing command...");
        
        // Parser le JSON
        json request = json::parse(jsonString);
        
        // Valider format
        if (!request.contains("command")) {
            Logger::error("CommandProcessorV2", "Missing 'command' field");
            return {
                {"success", false},
                {"error", "Missing 'command' field in request"},
                {"error_code", "INVALID_FORMAT"}
            };
        }
        
        std::string commandName = request["command"];
        json params = request.value("params", json::object());
        
        Logger::debug("CommandProcessorV2", "Command: " + commandName);
        
        // ✅ CORRECTION: hasCommand() → exists()
        if (!factory_.exists(commandName)) {
            Logger::error("CommandProcessorV2", 
                "Unknown command: " + commandName);
            return {
                {"success", false},
                {"error", "Unknown command: " + commandName},
                {"error_code", "UNKNOWN_COMMAND"}
            };
        }
        
        // Exécuter la commande
        try {
            auto result = factory_.execute(commandName, params);
            
            // Ajouter success si pas présent
            if (!result.contains("success")) {
                result["success"] = true;
            }
            
            Logger::debug("CommandProcessorV2", 
                "Command executed: " + commandName + 
                " (success: " + (result.value("success", false) ? "true" : "false") + ")");
            
            return result;
        }
        // ✅ CORRECTION: CommandException → MidiMindException
        catch (const MidiMindException& e) {
            Logger::error("CommandProcessorV2", 
                "Command execution failed: " + std::string(e.what()));
            
            return {
                {"success", false},
                {"error", e.what()},
                {"error_code", std::to_string(static_cast<int>(e.getCode()))}
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
// FIN DU FICHIER CommandProcessorV2.cpp v3.1.2 - COMPLET ET CORRIGÉ
// ============================================================================

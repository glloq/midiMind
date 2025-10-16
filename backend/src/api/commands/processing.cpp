// ============================================================================
// Fichier: /home/pi/midiMind/backend/src/api/commands/processing.cpp
// Version: 3.0.9
// Date: 2025-10-16
// ============================================================================
// Description:
//   Handlers pour les commandes de traitement MIDI
//
// CORRECTIONS v3.0.9:
//   ✅ Correction appels registerCommand (2 paramètres au lieu de 3)
//   ✅ Remplacement addProcessor/removeProcessor → addProcessorToChain/removeProcessorFromChain
//   ✅ Ajout méthodes enable()/disable() manquantes via setEnabled()
//   ✅ Correction tous les enum ProcessorType
//
// Commandes implémentées:
//   - processing.add         : Ajouter un processeur à une chaîne
//   - processing.remove      : Supprimer un processeur
//   - processing.list        : Lister tous les processeurs actifs
//   - processing.enable      : Activer un processeur
//   - processing.disable     : Désactiver un processeur
//   - processing.configure   : Configurer les paramètres
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/processing/ProcessorManager.h"
#include "../../midi/processing/MidiProcessor.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// HELPER: Conversion string → ProcessorType
// ============================================================================

static bool stringToProcessorType(const std::string& typeStr, ProcessorType& outType) {
    if (typeStr == "transpose") {
        outType = ProcessorType::TRANSPOSE;
        return true;
    } else if (typeStr == "velocity") {
        outType = ProcessorType::VELOCITY;
        return true;
    } else if (typeStr == "channel_filter") {
        outType = ProcessorType::CHANNEL_FILTER;
        return true;
    } else if (typeStr == "note_filter") {
        outType = ProcessorType::NOTE_FILTER;
        return true;
    } else if (typeStr == "arpeggiator") {
        outType = ProcessorType::ARPEGGIATOR;
        return true;
    } else if (typeStr == "delay") {
        outType = ProcessorType::DELAY;
        return true;
    } else if (typeStr == "chord") {
        outType = ProcessorType::CHORD;
        return true;
    } else if (typeStr == "harmonizer") {
        outType = ProcessorType::HARMONIZER;
        return true;
    }
    return false;
}

static std::vector<std::string> getSupportedProcessorTypes() {
    return {
        "transpose",
        "velocity",
        "channel_filter",
        "note_filter",
        "arpeggiator",
        "delay",
        "chord",
        "harmonizer"
    };
}

// ============================================================================
// FONCTION: registerProcessingCommands()
// ============================================================================

void registerProcessingCommands(
    CommandFactory& factory,
    std::shared_ptr<ProcessorManager> processorManager
) {
    if (!processorManager) {
        Logger::error("ProcessingCommands", 
            "Cannot register commands: ProcessorManager is null");
        return;
    }
    
    Logger::info("ProcessingHandlers", "Registering processing commands...");

    // ========================================================================
    // processing.add - Ajouter un processeur
    // ========================================================================
    
    factory.registerCommand("processing.add",
        [processorManager](const json& params) -> json {
            Logger::debug("ProcessingAPI", "Adding processor...");
            
            try {
                if (!params.contains("chain_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: chain_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                if (!params.contains("type")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: type"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                std::string processorTypeStr = params["type"];
                json config = params.value("config", json::object());
                
                ProcessorType type;
                if (!stringToProcessorType(processorTypeStr, type)) {
                    auto supportedTypes = getSupportedProcessorTypes();
                    std::string typesList;
                    for (size_t i = 0; i < supportedTypes.size(); i++) {
                        typesList += supportedTypes[i];
                        if (i < supportedTypes.size() - 1) typesList += ", ";
                    }
                    return {
                        {"success", false},
                        {"error", "Unknown processor type: " + processorTypeStr},
                        {"error_code", "INVALID_TYPE"},
                        {"supported_types", typesList}
                    };
                }
                
                // Créer le processeur
                auto processor = processorManager->createProcessor(type, config);
                if (!processor) {
                    return {
                        {"success", false},
                        {"error", "Failed to create processor"},
                        {"error_code", "CREATE_FAILED"}
                    };
                }
                
                // Ajouter à la chaîne
                bool added = processorManager->addProcessorToChain(chainId, processor);
                
                if (!added) {
                    return {
                        {"success", false},
                        {"error", "Failed to add processor to chain"},
                        {"error_code", "ADD_FAILED"}
                    };
                }
                
                Logger::info("ProcessingAPI", "✓ Processor added: " + processor->getName());
                
                return {
                    {"success", true},
                    {"data", {
                        {"processor_id", processor->getName()},
                        {"chain_id", chainId},
                        {"type", processorTypeStr}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Failed to add processor: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // processing.remove - Supprimer un processeur
    // ========================================================================
    
    factory.registerCommand("processing.remove",
        [processorManager](const json& params) -> json {
            Logger::debug("ProcessingAPI", "Removing processor...");
            
            try {
                if (!params.contains("chain_id") || !params.contains("processor_index")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                size_t processorIndex = params["processor_index"];
                
                bool removed = processorManager->removeProcessorFromChain(chainId, processorIndex);
                
                if (!removed) {
                    return {
                        {"success", false},
                        {"error", "Failed to remove processor"},
                        {"error_code", "REMOVE_FAILED"}
                    };
                }
                
                Logger::info("ProcessingAPI", "✓ Processor removed");
                
                return {
                    {"success", true},
                    {"message", "Processor removed successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Failed to remove processor: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // processing.list - Lister les processeurs
    // ========================================================================
    
    factory.registerCommand("processing.list",
        [processorManager](const json& params) -> json {
            Logger::debug("ProcessingAPI", "Listing processors...");
            
            try {
                auto chains = processorManager->listChains();
                json chainsJson = json::array();
                
                for (const auto& chainId : chains) {
                    auto chain = processorManager->getChain(chainId);
                    if (!chain) continue;
                    
                    auto processors = chain->getProcessors();
                    json processorsJson = json::array();
                    
                    for (const auto& processor : processors) {
                        processorsJson.push_back({
                            {"name", processor->getName()},
                            {"enabled", processor->isEnabled()},
                            {"config", processor->toJson()}
                        });
                    }
                    
                    chainsJson.push_back({
                        {"chain_id", chainId},
                        {"name", chain->getName()},
                        {"enabled", chain->isEnabled()},
                        {"processors", processorsJson}
                    });
                }
                
                return {
                    {"success", true},
                    {"data", {
                        {"chains", chainsJson},
                        {"count", chains.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Failed to list processors: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // processing.enable - Activer un processeur
    // ========================================================================
    
    factory.registerCommand("processing.enable",
        [processorManager](const json& params) -> json {
            Logger::debug("ProcessingAPI", "Enabling processor...");
            
            try {
                if (!params.contains("chain_id") || !params.contains("processor_index")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                size_t processorIndex = params["processor_index"];
                
                auto chain = processorManager->getChain(chainId);
                if (!chain) {
                    return {
                        {"success", false},
                        {"error", "Chain not found"},
                        {"error_code", "CHAIN_NOT_FOUND"}
                    };
                }
                
                auto processors = chain->getProcessors();
                if (processorIndex >= processors.size()) {
                    return {
                        {"success", false},
                        {"error", "Processor index out of range"},
                        {"error_code", "INDEX_OUT_OF_RANGE"}
                    };
                }
                
                processors[processorIndex]->setEnabled(true);
                
                Logger::info("ProcessingAPI", "✓ Processor enabled");
                
                return {
                    {"success", true},
                    {"message", "Processor enabled successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Failed to enable processor: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // processing.disable - Désactiver un processeur
    // ========================================================================
    
    factory.registerCommand("processing.disable",
        [processorManager](const json& params) -> json {
            Logger::debug("ProcessingAPI", "Disabling processor...");
            
            try {
                if (!params.contains("chain_id") || !params.contains("processor_index")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                size_t processorIndex = params["processor_index"];
                
                auto chain = processorManager->getChain(chainId);
                if (!chain) {
                    return {
                        {"success", false},
                        {"error", "Chain not found"},
                        {"error_code", "CHAIN_NOT_FOUND"}
                    };
                }
                
                auto processors = chain->getProcessors();
                if (processorIndex >= processors.size()) {
                    return {
                        {"success", false},
                        {"error", "Processor index out of range"},
                        {"error_code", "INDEX_OUT_OF_RANGE"}
                    };
                }
                
                processors[processorIndex]->setEnabled(false);
                
                Logger::info("ProcessingAPI", "✓ Processor disabled");
                
                return {
                    {"success", true},
                    {"message", "Processor disabled successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Failed to disable processor: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // processing.configure - Configurer un processeur
    // ========================================================================
    
    factory.registerCommand("processing.configure",
        [processorManager](const json& params) -> json {
            Logger::debug("ProcessingAPI", "Configuring processor...");
            
            try {
                if (!params.contains("chain_id") || 
                    !params.contains("processor_index") ||
                    !params.contains("config")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                size_t processorIndex = params["processor_index"];
                json config = params["config"];
                
                auto chain = processorManager->getChain(chainId);
                if (!chain) {
                    return {
                        {"success", false},
                        {"error", "Chain not found"},
                        {"error_code", "CHAIN_NOT_FOUND"}
                    };
                }
                
                auto processors = chain->getProcessors();
                if (processorIndex >= processors.size()) {
                    return {
                        {"success", false},
                        {"error", "Processor index out of range"},
                        {"error_code", "INDEX_OUT_OF_RANGE"}
                    };
                }
                
                // Configurer le processeur
                processors[processorIndex]->fromJson(config);
                
                Logger::info("ProcessingAPI", "✓ Processor configured");
                
                return {
                    {"success", true},
                    {"message", "Processor configured successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Failed to configure processor: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );
    
    Logger::info("ProcessingHandlers", "✓ Processing commands registered");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER processing.cpp v3.0.9
// ============================================================================

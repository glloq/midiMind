// ============================================================================
// Fichier: backend/src/api/commands/processing.cpp
// Version: 3.0.1-corrections
// Date: 2025-10-15
// ============================================================================
// Description:
//   Handlers pour les commandes de traitement MIDI
//
// CORRECTIONS v3.0.1:
//   ✅ Vérification complète conversion string → ProcessorType
//   ✅ Gestion "type inconnu"
//   ✅ Error codes ajoutés partout
//   ✅ Logging amélioré
//
// Commandes implémentées (6+ commandes):
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
#include "../../midi/processing/ProcessorType.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// HELPER: Conversion string → ProcessorType
// ============================================================================

/**
 * @brief Convertit un nom de type string vers ProcessorType enum
 * @param typeStr Nom du type en string
 * @param outType ProcessorType de sortie
 * @return true si conversion réussie
 */
static bool stringToProcessorType(const std::string& typeStr, ProcessorType& outType) {
    if (typeStr == "transpose") {
        outType = ProcessorType::TRANSPOSE;
        return true;
    } else if (typeStr == "filter") {
        outType = ProcessorType::FILTER;
        return true;
    } else if (typeStr == "velocity") {
        outType = ProcessorType::VELOCITY_MAP;
        return true;
    } else if (typeStr == "quantize") {
        outType = ProcessorType::QUANTIZE;
        return true;
    } else if (typeStr == "arpeggiator") {
        outType = ProcessorType::ARPEGGIATOR;
        return true;
    } else if (typeStr == "delay") {
        outType = ProcessorType::DELAY;
        return true;
    } else if (typeStr == "harmonizer") {
        outType = ProcessorType::HARMONIZER;
        return true;
    } else if (typeStr == "channelmap") {
        outType = ProcessorType::CHANNEL_MAP;
        return true;
    }
    
    // Type inconnu
    return false;
}

/**
 * @brief Liste des types de processeurs supportés
 */
static std::vector<std::string> getSupportedProcessorTypes() {
    return {
        "transpose",
        "filter",
        "velocity",
        "quantize",
        "arpeggiator",
        "delay",
        "harmonizer",
        "channelmap"
    };
}

// ============================================================================
// FONCTION: registerProcessingCommands()
// Enregistre toutes les commandes processing.*
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
                // Validation des paramètres obligatoires
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
                
                // Configuration optionnelle
                json config = params.value("config", json::object());
                
                // Convertir le type de string vers ProcessorType enum
                ProcessorType type;
                if (!stringToProcessorType(processorTypeStr, type)) {
                    auto supportedTypes = getSupportedProcessorTypes();
                    std::string typesList;
                    for (size_t i = 0; i < supportedTypes.size(); i++) {
                        typesList += supportedTypes[i];
                        if (i < supportedTypes.size() - 1) typesList += ", ";
                    }
                    
                    Logger::error("ProcessingAPI", 
                        "Unknown processor type: " + processorTypeStr);
                    
                    return {
                        {"success", false},
                        {"error", "Unknown processor type: " + processorTypeStr},
                        {"error_code", "INVALID_TYPE"},
                        {"data", {
                            {"requested_type", processorTypeStr},
                            {"supported_types", supportedTypes}
                        }}
                    };
                }
                
                // Ajouter le processeur
                bool added = processorManager->addProcessor(chainId, type, config);
                
                if (!added) {
                    Logger::error("ProcessingAPI", 
                        "Failed to add processor to chain: " + chainId);
                    return {
                        {"success", false},
                        {"error", "Failed to add processor"},
                        {"error_code", "ADD_FAILED"},
                        {"data", {
                            {"chain_id", chainId},
                            {"type", processorTypeStr}
                        }}
                    };
                }
                
                Logger::info("ProcessingAPI", 
                    "✓ Processor added: " + processorTypeStr + " to chain " + chainId);
                
                return {
                    {"success", true},
                    {"message", "Processor added successfully"},
                    {"data", {
                        {"chain_id", chainId},
                        {"type", processorTypeStr}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Error in processing.add: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
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
                // Validation des paramètres
                if (!params.contains("chain_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: chain_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                if (!params.contains("processor_index")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: processor_index"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                size_t processorIndex = params["processor_index"];
                
                // Supprimer le processeur
                bool removed = processorManager->removeProcessor(chainId, processorIndex);
                
                if (!removed) {
                    Logger::error("ProcessingAPI", 
                        "Failed to remove processor at index " + std::to_string(processorIndex));
                    return {
                        {"success", false},
                        {"error", "Failed to remove processor"},
                        {"error_code", "REMOVE_FAILED"},
                        {"data", {
                            {"chain_id", chainId},
                            {"processor_index", processorIndex}
                        }}
                    };
                }
                
                Logger::info("ProcessingAPI", 
                    "✓ Processor removed at index " + std::to_string(processorIndex) + 
                    " from chain " + chainId);
                
                return {
                    {"success", true},
                    {"message", "Processor removed successfully"},
                    {"data", {
                        {"chain_id", chainId},
                        {"processor_index", processorIndex}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Error in processing.remove: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );

    // ========================================================================
    // processing.list - Lister tous les processeurs
    // ========================================================================
    
    factory.registerCommand("processing.list",
        [processorManager](const json& params) -> json {
            Logger::debug("ProcessingAPI", "Listing processors...");
            
            try {
                // Récupérer toutes les chaînes
                auto chainIds = processorManager->listChains();
                
                json chains = json::array();
                
                for (const auto& chainId : chainIds) {
                    auto chain = processorManager->getChain(chainId);
                    
                    if (chain) {
                        json chainData;
                        chainData["id"] = chainId;
                        chainData["name"] = chain->getName();
                        chainData["enabled"] = chain->isEnabled();
                        
                        // Lister les processeurs de cette chaîne
                        auto processors = chain->getProcessors();
                        json processorsArray = json::array();
                        
                        size_t index = 0;
                        for (const auto& processor : processors) {
                            json processorData;
                            processorData["index"] = index++;
                            processorData["name"] = processor->getName();
                            processorData["type"] = processor->getType();
                            processorData["enabled"] = processor->isEnabled();
                            
                            processorsArray.push_back(processorData);
                        }
                        
                        chainData["processors"] = processorsArray;
                        chainData["processor_count"] = processors.size();
                        
                        chains.push_back(chainData);
                    }
                }
                
                Logger::debug("ProcessingAPI", 
                    "Listed " + std::to_string(chains.size()) + " processing chains");
                
                return {
                    {"success", true},
                    {"data", {
                        {"chains", chains},
                        {"chain_count", chains.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Error in processing.list: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "LIST_FAILED"}
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
                // Validation des paramètres
                if (!params.contains("chain_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: chain_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                if (!params.contains("processor_index")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: processor_index"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                size_t processorIndex = params["processor_index"];
                
                // Récupérer la chaîne
                auto chain = processorManager->getChain(chainId);
                
                if (!chain) {
                    return {
                        {"success", false},
                        {"error", "Chain not found: " + chainId},
                        {"error_code", "CHAIN_NOT_FOUND"}
                    };
                }
                
                // Récupérer les processeurs
                auto processors = chain->getProcessors();
                
                if (processorIndex >= processors.size()) {
                    return {
                        {"success", false},
                        {"error", "Processor index out of range"},
                        {"error_code", "INDEX_OUT_OF_RANGE"},
                        {"data", {
                            {"processor_index", processorIndex},
                            {"processor_count", processors.size()}
                        }}
                    };
                }
                
                // Activer le processeur
                processors[processorIndex]->enable();
                
                Logger::info("ProcessingAPI", 
                    "✓ Enabled processor at index " + std::to_string(processorIndex) + 
                    " in chain '" + chainId + "'");
                
                return {
                    {"success", true},
                    {"message", "Processor enabled"},
                    {"data", {
                        {"chain_id", chainId},
                        {"processor_index", processorIndex},
                        {"enabled", true}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Error in processing.enable: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "ENABLE_FAILED"}
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
                // Validation des paramètres
                if (!params.contains("chain_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: chain_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                if (!params.contains("processor_index")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: processor_index"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                size_t processorIndex = params["processor_index"];
                
                // Récupérer la chaîne
                auto chain = processorManager->getChain(chainId);
                
                if (!chain) {
                    return {
                        {"success", false},
                        {"error", "Chain not found: " + chainId},
                        {"error_code", "CHAIN_NOT_FOUND"}
                    };
                }
                
                // Récupérer les processeurs
                auto processors = chain->getProcessors();
                
                if (processorIndex >= processors.size()) {
                    return {
                        {"success", false},
                        {"error", "Processor index out of range"},
                        {"error_code", "INDEX_OUT_OF_RANGE"},
                        {"data", {
                            {"processor_index", processorIndex},
                            {"processor_count", processors.size()}
                        }}
                    };
                }
                
                // Désactiver le processeur
                processors[processorIndex]->disable();
                
                Logger::info("ProcessingAPI", 
                    "✓ Disabled processor at index " + std::to_string(processorIndex) + 
                    " in chain '" + chainId + "'");
                
                return {
                    {"success", true},
                    {"message", "Processor disabled"},
                    {"data", {
                        {"chain_id", chainId},
                        {"processor_index", processorIndex},
                        {"enabled", false}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Error in processing.disable: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "DISABLE_FAILED"}
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
                // Validation des paramètres
                if (!params.contains("chain_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: chain_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                if (!params.contains("processor_index")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: processor_index"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                if (!params.contains("config")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: config"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                size_t processorIndex = params["processor_index"];
                json config = params["config"];
                
                // Configurer le processeur
                bool configured = processorManager->configureProcessor(
                    chainId, processorIndex, config);
                
                if (!configured) {
                    Logger::error("ProcessingAPI", "Failed to configure processor");
                    return {
                        {"success", false},
                        {"error", "Failed to configure processor"},
                        {"error_code", "CONFIGURE_FAILED"}
                    };
                }
                
                Logger::info("ProcessingAPI", 
                    "✓ Processor configured at index " + std::to_string(processorIndex));
                
                return {
                    {"success", true},
                    {"message", "Processor configured successfully"},
                    {"data", {
                        {"chain_id", chainId},
                        {"processor_index", processorIndex}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingAPI", 
                    "Error in processing.configure: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "CONFIGURE_ERROR"}
                };
            }
        }
    );
    
    Logger::info("ProcessingHandlers", "✅ Processing commands registered (6 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER processing.cpp v3.0.1-corrections
// ============================================================================

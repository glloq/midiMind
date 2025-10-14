// ============================================================================
// Fichier: backend/src/api/commands/processing.cpp
// Version: 1.0.0
// Description: Implémentation des commandes API de traitement MIDI (processing.*)
// Date: 2025-10-14
// ============================================================================

#include "../../midi/processing/ProcessorManager.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================================================
// DÉCLARATION DES FONCTIONS D'ENREGISTREMENT
// ============================================================================

/**
 * @brief Enregistre toutes les commandes processing.* dans la CommandFactory
 * 
 * Commandes enregistrées:
 * - processing.add         : Ajouter un processeur à une chaîne
 * - processing.remove      : Supprimer un processeur d'une chaîne
 * - processing.list        : Lister tous les processeurs actifs
 * - processing.enable      : Activer un processeur
 * - processing.disable     : Désactiver un processeur
 * - processing.configure   : Configurer les paramètres d'un processeur
 * - processing.getChains   : Lister toutes les chaînes de traitement
 * - processing.createChain : Créer une nouvelle chaîne
 * - processing.deleteChain : Supprimer une chaîne
 * 
 * @param factory Factory de commandes
 * @param processorManager Manager de traitement MIDI
 */
void registerProcessingCommands(
    CommandFactory& factory,
    std::shared_ptr<ProcessorManager> processorManager
) {
    if (!processorManager) {
        Logger::error("ProcessingCommands", 
            "Cannot register commands: ProcessorManager is null");
        return;
    }

    // ========================================================================
    // processing.add - Ajouter un processeur
    // ========================================================================
    
    factory.registerCommand("processing.add",
        [processorManager](const json& params) -> json {
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
                std::string processorType = params["type"];
                
                // Configuration optionnelle
                json config = params.value("config", json::object());
                
                // Convertir le type de string vers ProcessorType enum
                ProcessorType type;
                if (processorType == "transpose") {
                    type = ProcessorType::TRANSPOSE;
                } else if (processorType == "velocity") {
                    type = ProcessorType::VELOCITY;
                } else if (processorType == "channel_filter") {
                    type = ProcessorType::CHANNEL_FILTER;
                } else if (processorType == "note_filter") {
                    type = ProcessorType::NOTE_FILTER;
                } else if (processorType == "arpeggiator") {
                    type = ProcessorType::ARPEGGIATOR;
                } else if (processorType == "delay") {
                    type = ProcessorType::DELAY;
                } else if (processorType == "chord") {
                    type = ProcessorType::CHORD;
                } else if (processorType == "harmonizer") {
                    type = ProcessorType::HARMONIZER;
                } else {
                    return {
                        {"success", false},
                        {"error", "Unknown processor type: " + processorType},
                        {"error_code", "INVALID_PROCESSOR_TYPE"}
                    };
                }
                
                // Créer le processeur
                auto processor = processorManager->createProcessor(type, config);
                
                if (!processor) {
                    return {
                        {"success", false},
                        {"error", "Failed to create processor"},
                        {"error_code", "CREATION_FAILED"}
                    };
                }
                
                // Ajouter à la chaîne
                if (!processorManager->addProcessorToChain(chainId, processor)) {
                    return {
                        {"success", false},
                        {"error", "Failed to add processor to chain"},
                        {"error_code", "ADD_FAILED"}
                    };
                }
                
                Logger::info("ProcessingCommands", 
                    "Added processor '" + processorType + "' to chain '" + chainId + "'");
                
                return {
                    {"success", true},
                    {"data", {
                        {"chain_id", chainId},
                        {"processor_type", processorType},
                        {"processor_name", processor->getName()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
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
                if (!processorManager->removeProcessorFromChain(chainId, processorIndex)) {
                    return {
                        {"success", false},
                        {"error", "Failed to remove processor from chain"},
                        {"error_code", "REMOVE_FAILED"}
                    };
                }
                
                Logger::info("ProcessingCommands", 
                    "Removed processor at index " + std::to_string(processorIndex) + 
                    " from chain '" + chainId + "'");
                
                return {
                    {"success", true},
                    {"data", {
                        {"chain_id", chainId},
                        {"processor_index", processorIndex}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
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
                
                Logger::debug("ProcessingCommands", 
                    "Listed " + std::to_string(chains.size()) + " processing chains");
                
                return {
                    {"success", true},
                    {"data", {
                        {"chains", chains},
                        {"chain_count", chains.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
                    "Error in processing.list: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );

    // ========================================================================
    // processing.enable - Activer un processeur
    // ========================================================================
    
    factory.registerCommand("processing.enable",
        [processorManager](const json& params) -> json {
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
                        {"error_code", "INDEX_OUT_OF_RANGE"}
                    };
                }
                
                // Activer le processeur
                processors[processorIndex]->enable();
                
                Logger::info("ProcessingCommands", 
                    "Enabled processor at index " + std::to_string(processorIndex) + 
                    " in chain '" + chainId + "'");
                
                return {
                    {"success", true},
                    {"data", {
                        {"chain_id", chainId},
                        {"processor_index", processorIndex},
                        {"enabled", true}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
                    "Error in processing.enable: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );

    // ========================================================================
    // processing.disable - Désactiver un processeur
    // ========================================================================
    
    factory.registerCommand("processing.disable",
        [processorManager](const json& params) -> json {
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
                        {"error_code", "INDEX_OUT_OF_RANGE"}
                    };
                }
                
                // Désactiver le processeur
                processors[processorIndex]->disable();
                
                Logger::info("ProcessingCommands", 
                    "Disabled processor at index " + std::to_string(processorIndex) + 
                    " in chain '" + chainId + "'");
                
                return {
                    {"success", true},
                    {"data", {
                        {"chain_id", chainId},
                        {"processor_index", processorIndex},
                        {"enabled", false}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
                    "Error in processing.disable: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );

    // ========================================================================
    // processing.configure - Configurer un processeur
    // ========================================================================
    
    factory.registerCommand("processing.configure",
        [processorManager](const json& params) -> json {
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
                        {"error_code", "INDEX_OUT_OF_RANGE"}
                    };
                }
                
                // Configurer le processeur
                auto processor = processors[processorIndex];
                processor->fromJson(config);
                
                Logger::info("ProcessingCommands", 
                    "Configured processor at index " + std::to_string(processorIndex) + 
                    " in chain '" + chainId + "'");
                
                return {
                    {"success", true},
                    {"data", {
                        {"chain_id", chainId},
                        {"processor_index", processorIndex},
                        {"processor_name", processor->getName()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
                    "Error in processing.configure: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );

    // ========================================================================
    // processing.getChains - Lister toutes les chaînes
    // ========================================================================
    
    factory.registerCommand("processing.getChains",
        [processorManager](const json& params) -> json {
            try {
                auto chainIds = processorManager->listChains();
                
                json chains = json::array();
                
                for (const auto& chainId : chainIds) {
                    auto chain = processorManager->getChain(chainId);
                    
                    if (chain) {
                        json chainData;
                        chainData["id"] = chainId;
                        chainData["name"] = chain->getName();
                        chainData["enabled"] = chain->isEnabled();
                        chainData["processor_count"] = chain->getProcessors().size();
                        
                        chains.push_back(chainData);
                    }
                }
                
                Logger::debug("ProcessingCommands", 
                    "Listed " + std::to_string(chains.size()) + " chains");
                
                return {
                    {"success", true},
                    {"data", {
                        {"chains", chains},
                        {"count", chains.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
                    "Error in processing.getChains: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );

    // ========================================================================
    // processing.createChain - Créer une nouvelle chaîne
    // ========================================================================
    
    factory.registerCommand("processing.createChain",
        [processorManager](const json& params) -> json {
            try {
                // Validation du paramètre name
                if (!params.contains("name")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: name"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string name = params["name"];
                
                // Créer la chaîne
                std::string chainId = processorManager->createChain(name);
                
                Logger::info("ProcessingCommands", 
                    "Created new processing chain: '" + name + "' with ID: " + chainId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"chain_id", chainId},
                        {"name", name}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
                    "Error in processing.createChain: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );

    // ========================================================================
    // processing.deleteChain - Supprimer une chaîne
    // ========================================================================
    
    factory.registerCommand("processing.deleteChain",
        [processorManager](const json& params) -> json {
            try {
                // Validation du paramètre chain_id
                if (!params.contains("chain_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: chain_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string chainId = params["chain_id"];
                
                // Supprimer la chaîne
                if (!processorManager->deleteChain(chainId)) {
                    return {
                        {"success", false},
                        {"error", "Failed to delete chain: " + chainId},
                        {"error_code", "DELETE_FAILED"}
                    };
                }
                
                Logger::info("ProcessingCommands", 
                    "Deleted processing chain with ID: " + chainId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"chain_id", chainId}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
                    "Error in processing.deleteChain: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );

    // ========================================================================
    // processing.getStats - Obtenir les statistiques de traitement
    // ========================================================================
    
    factory.registerCommand("processing.getStats",
        [processorManager](const json& params) -> json {
            try {
                json stats = processorManager->getStatistics();
                
                Logger::debug("ProcessingCommands", "Retrieved processing statistics");
                
                return {
                    {"success", true},
                    {"data", stats}
                };
                
            } catch (const std::exception& e) {
                Logger::error("ProcessingCommands", 
                    "Error in processing.getStats: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );

    Logger::info("ProcessingCommands", 
        "✅ Successfully registered 10 processing.* commands");
}

// ============================================================================
// FIN DU FICHIER
// ============================================================================
// ============================================================================
// Fichier: backend/src/api/commands/loops.cpp
// Version: 1.2.0 - CORRIGÉ
// Date: 2025-10-16
// ============================================================================
// Description:
//   Commandes WebSocket pour la gestion des loops (CRUD)
//   Enregistre toutes les commandes loops.* dans la CommandFactory
//
// Commandes implémentées (6 commandes):
//   ✅ loops.save      : Sauvegarder un loop (création ou mise à jour)
//   ✅ loops.load      : Charger un loop par ID
//   ✅ loops.list      : Lister les loops avec pagination
//   ✅ loops.delete    : Supprimer un loop
//   ✅ loops.search    : Rechercher des loops par nom
//   ✅ loops.count     : Compter le nombre total de loops
//
// Auteur: MidiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../loop/LoopManager.h"
#include "../../core/Logger.h"
#include "../../core/Error.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @brief Enregistre toutes les commandes loop dans la factory
 * @param factory Factory où enregistrer les commandes
 */
void registerLoopCommands(CommandFactory& factory) {
    
    Logger::info("LoopAPI", "Registering loop commands...");
    
    // ========================================================================
    // loops.save - Sauvegarder un loop (création ou mise à jour)
    // ========================================================================
    factory.registerCommand("loops.save",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Saving loop...");
            
            try {
                if (!params.contains("loop") || !params["loop"].is_object()) {
                    return {
                        {"success", false},
                        {"error", "Missing or invalid 'loop' parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                json loopData = params["loop"];
                
                // Sauvegarder via LoopManager
                auto& loopMgr = LoopManager::instance();
                std::string loopId = loopMgr.saveLoop(loopData);
                
                Logger::info("LoopAPI", "✓ Loop saved: " + loopId);
                
                return {
                    {"success", true},
                    {"message", "Loop saved successfully"},
                    {"data", {
                        {"loopId", loopId}
                    }}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to save loop: " + std::string(e.what()));
                // ✅ CORRECTION: getCodeName() → getCode() avec conversion
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", std::to_string(static_cast<int>(e.getCode()))}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.load - Charger un loop par ID
    // ========================================================================
    factory.registerCommand("loops.load",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Loading loop...");
            
            try {
                std::string loopId;
                if (params.contains("loopId") && params["loopId"].is_string()) {
                    loopId = params["loopId"];
                } else if (params.contains("loop_id") && params["loop_id"].is_string()) {
                    loopId = params["loop_id"];
                } else {
                    return {
                        {"success", false},
                        {"error", "Missing or invalid 'loopId' parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                // Charger via LoopManager
                auto& loopMgr = LoopManager::instance();
                auto loopOpt = loopMgr.loadLoop(loopId);
                
                if (!loopOpt) {
                    return {
                        {"success", false},
                        {"error", "Loop not found"},
                        {"error_code", "LOOP_NOT_FOUND"}
                    };
                }
                
                Logger::info("LoopAPI", "✓ Loop loaded: " + loopId);
                
                return {
                    {"success", true},
                    {"message", "Loop loaded successfully"},
                    {"data", {
                        {"loop", *loopOpt}
                    }}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to load loop: " + std::string(e.what()));
                // ✅ CORRECTION: getCodeName() → getCode() avec conversion
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", std::to_string(static_cast<int>(e.getCode()))}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.list - Lister les loops avec pagination
    // ========================================================================
    factory.registerCommand("loops.list",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Listing loops...");
            
            try {
                int limit = params.value("limit", 50);
                int offset = params.value("offset", 0);
                std::string sortBy = params.value("sortBy", "lastModified");
                std::string sortOrder = params.value("sortOrder", "desc");
                
                // Lister via LoopManager
                auto& loopMgr = LoopManager::instance();
                json loops = loopMgr.listLoops(limit, offset, sortBy, sortOrder);
                int totalCount = loopMgr.getTotalCount();
                
                Logger::info("LoopAPI", 
                    "✓ Listed " + std::to_string(loops.size()) + 
                    " loops (total: " + std::to_string(totalCount) + ")");
                
                return {
                    {"success", true},
                    {"data", {
                        {"loops", loops},
                        {"count", loops.size()},
                        {"total", totalCount},
                        {"limit", limit},
                        {"offset", offset}
                    }}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to list loops: " + std::string(e.what()));
                // ✅ CORRECTION: getCodeName() → getCode() avec conversion
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", std::to_string(static_cast<int>(e.getCode()))}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.delete - Supprimer un loop
    // ========================================================================
    factory.registerCommand("loops.delete",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Deleting loop...");
            
            try {
                std::string loopId;
                if (params.contains("loopId") && params["loopId"].is_string()) {
                    loopId = params["loopId"];
                } else if (params.contains("loop_id") && params["loop_id"].is_string()) {
                    loopId = params["loop_id"];
                } else {
                    return {
                        {"success", false},
                        {"error", "Missing or invalid 'loopId' parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                // Supprimer via LoopManager
                auto& loopMgr = LoopManager::instance();
                bool success = loopMgr.deleteLoop(loopId);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Loop not found or could not be deleted"},
                        {"error_code", "DELETE_FAILED"}
                    };
                }
                
                Logger::info("LoopAPI", "✓ Loop deleted: " + loopId);
                
                return {
                    {"success", true},
                    {"message", "Loop deleted successfully"}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to delete loop: " + std::string(e.what()));
                // ✅ CORRECTION: getCodeName() → getCode() avec conversion
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", std::to_string(static_cast<int>(e.getCode()))}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.search - Rechercher des loops par nom
    // ========================================================================
    factory.registerCommand("loops.search",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Searching loops...");
            
            try {
                if (!params.contains("query") || !params["query"].is_string()) {
                    return {
                        {"success", false},
                        {"error", "Missing or invalid 'query' parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string query = params["query"];
                int limit = params.value("limit", 50);
                
                // Rechercher via LoopManager
                auto& loopMgr = LoopManager::instance();
                json results = loopMgr.searchLoops(query, limit);
                
                Logger::info("LoopAPI", 
                    "✓ Search complete: " + std::to_string(results.size()) + " results");
                
                return {
                    {"success", true},
                    {"data", {
                        {"results", results},
                        {"count", results.size()},
                        {"query", query}
                    }}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to search loops: " + std::string(e.what()));
                // ✅ CORRECTION: getCodeName() → getCode() avec conversion
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", std::to_string(static_cast<int>(e.getCode()))}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // loops.count - Compter le nombre total de loops
    // ========================================================================
    factory.registerCommand("loops.count",
        [](const json& params) -> json {
            Logger::debug("LoopAPI", "Counting loops...");
            
            try {
                auto& loopMgr = LoopManager::instance();
                int totalCount = loopMgr.getTotalCount();
                
                Logger::info("LoopAPI", "✓ Total loops: " + std::to_string(totalCount));
                
                return {
                    {"success", true},
                    {"data", {
                        {"total", totalCount}
                    }}
                };
                
            } catch (const MidiMindException& e) {
                Logger::error("LoopAPI", 
                    "Failed to count loops: " + std::string(e.what()));
                // ✅ CORRECTION: getCodeName() → getCode() avec conversion
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", std::to_string(static_cast<int>(e.getCode()))}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoopAPI", 
                    "Unexpected error: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Internal error: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    Logger::info("LoopAPI", "✅ Loop commands registered (6 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER loops.cpp v1.2.0-CORRIGÉ
// ============================================================================

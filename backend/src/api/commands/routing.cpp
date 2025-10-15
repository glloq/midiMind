// ============================================================================
// Fichier: backend/src/api/commands/routing.cpp
// Version: 3.0.1-corrections
// Date: 2025-10-15
// ============================================================================
// Description:
//   Handlers pour les commandes de routage MIDI
//   VERSION LAMBDA DIRECTE (json -> json)
//
// CORRECTIONS v3.0.1:
//   ✅ Ajout error_code pour toutes les erreurs
//   ✅ Format de retour harmonisé avec enveloppe "data"
//   ✅ Validation des paramètres renforcée
//   ✅ Logging amélioré
//
// Commandes implémentées (6 commandes):
//   - routing.add       : Ajouter une route canal → device
//   - routing.remove    : Supprimer une route
//   - routing.list      : Lister toutes les routes
//   - routing.clear     : Effacer toutes les routes
//   - routing.enable    : Activer une route
//   - routing.disable   : Désactiver une route
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/MidiRouter.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FONCTION: registerRoutingCommands()
// Enregistre toutes les commandes de routage (6 commandes)
// ============================================================================

void registerRoutingCommands(CommandFactory& factory,
                            std::shared_ptr<MidiRouter> router) {
    
    Logger::info("RoutingHandlers", "Registering routing commands...");
    
    // ========================================================================
    // routing.add - Ajouter une route
    // ========================================================================
    
    factory.registerCommand("routing.add",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Adding route...");
            
            try {
                // Validation
                if (!params.contains("channel")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: channel"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                int channel = params["channel"];
                std::string deviceId = params["device_id"];
                
                // Validation canal
                if (channel < 0 || channel > 15) {
                    return {
                        {"success", false},
                        {"error", "Invalid channel: must be 0-15"},
                        {"error_code", "INVALID_PARAMETER"}
                    };
                }
                
                // Ajouter la route
                bool added = router->addRoute(channel, deviceId);
                
                if (!added) {
                    Logger::error("RoutingAPI", "Failed to add route");
                    return {
                        {"success", false},
                        {"error", "Failed to add route"},
                        {"error_code", "ROUTE_ADD_FAILED"}
                    };
                }
                
                Logger::info("RoutingAPI", 
                    "✓ Route added: channel " + std::to_string(channel) + 
                    " → " + deviceId);
                
                return {
                    {"success", true},
                    {"message", "Route added successfully"},
                    {"data", {
                        {"channel", channel},
                        {"device_id", deviceId}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to add route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to add route: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // routing.remove - Supprimer une route
    // ========================================================================
    
    factory.registerCommand("routing.remove",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Removing route...");
            
            try {
                // Validation
                if (!params.contains("channel")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: channel"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                int channel = params["channel"];
                
                // Validation canal
                if (channel < 0 || channel > 15) {
                    return {
                        {"success", false},
                        {"error", "Invalid channel: must be 0-15"},
                        {"error_code", "INVALID_PARAMETER"}
                    };
                }
                
                // Supprimer la route
                bool removed = router->removeRoute(channel);
                
                if (!removed) {
                    Logger::warn("RoutingAPI", 
                        "No route found for channel " + std::to_string(channel));
                    return {
                        {"success", false},
                        {"error", "No route found for this channel"},
                        {"error_code", "ROUTE_NOT_FOUND"},
                        {"data", {
                            {"channel", channel}
                        }}
                    };
                }
                
                Logger::info("RoutingAPI", 
                    "✓ Route removed: channel " + std::to_string(channel));
                
                return {
                    {"success", true},
                    {"message", "Route removed successfully"},
                    {"data", {
                        {"channel", channel}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to remove route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to remove route: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // routing.list - Lister toutes les routes
    // ========================================================================
    
    factory.registerCommand("routing.list",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Listing routes...");
            
            try {
                auto routes = router->getRoutes();
                
                json routesJson = json::array();
                for (const auto& route : routes) {
                    routesJson.push_back({
                        {"channel", route.channel},
                        {"device_id", route.deviceId},
                        {"enabled", route.enabled},
                        {"name", route.name}
                    });
                }
                
                Logger::debug("RoutingAPI", 
                    "Listed " + std::to_string(routes.size()) + " route(s)");
                
                return {
                    {"success", true},
                    {"data", {
                        {"routes", routesJson},
                        {"count", routes.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to list routes: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to list routes: " + std::string(e.what())},
                    {"error_code", "LIST_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // routing.clear - Effacer toutes les routes
    // ========================================================================
    
    factory.registerCommand("routing.clear",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Clearing all routes...");
            
            try {
                router->clearRoutes();
                
                Logger::info("RoutingAPI", "✓ All routes cleared");
                
                return {
                    {"success", true},
                    {"message", "All routes cleared successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to clear routes: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to clear routes: " + std::string(e.what())},
                    {"error_code", "CLEAR_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // routing.enable - Activer une route
    // ========================================================================
    
    factory.registerCommand("routing.enable",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Enabling route...");
            
            try {
                // Validation
                if (!params.contains("channel")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: channel"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                int channel = params["channel"];
                
                // Validation canal
                if (channel < 0 || channel > 15) {
                    return {
                        {"success", false},
                        {"error", "Invalid channel: must be 0-15"},
                        {"error_code", "INVALID_PARAMETER"}
                    };
                }
                
                // Activer la route
                bool enabled = router->setRouteEnabled(channel, true);
                
                if (!enabled) {
                    Logger::warn("RoutingAPI", 
                        "No route found for channel " + std::to_string(channel));
                    return {
                        {"success", false},
                        {"error", "No route found for this channel"},
                        {"error_code", "ROUTE_NOT_FOUND"},
                        {"data", {
                            {"channel", channel}
                        }}
                    };
                }
                
                Logger::info("RoutingAPI", 
                    "✓ Route enabled: channel " + std::to_string(channel));
                
                return {
                    {"success", true},
                    {"message", "Route enabled successfully"},
                    {"data", {
                        {"channel", channel},
                        {"enabled", true}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to enable route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to enable route: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    // ========================================================================
    // routing.disable - Désactiver une route
    // ========================================================================
    
    factory.registerCommand("routing.disable",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Disabling route...");
            
            try {
                // Validation
                if (!params.contains("channel")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: channel"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                int channel = params["channel"];
                
                // Validation canal
                if (channel < 0 || channel > 15) {
                    return {
                        {"success", false},
                        {"error", "Invalid channel: must be 0-15"},
                        {"error_code", "INVALID_PARAMETER"}
                    };
                }
                
                // Désactiver la route
                bool disabled = router->setRouteEnabled(channel, false);
                
                if (!disabled) {
                    Logger::warn("RoutingAPI", 
                        "No route found for channel " + std::to_string(channel));
                    return {
                        {"success", false},
                        {"error", "No route found for this channel"},
                        {"error_code", "ROUTE_NOT_FOUND"},
                        {"data", {
                            {"channel", channel}
                        }}
                    };
                }
                
                Logger::info("RoutingAPI", 
                    "✓ Route disabled: channel " + std::to_string(channel));
                
                return {
                    {"success", true},
                    {"message", "Route disabled successfully"},
                    {"data", {
                        {"channel", channel},
                        {"enabled", false}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to disable route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to disable route: " + std::string(e.what())},
                    {"error_code", "INTERNAL_ERROR"}
                };
            }
        }
    );
    
    Logger::info("RoutingHandlers", "✅ Routing commands registered (6 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER routing.cpp v3.0.1-corrections
// ============================================================================

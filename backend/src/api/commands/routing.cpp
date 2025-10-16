// ============================================================================
// Fichier: /home/pi/midiMind/backend/src/api/commands/routing.cpp
// Version: 3.0.5
// Date: 2025-10-16
// ============================================================================
// Description:
//   Handlers pour les commandes de routage MIDI
//
// CORRECTIONS v3.0.5:
//   ✅ Correction appels registerCommand (2 paramètres)
//
// Commandes implémentées:
//   - routing.addRoute     : Ajouter une route
//   - routing.removeRoute  : Supprimer une route
//   - routing.listRoutes   : Lister toutes les routes
//   - routing.enableRoute  : Activer une route
//   - routing.disableRoute : Désactiver une route
//   - routing.updateRoute  : Modifier une route
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/routing/MidiRouter.h"
#include "../../core/Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FONCTION: registerRoutingCommands()
// ============================================================================

void registerRoutingCommands(
    CommandFactory& factory,
    std::shared_ptr<MidiRouter> router
) {
    if (!router) {
        Logger::error("RoutingCommands", 
            "Cannot register commands: MidiRouter is null");
        return;
    }
    
    Logger::info("RoutingHandlers", "Registering routing commands...");

    // ========================================================================
    // routing.addRoute - Ajouter une route
    // ========================================================================
    
    factory.registerCommand("routing.addRoute",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Adding route...");
            
            try {
                if (!params.contains("source") || !params.contains("destination")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameters: source, destination"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string source = params["source"];
                std::string destination = params["destination"];
                bool enabled = params.value("enabled", true);
                
                auto routeId = router->addRoute(source, destination, enabled);
                
                if (routeId.empty()) {
                    return {
                        {"success", false},
                        {"error", "Failed to add route"},
                        {"error_code", "ADD_FAILED"}
                    };
                }
                
                Logger::info("RoutingAPI", "✓ Route added: " + routeId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"route_id", routeId},
                        {"source", source},
                        {"destination", destination},
                        {"enabled", enabled}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to add route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // routing.removeRoute - Supprimer une route
    // ========================================================================
    
    factory.registerCommand("routing.removeRoute",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Removing route...");
            
            try {
                if (!params.contains("route_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: route_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string routeId = params["route_id"];
                
                bool success = router->removeRoute(routeId);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to remove route"},
                        {"error_code", "REMOVE_FAILED"}
                    };
                }
                
                Logger::info("RoutingAPI", "✓ Route removed: " + routeId);
                
                return {
                    {"success", true},
                    {"message", "Route removed successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to remove route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // routing.listRoutes - Lister toutes les routes
    // ========================================================================
    
    factory.registerCommand("routing.listRoutes",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Listing routes...");
            
            try {
                auto routes = router->listRoutes();
                json routesJson = json::array();
                
                for (const auto& route : routes) {
                    routesJson.push_back({
                        {"route_id", route.id},
                        {"source", route.source},
                        {"destination", route.destination},
                        {"enabled", route.enabled}
                    });
                }
                
                Logger::info("RoutingAPI", 
                    "Found " + std::to_string(routes.size()) + " routes");
                
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
                    {"error", e.what()},
                    {"error_code", "LIST_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // routing.enableRoute - Activer une route
    // ========================================================================
    
    factory.registerCommand("routing.enableRoute",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Enabling route...");
            
            try {
                if (!params.contains("route_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: route_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string routeId = params["route_id"];
                
                bool success = router->setRouteEnabled(routeId, true);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to enable route"},
                        {"error_code", "ENABLE_FAILED"}
                    };
                }
                
                Logger::info("RoutingAPI", "✓ Route enabled: " + routeId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"route_id", routeId},
                        {"enabled", true}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to enable route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // routing.disableRoute - Désactiver une route
    // ========================================================================
    
    factory.registerCommand("routing.disableRoute",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Disabling route...");
            
            try {
                if (!params.contains("route_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: route_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string routeId = params["route_id"];
                
                bool success = router->setRouteEnabled(routeId, false);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to disable route"},
                        {"error_code", "DISABLE_FAILED"}
                    };
                }
                
                Logger::info("RoutingAPI", "✓ Route disabled: " + routeId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"route_id", routeId},
                        {"enabled", false}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to disable route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // routing.updateRoute - Modifier une route
    // ========================================================================
    
    factory.registerCommand("routing.updateRoute",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Updating route...");
            
            try {
                if (!params.contains("route_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: route_id"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string routeId = params["route_id"];
                
                // Récupérer la route existante
                auto route = router->getRoute(routeId);
                if (!route) {
                    return {
                        {"success", false},
                        {"error", "Route not found"},
                        {"error_code", "ROUTE_NOT_FOUND"}
                    };
                }
                
                // Mettre à jour les paramètres
                if (params.contains("source")) {
                    route->source = params["source"];
                }
                if (params.contains("destination")) {
                    route->destination = params["destination"];
                }
                if (params.contains("enabled")) {
                    route->enabled = params["enabled"];
                }
                
                bool success = router->updateRoute(routeId, *route);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to update route"},
                        {"error_code", "UPDATE_FAILED"}
                    };
                }
                
                Logger::info("RoutingAPI", "✓ Route updated: " + routeId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"route_id", routeId},
                        {"source", route->source},
                        {"destination", route->destination},
                        {"enabled", route->enabled}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to update route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "EXCEPTION"}
                };
            }
        }
    );

    // ========================================================================
    // routing.getStats - Statistiques de routage
    // ========================================================================
    
    factory.registerCommand("routing.getStats",
        [router](const json& params) -> json {
            Logger::debug("RoutingAPI", "Getting routing statistics...");
            
            try {
                auto stats = router->getStats();
                
                return {
                    {"success", true},
                    {"data", stats}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to get stats: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "STATS_FAILED"}
                };
            }
        }
    );
    
    Logger::info("RoutingHandlers", "✓ Routing commands registered");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER routing.cpp v3.0.5
// ============================================================================

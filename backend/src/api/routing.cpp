// ============================================================================
// Fichier: backend/src/api/routing.cpp
// Version: 3.0.0
// ============================================================================
// Description:
//   Handlers pour les commandes de routage MIDI
//   VERSION LAMBDA DIRECTE (json -> json)
//
// Commandes implémentées:
//   - routing.add       : Ajouter une route canal → device
//   - routing.remove    : Supprimer une route
//   - routing.list      : Lister toutes les routes
//   - routing.clear     : Effacer toutes les routes
//   - routing.enable    : Activer une route
//   - routing.disable   : Désactiver une route
//
// Auteur: midiMind Team
// Date: 2025-10-09
// ============================================================================

#include "../core/commands/CommandFactory.h"
#include "../midi/MidiRouter.h"
#include "../core/Logger.h"

namespace midiMind {

// ============================================================================
// FONCTION: registerRoutingCommands()
// Enregistre toutes les commandes de routage
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
                        {"error", "Missing required parameter: channel"}
                    };
                }
                
                if (!params.contains("device_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: device_id"}
                    };
                }
                
                int channel = params["channel"];
                std::string deviceId = params["device_id"];
                
                // Validation canal
                if (channel < 0 || channel > 15) {
                    return {
                        {"success", false},
                        {"error", "Invalid channel: must be 0-15"}
                    };
                }
                
                // Ajouter la route
                bool added = router->addRoute(channel, deviceId);
                
                if (added) {
                    Logger::info("RoutingAPI", 
                        "Route added: channel " + std::to_string(channel) + 
                        " → " + deviceId);
                    
                    return {
                        {"success", true},
                        {"message", "Route added successfully"},
                        {"channel", channel},
                        {"device_id", deviceId}
                    };
                } else {
                    return {
                        {"success", false},
                        {"error", "Failed to add route"}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to add route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to add route: " + std::string(e.what())}
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
                        {"error", "Missing required parameter: channel"}
                    };
                }
                
                int channel = params["channel"];
                
                // Validation canal
                if (channel < 0 || channel > 15) {
                    return {
                        {"success", false},
                        {"error", "Invalid channel: must be 0-15"}
                    };
                }
                
                // Supprimer la route
                bool removed = router->removeRoute(channel);
                
                if (removed) {
                    Logger::info("RoutingAPI", 
                        "Route removed: channel " + std::to_string(channel));
                    
                    return {
                        {"success", true},
                        {"message", "Route removed successfully"},
                        {"channel", channel}
                    };
                } else {
                    return {
                        {"success", false},
                        {"error", "No route found for this channel"}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to remove route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to remove route: " + std::string(e.what())}
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
                        {"enabled", route.enabled}
                    });
                }
                
                return {
                    {"success", true},
                    {"count", routes.size()},
                    {"routes", routesJson}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to list routes: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to list routes: " + std::string(e.what())}
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
                
                Logger::info("RoutingAPI", "All routes cleared");
                
                return {
                    {"success", true},
                    {"message", "All routes cleared successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to clear routes: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to clear routes: " + std::string(e.what())}
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
                        {"error", "Missing required parameter: channel"}
                    };
                }
                
                int channel = params["channel"];
                
                // Validation canal
                if (channel < 0 || channel > 15) {
                    return {
                        {"success", false},
                        {"error", "Invalid channel: must be 0-15"}
                    };
                }
                
                // Activer la route
                bool enabled = router->setRouteEnabled(channel, true);
                
                if (enabled) {
                    Logger::info("RoutingAPI", 
                        "Route enabled: channel " + std::to_string(channel));
                    
                    return {
                        {"success", true},
                        {"message", "Route enabled successfully"},
                        {"channel", channel}
                    };
                } else {
                    return {
                        {"success", false},
                        {"error", "Failed to enable route"}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to enable route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to enable route: " + std::string(e.what())}
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
                        {"error", "Missing required parameter: channel"}
                    };
                }
                
                int channel = params["channel"];
                
                // Validation canal
                if (channel < 0 || channel > 15) {
                    return {
                        {"success", false},
                        {"error", "Invalid channel: must be 0-15"}
                    };
                }
                
                // Désactiver la route
                bool disabled = router->setRouteEnabled(channel, false);
                
                if (disabled) {
                    Logger::info("RoutingAPI", 
                        "Route disabled: channel " + std::to_string(channel));
                    
                    return {
                        {"success", true},
                        {"message", "Route disabled successfully"},
                        {"channel", channel}
                    };
                } else {
                    return {
                        {"success", false},
                        {"error", "Failed to disable route"}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("RoutingAPI", 
                    "Failed to disable route: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to disable route: " + std::string(e.what())}
                };
            }
        }
    );
    
    Logger::info("RoutingHandlers", 
        "✓ Routing commands registered (6 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER routing.cpp
// ============================================================================

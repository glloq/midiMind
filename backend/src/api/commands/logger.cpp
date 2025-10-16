// ============================================================================
// Fichier: /home/pi/midiMind/backend/src/api/commands/logger.cpp
// Version: 3.0.7
// Date: 2025-10-16
// ============================================================================
// Description:
//   Handlers pour les commandes de configuration du logger
//
// CORRECTIONS v3.0.7:
//   ✅ Correction appels registerCommand (2 paramètres)
//   ✅ Utilisation des bonnes propriétés LoggerConfig
//   ✅ Suppression des méthodes Logger statiques inexistantes
//   ✅ Simplification avec Config::instance()
//
// Commandes implémentées:
//   - logger.getConfig    : Récupérer la configuration
//   - logger.setLevel     : Définir le niveau de log
//   - logger.setFile      : Configurer le logging fichier
//   - logger.enableFileLogging  : Activer le fichier
//   - logger.disableFileLogging : Désactiver le fichier
//   - logger.getStats     : Statistiques du logger
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../core/Logger.h"
#include "../../core/Config.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FONCTION: registerLoggerCommands()
// ============================================================================

void registerLoggerCommands(CommandFactory& factory) {
    
    Logger::info("LoggerHandlers", "Registering logger commands...");
    
    // ========================================================================
    // logger.getConfig - Récupérer configuration logger
    // ========================================================================
    factory.registerCommand("logger.getConfig",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Getting logger configuration...");
            
            try {
                const auto& loggerConfig = Config::instance().logger;
                
                return {
                    {"success", true},
                    {"data", {
                        {"level", loggerConfig.level},
                        {"console", {
                            {"enabled", loggerConfig.enableConsole}
                        }},
                        {"fileLogging", {
                            {"enabled", loggerConfig.enableFile},
                            {"path", loggerConfig.outputFile},
                            {"maxSizeBytes", loggerConfig.maxFileSize},
                            {"maxBackups", loggerConfig.maxBackups}
                        }}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoggerAPI", "Error getting logger config: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get logger configuration"},
                    {"details", e.what()}
                };
            }
        }
    );
    
    // ========================================================================
    // logger.setLevel - Définir le niveau de log
    // ========================================================================
    factory.registerCommand("logger.setLevel",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Setting logger level...");
            
            try {
                if (!params.contains("level")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: level"}
                    };
                }
                
                std::string level = params["level"];
                
                // Valider le niveau
                if (level != "debug" && level != "info" && level != "warning" && level != "error") {
                    return {
                        {"success", false},
                        {"error", "Invalid log level. Must be: debug, info, warning, or error"}
                    };
                }
                
                // Mettre à jour Config
                Config::instance().logger.level = level;
                Config::instance().save();
                
                Logger::info("LoggerAPI", "Log level changed to: " + level);
                
                return {
                    {"success", true},
                    {"data", {
                        {"level", level},
                        {"message", "Log level updated successfully"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoggerAPI", "Error setting logger level: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to set logger level"},
                    {"details", e.what()}
                };
            }
        }
    );
    
    // ========================================================================
    // logger.setFile - Activer/désactiver logging fichier
    // ========================================================================
    factory.registerCommand("logger.setFile",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Configuring file logging...");
            
            try {
                if (!params.contains("enabled")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: enabled"}
                    };
                }
                
                bool enabled = params["enabled"];
                std::string path = params.value("path", Config::instance().logger.outputFile);
                
                // Mettre à jour config
                Config::instance().logger.enableFile = enabled;
                Config::instance().logger.outputFile = path;
                Config::instance().save();
                
                // Activer/désactiver via Logger
                if (enabled) {
                    Logger::enableFileLogging(path, 
                        Config::instance().logger.maxFileSize,
                        Config::instance().logger.maxBackups);
                } else {
                    Logger::disableFileLogging();
                }
                
                Logger::info("LoggerAPI", 
                    std::string("File logging ") + (enabled ? "enabled" : "disabled"));
                
                return {
                    {"success", true},
                    {"data", {
                        {"enabled", enabled},
                        {"path", path},
                        {"message", "File logging configuration updated"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoggerAPI", "Error configuring file logging: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to configure file logging"},
                    {"details", e.what()}
                };
            }
        }
    );
    
    // ========================================================================
    // logger.enableFileLogging - Activer logging fichier (détaillé)
    // ========================================================================
    factory.registerCommand("logger.enableFileLogging",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Enabling file logging with options...");
            
            try {
                // Paramètres avec valeurs par défaut
                std::string path = params.value("path", "/var/log/midimind/midimind.log");
                int maxSizeMB = params.value("maxSizeMB", 10);
                int maxBackups = params.value("maxBackups", 5);
                
                size_t maxSizeBytes = maxSizeMB * 1024 * 1024;
                
                // Activer
                Logger::enableFileLogging(path, maxSizeBytes, maxBackups);
                
                // Mettre à jour Config et sauvegarder
                auto& config = Config::instance().logger;
                config.enableFile = true;
                config.outputFile = path;
                config.maxFileSize = maxSizeBytes;
                config.maxBackups = maxBackups;
                Config::instance().save();
                
                Logger::info("LoggerAPI", "File logging enabled: " + path);
                
                return {
                    {"success", true},
                    {"data", {
                        {"enabled", true},
                        {"path", path},
                        {"maxSizeMB", maxSizeMB},
                        {"maxBackups", maxBackups},
                        {"message", "File logging enabled successfully"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoggerAPI", "Error enabling file logging: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to enable file logging"},
                    {"details", e.what()}
                };
            }
        }
    );
    
    // ========================================================================
    // logger.disableFileLogging - Désactiver logging fichier
    // ========================================================================
    factory.registerCommand("logger.disableFileLogging",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Disabling file logging...");
            
            try {
                // Désactiver
                Logger::disableFileLogging();
                
                // Mettre à jour Config
                Config::instance().logger.enableFile = false;
                Config::instance().save();
                
                Logger::info("LoggerAPI", "File logging disabled");
                
                return {
                    {"success", true},
                    {"data", {
                        {"enabled", false},
                        {"message", "File logging disabled successfully"}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoggerAPI", "Error disabling file logging: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to disable file logging"},
                    {"details", e.what()}
                };
            }
        }
    );
    
    // ========================================================================
    // logger.getStats - Statistiques du logger
    // ========================================================================
    factory.registerCommand("logger.getStats",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Getting logger statistics...");
            
            try {
                const auto& loggerConfig = Config::instance().logger;
                
                json statsData = {
                    {"currentLevel", loggerConfig.level},
                    {"consoleEnabled", loggerConfig.enableConsole},
                    {"fileLoggingEnabled", loggerConfig.enableFile},
                    {"filePath", loggerConfig.outputFile},
                    {"maxFileSize", loggerConfig.maxFileSize},
                    {"maxBackups", loggerConfig.maxBackups}
                };
                
                return {
                    {"success", true},
                    {"data", statsData}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoggerAPI", "Error getting logger stats: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get logger statistics"},
                    {"details", e.what()}
                };
            }
        }
    );
    
    Logger::info("LoggerHandlers", "✓ Logger commands registered");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER logger.cpp v3.0.7
// ============================================================================

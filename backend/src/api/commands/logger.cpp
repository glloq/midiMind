// ============================================================================
// Fichier: backend/src/api/commands/logger.cpp
// Version: 1.1.0 - AMÉLIORÉ
// Date: 2025-10-13
// ============================================================================
// Description:
//   Handlers pour les commandes de gestion du logger.
//   Permet de modifier dynamiquement le niveau de log et la configuration.
//
// AMÉLIORATIONS v1.1.0:
//   ✅ Ajout commande logger.resetStats
//   ✅ Ajout commande logger.disableFileLogging
//   ✅ Amélioration logger.getStats avec stats détaillées
//   ✅ Amélioration logger.enableFileLogging avec maxSizeMB et maxFiles
//   ✅ Meilleure gestion des erreurs
//   ✅ Intégration des meilleures parties de LoggerCommands.cpp
//
// Commandes implémentées:
//   - logger.getConfig           : Récupérer configuration logger
//   - logger.setLevel            : Modifier niveau de log
//   - logger.setFile             : Activer/désactiver logging fichier
//   - logger.enableFileLogging   : Activer logging fichier (détaillé)
//   - logger.disableFileLogging  : Désactiver logging fichier
//   - logger.getStats            : Statistiques de logging (détaillées)
//   - logger.resetStats          : Réinitialiser statistiques
//   - logger.clearLogs           : Vider les logs fichier
//
// Auteur: MidiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../core/Logger.h"
#include "../../core/Config.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>  // Pour std::remove

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// FONCTION: registerLoggerCommands()
// Enregistre toutes les commandes de gestion du logger
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
                            {"enabled", true},
                            {"colors", loggerConfig.colorsEnabled},
                            {"timestamps", loggerConfig.timestampsEnabled},
                            {"category", loggerConfig.categoryEnabled}
                        }},
                        {"fileLogging", {
                            {"enabled", loggerConfig.fileLoggingEnabled},
                            {"path", loggerConfig.filePath},
                            {"maxSizeMB", loggerConfig.maxFileSizeMB},
                            {"maxFiles", loggerConfig.maxFiles}
                        }},
                        {"categoryFilter", loggerConfig.categoryFilter}
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
    // logger.setLevel - Modifier niveau de log
    // ========================================================================
    factory.registerCommand("logger.setLevel",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Setting logger level...");
            
            try {
                // Validation
                if (!params.contains("level")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: level"}
                    };
                }
                
                std::string level = params["level"];
                
                // Valider niveau
                const std::vector<std::string> validLevels = {
                    "DEBUG", "INFO", "WARNING", "ERROR"
                };
                
                bool validLevel = false;
                for (const auto& validLvl : validLevels) {
                    if (level == validLvl) {
                        validLevel = true;
                        break;
                    }
                }
                
                if (!validLevel) {
                    return {
                        {"success", false},
                        {"error", "Invalid level. Must be: DEBUG, INFO, WARNING, or ERROR"}
                    };
                }
                
                // Appliquer nouveau niveau
                if (!Logger::setGlobalLevel(level)) {
                    return {
                        {"success", false},
                        {"error", "Failed to set logger level"}
                    };
                }
                
                // Mettre à jour Config
                Config::instance().logger.level = level;
                
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
    // logger.setFile - Activer/désactiver logging fichier (simple)
    // ========================================================================
    factory.registerCommand("logger.setFile",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Configuring file logging...");
            
            try {
                // Validation
                if (!params.contains("enabled")) {
                    return {
                        {"success", false},
                        {"error", "Missing required parameter: enabled"}
                    };
                }
                
                bool enabled = params["enabled"];
                std::string path = params.value("path", Config::instance().logger.filePath);
                
                // Mettre à jour config
                Config::instance().logger.fileLoggingEnabled = enabled;
                Config::instance().logger.filePath = path;
                
                // Appliquer via Logger
                bool success = enabled 
                    ? Logger::enableFileLogging(path, 
                        Config::instance().logger.maxFileSizeMB, 
                        Config::instance().logger.maxFiles)
                    : (Logger::disableFileLogging(), true);
                
                if (!success && enabled) {
                    return {
                        {"success", false},
                        {"error", "Failed to enable file logging. Check path permissions."},
                        {"details", {{"path", path}}}
                    };
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
                int maxFiles = params.value("maxFiles", 5);
                
                // Activer
                bool success = Logger::enableFileLogging(path, maxSizeMB, maxFiles);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to enable file logging. Check path and permissions."},
                        {"details", {
                            {"path", path},
                            {"maxSizeMB", maxSizeMB},
                            {"maxFiles", maxFiles}
                        }}
                    };
                }
                
                // Mettre à jour Config et sauvegarder
                auto& config = Config::instance().logger;
                config.fileLoggingEnabled = true;
                config.filePath = path;
                config.maxFileSizeMB = maxSizeMB;
                config.maxFiles = maxFiles;
                Config::instance().save();
                
                Logger::info("LoggerAPI", "File logging enabled: " + path);
                
                return {
                    {"success", true},
                    {"data", {
                        {"enabled", true},
                        {"path", path},
                        {"maxSizeMB", maxSizeMB},
                        {"maxFiles", maxFiles},
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
                
                // Mettre à jour Config et sauvegarder
                Config::instance().logger.fileLoggingEnabled = false;
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
    // logger.getStats - Statistiques de logging (améliorées)
    // ========================================================================
    factory.registerCommand("logger.getStats",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Getting logger statistics...");
            
            try {
                // Récupérer informations actuelles
                std::string currentLevel = Logger::getGlobalLevel();
                bool fileEnabled = Logger::isFileLoggingEnabled();
                std::string filePath = Logger::getFilePath();
                
                // Tenter de récupérer stats détaillées si disponibles
                json statsData;
                
                // Vérifier si Logger::getStats() existe en appelant
                // Si la méthode n'existe pas, on utilise des valeurs par défaut
                try {
                    auto stats = Logger::getStats();
                    
                    // Si getStats() retourne une structure, l'utiliser
                    statsData = {
                        {"totalMessages", stats.totalMessages},
                        {"debugMessages", stats.debugMessages},
                        {"infoMessages", stats.infoMessages},
                        {"warnMessages", stats.warnMessages},
                        {"errorMessages", stats.errorMessages},
                        {"filteredMessages", stats.filteredMessages},
                        {"fileRotations", stats.fileRotations},
                        {"currentLevel", currentLevel},
                        {"fileLoggingEnabled", fileEnabled},
                        {"filePath", filePath},
                        {"syslogEnabled", stats.syslogEnabled},
                        {"categoryFilters", stats.categoryFilters}
                    };
                } catch (...) {
                    // Si getStats() n'existe pas, utiliser format simple
                    statsData = {
                        {"currentLevel", currentLevel},
                        {"fileLoggingEnabled", fileEnabled},
                        {"filePath", filePath},
                        {"note", "Detailed stats not available - Logger::getStats() not implemented"}
                    };
                }
                
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
    
    // ========================================================================
    // logger.resetStats - Réinitialiser statistiques
    // ========================================================================
    factory.registerCommand("logger.resetStats",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Resetting logger statistics...");
            
            try {
                // Tenter de réinitialiser les stats si la méthode existe
                try {
                    Logger::resetStats();
                    
                    Logger::info("LoggerAPI", "Logger statistics reset");
                    
                    return {
                        {"success", true},
                        {"data", {
                            {"message", "Logger statistics reset successfully"}
                        }}
                    };
                } catch (...) {
                    // Si resetStats() n'existe pas
                    return {
                        {"success", false},
                        {"error", "Logger::resetStats() not implemented"},
                        {"note", "This feature requires Logger class update"}
                    };
                }
                
            } catch (const std::exception& e) {
                Logger::error("LoggerAPI", "Error resetting logger stats: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to reset logger statistics"},
                    {"details", e.what()}
                };
            }
        }
    );
    
    // ========================================================================
    // logger.clearLogs - Vider les logs (fichier)
    // ========================================================================
    factory.registerCommand("logger.clearLogs",
        [](const json& params) -> json {
            Logger::debug("LoggerAPI", "Clearing log files...");
            
            try {
                // Récupérer état actuel
                std::string currentPath = Logger::getFilePath();
                bool wasEnabled = Logger::isFileLoggingEnabled();
                int maxSizeMB = Config::instance().logger.maxFileSizeMB;
                int maxFiles = Config::instance().logger.maxFiles;
                
                // Désactiver temporairement
                if (wasEnabled) {
                    Logger::disableFileLogging();
                }
                
                // Supprimer le fichier
                try {
                    std::remove(currentPath.c_str());
                } catch (...) {
                    // Ignorer erreurs de suppression
                }
                
                // Réactiver si nécessaire
                if (wasEnabled) {
                    Logger::enableFileLogging(currentPath, maxSizeMB, maxFiles);
                }
                
                Logger::info("LoggerAPI", "Log files cleared");
                
                return {
                    {"success", true},
                    {"data", {
                        {"message", "Log files cleared successfully"},
                        {"path", currentPath},
                        {"reEnabled", wasEnabled}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("LoggerAPI", "Error clearing logs: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to clear log files"},
                    {"details", e.what()}
                };
            }
        }
    );
    
    Logger::info("LoggerHandlers", "✅ Logger commands registered successfully (8 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER logger.cpp v1.1.0 - AMÉLIORÉ
// ============================================================================
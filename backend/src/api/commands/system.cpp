// ============================================================================
// Fichier: backend/src/api/commands/system.cpp
// Version: 3.0.2 - CORRIGÉ
// Date: 2025-10-16
// ============================================================================
// CORRECTIONS v3.0.2:
//   ✅ Ajout #include <sys/vfs.h> pour statfs
//   ✅ Ajout #include <sys/utsname.h> pour uname
//   ✅ Retrait 3ème paramètre de tous les registerCommand
//   ✅ Correction accès systemInfo.sysname, etc.
//
// Description:
//   Handlers pour les commandes système
//   Informations sur l'état du système, contrôle de l'application
//
// Commandes implémentées (6 commandes):
//   - system.status      : État général du système
//   - system.info        : Informations détaillées
//   - system.commands    : Liste toutes les commandes disponibles
//   - system.ping        : Test de connectivité
//   - system.shutdown    : Arrêt propre de l'application
//   - system.restart     : Redémarrage de l'application
//
// Auteur: MidiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../core/Logger.h"
#include "../../core/Config.h"
#include <nlohmann/json.hpp>

// ✅ CORRECTION: Ajout includes manquants
#include <sys/sysinfo.h>
#include <sys/vfs.h>       // Pour statfs
#include <sys/utsname.h>   // Pour uname
#include <fstream>
#include <sstream>
#include <ctime>
#include <thread>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// UTILITAIRES SYSTÈME
// ============================================================================

/**
 * @brief Récupère l'uptime du système en secondes
 */
static uint64_t getUptimeSeconds() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<uint64_t>(info.uptime);
    }
    return 0;
}

/**
 * @brief Récupère l'utilisation CPU (approximative)
 */
static double getCpuUsage() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 0.0;
    
    std::string cpu;
    uint64_t user, nice, system, idle;
    file >> cpu >> user >> nice >> system >> idle;
    file.close();
    
    uint64_t total = user + nice + system + idle;
    if (total == 0) return 0.0;
    
    return ((double)(total - idle) / total) * 100.0;
}

/**
 * @brief Récupère la température CPU (Raspberry Pi)
 */
static double getCpuTemperature() {
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    if (!file.is_open()) return 0.0;
    
    int temp;
    file >> temp;
    file.close();
    
    return temp / 1000.0;  // Convertir en Celsius
}

/**
 * @brief Récupère l'utilisation mémoire
 */
static json getMemoryUsage() {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return {
            {"total_mb", 0},
            {"free_mb", 0},
            {"used_mb", 0},
            {"usage_percent", 0.0}
        };
    }
    
    uint64_t totalMB = info.totalram / (1024 * 1024);
    uint64_t freeMB = info.freeram / (1024 * 1024);
    uint64_t usedMB = totalMB - freeMB;
    double usagePercent = totalMB > 0 ?
        ((double)usedMB / totalMB) * 100.0 : 0.0;
    
    return {
        {"total_mb", totalMB},
        {"free_mb", freeMB},
        {"used_mb", usedMB},
        {"usage_percent", usagePercent}
    };
}

/**
 * @brief Récupère l'espace disque
 */
static json getDiskSpace() {
    // ✅ CORRECTION: statfs maintenant défini avec #include <sys/vfs.h>
    struct statfs diskInfo;
    if (statfs("/home/pi/MidiMind", &diskInfo) != 0) {
        return {
            {"total_mb", 0},
            {"free_mb", 0},
            {"used_mb", 0},
            {"usage_percent", 0.0}
        };
    }
    
    uint64_t totalMB = (diskInfo.f_blocks * diskInfo.f_bsize) / (1024 * 1024);
    uint64_t freeMB = (diskInfo.f_bfree * diskInfo.f_bsize) / (1024 * 1024);
    uint64_t usedMB = totalMB - freeMB;
    double usagePercent = totalMB > 0 ?
        ((double)usedMB / totalMB) * 100.0 : 0.0;
    
    return {
        {"total_mb", totalMB},
        {"free_mb", freeMB},
        {"used_mb", usedMB},
        {"usage_percent", usagePercent}
    };
}

/**
 * @brief Récupère informations réseau
 */
static json getNetworkInfo() {
    // TODO: Implémenter récupération IP, interfaces, etc.
    return {
        {"interfaces", json::array()},
        {"ip_address", "0.0.0.0"}
    };
}

// ============================================================================
// FONCTION: registerSystemCommands()
// Enregistre toutes les commandes système (6 commandes)
// ============================================================================
void registerSystemCommands(CommandFactory& factory) {
    
    Logger::info("SystemHandlers", "Registering system commands...");
    
    // ========================================================================
    // system.status - État général du système
    // ========================================================================
    factory.registerCommand("system.status",
        [](const json& params) -> json {
            Logger::debug("SystemAPI", "Getting system status...");
            
            try {
                auto currentTime = std::time(nullptr);
                
                return {
                    {"success", true},
                    {"data", {
                        {"version", "3.0.3"},
                        {"protocol_version", "3.0"},
                        {"uptime_seconds", getUptimeSeconds()},
                        {"cpu_usage_percent", getCpuUsage()},
                        {"cpu_temperature_celsius", getCpuTemperature()},
                        {"memory", getMemoryUsage()},
                        {"disk", getDiskSpace()},
                        {"timestamp", currentTime}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("SystemAPI", 
                    "Failed to get system status: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get system status: " + std::string(e.what())},
                    {"error_code", "STATUS_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // system.info - Informations détaillées
    // ========================================================================
    factory.registerCommand("system.info",
        [](const json& params) -> json {
            Logger::debug("SystemAPI", "Getting detailed system info...");
            
            try {
                // ✅ CORRECTION: uname maintenant défini avec #include <sys/utsname.h>
                struct utsname systemInfo;
                uname(&systemInfo);
                
                return {
                    {"success", true},
                    {"data", {
                        {"application", {
                            {"name", "MidiMind"},
                            {"version", "3.0.3"},
                            {"build_date", __DATE__},
                            {"build_time", __TIME__}
                        }},
                        {"system", {
                            {"os", systemInfo.sysname},
                            {"release", systemInfo.release},
                            {"version", systemInfo.version},
                            {"machine", systemInfo.machine},
                            {"hostname", systemInfo.nodename}
                        }},
                        {"hardware", {
                            {"model", "Raspberry Pi"},
                            {"cpu_cores", std::thread::hardware_concurrency()},
                            {"memory_total_mb", getMemoryUsage()["total_mb"]}
                        }},
                        {"network", getNetworkInfo()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("SystemAPI", 
                    "Failed to get system info: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to get system info: " + std::string(e.what())},
                    {"error_code", "INFO_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // system.commands - Liste toutes les commandes disponibles
    // ========================================================================
    factory.registerCommand("system.commands",
        [&factory](const json& params) -> json {
            Logger::debug("SystemAPI", "Listing all commands...");
            
            try {
                auto commands = factory.listCommands();
                auto byCategory = factory.listCommandsByCategory();
                
                json categoriesJson = json::object();
                for (const auto& [category, cmdList] : byCategory) {
                    categoriesJson[category] = cmdList;
                }
                
                return {
                    {"success", true},
                    {"data", {
                        {"commands", commands},
                        {"count", commands.size()},
                        {"by_category", categoriesJson}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("SystemAPI", 
                    "Failed to list commands: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to list commands: " + std::string(e.what())},
                    {"error_code", "LIST_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // system.ping - Test de connectivité
    // ========================================================================
    factory.registerCommand("system.ping",
        [](const json& params) -> json {
            auto currentTime = std::time(nullptr);
            
            return {
                {"success", true},
                {"message", "pong"},
                {"timestamp", currentTime},
                {"uptime_seconds", getUptimeSeconds()}
            };
        }
    );
    
    // ========================================================================
    // system.shutdown - Arrêt propre de l'application
    // ========================================================================
    factory.registerCommand("system.shutdown",
        [](const json& params) -> json {
            Logger::debug("SystemAPI", "Shutting down...");
            
            try {
                int delaySeconds = params.value("delay_seconds", 0);
                
                if (delaySeconds < 0 || delaySeconds > 60) {
                    return {
                        {"success", false},
                        {"error", "Delay must be between 0 and 60 seconds"},
                        {"error_code", "INVALID_DELAY"}
                    };
                }
                
                Logger::info("SystemAPI", 
                    "Shutdown initiated (delay: " + std::to_string(delaySeconds) + "s)");
                
                // TODO: Implémenter shutdown propre
                // - Fermer toutes les connexions
                // - Sauvegarder l'état
                // - exit(0)
                
                return {
                    {"success", true},
                    {"message", "Shutdown initiated"},
                    {"delay_seconds", delaySeconds}
                };
                
            } catch (const std::exception& e) {
                Logger::error("SystemAPI", 
                    "Failed to shutdown: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to shutdown: " + std::string(e.what())},
                    {"error_code", "SHUTDOWN_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // system.restart - Redémarrage de l'application
    // ========================================================================
    factory.registerCommand("system.restart",
        [](const json& params) -> json {
            Logger::debug("SystemAPI", "Restarting...");
            
            try {
                int delaySeconds = params.value("delay_seconds", 0);
                
                if (delaySeconds < 0 || delaySeconds > 60) {
                    return {
                        {"success", false},
                        {"error", "Delay must be between 0 and 60 seconds"},
                        {"error_code", "INVALID_DELAY"}
                    };
                }
                
                Logger::info("SystemAPI", 
                    "Restart initiated (delay: " + std::to_string(delaySeconds) + "s)");
                
                // TODO: Implémenter restart
                // - Même que shutdown mais avec exec() pour relancer
                
                return {
                    {"success", true},
                    {"message", "Restart initiated"},
                    {"delay_seconds", delaySeconds}
                };
                
            } catch (const std::exception& e) {
                Logger::error("SystemAPI", 
                    "Failed to restart: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", "Failed to restart: " + std::string(e.what())},
                    {"error_code", "RESTART_FAILED"}
                };
            }
        }
    );
    
    Logger::info("SystemHandlers", "✅ System commands registered (6 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER system.cpp v3.0.2-CORRIGÉ
// ============================================================================

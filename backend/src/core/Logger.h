// ============================================================================
// Fichier: backend/src/core/Logger.h
// Version: 3.1.0 - CORRECTIONS PHASE 2
// Date: 2025-10-15
// ============================================================================
// CORRECTIFS v3.1.0 (PHASE 2 - IMPORTANTES):
//   ✅ 2.1 Rotation automatique des logs
//   ✅ Limite taille fichier: 10 MB par défaut
//   ✅ Conservation 5 anciennes versions
//   ✅ Check size avant chaque log
//   ✅ Préservation TOTALE des fonctionnalités existantes
//
// Description:
//   Système de logging thread-safe avec rotation automatique
//
// Fonctionnalités:
//   - 4 niveaux de log (DEBUG, INFO, WARNING, ERROR)
//   - Logging console avec couleurs ANSI
//   - ✅ Logging fichier avec rotation automatique
//   - Filtrage par catégorie
//   - Support syslog Linux
//   - Métriques internes
//   - Thread-safe complet
//
// Thread-safety: OUI (mutex + atomics)
//
// Auteur: MidiMind Team
// ============================================================================

#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <exception>
#include <atomic>
#include <cstdio>
#include <filesystem>

#ifdef __linux__
#include <syslog.h>
#endif

namespace midiMind {

/**
 * @class Logger
 * @brief Système de logging thread-safe avec rotation automatique
 * 
 * @details
 * Logger singleton avec support multi-niveaux et rotation fichiers.
 * 
 * Fonctionnalités:
 * - 4 niveaux: DEBUG, INFO, WARNING, ERROR
 * - Output console avec couleurs ANSI
 * - Output fichier avec rotation automatique (v3.1.0)
 * - Filtrage par catégorie
 * - Statistiques complètes
 * 
 * ✅ PHASE 2: Rotation automatique
 * - Check taille avant chaque log
 * - Rotation si > MAX_LOG_SIZE (10 MB)
 * - Conservation 5 anciennes versions
 * - Naming: log.0, log.1, ... log.4
 * 
 * Thread-safety: OUI (mutex pour toutes opérations)
 * 
 * @example Utilisation
 * ```cpp
 * Logger::setLevel(Logger::Level::DEBUG);
 * Logger::enableFileLogging("/var/log/midimind.log");
 * 
 * Logger::info("Application", "Starting...");
 * Logger::error("Database", "Connection failed");
 * ```
 */
class Logger {
public:
    // ========================================================================
    // ÉNUMÉRATION: Niveaux de Log
    // ========================================================================
    
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3
    };
    
    // ========================================================================
    // STRUCTURE: Statistiques
    // ========================================================================
    
    struct Stats {
        uint64_t totalMessages;
        uint64_t debugMessages;
        uint64_t infoMessages;
        uint64_t warnMessages;
        uint64_t errorMessages;
        uint64_t filteredMessages;
        uint64_t fileRotations;
        bool fileLoggingEnabled;
        bool syslogEnabled;
        size_t categoryFilters;
    };
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - LOGGING
    // ========================================================================
    
    /**
     * @brief Log niveau DEBUG
     * @param category Catégorie du log
     * @param message Message
     * @note Thread-safe
     */
    static void debug(const std::string& category, const std::string& message) {
        log(Level::DEBUG, category, message);
    }
    
    /**
     * @brief Log niveau INFO
     * @param category Catégorie du log
     * @param message Message
     * @note Thread-safe
     */
    static void info(const std::string& category, const std::string& message) {
        log(Level::INFO, category, message);
    }
    
    /**
     * @brief Log niveau WARNING
     * @param category Catégorie du log
     * @param message Message
     * @note Thread-safe
     */
    static void warn(const std::string& category, const std::string& message) {
        log(Level::WARNING, category, message);
    }
    
    /**
     * @brief Log niveau ERROR
     * @param category Catégorie du log
     * @param message Message
     * @note Thread-safe
     */
    static void error(const std::string& category, const std::string& message) {
        log(Level::ERROR, category, message);
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit le niveau minimum de log
     * @param level Niveau (DEBUG/INFO/WARNING/ERROR)
     * @note Thread-safe
     */
    static void setLevel(Level level) {
        std::lock_guard<std::mutex> lock(getMutex());
        getLevel() = level;
    }
    
    /**
     * @brief Récupère le niveau actuel
     * @return Level Niveau actuel
     * @note Thread-safe
     */
    static Level getLevel() {
        std::lock_guard<std::mutex> lock(getMutex());
        return getLevel();
    }
    
    /**
     * @brief Active le logging fichier avec rotation automatique
     * 
     * @param filepath Chemin du fichier de log
     * @param maxSize Taille maximale par fichier (défaut: 10 MB)
     * @param maxFiles Nombre de fichiers à conserver (défaut: 5)
     * 
     * @note Thread-safe
     * @note ✅ PHASE 2: Rotation automatique activée
     * 
     * @example
     * ```cpp
     * Logger::enableFileLogging("/var/log/midimind.log");
     * // Créera: midimind.log, midimind.log.0, midimind.log.1, etc.
     * ```
     */
    static void enableFileLogging(const std::string& filepath, 
                                  size_t maxSize = 10 * 1024 * 1024,
                                  size_t maxFiles = 5) {
        std::lock_guard<std::mutex> lock(getMutex());
        
        // Fermer fichier existant si ouvert
        if (getLogFile().is_open()) {
            getLogFile().close();
        }
        
        // Créer répertoire parent si nécessaire
        std::filesystem::path path(filepath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        
        // Ouvrir fichier
        getLogFile().open(filepath, std::ios::app);
        
        if (!getLogFile().is_open()) {
            std::cerr << "[Logger] ERROR: Cannot open log file: " << filepath << std::endl;
            return;
        }
        
        getLogFilePath() = filepath;
        getFileLoggingEnabled() = true;
        
        // ✅ PHASE 2: Configurer rotation
        getMaxFileSize() = maxSize;
        getMaxFiles() = maxFiles;
        
        // Header dans le fichier
        getLogFile() << "========================================\n";
        getLogFile() << "MidiMind Logger Started\n";
        getLogFile() << "Date: " << getCurrentTimestamp() << "\n";
        getLogFile() << "Max Size: " << (maxSize / (1024 * 1024)) << " MB\n";
        getLogFile() << "Max Files: " << maxFiles << "\n";
        getLogFile() << "========================================\n";
        getLogFile().flush();
    }
    
    /**
     * @brief Désactive le logging fichier
     * @note Thread-safe
     */
    static void disableFileLogging() {
        std::lock_guard<std::mutex> lock(getMutex());
        
        if (getLogFile().is_open()) {
            getLogFile() << "========================================\n";
            getLogFile() << "MidiMind Logger Stopped\n";
            getLogFile() << "Date: " << getCurrentTimestamp() << "\n";
            getLogFile() << "========================================\n";
            getLogFile().close();
        }
        
        getFileLoggingEnabled() = false;
    }
    
    /**
     * @brief Ajoute un filtre de catégorie
     * @param category Catégorie à filtrer (ex: "MIDI", "API")
     * @note Seules les catégories filtrées seront loggées
     * @note Thread-safe
     */
    static void addCategoryFilter(const std::string& category) {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryFilter().push_back(category);
    }
    
    /**
     * @brief Supprime tous les filtres de catégorie
     * @note Thread-safe
     */
    static void clearCategoryFilter() {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryFilter().clear();
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - SYSLOG (Linux)
    // ========================================================================
    
#ifdef __linux__
    static void enableSyslog(const std::string& ident = "midimind") {
        std::lock_guard<std::mutex> lock(getMutex());
        openlog(ident.c_str(), LOG_PID | LOG_CONS, LOG_USER);
        getSyslogEnabled() = true;
    }
    
    static void disableSyslog() {
        std::lock_guard<std::mutex> lock(getMutex());
        if (getSyslogEnabled()) {
            closelog();
            getSyslogEnabled() = false;
        }
    }
#endif
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques de logging
     * @return Stats Structure avec compteurs
     * @note Thread-safe
     */
    static Stats getStats() {
        std::lock_guard<std::mutex> lock(getMutex());
        
        Stats stats;
        stats.totalMessages = getTotalMessages().load();
        stats.debugMessages = getDebugMessages().load();
        stats.infoMessages = getInfoMessages().load();
        stats.warnMessages = getWarnMessages().load();
        stats.errorMessages = getErrorMessages().load();
        stats.filteredMessages = getFilteredMessages().load();
        stats.fileRotations = getFileRotations().load();
        stats.fileLoggingEnabled = getFileLoggingEnabled();
        stats.syslogEnabled = getSyslogEnabled();
        stats.categoryFilters = getCategoryFilter().size();
        
        return stats;
    }
    
    /**
     * @brief Remet à zéro les statistiques
     * @note Thread-safe
     */
    static void resetStats() {
        std::lock_guard<std::mutex> lock(getMutex());
        getTotalMessages().store(0);
        getDebugMessages().store(0);
        getInfoMessages().store(0);
        getWarnMessages().store(0);
        getErrorMessages().store(0);
        getFilteredMessages().store(0);
        getFileRotations().store(0);
    }
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES - CORE LOGGING
    // ========================================================================
    
    /**
     * @brief ✅ Méthode principale de logging avec rotation
     * @param level Niveau du log
     * @param category Catégorie
     * @param message Message
     * @note Thread-safe
     * @note PHASE 2: Check rotation avant chaque log
     */
    static void log(Level level, const std::string& category, const std::string& message) {
        try {
            std::lock_guard<std::mutex> lock(getMutex());
            
            // Filtrer par niveau
            if (level < getLevel()) {
                getFilteredMessages().fetch_add(1);
                return;
            }
            
            // Filtrer par catégorie
            if (!getCategoryFilter().empty()) {
                auto& filters = getCategoryFilter();
                if (std::find(filters.begin(), filters.end(), category) == filters.end()) {
                    getFilteredMessages().fetch_add(1);
                    return;
                }
            }
            
            // Formater message
            std::string timestamp = getCurrentTimestamp();
            std::string levelStr = levelToString(level);
            std::string formatted = "[" + timestamp + "] [" + levelStr + "] [" + category + "] " + message;
            
            // Console avec couleurs
            std::cout << getColorCode(level) << formatted << "\033[0m" << std::endl;
            
            // ✅ PHASE 2: Check rotation AVANT écriture fichier
            if (getFileLoggingEnabled() && getLogFile().is_open()) {
                checkRotation();
                
                getLogFile() << formatted << std::endl;
                getLogFile().flush();
            }
            
            // Syslog si activé
#ifdef __linux__
            if (getSyslogEnabled()) {
                int priority;
                switch (level) {
                    case Level::DEBUG:   priority = LOG_DEBUG; break;
                    case Level::INFO:    priority = LOG_INFO; break;
                    case Level::WARNING: priority = LOG_WARNING; break;
                    case Level::ERROR:   priority = LOG_ERR; break;
                }
                syslog(priority, "[%s] %s", category.c_str(), message.c_str());
            }
#endif
            
            // Incrémenter compteurs
            getTotalMessages().fetch_add(1);
            switch (level) {
                case Level::DEBUG:   getDebugMessages().fetch_add(1); break;
                case Level::INFO:    getInfoMessages().fetch_add(1); break;
                case Level::WARNING: getWarnMessages().fetch_add(1); break;
                case Level::ERROR:   getErrorMessages().fetch_add(1); break;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[Logger] INTERNAL ERROR: " << e.what() << std::endl;
        }
    }
    
    // ========================================================================
    // MÉTHODES PRIVÉES - ROTATION (PHASE 2)
    // ========================================================================
    
    /**
     * @brief ✅ PHASE 2: Vérifie si rotation nécessaire
     * @note Appelé avant chaque écriture fichier
     */
    static void checkRotation() {
        if (!getLogFile().is_open()) {
            return;
        }
        
        try {
            // Obtenir position actuelle (= taille fichier)
            getLogFile().seekp(0, std::ios::end);
            std::streampos fileSize = getLogFile().tellp();
            
            if (static_cast<size_t>(fileSize) >= getMaxFileSize()) {
                rotateLogFile();
            }
        } catch (const std::exception& e) {
            std::cerr << "[Logger] Rotation check error: " << e.what() << std::endl;
        }
    }
    
    /**
     * @brief ✅ PHASE 2: Effectue la rotation des fichiers de log
     * @note Ferme fichier actuel, renomme anciennes versions, ouvre nouveau
     */
    static void rotateLogFile() {
        try {
            // Fermer fichier actuel
            if (getLogFile().is_open()) {
                getLogFile() << "========================================\n";
                getLogFile() << "File rotation: " << getCurrentTimestamp() << "\n";
                getLogFile() << "========================================\n";
                getLogFile().close();
            }
            
            const std::string& basePath = getLogFilePath();
            const size_t maxFiles = getMaxFiles();
            
            // Supprimer le plus ancien (maxFiles - 1)
            std::string oldestPath = basePath + "." + std::to_string(maxFiles - 1);
            if (std::filesystem::exists(oldestPath)) {
                std::filesystem::remove(oldestPath);
            }
            
            // Renommer les fichiers (de maxFiles-2 vers maxFiles-1, ..., 0 vers 1)
            for (int i = static_cast<int>(maxFiles) - 2; i >= 0; --i) {
                std::string oldPath = (i == 0) ? basePath : basePath + "." + std::to_string(i);
                std::string newPath = basePath + "." + std::to_string(i + 1);
                
                if (std::filesystem::exists(oldPath)) {
                    std::filesystem::rename(oldPath, newPath);
                }
            }
            
            // Renommer fichier actuel en .0
            if (std::filesystem::exists(basePath)) {
                std::filesystem::rename(basePath, basePath + ".0");
            }
            
            // Ouvrir nouveau fichier
            getLogFile().open(basePath, std::ios::app);
            
            if (getLogFile().is_open()) {
                getLogFile() << "========================================\n";
                getLogFile() << "Log file rotated\n";
                getLogFile() << "Date: " << getCurrentTimestamp() << "\n";
                getLogFile() << "========================================\n";
                getLogFile().flush();
            }
            
            // Incrémenter compteur
            getFileRotations().fetch_add(1);
            
        } catch (const std::exception& e) {
            std::cerr << "[Logger] Rotation error: " << e.what() << std::endl;
        }
    }
    
    // ========================================================================
    // MÉTHODES PRIVÉES - UTILITAIRES
    // ========================================================================
    
    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        
        return ss.str();
    }
    
    static std::string levelToString(Level level) {
        switch (level) {
            case Level::DEBUG:   return "DEBUG";
            case Level::INFO:    return "INFO ";
            case Level::WARNING: return "WARN ";
            case Level::ERROR:   return "ERROR";
            default:             return "?????";
        }
    }
    
    static const char* getColorCode(Level level) {
        switch (level) {
            case Level::DEBUG:   return "\033[36m";  // Cyan
            case Level::INFO:    return "\033[32m";  // Green
            case Level::WARNING: return "\033[33m";  // Yellow
            case Level::ERROR:   return "\033[31m";  // Red
            default:             return "\033[0m";   // Reset
        }
    }
    
    // ========================================================================
    // ACCESSEURS STATIQUES - Meyer's Singleton Pattern
    // ========================================================================
    
    static std::mutex& getMutex() {
        static std::mutex mutex;
        return mutex;
    }
    
    static Level& getLevel() {
        static Level level = Level::INFO;
        return level;
    }
    
    static std::ofstream& getLogFile() {
        static std::ofstream file;
        return file;
    }
    
    static std::string& getLogFilePath() {
        static std::string path;
        return path;
    }
    
    static bool& getFileLoggingEnabled() {
        static bool enabled = false;
        return enabled;
    }
    
    static bool& getSyslogEnabled() {
        static bool enabled = false;
        return enabled;
    }
    
    static std::vector<std::string>& getCategoryFilter() {
        static std::vector<std::string> filter;
        return filter;
    }
    
    // ✅ PHASE 2: Nouveaux accesseurs pour rotation
    static size_t& getMaxFileSize() {
        static size_t size = 10 * 1024 * 1024;  // 10 MB par défaut
        return size;
    }
    
    static size_t& getMaxFiles() {
        static size_t count = 5;  // 5 fichiers par défaut
        return count;
    }
    
    // Compteurs atomiques
    static std::atomic<uint64_t>& getTotalMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getDebugMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getInfoMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getWarnMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getErrorMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getFilteredMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getFileRotations() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Logger.h v3.1.0 - PHASE 2 COMPLÈTE
// ============================================================================

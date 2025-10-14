// ============================================================================
// Fichier: backend/src/core/Logger.h
// Version: v3.1.1 - CORRIGÉ (sans duplications)
// Date: 2025-10-13
// ============================================================================
// Description:
//   Système de logging thread-safe complet avec file logging.
//   Version corrigée sans duplications de code.
//
// Fonctionnalités:
//   ✅ 4 niveaux de log (DEBUG, INFO, WARNING, ERROR)
//   ✅ Logging console avec couleurs ANSI
//   ✅ Logging fichier avec rotation automatique
//   ✅ Filtrage par catégorie
//   ✅ Support syslog Linux
//   ✅ Métriques internes
//   ✅ Thread-safe complet
//
// CORRECTIONS v3.1.1:
//   ✅ Suppression duplication enableFileLogging()
//   ✅ Suppression duplication accesseurs Meyer's Singleton
//   ✅ Organisation claire des méthodes publiques/privées
//   ✅ Méthode log() bien présente dans section privée
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

#ifdef __linux__
#include <syslog.h>
#endif

namespace midiMind {

// ============================================================================
// CLASSE: Logger v3.1.1
// ============================================================================

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
    
    static void debug(const std::string& category, const std::string& message) {
        log(Level::DEBUG, category, message);
    }
    
    static void info(const std::string& category, const std::string& message) {
        log(Level::INFO, category, message);
    }
    
    static void warn(const std::string& category, const std::string& message) {
        log(Level::WARNING, category, message);
    }
    
    static void error(const std::string& category, const std::string& message) {
        log(Level::ERROR, category, message);
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - CONFIGURATION NIVEAU
    // ========================================================================
    
    static void setLevel(Level level) {
        std::lock_guard<std::mutex> lock(getMutex());
        getMinLevel() = level;
    }
    
    static Level getLevel() {
        std::lock_guard<std::mutex> lock(getMutex());
        return getMinLevel();
    }
    
    static std::string getGlobalLevel() {
        std::lock_guard<std::mutex> lock(getMutex());
        switch (getMinLevel()) {
            case Level::DEBUG:   return "DEBUG";
            case Level::INFO:    return "INFO";
            case Level::WARNING: return "WARNING";
            case Level::ERROR:   return "ERROR";
            default:             return "INFO";
        }
    }
    
    static bool setGlobalLevel(const std::string& level) {
        std::lock_guard<std::mutex> lock(getMutex());
        std::string upperLevel = level;
        std::transform(upperLevel.begin(), upperLevel.end(), upperLevel.begin(), ::toupper);
        
        if (upperLevel == "DEBUG") {
            getMinLevel() = Level::DEBUG;
        } else if (upperLevel == "INFO") {
            getMinLevel() = Level::INFO;
        } else if (upperLevel == "WARNING" || upperLevel == "WARN") {
            getMinLevel() = Level::WARNING;
        } else if (upperLevel == "ERROR") {
            getMinLevel() = Level::ERROR;
        } else {
            return false;
        }
        return true;
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - CONFIGURATION AFFICHAGE
    // ========================================================================
    
    static void enableColors(bool enable) {
        std::lock_guard<std::mutex> lock(getMutex());
        getColorsEnabled() = enable;
    }
    
    static void enableTimestamps(bool enable) {
        std::lock_guard<std::mutex> lock(getMutex());
        getTimestampsEnabled() = enable;
    }
    
    static void enableCategory(bool enable) {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryEnabled() = enable;
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - FILE LOGGING (v3.1.1)
    // ========================================================================
    
    /**
     * Vérifie si le logging fichier est activé
     */
    static bool isFileLoggingEnabled() {
        std::lock_guard<std::mutex> lock(getMutex());
        return getFileLoggingEnabled();
    }
    
    /**
     * Active/désactive le logging fichier
     * @param enabled true pour activer, false pour désactiver
     * @param filepath Chemin du fichier (optionnel)
     * @return true si succès
     */
    static bool enableFileLogging(bool enabled, const std::string& filepath = "") {
        std::lock_guard<std::mutex> lock(getMutex());
        
        if (enabled) {
            // Mise à jour du chemin si fourni
            if (!filepath.empty()) {
                getLogFilePath() = filepath;
            }
            
            // Vérifier qu'on a un chemin valide
            if (getLogFilePath().empty()) {
                std::cerr << "[Logger] ERROR: Cannot enable file logging without filepath" << std::endl;
                return false;
            }
            
            try {
                // Ouvrir le fichier en mode append
                getLogFile().open(getLogFilePath(), std::ios::app);
                
                if (!getLogFile().is_open()) {
                    std::cerr << "[Logger] ERROR: Cannot open log file: " << getLogFilePath() << std::endl;
                    return false;
                }
                
                getFileLoggingEnabled() = true;
                
                // Message de démarrage
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                getLogFile() << "\n========================================\n";
                getLogFile() << "File logging enabled at " << std::ctime(&time);
                getLogFile() << "========================================\n";
                getLogFile().flush();
                
                return true;
                
            } catch (const std::exception& e) {
                std::cerr << "[Logger] ERROR: Exception: " << e.what() << std::endl;
                getFileLoggingEnabled() = false;
                return false;
            }
        } else {
            // Désactiver
            if (getLogFile().is_open()) {
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                getLogFile() << "\n========================================\n";
                getLogFile() << "File logging disabled at " << std::ctime(&time);
                getLogFile() << "========================================\n";
                getLogFile().flush();
                getLogFile().close();
            }
            
            getFileLoggingEnabled() = false;
            return true;
        }
    }
    
    /**
     * Définit le chemin du fichier log
     */
    static void setFilePath(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(getMutex());
        getLogFilePath() = filepath;
    }
    
    /**
     * Récupère le chemin du fichier log
     */
    static std::string getFilePath() {
        std::lock_guard<std::mutex> lock(getMutex());
        return getLogFilePath();
    }
    
    // ========================================================================
    // MÉTHODES PUBLIQUES - FILTRAGE CATÉGORIE
    // ========================================================================
    
    static void setCategoryFilter(const std::vector<std::string>& categories) {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryFilter() = categories;
    }
    
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
     * Méthode principale de logging (thread-safe)
     */
    static void log(Level level, const std::string& category, const std::string& message) {
        // Optimisation: vérifier niveau avant lock
        if (level < getMinLevel()) {
            getFilteredMessages().fetch_add(1);
            return;
        }
        
        // Lock pour thread-safety
        std::lock_guard<std::mutex> lock(getMutex());
        
        // Vérifier filtre de catégorie
        if (!getCategoryFilter().empty()) {
            if (std::find(getCategoryFilter().begin(), 
                          getCategoryFilter().end(), 
                          category) == getCategoryFilter().end()) {
                getFilteredMessages().fetch_add(1);
                return;
            }
        }
        
        try {
            // Construire le message formaté
            std::ostringstream oss;
            
            // Timestamp
            if (getTimestampsEnabled()) {
                oss << "[" << getCurrentTimestamp() << "] ";
            }
            
            // Niveau avec couleur
            if (getColorsEnabled()) {
                oss << getColorCode(level) << "[" << levelToString(level) << "]" 
                    << getColorReset() << " ";
            } else {
                oss << "[" << levelToString(level) << "] ";
            }
            
            // Catégorie
            if (getCategoryEnabled()) {
                oss << "[" << category << "] ";
            }
            
            // Message
            oss << message;
            
            std::string formattedMessage = oss.str();
            
            // Écrire sur console
            if (level >= Level::ERROR) {
                std::cerr << formattedMessage << std::endl;
            } else {
                std::cout << formattedMessage << std::endl;
            }
            
            // Écrire dans fichier si activé
            if (getFileLoggingEnabled() && getLogFile().is_open()) {
                getLogFile() << formattedMessage << std::endl;
                getLogFile().flush();
                
                // Vérifier rotation
                getLogFile().seekp(0, std::ios::end);
                size_t fileSize = static_cast<size_t>(getLogFile().tellp());
                
                if (fileSize >= getMaxFileSize()) {
                    rotateLogFile();
                }
            }
            
            // Écrire dans syslog si activé
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
    // MÉTHODES PRIVÉES - ROTATION FICHIER
    // ========================================================================
    
    static void rotateLogFile() {
        // Fermer fichier actuel
        if (getLogFile().is_open()) {
            getLogFile() << "========================================\n";
            getLogFile() << "File rotation: " << getCurrentTimestamp() << "\n";
            getLogFile() << "========================================\n";
            getLogFile().close();
        }
        
        const std::string& basePath = getLogFilePath();
        const size_t maxFiles = getMaxFiles();
        
        // Supprimer le plus ancien
        std::string oldestPath = basePath + "." + std::to_string(maxFiles);
        std::remove(oldestPath.c_str());
        
        // Renommer les fichiers
        for (int i = static_cast<int>(maxFiles) - 1; i > 0; --i) {
            std::string oldPath = basePath + "." + std::to_string(i);
            std::string newPath = basePath + "." + std::to_string(i + 1);
            std::rename(oldPath.c_str(), newPath.c_str());
        }
        
        // Renommer fichier actuel en .1
        std::string backupPath = basePath + ".1";
        std::rename(basePath.c_str(), backupPath.c_str());
        
        // Rouvrir nouveau fichier
        getLogFile().open(basePath, std::ios::app);
        
        if (getLogFile().is_open()) {
            getLogFile() << "========================================\n";
            getLogFile() << "New log file: " << getCurrentTimestamp() << "\n";
            getLogFile() << "========================================\n";
            getLogFile().flush();
        }
        
        getFileRotations().fetch_add(1);
    }
    
    // ========================================================================
    // MÉTHODES PRIVÉES - UTILITAIRES
    // ========================================================================
    
    static std::string levelToString(Level level) {
        switch (level) {
            case Level::DEBUG:   return "DEBUG";
            case Level::INFO:    return "INFO ";
            case Level::WARNING: return "WARN ";
            case Level::ERROR:   return "ERROR";
            default:             return "?????";
        }
    }
    
    static std::string getColorCode(Level level) {
        switch (level) {
            case Level::DEBUG:   return "\033[36m";  // Cyan
            case Level::INFO:    return "\033[32m";  // Vert
            case Level::WARNING: return "\033[33m";  // Jaune
            case Level::ERROR:   return "\033[31m";  // Rouge
            default:             return "";
        }
    }
    
    static std::string getColorReset() {
        return "\033[0m";
    }
    
    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;
        
        std::tm tm_buf;
        #ifdef _WIN32
            localtime_s(&tm_buf, &now_c);
        #else
            localtime_r(&now_c, &tm_buf);
        #endif
        
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << now_ms.count();
        
        return oss.str();
    }
    
    // ========================================================================
    // SINGLETON - VARIABLES STATIQUES (Meyer's Singleton)
    // ========================================================================
    
    static std::mutex& getMutex() {
        static std::mutex mutex;
        return mutex;
    }
    
    static Level& getMinLevel() {
        static Level minLevel = Level::INFO;
        return minLevel;
    }
    
    static bool& getColorsEnabled() {
        static bool enabled = true;
        return enabled;
    }
    
    static bool& getTimestampsEnabled() {
        static bool enabled = true;
        return enabled;
    }
    
    static bool& getCategoryEnabled() {
        static bool enabled = true;
        return enabled;
    }
    
    // Variables fichier
    static std::ofstream& getLogFile() {
        static std::ofstream file;
        return file;
    }
    
    static std::string& getLogFilePath() {
        static std::string path;
        return path;
    }
    
    static size_t& getMaxFileSize() {
        static size_t size = 10 * 1024 * 1024;  // 10 MB
        return size;
    }
    
    static size_t& getMaxFiles() {
        static size_t count = 5;
        return count;
    }
    
    static bool& getFileLoggingEnabled() {
        static bool enabled = false;
        return enabled;
    }
    
    // Variables filtrage
    static std::vector<std::string>& getCategoryFilter() {
        static std::vector<std::string> filters;
        return filters;
    }
    
    // Variables syslog
    static bool& getSyslogEnabled() {
        static bool enabled = false;
        return enabled;
    }
    
    // Métriques
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
// FIN DU FICHIER Logger.h v3.1.1 - CORRIGÉ
// ============================================================================
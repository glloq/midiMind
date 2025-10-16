// ============================================================================
// Fichier: backend/src/core/Logger.h
// Version: 3.0.2 - CORRECTION DES ERREURS DE COMPILATION
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - Ligne 216: Utilisation de std::remove avec algorithm au lieu de stdio.h
// - Ligne 296: Utilisation correcte de std::find avec algorithm
// - Ajout de removeCategoryFilter() manquant
// ============================================================================

#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <mutex>
#include <vector>
#include <atomic>
#include <algorithm>  // ✅ AJOUT POUR std::remove et std::find

#ifdef __linux__
#include <syslog.h>
#endif

namespace midiMind {

/**
 * @class Logger
 * @brief Système de logging thread-safe avec rotation
 */
class Logger {
public:
    // ========================================================================
    // ENUMS
    // ========================================================================
    
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3
    };
    
    struct Stats {
        uint64_t totalMessages = 0;
        uint64_t debugMessages = 0;
        uint64_t infoMessages = 0;
        uint64_t warnMessages = 0;
        uint64_t errorMessages = 0;
        uint64_t filteredMessages = 0;
        uint64_t fileRotations = 0;
        bool fileLoggingEnabled = false;
        bool syslogEnabled = false;
        size_t categoryFilters = 0;
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
    // CONFIGURATION
    // ========================================================================
    
    static void setLevel(Level level) {
        std::lock_guard<std::mutex> lock(getMutex());
        getLevel() = level;
    }
    
    static Level getLevel() {
        std::lock_guard<std::mutex> lock(getMutex());
        return getLevel();
    }
    
    // ========================================================================
    // FICHIER DE LOG
    // ========================================================================
    
    static bool enableFileLogging(const std::string& filepath, 
                                  size_t maxSizeMB = 10,
                                  int maxFiles = 5) {
        std::lock_guard<std::mutex> lock(getMutex());
        
        getLogFile().open(filepath, std::ios::app);
        if (!getLogFile().is_open()) {
            return false;
        }
        
        getFilePath() = filepath;
        getMaxFileSize() = maxSizeMB * 1024 * 1024;
        getMaxFiles() = maxFiles;
        getFileLoggingEnabled() = true;
        
        return true;
    }
    
    static void disableFileLogging() {
        std::lock_guard<std::mutex> lock(getMutex());
        
        if (getLogFile().is_open()) {
            getLogFile().close();
        }
        
        getFileLoggingEnabled() = false;
    }
    
    // ========================================================================
    // FILTRES DE CATÉGORIE
    // ========================================================================
    
    static void addCategoryFilter(const std::string& category) {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryFilter().push_back(category);
    }
    
    /**
     * @brief ✅ CORRECTION: Supprime un filtre de catégorie
     * @param category Catégorie à retirer du filtre
     */
    static void removeCategoryFilter(const std::string& category) {
        std::lock_guard<std::mutex> lock(getMutex());
        auto& filter = getCategoryFilter();
        
        // ✅ UTILISATION CORRECTE: std::remove de <algorithm> et non stdio.h
        filter.erase(
            std::remove(filter.begin(), filter.end(), category),
            filter.end()
        );
    }
    
    static void clearCategoryFilter() {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryFilter().clear();
    }
    
    // ========================================================================
    // SYSLOG (Linux)
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
    // STATISTIQUES
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
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief ✅ CORRECTION: Méthode de logging principale
     */
    static void log(Level level, const std::string& category, const std::string& message) {
        try {
            std::lock_guard<std::mutex> lock(getMutex());
            
            // Filtrer par niveau
            if (level < getLevel()) {
                getFilteredMessages().fetch_add(1);
                return;
            }
            
            // ✅ CORRECTION: Filtrer par catégorie avec std::find de <algorithm>
            if (!getCategoryFilter().empty()) {
                const auto& filter = getCategoryFilter();
                if (std::find(filter.begin(), filter.end(), category) == filter.end()) {
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
            
            // Fichier
            if (getFileLoggingEnabled() && getLogFile().is_open()) {
                checkRotation();
                getLogFile() << formatted << std::endl;
                getLogFile().flush();
            }
            
            // Syslog
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
            
        } catch (...) {
            // Ne pas propager d'exception depuis le logger
        }
    }
    
    static void checkRotation() {
        if (!getFileLoggingEnabled() || !getLogFile().is_open()) {
            return;
        }
        
        getLogFile().seekp(0, std::ios::end);
        size_t size = getLogFile().tellp();
        
        if (size >= getMaxFileSize()) {
            rotateLog();
        }
    }
    
    static void rotateLog() {
        getLogFile().close();
        
        // Rotation des fichiers
        for (int i = getMaxFiles() - 1; i > 0; --i) {
            std::string oldFile = getFilePath() + "." + std::to_string(i);
            std::string newFile = getFilePath() + "." + std::to_string(i + 1);
            std::rename(oldFile.c_str(), newFile.c_str());
        }
        
        std::rename(getFilePath().c_str(), (getFilePath() + ".1").c_str());
        
        getLogFile().open(getFilePath(), std::ios::app);
        getFileRotations().fetch_add(1);
    }
    
    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        
        std::ostringstream oss;
        oss << buffer << "." << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
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
    // SINGLETONS STATIQUES
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
        static std::ofstream logFile;
        return logFile;
    }
    
    static std::string& getFilePath() {
        static std::string path;
        return path;
    }
    
    static size_t& getMaxFileSize() {
        static size_t size = 10 * 1024 * 1024;
        return size;
    }
    
    static int& getMaxFiles() {
        static int count = 5;
        return count;
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
// FIN DU FICHIER Logger.h
// ============================================================================

// ============================================================================
// Fichier: backend/src/core/Logger.h
// Version: 3.1.1 - CORRECTIONS CRITIQUES
// Date: 2025-10-15
// ============================================================================
// CORRECTIONS v3.1.1:
//   ✅ FIX #1: Suppression du conflit getLevel() - une seule version publique
//   ✅ FIX #2: Correction setLevel() pour utiliser getLevelRef()
//
// Description:
//   Système de logging centralisé thread-safe avec rotation automatique
// ============================================================================

#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <atomic>
#include <ctime>

namespace midiMind {

/**
 * @class Logger
 * @brief Système de logging centralisé thread-safe
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
        getLevelRef() = level;  // ✅ FIX #2: Utilise getLevelRef() pour modifier
    }
    
    /**
     * @brief Récupère le niveau actuel
     * @return Level Niveau actuel
     * @note Thread-safe
     */
    static Level getLevel() {  // ✅ FIX #1: Une seule version publique
        std::lock_guard<std::mutex> lock(getMutex());
        return getLevelRef();
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
     */
    static bool enableFileLogging(const std::string& filepath, 
                                  size_t maxSize = 10 * 1024 * 1024,
                                  size_t maxFiles = 5) {
        std::lock_guard<std::mutex> lock(getMutex());
        
        // Fermer l'ancien fichier si ouvert
        if (getLogFile().is_open()) {
            getLogFile().close();
        }
        
        // Configurer rotation
        getMaxFileSize() = maxSize;
        getMaxFiles() = maxFiles;
        getLogFilePath() = filepath;
        
        // Ouvrir nouveau fichier
        getLogFile().open(filepath, std::ios::out | std::ios::app);
        
        if (!getLogFile().is_open()) {
            std::cerr << "[Logger] Failed to open log file: " << filepath << std::endl;
            return false;
        }
        
        getFileLoggingEnabled() = true;
        return true;
    }
    
    /**
     * @brief Désactive le logging fichier
     * @note Thread-safe
     */
    static void disableFileLogging() {
        std::lock_guard<std::mutex> lock(getMutex());
        
        if (getLogFile().is_open()) {
            getLogFile().close();
        }
        
        getFileLoggingEnabled() = false;
    }
    
    /**
     * @brief Ajoute un filtre de catégorie
     * @param category Catégorie à filtrer (exclure)
     * @note Thread-safe
     */
    static void addCategoryFilter(const std::string& category) {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryFilter().push_back(category);
    }
    
    /**
     * @brief Supprime un filtre de catégorie
     * @param category Catégorie à ne plus filtrer
     * @note Thread-safe
     */
    static void removeCategoryFilter(const std::string& category) {
        std::lock_guard<std::mutex> lock(getMutex());
        auto& filter = getCategoryFilter();
        filter.erase(
            std::remove(filter.begin(), filter.end(), category),
            filter.end()
        );
    }
    
    /**
     * @brief Efface tous les filtres de catégorie
     * @note Thread-safe
     */
    static void clearCategoryFilters() {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryFilter().clear();
    }
    
    /**
     * @brief Récupère les statistiques de logging
     * @return Stats Structure des statistiques
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
     * @brief Réinitialise les statistiques
     * @note Thread-safe
     */
    static void resetStats() {
        getTotalMessages().store(0);
        getDebugMessages().store(0);
        getInfoMessages().store(0);
        getWarnMessages().store(0);
        getErrorMessages().store(0);
        getFilteredMessages().store(0);
        getFileRotations().store(0);
    }
    
    /**
     * @brief Force une rotation du fichier de log
     * @note Thread-safe
     */
    static void rotateLogFile() {
        std::lock_guard<std::mutex> lock(getMutex());
        performRotation();
    }
    
private:
    // ========================================================================
    // MÉTHODES PRIVÉES - CORE
    // ========================================================================
    
    /**
     * @brief Méthode principale de logging
     * @note Thread-safe (appelant doit déjà avoir le lock)
     */
    static void log(Level level, const std::string& category, const std::string& message) {
        std::lock_guard<std::mutex> lock(getMutex());
        
        // Filtrage par niveau
        if (level < getLevelRef()) {
            getFilteredMessages()++;
            return;
        }
        
        // Filtrage par catégorie
        const auto& filter = getCategoryFilter();
        if (std::find(filter.begin(), filter.end(), category) != filter.end()) {
            getFilteredMessages()++;
            return;
        }
        
        // Statistiques
        getTotalMessages()++;
        switch (level) {
            case Level::DEBUG:   getDebugMessages()++; break;
            case Level::INFO:    getInfoMessages()++; break;
            case Level::WARNING: getWarnMessages()++; break;
            case Level::ERROR:   getErrorMessages()++; break;
        }
        
        // Formater le message
        std::string formatted = formatLogMessage(level, category, message);
        
        // Output console
        std::cout << getColorCode(level) << formatted << "\033[0m" << std::endl;
        
        // Output fichier
        if (getFileLoggingEnabled() && getLogFile().is_open()) {
            // Vérifier si rotation nécessaire
            checkAndRotate();
            
            getLogFile() << formatted << std::endl;
            getLogFile().flush();
        }
    }
    
    /**
     * @brief Formate un message de log
     */
    static std::string formatLogMessage(Level level, const std::string& category, 
                                       const std::string& message) {
        std::ostringstream ss;
        ss << "[" << getCurrentTimestamp() << "] "
           << "[" << levelToString(level) << "] "
           << "[" << category << "] "
           << message;
        return ss.str();
    }
    
    /**
     * @brief Vérifie et effectue la rotation si nécessaire
     */
    static void checkAndRotate() {
        if (!getLogFile().is_open()) return;
        
        // Obtenir taille actuelle
        auto currentPos = getLogFile().tellp();
        if (currentPos < 0) return;
        
        size_t currentSize = static_cast<size_t>(currentPos);
        
        // Rotation si nécessaire
        if (currentSize >= getMaxFileSize()) {
            performRotation();
        }
    }
    
    /**
     * @brief Effectue la rotation des fichiers
     */
    static void performRotation() {
        if (!getFileLoggingEnabled()) return;
        
        const std::string& basePath = getLogFilePath();
        
        // Fermer fichier actuel
        getLogFile().close();
        
        // Rotation: .4 → supprimé, .3 → .4, .2 → .3, .1 → .2, .0 → .1, actuel → .0
        size_t maxFiles = getMaxFiles();
        
        // Supprimer le plus ancien
        std::string oldestFile = basePath + "." + std::to_string(maxFiles - 1);
        std::remove(oldestFile.c_str());
        
        // Décaler les autres
        for (size_t i = maxFiles - 1; i > 0; --i) {
            std::string from = basePath + "." + std::to_string(i - 1);
            std::string to = basePath + "." + std::to_string(i);
            std::rename(from.c_str(), to.c_str());
        }
        
        // Renommer fichier actuel
        std::string firstBackup = basePath + ".0";
        std::rename(basePath.c_str(), firstBackup.c_str());
        
        // Créer nouveau fichier
        getLogFile().open(basePath, std::ios::out | std::ios::app);
        
        getFileRotations()++;
    }
    
    /**
     * @brief Génère un timestamp formaté
     */
    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm tm;
        localtime_r(&time, &tm);
        
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        
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
    
    // ✅ FIX #1: Version privée qui retourne une référence modifiable
    static Level& getLevelRef() {
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
// FIN DU FICHIER Logger.h v3.1.1 - CORRIGÉ
// ============================================================================

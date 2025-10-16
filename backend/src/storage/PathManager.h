// ============================================================================
// File: backend/src/storage/PathManager.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Centralized path management system for the application.
//   Defines and manages all paths used by MidiMind.
//
// Directory Structure:
//   /var/lib/midimind/
//   ├── config/
//   │   ├── config.json
//   │   └── presets/
//   ├── data/
//   │   ├── midimind.db
//   │   └── sessions/
//   ├── midi/
//   │   ├── files/
//   │   └── recordings/
//   ├── logs/
//   └── backups/
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Updated to use FileManager::Unsafe namespace
//   - Enhanced path validation
//   - Added migration paths
//   - Thread-safe operations
//
// ============================================================================

#pragma once

#include <string>
#include <mutex>
#include "../core/Logger.h"
#include "FileManager.h"

namespace midiMind {

/**
 * @class PathManager
 * @brief Centralized path management system
 * 
 * Singleton class that manages all filesystem paths used by MidiMind.
 * Ensures consistent path structure across the application.
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses internal mutex for synchronization
 * 
 * Example:
 * ```cpp
 * PathManager& pm = PathManager::instance();
 * pm.initialize();
 * 
 * std::string configPath = pm.getConfigFilePath();
 * std::string dbPath = pm.getDatabasePath();
 * std::string logPath = pm.getLogFilePath();
 * ```
 */
class PathManager {
public:
    // ========================================================================
    // SINGLETON PATTERN
    // ========================================================================
    
    /**
     * @brief Get singleton instance
     * @return Reference to PathManager instance
     */
    static PathManager& instance();
    
    // Disable copy and assignment
    PathManager(const PathManager&) = delete;
    PathManager& operator=(const PathManager&) = delete;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Initialize directory structure
     * @note Creates all required directories
     * @note Thread-safe
     */
    void initialize();
    
    /**
     * @brief Set base path
     * @param basePath Base directory path (default: /var/lib/midimind)
     * @note Thread-safe
     */
    void setBasePath(const std::string& basePath);
    
    /**
     * @brief Get base path
     * @return Base directory path
     * @note Thread-safe
     */
    std::string getBasePath() const;
    
    // ========================================================================
    // CONFIGURATION PATHS
    // ========================================================================
    
    /**
     * @brief Get config directory path
     * @return Path to config/ directory
     */
    std::string getConfigPath() const;
    
    /**
     * @brief Get config file path
     * @return Path to config.json file
     */
    std::string getConfigFilePath() const;
    
    /**
     * @brief Get presets directory path
     * @return Path to presets/ directory
     */
    std::string getPresetsPath() const;
    
    // ========================================================================
    // DATA PATHS
    // ========================================================================
    
    /**
     * @brief Get data directory path
     * @return Path to data/ directory
     */
    std::string getDataPath() const;
    
    /**
     * @brief Get database file path
     * @return Path to midimind.db file
     */
    std::string getDatabasePath() const;
    
    /**
     * @brief Get database migrations directory
     * @return Path to migrations/ directory
     */
    std::string getMigrationsPath() const;
    
    /**
     * @brief Get sessions directory path
     * @return Path to sessions/ directory
     */
    std::string getSessionsPath() const;
    
    // ========================================================================
    // MIDI PATHS
    // ========================================================================
    
    /**
     * @brief Get MIDI root directory path
     * @return Path to midi/ directory
     */
    std::string getMidiPath() const;
    
    /**
     * @brief Get MIDI files directory path
     * @return Path to midi/files/ directory
     */
    std::string getMidiFilesPath() const;
    
    /**
     * @brief Get MIDI recordings directory path
     * @return Path to midi/recordings/ directory
     */
    std::string getMidiRecordingsPath() const;
    
    // ========================================================================
    // LOG PATHS
    // ========================================================================
    
    /**
     * @brief Get logs directory path
     * @return Path to logs/ directory
     */
    std::string getLogsPath() const;
    
    /**
     * @brief Get current log file path
     * @return Path to current log file (dated)
     * @note Format: midimind_YYYY-MM-DD.log
     */
    std::string getLogFilePath() const;
    
    // ========================================================================
    // BACKUP PATHS
    // ========================================================================
    
    /**
     * @brief Get backups directory path
     * @return Path to backups/ directory
     */
    std::string getBackupsPath() const;
    
    /**
     * @brief Create database backup
     * @return Path to created backup file
     * @note Format: midimind_YYYY-MM-DD_HH-MM-SS.db
     * @note Thread-safe
     */
    std::string createDatabaseBackup();
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    /**
     * @brief Clean old files in directory
     * @param directory Directory to clean
     * @param maxAgeDays Maximum file age in days
     * @return Number of files deleted
     * @note Thread-safe
     */
    int cleanOldFiles(const std::string& directory, int maxAgeDays);
    
    /**
     * @brief Join path components
     * @param parts Path components to join
     * @return Joined path
     * @note Uses platform-specific separator
     */
    static std::string joinPath(const std::vector<std::string>& parts);
    
private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR (SINGLETON)
    // ========================================================================
    
    PathManager();
    ~PathManager();
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Base path for all application data
    std::string basePath_;
    
    /// Mutex for thread-safety
    mutable std::mutex mutex_;
};

} // namespace midiMind
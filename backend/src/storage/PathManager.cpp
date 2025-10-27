// ============================================================================
// File: backend/src/storage/PathManager.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of PathManager - Centralized path management.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Fixed TOCTOU race condition (removed access() check)
//   - Added NULL check for getpwuid()
//   - Added error checks for time functions
//   - Added errno checking for stat()
//   - Rewrote joinPath() for robustness
//   - Enhanced directory creation with permission validation
//
// ============================================================================

#include "PathManager.h"
#include "../core/TimeUtils.h"
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::string joinPath(const std::vector<std::string>& parts) {
    if (parts.empty()) {
        return "";
    }
    
    std::string result;
    
    for (size_t i = 0; i < parts.size(); ++i) {
        std::string part = parts[i];
        
        // Skip empty parts
        if (part.empty()) {
            continue;
        }
        
        // For first part, keep leading slash if present
        if (i == 0) {
            result = part;
        } else {
            // Remove trailing slashes from result
            while (result.length() > 1 && result.back() == '/') {
                result.pop_back();
            }
            
            // Remove leading slashes from part
            while (!part.empty() && part[0] == '/') {
                part.erase(0, 1);
            }
            
            // Join with separator
            if (!part.empty()) {
                if (!result.empty() && result.back() != '/') {
                    result += '/';
                }
                result += part;
            }
        }
    }
    
    return result;
}

// ============================================================================
// SINGLETON
// ============================================================================

PathManager& PathManager::instance() {
    static PathManager instance;
    return instance;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

PathManager::PathManager() {
    // Default path: /var/lib/midimind (for production)
    // Falls back to /home/$USER/MidiMind for development
    
    const char* varLibPath = "/var/lib/midimind";
    
    // Try to create directory first - avoid TOCTOU race
    // If creation succeeds, we know we have write access
    if (FileManager::Unsafe::createDirectory(varLibPath, true)) {
        basePath_ = varLibPath;
        Logger::debug("PathManager", "Using production path: " + std::string(varLibPath));
    } else {
        // Fall back to user home directory
        const char* homeDir = getenv("HOME");
        if (!homeDir) {
            struct passwd* pw = getpwuid(getuid());
            if (pw && pw->pw_dir) {
                homeDir = pw->pw_dir;
            } else {
                // Last resort fallback
                homeDir = "/home/pi";
                Logger::warning("PathManager", 
                              "Could not determine home directory, using default: /home/pi");
            }
        }
        basePath_ = std::string(homeDir) + "/MidiMind";
        Logger::debug("PathManager", "Using development path: " + basePath_);
    }
    
    Logger::info("PathManager", "PathManager created");
    Logger::info("PathManager", "  Base path: " + basePath_);
}

PathManager::~PathManager() {
    Logger::debug("PathManager", "PathManager destroyed");
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void PathManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("PathManager", "Initializing directory structure...");
    
    // List of all directories to create
    std::vector<std::string> directories = {
        basePath_,
        getConfigPath(),
        getPresetsPath(),
        getDataPath(),
        getMigrationsPath(),
        getSessionsPath(),
        getMidiPath(),
        getMidiFilesPath(),
        getMidiRecordingsPath(),
        getLogsPath(),
        getBackupsPath()
    };
    
    int created = 0;
    int existing = 0;
    int failed = 0;
    
    for (const auto& dir : directories) {
        if (!FileManager::Unsafe::exists(dir)) {
            if (FileManager::Unsafe::createDirectory(dir, true)) {
                Logger::info("PathManager", "  ✓ Created: " + dir);
                created++;
                
                // Verify write permissions by attempting to access
                if (access(dir.c_str(), W_OK) != 0) {
                    Logger::warning("PathManager", 
                                  "  ⚠ Created but not writable: " + dir + " (errno: " + 
                                  std::to_string(errno) + ")");
                }
            } else {
                Logger::error("PathManager", "  ✗ Failed: " + dir);
                failed++;
            }
        } else {
            Logger::debug("PathManager", "  - Exists: " + dir);
            existing++;
            
            // Verify write permissions on existing directories
            if (access(dir.c_str(), W_OK) != 0) {
                Logger::warning("PathManager", 
                              "  ⚠ Not writable: " + dir + " (errno: " + 
                              std::to_string(errno) + ")");
            }
        }
    }
    
    Logger::info("PathManager", "✓ Directory structure initialized");
    Logger::info("PathManager", 
                "  Created: " + std::to_string(created) + 
                ", Existing: " + std::to_string(existing) + 
                ", Failed: " + std::to_string(failed));
}

void PathManager::setBasePath(const std::string& basePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    basePath_ = basePath;
    Logger::info("PathManager", "Base path changed to: " + basePath_);
}

std::string PathManager::getBasePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return basePath_;
}

// ============================================================================
// CONFIGURATION PATHS
// ============================================================================

std::string PathManager::getConfigPath() const {
    return joinPath({basePath_, "config"});
}

std::string PathManager::getConfigFilePath() const {
    return joinPath({getConfigPath(), "config.json"});
}

std::string PathManager::getPresetsPath() const {
    return joinPath({getConfigPath(), "presets"});
}

// ============================================================================
// DATA PATHS
// ============================================================================

std::string PathManager::getDataPath() const {
    return joinPath({basePath_, "data"});
}

std::string PathManager::getDatabasePath() const {
    return joinPath({getDataPath(), "midimind.db"});
}

std::string PathManager::getMigrationsPath() const {
    return joinPath({getDataPath(), "migrations"});
}

std::string PathManager::getSessionsPath() const {
    return joinPath({getDataPath(), "sessions"});
}

// ============================================================================
// MIDI PATHS
// ============================================================================

std::string PathManager::getMidiPath() const {
    return joinPath({basePath_, "midi"});
}

std::string PathManager::getMidiFilesPath() const {
    return joinPath({getMidiPath(), "files"});
}

std::string PathManager::getMidiRecordingsPath() const {
    return joinPath({getMidiPath(), "recordings"});
}

// ============================================================================
// LOG PATHS
// ============================================================================

std::string PathManager::getLogsPath() const {
    return joinPath({basePath_, "logs"});
}

std::string PathManager::getLogFilePath() const {
    // Format: midimind_YYYY-MM-DD.log
    std::time_t now = std::time(nullptr);
    if (now == static_cast<std::time_t>(-1)) {
        Logger::error("PathManager", "Failed to get current time");
        return joinPath({getLogsPath(), "midimind.log"});  // Fallback
    }
    
    std::tm tm;
    if (!localtime_r(&now, &tm)) {
        Logger::error("PathManager", "Failed to convert time");
        return joinPath({getLogsPath(), "midimind.log"});  // Fallback
    }
    
    std::ostringstream oss;
    oss << "midimind_"
        << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900) << "-"
        << std::setw(2) << (tm.tm_mon + 1) << "-"
        << std::setw(2) << tm.tm_mday
        << ".log";
    
    return joinPath({getLogsPath(), oss.str()});
}

// ============================================================================
// BACKUP PATHS
// ============================================================================

std::string PathManager::getBackupsPath() const {
    return joinPath({basePath_, "backups"});
}

std::string PathManager::createDatabaseBackup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("PathManager", "Creating database backup...");
    
    // Generate backup filename with timestamp
    std::time_t now = std::time(nullptr);
    if (now == static_cast<std::time_t>(-1)) {
        Logger::error("PathManager", "Failed to get current time for backup");
        return "";
    }
    
    std::tm tm;
    if (!localtime_r(&now, &tm)) {
        Logger::error("PathManager", "Failed to convert time for backup");
        return "";
    }
    
    std::ostringstream oss;
    oss << "midimind_"
        << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900) << "-"
        << std::setw(2) << (tm.tm_mon + 1) << "-"
        << std::setw(2) << tm.tm_mday << "_"
        << std::setw(2) << tm.tm_hour << "-"
        << std::setw(2) << tm.tm_min << "-"
        << std::setw(2) << tm.tm_sec
        << ".db";
    
    std::string backupPath = joinPath({getBackupsPath(), oss.str()});
    std::string dbPath = getDatabasePath();
    
    // Check if database exists
    if (!FileManager::Unsafe::exists(dbPath)) {
        Logger::error("PathManager", "Database not found: " + dbPath);
        return "";
    }
    
    // Copy database file
    if (FileManager::Unsafe::copyFile(dbPath, backupPath)) {
        Logger::info("PathManager", "✓ Backup created: " + backupPath);
        return backupPath;
    } else {
        Logger::error("PathManager", "Failed to create backup");
        return "";
    }
}

// ============================================================================
// UTILITIES
// ============================================================================

int PathManager::cleanOldFiles(const std::string& directory, int maxAgeDays) {
    Logger::info("PathManager", "Cleaning old files in: " + directory);
    
    if (!FileManager::Unsafe::exists(directory)) {
        Logger::warning("PathManager", "Directory not found: " + directory);
        return 0;
    }
    
    auto files = FileManager::Unsafe::listFiles(directory);
    
    int deletedCount = 0;
    uint64_t maxAgeSeconds = static_cast<uint64_t>(maxAgeDays) * 24 * 3600;
    
    std::time_t nowTime = std::time(nullptr);
    if (nowTime == static_cast<std::time_t>(-1)) {
        Logger::error("PathManager", "Failed to get current time for cleanup");
        return 0;
    }
    uint64_t nowSeconds = static_cast<uint64_t>(nowTime);
    
    for (const auto& filename : files) {
        std::string filepath = joinPath({directory, filename});
        
        struct stat fileStat;
        if (stat(filepath.c_str(), &fileStat) != 0) {
            Logger::warning("PathManager", 
                          "  Failed to stat file: " + filename + " (errno: " + 
                          std::to_string(errno) + " - " + std::strerror(errno) + ")");
            continue;
        }
        
        uint64_t fileAge = nowSeconds - static_cast<uint64_t>(fileStat.st_mtime);
        
        if (fileAge > maxAgeSeconds) {
            if (FileManager::Unsafe::deleteFile(filepath)) {
                Logger::debug("PathManager", "  Deleted: " + filename);
                deletedCount++;
            } else {
                Logger::warning("PathManager", "  Failed to delete: " + filename);
            }
        }
    }
    
    Logger::info("PathManager", "✓ Cleaned " + std::to_string(deletedCount) + " old files");
    
    return deletedCount;
}

} // namespace midiMind

// ============================================================================
// END OF FILE PathManager.cpp
// ============================================================================
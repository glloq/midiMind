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
//   - Updated to use FileManager::Unsafe namespace
//   - Enhanced directory creation
//   - Added migration paths support
//
// ============================================================================

#include "PathManager.h"
#include "../core/TimeUtils.h"
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace midiMind {

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
    
    // Check if we have write access to /var/lib/midimind
    if (access(varLibPath, W_OK) == 0) {
        basePath_ = varLibPath;
    } else {
        // Fall back to user home directory
        const char* homeDir = getenv("HOME");
        if (!homeDir) {
            struct passwd* pw = getpwuid(getuid());
            homeDir = pw ? pw->pw_dir : "/home/pi";
        }
        basePath_ = std::string(homeDir) + "/MidiMind";
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
            } else {
                Logger::error("PathManager", "  ✗ Failed: " + dir);
                failed++;
            }
        } else {
            Logger::debug("PathManager", "  - Exists: " + dir);
            existing++;
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
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&time, &tm);
    
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
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&time, &tm);
    
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
        Logger::warn("PathManager", "Directory not found: " + directory);
        return 0;
    }
    
    auto files = FileManager::Unsafe::listFiles(directory);
    
    int deletedCount = 0;
    uint64_t maxAgeSeconds = static_cast<uint64_t>(maxAgeDays) * 24 * 3600;
    uint64_t nowSeconds = static_cast<uint64_t>(std::time(nullptr));
    
    for (const auto& filename : files) {
        std::string filepath = joinPath({directory, filename});
        
        struct stat fileStat;
        if (stat(filepath.c_str(), &fileStat) != 0) {
            continue;
        }
        
        uint64_t fileAge = nowSeconds - static_cast<uint64_t>(fileStat.st_mtime);
        
        if (fileAge > maxAgeSeconds) {
            if (FileManager::Unsafe::deleteFile(filepath)) {
                Logger::debug("PathManager", "  Deleted: " + filename);
                deletedCount++;
            } else {
                Logger::warn("PathManager", "  Failed to delete: " + filename);
            }
        }
    }
    
    Logger::info("PathManager", "✓ Cleaned " + std::to_string(deletedCount) + " old files");
    
    return deletedCount;
}

std::string PathManager::joinPath(const std::vector<std::string>& parts) {
    if (parts.empty()) {
        return "";
    }
    
    std::string result = parts[0];
    
    for (size_t i = 1; i < parts.size(); ++i) {
        // Remove trailing slash from result
        while (!result.empty() && (result.back() == '/' || result.back() == '\\')) {
            result.pop_back();
        }
        
        // Remove leading slash from part
        std::string part = parts[i];
        while (!part.empty() && (part[0] == '/' || part[0] == '\\')) {
            part.erase(0, 1);
        }
        
        if (!part.empty()) {
            result += "/" + part;
        }
    }
    
    return result;
}

} // namespace midiMind

// ============================================================================
// END OF FILE PathManager.cpp
// ============================================================================
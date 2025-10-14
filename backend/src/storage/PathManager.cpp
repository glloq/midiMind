// ============================================================================
// Fichier: src/storage/PathManager.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "PathManager.h"
#include "../core/TimeUtils.h"
#include <pwd.h>

namespace midiMind {

// ============================================================================
// SINGLETON
// ============================================================================

PathManager& PathManager::instance() {
    static PathManager instance;
    return instance;
}

// ============================================================================
// CONSTRUCTION PRIVÉE
// ============================================================================

PathManager::PathManager() {
    // Chemin par défaut: /home/pi/MidiMind
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        struct passwd* pw = getpwuid(getuid());
        homeDir = pw ? pw->pw_dir : "/home/pi";
    }
    
    basePath_ = std::string(homeDir) + "/MidiMind";
    
    Logger::info("PathManager", "PathManager created");
    Logger::info("PathManager", "  Base path: " + basePath_);
}

PathManager::~PathManager() {
    Logger::info("PathManager", "PathManager destroyed");
}

// ============================================================================
// INITIALISATION
// ============================================================================

void PathManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("PathManager", "Initializing directory structure...");
    
    // Créer tous les dossiers
    std::vector<std::string> directories = {
        basePath_,
        getConfigPath(),
        getPresetsPath(),
        getDataPath(),
        getSessionsPath(),
        getMidiPath(),
        getMidiFilesPath(),
        getMidiRecordingsPath(),
        getLogsPath(),
        getBackupsPath()
    };
    
    for (const auto& dir : directories) {
        if (!FileSystem::exists(dir)) {
            if (FileSystem::createDirectory(dir, true)) {
                Logger::info("PathManager", "  Created: " + dir);
            } else {
                Logger::error("PathManager", "  Failed to create: " + dir);
            }
        } else {
            Logger::debug("PathManager", "  Exists: " + dir);
        }
    }
    
    Logger::info("PathManager", "✓ Directory structure initialized");
}

void PathManager::setBasePath(const std::string& basePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    basePath_ = basePath;
    Logger::info("PathManager", "Base path set to: " + basePath_);
}

std::string PathManager::getBasePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return basePath_;
}

// ============================================================================
// CHEMINS PRINCIPAUX
// ============================================================================

std::string PathManager::getConfigPath() const {
    return FileSystem::joinPath({basePath_, "config"});
}

std::string PathManager::getConfigFilePath() const {
    return FileSystem::joinPath({getConfigPath(), "config.json"});
}

std::string PathManager::getPresetsPath() const {
    return FileSystem::joinPath({getConfigPath(), "presets"});
}

std::string PathManager::getDataPath() const {
    return FileSystem::joinPath({basePath_, "data"});
}

std::string PathManager::getDatabasePath() const {
    return FileSystem::joinPath({getDataPath(), "midimind.db"});
}

std::string PathManager::getSessionsPath() const {
    return FileSystem::joinPath({getDataPath(), "sessions"});
}

std::string PathManager::getMidiPath() const {
    return FileSystem::joinPath({basePath_, "midi"});
}

std::string PathManager::getMidiFilesPath() const {
    return FileSystem::joinPath({getMidiPath(), "files"});
}

std::string PathManager::getMidiRecordingsPath() const {
    return FileSystem::joinPath({getMidiPath(), "recordings"});
}

std::string PathManager::getLogsPath() const {
    return FileSystem::joinPath({basePath_, "logs"});
}

std::string PathManager::getLogFilePath() const {
    // Format: midimind_YYYY-MM-DD.log
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);
    
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "midimind_%Y-%m-%d.log", tm);
    
    return FileSystem::joinPath({getLogsPath(), buffer});
}

std::string PathManager::getBackupsPath() const {
    return FileSystem::joinPath({basePath_, "backups"});
}

// ============================================================================
// UTILITAIRES
// ============================================================================

int PathManager::cleanOldFiles(const std::string& directory, int maxAgeDays) {
    Logger::info("PathManager", "Cleaning old files in: " + directory);
    
    auto files = FileSystem::listFiles(directory);
    
    int deletedCount = 0;
    uint64_t maxAgeSeconds = maxAgeDays * 24 * 3600;
    uint64_t nowSeconds = TimeUtils::getCurrentTimestampSec();
    
    for (const auto& file : files) {
        struct stat buffer;
        if (stat(file.c_str(), &buffer) != 0) continue;
        
        uint64_t fileAge = nowSeconds - buffer.st_mtime;
        
        if (fileAge > maxAgeSeconds) {
            if (FileSystem::removeFile(file)) {
                Logger::debug("PathManager", "  Deleted: " + file);
                deletedCount++;
            }
        }
    }
    
    Logger::info("PathManager", "✓ Deleted " + std::to_string(deletedCount) + " old files");
    
    return deletedCount;
}

std::string PathManager::createDatabaseBackup() {
    Logger::info("PathManager", "Creating database backup...");
    
    std::string dbPath = getDatabasePath();
    
    if (!FileSystem::exists(dbPath)) {
        Logger::warn("PathManager", "Database does not exist");
        return "";
    }
    
    // Format: midimind_backup_YYYY-MM-DD_HH-MM-SS.db
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);
    
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "midimind_backup_%Y-%m-%d_%H-%M-%S.db", tm);
    
    std::string backupPath = FileSystem::joinPath({getBackupsPath(), buffer});
    
    if (FileSystem::copyFile(dbPath, backupPath)) {
        Logger::info("PathManager", "✓ Backup created: " + backupPath);
        return backupPath;
    } else {
        Logger::error("PathManager", "Failed to create backup");
        return "";
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER PathManager.cpp
// ============================================================================
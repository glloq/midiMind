// ============================================================================
// Fichier: backend/src/storage/FileManager.cpp
// Version: 3.0.0
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Implémentation du gestionnaire de fichiers applicatifs.
//   Gère logs, backups, exports, uploads et fichiers temporaires.
//
// Auteur: MidiMind Team
// Date: 2025-10-13
// Statut: ✅ COMPLET
// ============================================================================

#include "FileManager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

FileManager::FileManager(const std::string& rootPath)
    : rootPath_(rootPath) {
    
    if (rootPath_.empty()) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR, "Root path cannot be empty");
    }
    
    // Supprimer le / final si présent
    if (rootPath_.back() == '/') {
        rootPath_.pop_back();
    }
    
    Logger::info("FileManager", "═══════════════════════════════════════");
    Logger::info("FileManager", "  FileManager v3.0.0");
    Logger::info("FileManager", "═══════════════════════════════════════");
    Logger::info("FileManager", "  Root path: " + rootPath_);
    
    // Vérifier que le chemin racine existe
    if (!FileSystem::exists(rootPath_)) {
        Logger::warn("FileManager", "Root path does not exist, creating...");
        if (!FileSystem::createDirectory(rootPath_)) {
            THROW_ERROR(ErrorCode::FILE_WRITE_FAILED, 
                       "Failed to create root directory: " + rootPath_);
        }
    }
    
    Logger::info("FileManager", "✓ FileManager initialized");
}

FileManager::~FileManager() {
    Logger::info("FileManager", "FileManager destroyed");
}

// ============================================================================
// INITIALISATION
// ============================================================================

bool FileManager::initializeDirectories() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Initializing directory structure...");
    
    std::vector<std::string> directories = {
        DIR_LOGS,
        DIR_BACKUPS,
        DIR_EXPORTS,
        DIR_UPLOADS,
        DIR_TEMP
    };
    
    bool allSuccess = true;
    
    for (const auto& dir : directories) {
        std::string fullPath = rootPath_ + "/" + dir;
        
        if (FileSystem::exists(fullPath)) {
            Logger::debug("FileManager", "  ✓ " + dir + " exists");
        } else {
            Logger::info("FileManager", "  Creating " + dir + "...");
            if (FileSystem::createDirectory(fullPath)) {
                Logger::info("FileManager", "  ✓ " + dir + " created");
            } else {
                Logger::error("FileManager", "  ✗ Failed to create " + dir);
                allSuccess = false;
            }
        }
    }
    
    if (allSuccess) {
        Logger::info("FileManager", "✓ Directory structure initialized");
    } else {
        Logger::warn("FileManager", "⚠ Some directories failed to initialize");
    }
    
    return allSuccess;
}

// ============================================================================
// LISTAGE FICHIERS
// ============================================================================

std::vector<FileInfo> FileManager::listFiles(DirectoryType dirType, 
                                             const std::string& extension) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    
    Logger::debug("FileManager", "Listing files in: " + dirPath);
    
    if (!FileSystem::exists(dirPath)) {
        Logger::warn("FileManager", "Directory does not exist: " + dirPath);
        return {};
    }
    
    // Lister les fichiers
    auto filePaths = FileSystem::listFiles(dirPath, extension);
    
    // Convertir en FileInfo
    std::vector<FileInfo> files;
    files.reserve(filePaths.size());
    
    for (const auto& path : filePaths) {
        files.push_back(parseFileInfo(path, dirType));
    }
    
    // Trier par date de modification (plus récent en premier)
    std::sort(files.begin(), files.end(),
        [](const FileInfo& a, const FileInfo& b) {
            return a.modified > b.modified;
        });
    
    Logger::debug("FileManager", "Found " + std::to_string(files.size()) + " files");
    
    return files;
}

std::string FileManager::getDirectoryPath(DirectoryType dirType) const {
    std::string dirName = directoryTypeToString(dirType);
    return rootPath_ + "/" + dirName;
}

std::optional<FileInfo> FileManager::getFileInfo(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!validatePath(filepath)) {
        Logger::warn("FileManager", "Invalid path: " + filepath);
        return std::nullopt;
    }
    
    if (!FileSystem::exists(filepath)) {
        return std::nullopt;
    }
    
    // Déterminer le type de répertoire
    DirectoryType dirType = DirectoryType::TEMP; // Par défaut
    
    if (filepath.find(DIR_LOGS) != std::string::npos) {
        dirType = DirectoryType::LOGS;
    } else if (filepath.find(DIR_BACKUPS) != std::string::npos) {
        dirType = DirectoryType::BACKUPS;
    } else if (filepath.find(DIR_EXPORTS) != std::string::npos) {
        dirType = DirectoryType::EXPORTS;
    } else if (filepath.find(DIR_UPLOADS) != std::string::npos) {
        dirType = DirectoryType::UPLOADS;
    }
    
    return parseFileInfo(filepath, dirType);
}

// ============================================================================
// UPLOAD/DOWNLOAD
// ============================================================================

std::string FileManager::uploadFile(const std::vector<uint8_t>& data,
                                    const std::string& filename,
                                    DirectoryType destDir,
                                    bool overwrite) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validation taille
    if (data.size() > MAX_UPLOAD_SIZE) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR,
                   "File too large (max " + std::to_string(MAX_UPLOAD_SIZE) + " bytes)");
    }
    
    // Sécuriser le nom de fichier
    std::string safeName = sanitizeFilename(filename);
    if (safeName.empty()) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR, "Invalid filename");
    }
    
    // Construire le chemin destination
    std::string destPath = getDirectoryPath(destDir) + "/" + safeName;
    
    // Vérifier si le fichier existe
    if (!overwrite && FileSystem::exists(destPath)) {
        THROW_ERROR(ErrorCode::FILE_ALREADY_EXISTS,
                   "File already exists: " + safeName);
    }
    
    Logger::info("FileManager", "Uploading file: " + safeName + 
                " (" + std::to_string(data.size()) + " bytes)");
    
    // Écrire le fichier
    if (!FileSystem::writeBinaryFile(destPath, data)) {
        THROW_ERROR(ErrorCode::FILE_WRITE_FAILED,
                   "Failed to write file: " + destPath);
    }
    
    Logger::info("FileManager", "✓ File uploaded: " + destPath);
    
    return destPath;
}

std::vector<uint8_t> FileManager::downloadFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Valider le chemin
    if (!validatePath(filepath)) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR,
                   "Invalid or unsafe path: " + filepath);
    }
    
    if (!FileSystem::exists(filepath)) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND,
                   "File not found: " + filepath);
    }
    
    Logger::debug("FileManager", "Downloading file: " + filepath);
    
    // Lire le fichier
    auto data = FileSystem::readBinaryFile(filepath);
    
    if (data.empty()) {
        THROW_ERROR(ErrorCode::FILE_READ_FAILED,
                   "Failed to read file: " + filepath);
    }
    
    Logger::debug("FileManager", "✓ File downloaded (" + 
                 std::to_string(data.size()) + " bytes)");
    
    return data;
}

// ============================================================================
// OPÉRATIONS FICHIERS
// ============================================================================

bool FileManager::copyFile(const std::string& source, 
                           const std::string& destination,
                           bool overwrite) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Valider les chemins
    if (!validatePath(source) || !validatePath(destination)) {
        Logger::warn("FileManager", "Invalid path in copy operation");
        return false;
    }
    
    // Vérifier que la source existe
    if (!FileSystem::exists(source)) {
        Logger::warn("FileManager", "Source file not found: " + source);
        return false;
    }
    
    // Vérifier la destination
    if (!overwrite && FileSystem::exists(destination)) {
        Logger::warn("FileManager", "Destination already exists: " + destination);
        return false;
    }
    
    Logger::debug("FileManager", "Copying: " + source + " -> " + destination);
    
    // Copier
    if (!FileSystem::copyFile(source, destination)) {
        Logger::error("FileManager", "Failed to copy file");
        return false;
    }
    
    Logger::debug("FileManager", "✓ File copied");
    return true;
}

bool FileManager::moveFile(const std::string& source, 
                           const std::string& destination) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Valider les chemins
    if (!validatePath(source) || !validatePath(destination)) {
        Logger::warn("FileManager", "Invalid path in move operation");
        return false;
    }
    
    if (!FileSystem::exists(source)) {
        Logger::warn("FileManager", "Source file not found: " + source);
        return false;
    }
    
    Logger::debug("FileManager", "Moving: " + source + " -> " + destination);
    
    // Déplacer
    if (!FileSystem::moveFile(source, destination)) {
        Logger::error("FileManager", "Failed to move file");
        return false;
    }
    
    Logger::debug("FileManager", "✓ File moved");
    return true;
}

bool FileManager::deleteFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Valider le chemin
    if (!validatePath(filepath)) {
        Logger::warn("FileManager", "Invalid path: " + filepath);
        return false;
    }
    
    if (!FileSystem::exists(filepath)) {
        // Pas d'erreur si le fichier n'existe pas
        return true;
    }
    
    Logger::debug("FileManager", "Deleting file: " + filepath);
    
    if (!FileSystem::removeFile(filepath)) {
        Logger::error("FileManager", "Failed to delete file");
        return false;
    }
    
    Logger::debug("FileManager", "✓ File deleted");
    return true;
}

int FileManager::clearDirectory(DirectoryType dirType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    std::string dirName = directoryTypeToString(dirType);
    
    Logger::warn("FileManager", "Clearing directory: " + dirName);
    
    auto files = FileSystem::listFiles(dirPath);
    int deleted = 0;
    
    for (const auto& file : files) {
        if (FileSystem::removeFile(file)) {
            deleted++;
        }
    }
    
    Logger::info("FileManager", "✓ Cleared " + dirName + 
                " (" + std::to_string(deleted) + " files deleted)");
    
    return deleted;
}

// ============================================================================
// NETTOYAGE AUTOMATIQUE
// ============================================================================

int FileManager::cleanOldLogs(int maxAgeDays) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Cleaning old logs (max age: " + 
                std::to_string(maxAgeDays) + " days)");
    
    std::string logsDir = getDirectoryPath(DirectoryType::LOGS);
    auto files = FileSystem::listFiles(logsDir, ".log");
    
    if (files.size() <= 5) {
        Logger::debug("FileManager", "Keeping all logs (less than 5)");
        return 0;
    }
    
    // Calculer la date limite
    std::time_t now = std::time(nullptr);
    std::time_t cutoff = now - (maxAgeDays * 24 * 3600);
    
    // Collecter les fichiers à supprimer
    std::vector<std::string> toDelete;
    
    for (const auto& file : files) {
        auto info = parseFileInfo(file, DirectoryType::LOGS);
        
        if (info.modified < cutoff) {
            toDelete.push_back(file);
        }
    }
    
    // Garder au moins les 5 plus récents
    if (files.size() - toDelete.size() < 5) {
        size_t keepCount = 5;
        size_t deleteCount = files.size() > keepCount ? files.size() - keepCount : 0;
        toDelete.resize(deleteCount);
    }
    
    // Supprimer les fichiers
    int deleted = 0;
    for (const auto& file : toDelete) {
        if (FileSystem::removeFile(file)) {
            deleted++;
            Logger::debug("FileManager", "  Deleted: " + FileSystem::getFileName(file));
        }
    }
    
    Logger::info("FileManager", "✓ Cleaned " + std::to_string(deleted) + " old logs");
    
    return deleted;
}

int FileManager::cleanOldBackups(int maxCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Cleaning old backups (keep " + 
                std::to_string(maxCount) + " most recent)");
    
    std::string backupsDir = getDirectoryPath(DirectoryType::BACKUPS);
    auto files = FileSystem::listFiles(backupsDir);
    
    if (static_cast<int>(files.size()) <= maxCount) {
        Logger::debug("FileManager", "No cleanup needed");
        return 0;
    }
    
    // Trier par date (plus récent en premier)
    std::vector<FileInfo> backups;
    for (const auto& file : files) {
        backups.push_back(parseFileInfo(file, DirectoryType::BACKUPS));
    }
    
    std::sort(backups.begin(), backups.end(),
        [](const FileInfo& a, const FileInfo& b) {
            return a.modified > b.modified;
        });
    
    // Supprimer les anciens
    int deleted = 0;
    for (size_t i = maxCount; i < backups.size(); i++) {
        if (FileSystem::removeFile(backups[i].path)) {
            deleted++;
            Logger::debug("FileManager", "  Deleted: " + backups[i].name);
        }
    }
    
    Logger::info("FileManager", "✓ Cleaned " + std::to_string(deleted) + " old backups");
    
    return deleted;
}

int FileManager::cleanTemp() {
    Logger::info("FileManager", "Cleaning temp directory...");
    
    int deleted = clearDirectory(DirectoryType::TEMP);
    
    Logger::info("FileManager", "✓ Temp directory cleaned (" + 
                std::to_string(deleted) + " files)");
    
    return deleted;
}

// ============================================================================
// STATISTIQUES
// ============================================================================

uint64_t FileManager::getDirectorySize(DirectoryType dirType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    auto files = FileSystem::listFiles(dirPath);
    
    uint64_t totalSize = 0;
    
    for (const auto& file : files) {
        totalSize += FileSystem::getFileSize(file);
    }
    
    return totalSize;
}

int FileManager::getFileCount(DirectoryType dirType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    auto files = FileSystem::listFiles(dirPath);
    
    return static_cast<int>(files.size());
}

json FileManager::getStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["root_path"] = rootPath_;
    
    // Stats pour chaque répertoire
    std::vector<DirectoryType> dirTypes = {
        DirectoryType::LOGS,
        DirectoryType::BACKUPS,
        DirectoryType::EXPORTS,
        DirectoryType::UPLOADS,
        DirectoryType::TEMP
    };
    
    uint64_t totalSize = 0;
    
    for (auto dirType : dirTypes) {
        std::string dirName = directoryTypeToString(dirType);
        std::string dirPath = getDirectoryPath(dirType);
        
        auto files = FileSystem::listFiles(dirPath);
        uint64_t size = 0;
        
        for (const auto& file : files) {
            size += FileSystem::getFileSize(file);
        }
        
        stats[dirName] = {
            {"files", files.size()},
            {"size", size}
        };
        
        totalSize += size;
    }
    
    stats["total_size"] = totalSize;
    
    return stats;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

bool FileManager::validatePath(const std::string& path) const {
    // Vérifier que le chemin n'est pas vide
    if (path.empty()) {
        return false;
    }
    
    // Vérifier qu'il n'y a pas de ".." (path traversal)
    if (path.find("..") != std::string::npos) {
        Logger::warn("FileManager", "Path traversal detected: " + path);
        return false;
    }
    
    // Vérifier que le chemin est sous rootPath_
    // (si c'est un chemin absolu)
    if (path[0] == '/' && path.find(rootPath_) != 0) {
        Logger::warn("FileManager", "Path outside root: " + path);
        return false;
    }
    
    return true;
}

std::string FileManager::sanitizeFilename(const std::string& filename) const {
    if (filename.empty()) {
        return "";
    }
    
    std::string safe = filename;
    
    // Caractères dangereux à remplacer
    const std::string dangerous = "\\/:*?\"<>|";
    
    for (char& c : safe) {
        if (dangerous.find(c) != std::string::npos) {
            c = '_';
        }
    }
    
    // Supprimer ".." pour éviter path traversal
    size_t pos;
    while ((pos = safe.find("..")) != std::string::npos) {
        safe.replace(pos, 2, "__");
    }
    
    return safe;
}

std::string FileManager::directoryTypeToString(DirectoryType dirType) const {
    switch (dirType) {
        case DirectoryType::LOGS:    return DIR_LOGS;
        case DirectoryType::BACKUPS: return DIR_BACKUPS;
        case DirectoryType::EXPORTS: return DIR_EXPORTS;
        case DirectoryType::UPLOADS: return DIR_UPLOADS;
        case DirectoryType::TEMP:    return DIR_TEMP;
        default:                     return "unknown";
    }
}

FileInfo FileManager::parseFileInfo(const std::string& filepath, DirectoryType dirType) {
    FileInfo info;
    
    info.path = filepath;
    info.name = FileSystem::getFileName(filepath);
    info.extension = FileSystem::getExtension(filepath);
    info.size = FileSystem::getFileSize(filepath);
    info.modified = FileSystem::getModifiedTime(filepath);
    info.directory = directoryTypeToString(dirType);
    
    return info;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER FileManager.cpp
// ============================================================================

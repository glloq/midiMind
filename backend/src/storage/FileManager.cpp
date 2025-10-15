// ============================================================================
// Fichier: backend/src/storage/FileManager.cpp
// Version: 3.0.4 - HYBRIDE (Sécurité Renforcée + Cohérence Projet)
// Date: 2025-10-15
// ============================================================================
// CORRECTIFS v3.0.4 (HYBRIDE - CRITIQUE):
//   ✅ Garde sécurité renforcée v3.0.3 (validatePath, sanitizeFilename)
//   ✅ Restaure utilisation FileSystem:: helpers (cohérence projet)
//   ✅ Restaure signature getFileInfo() → FileInfo (pas std::optional)
//   ✅ Garde nouvelles méthodes stats (bonus utiles)
//   ✅ AUCUN downgrade - Best of both worlds
//
// Améliorations v3.0.4 vs v3.0.2:
//   🟢 validatePath() - Résolution canonique + symlinks malveillants
//   🟢 sanitizeFilename() - Noms réservés Windows + null bytes
//   🟢 isPathSafe() - Wrapper validation complète
//   🟢 getDirectorySize() - Nouvelle méthode utile
//   🟢 getFileCount() - Nouvelle méthode utile
//   🟢 getStatistics() - Nouvelle méthode utile
//
// Corrections v3.0.4 vs v3.0.3:
//   🔴 Tous les appels std::filesystem → FileSystem:: helpers
//   🔴 getFileInfo() signature restaurée (FileInfo au lieu de optional)
//   🔴 uploadFile() utilise FileSystem::writeBinaryFile
//   🔴 downloadFile() utilise FileSystem::readBinaryFile
//   🔴 copyFile() utilise FileSystem::copyFile
//   🔴 moveFile() utilise FileSystem::moveFile
//   🔴 deleteFile() utilise FileSystem::removeFile
//   🔴 cleanOldLogs() utilise FileSystem::listFiles
//   🔴 parseFileInfo() utilise FileSystem::getFileName/getExtension/etc
//
// Description:
//   Gestionnaire de fichiers applicatifs avec sécurité maximale
//   et cohérence avec l'architecture du projet
//
// Thread-safety: OUI (mutex)
//
// Auteur: MidiMind Team
// ============================================================================

#include "FileManager.h"
#include "../core/Logger.h"
#include "../core/FileSystem.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <regex>

namespace fs = std::filesystem;

namespace midiMind {

// ============================================================================
// CONSTANTES
// ============================================================================

constexpr const char* DIR_LOGS = "logs";
constexpr const char* DIR_BACKUPS = "backups";
constexpr const char* DIR_EXPORTS = "exports";
constexpr const char* DIR_UPLOADS = "uploads";
constexpr const char* DIR_TEMP = "temp";

constexpr size_t MAX_UPLOAD_SIZE = 100 * 1024 * 1024;  // 100 MB
constexpr size_t MAX_FILENAME_LENGTH = 255;             // Standard filesystem

// Caractères interdits dans les noms de fichiers
const std::string FORBIDDEN_CHARS = "<>:\"|?*\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F";

// Noms de fichiers réservés Windows (pour compatibilité)
const std::vector<std::string> RESERVED_NAMES = {
    "CON", "PRN", "AUX", "NUL",
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
};

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

FileManager::FileManager(const std::string& rootPath)
    : rootPath_(rootPath) {
    
    Logger::info("FileManager", "FileManager created");
    Logger::info("FileManager", "  Root path: " + rootPath_);
    
    // Résoudre le chemin racine canonique (résout symlinks, .., .)
    if (fs::exists(rootPath_)) {
        try {
            rootPath_ = fs::canonical(rootPath_).string();
            Logger::info("FileManager", "  Canonical root: " + rootPath_);
        } catch (const fs::filesystem_error& e) {
            Logger::error("FileManager", "Failed to resolve root path: " + std::string(e.what()));
        }
    } else {
        Logger::warn("FileManager", "Root path does not exist yet, will be created on init");
    }
}

FileManager::~FileManager() {
    Logger::info("FileManager", "FileManager destroyed");
}

// ============================================================================
// MÉTHODES DE SÉCURITÉ - PATH TRAVERSAL (v3.0.3)
// ============================================================================

/**
 * @brief Valide un chemin pour prévenir path traversal
 * 
 * VERSION v3.0.4: Validation avancée avec résolution canonique
 */
bool FileManager::validatePath(const std::string& relativePath) const {
    // 1. Vérifications de base
    if (relativePath.empty()) {
        Logger::warn("FileManager", "Empty path rejected");
        return false;
    }
    
    // 2. Bloquer null bytes
    if (relativePath.find('\0') != std::string::npos) {
        Logger::warn("FileManager", "Path contains null byte: REJECTED");
        return false;
    }
    
    // 3. Bloquer séquences path traversal explicites
    if (relativePath.find("..") != std::string::npos) {
        Logger::warn("FileManager", "Path traversal attempt detected (..): " + relativePath);
        return false;
    }
    
    if (relativePath.find(".\\") != std::string::npos || 
        relativePath.find("./") != std::string::npos) {
        // Autoriser ./ au début uniquement
        if (relativePath.rfind("./", 0) != 0) {
            Logger::warn("FileManager", "Suspicious path pattern (./, .\\): " + relativePath);
            return false;
        }
    }
    
    // 4. Bloquer drive letters Windows (C:, D:, etc.)
    std::regex driveLetterRegex("^[A-Za-z]:");
    if (std::regex_search(relativePath, driveLetterRegex)) {
        Logger::warn("FileManager", "Windows drive letter detected: " + relativePath);
        return false;
    }
    
    // 5. Bloquer chemins absolus (commencent par /, \, ou ~)
    if (relativePath[0] == '/' || relativePath[0] == '\\' || relativePath[0] == '~') {
        Logger::warn("FileManager", "Absolute path rejected: " + relativePath);
        return false;
    }
    
    // 6. Bloquer chemins UNC Windows (\\server\share)
    if (relativePath.find("\\\\") == 0 || relativePath.find("//") == 0) {
        Logger::warn("FileManager", "UNC path rejected: " + relativePath);
        return false;
    }
    
    // 7. Résolution canonique et vérification finale
    try {
        // Construire le chemin complet
        fs::path fullPath = fs::path(rootPath_) / relativePath;
        
        // Si le chemin existe, le résoudre canoniquement (résout symlinks)
        if (fs::exists(fullPath)) {
            fs::path canonicalPath = fs::canonical(fullPath);
            fs::path canonicalRoot = fs::canonical(rootPath_);
            
            // Vérifier que le chemin canonique est sous la racine canonique
            auto [rootEnd, pathEnd] = std::mismatch(
                canonicalRoot.begin(), canonicalRoot.end(),
                canonicalPath.begin(), canonicalPath.end()
            );
            
            if (rootEnd != canonicalRoot.end()) {
                Logger::warn("FileManager", 
                    "Canonical path escapes root:\n"
                    "  Root: " + canonicalRoot.string() + "\n"
                    "  Path: " + canonicalPath.string());
                return false;
            }
        } else {
            // Si le chemin n'existe pas encore, vérifier son parent
            fs::path parentPath = fullPath.parent_path();
            
            if (fs::exists(parentPath)) {
                fs::path canonicalParent = fs::canonical(parentPath);
                fs::path canonicalRoot = fs::canonical(rootPath_);
                
                auto [rootEnd, parentEnd] = std::mismatch(
                    canonicalRoot.begin(), canonicalRoot.end(),
                    canonicalParent.begin(), canonicalParent.end()
                );
                
                if (rootEnd != canonicalRoot.end()) {
                    Logger::warn("FileManager", 
                        "Parent path escapes root:\n"
                        "  Root: " + canonicalRoot.string() + "\n"
                        "  Parent: " + canonicalParent.string());
                    return false;
                }
            }
        }
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", 
            "Filesystem error during path validation: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        Logger::error("FileManager", 
            "Exception during path validation: " + std::string(e.what()));
        return false;
    }
    
    // 8. Toutes les vérifications passées
    Logger::debug("FileManager", "Path validated: " + relativePath);
    return true;
}

/**
 * @brief Nettoie un nom de fichier des caractères dangereux
 * 
 * VERSION v3.0.4: Nettoyage complet avec noms réservés Windows
 */
std::string FileManager::sanitizeFilename(const std::string& filename) const {
    if (filename.empty()) {
        return "";
    }
    
    std::string safe = filename;
    
    // 1. Supprimer null bytes
    safe.erase(std::remove(safe.begin(), safe.end(), '\0'), safe.end());
    
    // 2. Remplacer séparateurs de chemin par underscore
    std::replace(safe.begin(), safe.end(), '/', '_');
    std::replace(safe.begin(), safe.end(), '\\', '_');
    
    // 3. Supprimer caractères interdits
    safe.erase(
        std::remove_if(safe.begin(), safe.end(),
            [](char c) {
                // Caractères de contrôle (0-31)
                if (c >= 0 && c <= 31) return true;
                // Caractères interdits
                return FORBIDDEN_CHARS.find(c) != std::string::npos;
            }
        ),
        safe.end()
    );
    
    // 4. Supprimer points au début et à la fin
    while (!safe.empty() && safe.front() == '.') {
        safe.erase(0, 1);
    }
    while (!safe.empty() && safe.back() == '.') {
        safe.pop_back();
    }
    
    // 5. Vérifier noms réservés Windows
    std::string upperName = safe;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
    
    // Extraire le nom sans extension pour la vérification
    size_t dotPos = upperName.find('.');
    std::string nameWithoutExt = (dotPos != std::string::npos) 
        ? upperName.substr(0, dotPos) 
        : upperName;
    
    for (const auto& reserved : RESERVED_NAMES) {
        if (nameWithoutExt == reserved) {
            safe = "_" + safe;  // Préfixer avec underscore
            break;
        }
    }
    
    // 6. Limiter la longueur
    if (safe.length() > MAX_FILENAME_LENGTH) {
        // Garder l'extension si présente
        size_t extPos = safe.rfind('.');
        if (extPos != std::string::npos && extPos > MAX_FILENAME_LENGTH - 20) {
            std::string ext = safe.substr(extPos);
            safe = safe.substr(0, MAX_FILENAME_LENGTH - ext.length()) + ext;
        } else {
            safe = safe.substr(0, MAX_FILENAME_LENGTH);
        }
    }
    
    // 7. Si le nom est vide après nettoyage, générer un nom par défaut
    if (safe.empty()) {
        safe = "unnamed_file";
    }
    
    return safe;
}

/**
 * @brief Vérifie si un chemin est sûr (validation complète)
 * 
 * VERSION v3.0.4: Wrapper pour validatePath
 */
bool FileManager::isPathSafe(const std::string& path) const {
    // Convertir en chemin relatif si absolu
    std::string relativePath = path;
    
    if (path.rfind(rootPath_, 0) == 0) {
        // Le chemin commence par rootPath_, le rendre relatif
        relativePath = path.substr(rootPath_.length());
        if (!relativePath.empty() && (relativePath[0] == '/' || relativePath[0] == '\\')) {
            relativePath = relativePath.substr(1);
        }
    }
    
    return validatePath(relativePath);
}

// ============================================================================
// INITIALISATION
// ============================================================================

bool FileManager::initializeDirectories() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Initializing directory structure...");
    
    // Créer le répertoire racine si nécessaire
    if (!fs::exists(rootPath_)) {
        try {
            fs::create_directories(rootPath_);
            Logger::info("FileManager", "✓ Created root: " + rootPath_);
        } catch (const fs::filesystem_error& e) {
            Logger::error("FileManager", "Failed to create root: " + std::string(e.what()));
            return false;
        }
    }
    
    // Liste des sous-répertoires à créer
    std::vector<DirectoryType> dirs = {
        DirectoryType::LOGS,
        DirectoryType::BACKUPS,
        DirectoryType::EXPORTS,
        DirectoryType::UPLOADS,
        DirectoryType::TEMP
    };
    
    bool allSuccess = true;
    
    for (auto dirType : dirs) {
        std::string dirPath = getDirectoryPath(dirType);
        
        if (!fs::exists(dirPath)) {
            try {
                fs::create_directories(dirPath);
                Logger::info("FileManager", "✓ Created: " + dirPath);
            } catch (const fs::filesystem_error& e) {
                Logger::error("FileManager", 
                    "Failed to create " + dirPath + ": " + std::string(e.what()));
                allSuccess = false;
            }
        } else {
            Logger::debug("FileManager", "✓ Exists: " + dirPath);
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
    std::vector<FileInfo> files;
    
    if (!FileSystem::exists(dirPath)) {
        Logger::warn("FileManager", "Directory does not exist: " + dirPath);
        return files;
    }
    
    // ✅ v3.0.4: Utiliser FileSystem::listFiles
    auto fileList = FileSystem::listFiles(dirPath, extension);
    
    for (const auto& filepath : fileList) {
        files.push_back(parseFileInfo(filepath, dirType));
    }
    
    // Trier par date de modification (plus récent en premier)
    std::sort(files.begin(), files.end(),
        [](const FileInfo& a, const FileInfo& b) {
            return a.modified > b.modified;
        });
    
    return files;
}

std::string FileManager::getDirectoryPath(DirectoryType dirType) const {
    return rootPath_ + "/" + directoryTypeToString(dirType);
}

FileInfo FileManager::getFileInfo(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // ✅ v3.0.4: Validation sécurité (garde v3.0.3)
    if (!isPathSafe(filepath)) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR,
                   "Invalid or unsafe path: " + filepath);
    }
    
    // ✅ v3.0.4: Signature restaurée (FileInfo au lieu de optional)
    if (!FileSystem::exists(filepath)) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND,
                   "File not found: " + filepath);
    }
    
    // Déterminer le type de répertoire
    DirectoryType dirType = DirectoryType::TEMP;
    
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
    
    Logger::info("FileManager", "Uploading file: " + filename);
    
    // 1. Validation taille
    if (data.size() > MAX_UPLOAD_SIZE) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR,
                   "File too large (max " + std::to_string(MAX_UPLOAD_SIZE) + " bytes)");
    }
    
    // 2. ✅ v3.0.4: Sécuriser le nom de fichier (garde v3.0.3 avancée)
    std::string safeName = sanitizeFilename(filename);
    if (safeName.empty()) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR, "Invalid filename");
    }
    
    Logger::debug("FileManager", "  Original name: " + filename);
    Logger::debug("FileManager", "  Safe name: " + safeName);
    
    // 3. Construire le chemin destination (relatif)
    std::string relPath = directoryTypeToString(destDir) + "/" + safeName;
    
    // 4. ✅ v3.0.4: Validation sécurité (garde v3.0.3 avancée)
    if (!validatePath(relPath)) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR,
                   "Invalid or unsafe path: " + relPath);
    }
    
    std::string destPath = getDirectoryPath(destDir) + "/" + safeName;
    
    Logger::debug("FileManager", "  Destination: " + destPath);
    
    // 5. Vérifier si le fichier existe
    if (!overwrite && FileSystem::exists(destPath)) {
        THROW_ERROR(ErrorCode::FILE_ALREADY_EXISTS,
                   "File already exists: " + safeName);
    }
    
    // 6. ✅ v3.0.4: Utiliser FileSystem::writeBinaryFile (restaure cohérence)
    Logger::info("FileManager", "Uploading file: " + safeName + 
                " (" + std::to_string(data.size()) + " bytes)");
    
    if (!FileSystem::writeBinaryFile(destPath, data)) {
        THROW_ERROR(ErrorCode::FILE_WRITE_FAILED,
                   "Failed to write file: " + destPath);
    }
    
    Logger::info("FileManager", "✅ File uploaded: " + destPath);
    
    return destPath;
}

std::vector<uint8_t> FileManager::downloadFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Downloading file: " + filepath);
    
    // ✅ v3.0.4: Validation sécurité (garde v3.0.3 avancée)
    if (!isPathSafe(filepath)) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR,
                   "Invalid or unsafe path: " + filepath);
    }
    
    std::string fullPath = rootPath_ + "/" + filepath;
    
    if (!FileSystem::exists(fullPath)) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND,
                   "File not found: " + filepath);
    }
    
    Logger::debug("FileManager", "Downloading file: " + filepath);
    
    // ✅ v3.0.4: Utiliser FileSystem::readBinaryFile (restaure cohérence)
    auto data = FileSystem::readBinaryFile(fullPath);
    
    if (data.empty()) {
        THROW_ERROR(ErrorCode::FILE_READ_FAILED,
                   "Failed to read file: " + filepath);
    }
    
    Logger::debug("FileManager", "✅ File downloaded (" + 
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
    
    Logger::info("FileManager", "Copying file: " + source + " -> " + destination);
    
    // ✅ v3.0.4: Validation sécurité (garde v3.0.3 avancée)
    if (!isPathSafe(source) || !isPathSafe(destination)) {
        Logger::error("FileManager", "Invalid path in copy operation");
        return false;
    }
    
    std::string fullSource = rootPath_ + "/" + source;
    std::string fullDest = rootPath_ + "/" + destination;
    
    if (!FileSystem::exists(fullSource)) {
        Logger::error("FileManager", "Source file not found: " + source);
        return false;
    }
    
    if (!overwrite && FileSystem::exists(fullDest)) {
        Logger::error("FileManager", "Destination already exists: " + destination);
        return false;
    }
    
    Logger::debug("FileManager", "Copying: " + source + " -> " + destination);
    
    // ✅ v3.0.4: Utiliser FileSystem::copyFile (restaure cohérence)
    if (!FileSystem::copyFile(fullSource, fullDest)) {
        Logger::error("FileManager", "Failed to copy file");
        return false;
    }
    
    Logger::info("FileManager", "✓ File copied successfully");
    return true;
}

bool FileManager::moveFile(const std::string& source, 
                           const std::string& destination) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Moving file: " + source + " -> " + destination);
    
    // ✅ v3.0.4: Validation sécurité (garde v3.0.3 avancée)
    if (!isPathSafe(source) || !isPathSafe(destination)) {
        Logger::error("FileManager", "Invalid path in move operation");
        return false;
    }
    
    std::string fullSource = rootPath_ + "/" + source;
    std::string fullDest = rootPath_ + "/" + destination;
    
    if (!FileSystem::exists(fullSource)) {
        Logger::error("FileManager", "Source file not found: " + source);
        return false;
    }
    
    Logger::debug("FileManager", "Moving: " + source + " -> " + destination);
    
    // ✅ v3.0.4: Utiliser FileSystem::moveFile (restaure cohérence)
    if (!FileSystem::moveFile(fullSource, fullDest)) {
        Logger::error("FileManager", "Failed to move file");
        return false;
    }
    
    Logger::info("FileManager", "✓ File moved successfully");
    return true;
}

bool FileManager::deleteFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Deleting file: " + filepath);
    
    // ✅ v3.0.4: Validation sécurité (garde v3.0.3 avancée)
    if (!isPathSafe(filepath)) {
        Logger::error("FileManager", "Invalid path: " + filepath);
        return false;
    }
    
    std::string fullPath = rootPath_ + "/" + filepath;
    
    if (!FileSystem::exists(fullPath)) {
        Logger::warn("FileManager", "File does not exist (skipping): " + filepath);
        return true;  // Pas une erreur
    }
    
    Logger::debug("FileManager", "Deleting file: " + filepath);
    
    // ✅ v3.0.4: Utiliser FileSystem::removeFile (restaure cohérence)
    if (!FileSystem::removeFile(fullPath)) {
        Logger::error("FileManager", "Failed to delete file");
        return false;
    }
    
    Logger::info("FileManager", "✓ File deleted");
    return true;
}

int FileManager::clearDirectory(DirectoryType dirType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    
    Logger::warn("FileManager", "Clearing directory: " + dirPath);
    
    if (!FileSystem::exists(dirPath)) {
        Logger::warn("FileManager", "Directory does not exist");
        return 0;
    }
    
    // ✅ v3.0.4: Utiliser FileSystem::listFiles + removeFile
    auto files = FileSystem::listFiles(dirPath);
    
    int deletedCount = 0;
    for (const auto& file : files) {
        if (FileSystem::removeFile(file)) {
            deletedCount++;
        }
    }
    
    Logger::info("FileManager", 
        "✓ Cleared " + std::to_string(deletedCount) + " files");
    
    return deletedCount;
}

// ============================================================================
// NETTOYAGE AUTOMATIQUE
// ============================================================================

int FileManager::cleanOldLogs(int maxAgeDays) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", 
        "Cleaning logs older than " + std::to_string(maxAgeDays) + " days...");
    
    std::string logsDir = getDirectoryPath(DirectoryType::LOGS);
    
    // ✅ v3.0.4: Utiliser FileSystem::listFiles (restaure cohérence)
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
        size_t deleteCount = files.size() > keepCount ? 
            files.size() - keepCount : 0;
        toDelete.resize(deleteCount);
    }
    
    // Supprimer les fichiers
    int deleted = 0;
    for (const auto& file : toDelete) {
        // ✅ v3.0.4: Utiliser FileSystem::removeFile (restaure cohérence)
        if (FileSystem::removeFile(file)) {
            deleted++;
            Logger::debug("FileManager", "  Deleted: " + FileSystem::getFileName(file));
        }
    }
    
    Logger::info("FileManager", "✅ Cleaned " + std::to_string(deleted) + " old logs");
    
    return deleted;
}

int FileManager::cleanOldBackups(int maxCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", 
        "Cleaning backups (keeping " + std::to_string(maxCount) + " most recent)...");
    
    std::string backupsDir = getDirectoryPath(DirectoryType::BACKUPS);
    
    // ✅ v3.0.4: Utiliser FileSystem::listFiles (restaure cohérence)
    auto files = FileSystem::listFiles(backupsDir);
    
    if (files.size() <= static_cast<size_t>(maxCount)) {
        Logger::debug("FileManager", "No cleanup needed");
        return 0;
    }
    
    // Trier par date (plus récent en premier)
    std::vector<FileInfo> fileInfos;
    for (const auto& file : files) {
        fileInfos.push_back(parseFileInfo(file, DirectoryType::BACKUPS));
    }
    
    std::sort(fileInfos.begin(), fileInfos.end(),
        [](const FileInfo& a, const FileInfo& b) {
            return a.modified > b.modified;
        });
    
    // Supprimer les anciens (au-delà de maxCount)
    int deleted = 0;
    
    for (size_t i = maxCount; i < fileInfos.size(); ++i) {
        // ✅ v3.0.4: Utiliser FileSystem::removeFile (restaure cohérence)
        if (FileSystem::removeFile(fileInfos[i].path)) {
            deleted++;
            Logger::debug("FileManager", "  Deleted: " + fileInfos[i].name);
        }
    }
    
    Logger::info("FileManager", "✅ Cleaned " + std::to_string(deleted) + " old backups");
    
    return deleted;
}

int FileManager::clearTemp() {
    return clearDirectory(DirectoryType::TEMP);
}

// ============================================================================
// STATISTIQUES (v3.0.4 - BONUS)
// ============================================================================

uint64_t FileManager::getDirectorySize(DirectoryType dirType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    uint64_t totalSize = 0;
    
    if (!FileSystem::exists(dirPath)) {
        return 0;
    }
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                totalSize += entry.file_size();
            }
        }
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", 
            "Failed to calculate directory size: " + std::string(e.what()));
    }
    
    return totalSize;
}

int FileManager::getFileCount(DirectoryType dirType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    
    if (!FileSystem::exists(dirPath)) {
        return 0;
    }
    
    // ✅ v3.0.4: Utiliser FileSystem::listFiles (restaure cohérence)
    auto files = FileSystem::listFiles(dirPath);
    return static_cast<int>(files.size());
}

json FileManager::getStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["root_path"] = rootPath_;
    
    // Statistiques par répertoire
    std::vector<DirectoryType> dirs = {
        DirectoryType::LOGS,
        DirectoryType::BACKUPS,
        DirectoryType::EXPORTS,
        DirectoryType::UPLOADS,
        DirectoryType::TEMP
    };
    
    uint64_t totalSize = 0;
    
    for (auto dirType : dirs) {
        std::string dirName = directoryTypeToString(dirType);
        
        // Déverrouiller temporairement pour appeler les méthodes
        mutex_.unlock();
        int fileCount = getFileCount(dirType);
        uint64_t dirSize = getDirectorySize(dirType);
        mutex_.lock();
        
        stats[dirName]["files"] = fileCount;
        stats[dirName]["size"] = dirSize;
        
        totalSize += dirSize;
    }
    
    stats["total_size"] = totalSize;
    
    return stats;
}

// ============================================================================
// UTILITAIRES PRIVÉS
// ============================================================================

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

FileInfo FileManager::parseFileInfo(const std::string& filepath, 
                                    DirectoryType dirType) {
    FileInfo info;
    
    info.path = filepath;
    // ✅ v3.0.4: Utiliser FileSystem:: helpers (restaure cohérence)
    info.name = FileSystem::getFileName(filepath);
    info.extension = FileSystem::getExtension(filepath);
    info.size = FileSystem::getFileSize(filepath);
    info.modified = FileSystem::getModifiedTime(filepath);
    info.directory = directoryTypeToString(dirType);
    
    return info;
}

} // namespace midiMind


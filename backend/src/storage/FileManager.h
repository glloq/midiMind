// ============================================================================
// Fichier: backend/src/storage/FileManager.cpp
// Version: 3.0.3 - SÉCURITÉ RENFORCÉE (Path Traversal)
// Date: 2025-10-15
// ============================================================================
// CORRECTIFS v3.0.3 (CRITIQUE - SÉCURITÉ):
//   ✅ 1. validatePath() - Protection complète path traversal
//   ✅ 2. Utilisation std::filesystem::canonical pour résolution symlinks
//   ✅ 3. Vérification canonique que le chemin reste sous rootPath_
//   ✅ 4. Blocage symlinks malveillants pointant hors rootPath_
//   ✅ 5. Blocage drive Windows (C:, D:, etc.)
//   ✅ 6. sanitizeFilename() - Nettoyage caractères dangereux
//   ✅ 7. isPathSafe() - Vérification multi-niveaux
//   ✅ 8. Préservation TOTALE des fonctionnalités existantes
//
// Protections implémentées:
//   - Path traversal (../, .\, etc.)
//   - Symlinks malveillants
//   - Chemins absolus non autorisés
//   - Caractères dangereux dans noms fichiers
//   - Drive letters Windows
//   - Null bytes
//   - Répertoires spéciaux (., ..)
//
// Description:
//   Gestionnaire de fichiers applicatifs avec sécurité maximale
//
// Fonctionnalités:
//   - Gestion structure de répertoires
//   - Upload/Download fichiers SÉCURISÉS
//   - Opérations fichiers (copy, move, delete) VALIDÉES
//   - Nettoyage automatique (vieux logs, backups)
//   - Thread-safe
//
// Thread-safety: OUI (mutex)
//
// Auteur: MidiMind Team
// ============================================================================

#include "FileManager.h"
#include "../core/Logger.h"
#include "../core/Error.h"
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
    
    // Résoudre le chemin racine canonique (résout symlinks, .., etc.)
    try {
        if (fs::exists(rootPath_)) {
            rootPath_ = fs::canonical(rootPath_).string();
            Logger::info("FileManager", "  Canonical root: " + rootPath_);
        } else {
            Logger::warn("FileManager", "  Root path does not exist yet (will be created)");
        }
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", "Failed to resolve root path: " + std::string(e.what()));
    }
}

FileManager::~FileManager() {
    Logger::info("FileManager", "FileManager destroyed");
}

// ============================================================================
// MÉTHODES DE SÉCURITÉ - PATH TRAVERSAL
// ============================================================================

/**
 * @brief Valide un chemin pour prévenir path traversal
 * 
 * Effectue une validation multi-niveaux:
 * 1. Détection de séquences dangereuses (.., .\, etc.)
 * 2. Résolution canonique du chemin (symlinks, .., .)
 * 3. Vérification que le chemin final reste sous rootPath_
 * 4. Blocage des chemins absolus non autorisés
 * 
 * @param relativePath Chemin relatif à valider
 * @return bool True si le chemin est sûr
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
 * Remplace ou supprime:
 * - Caractères de contrôle
 * - Séparateurs de chemin (/, \)
 * - Caractères interdits (<, >, :, ", |, ?, *, etc.)
 * - Noms réservés Windows
 * 
 * @param filename Nom de fichier à nettoyer
 * @return std::string Nom de fichier sûr
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
 * @param path Chemin à vérifier (relatif ou absolu)
 * @return bool True si le chemin est sûr
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
    
    if (!fs::exists(dirPath)) {
        Logger::warn("FileManager", "Directory does not exist: " + dirPath);
        return files;
    }
    
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            
            std::string filepath = entry.path().string();
            
            // Filtrer par extension si demandé
            if (!extension.empty()) {
                std::string fileExt = entry.path().extension().string();
                if (fileExt != extension) {
                    continue;
                }
            }
            
            files.push_back(parseFileInfo(filepath, dirType));
        }
        
        // Trier par date de modification (plus récent en premier)
        std::sort(files.begin(), files.end(),
            [](const FileInfo& a, const FileInfo& b) {
                return a.modified > b.modified;
            });
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", 
            "Failed to list files in " + dirPath + ": " + std::string(e.what()));
    }
    
    return files;
}

std::string FileManager::getDirectoryPath(DirectoryType dirType) const {
    return rootPath_ + "/" + directoryTypeToString(dirType);
}

std::optional<FileInfo> FileManager::getFileInfo(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // ✅ VALIDATION SÉCURITÉ
    if (!isPathSafe(filepath)) {
        Logger::error("FileManager", "Invalid or unsafe path: " + filepath);
        return std::nullopt;
    }
    
    if (!fs::exists(filepath)) {
        Logger::warn("FileManager", "File not found: " + filepath);
        return std::nullopt;
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
    
    // 2. Sécuriser le nom de fichier
    std::string safeName = sanitizeFilename(filename);
    if (safeName.empty()) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR, "Invalid filename");
    }
    
    Logger::debug("FileManager", "  Original name: " + filename);
    Logger::debug("FileManager", "  Safe name: " + safeName);
    
    // 3. Construire le chemin destination (relatif)
    std::string relPath = directoryTypeToString(destDir) + "/" + safeName;
    
    // 4. ✅ VALIDATION SÉCURITÉ
    if (!validatePath(relPath)) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR,
                   "Invalid or unsafe path: " + relPath);
    }
    
    std::string destPath = getDirectoryPath(destDir) + "/" + safeName;
    
    Logger::debug("FileManager", "  Destination: " + destPath);
    
    // 5. Vérifier si le fichier existe
    if (!overwrite && fs::exists(destPath)) {
        THROW_ERROR(ErrorCode::FILE_ALREADY_EXISTS,
                   "File already exists: " + safeName);
    }
    
    // 6. Écrire le fichier
    try {
        std::ofstream file(destPath, std::ios::binary);
        if (!file) {
            THROW_ERROR(ErrorCode::FILE_IO_ERROR,
                       "Failed to create file: " + destPath);
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        Logger::info("FileManager", 
            "✓ File uploaded (" + std::to_string(data.size()) + " bytes): " + safeName);
        
        return destPath;
        
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::FILE_IO_ERROR,
                   "Failed to write file: " + std::string(e.what()));
    }
}

std::vector<uint8_t> FileManager::downloadFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Downloading file: " + filepath);
    
    // ✅ VALIDATION SÉCURITÉ
    if (!isPathSafe(filepath)) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR,
                   "Invalid or unsafe path: " + filepath);
    }
    
    if (!fs::exists(filepath)) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND,
                   "File not found: " + filepath);
    }
    
    try {
        // Lire le fichier
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file) {
            THROW_ERROR(ErrorCode::FILE_IO_ERROR,
                       "Failed to open file: " + filepath);
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            THROW_ERROR(ErrorCode::FILE_IO_ERROR,
                       "Failed to read file: " + filepath);
        }
        
        Logger::info("FileManager", 
            "✓ File downloaded (" + std::to_string(size) + " bytes)");
        
        return buffer;
        
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::FILE_IO_ERROR,
                   "Failed to download file: " + std::string(e.what()));
    }
}

// ============================================================================
// OPÉRATIONS FICHIERS
// ============================================================================

bool FileManager::copyFile(const std::string& source, 
                           const std::string& destination,
                           bool overwrite) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Copying file: " + source + " -> " + destination);
    
    // ✅ VALIDATION SÉCURITÉ (source et destination)
    if (!isPathSafe(source)) {
        Logger::error("FileManager", "Invalid source path: " + source);
        return false;
    }
    
    if (!isPathSafe(destination)) {
        Logger::error("FileManager", "Invalid destination path: " + destination);
        return false;
    }
    
    if (!fs::exists(source)) {
        Logger::error("FileManager", "Source file not found: " + source);
        return false;
    }
    
    if (!overwrite && fs::exists(destination)) {
        Logger::error("FileManager", "Destination already exists: " + destination);
        return false;
    }
    
    try {
        fs::copy_file(source, destination, 
            overwrite ? fs::copy_options::overwrite_existing 
                     : fs::copy_options::none);
        
        Logger::info("FileManager", "✓ File copied successfully");
        return true;
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", "Copy failed: " + std::string(e.what()));
        return false;
    }
}

bool FileManager::moveFile(const std::string& source, 
                           const std::string& destination) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Moving file: " + source + " -> " + destination);
    
    // ✅ VALIDATION SÉCURITÉ (source et destination)
    if (!isPathSafe(source)) {
        Logger::error("FileManager", "Invalid source path: " + source);
        return false;
    }
    
    if (!isPathSafe(destination)) {
        Logger::error("FileManager", "Invalid destination path: " + destination);
        return false;
    }
    
    if (!fs::exists(source)) {
        Logger::error("FileManager", "Source file not found: " + source);
        return false;
    }
    
    try {
        fs::rename(source, destination);
        
        Logger::info("FileManager", "✓ File moved successfully");
        return true;
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", "Move failed: " + std::string(e.what()));
        return false;
    }
}

bool FileManager::deleteFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Deleting file: " + filepath);
    
    // ✅ VALIDATION SÉCURITÉ
    if (!isPathSafe(filepath)) {
        Logger::error("FileManager", "Invalid path: " + filepath);
        return false;
    }
    
    if (!fs::exists(filepath)) {
        Logger::warn("FileManager", "File does not exist (skipping): " + filepath);
        return true;  // Pas une erreur
    }
    
    try {
        fs::remove(filepath);
        
        Logger::info("FileManager", "✓ File deleted");
        return true;
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", "Delete failed: " + std::string(e.what()));
        return false;
    }
}

int FileManager::clearDirectory(DirectoryType dirType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    
    Logger::warn("FileManager", "Clearing directory: " + dirPath);
    
    if (!fs::exists(dirPath)) {
        Logger::warn("FileManager", "Directory does not exist");
        return 0;
    }
    
    int deletedCount = 0;
    
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                fs::remove(entry.path());
                deletedCount++;
            }
        }
        
        Logger::info("FileManager", 
            "✓ Cleared " + std::to_string(deletedCount) + " files");
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", "Clear failed: " + std::string(e.what()));
    }
    
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
    
    if (!fs::exists(logsDir)) {
        return 0;
    }
    
    auto now = std::chrono::system_clock::now();
    auto maxAge = std::chrono::hours(maxAgeDays * 24);
    
    std::vector<std::string> files;
    int deleted = 0;
    
    try {
        // Lister tous les logs
        for (const auto& entry : fs::directory_iterator(logsDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            
            std::string ext = entry.path().extension().string();
            if (ext != ".log") {
                continue;
            }
            
            files.push_back(entry.path().string());
        }
        
        // Trier par date (plus récent en premier)
        std::sort(files.begin(), files.end(),
            [](const std::string& a, const std::string& b) {
                return fs::last_write_time(a) > fs::last_write_time(b);
            });
        
        // Garder les 5 plus récents, supprimer les vieux
        for (size_t i = 5; i < files.size(); ++i) {
            auto fileTime = fs::last_write_time(files[i]);
            auto fileAge = now - std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            
            if (fileAge > maxAge) {
                fs::remove(files[i]);
                deleted++;
                Logger::debug("FileManager", "  Deleted: " + files[i]);
            }
        }
        
        Logger::info("FileManager", "✓ Cleaned " + std::to_string(deleted) + " old logs");
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", "Clean logs failed: " + std::string(e.what()));
    }
    
    return deleted;
}

int FileManager::cleanOldBackups(int maxCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", 
        "Cleaning backups (keeping " + std::to_string(maxCount) + " most recent)...");
    
    std::string backupsDir = getDirectoryPath(DirectoryType::BACKUPS);
    
    if (!fs::exists(backupsDir)) {
        return 0;
    }
    
    std::vector<std::string> files;
    
    try {
        // Lister tous les backups
        for (const auto& entry : fs::directory_iterator(backupsDir)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
        
        // Trier par date (plus récent en premier)
        std::sort(files.begin(), files.end(),
            [](const std::string& a, const std::string& b) {
                return fs::last_write_time(a) > fs::last_write_time(b);
            });
        
        // Supprimer les anciens (au-delà de maxCount)
        int deleted = 0;
        
        for (size_t i = maxCount; i < files.size(); ++i) {
            fs::remove(files[i]);
            deleted++;
            Logger::debug("FileManager", "  Deleted: " + files[i]);
        }
        
        Logger::info("FileManager", "✓ Cleaned " + std::to_string(deleted) + " old backups");
        
        return deleted;
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", "Clean backups failed: " + std::string(e.what()));
        return 0;
    }
}

int FileManager::clearTemp() {
    return clearDirectory(DirectoryType::TEMP);
}

// ============================================================================
// STATISTIQUES
// ============================================================================

uint64_t FileManager::getDirectorySize(DirectoryType dirType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    uint64_t totalSize = 0;
    
    if (!fs::exists(dirPath)) {
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
    int count = 0;
    
    if (!fs::exists(dirPath)) {
        return 0;
    }
    
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                count++;
            }
        }
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", 
            "Failed to count files: " + std::string(e.what()));
    }
    
    return count;
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
    
    try {
        fs::path path(filepath);
        
        info.path = filepath;
        info.name = path.filename().string();
        info.extension = path.extension().string();
        info.directory = directoryTypeToString(dirType);
        
        if (fs::exists(path)) {
            info.size = fs::file_size(path);
            
            auto ftime = fs::last_write_time(path);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            info.modified = std::chrono::system_clock::to_time_t(sctp);
        }
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("FileManager", 
            "Failed to parse file info: " + std::string(e.what()));
    }
    
    return info;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER FileManager.cpp v3.0.3 - SÉCURITÉ RENFORCÉE
// ============================================================================
// RÉSUMÉ DES PROTECTIONS:
//   ✅ Path traversal (../, .\) bloqué
//   ✅ Symlinks malveillants détectés via canonical()
//   ✅ Chemins absolus non autorisés bloqués
//   ✅ Drive letters Windows bloqués
//   ✅ Chemins UNC bloqués
//   ✅ Null bytes bloqués
//   ✅ Caractères dangereux supprimés
//   ✅ Noms réservés Windows gérés
//   ✅ Validation multi-niveaux
//   ✅ Résolution canonique des chemins
//   ✅ Toutes opérations fichiers validées
// ============================================================================
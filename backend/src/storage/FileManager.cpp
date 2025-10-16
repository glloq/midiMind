// ============================================================================
// File: backend/src/storage/FileManager.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of FileManager - Unified file management system.
//   Combines fast unsafe operations (internal) with secure validated
//   operations (public API).
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - All unsafe operations now use FileManager::Unsafe:: namespace
//   - Enhanced path validation with canonical resolution
//   - Improved filename sanitization
//   - Thread-safe operations
//
// ============================================================================

#include "FileManager.h"
#include "../core/TimeUtils.h"
#include <algorithm>
#include <cctype>
#include <regex>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

FileManager::FileManager(const std::string& rootPath)
    : rootPath_(rootPath)
{
    Logger::info("FileManager", "FileManager created with root: " + rootPath);
    
    // Ensure root path doesn't end with slash
    if (!rootPath_.empty() && (rootPath_.back() == '/' || rootPath_.back() == '\\')) {
        rootPath_.pop_back();
    }
}

FileManager::~FileManager() {
    Logger::debug("FileManager", "FileManager destroyed");
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool FileManager::initializeDirectories() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Initializing directory structure...");
    
    // Create root directory if it doesn't exist
    if (!Unsafe::exists(rootPath_)) {
        if (!Unsafe::createDirectory(rootPath_, true)) {
            Logger::error("FileManager", "Failed to create root directory: " + rootPath_);
            return false;
        }
        Logger::info("FileManager", "  ✓ Created root: " + rootPath_);
    }
    
    // Create subdirectories
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
        
        if (!Unsafe::exists(dirPath)) {
            if (Unsafe::createDirectory(dirPath, true)) {
                Logger::info("FileManager", "  ✓ Created: " + dirPath);
            } else {
                Logger::error("FileManager", "  ✗ Failed: " + dirPath);
                allSuccess = false;
            }
        } else {
            Logger::debug("FileManager", "  - Exists: " + dirPath);
        }
    }
    
    if (allSuccess) {
        Logger::info("FileManager", "✓ Directory structure initialized");
    } else {
        Logger::warning("FileManager", "⚠ Some directories failed to initialize");
    }
    
    return allSuccess;
}

// ============================================================================
// UPLOAD / DOWNLOAD
// ============================================================================

std::string FileManager::uploadFile(const std::vector<uint8_t>& data,
                                    const std::string& filename,
                                    DirectoryType destDir,
                                    bool overwrite) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Uploading file: " + filename);
    
    // 1. Validate size
    if (data.size() > MAX_UPLOAD_SIZE) {
        THROW_ERROR(ErrorCode::VALIDATION_FAILED,
                   "File too large (max " + std::to_string(MAX_UPLOAD_SIZE) + " bytes)");
    }
    
    // 2. Sanitize filename
    std::string safeName = sanitizeFilename(filename);
    if (safeName.empty()) {
        THROW_ERROR(ErrorCode::VALIDATION_FAILED, "Invalid filename");
    }
    
    Logger::debug("FileManager", "  Original: " + filename);
    Logger::debug("FileManager", "  Sanitized: " + safeName);
    
    // 3. Build destination path
    std::string relPath = directoryTypeToString(destDir) + "/" + safeName;
    
    // 4. Validate path
    if (!validatePath(relPath)) {
        THROW_ERROR(ErrorCode::VALIDATION_FAILED,
                   "Invalid or unsafe path: " + relPath);
    }
    
    std::string destPath = getDirectoryPath(destDir) + "/" + safeName;
    
    Logger::debug("FileManager", "  Destination: " + destPath);
    
    // 5. Check if file exists
    if (!overwrite && Unsafe::exists(destPath)) {
        THROW_ERROR(ErrorCode::STORAGE_FILE_EXISTS,
                   "File already exists: " + safeName);
    }
    
    // 6. Write file
    if (!Unsafe::writeBinaryFile(destPath, data)) {
        THROW_ERROR(ErrorCode::STORAGE_IO_ERROR,
                   "Failed to write file: " + destPath);
    }
    
    Logger::info("FileManager", 
        "✓ File uploaded (" + std::to_string(data.size()) + " bytes): " + safeName);
    
    return destPath;
}

std::vector<uint8_t> FileManager::downloadFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Downloading file: " + filepath);
    
    // Validate path
    if (!isPathSafe(filepath)) {
        THROW_ERROR(ErrorCode::VALIDATION_FAILED,
                   "Invalid or unsafe path: " + filepath);
    }
    
    std::string fullPath = rootPath_ + "/" + filepath;
    
    // Check if file exists
    if (!Unsafe::exists(fullPath)) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND,
                   "File not found: " + filepath);
    }
    
    // Read file
    auto data = Unsafe::readBinaryFile(fullPath);
    
    if (data.empty()) {
        Logger::warning("FileManager", "File is empty or read failed: " + filepath);
    }
    
    Logger::info("FileManager", 
        "✓ File downloaded (" + std::to_string(data.size()) + " bytes)");
    
    return data;
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

bool FileManager::deleteFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Deleting file: " + filepath);
    
    // Validate path
    if (!isPathSafe(filepath)) {
        Logger::error("FileManager", "Invalid or unsafe path: " + filepath);
        return false;
    }
    
    std::string fullPath = rootPath_ + "/" + filepath;
    
    // Check if file exists
    if (!Unsafe::exists(fullPath)) {
        Logger::warning("FileManager", "File not found: " + filepath);
        return false;
    }
    
    // Delete file
    if (Unsafe::deleteFile(fullPath)) {
        Logger::info("FileManager", "✓ File deleted: " + filepath);
        return true;
    } else {
        Logger::error("FileManager", "Failed to delete file: " + filepath);
        return false;
    }
}

bool FileManager::copyFile(const std::string& source, const std::string& dest) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Copying file: " + source + " -> " + dest);
    
    // Validate paths
    if (!isPathSafe(source) || !isPathSafe(dest)) {
        Logger::error("FileManager", "Invalid or unsafe path");
        return false;
    }
    
    std::string sourcePath = rootPath_ + "/" + source;
    std::string destPath = rootPath_ + "/" + dest;
    
    // Check source exists
    if (!Unsafe::exists(sourcePath)) {
        Logger::error("FileManager", "Source file not found: " + source);
        return false;
    }
    
    // Copy file
    if (Unsafe::copyFile(sourcePath, destPath)) {
        Logger::info("FileManager", "✓ File copied");
        return true;
    } else {
        Logger::error("FileManager", "Failed to copy file");
        return false;
    }
}

bool FileManager::moveFile(const std::string& source, const std::string& dest) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Moving file: " + source + " -> " + dest);
    
    // Validate paths
    if (!isPathSafe(source) || !isPathSafe(dest)) {
        Logger::error("FileManager", "Invalid or unsafe path");
        return false;
    }
    
    std::string sourcePath = rootPath_ + "/" + source;
    std::string destPath = rootPath_ + "/" + dest;
    
    // Check source exists
    if (!Unsafe::exists(sourcePath)) {
        Logger::error("FileManager", "Source file not found: " + source);
        return false;
    }
    
    // Move file
    if (Unsafe::moveFile(sourcePath, destPath)) {
        Logger::info("FileManager", "✓ File moved");
        return true;
    } else {
        Logger::error("FileManager", "Failed to move file");
        return false;
    }
}

// ============================================================================
// DIRECTORY OPERATIONS
// ============================================================================

std::vector<FileInfo> FileManager::listFiles(DirectoryType dirType) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string dirPath = getDirectoryPath(dirType);
    std::vector<FileInfo> files;
    
    Logger::debug("FileManager", "Listing files in: " + dirPath);
    
    if (!Unsafe::exists(dirPath)) {
        Logger::warning("FileManager", "Directory not found: " + dirPath);
        return files;
    }
    
    auto filenames = Unsafe::listFiles(dirPath);
    
    for (const auto& filename : filenames) {
        std::string filepath = dirPath + "/" + filename;
        auto fileInfo = parseFileInfo(filepath, dirType);
        
        if (fileInfo.has_value()) {
            files.push_back(fileInfo.value());
        }
    }
    
    Logger::debug("FileManager", "Found " + std::to_string(files.size()) + " files");
    
    return files;
}

std::optional<FileInfo> FileManager::getFileInfo(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate path
    if (!isPathSafe(filepath)) {
        Logger::error("FileManager", "Invalid or unsafe path: " + filepath);
        return std::nullopt;
    }
    
    std::string fullPath = rootPath_ + "/" + filepath;
    
    if (!Unsafe::exists(fullPath)) {
        Logger::warning("FileManager", "File not found: " + filepath);
        return std::nullopt;
    }
    
    // Determine directory type from path
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
    
    return parseFileInfo(fullPath, dirType);
}

std::string FileManager::getDirectoryPath(DirectoryType dirType) const {
    return rootPath_ + "/" + directoryTypeToString(dirType);
}

// ============================================================================
// VALIDATION
// ============================================================================

std::string FileManager::sanitizeFilename(const std::string& filename) const {
    if (filename.empty()) {
        return "";
    }
    
    std::string safe = filename;
    
    // 1. Remove path components (keep only filename)
    size_t lastSlash = safe.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        safe = safe.substr(lastSlash + 1);
    }
    
    // 2. Remove null bytes
    safe.erase(std::remove(safe.begin(), safe.end(), '\0'), safe.end());
    
    // 3. Remove dangerous characters
    const std::string forbidden = "<>:\"|?*\r\n\t";
    for (char c : forbidden) {
        safe.erase(std::remove(safe.begin(), safe.end(), c), safe.end());
    }
    
    // 4. Remove leading/trailing dots and spaces
    while (!safe.empty() && (safe[0] == '.' || safe[0] == ' ')) {
        safe.erase(0, 1);
    }
    while (!safe.empty() && (safe.back() == '.' || safe.back() == ' ')) {
        safe.pop_back();
    }
    
    // 5. Check for reserved names (Windows)
    const std::vector<std::string> reserved = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    
    std::string upperSafe = safe;
    std::transform(upperSafe.begin(), upperSafe.end(), upperSafe.begin(), ::toupper);
    
    for (const auto& name : reserved) {
        if (upperSafe == name || upperSafe.find(name + ".") == 0) {
            safe = "_" + safe;
            break;
        }
    }
    
    // 6. Limit length
    if (safe.length() > MAX_FILENAME_LENGTH) {
        // Keep extension if present
        std::string ext = Unsafe::getExtension(safe);
        size_t maxBase = MAX_FILENAME_LENGTH - ext.length();
        safe = safe.substr(0, maxBase) + ext;
    }
    
    // 7. Generate default name if empty
    if (safe.empty()) {
        safe = "unnamed_file";
    }
    
    return safe;
}

bool FileManager::validatePath(const std::string& path) const {
    if (path.empty()) {
        return false;
    }
    
    // 1. Check for absolute paths (not allowed)
    if (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
        Logger::warning("FileManager", "Absolute path not allowed: " + path);
        return false;
    }
    
    // 2. Check for drive letters (Windows)
    if (path.length() >= 2 && path[1] == ':') {
        Logger::warning("FileManager", "Drive letter not allowed: " + path);
        return false;
    }
    
    // 3. Check for path traversal
    if (path.find("..") != std::string::npos) {
        Logger::warning("FileManager", "Path traversal detected: " + path);
        return false;
    }
    
    // 4. Check for null bytes
    if (path.find('\0') != std::string::npos) {
        Logger::warning("FileManager", "Null byte in path: " + path);
        return false;
    }
    
    // 5. Check for dangerous patterns
    const std::vector<std::string> dangerous = {
        "//", "\\\\", "./", ".\\", "~/", "~\\"
    };
    
    for (const auto& pattern : dangerous) {
        if (path.find(pattern) != std::string::npos) {
            Logger::warning("FileManager", "Dangerous pattern in path: " + pattern);
            return false;
        }
    }
    
    // 6. Try to resolve canonical path
    try {
        std::string fullPath = rootPath_ + "/" + path;
        fs::path canonical = fs::weakly_canonical(fullPath);
        fs::path rootCanonical = fs::weakly_canonical(rootPath_);
        
        // Check if canonical path is under root
        std::string canonicalStr = canonical.string();
        std::string rootStr = rootCanonical.string();
        
        if (canonicalStr.find(rootStr) != 0) {
            Logger::warning("FileManager", "Path escapes root: " + path);
            return false;
        }
        
    } catch (const std::exception& e) {
        Logger::warning("FileManager", "Cannot resolve path: " + std::string(e.what()));
        return false;
    }
    
    return true;
}

bool FileManager::isPathSafe(const std::string& path) const {
    // Convert to relative path if absolute
    std::string relativePath = path;
    
    if (path.find(rootPath_) == 0) {
        // Path starts with rootPath_, make it relative
        relativePath = path.substr(rootPath_.length());
        if (!relativePath.empty() && (relativePath[0] == '/' || relativePath[0] == '\\')) {
            relativePath = relativePath.substr(1);
        }
    }
    
    return validatePath(relativePath);
}

// ============================================================================
// STATISTICS
// ============================================================================

json FileManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats = {
        {"root_path", rootPath_},
        {"directories", json::object()}
    };
    
    // Get stats for each directory type
    std::vector<DirectoryType> dirs = {
        DirectoryType::LOGS,
        DirectoryType::BACKUPS,
        DirectoryType::EXPORTS,
        DirectoryType::UPLOADS,
        DirectoryType::TEMP
    };
    
    for (auto dirType : dirs) {
        std::string dirPath = getDirectoryPath(dirType);
        std::string dirName = directoryTypeToString(dirType);
        
        if (Unsafe::exists(dirPath)) {
            auto files = Unsafe::listFiles(dirPath);
            size_t totalSize = 0;
            
            for (const auto& file : files) {
                std::string filepath = dirPath + "/" + file;
                totalSize += Unsafe::fileSize(filepath);
            }
            
            stats["directories"][dirName] = {
                {"file_count", files.size()},
                {"total_size_bytes", totalSize},
                {"path", dirPath}
            };
        } else {
            stats["directories"][dirName] = {
                {"file_count", 0},
                {"total_size_bytes", 0},
                {"path", dirPath},
                {"exists", false}
            };
        }
    }
    
    return stats;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

std::optional<FileInfo> FileManager::parseFileInfo(const std::string& filepath,
                                                   DirectoryType dirType) {
    if (!Unsafe::exists(filepath)) {
        return std::nullopt;
    }
    
    FileInfo info;
    
    try {
        fs::path p(filepath);
        
        info.name = p.filename().string();
        info.path = filepath;
        info.extension = Unsafe::getExtension(filepath);
        info.size = Unsafe::fileSize(filepath);
        info.directory = dirType;
        info.isDirectory = Unsafe::isDirectory(filepath);
        
        // Get timestamps
        auto ftime = fs::last_write_time(p);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        info.modifiedAt = std::chrono::system_clock::to_time_t(sctp);
        info.createdAt = info.modifiedAt; // Fallback to modified time
        
    } catch (const std::exception& e) {
        Logger::error("FileManager", "Failed to parse file info: " + std::string(e.what()));
        return std::nullopt;
    }
    
    return info;
}

std::string FileManager::directoryTypeToString(DirectoryType type) const {
    switch (type) {
        case DirectoryType::LOGS:    return DIR_LOGS;
        case DirectoryType::BACKUPS: return DIR_BACKUPS;
        case DirectoryType::EXPORTS: return DIR_EXPORTS;
        case DirectoryType::UPLOADS: return DIR_UPLOADS;
        case DirectoryType::TEMP:    return DIR_TEMP;
        default:                     return DIR_TEMP;
    }
}

} // namespace midiMind

// ============================================================================
// END OF FILE FileManager.cpp
// ============================================================================
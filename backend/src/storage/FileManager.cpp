// ============================================================================
// File: backend/src/storage/FileManager.cpp
// Version: 4.4.0 - SECURE
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of FileManager - Unified file management system.
//   Combines fast unsafe operations (internal) with secure validated
//   operations (public API).
//
// Changes v4.4.0:
//   - Fixed: buildFullPath() for canonical path validation
//   - Fixed: downloadFile() distinguishes empty file vs read error
//   - Fixed: All path concatenations use buildFullPath()
//   - Fixed: Proper error propagation
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
    if (!FileManagerUnsafe::exists(rootPath_)) {
        if (!FileManagerUnsafe::createDirectory(rootPath_, true)) {
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
        
        if (!FileManagerUnsafe::exists(dirPath)) {
            if (FileManagerUnsafe::createDirectory(dirPath, true)) {
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
    
    // 3. Build destination path with validation
    std::string relPath = directoryTypeToString(destDir) + "/" + safeName;
    std::string destPath;
    
    try {
        destPath = buildFullPath(relPath);
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::VALIDATION_FAILED,
                   "Invalid or unsafe path: " + relPath);
    }
    
    Logger::debug("FileManager", "  Destination: " + destPath);
    
    // 4. Check if file exists
    if (!overwrite && FileManagerUnsafe::exists(destPath)) {
        THROW_ERROR(ErrorCode::STORAGE_FILE_EXISTS,
                   "File already exists: " + safeName);
    }
    
    // 5. Write file
    if (!FileManagerUnsafe::writeBinaryFile(destPath, data)) {
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
    
    // Build and validate full path
    std::string fullPath;
    try {
        fullPath = buildFullPath(filepath);
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::VALIDATION_FAILED,
                   "Invalid or unsafe path: " + filepath);
    }
    
    // Check if file exists
    if (!FileManagerUnsafe::exists(fullPath)) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND,
                   "File not found: " + filepath);
    }
    
    // Get file size to distinguish empty file from read error
    size_t expectedSize = FileManagerUnsafe::fileSize(fullPath);
    
    // Read file
    auto data = FileManagerUnsafe::readBinaryFile(fullPath);
    
    // Verify read was successful
    if (data.size() != expectedSize) {
        if (expectedSize == 0 && data.empty()) {
            Logger::info("FileManager", "File is empty (0 bytes)");
        } else {
            THROW_ERROR(ErrorCode::STORAGE_IO_ERROR,
                       "File read failed or incomplete: expected " + 
                       std::to_string(expectedSize) + " bytes, got " + 
                       std::to_string(data.size()));
        }
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
    
    // Build and validate full path
    std::string fullPath;
    try {
        fullPath = buildFullPath(filepath);
    } catch (const std::exception& e) {
        Logger::error("FileManager", "Invalid or unsafe path: " + filepath);
        return false;
    }
    
    // Check if file exists
    if (!FileManagerUnsafe::exists(fullPath)) {
        Logger::warning("FileManager", "File not found: " + filepath);
        return false;
    }
    
    // Delete file
    if (FileManagerUnsafe::deleteFile(fullPath)) {
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
    
    // Build and validate both paths
    std::string sourcePath, destPath;
    try {
        sourcePath = buildFullPath(source);
        destPath = buildFullPath(dest);
    } catch (const std::exception& e) {
        Logger::error("FileManager", "Invalid or unsafe path");
        return false;
    }
    
    // Check source exists
    if (!FileManagerUnsafe::exists(sourcePath)) {
        Logger::error("FileManager", "Source file not found: " + source);
        return false;
    }
    
    // Copy file
    if (FileManagerUnsafe::copyFile(sourcePath, destPath)) {
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
    
    // Build and validate both paths
    std::string sourcePath, destPath;
    try {
        sourcePath = buildFullPath(source);
        destPath = buildFullPath(dest);
    } catch (const std::exception& e) {
        Logger::error("FileManager", "Invalid or unsafe path");
        return false;
    }
    
    // Check source exists
    if (!FileManagerUnsafe::exists(sourcePath)) {
        Logger::error("FileManager", "Source file not found: " + source);
        return false;
    }
    
    // Move file
    if (FileManagerUnsafe::moveFile(sourcePath, destPath)) {
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
    
    if (!FileManagerUnsafe::exists(dirPath)) {
        Logger::warning("FileManager", "Directory not found: " + dirPath);
        return {};
    }
    
    auto filenames = FileManagerUnsafe::listFiles(dirPath);
    
    std::vector<FileInfo> files;
    files.reserve(filenames.size());
    
    for (const auto& name : filenames) {
        std::string filepath = dirPath + "/" + name;
        auto fileInfo = parseFileInfo(filepath, dirType);
        
        if (fileInfo.has_value()) {
            files.push_back(fileInfo.value());
        }
    }
    
    return files;
}

std::optional<FileInfo> FileManager::getFileInfo(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Build and validate full path
    std::string fullPath;
    try {
        fullPath = buildFullPath(filepath);
    } catch (const std::exception& e) {
        Logger::warning("FileManager", "Invalid path: " + filepath);
        return std::nullopt;
    }
    
    if (!FileManagerUnsafe::exists(fullPath)) {
        return std::nullopt;
    }
    
    // Determine directory type from path
    DirectoryType dirType = DirectoryType::UPLOADS;
    if (filepath.find("logs/") == 0) dirType = DirectoryType::LOGS;
    else if (filepath.find("backups/") == 0) dirType = DirectoryType::BACKUPS;
    else if (filepath.find("exports/") == 0) dirType = DirectoryType::EXPORTS;
    else if (filepath.find("temp/") == 0) dirType = DirectoryType::TEMP;
    
    return parseFileInfo(fullPath, dirType);
}

std::string FileManager::getDirectoryPath(DirectoryType dirType) const {
    return rootPath_ + "/" + directoryTypeToString(dirType);
}

// ============================================================================
// VALIDATION
// ============================================================================

bool FileManager::isPathSafe(const std::string& path) const {
    return validatePath(path);
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

std::string FileManager::buildFullPath(const std::string& relativePath) const {
    // 1. Basic validation
    if (relativePath.empty()) {
        throw std::runtime_error("Empty path");
    }
    
    // 2. Check for absolute paths (not allowed)
    if (!relativePath.empty() && (relativePath[0] == '/' || relativePath[0] == '\\')) {
        throw std::runtime_error("Absolute path not allowed");
    }
    
    // 3. Build full path
    std::string fullPath = rootPath_ + "/" + relativePath;
    
    // 4. Resolve to canonical path
    try {
        fs::path canonical = fs::weakly_canonical(fullPath);
        fs::path rootCanonical = fs::weakly_canonical(rootPath_);
        
        // 5. Verify path stays within root
        std::string canonicalStr = canonical.string();
        std::string rootStr = rootCanonical.string();
        
        // Ensure both paths use same separator
        std::replace(canonicalStr.begin(), canonicalStr.end(), '\\', '/');
        std::replace(rootStr.begin(), rootStr.end(), '\\', '/');
        
        if (canonicalStr.find(rootStr) != 0) {
            throw std::runtime_error("Path escapes root directory");
        }
        
        return canonical.string();
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Path validation failed: " + std::string(e.what()));
    }
}

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
        std::string ext = FileManagerUnsafe::getExtension(safe);
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
    try {
        buildFullPath(path);
        return true;
    } catch (const std::exception& e) {
        Logger::warning("FileManager", "Path validation failed: " + std::string(e.what()));
        return false;
    }
}

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
        
        if (FileManagerUnsafe::exists(dirPath)) {
            auto files = FileManagerUnsafe::listFiles(dirPath);
            size_t totalSize = 0;
            
            for (const auto& file : files) {
                std::string filepath = dirPath + "/" + file;
                totalSize += FileManagerUnsafe::fileSize(filepath);
            }
            
            json dirInfo = {
                {"file_count", files.size()},
                {"total_size_bytes", totalSize},
                {"path", dirPath}
            };
            stats["directories"][dirName] = dirInfo;
        } else {
            json dirInfo = {
                {"file_count", 0},
                {"total_size_bytes", 0},
                {"path", dirPath},
                {"exists", false}
            };
            stats["directories"][dirName] = dirInfo;
        }
    }
    
    return stats;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

std::optional<FileInfo> FileManager::parseFileInfo(const std::string& filepath,
                                                   DirectoryType dirType) {
    if (!FileManagerUnsafe::exists(filepath)) {
        return std::nullopt;
    }
    
    FileInfo info;
    
    try {
        fs::path p(filepath);
        
        info.name = p.filename().string();
        info.id = p.filename().string();  // Set ID to filename
        info.path = filepath;
        info.extension = FileManagerUnsafe::getExtension(filepath);
        info.size = FileManagerUnsafe::fileSize(filepath);
        info.directory = dirType;
        info.isDirectory = FileManagerUnsafe::isDirectory(filepath);
        
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

// ============================================================================
// BASE64 ENCODING/DECODING
// ============================================================================

// Base64 encoding table
static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string FileManager::base64Encode(const std::vector<uint8_t>& data) const {
    std::string encoded;
    int val = 0;
    int valb = -6;
    
    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }
    
    return encoded;
}

std::vector<uint8_t> FileManager::base64Decode(const std::string& encoded) const {
    std::vector<uint8_t> decoded;
    std::vector<int> T(256, -1);
    
    for (int i = 0; i < 64; i++) {
        T[base64_chars[i]] = i;
    }
    
    int val = 0;
    int valb = -8;
    
    for (unsigned char c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decoded;
}

FileInfo FileManager::uploadFileBase64(const std::string& filename, 
                                       const std::string& base64Data) {
    Logger::info("FileManager", "Upload base64: " + filename);
    
    // Decode base64
    auto binaryData = base64Decode(base64Data);
    
    // Upload using existing method
    std::string destPath = uploadFile(binaryData, filename, 
                                     DirectoryType::UPLOADS, false);
    
    // Get file info
    auto fileInfo = getFileInfo("uploads/" + sanitizeFilename(filename));
    
    if (!fileInfo.has_value()) {
        THROW_ERROR(ErrorCode::STORAGE_IO_ERROR, 
                   "Failed to get uploaded file info");
    }
    
    // Set ID to filename
    fileInfo->id = sanitizeFilename(filename);
    
    return fileInfo.value();
}

std::string FileManager::downloadFileBase64(const std::string& fileId) {
    Logger::info("FileManager", "Download base64: " + fileId);
    
    std::string filepath = "uploads/" + fileId;
    auto binaryData = downloadFile(filepath);
    
    return base64Encode(binaryData);
}

bool FileManager::renameFile(const std::string& fileId, const std::string& newName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Rename: " + fileId + " -> " + newName);
    
    std::string sanitizedNew = sanitizeFilename(newName);
    
    // Build validated paths
    std::string sourcePath, destPath;
    try {
        sourcePath = buildFullPath("uploads/" + fileId);
        destPath = buildFullPath("uploads/" + sanitizedNew);
    } catch (const std::exception& e) {
        Logger::error("FileManager", "Invalid path: " + std::string(e.what()));
        return false;
    }
    
    if (!FileManagerUnsafe::exists(sourcePath)) {
        Logger::error("FileManager", "Source not found: " + fileId);
        return false;
    }
    
    if (FileManagerUnsafe::exists(destPath)) {
        Logger::error("FileManager", "Destination already exists: " + sanitizedNew);
        return false;
    }
    
    bool success = FileManagerUnsafe::moveFile(sourcePath, destPath);
    
    if (success) {
        Logger::info("FileManager", "✓ File renamed");
    } else {
        Logger::error("FileManager", "Failed to rename");
    }
    
    return success;
}

FileInfo FileManager::copyFileByName(const std::string& fileId, 
                                     const std::string& newName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("FileManager", "Copy: " + fileId + " -> " + newName);
    
    std::string sanitizedNew = sanitizeFilename(newName);
    
    // Build validated paths
    std::string sourcePath, destPath;
    try {
        sourcePath = buildFullPath("uploads/" + fileId);
        destPath = buildFullPath("uploads/" + sanitizedNew);
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::VALIDATION_FAILED, 
                   "Invalid path: " + std::string(e.what()));
    }
    
    if (!FileManagerUnsafe::exists(sourcePath)) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND, "Source not found: " + fileId);
    }
    
    if (FileManagerUnsafe::exists(destPath)) {
        THROW_ERROR(ErrorCode::STORAGE_FILE_EXISTS, 
                   "Destination already exists: " + sanitizedNew);
    }
    
    if (!FileManagerUnsafe::copyFile(sourcePath, destPath)) {
        THROW_ERROR(ErrorCode::STORAGE_IO_ERROR, "Failed to copy file");
    }
    
    Logger::info("FileManager", "✓ File copied");
    
    // Get new file info
    auto fileInfo = parseFileInfo(destPath, DirectoryType::UPLOADS);
    
    if (!fileInfo.has_value()) {
        THROW_ERROR(ErrorCode::STORAGE_IO_ERROR, 
                   "Failed to get copied file info");
    }
    
    fileInfo->id = sanitizedNew;
    
    return fileInfo.value();
}

std::vector<FileInfo> FileManager::listFiles() {
    return listFiles(DirectoryType::UPLOADS);
}

} // namespace midiMind

// ============================================================================
// END OF FILE FileManager.cpp v4.4.0
// ============================================================================
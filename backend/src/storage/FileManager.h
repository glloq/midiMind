// ============================================================================
// File: backend/src/storage/FileManager.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Unified file management system combining fast unsafe operations
//   and secure validated operations.
//
//   CRITICAL: This replaces the old FileSystem.h + FileManager.cpp split.
//   All code must migrate from FileSystem:: to FileManager::Unsafe::
//
// Architecture:
//   - FileManager::Unsafe: Fast inline methods (no validation)
//   - FileManager (default): Safe methods with validation
//
// Security:
//   - Path traversal protection
//   - Filename sanitization
//   - Extension whitelisting
//   - Size limits
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Unified FileSystem.h into FileManager::Unsafe namespace
//   - All unsafe methods are now static inline
//   - Safe methods remain as instance methods
//   - Enhanced security validation
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "../core/Logger.h"
#include "../core/Error.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ENUMS & STRUCTURES
// ============================================================================

/**
 * @enum DirectoryType
 * @brief Predefined directory types
 */
enum class DirectoryType {
    LOGS,
    BACKUPS,
    EXPORTS,
    UPLOADS,
    TEMP
};

/**
 * @struct FileInfo
 * @brief File metadata structure
 */
struct FileInfo {
    std::string name;
    std::string path;
    std::string extension;
    size_t size;
    std::time_t createdAt;
    std::time_t modifiedAt;
    DirectoryType directory;
    bool isDirectory;
    
    json toJson() const {
        return {
            {"name", name},
            {"path", path},
            {"extension", extension},
            {"size", size},
            {"created_at", createdAt},
            {"modified_at", modifiedAt},
            {"is_directory", isDirectory}
        };
    }
};

// ============================================================================
// CLASS: FileManager
// ============================================================================

/**
 * @class FileManager
 * @brief Unified file management with safe and unsafe operations
 * 
 * Two APIs:
 * 
 * 1. FileManager::Unsafe - Fast, no validation (internal use only)
 *    - Static inline methods
 *    - Direct filesystem operations
 *    - No security checks
 *    - Use with extreme caution
 * 
 * 2. FileManager (instance) - Safe, validated operations (public API)
 *    - Full path validation
 *    - Filename sanitization
 *    - Security checks
 *    - Thread-safe
 * 
 * Migration from v4.0.3:
 * - FileSystem::method() -> FileManager::Unsafe::method()
 * - FileManager instance methods remain unchanged
 * 
 * Example:
 * ```cpp
 * // Unsafe (internal code only)
 * if (FileManager::Unsafe::exists("/path/to/file")) {
 *     auto data = FileManager::Unsafe::readBinaryFile("/path/to/file");
 * }
 * 
 * // Safe (public API)
 * FileManager fm("/var/lib/midimind");
 * fm.uploadFile(data, "file.mid", DirectoryType::UPLOADS);
 * ```
 */
class FileManager {
public:
    // ========================================================================
    // UNSAFE NAMESPACE (INTERNAL USE ONLY)
    // ========================================================================
    
    /**
     * @brief Fast filesystem operations without validation
     * @warning Use only in internal code where paths are trusted
     * @warning No security checks, no path validation
     */
    struct Unsafe {
        /**
         * @brief Check if file or directory exists
         */
        static inline bool exists(const std::string& path) {
            return fs::exists(path);
        }
        
        /**
         * @brief Check if path is a file
         */
        static inline bool isFile(const std::string& path) {
            return fs::exists(path) && fs::is_regular_file(path);
        }
        
        /**
         * @brief Check if path is a directory
         */
        static inline bool isDirectory(const std::string& path) {
            return fs::exists(path) && fs::is_directory(path);
        }
        
        /**
         * @brief Get file size in bytes
         */
        static inline size_t fileSize(const std::string& path) {
            try {
                return fs::file_size(path);
            } catch (...) {
                return 0;
            }
        }
        
        /**
         * @brief Create directory (with parents if recursive=true)
         */
        static inline bool createDirectory(const std::string& path, bool recursive = false) {
            try {
                if (recursive) {
                    return fs::create_directories(path);
                } else {
                    return fs::create_directory(path);
                }
            } catch (...) {
                return false;
            }
        }
        
        /**
         * @brief Delete file
         */
        static inline bool deleteFile(const std::string& path) {
            try {
                return fs::remove(path);
            } catch (...) {
                return false;
            }
        }
        
        /**
         * @brief Delete directory recursively
         */
        static inline bool deleteDirectory(const std::string& path) {
            try {
                return fs::remove_all(path) > 0;
            } catch (...) {
                return false;
            }
        }
        
        /**
         * @brief List files in directory
         */
        static inline std::vector<std::string> listFiles(const std::string& path) {
            std::vector<std::string> files;
            try {
                for (const auto& entry : fs::directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        files.push_back(entry.path().filename().string());
                    }
                }
            } catch (...) {
                // Return empty vector on error
            }
            return files;
        }
        
        /**
         * @brief List directories in directory
         */
        static inline std::vector<std::string> listDirectories(const std::string& path) {
            std::vector<std::string> dirs;
            try {
                for (const auto& entry : fs::directory_iterator(path)) {
                    if (entry.is_directory()) {
                        dirs.push_back(entry.path().filename().string());
                    }
                }
            } catch (...) {
                // Return empty vector on error
            }
            return dirs;
        }
        
        /**
         * @brief Read text file
         */
        static inline std::string readTextFile(const std::string& path) {
            std::ifstream file(path, std::ios::in);
            if (!file.is_open()) {
                return "";
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
        
        /**
         * @brief Write text file
         */
        static inline bool writeTextFile(const std::string& path, const std::string& content) {
            std::ofstream file(path, std::ios::out | std::ios::trunc);
            if (!file.is_open()) {
                return false;
            }
            file << content;
            return file.good();
        }
        
        /**
         * @brief Read binary file
         */
        static inline std::vector<uint8_t> readBinaryFile(const std::string& path) {
            std::ifstream file(path, std::ios::in | std::ios::binary);
            if (!file.is_open()) {
                return {};
            }
            return std::vector<uint8_t>(
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()
            );
        }
        
        /**
         * @brief Write binary file
         */
        static inline bool writeBinaryFile(const std::string& path, 
                                          const std::vector<uint8_t>& data) {
            std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                return false;
            }
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            return file.good();
        }
        
        /**
         * @brief Copy file
         */
        static inline bool copyFile(const std::string& source, const std::string& dest) {
            try {
                fs::copy_file(source, dest, fs::copy_options::overwrite_existing);
                return true;
            } catch (...) {
                return false;
            }
        }
        
        /**
         * @brief Move/rename file
         */
        static inline bool moveFile(const std::string& source, const std::string& dest) {
            try {
                fs::rename(source, dest);
                return true;
            } catch (...) {
                return false;
            }
        }
        
        /**
         * @brief Get file extension
         */
        static inline std::string getExtension(const std::string& path) {
            fs::path p(path);
            return p.extension().string();
        }
        
        /**
         * @brief Get filename without extension
         */
        static inline std::string getStem(const std::string& path) {
            fs::path p(path);
            return p.stem().string();
        }
        
        /**
         * @brief Get parent directory
         */
        static inline std::string getParent(const std::string& path) {
            fs::path p(path);
            return p.parent_path().string();
        }
    };
    
    // ========================================================================
    // SAFE METHODS (PUBLIC API)
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param rootPath Root directory for all operations
     */
    explicit FileManager(const std::string& rootPath);
    
    /**
     * @brief Destructor
     */
    ~FileManager();
    
    // Disable copy
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Initialize directory structure
     * @return true if successful
     * @note Creates all predefined directories
     */
    bool initializeDirectories();
    
    // ========================================================================
    // UPLOAD / DOWNLOAD
    // ========================================================================
    
    /**
     * @brief Upload file with validation
     * @param data File data
     * @param filename Original filename
     * @param destDir Destination directory type
     * @param overwrite Allow overwriting existing files
     * @return Path to uploaded file
     * @throws std::runtime_error on validation failure
     */
    std::string uploadFile(const std::vector<uint8_t>& data,
                          const std::string& filename,
                          DirectoryType destDir = DirectoryType::UPLOADS,
                          bool overwrite = false);
    
    /**
     * @brief Download file
     * @param filepath Relative file path
     * @return File data
     * @throws std::runtime_error if file not found
     */
    std::vector<uint8_t> downloadFile(const std::string& filepath);
    
    // ========================================================================
    // FILE OPERATIONS
    // ========================================================================
    
    /**
     * @brief Delete file
     * @param filepath Relative file path
     * @return true if successful
     */
    bool deleteFile(const std::string& filepath);
    
    /**
     * @brief Copy file
     * @param source Source relative path
     * @param dest Destination relative path
     * @return true if successful
     */
    bool copyFile(const std::string& source, const std::string& dest);
    
    /**
     * @brief Move/rename file
     * @param source Source relative path
     * @param dest Destination relative path
     * @return true if successful
     */
    bool moveFile(const std::string& source, const std::string& dest);
    
    // ========================================================================
    // DIRECTORY OPERATIONS
    // ========================================================================
    
    /**
     * @brief List files in directory
     * @param dirType Directory type
     * @return Vector of FileInfo
     */
    std::vector<FileInfo> listFiles(DirectoryType dirType);
    
    /**
     * @brief Get file info
     * @param filepath Relative file path
     * @return Optional FileInfo
     */
    std::optional<FileInfo> getFileInfo(const std::string& filepath);
    
    /**
     * @brief Get directory path
     * @param dirType Directory type
     * @return Absolute directory path
     */
    std::string getDirectoryPath(DirectoryType dirType) const;
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Sanitize filename (remove dangerous characters)
     * @param filename Original filename
     * @return Safe filename
     */
    std::string sanitizeFilename(const std::string& filename) const;
    
    /**
     * @brief Check if path is safe
     * @param path Path to check
     * @return true if safe
     */
    bool isPathSafe(const std::string& path) const;
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get storage statistics
     * @return JSON with statistics
     */
    json getStatistics() const;
    
private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Validate path (no traversal, no absolute)
     */
    bool validatePath(const std::string& path) const;
    
    /**
     * @brief Parse FileInfo from path
     */
    std::optional<FileInfo> parseFileInfo(const std::string& filepath, 
                                         DirectoryType dirType);
    
    /**
     * @brief Convert DirectoryType to string
     */
    std::string directoryTypeToString(DirectoryType type) const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    std::string rootPath_;
    mutable std::mutex mutex_;
    
    // Directory names
    static constexpr const char* DIR_LOGS = "logs";
    static constexpr const char* DIR_BACKUPS = "backups";
    static constexpr const char* DIR_EXPORTS = "exports";
    static constexpr const char* DIR_UPLOADS = "uploads";
    static constexpr const char* DIR_TEMP = "temp";
    
    // Limits
    static constexpr size_t MAX_UPLOAD_SIZE = 100 * 1024 * 1024; // 100 MB
    static constexpr size_t MAX_FILENAME_LENGTH = 255;
    
    // Allowed extensions
    std::vector<std::string> allowedExtensions_ = {
        ".mid", ".midi", ".json", ".txt", ".log", ".bak"
    };
};

} // namespace midiMind
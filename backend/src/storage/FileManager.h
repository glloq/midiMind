// ============================================================================
// File: backend/src/storage/FileManager.h
// Version: 4.4.0 - SECURE
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.4.0:
//   - Fixed: Unsafe namespace (was struct) with proper error logging
//   - Fixed: Read operations verify file.good() and file size
//   - Fixed: allowedExtensions_ now const
//   - Fixed: All exceptions properly logged
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

enum class DirectoryType {
    LOGS,
    BACKUPS,
    EXPORTS,
    UPLOADS,
    TEMP
};

struct FileInfo {
    std::string id;              // Unique file ID (filename)
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
            {"id", id},
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
// UNSAFE NAMESPACE (INTERNAL USE ONLY - MINIMAL VALIDATION)
// ============================================================================

namespace FileManagerUnsafe {
    
    inline bool exists(const std::string& path) {
        return fs::exists(path);
    }
    
    inline bool isFile(const std::string& path) {
        return fs::exists(path) && fs::is_regular_file(path);
    }
    
    inline bool isDirectory(const std::string& path) {
        return fs::exists(path) && fs::is_directory(path);
    }
    
    inline size_t fileSize(const std::string& path) {
        try {
            return fs::file_size(path);
        } catch (const std::exception& e) {
            Logger::error("FileManagerUnsafe", "fileSize failed: " + std::string(e.what()));
            return 0;
        }
    }
    
    inline bool createDirectory(const std::string& path, bool recursive = false) {
        try {
            if (recursive) {
                return fs::create_directories(path);
            } else {
                return fs::create_directory(path);
            }
        } catch (const std::exception& e) {
            Logger::error("FileManagerUnsafe", "createDirectory failed: " + std::string(e.what()));
            return false;
        }
    }
    
    inline bool deleteFile(const std::string& path) {
        try {
            return fs::remove(path);
        } catch (const std::exception& e) {
            Logger::error("FileManagerUnsafe", "deleteFile failed: " + std::string(e.what()));
            return false;
        }
    }
    
    inline bool deleteDirectory(const std::string& path) {
        try {
            return fs::remove_all(path) > 0;
        } catch (const std::exception& e) {
            Logger::error("FileManagerUnsafe", "deleteDirectory failed: " + std::string(e.what()));
            return false;
        }
    }
    
    inline std::vector<std::string> listFiles(const std::string& path) {
        std::vector<std::string> files;
        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().filename().string());
                }
            }
        } catch (const std::exception& e) {
            Logger::error("FileManagerUnsafe", "listFiles failed: " + std::string(e.what()));
        }
        return files;
    }
    
    inline std::vector<std::string> listDirectories(const std::string& path) {
        std::vector<std::string> dirs;
        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_directory()) {
                    dirs.push_back(entry.path().filename().string());
                }
            }
        } catch (const std::exception& e) {
            Logger::error("FileManagerUnsafe", "listDirectories failed: " + std::string(e.what()));
        }
        return dirs;
    }
    
    inline std::string readTextFile(const std::string& path) {
        std::ifstream file(path, std::ios::in);
        if (!file.is_open()) {
            Logger::error("FileManagerUnsafe", "Failed to open for reading: " + path);
            return "";
        }
        
        std::string content{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };
        
        if (!file.good() && !file.eof()) {
            Logger::error("FileManagerUnsafe", "Read error: " + path);
            return "";
        }
        
        return content;
    }
    
    inline bool writeTextFile(const std::string& path, const std::string& content) {
        std::ofstream file(path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            Logger::error("FileManagerUnsafe", "Failed to open for writing: " + path);
            return false;
        }
        file << content;
        bool success = file.good();
        file.close();
        if (!success) {
            Logger::error("FileManagerUnsafe", "Write error: " + path);
        }
        return success;
    }
    
    inline std::vector<uint8_t> readBinaryFile(const std::string& path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            Logger::error("FileManagerUnsafe", "Failed to open for reading: " + path);
            return {};
        }
        
        // Get file size first
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        if (size < 0) {
            Logger::error("FileManagerUnsafe", "Failed to get file size: " + path);
            return {};
        }
        
        std::vector<uint8_t> data{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };
        
        if (!file.good() && !file.eof()) {
            Logger::error("FileManagerUnsafe", "Read error: " + path);
            return {};
        }
        
        // Verify size matches
        if (static_cast<std::streamsize>(data.size()) != size) {
            Logger::warning("FileManagerUnsafe", 
                "Size mismatch for " + path + ": expected " + std::to_string(size) + 
                ", got " + std::to_string(data.size()));
        }
        
        return data;
    }
    
    inline bool writeBinaryFile(const std::string& path, 
                               const std::vector<uint8_t>& data) {
        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            Logger::error("FileManagerUnsafe", "Failed to open for writing: " + path);
            return false;
        }
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        bool success = file.good();
        file.close();
        if (!success) {
            Logger::error("FileManagerUnsafe", "Write error: " + path);
        }
        return success;
    }
    
    inline bool copyFile(const std::string& source, const std::string& dest) {
        try {
            fs::copy_file(source, dest, fs::copy_options::overwrite_existing);
            return true;
        } catch (const std::exception& e) {
            Logger::error("FileManagerUnsafe", "copyFile failed: " + std::string(e.what()));
            return false;
        }
    }
    
    inline bool moveFile(const std::string& source, const std::string& dest) {
        try {
            fs::rename(source, dest);
            return true;
        } catch (const std::exception& e) {
            Logger::error("FileManagerUnsafe", "moveFile failed: " + std::string(e.what()));
            return false;
        }
    }
    
    inline std::string getExtension(const std::string& path) {
        fs::path p(path);
        return p.extension().string();
    }
    
    inline std::string getStem(const std::string& path) {
        fs::path p(path);
        return p.stem().string();
    }
    
    inline std::string getParent(const std::string& path) {
        fs::path p(path);
        return p.parent_path().string();
    }

} // namespace FileManagerUnsafe

// ============================================================================
// CLASS: FileManager
// ============================================================================

/**
 * @class FileManager
 * @brief Thread-safe file manager with path validation
 * 
 * Provides two API levels:
 * - FileManagerUnsafe:: for internal use (minimal validation)
 * - FileManager:: for public use (full validation)
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses internal mutex for synchronization
 */
class FileManager {
public:
    // For backward compatibility - delegate to namespace
    using Unsafe = FileManagerUnsafe;
    
    explicit FileManager(const std::string& rootPath);
    ~FileManager();
    
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    bool initializeDirectories();
    
    // ========================================================================
    // UPLOAD / DOWNLOAD
    // ========================================================================
    
    std::string uploadFile(const std::vector<uint8_t>& data,
                          const std::string& filename,
                          DirectoryType destDir = DirectoryType::UPLOADS,
                          bool overwrite = false);
    
    std::vector<uint8_t> downloadFile(const std::string& filepath);
    
    FileInfo uploadFileBase64(const std::string& filename, const std::string& base64Data);
    std::string downloadFileBase64(const std::string& fileId);
    
    // ========================================================================
    // FILE OPERATIONS
    // ========================================================================
    
    bool deleteFile(const std::string& filepath);
    bool copyFile(const std::string& source, const std::string& dest);
    bool moveFile(const std::string& source, const std::string& dest);
    
    bool renameFile(const std::string& fileId, const std::string& newName);
    FileInfo copyFileByName(const std::string& fileId, const std::string& newName);
    
    // ========================================================================
    // DIRECTORY OPERATIONS
    // ========================================================================
    
    std::vector<FileInfo> listFiles(DirectoryType dirType);
    std::vector<FileInfo> listFiles();
    
    std::optional<FileInfo> getFileInfo(const std::string& filepath);
    std::string getDirectoryPath(DirectoryType dirType) const;
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    std::string sanitizeFilename(const std::string& filename) const;
    bool isPathSafe(const std::string& path) const;
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    json getStatistics() const;
    
private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    bool validatePath(const std::string& path) const;
    std::optional<FileInfo> parseFileInfo(const std::string& filepath, 
                                         DirectoryType dirType);
    std::string directoryTypeToString(DirectoryType type) const;
    
    /**
     * @brief Build validated full path from relative path
     * @throws ErrorCode::VALIDATION_FAILED if path escapes root
     */
    std::string buildFullPath(const std::string& relativePath) const;
    
    std::string base64Encode(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> base64Decode(const std::string& encoded) const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    std::string rootPath_;
    mutable std::mutex mutex_;
    
    static constexpr const char* DIR_LOGS = "logs";
    static constexpr const char* DIR_BACKUPS = "backups";
    static constexpr const char* DIR_EXPORTS = "exports";
    static constexpr const char* DIR_UPLOADS = "uploads";
    static constexpr const char* DIR_TEMP = "temp";
    
    static constexpr size_t MAX_UPLOAD_SIZE = 100 * 1024 * 1024;
    static constexpr size_t MAX_FILENAME_LENGTH = 255;
    
    const std::vector<std::string> allowedExtensions_ = {
        ".mid", ".midi", ".json", ".txt", ".log", ".bak"
    };
};

} // namespace midiMind
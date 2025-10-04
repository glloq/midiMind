// ============================================================================
// src/midi/filemanager/FileSystemScanner.h
// Responsabilité: Scanner le filesystem pour trouver les fichiers MIDI
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include "../../core/Logger.h"

namespace fs = std::filesystem;

namespace midiMind {

struct ScannedFile {
    std::string filepath;
    std::string relativePath;
    uint64_t sizeBytes;
    std::time_t lastModified;
};

class FileSystemScanner {
public:
    using ScanCallback = std::function<void(const ScannedFile&)>;
    
    FileSystemScanner(const std::string& rootDirectory)
        : rootDir_(rootDirectory) {}
    
    /**
     * @brief Scanne le répertoire pour trouver les fichiers MIDI
     * @param recursive Si true, scanne les sous-répertoires
     * @param callback Appelé pour chaque fichier trouvé
     * @return Nombre de fichiers trouvés
     */
    size_t scan(bool recursive, ScanCallback callback) {
        size_t count = 0;
        
        try {
            if (!fs::exists(rootDir_)) {
                Logger::error("FileSystemScanner", "Directory not found: " + rootDir_);
                return 0;
            }
            
            Logger::info("FileSystemScanner", "Scanning: " + rootDir_ + 
                        (recursive ? " (recursive)" : ""));
            
            if (recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(rootDir_)) {
                    if (processEntry(entry, callback)) {
                        count++;
                    }
                }
            } else {
                for (const auto& entry : fs::directory_iterator(rootDir_)) {
                    if (processEntry(entry, callback)) {
                        count++;
                    }
                }
            }
            
            Logger::info("FileSystemScanner", "Found " + std::to_string(count) + " MIDI files");
            
        } catch (const fs::filesystem_error& e) {
            Logger::error("FileSystemScanner", "Scan error: " + std::string(e.what()));
        }
        
        return count;
    }
    
    /**
     * @brief Vérifie si un fichier est un fichier MIDI
     */
    static bool isMidiFile(const std::string& filepath) {
        std::string ext = fs::path(filepath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return (ext == ".mid" || ext == ".midi");
    }

private:
    bool processEntry(const fs::directory_entry& entry, ScanCallback callback) {
        if (!entry.is_regular_file()) {
            return false;
        }
        
        if (!isMidiFile(entry.path().string())) {
            return false;
        }
        
        try {
            ScannedFile file;
            file.filepath = entry.path().string();
            file.relativePath = fs::relative(entry.path(), rootDir_).string();
            file.sizeBytes = entry.file_size();
            
            auto ftime = entry.last_write_time();
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            file.lastModified = std::chrono::system_clock::to_time_t(sctp);
            
            callback(file);
            return true;
            
        } catch (const std::exception& e) {
            Logger::warn("FileSystemScanner", 
                "Error processing file: " + entry.path().string() + " - " + e.what());
            return false;
        }
    }
    
    std::string rootDir_;
};

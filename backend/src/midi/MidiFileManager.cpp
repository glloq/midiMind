// ============================================================================
// Fichier: backend/src/midi/MidiFileManager.cpp
// Version: 3.0.4 - COMPLET (Partie 1/2)
// Date: 2025-10-13
// ============================================================================
// Description:
//   Implémentation du gestionnaire de bibliothèque de fichiers MIDI
//
// PARTIE 1: Constructeur, scan, indexation, récupération de données
// PARTIE 2: Modifications, conversion, cache, utilitaires
// ============================================================================

#include "MidiFileManager.h"
#include "../core/Logger.h"
#include "../core/Error.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

namespace midiMind {

// ============================================================================
// CONSTRUCTEUR MIDIFILEENTRY
// ============================================================================

MidiFileEntry::MidiFileEntry()
    : fileSizeBytes(0)
    , lastModified(0)
    , addedDate(0)
    , format(1)
    , division(480)
    , trackCount(0)
    , durationMs(0)
    , initialTempo(120.0)
    , timeSignature("4/4")
    , keySignature("C")
    , rating(0)
    , playCount(0)
    , lastPlayed(0)
{}

json MidiFileEntry::toJson() const {
    json j;
    j["id"] = id;
    j["filename"] = filename;
    j["filepath"] = filepath;
    j["relative_path"] = relativePath;
    j["directory"] = directory;
    j["file_size"] = fileSizeBytes;
    j["last_modified"] = lastModified;
    j["added_date"] = addedDate;
    j["format"] = format;
    j["division"] = division;
    j["track_count"] = trackCount;
    j["duration_ms"] = durationMs;
    j["initial_tempo"] = initialTempo;
    j["time_signature"] = timeSignature;
    j["key_signature"] = keySignature;
    j["title"] = title;
    j["composer"] = composer;
    j["copyright"] = copyright;
    j["comments"] = comments;
    j["tags"] = tags;
    j["rating"] = rating;
    j["play_count"] = playCount;
    j["last_played"] = lastPlayed;
    return j;
}

MidiFileEntry MidiFileEntry::fromJson(const json& j) {
    MidiFileEntry entry;
    entry.id = j.value("id", "");
    entry.filename = j.value("filename", "");
    entry.filepath = j.value("filepath", "");
    entry.relativePath = j.value("relative_path", "");
    entry.directory = j.value("directory", "");
    entry.fileSizeBytes = j.value("file_size", 0ULL);
    entry.lastModified = j.value("last_modified", 0ULL);
    entry.addedDate = j.value("added_date", 0ULL);
    entry.format = j.value("format", 1);
    entry.division = j.value("division", 480);
    entry.trackCount = j.value("track_count", 0);
    entry.durationMs = j.value("duration_ms", 0U);
    entry.initialTempo = j.value("initial_tempo", 120.0);
    entry.timeSignature = j.value("time_signature", "4/4");
    entry.keySignature = j.value("key_signature", "C");
    entry.title = j.value("title", "");
    entry.composer = j.value("composer", "");
    entry.copyright = j.value("copyright", "");
    entry.comments = j.value("comments", "");
    entry.tags = j.value("tags", "");
    entry.rating = j.value("rating", 0);
    entry.playCount = j.value("play_count", 0);
    entry.lastPlayed = j.value("last_played", 0ULL);
    return entry;
}

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR MIDIFILEMANAGER
// ============================================================================

MidiFileManager::MidiFileManager(std::shared_ptr<Database> database,
                                 const std::string& rootDirectory)
    : database_(database)
    , rootDirectory_(rootDirectory)
{
    Logger::info("MidiFileManager", "Initializing MidiFileManager...");
    Logger::info("MidiFileManager", "  Root directory: " + rootDirectory_);
    
    // Créer les composants
    analyzer_ = std::make_unique<MidiFileAnalyzer>();
    converter_ = std::make_unique<JsonMidiConverter>();
    
    // Vérifier que le répertoire existe
    if (!fs::exists(rootDirectory_)) {
        Logger::warn("MidiFileManager", "Root directory does not exist, creating: " + rootDirectory_);
        fs::create_directories(rootDirectory_);
    }
    
    // Charger le cache depuis la BDD
    loadCache();
    
    Logger::info("MidiFileManager", "✓ MidiFileManager initialized");
}

MidiFileManager::~MidiFileManager() {
    Logger::info("MidiFileManager", "Destroying MidiFileManager...");
}

// ============================================================================
// SCAN ET INDEXATION
// ============================================================================

size_t MidiFileManager::scanDirectory(const std::string& directory, bool recursive) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string scanPath = directory.empty() ? rootDirectory_ : directory;
    
    Logger::info("MidiFileManager", "Scanning directory: " + scanPath);
    Logger::info("MidiFileManager", "  Recursive: " + std::string(recursive ? "yes" : "no"));
    
    size_t filesFound = 0;
    
    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(scanPath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    if (ext == ".mid" || ext == ".midi") {
                        auto fileId = indexFile(entry.path().string());
                        if (fileId.has_value()) {
                            filesFound++;
                        }
                    }
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(scanPath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    if (ext == ".mid" || ext == ".midi") {
                        auto fileId = indexFile(entry.path().string());
                        if (fileId.has_value()) {
                            filesFound++;
                        }
                    }
                }
            }
        }
        
        Logger::info("MidiFileManager", "✓ Scan complete: " + std::to_string(filesFound) + " files found");
        
    } catch (const fs::filesystem_error& e) {
        Logger::error("MidiFileManager", "Scan failed: " + std::string(e.what()));
    }
    
    return filesFound;
}

std::optional<std::string> MidiFileManager::indexFile(const std::string& filepath) {
    // NOTE: mutex déjà acquis par scanDirectory()
    
    try {
        Logger::debug("MidiFileManager", "Indexing file: " + filepath);
        
        // Vérifier que le fichier existe
        if (!fs::exists(filepath)) {
            Logger::warn("MidiFileManager", "File does not exist: " + filepath);
            return std::nullopt;
        }
        
        // Vérifier si déjà indexé
        auto existingOpt = getFileByPath(filepath);
        if (existingOpt.has_value()) {
            Logger::debug("MidiFileManager", "File already indexed: " + filepath);
            return existingOpt->id;
        }
        
        // Créer l'entrée
        MidiFileEntry entry;
        entry.id = generateFileId();
        
        fs::path path(filepath);
        entry.filename = path.filename().string();
        entry.filepath = filepath;
        entry.relativePath = makeRelativePath(filepath);
        entry.directory = path.parent_path().string();
        
        // Métadonnées fichier
        entry.fileSizeBytes = fs::file_size(filepath);
        auto ftime = fs::last_write_time(filepath);
        entry.lastModified = std::chrono::duration_cast<std::chrono::seconds>(
            ftime.time_since_epoch()).count();
        
        auto now = std::chrono::system_clock::now();
        entry.addedDate = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        // Analyser le fichier MIDI
        try {
            auto analysis = analyzer_->analyze(filepath);
            
            entry.format = analysis.value("format", 1);
            entry.division = analysis.value("division", 480);
            entry.trackCount = analysis.value("track_count", 0);
            entry.durationMs = analysis.value("duration_ms", 0U);
            entry.initialTempo = analysis.value("initial_tempo", 120.0);
            entry.timeSignature = analysis.value("time_signature", "4/4");
            entry.keySignature = analysis.value("key_signature", "C");
            entry.title = analysis.value("title", entry.filename);
            entry.composer = analysis.value("composer", "");
            entry.copyright = analysis.value("copyright", "");
            entry.comments = analysis.value("comments", "");
            
        } catch (const std::exception& e) {
            Logger::warn("MidiFileManager", "Analysis failed: " + std::string(e.what()));
            // Continuer avec des valeurs par défaut
            entry.title = entry.filename;
        }
        
        // Sauvegarder en BDD
        syncDatabase(entry);
        
        // Ajouter au cache
        updateCache(entry);
        
        Logger::debug("MidiFileManager", "✓ File indexed: " + entry.id);
        
        return entry.id;
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to index file: " + std::string(e.what()));
        return std::nullopt;
    }
}

void MidiFileManager::reindexAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiFileManager", "Reindexing all files...");
    
    // Vider le cache
    cache_.clear();
    pathToIdCache_.clear();
    
    // Scanner récursivement
    scanDirectory(rootDirectory_, true);
    
    Logger::info("MidiFileManager", "✓ Reindex complete");
}

// ============================================================================
// RÉCUPÉRATION DE DONNÉES
// ============================================================================

std::optional<MidiFileEntry> MidiFileManager::getFileMetadata(const std::string& fileId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Chercher dans le cache
    auto it = cache_.find(fileId);
    if (it != cache_.end()) {
        return it->second;
    }
    
    // Charger depuis la BDD
    return loadFromDatabase(fileId);
}

std::optional<MidiFileEntry> MidiFileManager::getFileByPath(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Chercher dans pathToIdCache
    auto it = pathToIdCache_.find(filepath);
    if (it != pathToIdCache_.end()) {
        return getFileMetadata(it->second);
    }
    
    // Chercher en BDD
    try {
        auto result = database_->query(
            "SELECT * FROM midi_files WHERE filepath = ?",
            {filepath}
        );
        
        if (!result.success || result.rows.empty()) {
            return std::nullopt;
        }
        
        auto entry = rowToEntry(result.rows[0]);
        updateCache(entry);
        return entry;
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to get file by path: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<MidiFileEntry> MidiFileManager::listFiles(const std::string& directory) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<MidiFileEntry> files;
    
    try {
        std::string sql;
        std::vector<std::string> params;
        
        if (directory.empty()) {
            sql = "SELECT * FROM midi_files ORDER BY title, filename";
        } else {
            sql = "SELECT * FROM midi_files WHERE directory = ? ORDER BY title, filename";
            params.push_back(directory);
        }
        
        auto result = database_->query(sql, params);
        
        if (result.success) {
            for (const auto& row : result.rows) {
                files.push_back(rowToEntry(row));
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to list files: " + std::string(e.what()));
    }
    
    return files;
}

json MidiFileManager::getLibraryStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    
    try {
        auto result = database_->query(
            "SELECT "
            "  COUNT(*) as count, "
            "  SUM(file_size) as total_size, "
            "  SUM(duration_ms) as total_duration, "
            "  AVG(rating) as avg_rating "
            "FROM midi_files"
        );
        
        if (result.success && !result.rows.empty()) {
            const auto& row = result.rows[0];
            stats["file_count"] = std::stoi(row.at("count"));
            stats["total_size_bytes"] = std::stoull(row.at("total_size"));
            stats["total_duration_ms"] = std::stoull(row.at("total_duration"));
            stats["average_rating"] = std::stod(row.at("avg_rating"));
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to get stats: " + std::string(e.what()));
        stats["error"] = e.what();
    }
    
    return stats;
}


// ============================================================================
// MODIFICATION DE FICHIERS
// ============================================================================

std::optional<std::string> MidiFileManager::renameFile(const std::string& filepath, 
                                                       const std::string& newName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        Logger::info("MidiFileManager", "Renaming file: " + filepath + " -> " + newName);
        
        // Vérifier que le fichier existe
        if (!fs::exists(filepath)) {
            Logger::error("MidiFileManager", "File does not exist: " + filepath);
            return std::nullopt;
        }
        
        // Construire nouveau chemin
        fs::path oldPath(filepath);
        fs::path newPath = oldPath.parent_path() / newName;
        
        // Vérifier que le nouveau nom n'existe pas déjà
        if (fs::exists(newPath)) {
            Logger::error("MidiFileManager", "File already exists: " + newPath.string());
            return std::nullopt;
        }
        
        // Renommer le fichier physiquement
        fs::rename(oldPath, newPath);
        
        // Mettre à jour la BDD
        database_->execute(
            "UPDATE midi_files SET filename = ?, filepath = ? WHERE filepath = ?",
            {newName, newPath.string(), filepath}
        );
        
        // Mettre à jour le cache
        updateFilePath(oldPath.string(), newPath.string());
        
        Logger::info("MidiFileManager", "✓ File renamed successfully");
        
        return newPath.string();
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to rename file: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<std::string> MidiFileManager::moveFile(const std::string& fileId, 
                                                     const std::string& newDirectory) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        Logger::info("MidiFileManager", "Moving file: " + fileId + " -> " + newDirectory);
        
        // Récupérer le fichier
        auto fileOpt = getFileMetadata(fileId);
        if (!fileOpt.has_value()) {
            Logger::error("MidiFileManager", "File not found: " + fileId);
            return std::nullopt;
        }
        
        const auto& file = fileOpt.value();
        
        // Vérifier que le répertoire de destination existe
        if (!fs::exists(newDirectory)) {
            Logger::info("MidiFileManager", "Creating directory: " + newDirectory);
            fs::create_directories(newDirectory);
        }
        
        // Construire le nouveau chemin
        fs::path oldPath(file.filepath);
        fs::path newPath = fs::path(newDirectory) / oldPath.filename();
        
        // Vérifier que le fichier n'existe pas déjà
        if (fs::exists(newPath)) {
            Logger::error("MidiFileManager", "File already exists at destination: " + newPath.string());
            return std::nullopt;
        }
        
        // Déplacer le fichier physiquement
        fs::rename(oldPath, newPath);
        
        // Calculer le nouveau chemin relatif
        std::string newRelativePath = makeRelativePath(newPath.string());
        
        // Mettre à jour la BDD
        database_->execute(
            "UPDATE midi_files SET filepath = ?, directory = ?, relative_path = ? WHERE id = ?",
            {newPath.string(), newDirectory, newRelativePath, fileId}
        );
        
        // Mettre à jour le cache
        updateFilePath(oldPath.string(), newPath.string());
        
        Logger::info("MidiFileManager", "✓ File moved successfully");
        
        return newPath.string();
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to move file: " + std::string(e.what()));
        return std::nullopt;
    }
}

void MidiFileManager::updateFilePath(const std::string& oldPath, const std::string& newPath) {
    // NOTE: mutex déjà acquis par les fonctions appelantes
    
    Logger::debug("MidiFileManager", "Updating file path in cache: " + oldPath + " -> " + newPath);
    
    try {
        // Trouver l'ID du fichier
        auto it = pathToIdCache_.find(oldPath);
        if (it == pathToIdCache_.end()) {
            Logger::warn("MidiFileManager", "File path not found in cache: " + oldPath);
            return;
        }
        
        std::string fileId = it->second;
        
        // Mettre à jour pathToIdCache_
        pathToIdCache_.erase(it);
        pathToIdCache_[newPath] = fileId;
        
        // Mettre à jour le cache principal
        auto cacheIt = cache_.find(fileId);
        if (cacheIt != cache_.end()) {
            MidiFileEntry& entry = cacheIt->second;
            
            fs::path path(newPath);
            entry.filepath = newPath;
            entry.filename = path.filename().string();
            entry.directory = path.parent_path().string();
            entry.relativePath = makeRelativePath(newPath);
        }
        
        Logger::debug("MidiFileManager", "✓ File path updated in cache");
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Error updating file path: " + std::string(e.what()));
    }
}

std::string MidiFileManager::uploadFile(const std::string& filename, 
                                       const std::string& base64Data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        Logger::info("MidiFileManager", "Uploading file: " + filename);
        
        // TODO: Décoder base64
        // Pour l'instant, on suppose que base64Data est déjà décodé
        
        // Construire le chemin de destination
        fs::path destPath = fs::path(rootDirectory_) / "uploads" / filename;
        
        // Créer le répertoire uploads si nécessaire
        fs::create_directories(destPath.parent_path());
        
        // TODO: Écrire les données décodées dans le fichier
        
        // Indexer le fichier
        auto fileId = indexFile(destPath.string());
        
        if (fileId.has_value()) {
            Logger::info("MidiFileManager", "✓ File uploaded: " + fileId.value());
            return fileId.value();
        }
        
        return "";
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to upload file: " + std::string(e.what()));
        return "";
    }
}

bool MidiFileManager::deleteFile(const std::string& fileId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        Logger::info("MidiFileManager", "Deleting file: " + fileId);
        
        // Récupérer le fichier
        auto fileOpt = getFileMetadata(fileId);
        if (!fileOpt.has_value()) {
            Logger::error("MidiFileManager", "File not found: " + fileId);
            return false;
        }
        
        const auto& file = fileOpt.value();
        
        // Supprimer le fichier physique
        if (fs::exists(file.filepath)) {
            fs::remove(file.filepath);
        }
        
        // Supprimer de la BDD
        database_->execute("DELETE FROM midi_files WHERE id = ?", {fileId});
        
        // Supprimer du cache
        removeFromCache(fileId);
        
        Logger::info("MidiFileManager", "✓ File deleted");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to delete file: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// TAGS ET RATING
// ============================================================================

bool MidiFileManager::updateTags(const std::string& fileId, 
                                 const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Convertir les tags en CSV
        std::string tagsCsv;
        for (size_t i = 0; i < tags.size(); i++) {
            tagsCsv += tags[i];
            if (i < tags.size() - 1) {
                tagsCsv += ",";
            }
        }
        
        // Mettre à jour la BDD
        database_->execute(
            "UPDATE midi_files SET tags = ? WHERE id = ?",
            {tagsCsv, fileId}
        );
        
        // Mettre à jour le cache
        auto it = cache_.find(fileId);
        if (it != cache_.end()) {
            it->second.tags = tagsCsv;
        }
        
        Logger::debug("MidiFileManager", "✓ Tags updated for file: " + fileId);
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to update tags: " + std::string(e.what()));
        return false;
    }
}

bool MidiFileManager::updateRating(const std::string& fileId, int rating) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Valider le rating
        if (rating < 0 || rating > 5) {
            Logger::error("MidiFileManager", "Invalid rating: " + std::to_string(rating));
            return false;
        }
        
        // Mettre à jour la BDD
        database_->execute(
            "UPDATE midi_files SET rating = ? WHERE id = ?",
            {std::to_string(rating), fileId}
        );
        
        // Mettre à jour le cache
        auto it = cache_.find(fileId);
        if (it != cache_.end()) {
            it->second.rating = rating;
        }
        
        Logger::debug("MidiFileManager", "✓ Rating updated for file: " + fileId);
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to update rating: " + std::string(e.what()));
        return false;
    }
}

void MidiFileManager::incrementPlayCount(const std::string& fileId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto now = std::chrono::system_clock::now();
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        database_->execute(
            "UPDATE midi_files SET play_count = play_count + 1, last_played = ? WHERE id = ?",
            {std::to_string(timestamp), fileId}
        );
        
        // Mettre à jour le cache
        auto it = cache_.find(fileId);
        if (it != cache_.end()) {
            it->second.playCount++;
            it->second.lastPlayed = timestamp;
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to increment play count: " + std::string(e.what()));
    }
}

// ============================================================================
// RECHERCHE ET FILTRAGE
// ============================================================================

std::vector<MidiFileEntry> MidiFileManager::search(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<MidiFileEntry> results;
    
    try {
        std::string searchQuery = "%" + query + "%";
        
        auto result = database_->query(
            "SELECT * FROM midi_files WHERE "
            "title LIKE ? OR composer LIKE ? OR filename LIKE ? OR comments LIKE ?",
            {searchQuery, searchQuery, searchQuery, searchQuery}
        );
        
        if (result.success) {
            for (const auto& row : result.rows) {
                results.push_back(rowToEntry(row));
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Search failed: " + std::string(e.what()));
    }
    
    return results;
}

std::vector<MidiFileEntry> MidiFileManager::filterByTags(const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<MidiFileEntry> results;
    
    try {
        auto result = database_->query("SELECT * FROM midi_files");
        
        if (result.success) {
            for (const auto& row : result.rows) {
                auto entry = rowToEntry(row);
                
                // Vérifier que tous les tags sont présents
                bool hasAllTags = true;
                for (const auto& tag : tags) {
                    if (entry.tags.find(tag) == std::string::npos) {
                        hasAllTags = false;
                        break;
                    }
                }
                
                if (hasAllTags) {
                    results.push_back(entry);
                }
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Filter by tags failed: " + std::string(e.what()));
    }
    
    return results;
}

std::vector<MidiFileEntry> MidiFileManager::filterByRating(int minRating) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<MidiFileEntry> results;
    
    try {
        auto result = database_->query(
            "SELECT * FROM midi_files WHERE rating >= ?",
            {std::to_string(minRating)}
        );
        
        if (result.success) {
            for (const auto& row : result.rows) {
                results.push_back(rowToEntry(row));
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Filter by rating failed: " + std::string(e.what()));
    }
    
    return results;
}

std::vector<MidiFileEntry> MidiFileManager::filterByComposer(const std::string& composer) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<MidiFileEntry> results;
    
    try {
        std::string searchQuery = "%" + composer + "%";
        
        auto result = database_->query(
            "SELECT * FROM midi_files WHERE composer LIKE ?",
            {searchQuery}
        );
        
        if (result.success) {
            for (const auto& row : result.rows) {
                results.push_back(rowToEntry(row));
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Filter by composer failed: " + std::string(e.what()));
    }
    
    return results;
}

// ============================================================================
// CONVERSION JSONMIDI
// ============================================================================

std::optional<json> MidiFileManager::convertToJsonMidi(const std::string& fileId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Récupérer le fichier
        auto fileOpt = getFileMetadata(fileId);
        if (!fileOpt.has_value()) {
            Logger::error("MidiFileManager", "File not found: " + fileId);
            return std::nullopt;
        }
        
        const auto& file = fileOpt.value();
        
        // Vérifier le cache JsonMidi en BDD
        if (!file.jsonmidi.empty()) {
            Logger::debug("MidiFileManager", "Using cached JsonMidi for: " + fileId);
            return json::parse(file.jsonmidi);
        }
        
        // Convertir le fichier MIDI
        Logger::debug("MidiFileManager", "Converting MIDI to JsonMidi: " + file.filepath);
        
        auto jsonmidi = converter_->fromMidiFile(file.filepath);
        
        // Sauvegarder en cache (BDD)
        database_->execute(
            "UPDATE midi_files SET jsonmidi = ? WHERE id = ?",
            {jsonmidi.dump(), fileId}
        );
        
        return jsonmidi;
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to convert to JsonMidi: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<std::string> MidiFileManager::saveFromJsonMidi(const json& jsonmidi, 
                                                            const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        Logger::info("MidiFileManager", "Saving JsonMidi as: " + filename);
        
        // Construire le chemin de destination
        fs::path destPath = fs::path(rootDirectory_) / "generated" / filename;
        
        // Créer le répertoire si nécessaire
        fs::create_directories(destPath.parent_path());
        
        // Convertir JsonMidi en MIDI
        converter_->toMidiFile(jsonmidi, destPath.string());
        
        // Indexer le fichier créé
        auto fileId = indexFile(destPath.string());
        
        if (fileId.has_value()) {
            Logger::info("MidiFileManager", "✓ JsonMidi saved: " + fileId.value());
            return fileId.value();
        }
        
        return std::nullopt;
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to save JsonMidi: " + std::string(e.what()));
        return std::nullopt;
    }
}

// ============================================================================
// CACHE
// ============================================================================

void MidiFileManager::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiFileManager", "Clearing cache...");
    
    cache_.clear();
    pathToIdCache_.clear();
    
    Logger::info("MidiFileManager", "✓ Cache cleared");
}

void MidiFileManager::warmupCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiFileManager", "Warming up cache (this may take a while)...");
    
    try {
        auto result = database_->query("SELECT id FROM midi_files");
        
        if (result.success) {
            for (const auto& row : result.rows) {
                const std::string& fileId = row.at("id");
                
                // Convertir en JsonMidi (sera caché automatiquement)
                convertToJsonMidi(fileId);
            }
        }
        
        Logger::info("MidiFileManager", "✓ Cache warmup complete");
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Cache warmup failed: " + std::string(e.what()));
    }
}

void MidiFileManager::loadCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiFileManager", "Loading cache from database...");
    
    try {
        auto result = database_->query("SELECT * FROM midi_files");
        
        if (result.success) {
            for (const auto& row : result.rows) {
                auto entry = rowToEntry(row);
                cache_[entry.id] = entry;
                pathToIdCache_[entry.filepath] = entry.id;
            }
        }
        
        Logger::info("MidiFileManager", "✓ Cache loaded: " + std::to_string(cache_.size()) + " entries");
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to load cache: " + std::string(e.what()));
    }
}

// ============================================================================
// STATISTIQUES
// ============================================================================

size_t MidiFileManager::getFileCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

uint64_t MidiFileManager::getTotalSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t total = 0;
    for (const auto& pair : cache_) {
        total += pair.second.fileSizeBytes;
    }
    return total;
}

uint64_t MidiFileManager::getTotalDuration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t total = 0;
    for (const auto& pair : cache_) {
        total += pair.second.durationMs;
    }
    return total;
}



json MidiFileManager::getStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::debug("MidiFileManager", "Computing library statistics...");
    
    try {
        // Compteurs
        uint64_t totalSize = 0;
        uint64_t totalDuration = 0;
        int maxRating = 0;
        int totalPlays = 0;
        
        // Distributioncomposer
        std::unordered_map<std::string, int> composerCounts;
        std::unordered_map<int, int> ratingDistribution = {
            {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}
        };
        
        // Format distribution
        std::unordered_map<int, int> formatCounts = {
            {0, 0}, {1, 0}, {2, 0}
        };
        
        // Track count stats
        int minTracks = INT_MAX;
        int maxTracks = 0;
        double totalTracks = 0;
        
        // Parcourir tout le cache
        for (const auto& [id, entry] : cache_) {
            // Taille et durée
            totalSize += entry.fileSizeBytes;
            totalDuration += entry.durationMs;
            
            // Rating
            if (entry.rating > maxRating) {
                maxRating = entry.rating;
            }
            ratingDistribution[entry.rating]++;
            
            // Play count
            totalPlays += entry.playCount;
            
            // Compositeur
            if (!entry.composer.empty()) {
                composerCounts[entry.composer]++;
            }
            
            // Format
            formatCounts[entry.format]++;
            
            // Tracks
            if (entry.trackCount < minTracks) {
                minTracks = entry.trackCount;
            }
            if (entry.trackCount > maxTracks) {
                maxTracks = entry.trackCount;
            }
            totalTracks += entry.trackCount;
        }
        
        size_t fileCount = cache_.size();
        
        // Calculer moyennes
        double avgFileSize = fileCount > 0 ? 
            static_cast<double>(totalSize) / fileCount : 0.0;
        double avgDuration = fileCount > 0 ? 
            static_cast<double>(totalDuration) / fileCount : 0.0;
        double avgTracks = fileCount > 0 ? 
            totalTracks / fileCount : 0.0;
        
        // Top 5 compositeurs
        std::vector<std::pair<std::string, int>> composerVec(
            composerCounts.begin(), composerCounts.end());
        std::sort(composerVec.begin(), composerVec.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        json topComposers = json::array();
        for (size_t i = 0; i < std::min(size_t(5), composerVec.size()); i++) {
            topComposers.push_back({
                {"name", composerVec[i].first},
                {"count", composerVec[i].second}
            });
        }
        
        // Construire résultat JSON
        json stats = {
            // Counts
            {"total_files", fileCount},
            
            // Sizes
            {"total_size_bytes", totalSize},
            {"total_size_mb", totalSize / (1024.0 * 1024.0)},
            {"total_size_gb", totalSize / (1024.0 * 1024.0 * 1024.0)},
            {"average_file_size_bytes", static_cast<uint64_t>(avgFileSize)},
            {"average_file_size_mb", avgFileSize / (1024.0 * 1024.0)},
            
            // Durations
            {"total_duration_ms", totalDuration},
            {"total_duration_seconds", totalDuration / 1000.0},
            {"total_duration_minutes", totalDuration / (1000.0 * 60.0)},
            {"total_duration_hours", totalDuration / (1000.0 * 3600.0)},
            {"average_duration_ms", static_cast<uint32_t>(avgDuration)},
            {"average_duration_seconds", avgDuration / 1000.0},
            {"average_duration_minutes", avgDuration / (1000.0 * 60.0)},
            
            // Ratings
            {"highest_rating", maxRating},
            {"rating_distribution", ratingDistribution},
            
            // Playback
            {"total_plays", totalPlays},
            
            // Tracks
            {"min_tracks", minTracks == INT_MAX ? 0 : minTracks},
            {"max_tracks", maxTracks},
            {"average_tracks", avgTracks},
            
            // Formats
            {"format_distribution", {
                {"format_0", formatCounts[0]},
                {"format_1", formatCounts[1]},
                {"format_2", formatCounts[2]}
            }},
            
            // Composers
            {"unique_composers", composerCounts.size()},
            {"top_composers", topComposers}
        };
        
        Logger::debug("MidiFileManager", "✓ Statistics computed");
        
        return stats;
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", 
            "Failed to compute statistics: " + std::string(e.what()));
        
        // Retourner stats minimales en cas d'erreur
        return {
            {"total_files", cache_.size()},
            {"error", e.what()}
        };
    }
}
// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

std::string MidiFileManager::generateFileId() const {
    static std::atomic<uint32_t> counter{1};
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "file_" << timestamp << "_" << std::setfill('0') << std::setw(6) << counter++;
    
    return ss.str();
}

void MidiFileManager::updateCache(const MidiFileEntry& entry) {
    // NOTE: mutex déjà acquis par les fonctions appelantes
    cache_[entry.id] = entry;
    pathToIdCache_[entry.filepath] = entry.id;
}

void MidiFileManager::removeFromCache(const std::string& fileId) {
    // NOTE: mutex déjà acquis par les fonctions appelantes
    
    auto it = cache_.find(fileId);
    if (it != cache_.end()) {
        // Supprimer du pathToIdCache
        pathToIdCache_.erase(it->second.filepath);
        
        // Supprimer du cache principal
        cache_.erase(it);
    }
}

void MidiFileManager::syncDatabase(const MidiFileEntry& entry) {
    // NOTE: mutex déjà acquis par les fonctions appelantes
    
    try {
        // Vérifier si l'entrée existe déjà
        auto result = database_->query(
            "SELECT id FROM midi_files WHERE id = ?",
            {entry.id}
        );
        
        if (result.success && !result.rows.empty()) {
            // UPDATE
            database_->execute(
                "UPDATE midi_files SET "
                "filename = ?, filepath = ?, relative_path = ?, directory = ?, "
                "file_size = ?, last_modified = ?, "
                "format = ?, division = ?, track_count = ?, duration_ms = ?, "
                "initial_tempo = ?, time_signature = ?, key_signature = ?, "
                "title = ?, composer = ?, copyright = ?, comments = ?, "
                "tags = ?, rating = ?, play_count = ?, last_played = ? "
                "WHERE id = ?",
                {
                    entry.filename, entry.filepath, entry.relativePath, entry.directory,
                    std::to_string(entry.fileSizeBytes), std::to_string(entry.lastModified),
                    std::to_string(entry.format), std::to_string(entry.division),
                    std::to_string(entry.trackCount), std::to_string(entry.durationMs),
                    std::to_string(entry.initialTempo), entry.timeSignature, entry.keySignature,
                    entry.title, entry.composer, entry.copyright, entry.comments,
                    entry.tags, std::to_string(entry.rating),
                    std::to_string(entry.playCount), std::to_string(entry.lastPlayed),
                    entry.id
                }
            );
        } else {
            // INSERT
            database_->execute(
                "INSERT INTO midi_files ("
                "id, filename, filepath, relative_path, directory, "
                "file_size, added_date, last_modified, "
                "format, division, track_count, duration_ms, "
                "initial_tempo, time_signature, key_signature, "
                "title, composer, copyright, comments, "
                "tags, rating, play_count, last_played) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                {
                    entry.id, entry.filename, entry.filepath, entry.relativePath, entry.directory,
                    std::to_string(entry.fileSizeBytes), std::to_string(entry.addedDate),
                    std::to_string(entry.lastModified),
                    std::to_string(entry.format), std::to_string(entry.division),
                    std::to_string(entry.trackCount), std::to_string(entry.durationMs),
                    std::to_string(entry.initialTempo), entry.timeSignature, entry.keySignature,
                    entry.title, entry.composer, entry.copyright, entry.comments,
                    entry.tags, std::to_string(entry.rating),
                    std::to_string(entry.playCount), std::to_string(entry.lastPlayed)
                }
            );
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to sync database: " + std::string(e.what()));
    }
}

std::optional<MidiFileEntry> MidiFileManager::loadFromDatabase(const std::string& fileId) {
    // NOTE: mutex déjà acquis par les fonctions appelantes
    
    try {
        auto result = database_->query(
            "SELECT * FROM midi_files WHERE id = ?",
            {fileId}
        );
        
        if (!result.success || result.rows.empty()) {
            return std::nullopt;
        }
        
        auto entry = rowToEntry(result.rows[0]);
        updateCache(entry);
        return entry;
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileManager", "Failed to load from database: " + std::string(e.what()));
        return std::nullopt;
    }
}

MidiFileEntry MidiFileManager::rowToEntry(const DatabaseRow& row) const {
    MidiFileEntry entry;
    
    entry.id = row.at("id");
    entry.filename = row.at("filename");
    entry.filepath = row.at("filepath");
    entry.relativePath = row.at("relative_path");
    entry.directory = row.at("directory");
    
    entry.fileSizeBytes = std::stoull(row.at("file_size"));
    entry.lastModified = std::stoull(row.at("last_modified"));
    entry.addedDate = std::stoull(row.at("added_date"));
    
    entry.format = std::stoi(row.at("format"));
    entry.division = std::stoi(row.at("division"));
    entry.trackCount = std::stoi(row.at("track_count"));
    entry.durationMs = std::stoul(row.at("duration_ms"));
    entry.initialTempo = std::stod(row.at("initial_tempo"));
    entry.timeSignature = row.at("time_signature");
    entry.keySignature = row.at("key_signature");
    
    entry.title = row.at("title");
    entry.composer = row.at("composer");
    entry.copyright = row.at("copyright");
    entry.comments = row.at("comments");
    
    entry.tags = row.at("tags");
    entry.rating = std::stoi(row.at("rating"));
    entry.playCount = std::stoi(row.at("play_count"));
    entry.lastPlayed = std::stoull(row.at("last_played"));
    
    if (row.find("jsonmidi") != row.end()) {
        entry.jsonmidi = row.at("jsonmidi");
    }
    
    return entry;
}

std::string MidiFileManager::makeRelativePath(const std::string& absolutePath) const {
    if (absolutePath.find(rootDirectory_) == 0) {
        return absolutePath.substr(rootDirectory_.length() + 1);
    }
    return absolutePath;
}

bool MidiFileManager::isValidMidiData(const std::vector<uint8_t>& data) const {
    // Vérifier signature "MThd"
    if (data.size() < 4) {
        return false;
    }
    
    return data[0] == 'M' && data[1] == 'T' && 
           data[2] == 'h' && data[3] == 'd';
}

// ============================================================================
// FIN DU FICHIER MidiFileManager.cpp v3.0.4-COMPLET
// ============================================================================

} // namespace midiMind
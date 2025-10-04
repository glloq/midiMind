// ============================================================================
// src/midi/filemanager/MidiFileDatabase.h
// Responsabilité: Accès à la base de données SQLite des fichiers MIDI
// ============================================================================

#include <sqlite3.h>
#include <optional>
#include <vector>
#include "../MidiFileAnalyzer.h"

namespace midiMind {

struct MidiFileEntry {
    std::string id;
    std::string filename;
    std::string filepath;
    std::string relativePath;
    
    uint32_t durationMs;
    uint16_t trackCount;
    std::string title;
    std::string composer;
    std::vector<std::string> tags;
    
    uint64_t fileSizeBytes;
    std::time_t lastModified;
    std::time_t addedDate;
    
    uint32_t playCount;
    std::time_t lastPlayed;
    
    json toJson() const {
        json j;
        j["id"] = id;
        j["filename"] = filename;
        j["filepath"] = filepath;
        j["relative_path"] = relativePath;
        j["duration_ms"] = durationMs;
        j["track_count"] = trackCount;
        j["title"] = title;
        j["composer"] = composer;
        j["tags"] = tags;
        j["file_size"] = fileSizeBytes;
        j["last_modified"] = lastModified;
        j["added_date"] = addedDate;
        j["play_count"] = playCount;
        j["last_played"] = lastPlayed;
        return j;
    }
};

class MidiFileDatabase {
public:
    MidiFileDatabase(const std::string& dbPath) : db_(nullptr) {
        if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
            Logger::error("MidiFileDatabase", "Failed to open: " + dbPath);
            return;
        }
        
        createTables();
        Logger::info("MidiFileDatabase", "Database opened: " + dbPath);
    }
    
    ~MidiFileDatabase() {
        if (db_) {
            sqlite3_close(db_);
        }
    }
    
    /**
     * @brief Insère ou met à jour un fichier
     */
    bool insertOrUpdate(const MidiFileEntry& entry, bool isUpdate = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const char* sql = isUpdate ? R"(
            UPDATE midi_files SET
                filename = ?, filepath = ?, relative_path = ?,
                duration_ms = ?, track_count = ?, title = ?, composer = ?,
                tags = ?, file_size = ?, last_modified = ?
            WHERE id = ?
        )" : R"(
            INSERT INTO midi_files (
                id, filename, filepath, relative_path, duration_ms, track_count,
                title, composer, tags, file_size, last_modified, added_date
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            Logger::error("MidiFileDatabase", "Prepare failed: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
        
        // Bind parameters
        int idx = 1;
        if (!isUpdate) {
            sqlite3_bind_text(stmt, idx++, entry.id.c_str(), -1, SQLITE_TRANSIENT);
        }
        sqlite3_bind_text(stmt, idx++, entry.filename.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, entry.filepath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, entry.relativePath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, idx++, entry.durationMs);
        sqlite3_bind_int(stmt, idx++, entry.trackCount);
        sqlite3_bind_text(stmt, idx++, entry.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, entry.composer.c_str(), -1, SQLITE_TRANSIENT);
        
        // Sérialiser tags en JSON
        json tagsJson = entry.tags;
        std::string tagsStr = tagsJson.dump();
        sqlite3_bind_text(stmt, idx++, tagsStr.c_str(), -1, SQLITE_TRANSIENT);
        
        sqlite3_bind_int64(stmt, idx++, entry.fileSizeBytes);
        sqlite3_bind_int64(stmt, idx++, entry.lastModified);
        
        if (!isUpdate) {
            sqlite3_bind_int64(stmt, idx++, entry.addedDate);
        } else {
            sqlite3_bind_text(stmt, idx++, entry.id.c_str(), -1, SQLITE_TRANSIENT);
        }
        
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        
        if (!success) {
            Logger::error("MidiFileDatabase", "Insert/Update failed: " + std::string(sqlite3_errmsg(db_)));
        }
        
        return success;
    }
    
    /**
     * @brief Récupère un fichier par ID
     */
    std::optional<MidiFileEntry> getById(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const char* sql = "SELECT * FROM midi_files WHERE id = ?";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return std::nullopt;
        }
        
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        
        std::optional<MidiFileEntry> result;
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = rowToEntry(stmt);
        }
        
        sqlite3_finalize(stmt);
        return result;
    }
    
    /**
     * @brief Liste les fichiers avec pagination
     */
    std::vector<MidiFileEntry> list(int limit, int offset) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const char* sql = "SELECT * FROM midi_files ORDER BY added_date DESC LIMIT ? OFFSET ?";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return {};
        }
        
        sqlite3_bind_int(stmt, 1, limit);
        sqlite3_bind_int(stmt, 2, offset);
        
        std::vector<MidiFileEntry> results;
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            results.push_back(rowToEntry(stmt));
        }
        
        sqlite3_finalize(stmt);
        return results;
    }
    
    /**
     * @brief Recherche par nom, compositeur ou tags
     */
    std::vector<MidiFileEntry> search(const std::string& query) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const char* sql = R"(
            SELECT * FROM midi_files 
            WHERE filename LIKE ? OR title LIKE ? OR composer LIKE ? OR tags LIKE ?
            ORDER BY play_count DESC
            LIMIT 100
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return {};
        }
        
        std::string pattern = "%" + query + "%";
        for (int i = 1; i <= 4; i++) {
            sqlite3_bind_text(stmt, i, pattern.c_str(), -1, SQLITE_TRANSIENT);
        }
        
        std::vector<MidiFileEntry> results;
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            results.push_back(rowToEntry(stmt));
        }
        
        sqlite3_finalize(stmt);
        return results;
    }
    
    /**
     * @brief Incrémente le compteur de lecture
     */
    void incrementPlayCount(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const char* sql = R"(
            UPDATE midi_files 
            SET play_count = play_count + 1, last_played = ?
            WHERE id = ?
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return;
        }
        
        sqlite3_bind_int64(stmt, 1, std::time(nullptr));
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    /**
     * @brief Obtient des statistiques
     */
    json getStatistics() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        json stats;
        
        // Nombre total de fichiers
        const char* sqlCount = "SELECT COUNT(*) FROM midi_files";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db_, sqlCount, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                stats["total_files"] = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        
        // Durée totale
        const char* sqlDuration = "SELECT SUM(duration_ms) FROM midi_files";
        if (sqlite3_prepare_v2(db_, sqlDuration, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                stats["total_duration_ms"] = sqlite3_column_int64(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        
        // Taille totale
        const char* sqlSize = "SELECT SUM(file_size) FROM midi_files";
        if (sqlite3_prepare_v2(db_, sqlSize, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                stats["total_size_bytes"] = sqlite3_column_int64(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        
        return stats;
    }

private:
    void createTables() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS midi_files (
                id TEXT PRIMARY KEY,
                filename TEXT NOT NULL,
                filepath TEXT NOT NULL UNIQUE,
                relative_path TEXT,
                duration_ms INTEGER,
                track_count INTEGER,
                title TEXT,
                composer TEXT,
                tags TEXT,
                file_size INTEGER,
                last_modified INTEGER,
                added_date INTEGER,
                play_count INTEGER DEFAULT 0,
                last_played INTEGER
            );
            
            CREATE INDEX IF NOT EXISTS idx_filename ON midi_files(filename);
            CREATE INDEX IF NOT EXISTS idx_tags ON midi_files(tags);
            CREATE INDEX IF NOT EXISTS idx_play_count ON midi_files(play_count DESC);
        )";
        
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            Logger::error("MidiFileDatabase", "Table creation error: " + std::string(errMsg));
            sqlite3_free(errMsg);
        }
    }
    
    MidiFileEntry rowToEntry(sqlite3_stmt* stmt) {
        MidiFileEntry entry;
        
        entry.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        entry.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.filepath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        entry.relativePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        entry.durationMs = sqlite3_column_int(stmt, 4);
        entry.trackCount = sqlite3_column_int(stmt, 5);
        entry.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        entry.composer = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        
        // Désérialiser tags
        const char* tagsStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        if (tagsStr) {
            try {
                json tagsJson = json::parse(tagsStr);
                entry.tags = tagsJson.get<std::vector<std::string>>();
            } catch (...) {}
        }
        
        entry.fileSizeBytes = sqlite3_column_int64(stmt, 9);
        entry.lastModified = sqlite3_column_int64(stmt, 10);
        entry.addedDate = sqlite3_column_int64(stmt, 11);
        entry.playCount = sqlite3_column_int(stmt, 12);
        entry.lastPlayed = sqlite3_column_int64(stmt, 13);
        
        return entry;
    }
    
    sqlite3* db_;
    std::mutex mutex_;
};

// ============================================================================
// src/midi/filemanager/PlaylistManager.h
// Responsabilité: Gestion des playlists
// ============================================================================

struct Playlist {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> fileIds;
    std::time_t createdDate;
    std::time_t modifiedDate;
    
    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["description"] = description;
        j["file_ids"] = fileIds;
        j["created_date"] = createdDate;
        j["modified_date"] = modifiedDate;
        j["file_count"] = fileIds.size();
        return j;
    }
};

class PlaylistManager {
public:
    PlaylistManager(sqlite3* db) : db_(db) {
        createTable();
    }
    
    /**
     * @brief Crée une nouvelle playlist
     */
    std::string create(const std::string& name, const std::string& description = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string id = generateUUID();
        
        const char* sql = R"(
            INSERT INTO playlists (id, name, description, file_ids, created_date, modified_date)
            VALUES (?, ?, ?, '[]', ?, ?)
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return "";
        }
        
        std::time_t now = std::time(nullptr);
        
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, now);
        sqlite3_bind_int64(stmt, 5, now);
        
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        
        if (success) {
            Logger::info("PlaylistManager", "Created playlist: " + name);
            return id;
        }
        
        return "";
    }
    
    /**
     * @brief Liste toutes les playlists
     */
    std::vector<Playlist> list() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const char* sql = "SELECT * FROM playlists ORDER BY created_date DESC";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return {};
        }
        
        std::vector<Playlist> playlists;
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            playlists.push_back(rowToPlaylist(stmt));
        }
        
        sqlite3_finalize(stmt);
        return playlists;
    }
    
    /**
     * @brief Récupère une playlist par ID
     */
    std::optional<Playlist> get(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const char* sql = "SELECT * FROM playlists WHERE id = ?";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return std::nullopt;
        }
        
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        
        std::optional<Playlist> result;
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = rowToPlaylist(stmt);
        }
        
        sqlite3_finalize(stmt);
        return result;
    }
    
    /**
     * @brief Ajoute un fichier à une playlist
     */
    bool addFile(const std::string& playlistId, const std::string& fileId) {
        auto playlist = get(playlistId);
        if (!playlist) return false;
        
        // Vérifier si déjà présent
        if (std::find(playlist->fileIds.begin(), playlist->fileIds.end(), fileId) 
            != playlist->fileIds.end()) {
            return false;
        }
        
        playlist->fileIds.push_back(fileId);
        return update(*playlist);
    }
    
    /**
     * @brief Retire un fichier d'une playlist
     */
    bool removeFile(const std::string& playlistId, const std::string& fileId) {
        auto playlist = get(playlistId);
        if (!playlist) return false;
        
        auto it = std::find(playlist->fileIds.begin(), playlist->fileIds.end(), fileId);
        if (it == playlist->fileIds.end()) {
            return false;
        }
        
        playlist->fileIds.erase(it);
        return update(*playlist);
    }
    
    /**
     * @brief Supprime une playlist
     */
    bool remove(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const char* sql = "DELETE FROM playlists WHERE id = ?";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        
        if (success) {
            Logger::info("PlaylistManager", "Deleted playlist: " + id);
        }
        
        return success;
    }

private:
    void createTable() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS playlists (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                description TEXT,
                file_ids TEXT,
                created_date INTEGER,
                modified_date INTEGER
            )
        )";
        
        char* errMsg = nullptr;
        sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
        if (errMsg) sqlite3_free(errMsg);
    }
    
    bool update(const Playlist& playlist) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        const char* sql = R"(
            UPDATE playlists 
            SET file_ids = ?, modified_date = ?
            WHERE id = ?
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        
        json fileIdsJson = playlist.fileIds;
        std::string fileIdsStr = fileIdsJson.dump();
        
        sqlite3_bind_text(stmt, 1, fileIdsStr.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, std::time(nullptr));
        sqlite3_bind_text(stmt, 3, playlist.id.c_str(), -1, SQLITE_TRANSIENT);
        
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        
        return success;
    }
    
    Playlist rowToPlaylist(sqlite3_stmt* stmt) {
        Playlist p;
        
        p.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        
        const char* fileIdsStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (fileIdsStr) {
            try {
                json fileIdsJson = json::parse(fileIdsStr);
                p.fileIds = fileIdsJson.get<std::vector<std::string>>();
            } catch (...) {}
        }
        
        p.createdDate = sqlite3_column_int64(stmt, 4);
        p.modifiedDate = sqlite3_column_int64(stmt, 5);
        
        return p;
    }
    
    std::string generateUUID() {
        thread_local std::random_device rd;
        thread_local std::mt19937 gen(rd());
        thread_local std::uniform_int_distribution<> dis(0, 15);
        
        static const char* hex = "0123456789abcdef";
        std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
        
        for (char& c : uuid) {
            if (c == 'x') c = hex[dis(gen)];
            else if (c == 'y') c = hex[(dis(gen) & 0x3) | 0x8];
        }
        
        return uuid;
    }
    
    sqlite3* db_;
    std::mutex mutex_;
};

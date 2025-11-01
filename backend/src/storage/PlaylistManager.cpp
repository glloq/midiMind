// ============================================================================
// File: backend/src/storage/PlaylistManager.cpp
// Version: 4.2.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#include "PlaylistManager.h"
#include "Database.h"
#include "../core/Logger.h"
#include "../core/TimeUtils.h"
#include <stdexcept>

namespace midiMind {

// ============================================================================
// PLAYLIST ITEM
// ============================================================================

json PlaylistItem::toJson() const {
    return json{
        {"id", id},
        {"playlist_id", playlistId},
        {"midi_file_id", midiFileId},
        {"position", position},
        {"filename", filename}
    };
}

PlaylistItem PlaylistItem::fromJson(const json& j) {
    PlaylistItem item;
    item.id = j.value("id", 0);
    item.playlistId = j.value("playlist_id", 0);
    item.midiFileId = j.value("midi_file_id", 0);
    item.position = j.value("position", 0);
    item.filename = j.value("filename", "");
    return item;
}

// ============================================================================
// PLAYLIST
// ============================================================================

json Playlist::toJson() const {
    json itemsJson = json::array();
    for (const auto& item : items) {
        itemsJson.push_back(item.toJson());
    }
    
    return json{
        {"id", id},
        {"name", name},
        {"description", description},
        {"loop", loop},
        {"created_at", createdAt},
        {"updated_at", updatedAt},
        {"items", itemsJson}
    };
}

Playlist Playlist::fromJson(const json& j) {
    Playlist playlist;
    playlist.id = j.value("id", 0);
    playlist.name = j.value("name", "");
    playlist.description = j.value("description", "");
    playlist.loop = j.value("loop", false);
    playlist.createdAt = j.value("created_at", 0);
    playlist.updatedAt = j.value("updated_at", 0);
    
    if (j.contains("items") && j["items"].is_array()) {
        for (const auto& itemJson : j["items"]) {
            playlist.items.push_back(PlaylistItem::fromJson(itemJson));
        }
    }
    
    return playlist;
}

// ============================================================================
// PLAYLIST MANAGER
// ============================================================================

PlaylistManager::PlaylistManager(Database& db)
    : db_(db)
{
    Logger::info("PlaylistManager", "PlaylistManager initialized");
}

PlaylistManager::~PlaylistManager() {
    Logger::info("PlaylistManager", "PlaylistManager destroyed");
}

int PlaylistManager::createPlaylist(const std::string& name, const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int64_t now = TimeUtils::systemNow();
    
    auto stmt = db_.prepare(R"(
        INSERT INTO playlists (name, description, loop, created_at, updated_at)
        VALUES (?, ?, 0, ?, ?)
    )");
    
    stmt.bind(1, name);
    stmt.bind(2, description);
    stmt.bind(3, now);
    stmt.bind(4, now);
    stmt.execute();
    
    int id = static_cast<int>(db_.getLastInsertRowId());
    
    Logger::info("PlaylistManager", "Created playlist: " + name + " (ID: " + std::to_string(id) + ")");
    
    return id;
}

bool PlaylistManager::deletePlaylist(int playlistId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto stmt = db_.prepare("DELETE FROM playlists WHERE id = ?");
    stmt.bind(1, playlistId);
    stmt.execute();
    
    bool deleted = stmt.getChanges() > 0;
    
    if (deleted) {
        Logger::info("PlaylistManager", "Deleted playlist ID: " + std::to_string(playlistId));
    }
    
    return deleted;
}

bool PlaylistManager::updatePlaylist(int playlistId, const std::string& name, const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto stmt = db_.prepare(R"(
        UPDATE playlists 
        SET name = ?, description = ?, updated_at = ?
        WHERE id = ?
    )");
    
    stmt.bind(1, name);
    stmt.bind(2, description);
    stmt.bind(3, TimeUtils::systemNow());
    stmt.bind(4, playlistId);
    stmt.execute();
    
    return stmt.getChanges() > 0;
}

std::vector<Playlist> PlaylistManager::listPlaylists() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Playlist> playlists;
    
    auto stmt = db_.prepare(R"(
        SELECT id, name, description, loop, created_at, updated_at 
        FROM playlists 
        ORDER BY updated_at DESC
    )");
    
    while (stmt.step()) {
        Playlist playlist;
        playlist.id = stmt.getInt(0);
        playlist.name = stmt.getText(1);
        playlist.description = stmt.getText(2);
        playlist.loop = stmt.getInt(3) != 0;
        playlist.createdAt = stmt.getInt64(4);
        playlist.updatedAt = stmt.getInt64(5);
        
        // Load items for this playlist
        auto itemStmt = db_.prepare(R"(
            SELECT pi.id, pi.playlist_id, pi.midi_file_id, pi.position, mf.filename
            FROM playlist_items pi
            JOIN midi_files mf ON pi.midi_file_id = mf.id
            WHERE pi.playlist_id = ?
            ORDER BY pi.position
        )");
        
        itemStmt.bind(1, playlist.id);
        
        while (itemStmt.step()) {
            PlaylistItem item;
            item.id = itemStmt.getInt(0);
            item.playlistId = itemStmt.getInt(1);
            item.midiFileId = itemStmt.getInt(2);
            item.position = itemStmt.getInt(3);
            item.filename = itemStmt.getText(4);
            
            playlist.items.push_back(item);
        }
        
        playlists.push_back(playlist);
    }
    
    return playlists;
}

Playlist PlaylistManager::getPlaylist(int playlistId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto stmt = db_.prepare(R"(
        SELECT id, name, description, loop, created_at, updated_at 
        FROM playlists 
        WHERE id = ?
    )");
    
    stmt.bind(1, playlistId);
    
    if (!stmt.step()) {
        throw std::runtime_error("Playlist not found: " + std::to_string(playlistId));
    }
    
    Playlist playlist;
    playlist.id = stmt.getInt(0);
    playlist.name = stmt.getText(1);
    playlist.description = stmt.getText(2);
    playlist.loop = stmt.getInt(3) != 0;
    playlist.createdAt = stmt.getInt64(4);
    playlist.updatedAt = stmt.getInt64(5);
    
    // Load items
    auto itemStmt = db_.prepare(R"(
        SELECT pi.id, pi.playlist_id, pi.midi_file_id, pi.position, mf.filename
        FROM playlist_items pi
        JOIN midi_files mf ON pi.midi_file_id = mf.id
        WHERE pi.playlist_id = ?
        ORDER BY pi.position
    )");
    
    itemStmt.bind(1, playlistId);
    
    while (itemStmt.step()) {
        PlaylistItem item;
        item.id = itemStmt.getInt(0);
        item.playlistId = itemStmt.getInt(1);
        item.midiFileId = itemStmt.getInt(2);
        item.position = itemStmt.getInt(3);
        item.filename = itemStmt.getText(4);
        
        playlist.items.push_back(item);
    }
    
    return playlist;
}

bool PlaylistManager::addItem(int playlistId, int midiFileId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get next position
    auto posStmt = db_.prepare(R"(
        SELECT COALESCE(MAX(position), -1) + 1 
        FROM playlist_items 
        WHERE playlist_id = ?
    )");
    
    posStmt.bind(1, playlistId);
    posStmt.step();
    int position = posStmt.getInt(0);
    
    // Insert item
    auto stmt = db_.prepare(R"(
        INSERT INTO playlist_items (playlist_id, midi_file_id, position)
        VALUES (?, ?, ?)
    )");
    
    stmt.bind(1, playlistId);
    stmt.bind(2, midiFileId);
    stmt.bind(3, position);
    stmt.execute();
    
    updatePlaylistTimestamp(playlistId);
    
    Logger::info("PlaylistManager", 
        "Added item to playlist " + std::to_string(playlistId) + 
        " at position " + std::to_string(position));
    
    return true;
}

bool PlaylistManager::removeItem(int playlistId, int itemId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get position of item to remove
    auto posStmt = db_.prepare("SELECT position FROM playlist_items WHERE id = ?");
    posStmt.bind(1, itemId);
    
    if (!posStmt.step()) {
        return false;
    }
    
    int removedPosition = posStmt.getInt(0);
    
    // Delete item
    auto delStmt = db_.prepare("DELETE FROM playlist_items WHERE id = ?");
    delStmt.bind(1, itemId);
    delStmt.execute();
    
    if (delStmt.getChanges() == 0) {
        return false;
    }
    
    // Reorder remaining items
    auto updateStmt = db_.prepare(R"(
        UPDATE playlist_items 
        SET position = position - 1 
        WHERE playlist_id = ? AND position > ?
    )");
    
    updateStmt.bind(1, playlistId);
    updateStmt.bind(2, removedPosition);
    updateStmt.execute();
    
    updatePlaylistTimestamp(playlistId);
    
    Logger::info("PlaylistManager", "Removed item " + std::to_string(itemId) + " from playlist");
    
    return true;
}

bool PlaylistManager::reorderItems(int playlistId, const std::vector<int>& itemIds) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        db_.execute("BEGIN TRANSACTION");
        
        auto stmt = db_.prepare(R"(
            UPDATE playlist_items 
            SET position = ? 
            WHERE id = ? AND playlist_id = ?
        )");
        
        for (size_t i = 0; i < itemIds.size(); ++i) {
            stmt.reset();
            stmt.bind(1, static_cast<int>(i));
            stmt.bind(2, itemIds[i]);
            stmt.bind(3, playlistId);
            stmt.execute();
        }
        
        db_.execute("COMMIT");
        
        updatePlaylistTimestamp(playlistId);
        
        Logger::info("PlaylistManager", 
            "Reordered " + std::to_string(itemIds.size()) + " items in playlist " + 
            std::to_string(playlistId));
        
        return true;
        
    } catch (const std::exception& e) {
        db_.execute("ROLLBACK");
        Logger::error("PlaylistManager", "Failed to reorder items: " + std::string(e.what()));
        return false;
    }
}

bool PlaylistManager::setLoop(int playlistId, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto stmt = db_.prepare("UPDATE playlists SET loop = ? WHERE id = ?");
    stmt.bind(1, enabled ? 1 : 0);
    stmt.bind(2, playlistId);
    stmt.execute();
    
    return stmt.getChanges() > 0;
}

void PlaylistManager::updatePlaylistTimestamp(int playlistId) {
    auto stmt = db_.prepare("UPDATE playlists SET updated_at = ? WHERE id = ?");
    stmt.bind(1, TimeUtils::systemNow());
    stmt.bind(2, playlistId);
    stmt.execute();
}

} // namespace midiMind
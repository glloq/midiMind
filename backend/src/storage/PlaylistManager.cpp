// ============================================================================
// File: backend/src/storage/PlaylistManager.cpp
// Version: 4.2.4 - FIX includes and types
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#include "PlaylistManager.h"
#include "Database.h"
#include "../core/Logger.h"
#include "../core/TimeUtils.h"
#include <algorithm>

namespace midiMind {

PlaylistManager::PlaylistManager(Database& db)
    : db_(db)
{
    Logger::debug("PlaylistManager", "PlaylistManager created");
}

PlaylistManager::~PlaylistManager() {
    Logger::debug("PlaylistManager", "PlaylistManager destroyed");
}

int PlaylistManager::createPlaylist(const std::string& name, const std::string& description) {
    try {
        auto now = TimeUtils::systemNow();
        
        auto result = db_.execute(
            R"(
                INSERT INTO playlists (name, description, loop, created_at, updated_at)
                VALUES (?, ?, 0, ?, ?)
            )",
            {name, description, std::to_string(now), std::to_string(now)}
        );
        
        if (!result.success) {
            Logger::error("PlaylistManager", "Failed to create playlist: " + result.error);
            return -1;
        }
        
        int id = static_cast<int>(result.lastInsertId);
        Logger::info("PlaylistManager", "Created playlist: " + name + " (id=" + std::to_string(id) + ")");
        return id;
        
    } catch (const std::exception& e) {
        Logger::error("PlaylistManager", "Exception creating playlist: " + std::string(e.what()));
        return -1;
    }
}

bool PlaylistManager::deletePlaylist(int playlistId) {
    try {
        auto result = db_.execute("DELETE FROM playlists WHERE id = ?", {std::to_string(playlistId)});
        
        if (!result.success) {
            Logger::error("PlaylistManager", "Failed to delete playlist: " + result.error);
            return false;
        }
        
        Logger::info("PlaylistManager", "Deleted playlist id=" + std::to_string(playlistId));
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PlaylistManager", "Exception deleting playlist: " + std::string(e.what()));
        return false;
    }
}

bool PlaylistManager::updatePlaylist(int playlistId, const std::string& name, const std::string& description) {
    try {
        auto now = TimeUtils::systemNow();
        
        auto result = db_.execute(
            R"(
                UPDATE playlists
                SET name = ?, description = ?, updated_at = ?
                WHERE id = ?
            )",
            {name, description, std::to_string(now), std::to_string(playlistId)}
        );
        
        if (!result.success) {
            Logger::error("PlaylistManager", "Failed to update playlist: " + result.error);
            return false;
        }
        
        Logger::info("PlaylistManager", "Updated playlist id=" + std::to_string(playlistId));
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PlaylistManager", "Exception updating playlist: " + std::string(e.what()));
        return false;
    }
}

std::vector<Playlist> PlaylistManager::listPlaylists() const {
    std::vector<Playlist> playlists;
    
    try {
        auto result = db_.query(
            R"(
                SELECT id, name, description, loop, created_at, updated_at
                FROM playlists
                ORDER BY created_at DESC
            )"
        );
        
        if (!result.success) {
            Logger::error("PlaylistManager", "Failed to list playlists: " + result.error);
            return playlists;
        }
        
        for (const auto& row : result.rows) {
            Playlist playlist;
            playlist.id = std::stoi(row.at("id"));
            playlist.name = row.at("name");
            playlist.description = row.at("description");
            playlist.loop = (row.at("loop") == "1");
            playlist.createdAt = std::stoll(row.at("created_at"));
            playlist.updatedAt = std::stoll(row.at("updated_at"));
            
            auto itemsResult = db_.query(
                R"(
                    SELECT pi.id, pi.playlist_id, pi.midi_id as midi_file_id, pi.position, m.name as filename
                    FROM playlist_items pi
                    LEFT JOIN midi_files m ON pi.midi_id = m.id
                    WHERE pi.playlist_id = ?
                    ORDER BY pi.position
                )",
                {std::to_string(playlist.id)}
            );
            
            if (itemsResult.success) {
                for (const auto& itemRow : itemsResult.rows) {
                    PlaylistItem item;
                    item.id = std::stoi(itemRow.at("id"));
                    item.playlistId = std::stoi(itemRow.at("playlist_id"));
                    item.midiFileId = std::stoi(itemRow.at("midi_file_id"));
                    item.position = std::stoi(itemRow.at("position"));
                    item.filename = itemRow.count("filename") ? itemRow.at("filename") : "";
                    playlist.items.push_back(item);
                }
            }
            
            playlists.push_back(playlist);
        }
        
        Logger::debug("PlaylistManager", "Listed " + std::to_string(playlists.size()) + " playlists");
        
    } catch (const std::exception& e) {
        Logger::error("PlaylistManager", "Exception listing playlists: " + std::string(e.what()));
    }
    
    return playlists;
}

Playlist PlaylistManager::getPlaylist(int playlistId) const {
    Playlist playlist;
    playlist.id = -1;
    
    try {
        auto result = db_.query(
            R"(
                SELECT id, name, description, loop, created_at, updated_at
                FROM playlists
                WHERE id = ?
            )",
            {std::to_string(playlistId)}
        );
        
        if (!result.success || result.rows.empty()) {
            Logger::warning("PlaylistManager", "Playlist not found: id=" + std::to_string(playlistId));
            return playlist;
        }
        
        const auto& row = result.rows[0];
        playlist.id = std::stoi(row.at("id"));
        playlist.name = row.at("name");
        playlist.description = row.at("description");
        playlist.loop = (row.at("loop") == "1");
        playlist.createdAt = std::stoll(row.at("created_at"));
        playlist.updatedAt = std::stoll(row.at("updated_at"));
        
        auto itemsResult = db_.query(
            R"(
                SELECT pi.id, pi.playlist_id, pi.midi_id as midi_file_id, pi.position, m.name as filename
                FROM playlist_items pi
                LEFT JOIN midi_files m ON pi.midi_id = m.id
                WHERE pi.playlist_id = ?
                ORDER BY pi.position
            )",
            {std::to_string(playlistId)}
        );
        
        if (itemsResult.success) {
            for (const auto& itemRow : itemsResult.rows) {
                PlaylistItem item;
                item.id = std::stoi(itemRow.at("id"));
                item.playlistId = std::stoi(itemRow.at("playlist_id"));
                item.midiFileId = std::stoi(itemRow.at("midi_file_id"));
                item.position = std::stoi(itemRow.at("position"));
                item.filename = itemRow.count("filename") ? itemRow.at("filename") : "";
                playlist.items.push_back(item);
            }
        }
        
        Logger::debug("PlaylistManager", "Got playlist: " + playlist.name + " with " + 
                     std::to_string(playlist.items.size()) + " items");
        
    } catch (const std::exception& e) {
        Logger::error("PlaylistManager", "Exception getting playlist: " + std::string(e.what()));
        playlist.id = -1;
    }
    
    return playlist;
}

bool PlaylistManager::addItem(int playlistId, int midiId) {
    try {
        auto posResult = db_.query(
            R"(
                SELECT COALESCE(MAX(position), -1) + 1 as next_pos
                FROM playlist_items
                WHERE playlist_id = ?
            )",
            {std::to_string(playlistId)}
        );
        
        if (!posResult.success || posResult.rows.empty()) {
            Logger::error("PlaylistManager", "Failed to get next position");
            return false;
        }
        
        int position = std::stoi(posResult.rows[0].at("next_pos"));
        
        auto result = db_.execute(
            R"(
                INSERT INTO playlist_items (playlist_id, midi_id, position)
                VALUES (?, ?, ?)
            )",
            {std::to_string(playlistId), std::to_string(midiId), std::to_string(position)}
        );
        
        if (!result.success) {
            Logger::error("PlaylistManager", "Failed to add item: " + result.error);
            return false;
        }
        
        updatePlaylistTimestamp(playlistId);
        
        Logger::info("PlaylistManager", "Added item to playlist id=" + std::to_string(playlistId));
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PlaylistManager", "Exception adding item: " + std::string(e.what()));
        return false;
    }
}

bool PlaylistManager::removeItem(int playlistId, int itemId) {
    try {
        auto posResult = db_.query(
            "SELECT position FROM playlist_items WHERE id = ?",
            {std::to_string(itemId)}
        );
        
        if (!posResult.success || posResult.rows.empty()) {
            Logger::error("PlaylistManager", "Item not found");
            return false;
        }
        
        int deletedPosition = std::stoi(posResult.rows[0].at("position"));
        
        auto delResult = db_.execute(
            "DELETE FROM playlist_items WHERE id = ?",
            {std::to_string(itemId)}
        );
        
        if (!delResult.success) {
            Logger::error("PlaylistManager", "Failed to delete item: " + delResult.error);
            return false;
        }
        
        auto updateResult = db_.execute(
            R"(
                UPDATE playlist_items
                SET position = position - 1
                WHERE playlist_id = ? AND position > ?
            )",
            {std::to_string(playlistId), std::to_string(deletedPosition)}
        );
        
        if (!updateResult.success) {
            Logger::warning("PlaylistManager", "Failed to reorder after delete: " + updateResult.error);
        }
        
        updatePlaylistTimestamp(playlistId);
        
        Logger::info("PlaylistManager", "Removed item from playlist id=" + std::to_string(playlistId));
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PlaylistManager", "Exception removing item: " + std::string(e.what()));
        return false;
    }
}

bool PlaylistManager::reorderItems(int playlistId, const std::vector<int>& itemIds) {
    try {
        for (size_t i = 0; i < itemIds.size(); ++i) {
            auto result = db_.execute(
                R"(
                    UPDATE playlist_items
                    SET position = ?
                    WHERE id = ? AND playlist_id = ?
                )",
                {std::to_string(i), std::to_string(itemIds[i]), std::to_string(playlistId)}
            );
            
            if (!result.success) {
                Logger::error("PlaylistManager", "Failed to reorder item: " + result.error);
                return false;
            }
        }
        
        updatePlaylistTimestamp(playlistId);
        
        Logger::info("PlaylistManager", "Reordered items in playlist id=" + std::to_string(playlistId));
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PlaylistManager", "Exception reordering items: " + std::string(e.what()));
        return false;
    }
}

bool PlaylistManager::setLoop(int playlistId, bool loop) {
    try {
        auto result = db_.execute(
            "UPDATE playlists SET loop = ? WHERE id = ?",
            {loop ? "1" : "0", std::to_string(playlistId)}
        );
        
        if (!result.success) {
            Logger::error("PlaylistManager", "Failed to set loop: " + result.error);
            return false;
        }
        
        Logger::info("PlaylistManager", "Set loop=" + std::string(loop ? "true" : "false") + 
                    " for playlist id=" + std::to_string(playlistId));
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PlaylistManager", "Exception setting loop: " + std::string(e.what()));
        return false;
    }
}

void PlaylistManager::updatePlaylistTimestamp(int playlistId) {
    try {
        auto now = TimeUtils::systemNow();
        auto result = db_.execute(
            "UPDATE playlists SET updated_at = ? WHERE id = ?",
            {std::to_string(now), std::to_string(playlistId)}
        );
        
        if (!result.success) {
            Logger::warning("PlaylistManager", "Failed to update timestamp: " + result.error);
        }
        
    } catch (const std::exception& e) {
        Logger::warning("PlaylistManager", "Exception updating timestamp: " + std::string(e.what()));
    }
}

} // namespace midiMind

// ============================================================================
// END OF FILE PlaylistManager.cpp
// ============================================================================
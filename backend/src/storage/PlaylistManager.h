// ============================================================================
// File: backend/src/storage/PlaylistManager.h
// Version: 4.2.4 - FIX ABI linking issue
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// Forward declaration
class Database;

struct PlaylistItem {
    int id = 0;
    int playlistId = 0;
    int midiFileId = 0;
    int position = 0;
    std::string filename;
    
    inline json toJson() const {
        return json{
            {"id", id},
            {"playlist_id", playlistId},
            {"midi_file_id", midiFileId},
            {"position", position},
            {"filename", filename}
        };
    }
    
    static PlaylistItem fromJson(const json& j) {
        PlaylistItem item;
        item.id = j.value("id", 0);
        item.playlistId = j.value("playlist_id", 0);
        item.midiFileId = j.value("midi_file_id", 0);
        item.position = j.value("position", 0);
        item.filename = j.value("filename", "");
        return item;
    }
};

struct Playlist {
    int id = 0;
    std::string name;
    std::string description;
    bool loop = false;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    std::vector<PlaylistItem> items;
    
    inline json toJson() const {
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
    
    static Playlist fromJson(const json& j) {
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
};

class PlaylistManager {
public:
    explicit PlaylistManager(Database& db);
    ~PlaylistManager();
    
    PlaylistManager(const PlaylistManager&) = delete;
    PlaylistManager& operator=(const PlaylistManager&) = delete;
    
    // Playlist operations
    int createPlaylist(const std::string& name, const std::string& description = "");
    bool deletePlaylist(int playlistId);
    bool updatePlaylist(int playlistId, const std::string& name, const std::string& description);
    std::vector<Playlist> listPlaylists() const;
    Playlist getPlaylist(int playlistId) const;
    
    // Item operations
    bool addItem(int playlistId, int midiFileId);
    bool removeItem(int playlistId, int itemId);
    bool reorderItems(int playlistId, const std::vector<int>& itemIds);
    
    // Playback control
    bool setLoop(int playlistId, bool enabled);
    
private:
    void updatePlaylistTimestamp(int playlistId);
    
    Database& db_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE PlaylistManager.h
// ============================================================================
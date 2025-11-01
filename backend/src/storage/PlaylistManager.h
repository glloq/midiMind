// ============================================================================
// File: backend/src/storage/PlaylistManager.h
// Version: 4.2.1
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
    
    json toJson() const;
    static PlaylistItem fromJson(const json& j);
};

struct Playlist {
    int id = 0;
    std::string name;
    std::string description;
    bool loop = false;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    std::vector<PlaylistItem> items;
    
    json toJson() const;
    static Playlist fromJson(const json& j);
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
    void ensureTables();
    void updatePlaylistTimestamp(int playlistId);
    
    Database& db_;
    mutable std::mutex mutex_;
};

} // namespace midiMind
// ============================================================================
// src/api/commands/playlist/PlaylistCommands.h
// Toutes les commandes de gestion des playlists
// ============================================================================
#pragma once

#include "../../../core/commands/BaseCommand.h"
#include "../../../midi/MidiFileManager.h"

namespace midiMind {

// ============================================================================
// PLAYLIST CREATE COMMAND
// ============================================================================

class PlaylistCreateCommand : public BaseCommand {
public:
    PlaylistCreateCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "playlist.create"; }
    
    std::string getDescription() const override {
        return "Create a new playlist";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "name"}, {"type", "string"}, {"required", true},
             {"description", "Playlist name (max 100 characters)"}},
            {{"name", "description"}, {"type", "string"}, {"required", false},
             {"description", "Playlist description (max 500 characters)"}}
        });
    }
    
    bool validate(std::string& error) const override {
        if (!validateString("name", 100, error)) {
            return false;
        }
        
        // Description optionnelle
        if (params_.contains("description")) {
            if (!validateString("description", 500, error)) {
                return false;
            }
        }
        
        return true;
    }
    
    json execute() override {
        std::string name = params_["name"];
        std::string description = getOptional<std::string>("description", "");
        
        std::string playlistId = fileManager_->createPlaylist(name, description);
        
        if (playlistId.empty()) {
            return jsonError("Failed to create playlist");
        }
        
        json response = jsonSuccess("Playlist created");
        response["playlist_id"] = playlistId;
        response["name"] = name;
        response["description"] = description;
        
        return response;
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

// ============================================================================
// PLAYLIST LIST COMMAND
// ============================================================================

class PlaylistListCommand : public BaseCommand {
public:
    PlaylistListCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "playlist.list"; }
    
    std::string getDescription() const override {
        return "List all playlists";
    }
    
    json getParameterSpec() const override {
        return json::array(); // Pas de paramètres requis
    }
    
    bool validate(std::string& error) const override {
        return true; // Toujours valide
    }
    
    json execute() override {
        auto playlists = fileManager_->listPlaylists();
        
        json response = jsonSuccess();
        response["playlists"] = json::array();
        
        for (const auto& playlist : playlists) {
            response["playlists"].push_back(playlist.toJson());
        }
        
        response["count"] = playlists.size();
        
        return response;
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

// ============================================================================
// PLAYLIST GET COMMAND
// ============================================================================

class PlaylistGetCommand : public BaseCommand {
public:
    PlaylistGetCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "playlist.get"; }
    
    std::string getDescription() const override {
        return "Get a playlist by ID with all file details";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "playlist_id"}, {"type", "string"}, {"required", true},
             {"description", "Playlist ID"}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateString("playlist_id", 100, error);
    }
    
    json execute() override {
        std::string playlistId = params_["playlist_id"];
        
        auto playlist = fileManager_->getPlaylist(playlistId);
        
        if (!playlist) {
            return jsonError("Playlist not found");
        }
        
        // Récupérer les détails de chaque fichier
        json response = jsonSuccess();
        response["playlist"] = playlist->toJson();
        response["files"] = json::array();
        
        for (const auto& fileId : playlist->fileIds) {
            auto file = fileManager_->getFile(fileId);
            if (file) {
                response["files"].push_back(file->toJson());
            }
        }
        
        return response;
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

// ============================================================================
// PLAYLIST ADD COMMAND
// ============================================================================

class PlaylistAddCommand : public BaseCommand {
public:
    PlaylistAddCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "playlist.add"; }
    
    std::string getDescription() const override {
        return "Add a file to a playlist";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "playlist_id"}, {"type", "string"}, {"required", true},
             {"description", "Playlist ID"}},
            {{"name", "file_id"}, {"type", "string"}, {"required", true},
             {"description", "MIDI file ID to add"}}
        });
    }
    
    bool validate(std::string& error) const override {
        if (!validateString("playlist_id", 100, error)) {
            return false;
        }
        
        if (!validateString("file_id", 100, error)) {
            return false;
        }
        
        // Vérifier que le fichier existe
        std::string fileId = params_["file_id"];
        auto file = fileManager_->getFile(fileId);
        
        if (!file) {
            error = "File not found: " + fileId;
            return false;
        }
        
        return true;
    }
    
    json execute() override {
        std::string playlistId = params_["playlist_id"];
        std::string fileId = params_["file_id"];
        
        if (fileManager_->addToPlaylist(playlistId, fileId)) {
            json response = jsonSuccess("File added to playlist");
            response["playlist_id"] = playlistId;
            response["file_id"] = fileId;
            return response;
        } else {
            return jsonError("Failed to add file (may already exist in playlist)");
        }
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

// ============================================================================
// PLAYLIST REMOVE COMMAND
// ============================================================================

class PlaylistRemoveCommand : public BaseCommand {
public:
    PlaylistRemoveCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "playlist.remove"; }
    
    std::string getDescription() const override {
        return "Remove a file from a playlist";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "playlist_id"}, {"type", "string"}, {"required", true},
             {"description", "Playlist ID"}},
            {{"name", "file_id"}, {"type", "string"}, {"required", true},
             {"description", "MIDI file ID to remove"}}
        });
    }
    
    bool validate(std::string& error) const override {
        if (!validateString("playlist_id", 100, error)) {
            return false;
        }
        
        if (!validateString("file_id", 100, error)) {
            return false;
        }
        
        return true;
    }
    
    json execute() override {
        std::string playlistId = params_["playlist_id"];
        std::string fileId = params_["file_id"];
        
        if (fileManager_->removeFromPlaylist(playlistId, fileId)) {
            json response = jsonSuccess("File removed from playlist");
            response["playlist_id"] = playlistId;
            response["file_id"] = fileId;
            return response;
        } else {
            return jsonError("Failed to remove file (file may not be in playlist)");
        }
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

// ============================================================================
// PLAYLIST DELETE COMMAND
// ============================================================================

class PlaylistDeleteCommand : public BaseCommand {
public:
    PlaylistDeleteCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "playlist.delete"; }
    
    std::string getDescription() const override {
        return "Delete a playlist (files are not deleted)";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "playlist_id"}, {"type", "string"}, {"required", true},
             {"description", "Playlist ID to delete"}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateString("playlist_id", 100, error);
    }
    
    json execute() override {
        std::string playlistId = params_["playlist_id"];
        
        if (fileManager_->deletePlaylist(playlistId)) {
            json response = jsonSuccess("Playlist deleted");
            response["playlist_id"] = playlistId;
            return response;
        } else {
            return jsonError("Failed to delete playlist (may not exist)");
        }
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

} // namespace midiMind

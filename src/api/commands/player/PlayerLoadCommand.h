// ============================================================================
// FICHIERS 14-21: src/api/commands/player/*.h
// Commandes de contrôle du lecteur MIDI
// ============================================================================
#pragma once

#include "../../../core/commands/BaseCommand.h"
#include "../../../midi/MidiPlayer.h"

namespace midiMind {

// ============================================================================
// FICHIER 14/62: PlayerLoadCommand.h
// ============================================================================

class PlayerLoadCommand : public BaseCommand {
public:
    PlayerLoadCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "player.load"; }
    std::string getDescription() const override {
        return "Load a MIDI file into the player";
    }
    
    json getParameterSpec() const override {
        return json::array({{
            {"name", "file"}, {"type", "string"}, {"required", true},
            {"description", "Path to MIDI file (relative to midi_files_directory)"}
        }});
    }
    
    bool validate(std::string& error) const override {
        return validateFilePath("file", error);
    }
    
    json execute() override {
        std::string filepath = params_["file"];
        
        if (player_->loadFile(filepath)) {
            json response = jsonSuccess("File loaded successfully");
            response["file"] = filepath;
            response["duration_ms"] = player_->getDuration();
            response["track_count"] = player_->getTrackCount();
            return response;
        } else {
            return jsonError("Failed to load file");
        }
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};
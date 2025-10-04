// ============================================================================
// FICHIERS 22-27: Track, File et MIDI Commands
// ============================================================================
#pragma once

#include "../../../core/commands/BaseCommand.h"
#include "../../../midi/MidiPlayer.h"
#include "../../../midi/MidiFileAnalyzer.h"
#include "../../../midi/devices/MidiDeviceManager.h"

namespace midiMind {

// ============================================================================
// FICHIER 22/62: src/api/commands/track/TrackMuteCommand.h
// ============================================================================

class TrackMuteCommand : public BaseCommand {
public:
    TrackMuteCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "track.mute"; }
    std::string getDescription() const override {
        return "Mute/unmute a specific track";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "track"}, {"type", "integer"}, {"required", true}, 
             {"description", "Track index (0-127)"}},
            {{"name", "mute"}, {"type", "boolean"}, {"required", true}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateRange("track", 0, 127, error) &&
               validateBoolean("mute", error);
    }
    
    json execute() override {
        int track = params_["track"];
        bool mute = params_["mute"];
        
        player_->setTrackMute(track, mute);
        
        return jsonSuccess("Track " + std::to_string(track) + " " +
                          (mute ? "muted" : "unmuted"));
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};

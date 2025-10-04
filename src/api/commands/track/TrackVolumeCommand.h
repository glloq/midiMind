// ============================================================================
// FICHIER 24/62: src/api/commands/track/TrackVolumeCommand.h
// ============================================================================

class TrackVolumeCommand : public BaseCommand {
public:
    TrackVolumeCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "track.volume"; }
    std::string getDescription() const override {
        return "Set volume for a specific track";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "track"}, {"type", "integer"}, {"required", true}},
            {{"name", "volume"}, {"type", "number"}, {"required", true},
             {"description", "Volume level (0.0 to 1.0)"}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateRange("track", 0, 127, error) &&
               validateVolume("volume", error);
    }
    
    json execute() override {
        int track = params_["track"];
        float volume = params_["volume"];
        
        player_->setTrackVolume(track, volume);
        
        return jsonSuccess("Track volume set to " + std::to_string(volume));
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};
// ============================================================================
// FICHIER 23/62: src/api/commands/track/TrackSoloCommand.h
// ============================================================================

class TrackSoloCommand : public BaseCommand {
public:
    TrackSoloCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "track.solo"; }
    std::string getDescription() const override {
        return "Solo a specific track";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "track"}, {"type", "integer"}, {"required", true}},
            {{"name", "solo"}, {"type", "boolean"}, {"required", true}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateRange("track", 0, 127, error) &&
               validateBoolean("solo", error);
    }
    
    json execute() override {
        int track = params_["track"];
        bool solo = params_["solo"];
        
        player_->setTrackSolo(track, solo);
        
        return jsonSuccess("Track solo " + std::string(solo ? "enabled" : "disabled"));
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};
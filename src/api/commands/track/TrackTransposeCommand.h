// ============================================================================
// FICHIER 25/62: src/api/commands/track/TrackTransposeCommand.h
// ============================================================================

class TrackTransposeCommand : public BaseCommand {
public:
    TrackTransposeCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "track.transpose"; }
    std::string getDescription() const override {
        return "Set transpose for a specific track";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "track"}, {"type", "integer"}, {"required", true}},
            {{"name", "semitones"}, {"type", "integer"}, {"required", true},
             {"description", "Transpose amount (-12 to +12)"}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateRange("track", 0, 127, error) &&
               validateRange("semitones", -12, 12, error);
    }
    
    json execute() override {
        int track = params_["track"];
        int semitones = params_["semitones"];
        
        player_->setTrackTranspose(track, semitones);
        
        return jsonSuccess("Track transpose set to " + std::to_string(semitones));
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};
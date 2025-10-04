// ============================================================================
// FICHIER 20/62: PlayerTransposeCommand.h
// ============================================================================

class PlayerTransposeCommand : public BaseCommand {
public:
    PlayerTransposeCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "player.transpose"; }
    std::string getDescription() const override {
        return "Set global transpose in semitones";
    }
    
    json getParameterSpec() const override {
        return json::array({{
            {"name", "semitones"}, {"type", "integer"}, {"required", true},
            {"description", "Transpose amount in semitones (-12 to +12)"}
        }});
    }
    
    bool validate(std::string& error) const override {
        return validateRange("semitones", -12, 12, error);
    }
    
    json execute() override {
        int semitones = params_["semitones"];
        
        player_->setGlobalTranspose(semitones);
        
        json response = jsonSuccess("Transpose set");
        response["semitones"] = semitones;
        return response;
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};

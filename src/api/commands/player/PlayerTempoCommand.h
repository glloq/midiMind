// ============================================================================
// FICHIER 19/62: PlayerTempoCommand.h
// ============================================================================

class PlayerTempoCommand : public BaseCommand {
public:
    PlayerTempoCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "player.tempo"; }
    std::string getDescription() const override {
        return "Set playback tempo multiplier";
    }
    
    json getParameterSpec() const override {
        return json::array({{
            {"name", "multiplier"}, {"type", "number"}, {"required", true},
            {"description", "Tempo multiplier (0.1 to 4.0, 1.0 = normal speed)"}
        }});
    }
    
    bool validate(std::string& error) const override {
        return validateRange<float>("multiplier", 0.1f, 4.0f, error);
    }
    
    json execute() override {
        float multiplier = params_["multiplier"];
        
        player_->setTempo(multiplier);
        
        json response = jsonSuccess("Tempo set");
        response["tempo"] = multiplier;
        return response;
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};
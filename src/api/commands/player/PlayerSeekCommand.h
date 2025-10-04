// ============================================================================
// FICHIER 18/62: PlayerSeekCommand.h
// ============================================================================

class PlayerSeekCommand : public BaseCommand {
public:
    PlayerSeekCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "player.seek"; }
    std::string getDescription() const override {
        return "Seek to a specific position in the file";
    }
    
    json getParameterSpec() const override {
        return json::array({{
            {"name", "position_ms"}, {"type", "integer"}, {"required", true},
            {"description", "Target position in milliseconds"}
        }});
    }
    
    bool validate(std::string& error) const override {
        if (!validateRequired("position_ms", error)) return false;
        
        try {
            uint32_t pos = params_["position_ms"].get<uint32_t>();
            
            // Vérifier que la position est dans les limites
            uint32_t duration = player_->getDuration();
            if (duration > 0 && pos > duration) {
                error = "Position " + std::to_string(pos) + "ms exceeds duration " +
                       std::to_string(duration) + "ms";
                return false;
            }
            
            return true;
            
        } catch (const json::exception& e) {
            error = "Field 'position_ms' must be a positive integer";
            return false;
        }
    }
    
    json execute() override {
        uint32_t posMs = params_["position_ms"];
        
        player_->seek(posMs);
        
        json response = jsonSuccess("Seeked to position");
        response["position_ms"] = posMs;
        return response;
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};
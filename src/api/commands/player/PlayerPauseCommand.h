// ============================================================================
// FICHIER 16/62: PlayerPauseCommand.h
// ============================================================================

class PlayerPauseCommand : public BaseCommand {
public:
    PlayerPauseCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "player.pause"; }
    std::string getDescription() const override {
        return "Pause playback (position maintained)";
    }
    
    bool validate(std::string& error) const override {
        return true;
    }
    
    json execute() override {
        player_->pause();
        
        json response = jsonSuccess("Playback paused");
        response["position_ms"] = player_->getPosition();
        return response;
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};
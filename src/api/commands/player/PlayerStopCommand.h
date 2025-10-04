// ============================================================================
// FICHIER 17/62: PlayerStopCommand.h
// ============================================================================

class PlayerStopCommand : public BaseCommand {
public:
    PlayerStopCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "player.stop"; }
    std::string getDescription() const override {
        return "Stop playback and reset position to start";
    }
    
    bool validate(std::string& error) const override {
        return true;
    }
    
    json execute() override {
        player_->stop();
        
        return jsonSuccess("Playback stopped");
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};

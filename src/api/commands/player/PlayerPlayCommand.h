// ============================================================================
// FICHIER 15/62: PlayerPlayCommand.h
// ============================================================================

class PlayerPlayCommand : public BaseCommand {
public:
    PlayerPlayCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "player.play"; }
    std::string getDescription() const override {
        return "Start or resume playback";
    }
    
    bool validate(std::string& error) const override {
        return true; // Pas de paramètres
    }
    
    json execute() override {
        if (player_->getCurrentFile().empty()) {
            return jsonError("No file loaded");
        }
        
        player_->play();
        
        json response = jsonSuccess("Playback started");
        response["position_ms"] = player_->getPosition();
        response["file"] = player_->getCurrentFile();
        return response;
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};
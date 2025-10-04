// ============================================================================
// FICHIER 21/62: PlayerStatusCommand.h
// ============================================================================

class PlayerStatusCommand : public BaseCommand {
public:
    PlayerStatusCommand(const json& params, std::shared_ptr<MidiPlayer> player)
        : BaseCommand(params), player_(player) {}
    
    std::string getName() const override { return "player.status"; }
    std::string getDescription() const override {
        return "Get current player status and information";
    }
    
    bool validate(std::string& error) const override {
        return true; // Pas de paramètres
    }
    
    json execute() override {
        json response = jsonSuccess();
        
        // État du player
        PlayerState state = player_->getState();
        response["state"] = (state == PlayerState::PLAYING) ? "playing" :
                           (state == PlayerState::PAUSED) ? "paused" : "stopped";
        
        // Informations de timing
        response["position_ms"] = player_->getPosition();
        response["duration_ms"] = player_->getDuration();
        
        // Calcul du pourcentage
        uint32_t duration = player_->getDuration();
        uint32_t position = player_->getPosition();
        if (duration > 0) {
            response["progress_percent"] = 
                static_cast<float>(position) / duration * 100.0f;
        } else {
            response["progress_percent"] = 0.0f;
        }
        
        // Paramètres globaux
        response["tempo"] = player_->getTempo();
        response["transpose"] = player_->getGlobalTranspose();
        
        // Fichier actuel
        response["file"] = player_->getCurrentFile();
        response["has_file_loaded"] = !player_->getCurrentFile().empty();
        
        // Nombre de tracks
        response["track_count"] = player_->getTrackCount();
        
        return response;
    }
    
    json getExampleResponse() const override {
        return {
            {"success", true},
            {"state", "playing"},
            {"position_ms", 45230},
            {"duration_ms", 180000},
            {"progress_percent", 25.13f},
            {"tempo", 1.0f},
            {"transpose", 0},
            {"file", "songs/example.mid"},
            {"has_file_loaded", true},
            {"track_count", 16}
        };
    }

private:
    std::shared_ptr<MidiPlayer> player_;
};

} // namespace midiMind
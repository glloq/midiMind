// ============================================================================
// FICHIER 12/62: src/api/commands/routes/RouteVolumeCommand.h
// ============================================================================

class RouteVolumeCommand : public BaseCommand {
public:
    RouteVolumeCommand(const json& params, std::shared_ptr<MidiRouter> router)
        : BaseCommand(params), router_(router) {}
    
    std::string getName() const override { return "routes.volume"; }
    std::string getDescription() const override {
        return "Set volume for a specific route";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "channel"}, {"type", "integer"}, {"required", true}},
            {{"name", "device_id"}, {"type", "string"}, {"required", true}},
            {{"name", "volume"}, {"type", "number"}, {"required", true},
             {"description", "Volume level (0.0 to 1.0)"}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateMidiChannel("channel", error) &&
               validateDeviceId("device_id", error) &&
               validateVolume("volume", error);
    }
    
    json execute() override {
        int channel = params_["channel"];
        std::string deviceId = params_["device_id"];
        float volume = params_["volume"];
        
        router_->setVolume(channel, deviceId, volume);
        
        return jsonSuccess("Volume set to " + std::to_string(volume));
    }

private:
    std::shared_ptr<MidiRouter> router_;
};
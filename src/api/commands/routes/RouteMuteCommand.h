// ============================================================================
// FICHIER 10/62: src/api/commands/routes/RouteMuteCommand.h
// ============================================================================

class RouteMuteCommand : public BaseCommand {
public:
    RouteMuteCommand(const json& params, std::shared_ptr<MidiRouter> router)
        : BaseCommand(params), router_(router) {}
    
    std::string getName() const override { return "routes.mute"; }
    std::string getDescription() const override {
        return "Mute/unmute a route or entire channel";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "channel"}, {"type", "integer"}, {"required", true}},
            {{"name", "mute"}, {"type", "boolean"}, {"required", true}},
            {{"name", "device_id"}, {"type", "string"}, {"required", false},
             {"description", "If omitted, mutes entire channel"}}
        });
    }
    
    bool validate(std::string& error) const override {
        if (!validateMidiChannel("channel", error)) return false;
        if (!validateBoolean("mute", error)) return false;
        
        // device_id optionnel
        if (params_.contains("device_id")) {
            return validateDeviceId("device_id", error);
        }
        
        return true;
    }
    
    json execute() override {
        int channel = params_["channel"];
        bool mute = params_["mute"];
        std::string deviceId = getOptional<std::string>("device_id", "");
        
        router_->setMute(channel, deviceId, mute);
        
        return jsonSuccess(std::string(mute ? "Muted" : "Unmuted") + 
                          " channel " + std::to_string(channel));
    }

private:
    std::shared_ptr<MidiRouter> router_;
};

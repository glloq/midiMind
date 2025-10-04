// ============================================================================
// FICHIER 8/62: src/api/commands/routes/RouteRemoveCommand.h
// ============================================================================

class RouteRemoveCommand : public BaseCommand {
public:
    RouteRemoveCommand(const json& params, std::shared_ptr<MidiRouter> router)
        : BaseCommand(params), router_(router) {}
    
    std::string getName() const override { return "routes.remove"; }
    std::string getDescription() const override {
        return "Remove a MIDI route";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "channel"}, {"type", "integer"}, {"required", true}},
            {{"name", "device_id"}, {"type", "string"}, {"required", true}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateMidiChannel("channel", error) &&
               validateDeviceId("device_id", error);
    }
    
    json execute() override {
        int channel = params_["channel"];
        std::string deviceId = params_["device_id"];
        
        router_->removeRoute(channel, deviceId);
        
        return jsonSuccess("Route removed");
    }

private:
    std::shared_ptr<MidiRouter> router_;
};
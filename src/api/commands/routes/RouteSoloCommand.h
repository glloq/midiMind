// ============================================================================
// FICHIER 11/62: src/api/commands/routes/RouteSoloCommand.h
// ============================================================================

class RouteSoloCommand : public BaseCommand {
public:
    RouteSoloCommand(const json& params, std::shared_ptr<MidiRouter> router)
        : BaseCommand(params), router_(router) {}
    
    std::string getName() const override { return "routes.solo"; }
    std::string getDescription() const override {
        return "Solo a route or entire channel";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "channel"}, {"type", "integer"}, {"required", true}},
            {{"name", "solo"}, {"type", "boolean"}, {"required", true}},
            {{"name", "device_id"}, {"type", "string"}, {"required", false}}
        });
    }
    
    bool validate(std::string& error) const override {
        if (!validateMidiChannel("channel", error)) return false;
        if (!validateBoolean("solo", error)) return false;
        
        if (params_.contains("device_id")) {
            return validateDeviceId("device_id", error);
        }
        
        return true;
    }
    
    json execute() override {
        int channel = params_["channel"];
        bool solo = params_["solo"];
        std::string deviceId = getOptional<std::string>("device_id", "");
        
        router_->setSolo(channel, deviceId, solo);
        
        return jsonSuccess("Solo " + std::string(solo ? "enabled" : "disabled"));
    }

private:
    std::shared_ptr<MidiRouter> router_;
};
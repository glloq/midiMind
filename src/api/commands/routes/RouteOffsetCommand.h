/ ============================================================================
// FICHIER 13/62: src/api/commands/routes/RouteOffsetCommand.h
// ============================================================================

class RouteOffsetCommand : public BaseCommand {
public:
    RouteOffsetCommand(const json& params, std::shared_ptr<MidiRouter> router)
        : BaseCommand(params), router_(router) {}
    
    std::string getName() const override { return "routes.offset"; }
    std::string getDescription() const override {
        return "Set timing offset for a specific route";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "channel"}, {"type", "integer"}, {"required", true}},
            {{"name", "device_id"}, {"type", "string"}, {"required", true}},
            {{"name", "offset_ms"}, {"type", "integer"}, {"required", true},
             {"description", "Timing offset in milliseconds (0-10000)"}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateMidiChannel("channel", error) &&
               validateDeviceId("device_id", error) &&
               validateRange("offset_ms", 0, 10000, error);
    }
    
    json execute() override {
        int channel = params_["channel"];
        std::string deviceId = params_["device_id"];
        int offsetMs = params_["offset_ms"];
        
        router_->setOffset(channel, deviceId, offsetMs);
        
        return jsonSuccess("Offset set to " + std::to_string(offsetMs) + "ms");
    }

private:
    std::shared_ptr<MidiRouter> router_;
};

} // namespace midiMind

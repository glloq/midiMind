// ============================================================================
// FICHIER 27/62: src/api/commands/midi/MidiSendCommand.h
// ============================================================================

class MidiSendCommand : public BaseCommand {
public:
    MidiSendCommand(const json& params, 
                    std::shared_ptr<MidiDeviceManager> deviceManager)
        : BaseCommand(params), deviceManager_(deviceManager) {}
    
    std::string getName() const override { return "midi.send"; }
    std::string getDescription() const override {
        return "Send a raw MIDI message to a device";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "device_id"}, {"type", "string"}, {"required", true}},
            {{"name", "status"}, {"type", "integer"}, {"required", true},
             {"description", "MIDI status byte (0x80-0xFF)"}},
            {{"name", "data1"}, {"type", "integer"}, {"required", true},
             {"description", "First data byte (0-127)"}},
            {{"name", "data2"}, {"type", "integer"}, {"required", false},
             {"description", "Second data byte (0-127)"}}
        });
    }
    
    bool validate(std::string& error) const override {
        if (!validateDeviceId("device_id", error)) return false;
        if (!validateRange("status", 0x80, 0xFF, error)) return false;
        if (!validateRange("data1", 0, 127, error)) return false;
        
        if (params_.contains("data2")) {
            return validateRange("data2", 0, 127, error);
        }
        
        return true;
    }
    
    json execute() override {
        std::string deviceId = params_["device_id"];
        uint8_t status = params_["status"];
        uint8_t data1 = params_["data1"];
        uint8_t data2 = getOptional("data2", 0);
        
        auto device = deviceManager_->getDevice(deviceId);
        if (!device) {
            return jsonError("Device not found: " + deviceId);
        }
        
        if (!device->isConnected()) {
            return jsonError("Device not connected");
        }
        
        MidiMessage msg(status, data1, data2);
        
        if (device->sendMessage(msg)) {
            return jsonSuccess("MIDI message sent");
        } else {
            return jsonError("Failed to send MIDI message");
        }
    }

private:
    std::shared_ptr<MidiDeviceManager> deviceManager_;
};

} // namespace midiMind
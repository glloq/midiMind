// ============================================================================
// FICHIER 7/62: src/api/commands/routes/RouteAddCommand.h
// ============================================================================
#pragma once

#include "../../../core/commands/BaseCommand.h"
#include "../../../midi/MidiRouter.h"

namespace midiMind {

class RouteAddCommand : public BaseCommand {
public:
    RouteAddCommand(const json& params, std::shared_ptr<MidiRouter> router)
        : BaseCommand(params), router_(router) {}
    
    std::string getName() const override { return "routes.add"; }
    std::string getDescription() const override {
        return "Add a MIDI route from a channel to a device";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "channel"}, {"type", "integer"}, {"required", true}, 
             {"description", "MIDI channel (0-15)"}},
            {{"name", "device_id"}, {"type", "string"}, {"required", true},
             {"description", "Target device ID"}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateMidiChannel("channel", error) &&
               validateDeviceId("device_id", error);
    }
    
    json execute() override {
        int channel = params_["channel"];
        std::string deviceId = params_["device_id"];
        
        router_->addRoute(channel, deviceId);
        
        return jsonSuccess("Route added: channel " + std::to_string(channel) + 
                          " → " + deviceId);
    }

private:
    std::shared_ptr<MidiRouter> router_;
};


// ============================================================================
// FICHIER 6/62: src/api/commands/devices/DeviceDisconnectCommand.h
// ============================================================================
#pragma once

#include "../../../core/commands/BaseCommand.h"
#include "../../../midi/devices/MidiDeviceManager.h"

namespace midiMind {

/**
 * @brief Commande pour déconnecter un périphérique MIDI
 */
class DeviceDisconnectCommand : public BaseCommand {
public:
    DeviceDisconnectCommand(const json& params, 
                           std::shared_ptr<MidiDeviceManager> deviceManager)
        : BaseCommand(params), deviceManager_(deviceManager) {}
    
    std::string getName() const override { 
        return "devices.disconnect"; 
    }
    
    std::string getDescription() const override {
        return "Disconnect from a MIDI device";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {
                {"name", "device_id"},
                {"type", "string"},
                {"required", true},
                {"description", "ID of the device to disconnect"}
            }
        });
    }
    
    bool validate(std::string& error) const override {
        if (!validateDeviceId("device_id", error)) {
            return false;
        }
        
        return true;
    }
    
    json execute() override {
        std::string deviceId = params_["device_id"];
        
        // Vérifier que le device existe
        auto device = deviceManager_->getDevice(deviceId);
        if (!device) {
            return jsonError("Device not found: " + deviceId);
        }
        
        // Déconnecter
        deviceManager_->disconnectDevice(deviceId);
        
        json response = jsonSuccess("Device disconnected successfully");
        response["device_id"] = deviceId;
        
        return response;
    }
    
    json getExampleRequest() const override {
        return {
            {"command", "devices.disconnect"},
            {"device_id", "usb_0"}
        };
    }
    
    json getExampleResponse() const override {
        return {
            {"success", true},
            {"message", "Device disconnected successfully"},
            {"device_id", "usb_0"}
        };
    }

private:
    std::shared_ptr<MidiDeviceManager> deviceManager_;
};

} // namespace midiMind

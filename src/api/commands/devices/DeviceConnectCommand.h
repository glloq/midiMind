// ============================================================================
// FICHIER 5/62: src/api/commands/devices/DeviceConnectCommand.h
// ============================================================================
#pragma once

#include "../../../core/commands/BaseCommand.h"
#include "../../../midi/devices/MidiDeviceManager.h"

namespace midiMind {

/**
 * @brief Commande pour connecter un périphérique MIDI
 */
class DeviceConnectCommand : public BaseCommand {
public:
    DeviceConnectCommand(const json& params, 
                        std::shared_ptr<MidiDeviceManager> deviceManager)
        : BaseCommand(params), deviceManager_(deviceManager) {}
    
    std::string getName() const override { 
        return "devices.connect"; 
    }
    
    std::string getDescription() const override {
        return "Connect to a MIDI device by its ID";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {
                {"name", "device_id"},
                {"type", "string"},
                {"required", true},
                {"description", "ID of the device to connect (format: usb_*, wifi_*, bt_*)"}
            }
        });
    }
    
    bool validate(std::string& error) const override {
        // Valider device_id
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
        
        // Vérifier si déjà connecté
        if (device->isConnected()) {
            return jsonError("Device already connected");
        }
        
        // Connecter
        if (deviceManager_->connectDevice(deviceId)) {
            json response = jsonSuccess("Device connected successfully");
            response["device_id"] = deviceId;
            response["device_name"] = device->getName();
            return response;
        } else {
            return jsonError("Failed to connect to device");
        }
    }
    
    json getExampleRequest() const override {
        return {
            {"command", "devices.connect"},
            {"device_id", "usb_0"}
        };
    }
    
    json getExampleResponse() const override {
        return {
            {"success", true},
            {"message", "Device connected successfully"},
            {"device_id", "usb_0"},
            {"device_name", "Roland FP-30"}
        };
    }

private:
    std::shared_ptr<MidiDeviceManager> deviceManager_;
};

} // namespace midiMind


// ============================================================================
// FICHIER 4/62: src/api/commands/devices/DeviceListCommand.h
// ============================================================================

#pragma once

#include "../../../core/commands/BaseCommand.h"
#include "../../../midi/devices/MidiDeviceManager.h"

namespace midiMind {

/**
 * @brief Commande pour lister tous les périphériques MIDI disponibles
 */
class DeviceListCommand : public BaseCommand {
public:
    DeviceListCommand(const json& params, 
                     std::shared_ptr<MidiDeviceManager> deviceManager)
        : BaseCommand(params), deviceManager_(deviceManager) {}
    
    std::string getName() const override { 
        return "devices.list"; 
    }
    
    std::string getDescription() const override {
        return "List all available MIDI devices (USB, WiFi, Bluetooth)";
    }
    
    bool validate(std::string& error) const override {
        // Pas de paramètres requis
        return true;
    }
    
    json execute() override {
        auto devices = deviceManager_->listDevices();
        
        json devicesArray = json::array();
        
        for (const auto& device : devices) {
            json dev;
            dev["id"] = device->getId();
            dev["name"] = device->getName();
            dev["type"] = device->getTypeString();
            dev["status"] = device->getStatusString();
            dev["is_connected"] = device->isConnected();
            
            devicesArray.push_back(dev);
        }
        
        json response = jsonSuccess();
        response["devices"] = devicesArray;
        response["count"] = devicesArray.size();
        
        return response;
    }
    
    json getExampleResponse() const override {
        return {
            {"success", true},
            {"count", 2},
            {"devices", json::array({
                {
                    {"id", "usb_0"},
                    {"name", "Roland FP-30"},
                    {"type", "USB"},
                    {"status", "connected"},
                    {"is_connected", true}
                },
                {
                    {"id", "wifi_192.168.1.100_5004"},
                    {"name", "WiFi MIDI Device"},
                    {"type", "WiFi"},
                    {"status", "disconnected"},
                    {"is_connected", false}
                }
            })}
        };
    }

private:
    std::shared_ptr<MidiDeviceManager> deviceManager_;
};

} // namespace midiMind
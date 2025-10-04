#include "MidiDeviceManager.h"
#include "Logger.h"
#include <algorithm>

void MidiDeviceManager::addDevice(std::shared_ptr<MidiDevice> device) {
    std::lock_guard<std::mutex> lock(devMutex);
    devices.push_back(device);
    Logger::log(LogLevel::INFO, "Device added: " + device->getName());
}

void MidiDeviceManager::removeDevice(const std::string& deviceName) {
    std::lock_guard<std::mutex> lock(devMutex);
    devices.erase(std::remove_if(devices.begin(), devices.end(),
        [&](const std::shared_ptr<MidiDevice>& d){ return d->getName() == deviceName; }),
        devices.end());
    Logger::log(LogLevel::INFO, "Device removed: " + deviceName);
}

std::vector<std::shared_ptr<MidiDevice>> MidiDeviceManager::listDevices() {
    std::lock_guard<std::mutex> lock(devMutex);
    return devices;
}

std::shared_ptr<MidiDevice> MidiDeviceManager::getDeviceByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(devMutex);
    for (auto& d : devices) {
        if (d->getName() == name) return d;
    }
    return nullptr;
}

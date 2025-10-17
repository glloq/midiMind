// ============================================================================
// File: backend/src/midi/devices/MidiDeviceManager.cpp
// Version: 4.1.0 - COMPATIBLE
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#include "MidiDeviceManager.h"
#include "../../core/Logger.h"
#include <algorithm>
#include <chrono>
#include <thread>

#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

MidiDeviceManager::MidiDeviceManager() {
    Logger::info("MidiDeviceManager", "Initializing MidiDeviceManager v4.1.0");
}

MidiDeviceManager::~MidiDeviceManager() {
    Logger::info("MidiDeviceManager", "Shutting down MidiDeviceManager");
    stopHotPlugMonitoring();
    disconnectAll();
}

// ============================================================================
// DEVICE DISCOVERY
// ============================================================================

std::vector<MidiDeviceInfo> MidiDeviceManager::discoverDevices(bool fullScan) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiDeviceManager", "Starting device discovery (fullScan=" + 
                std::string(fullScan ? "true" : "false") + ")");
    
    if (fullScan) {
        availableDevices_.clear();
    }
    
    // Discover USB devices via ALSA
    auto usbDevices = discoverUsbDevices();
    availableDevices_.insert(availableDevices_.end(), usbDevices.begin(), usbDevices.end());
    
    Logger::info("MidiDeviceManager", "Discovery complete: " + 
                std::to_string(availableDevices_.size()) + " devices found");
    
    return availableDevices_;
}

std::vector<MidiDeviceInfo> MidiDeviceManager::getAvailableDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return availableDevices_;
}

int MidiDeviceManager::getDeviceCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(devices_.size());
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

bool MidiDeviceManager::connect(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiDeviceManager", "Connecting to device: " + deviceId);
    
    // Check if already connected
    for (const auto& device : devices_) {
        if (device->getId() == deviceId) {
            Logger::warning("MidiDeviceManager", "Device already connected: " + deviceId);
            return true;
        }
    }
    
    // Find device info
    auto it = std::find_if(availableDevices_.begin(), availableDevices_.end(),
                          [&deviceId](const MidiDeviceInfo& info) {
                              return info.id == deviceId;
                          });
    
    if (it == availableDevices_.end()) {
        Logger::error("MidiDeviceManager", "Device not found: " + deviceId);
        return false;
    }
    
    // Create device
    auto device = createDevice(*it);
    if (!device) {
        Logger::error("MidiDeviceManager", "Failed to create device: " + deviceId);
        return false;
    }
    
    // Connect device
    if (!device->connect()) {
        Logger::error("MidiDeviceManager", "Failed to connect device: " + deviceId);
        return false;
    }
    
    devices_.push_back(device);
    
    Logger::info("MidiDeviceManager", "✓ Device connected: " + device->getName());
    
    if (onDeviceConnect_) {
        onDeviceConnect_(deviceId);
    }
    
    return true;
}

void MidiDeviceManager::disconnect(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiDeviceManager", "Disconnecting device: " + deviceId);
    
    auto it = std::find_if(devices_.begin(), devices_.end(),
                          [&deviceId](const std::shared_ptr<MidiDevice>& device) {
                              return device->getId() == deviceId;
                          });
    
    if (it != devices_.end()) {
        (*it)->disconnect();
        devices_.erase(it);
        
        Logger::info("MidiDeviceManager", "✓ Device disconnected: " + deviceId);
        
        if (onDeviceDisconnect_) {
            onDeviceDisconnect_(deviceId);
        }
    }
}

void MidiDeviceManager::disconnectAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiDeviceManager", "Disconnecting all devices...");
    
    for (auto& device : devices_) {
        device->disconnect();
    }
    
    devices_.clear();
    
    Logger::info("MidiDeviceManager", "✓ All devices disconnected");
}

bool MidiDeviceManager::isConnected(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return std::any_of(devices_.begin(), devices_.end(),
                      [&deviceId](const std::shared_ptr<MidiDevice>& device) {
                          return device->getId() == deviceId;
                      });
}

// ============================================================================
// DEVICE ACCESS
// ============================================================================

std::shared_ptr<MidiDevice> MidiDeviceManager::getDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(devices_.begin(), devices_.end(),
                          [&deviceId](const std::shared_ptr<MidiDevice>& device) {
                              return device->getId() == deviceId;
                          });
    
    return (it != devices_.end()) ? *it : nullptr;
}

std::vector<std::shared_ptr<MidiDevice>> MidiDeviceManager::getConnectedDevices() {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_;
}

// ============================================================================
// HOT-PLUG MONITORING
// ============================================================================

void MidiDeviceManager::startHotPlugMonitoring(int intervalMs) {
    if (hotPlugRunning_) {
        Logger::warning("MidiDeviceManager", "Hot-plug monitoring already active");
        return;
    }
    
    Logger::info("MidiDeviceManager", "Starting hot-plug monitoring (interval=" + 
                std::to_string(intervalMs) + "ms)");
    
    scanIntervalMs_ = intervalMs;
    hotPlugRunning_ = true;
    hotPlugThread_ = std::thread(&MidiDeviceManager::hotPlugThread, this);
    
    Logger::info("MidiDeviceManager", "✓ Hot-plug monitoring started");
}

void MidiDeviceManager::stopHotPlugMonitoring() {
    if (!hotPlugRunning_) {
        return;
    }
    
    Logger::info("MidiDeviceManager", "Stopping hot-plug monitoring...");
    
    hotPlugRunning_ = false;
    
    if (hotPlugThread_.joinable()) {
        hotPlugThread_.join();
    }
    
    Logger::info("MidiDeviceManager", "✓ Hot-plug monitoring stopped");
}

bool MidiDeviceManager::isHotPlugMonitoringActive() const {
    return hotPlugRunning_;
}

void MidiDeviceManager::setHotPlugCallbacks(
    std::function<void(const std::string&)> onConnect,
    std::function<void(const std::string&)> onDisconnect) 
{
    onDeviceConnect_ = onConnect;
    onDeviceDisconnect_ = onDisconnect;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void MidiDeviceManager::hotPlugThread() {
    Logger::debug("MidiDeviceManager", "Hot-plug monitor thread started");
    
    while (hotPlugRunning_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(scanIntervalMs_));
        
        if (!hotPlugRunning_) break;
        
        Logger::debug("MidiDeviceManager", "Hot-plug: Rescanning devices...");
        discoverDevices(false);
    }
    
    Logger::debug("MidiDeviceManager", "Hot-plug monitor thread stopped");
}

std::vector<MidiDeviceInfo> MidiDeviceManager::discoverUsbDevices() {
    std::vector<MidiDeviceInfo> devices;
    
#ifdef __linux__
    Logger::info("MidiDeviceManager", "Scanning USB MIDI devices (ALSA)...");
    
    snd_seq_t* seq = nullptr;
    
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        Logger::warning("MidiDeviceManager", "Failed to open ALSA sequencer");
        return devices;
    }
    
    snd_seq_client_info_t* cinfo;
    snd_seq_port_info_t* pinfo;
    
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    
    snd_seq_client_info_set_client(cinfo, -1);
    
    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);
        
        if (client == 0 || client == SND_SEQ_CLIENT_SYSTEM) {
            continue;
        }
        
        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        
        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            unsigned int caps = snd_seq_port_info_get_capability(pinfo);
            
            if ((caps & SND_SEQ_PORT_CAP_READ) || (caps & SND_SEQ_PORT_CAP_WRITE)) {
                MidiDeviceInfo info;
                
                int port = snd_seq_port_info_get_port(pinfo);
                info.id = "usb_" + std::to_string(client) + "_" + std::to_string(port);
                info.name = snd_seq_port_info_get_name(pinfo);
                info.type = DeviceType::USB;
                info.port = std::to_string(client) + ":" + std::to_string(port);
                info.manufacturer = snd_seq_client_info_get_name(cinfo);
                info.available = true;
                info.status = DeviceStatus::DISCONNECTED;
                info.messagesReceived = 0;
                info.messagesSent = 0;
                
                if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_WRITE)) {
                    info.direction = DeviceDirection::BIDIRECTIONAL;
                } else if (caps & SND_SEQ_PORT_CAP_READ) {
                    info.direction = DeviceDirection::INPUT;
                } else {
                    info.direction = DeviceDirection::OUTPUT;
                }
                
                devices.push_back(info);
                
                Logger::info("MidiDeviceManager", "  Found: " + info.name);
            }
        }
    }
    
    snd_seq_close(seq);
    
    Logger::info("MidiDeviceManager", "✓ USB scan complete: " + 
                std::to_string(devices.size()) + " devices found");
#else
    Logger::warning("MidiDeviceManager", "USB MIDI scanning not supported on this platform");
#endif
    
    return devices;
}

std::shared_ptr<MidiDevice> MidiDeviceManager::createDevice(const MidiDeviceInfo& info) {
    Logger::debug("MidiDeviceManager", "Creating device: " + info.name);
    
    // TODO: Implement device creation based on type
    Logger::warning("MidiDeviceManager", "Device creation not yet implemented");
    
    return nullptr;
}

} // namespace midiMind

// ============================================================================
// END OF FILE MidiDeviceManager.cpp
// ============================================================================

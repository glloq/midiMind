// ============================================================================
// src/midi/devices/plugins/UsbDevicePlugin.h
// ============================================================================
#pragma once
#include "../DevicePlugin.h"
#include <RtMidi.h>

namespace midiMind {

class UsbDevicePlugin : public IDevicePlugin {
public:
    std::string getName() const override { return "USB MIDI"; }
    std::string getVersion() const override { return "2.0.0"; }
    DeviceType getType() const override { return DeviceType::USB; }
    
    bool supportsDiscovery() const override { return true; }
    bool supportsHotplug() const override { return true; }
    
    bool initialize() override {
        try {
            midiOut_ = std::make_unique<RtMidiOut>();
            Logger::info("UsbPlugin", "Initialized RtMidi");
            return true;
        } catch (RtMidiError& error) {
            Logger::error("UsbPlugin", "Init failed: " + error.getMessage());
            return false;
        }
    }
    
    void shutdown() override {
        midiOut_.reset();
        Logger::info("UsbPlugin", "Shutdown complete");
    }
    
    std::vector<DeviceInfo> discover() override {
        std::vector<DeviceInfo> devices;
        
        if (!midiOut_) return devices;
        
        try {
            unsigned int nPorts = midiOut_->getPortCount();
            
            for (unsigned int i = 0; i < nPorts; i++) {
                std::string portName = midiOut_->getPortName(i);
                
                DeviceInfo info;
                info.id = "usb_" + std::to_string(i);
                info.name = portName;
                info.type = DeviceType::USB;
                info.metadata["usb_port"] = i;
                info.metadata["port_name"] = portName;
                
                devices.push_back(info);
            }
            
        } catch (RtMidiError& error) {
            Logger::error("UsbPlugin", "Discovery error: " + error.getMessage());
        }
        
        return devices;
    }
    
    std::shared_ptr<MidiDevice> createDevice(const DeviceInfo& info) override {
        int portNumber = info.metadata.value("usb_port", -1);
        
        if (portNumber < 0) {
            Logger::error("UsbPlugin", "Invalid port number in DeviceInfo");
            return nullptr;
        }
        
        // Créer le device avec retry et validation
        auto device = std::make_shared<EnhancedUsbMidiDevice>(
            info.id, 
            info.name, 
            portNumber
        );
        
        return device;
    }

private:
    std::unique_ptr<RtMidiOut> midiOut_;
};

// ============================================================================
// DEVICE USB AMÉLIORÉ
// ============================================================================

class EnhancedUsbMidiDevice : public MidiDevice {
public:
    EnhancedUsbMidiDevice(const std::string& id, const std::string& name, int port)
        : MidiDevice(id, name, DeviceType::USB), portNumber_(port) {
        
        connectionRetryCount_ = 0;
        maxRetries_ = 3;
    }

    bool connect() override {
        for (int attempt = 0; attempt < maxRetries_; attempt++) {
            if (attempt > 0) {
                Logger::info("UsbDevice", "Retry " + std::to_string(attempt) + "/" + 
                           std::to_string(maxRetries_));
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            try {
                setStatus(DeviceStatus::CONNECTING);
                
                midiOut_ = std::make_unique<RtMidiOut>();
                midiOut_->openPort(portNumber_, name_);
                
                // Validation: envoyer un message test
                if (validateConnection()) {
                    setStatus(DeviceStatus::CONNECTED);
                    connectionRetryCount_ = 0;
                    Logger::info("UsbDevice", "✓ Connected: " + name_);
                    return true;
                }
                
            } catch (RtMidiError& error) {
                Logger::warn("UsbDevice", "Attempt " + std::to_string(attempt + 1) + 
                           " failed: " + error.getMessage());
                midiOut_.reset();
            }
        }
        
        setStatus(DeviceStatus::ERROR);
        return false;
    }

    void disconnect() override {
        if (midiOut_ && midiOut_->isPortOpen()) {
            midiOut_->closePort();
            setStatus(DeviceStatus::DISCONNECTED);
            Logger::info("UsbDevice", "Disconnected: " + name_);
        }
    }

    bool sendMessage(const MidiMessage& msg) override {
        if (!isConnected() || !midiOut_) {
            // Ajouter au buffer pour retry
            messageBuffer_.push(msg);
            
            if (messageBuffer_.size() > MAX_BUFFER_SIZE) {
                Logger::warn("UsbDevice", "Buffer overflow, dropping oldest message");
                messageBuffer_.pop();
            }
            
            // Tenter reconnexion asynchrone
            if (shouldAutoReconnect() && !reconnecting_.test_and_set()) {
                std::thread([this]() {
                    if (attemptReconnect()) {
                        flushBuffer();
                    }
                    reconnecting_.clear();
                }).detach();
            }
            
            return false;
        }

        try {
            midiOut_->sendMessage(&msg.getData());
            lastSuccessfulSend_ = std::chrono::steady_clock::now();
            return true;
            
        } catch (RtMidiError& error) {
            Logger::error("UsbDevice", "Send error: " + error.getMessage());
            setStatus(DeviceStatus::ERROR);
            messageBuffer_.push(msg); // Buffer pour retry
            return false;
        }
    }

private:
    bool validateConnection() {
        // Envoyer un message de test (Active Sensing)
        try {
            std::vector<uint8_t> testMsg = {0xFE};
            midiOut_->sendMessage(&testMsg);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true;
        } catch (...) {
            return false;
        }
    }
    
    void flushBuffer() {
        Logger::info("UsbDevice", "Flushing " + std::to_string(messageBuffer_.size()) + 
                    " buffered messages");
        
        while (!messageBuffer_.empty()) {
            MidiMessage msg = messageBuffer_.front();
            messageBuffer_.pop();
            
            if (!sendMessage(msg)) {
                break; // Stop si échec
            }
        }
    }

    std::unique_ptr<RtMidiOut> midiOut_;
    int portNumber_;
    int connectionRetryCount_;
    int maxRetries_;
    
    std::queue<MidiMessage> messageBuffer_;
    static constexpr size_t MAX_BUFFER_SIZE = 1000;
    
    std::atomic_flag reconnecting_ = ATOMIC_FLAG_INIT;
    std::chrono::steady_clock::time_point lastSuccessfulSend_;
};

// Auto-enregistrement du plugin
REGISTER_DEVICE_PLUGIN(UsbDevicePlugin);

} // namespace midiMind

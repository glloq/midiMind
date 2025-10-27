// ============================================================================
// File: backend/src/midi/devices/VirtualMidiDevice.h
// Version: 4.2.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#pragma once

#include "MidiDevice.h"
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

namespace midiMind {

class VirtualMidiDevice : public MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    VirtualMidiDevice(const std::string& id, const std::string& name);
    ~VirtualMidiDevice() override;
    
    // ========================================================================
    // MIDIDEVICE INTERFACE IMPLEMENTATION
    // ========================================================================
    
    bool connect() override;
    bool disconnect() override;
    bool sendMessage(const MidiMessage& message) override;
    MidiMessage receiveMessage() override;
    bool isConnected() const override;
    
    bool requestIdentity() override;
    json getCapabilities() const override;
    
    // ========================================================================
    // ADDITIONAL METHODS
    // ========================================================================
    
    bool hasMessages() const override;
    std::string getPort() const override;
    json getInfo() const override;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    void setPortDirection(bool input, bool output);
    void setMaxQueueSize(size_t size);
    size_t getMessageCount() const;
    void clearMessages();
    
    // ========================================================================
    // CALLBACK
    // ========================================================================
    
    void setMessageCallback(std::function<void(const MidiMessage&)> callback);

private:
    // ========================================================================
    // PRIVATE METHODS - ALSA
    // ========================================================================
    
    bool openSequencer();
    void closeSequencer();
    bool createVirtualPort();
    void deleteVirtualPort();
    
    // ========================================================================
    // PRIVATE METHODS - THREADING
    // ========================================================================
    
    void receiveThreadFunc();
    void processAlsaEvent(const snd_seq_event_t* ev);
    
    // ========================================================================
    // PRIVATE METHODS - CONVERSION
    // ========================================================================
    
    void midiMessageToAlsaEvent(const MidiMessage& msg, snd_seq_event_t* ev);
    MidiMessage alsaEventToMidiMessage(const snd_seq_event_t* ev);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
#ifdef __linux__
    snd_seq_t* alsaSeq_;
#else
    void* alsaSeq_;
#endif
    
    std::atomic<int> virtualPort_;
    
    std::atomic<bool> isInput_;
    std::atomic<bool> isOutput_;
    
    std::thread receiveThread_;
    std::atomic<bool> shouldStop_;
    
    std::queue<MidiMessage> receiveQueue_;
    mutable std::mutex receiveMutex_;
    
    std::queue<MidiMessage> sendQueue_;
    mutable std::mutex sendMutex_;
    
    std::atomic<size_t> maxQueueSize_;
    
    std::function<void(const MidiMessage&)> messageCallback_;
    std::mutex callbackMutex_;
};

} // namespace midiMind
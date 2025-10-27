// ============================================================================
// File: backend/src/midi/devices/UsbMidiDevice.h
// Version: 4.2.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#pragma once

#include "MidiDevice.h"
#include "../sysex/SysExHandler.h"
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>

#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

namespace midiMind {

class UsbMidiDevice : public MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    UsbMidiDevice(const std::string& id,
                  const std::string& name,
                  int alsaClient,
                  int alsaPort);
    
    ~UsbMidiDevice() override;
    
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
    // CALLBACK
    // ========================================================================
    
    void setMessageCallback(std::function<void(const MidiMessage&)> callback);
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    void setAutoReconnect(bool enabled);
    void setMaxRetries(int maxRetries);
    void setRetryDelay(int delayMs);
    void setSysExHandler(std::shared_ptr<SysExHandler> handler);
    void setMaxBufferSize(size_t size);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    json getAlsaStatistics() const;

private:
    // ========================================================================
    // PRIVATE METHODS - ALSA
    // ========================================================================
    
    bool openSequencer();
    void closeSequencer();
    bool createPorts();
    bool connectToPorts();
    void disconnectFromPorts();
    bool validateConnection();
    
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
    // PRIVATE METHODS - RECONNECTION
    // ========================================================================
    
    bool attemptReconnect();
    void flushMessageBuffer();
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
#ifdef __linux__
    snd_seq_t* alsaSeq_;
#else
    void* alsaSeq_;
#endif
    
    int alsaClient_;
    int alsaPort_;
    std::atomic<int> myPort_;
    
    std::thread receiveThread_;
    std::atomic<bool> shouldStop_;
    
    std::queue<MidiMessage> receiveQueue_;
    mutable std::mutex receiveMutex_;
    std::condition_variable receiveCv_;
    
    std::queue<MidiMessage> sendBuffer_;
    mutable std::mutex sendMutex_;
    std::atomic<size_t> maxBufferSize_;
    
    std::function<void(const MidiMessage&)> messageCallback_;
    std::mutex callbackMutex_;
    
    std::atomic<bool> autoReconnect_;
    std::atomic<int> retryCount_;
    std::atomic<int> maxRetries_;
    std::atomic<int> retryDelayMs_;
    std::atomic_flag reconnecting_;
    
    std::atomic<uint64_t> alsaEventsReceived_;
    std::atomic<uint64_t> alsaEventsSent_;
    std::atomic<uint64_t> alsaErrors_;
    
    std::shared_ptr<SysExHandler> sysexHandler_;
};

} // namespace midiMind
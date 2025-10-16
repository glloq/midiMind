// ============================================================================
// File: backend/src/midi/devices/UsbMidiDevice.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   USB MIDI device implementation using ALSA Sequencer API.
//   Supports USB MIDI Class compliant devices on Linux.
//
// Features:
//   - ALSA sequencer integration
//   - Bidirectional communication
//   - Asynchronous message reception
//   - Auto-reconnection support
//   - Message buffering
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Enhanced ALSA error handling
//   - Better thread management
//   - Improved auto-reconnect
//
// ============================================================================

#pragma once

#include "MidiDevice.h"
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>

// ALSA includes (Linux only)
#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

namespace midiMind {

/**
 * @class UsbMidiDevice
 * @brief USB MIDI device using ALSA
 * 
 * Implements MIDI communication with USB MIDI Class devices through
 * ALSA (Advanced Linux Sound Architecture) sequencer API.
 * 
 * Thread Safety: YES
 * Platform: Linux only
 * 
 * Example:
 * ```cpp
 * // Create device (client 128, port 0)
 * UsbMidiDevice device("usb_128_0", "Yamaha Piano", 128, 0);
 * 
 * // Set callback
 * device.setMessageCallback([](const MidiMessage& msg) {
 *     std::cout << "Received: " << msg.getTypeName() << "\n";
 * });
 * 
 * // Connect
 * if (device.connect()) {
 *     // Send note
 *     device.sendMessage(MidiMessage::noteOn(0, 60, 100));
 * }
 * ```
 */
class UsbMidiDevice : public MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param id Device identifier
     * @param name Device name
     * @param alsaClient ALSA client number
     * @param alsaPort ALSA port number
     */
    UsbMidiDevice(const std::string& id,
                  const std::string& name,
                  int alsaClient,
                  int alsaPort);
    
    /**
     * @brief Destructor
     */
    ~UsbMidiDevice() override;
    
    // ========================================================================
    // MIDIDEVICE INTERFACE IMPLEMENTATION
    // ========================================================================
    
    bool connect() override;
    bool disconnect() override;
    bool sendMessage(const MidiMessage& message) override;
    MidiMessage receiveMessage() override;
    bool isConnected() const override;
    
    // ========================================================================
    // ADDITIONAL METHODS
    // ========================================================================
    
    /**
     * @brief Check if messages are available
     */
    bool hasMessages() const override;
    
    /**
     * @brief Get port string
     */
    std::string getPort() const override;
    
    /**
     * @brief Get device info
     */
    json getInfo() const override;
    
    // ========================================================================
    // CALLBACK
    // ========================================================================
    
    /**
     * @brief Set message received callback
     * @param callback Callback function
     * @note Called from receive thread
     */
    void setMessageCallback(std::function<void(const MidiMessage&)> callback);
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Enable/disable auto-reconnect
     * @param enabled State
     */
    void setAutoReconnect(bool enabled);
    
    /**
     * @brief Set max reconnection attempts
     * @param maxRetries Maximum retries
     */
    void setMaxRetries(int maxRetries);
    
    /**
     * @brief Set retry delay
     * @param delayMs Delay in milliseconds
     */
    void setRetryDelay(int delayMs);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get ALSA-specific statistics
     */
    json getAlsaStatistics() const;

private:
    // ========================================================================
    // PRIVATE METHODS - ALSA
    // ========================================================================
    
    /**
     * @brief Open ALSA sequencer
     */
    bool openSequencer();
    
    /**
     * @brief Close ALSA sequencer
     */
    void closeSequencer();
    
    /**
     * @brief Create ALSA ports
     */
    bool createPorts();
    
    /**
     * @brief Connect to target device ports
     */
    bool connectToPorts();
    
    /**
     * @brief Disconnect from ports
     */
    void disconnectFromPorts();
    
    /**
     * @brief Validate connection
     */
    bool validateConnection();
    
    // ========================================================================
    // PRIVATE METHODS - THREADING
    // ========================================================================
    
    /**
     * @brief Receive thread function
     */
    void receiveThreadFunc();
    
    /**
     * @brief Process ALSA event
     */
    void processAlsaEvent(const snd_seq_event_t* ev);
    
    // ========================================================================
    // PRIVATE METHODS - CONVERSION
    // ========================================================================
    
    /**
     * @brief Convert MidiMessage to ALSA event
     */
    void midiMessageToAlsaEvent(const MidiMessage& msg, snd_seq_event_t* ev);
    
    /**
     * @brief Convert ALSA event to MidiMessage
     */
    MidiMessage alsaEventToMidiMessage(const snd_seq_event_t* ev);
    
    // ========================================================================
    // PRIVATE METHODS - RECONNECTION
    // ========================================================================
    
    /**
     * @brief Attempt reconnection
     */
    bool attemptReconnect();
    
    /**
     * @brief Flush message buffer
     */
    void flushMessageBuffer();
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
#ifdef __linux__
    /// ALSA sequencer handle
    snd_seq_t* alsaSeq_;
#else
    void* alsaSeq_;  // Placeholder for non-Linux
#endif
    
    /// Target ALSA client number
    int alsaClient_;
    
    /// Target ALSA port number
    int alsaPort_;
    
    /// Our ALSA port number
    int myPort_;
    
    /// Receive thread
    std::thread receiveThread_;
    
    /// Stop flag
    std::atomic<bool> shouldStop_;
    
    /// Receive queue
    std::queue<MidiMessage> receiveQueue_;
    mutable std::mutex receiveMutex_;
    std::condition_variable receiveCv_;
    
    /// Send buffer (for retry)
    std::queue<MidiMessage> sendBuffer_;
    mutable std::mutex sendMutex_;
    static constexpr size_t MAX_BUFFER_SIZE = 1000;
    
    /// Message callback
    std::function<void(const MidiMessage&)> messageCallback_;
    std::mutex callbackMutex_;
    
    /// Auto-reconnect
    std::atomic<bool> autoReconnect_;
    std::atomic<int> retryCount_;
    int maxRetries_;
    int retryDelayMs_;
    std::atomic_flag reconnecting_;
    
    /// Statistics
    std::atomic<uint64_t> alsaEventsReceived_;
    std::atomic<uint64_t> alsaEventsSent_;
    std::atomic<uint64_t> alsaErrors_;
};

} // namespace midiMind
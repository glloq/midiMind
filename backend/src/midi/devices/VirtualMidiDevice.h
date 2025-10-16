// ============================================================================
// File: backend/src/midi/devices/VirtualMidiDevice.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Virtual MIDI device for internal routing and inter-process communication.
//   Creates ALSA virtual ports visible to other applications.
//
// Features:
//   - ALSA virtual port creation
//   - Internal message routing
//   - Lock-free queues
//   - Bidirectional communication
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Enhanced ALSA integration
//   - Better queue management
//   - Improved thread safety
//
// ============================================================================

#pragma once

#include "MidiDevice.h"
#include <queue>
#include <mutex>
#include <thread>

// ALSA includes (Linux only)
#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

namespace midiMind {

/**
 * @class VirtualMidiDevice
 * @brief Virtual MIDI device for internal routing
 * 
 * Creates an ALSA virtual port that can be used for:
 * - Internal routing between components
 * - Communication with other MIDI applications
 * - Testing and simulation
 * 
 * Thread Safety: YES
 * Platform: Linux (ALSA), with fallback for other platforms
 * 
 * Example:
 * ```cpp
 * // Create virtual device
 * VirtualMidiDevice device("virtual_0", "MidiMind Virtual");
 * 
 * // Set direction
 * device.setPortDirection(true, true);  // Bidirectional
 * 
 * // Connect
 * if (device.connect()) {
 *     // Send message
 *     device.sendMessage(MidiMessage::noteOn(0, 60, 100));
 *     
 *     // Receive message
 *     if (device.hasMessages()) {
 *         MidiMessage msg = device.receiveMessage();
 *     }
 * }
 * ```
 */
class VirtualMidiDevice : public MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param id Device identifier
     * @param name Port name
     */
    VirtualMidiDevice(const std::string& id, const std::string& name);
    
    /**
     * @brief Destructor
     */
    ~VirtualMidiDevice() override;
    
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
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Set port direction
     * @param input Enable input
     * @param output Enable output
     */
    void setPortDirection(bool input, bool output);
    
    /**
     * @brief Get message count in queue
     */
    size_t getMessageCount() const;
    
    /**
     * @brief Clear message queues
     */
    void clearMessages();
    
    // ========================================================================
    // CALLBACK
    // ========================================================================
    
    /**
     * @brief Set message received callback
     * @param callback Callback function
     */
    void setMessageCallback(std::function<void(const MidiMessage&)> callback);

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
     * @brief Create virtual port
     */
    bool createVirtualPort();
    
    /**
     * @brief Delete virtual port
     */
    void deleteVirtualPort();
    
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
    // MEMBER VARIABLES
    // ========================================================================
    
#ifdef __linux__
    /// ALSA sequencer handle
    snd_seq_t* alsaSeq_;
#else
    void* alsaSeq_;  // Placeholder
#endif
    
    /// Virtual port number
    int virtualPort_;
    
    /// Port capabilities
    bool isInput_;
    bool isOutput_;
    
    /// Receive thread
    std::thread receiveThread_;
    
    /// Stop flag
    std::atomic<bool> shouldStop_;
    
    /// Receive queue
    std::queue<MidiMessage> receiveQueue_;
    mutable std::mutex receiveMutex_;
    
    /// Send queue (for non-ALSA mode)
    std::queue<MidiMessage> sendQueue_;
    mutable std::mutex sendMutex_;
    
    /// Max queue size
    static constexpr size_t MAX_QUEUE_SIZE = 1000;
    
    /// Message callback
    std::function<void(const MidiMessage&)> messageCallback_;
    std::mutex callbackMutex_;
};

} // namespace midiMind
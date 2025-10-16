// ============================================================================
// File: backend/src/midi/devices/MidiDevice.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Abstract base class for all MIDI devices.
//   Defines common interface for USB, Network, Bluetooth, and Virtual devices.
//
// Features:
//   - Pure virtual interface
//   - Connection management
//   - Message sending/receiving
//   - Device metadata
//   - Statistics tracking
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Enhanced interface
//   - Better metadata support
//   - Improved statistics
//
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include <string>
#include <atomic>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum DeviceType
 * @brief Type of MIDI device
 */
enum class DeviceType {
    USB,        ///< USB MIDI device (ALSA)
    NETWORK,    ///< Network MIDI (RTP-MIDI, WiFi)
    BLUETOOTH,  ///< Bluetooth MIDI (BLE)
    VIRTUAL,    ///< Virtual MIDI device (ALSA Virtual)
    UNKNOWN     ///< Unknown type
};

/**
 * @enum DeviceDirection
 * @brief Direction of MIDI communication
 */
enum class DeviceDirection {
    INPUT,      ///< Input only
    OUTPUT,     ///< Output only
    BIDIRECTIONAL  ///< Both input and output
};

/**
 * @enum DeviceStatus
 * @brief Connection status
 */
enum class DeviceStatus {
    DISCONNECTED,   ///< Not connected
    CONNECTING,     ///< Connection in progress
    CONNECTED,      ///< Connected and ready
    ERROR           ///< Error state
};

// ============================================================================
// CLASS: MidiDevice (Abstract Base)
// ============================================================================

/**
 * @class MidiDevice
 * @brief Abstract base class for all MIDI devices
 * 
 * Defines the common interface that all MIDI device implementations must follow.
 * Provides basic functionality for connection management, messaging, and metadata.
 * 
 * Implementation Classes:
 * - UsbMidiDevice (ALSA Sequencer)
 * - WifiMidiDevice (RTP-MIDI)
 * - BleMidiDevice (BLE MIDI)
 * - VirtualMidiDevice (ALSA Virtual)
 * 
 * Thread Safety:
 * - Implementations must be thread-safe
 * - sendMessage() must be callable from multiple threads
 * - receiveMessage() must be thread-safe
 * 
 * Example:
 * ```cpp
 * class MyMidiDevice : public MidiDevice {
 * public:
 *     MyMidiDevice(const std::string& id, const std::string& name)
 *         : MidiDevice(id, name, DeviceType::USB, DeviceDirection::BIDIRECTIONAL) {}
 *     
 *     bool connect() override {
 *         // Implementation
 *         status_ = DeviceStatus::CONNECTED;
 *         return true;
 *     }
 *     
 *     bool sendMessage(const MidiMessage& msg) override {
 *         // Implementation
 *         messagesSent_++;
 *         return true;
 *     }
 *     
 *     // ... other methods
 * };
 * ```
 */
class MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param id Unique device identifier
     * @param name Device name
     * @param type Device type
     * @param direction Communication direction
     */
    MidiDevice(const std::string& id,
               const std::string& name,
               DeviceType type,
               DeviceDirection direction)
        : id_(id)
        , name_(name)
        , type_(type)
        , direction_(direction)
        , status_(DeviceStatus::DISCONNECTED)
        , messagesReceived_(0)
        , messagesSent_(0)
    {}
    
    /**
     * @brief Virtual destructor
     */
    virtual ~MidiDevice() = default;
    
    // Disable copy
    MidiDevice(const MidiDevice&) = delete;
    MidiDevice& operator=(const MidiDevice&) = delete;
    
    // ========================================================================
    // PURE VIRTUAL METHODS (MUST BE IMPLEMENTED)
    // ========================================================================
    
    /**
     * @brief Connect to device
     * @return true if successful
     * @note Must update status_
     * @note Must be thread-safe
     */
    virtual bool connect() = 0;
    
    /**
     * @brief Disconnect from device
     * @return true if successful
     * @note Must update status_
     * @note Must stop any reception threads
     * @note Must be thread-safe
     */
    virtual bool disconnect() = 0;
    
    /**
     * @brief Send MIDI message
     * @param message Message to send
     * @return true if successful
     * @note Must increment messagesSent_ on success
     * @note Must be thread-safe
     */
    virtual bool sendMessage(const MidiMessage& message) = 0;
    
    /**
     * @brief Receive MIDI message (if available)
     * @return MidiMessage Received message or empty message
     * @note Must increment messagesReceived_ when message received
     * @note Must be thread-safe
     */
    virtual MidiMessage receiveMessage() = 0;
    
    /**
     * @brief Check if device is connected
     * @return true if connected
     */
    virtual bool isConnected() const = 0;
    
    // ========================================================================
    // VIRTUAL METHODS WITH DEFAULT IMPLEMENTATION
    // ========================================================================
    
    /**
     * @brief Open device (alias for connect)
     * @return true if successful
     */
    virtual bool open() {
        return connect();
    }
    
    /**
     * @brief Close device (alias for disconnect)
     */
    virtual void close() {
        disconnect();
    }
    
    /**
     * @brief Send message (alias for sendMessage)
     * @param message Message to send
     */
    virtual void send(const MidiMessage& message) {
        sendMessage(message);
    }
    
    /**
     * @brief Receive message (alias for receiveMessage)
     * @return MidiMessage Received message
     */
    virtual MidiMessage receive() {
        return receiveMessage();
    }
    
    /**
     * @brief Check if messages are available
     * @return true if messages pending
     * @note Default returns false
     */
    virtual bool hasMessages() const {
        return false;
    }
    
    /**
     * @brief Get device port/address
     * @return std::string Port or address (empty by default)
     * @note USB: "128:0", Network: "192.168.1.42:5004", etc.
     */
    virtual std::string getPort() const {
        return "";
    }
    
    /**
     * @brief Get detailed device information
     * @return json Device information
     */
    virtual json getInfo() const {
        return {
            {"id", id_},
            {"name", name_},
            {"type", deviceTypeToString(type_)},
            {"direction", deviceDirectionToString(direction_)},
            {"status", deviceStatusToString(status_.load())},
            {"connected", isConnected()},
            {"messages_received", messagesReceived_.load()},
            {"messages_sent", messagesSent_.load()},
            {"port", getPort()}
        };
    }
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * @brief Get device ID
     */
    std::string getId() const { return id_; }
    
    /**
     * @brief Get device name
     */
    std::string getName() const { return name_; }
    
    /**
     * @brief Get device type
     */
    DeviceType getType() const { return type_; }
    
    /**
     * @brief Get direction
     */
    DeviceDirection getDirection() const { return direction_; }
    
    /**
     * @brief Get status
     */
    DeviceStatus getStatus() const { return status_.load(); }
    
    /**
     * @brief Check if open (alias for isConnected)
     */
    bool isOpen() const { return isConnected(); }
    
    /**
     * @brief Get messages received count
     */
    uint64_t getMessagesReceived() const { return messagesReceived_.load(); }
    
    /**
     * @brief Get messages sent count
     */
    uint64_t getMessagesSent() const { return messagesSent_.load(); }
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics() {
        messagesReceived_ = 0;
        messagesSent_ = 0;
    }
    
    /**
     * @brief Convert device type to string
     */
    static std::string deviceTypeToString(DeviceType type) {
        switch (type) {
            case DeviceType::USB: return "USB";
            case DeviceType::NETWORK: return "NETWORK";
            case DeviceType::BLUETOOTH: return "BLUETOOTH";
            case DeviceType::VIRTUAL: return "VIRTUAL";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief Convert device direction to string
     */
    static std::string deviceDirectionToString(DeviceDirection dir) {
        switch (dir) {
            case DeviceDirection::INPUT: return "INPUT";
            case DeviceDirection::OUTPUT: return "OUTPUT";
            case DeviceDirection::BIDIRECTIONAL: return "BIDIRECTIONAL";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief Convert device status to string
     */
    static std::string deviceStatusToString(DeviceStatus status) {
        switch (status) {
            case DeviceStatus::DISCONNECTED: return "DISCONNECTED";
            case DeviceStatus::CONNECTING: return "CONNECTING";
            case DeviceStatus::CONNECTED: return "CONNECTED";
            case DeviceStatus::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

protected:
    // ========================================================================
    // PROTECTED MEMBERS (accessible by derived classes)
    // ========================================================================
    
    /// Unique device identifier
    std::string id_;
    
    /// Device name
    std::string name_;
    
    /// Device type
    DeviceType type_;
    
    /// Communication direction
    DeviceDirection direction_;
    
    /// Connection status (atomic for thread-safety)
    std::atomic<DeviceStatus> status_;
    
    /// Messages received counter (atomic)
    std::atomic<uint64_t> messagesReceived_;
    
    /// Messages sent counter (atomic)
    std::atomic<uint64_t> messagesSent_;
};

} // namespace midiMind
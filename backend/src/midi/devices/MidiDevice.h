// ============================================================================
// File: backend/src/midi/devices/MidiDevice.h
// Version: 4.2.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
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

enum class DeviceType {
    USB,
    NETWORK,
    BLUETOOTH,
    VIRTUAL,
    UNKNOWN
};

enum class DeviceDirection {
    INPUT,
    OUTPUT,
    BIDIRECTIONAL
};

enum class DeviceStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

// ============================================================================
// CLASS: MidiDevice (Abstract Base)
// ============================================================================

/**
 * @class MidiDevice
 * @brief Abstract base class for all MIDI devices
 * 
 * Thread Safety: Methods are thread-safe unless noted otherwise.
 * Statistics (messagesReceived_, messagesSent_) use atomics.
 * 
 * Note: This class is not copyable or movable due to atomics.
 */
class MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
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
    
    virtual ~MidiDevice() = default;
    
    // Non-copyable
    MidiDevice(const MidiDevice&) = delete;
    MidiDevice& operator=(const MidiDevice&) = delete;
    
    // Non-movable (due to atomics and const members)
    MidiDevice(MidiDevice&&) = delete;
    MidiDevice& operator=(MidiDevice&&) = delete;
    
    // ========================================================================
    // PURE VIRTUAL METHODS (MUST BE IMPLEMENTED)
    // ========================================================================
    
    /**
     * @brief Connect to the device
     * @return true if connection successful
     */
    virtual bool connect() = 0;
    
    /**
     * @brief Disconnect from the device
     * @return true if disconnection successful
     */
    virtual bool disconnect() = 0;
    
    /**
     * @brief Send MIDI message to device
     * @param message Message to send
     * @return true if sent successfully
     */
    virtual bool sendMessage(const MidiMessage& message) = 0;
    
    /**
     * @brief Receive MIDI message from device
     * @return MidiMessage Received message (empty if none available)
     * @note Uses move semantics internally for efficiency
     */
    virtual MidiMessage receiveMessage() = 0;
    
    /**
     * @brief Check if device is connected
     * @return true if connected
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief Check if messages are available to read
     * @return true if messages pending
     */
    virtual bool hasMessages() const = 0;
    
    /**
     * @brief Request SysEx Identity from device
     * @return true if request sent successfully
     */
    virtual bool requestIdentity() = 0;
    
    /**
     * @brief Get device capabilities
     * @return json Capabilities (channels, polyphony, etc.)
     */
    virtual json getCapabilities() const = 0;
    
    // ========================================================================
    // VIRTUAL METHODS WITH DEFAULT IMPLEMENTATION
    // ========================================================================
    
    /**
     * @brief Get device port identifier
     * @return std::string Port identifier (empty if not applicable)
     */
    virtual std::string getPort() const {
        return "";
    }
    
    /**
     * @brief Get complete device information as JSON
     * @return json Device info including ID, name, type, status, statistics
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
     * @brief Get device ID (immutable after construction)
     */
    std::string getId() const { return id_; }
    
    /**
     * @brief Get device name (immutable after construction)
     */
    std::string getName() const { return name_; }
    
    DeviceType getType() const { return type_; }
    DeviceDirection getDirection() const { return direction_; }
    DeviceStatus getStatus() const { return status_.load(); }
    uint64_t getMessagesReceived() const { return messagesReceived_.load(); }
    uint64_t getMessagesSent() const { return messagesSent_.load(); }
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    /**
     * @brief Reset message statistics counters
     */
    void resetStatistics() {
        messagesReceived_ = 0;
        messagesSent_ = 0;
    }
    
    static std::string deviceTypeToString(DeviceType type) {
        switch (type) {
            case DeviceType::USB: return "USB";
            case DeviceType::NETWORK: return "NETWORK";
            case DeviceType::BLUETOOTH: return "BLUETOOTH";
            case DeviceType::VIRTUAL: return "VIRTUAL";
            default: return "UNKNOWN";
        }
    }
    
    static std::string deviceDirectionToString(DeviceDirection dir) {
        switch (dir) {
            case DeviceDirection::INPUT: return "INPUT";
            case DeviceDirection::OUTPUT: return "OUTPUT";
            case DeviceDirection::BIDIRECTIONAL: return "BIDIRECTIONAL";
            default: return "UNKNOWN";
        }
    }
    
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
    // Immutable after construction (thread-safe reads)
    const std::string id_;
    const std::string name_;
    const DeviceType type_;
    const DeviceDirection direction_;
    
    // Mutable state (thread-safe via atomics)
    std::atomic<DeviceStatus> status_;
    std::atomic<uint64_t> messagesReceived_;
    std::atomic<uint64_t> messagesSent_;
};

} // namespace midiMind
// ============================================================================
// File: backend/src/midi/MidiDeviceManager.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Central manager for all MIDI devices.
//   Handles discovery, connection, and lifecycle management.
//
// Features:
//   - Device discovery (USB, Network, BLE, Virtual)
//   - Connection management
//   - Hot-plug detection
//   - Device registry
//   - Observer pattern for events
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Enhanced hot-plug support
//   - Better error handling
//   - Improved statistics
//
// ============================================================================

#pragma once

#include "devices/MidiDevice.h"
#include "MidiMessage.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURE: DeviceInfo
// ============================================================================

/**
 * @struct DeviceInfo
 * @brief Information about a discovered device
 */
struct DeviceInfo {
    /// Unique device identifier
    std::string id;
    
    /// Device name
    std::string name;
    
    /// Device type
    DeviceType type;
    
    /// Communication direction
    DeviceDirection direction;
    
    /// Port/address
    std::string port;
    
    /// Manufacturer
    std::string manufacturer;
    
    /// Connection status
    bool connected;
    
    /// Additional metadata
    json metadata;
    
    /**
     * @brief Constructor
     */
    DeviceInfo()
        : type(DeviceType::UNKNOWN)
        , direction(DeviceDirection::BIDIRECTIONAL)
        , connected(false)
    {}
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const {
        return {
            {"id", id},
            {"name", name},
            {"type", MidiDevice::deviceTypeToString(type)},
            {"direction", MidiDevice::deviceDirectionToString(direction)},
            {"port", port},
            {"manufacturer", manufacturer},
            {"connected", connected},
            {"metadata", metadata}
        };
    }
    
    /**
     * @brief Create from JSON
     */
    static DeviceInfo fromJson(const json& j) {
        DeviceInfo info;
        info.id = j.value("id", "");
        info.name = j.value("name", "");
        info.port = j.value("port", "");
        info.manufacturer = j.value("manufacturer", "");
        info.connected = j.value("connected", false);
        
        if (j.contains("metadata")) {
            info.metadata = j["metadata"];
        }
        
        return info;
    }
};

// ============================================================================
// CLASS: MidiDeviceManager
// ============================================================================

/**
 * @class MidiDeviceManager
 * @brief Central manager for MIDI devices
 * 
 * Responsibilities:
 * - Discover available MIDI devices
 * - Connect/disconnect devices
 * - Monitor hot-plug events
 * - Maintain device registry
 * - Notify observers of device events
 * 
 * Thread Safety: YES (all methods are thread-safe)
 * 
 * Example:
 * ```cpp
 * MidiDeviceManager manager;
 * 
 * // Set callbacks
 * manager.setOnDeviceDiscovered([](const std::string& deviceId) {
 *     std::cout << "Device discovered: " << deviceId << "\n";
 * });
 * 
 * // Discover devices
 * auto devices = manager.discoverDevices();
 * 
 * // Connect to device
 * if (manager.connect(devices[0].id)) {
 *     auto device = manager.getDevice(devices[0].id);
 *     device->sendMessage(MidiMessage::noteOn(0, 60, 100));
 * }
 * ```
 */
class MidiDeviceManager {
public:
    // ========================================================================
    // TYPE DEFINITIONS
    // ========================================================================
    
    /// Device discovered callback
    using DeviceDiscoveredCallback = std::function<void(const std::string& deviceId)>;
    
    /// Device connected callback
    using DeviceConnectedCallback = std::function<void(const std::string& deviceId)>;
    
    /// Device disconnected callback
    using DeviceDisconnectedCallback = std::function<void(const std::string& deviceId)>;
    
    /// Message received callback
    using MessageReceivedCallback = std::function<void(const std::string& deviceId, 
                                                      const MidiMessage& message)>;
    
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     */
    MidiDeviceManager();
    
    /**
     * @brief Destructor
     */
    ~MidiDeviceManager();
    
    // Disable copy
    MidiDeviceManager(const MidiDeviceManager&) = delete;
    MidiDeviceManager& operator=(const MidiDeviceManager&) = delete;
    
    // ========================================================================
    // DEVICE DISCOVERY
    // ========================================================================
    
    /**
     * @brief Discover available MIDI devices
     * @param fullScan If true, clear cache and rescan everything
     * @return std::vector<DeviceInfo> List of discovered devices
     * @note Thread-safe
     */
    std::vector<DeviceInfo> discoverDevices(bool fullScan = false);
    
    /**
     * @brief Scan for devices (alias for discoverDevices)
     * @param fullScan If true, clear cache and rescan
     * @note Compatibility method
     */
    void scanDevices(bool fullScan = false);
    
    /**
     * @brief Get list of available devices
     * @return std::vector<DeviceInfo> Available devices
     * @note Thread-safe
     */
    std::vector<DeviceInfo> getAvailableDevices() const;
    
    /**
     * @brief Get device info by ID
     * @param deviceId Device identifier
     * @return DeviceInfo Device information
     * @throws std::runtime_error if device not found
     */
    DeviceInfo getDeviceInfo(const std::string& deviceId) const;
    
    // ========================================================================
    // CONNECTION MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Connect to a device
     * @param deviceId Device identifier
     * @return true if successful
     * @note Thread-safe
     */
    bool connect(const std::string& deviceId);
    
    /**
     * @brief Disconnect from a device
     * @param deviceId Device identifier
     * @note Thread-safe
     */
    void disconnect(const std::string& deviceId);
    
    /**
     * @brief Disconnect all devices
     * @note Thread-safe
     */
    void disconnectAll();
    
    /**
     * @brief Check if device is connected
     * @param deviceId Device identifier
     * @return true if connected
     */
    bool isConnected(const std::string& deviceId) const;
    
    /**
     * @brief Reconnect device
     * @param deviceId Device identifier
     * @return true if successful
     * @note Disconnects then reconnects after 100ms delay
     */
    bool reconnectDevice(const std::string& deviceId);
    
    // ========================================================================
    // DEVICE ACCESS
    // ========================================================================
    
    /**
     * @brief Get device by ID
     * @param deviceId Device identifier
     * @return std::shared_ptr<MidiDevice> Device or nullptr
     * @note Thread-safe
     */
    std::shared_ptr<MidiDevice> getDevice(const std::string& deviceId) const;
    
    /**
     * @brief Get all connected devices
     * @return std::vector<std::shared_ptr<MidiDevice>> Connected devices
     * @note Thread-safe
     */
    std::vector<std::shared_ptr<MidiDevice>> getConnectedDevices() const;
    
    /**
     * @brief Get devices by type
     * @param type Device type
     * @return std::vector<std::shared_ptr<MidiDevice>> Devices of type
     */
    std::vector<std::shared_ptr<MidiDevice>> getDevicesByType(DeviceType type) const;
    
    // ========================================================================
    // HOT-PLUG MONITORING
    // ========================================================================
    
    /**
     * @brief Start hot-plug monitoring
     * @note Starts background thread to monitor USB connections
     */
    void startHotPlugMonitoring();
    
    /**
     * @brief Stop hot-plug monitoring
     */
    void stopHotPlugMonitoring();
    
    /**
     * @brief Check if hot-plug monitoring is active
     */
    bool isHotPlugMonitoringActive() const { return hotPlugMonitoring_; }
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Set device discovered callback
     */
    void setOnDeviceDiscovered(DeviceDiscoveredCallback callback) {
        onDeviceDiscovered_ = callback;
    }
    
    /**
     * @brief Set device connected callback
     */
    void setOnDeviceConnected(DeviceConnectedCallback callback) {
        onDeviceConnected_ = callback;
    }
    
    /**
     * @brief Set device disconnected callback
     */
    void setOnDeviceDisconnected(DeviceDisconnectedCallback callback) {
        onDeviceDisconnected_ = callback;
    }
    
    /**
     * @brief Set message received callback
     */
    void setOnMessageReceived(MessageReceivedCallback callback) {
        onMessageReceived_ = callback;
    }
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get statistics
     * @return json Statistics data
     */
    json getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics();
    
private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Scan USB devices
     */
    void scanUSBDevices();
    
    /**
     * @brief Scan virtual devices
     */
    void scanVirtualDevices();
    
    /**
     * @brief Scan network devices
     */
    void scanNetworkDevices();
    
    /**
     * @brief Scan Bluetooth devices
     */
    void scanBluetoothDevices();
    
    /**
     * @brief Create device instance from info
     */
    std::shared_ptr<MidiDevice> createDevice(const DeviceInfo& info);
    
    /**
     * @brief Hot-plug monitoring thread function
     */
    void hotPlugMonitorThread();
    
    /**
     * @brief Handle device connection
     */
    void handleDeviceConnected(const std::string& deviceId);
    
    /**
     * @brief Handle device disconnection
     */
    void handleDeviceDisconnected(const std::string& deviceId);
    
    /**
     * @brief Handle message received
     */
    void handleMessageReceived(const std::string& deviceId, const MidiMessage& message);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Available devices (discovered but not necessarily connected)
    std::vector<DeviceInfo> availableDevices_;
    
    /// Connected devices (deviceId -> device)
    std::unordered_map<std::string, std::shared_ptr<MidiDevice>> connectedDevices_;
    
    /// Mutex for thread-safety
    mutable std::mutex devicesMutex_;
    
    /// Hot-plug monitoring flag
    std::atomic<bool> hotPlugMonitoring_;
    
    /// Hot-plug monitoring thread
    std::thread hotPlugThread_;
    
    /// Callbacks
    DeviceDiscoveredCallback onDeviceDiscovered_;
    DeviceConnectedCallback onDeviceConnected_;
    DeviceDisconnectedCallback onDeviceDisconnected_;
    MessageReceivedCallback onMessageReceived_;
    
    /// Statistics
    struct {
        uint64_t devicesDiscovered;
        uint64_t connectionsSucceeded;
        uint64_t connectionsFailed;
        uint64_t messagesReceived;
    } stats_;
};

} // namespace midiMind
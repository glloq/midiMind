// ============================================================================
// File: backend/src/midi/devices/MidiDeviceManager.h
// Version: 4.2.1 - BLE MIDI SUPPORT + Pairing Management
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.1:
//   - Added BLE pairing/unpairing methods
//   - Added scanBleDevices() with filter support
//   - Added getBleDeviceSignal() for RSSI
//   - Added getPairedBleDevices() listing
//
// ============================================================================

#pragma once

#include "MidiDevice.h"
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

namespace midiMind {

// Forward declarations
class EventBus;

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct MidiDeviceInfo
 * @brief MIDI device information
 */
struct MidiDeviceInfo {
    std::string id;
    std::string name;
    DeviceType type;
    DeviceDirection direction;
    DeviceStatus status;
    std::string port;
    bool available;
    
    // Metadata
    std::string manufacturer;
    std::string model;
    std::string version;
    
    // Bluetooth-specific (for BLE devices)
    std::string bluetoothAddress;
    std::string objectPath;
    bool paired = false;
    int signalStrength = 0;  // RSSI
    
    // Statistics
    uint64_t messagesReceived;
    uint64_t messagesSent;
};

// ============================================================================
// CLASS: MidiDeviceManager
// ============================================================================

/**
 * @class MidiDeviceManager
 * @brief Manages MIDI devices (discovery, connection, hot-plug)
 * 
 * Thread Safety: Methods are thread-safe. Callbacks are invoked without
 * holding internal locks to prevent deadlocks.
 */
class MidiDeviceManager {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param eventBus Optional EventBus for publishing events
     */
    explicit MidiDeviceManager(std::shared_ptr<EventBus> eventBus = nullptr);
    
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
     * @param fullScan If true, perform full rescan
     * @return std::vector<MidiDeviceInfo> List of devices
     */
    std::vector<MidiDeviceInfo> discoverDevices(bool fullScan = false);
    
    /**
     * @brief Get list of available devices
     * @return std::vector<MidiDeviceInfo> Device list
     */
    std::vector<MidiDeviceInfo> getAvailableDevices() const;
    
    /**
     * @brief Get device count
     * @return int Number of devices
     */
    int getDeviceCount() const;
    
    // ========================================================================
    // BLE CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Enable/disable BLE MIDI scanning
     * @param enable true to enable BLE scanning
     */
    void setBluetoothEnabled(bool enable);
    
    /**
     * @brief Check if BLE scanning is enabled
     * @return bool true if enabled
     */
    bool isBluetoothEnabled() const;
    
    /**
     * @brief Set BLE scan timeout
     * @param seconds Scan duration (1-30 seconds)
     */
    void setBluetoothScanTimeout(int seconds);
    
    // ========================================================================
    // BLE OPERATIONS
    // ========================================================================
    
    /**
     * @brief Scan for BLE MIDI devices
     * @param duration Scan duration in seconds (1-30)
     * @param nameFilter Optional name filter (empty = all devices)
     * @return std::vector<MidiDeviceInfo> Found devices
     */
    std::vector<MidiDeviceInfo> scanBleDevices(int duration = 5, 
                                                const std::string& nameFilter = "");
    
    /**
     * @brief Pair with BLE device
     * @param address Bluetooth MAC address
     * @param pin Optional PIN code (empty if no PIN required)
     * @return bool true if pairing successful
     */
    bool pairBleDevice(const std::string& address, const std::string& pin = "");
    
    /**
     * @brief Unpair BLE device
     * @param address Bluetooth MAC address
     * @return bool true if unpairing successful
     */
    bool unpairBleDevice(const std::string& address);
    
    /**
     * @brief Get list of paired BLE devices
     * @return std::vector<MidiDeviceInfo> Paired devices
     */
    std::vector<MidiDeviceInfo> getPairedBleDevices() const;
    
    /**
     * @brief Forget BLE device (unpair + remove from cache)
     * @param address Bluetooth MAC address
     * @return bool true if successful
     */
    bool forgetBleDevice(const std::string& address);
    
    /**
     * @brief Get signal strength for connected BLE device
     * @param deviceId Device ID
     * @return int RSSI value (or 0 if not available)
     */
    int getBleDeviceSignal(const std::string& deviceId) const;
    
    // ========================================================================
    // CONNECTION MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Connect to device
     * @param deviceId Device ID
     * @return bool true if connected
     */
    bool connect(const std::string& deviceId);
    
    /**
     * @brief Disconnect device
     * @param deviceId Device ID
     */
    void disconnect(const std::string& deviceId);
    
    /**
     * @brief Disconnect all devices
     */
    void disconnectAll();
    
    /**
     * @brief Check if device is connected
     * @param deviceId Device ID
     * @return bool true if connected
     */
    bool isConnected(const std::string& deviceId) const;
    
    // ========================================================================
    // DEVICE ACCESS
    // ========================================================================
    
    /**
     * @brief Get device by ID
     * @param deviceId Device ID
     * @return std::shared_ptr<MidiDevice> Device or nullptr
     */
    std::shared_ptr<MidiDevice> getDevice(const std::string& deviceId);
    
    /**
     * @brief Get all connected devices
     * @return std::vector<std::shared_ptr<MidiDevice>> Connected devices
     */
    std::vector<std::shared_ptr<MidiDevice>> getConnectedDevices();
    
    // ========================================================================
    // HOT-PLUG MONITORING
    // ========================================================================
    
    /**
     * @brief Start hot-plug monitoring
     * @param intervalMs Scan interval in milliseconds (default 2000)
     */
    void startHotPlugMonitoring(int intervalMs = 2000);
    
    /**
     * @brief Stop hot-plug monitoring
     */
    void stopHotPlugMonitoring();
    
    /**
     * @brief Check if hot-plug monitoring is active
     * @return bool true if monitoring
     */
    bool isHotPlugMonitoringActive() const;
    
    /**
     * @brief Set hot-plug callback
     * @param onConnect Called when device connected
     * @param onDisconnect Called when device disconnected
     */
    void setHotPlugCallbacks(
        std::function<void(const std::string&)> onConnect,
        std::function<void(const std::string&)> onDisconnect
    );
    
    // ========================================================================
    // EVENTBUS
    // ========================================================================
    
    /**
     * @brief Set EventBus for publishing events
     * @param eventBus EventBus instance
     */
    void setEventBus(std::shared_ptr<EventBus> eventBus);

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Hot-plug monitoring thread
     */
    void hotPlugThread();
    
    /**
     * @brief Discover USB MIDI devices (ALSA)
     */
    std::vector<MidiDeviceInfo> discoverUsbDevices();
    
    /**
     * @brief Discover Bluetooth LE MIDI devices (BlueZ)
     */
    std::vector<MidiDeviceInfo> discoverBluetoothDevices();
    
    /**
     * @brief Create device instance
     */
    std::shared_ptr<MidiDevice> createDevice(const MidiDeviceInfo& info);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Connected devices
    std::vector<std::shared_ptr<MidiDevice>> devices_;
    
    /// Available devices (last scan)
    std::vector<MidiDeviceInfo> availableDevices_;
    
    /// Thread safety for devices and discovery
    mutable std::mutex mutex_;
    
    /// Hot-plug monitoring
    std::thread hotPlugThread_;
    std::atomic<bool> hotPlugRunning_{false};
    std::atomic<int> scanIntervalMs_{2000};
    
    /// Callbacks (protected by separate mutex to avoid deadlock)
    std::mutex callbackMutex_;
    std::function<void(const std::string&)> onDeviceConnect_;
    std::function<void(const std::string&)> onDeviceDisconnect_;
    
    /// Bluetooth configuration
    std::atomic<bool> bluetoothEnabled_{true};
    std::atomic<int> bluetoothScanTimeout_{5};
    
    /// EventBus for publishing events
    std::shared_ptr<EventBus> eventBus_;

    /// SysEx handler for device identification
    std::shared_ptr<class SysExHandler> sysexHandler_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE MidiDeviceManager.h
// ============================================================================
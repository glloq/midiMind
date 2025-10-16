// ============================================================================
// File: backend/src/midi/devices/MidiDeviceManager.h
// Version: 4.1.0 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.0:
//   - Fixed include path: "MidiDevice.h" (not "devices/MidiDevice.h")
//
// ============================================================================

#pragma once

#include "MidiDevice.h"  // ✅ CORRIGÉ (était "devices/MidiDevice.h")
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

namespace midiMind {

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
 */
class MidiDeviceManager {
public:
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
    
    /// Thread safety
    mutable std::mutex mutex_;
    
    /// Hot-plug monitoring
    std::thread hotPlugThread_;
    std::atomic<bool> hotPlugRunning_{false};
    int scanIntervalMs_{2000};
    
    /// Callbacks
    std::function<void(const std::string&)> onDeviceConnect_;
    std::function<void(const std::string&)> onDeviceDisconnect_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE MidiDeviceManager.h
// ============================================================================
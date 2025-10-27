// ============================================================================
// File: backend/src/midi/devices/BleMidiDevice.h
// Version: 2.0.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#pragma once

#include "MidiDevice.h"
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>

// Forward declarations pour BlueZ (via GIO)
struct _GDBusConnection;
typedef struct _GDBusConnection GDBusConnection;

namespace midiMind {

/**
 * @struct BleDeviceInfo
 * @brief Information about a discovered BLE device
 */
struct BleDeviceInfo {
    std::string address;      ///< MAC address (AA:BB:CC:DD:EE:FF)
    std::string name;         ///< Device name
    std::string objectPath;   ///< BlueZ D-Bus object path
    int rssi;                 ///< Signal strength
    bool paired;              ///< Pairing status
    bool connected;           ///< Connection status
};

/**
 * @class BleMidiDevice
 * @brief BLE MIDI device implementation using BlueZ
 * 
 * Thread Safety: All methods are thread-safe.
 */
class BleMidiDevice : public MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    BleMidiDevice(const std::string& id,
                  const std::string& name,
                  const std::string& address,
                  const std::string& objectPath);
    
    virtual ~BleMidiDevice();
    
    // ========================================================================
    // MIDIDEVICE INTERFACE IMPLEMENTATION
    // ========================================================================
    
    bool connect() override;
    bool disconnect() override;
    bool sendMessage(const MidiMessage& message) override;
    MidiMessage receiveMessage() override;
    bool isConnected() const override;
    bool hasMessages() const override;
    bool requestIdentity() override;
    json getCapabilities() const override;
    std::string getPort() const override;
    json getInfo() const override;
    
    // ========================================================================
    // BLE DISCOVERY & PAIRING (STATIC)
    // ========================================================================
    
    /**
     * @brief Scan for BLE MIDI devices
     * @param durationSeconds Scan duration in seconds
     * @param filter Optional name filter
     * @return List of discovered devices
     */
    static std::vector<BleDeviceInfo> scanDevices(int durationSeconds = 5,
                                                   const std::string& filter = "");
    
    /**
     * @brief Get list of paired BLE devices
     * @return List of paired devices
     */
    static std::vector<BleDeviceInfo> getPairedDevices();
    
    // ========================================================================
    // BLE PAIRING (INSTANCE)
    // ========================================================================
    
    /**
     * @brief Pair with this device
     * @param pin Optional PIN code for pairing
     * @return true if successful
     */
    bool pairDevice(const std::string& pin = "");
    
    /**
     * @brief Unpair from this device
     * @return true if successful
     */
    bool unpairDevice();
    
    /**
     * @brief Forget device (unpair + remove from cache)
     * @return true if successful
     */
    bool forgetDevice();
    
    /**
     * @brief Check if device is paired
     */
    bool isPaired() const;
    
    // ========================================================================
    // BLE-SPECIFIC METHODS
    // ========================================================================
    
    std::string getBluetoothAddress() const { return address_; }
    std::string getObjectPath() const { return objectPath_; }
    
    /**
     * @brief Get signal strength
     * @return RSSI value (negative, closer to 0 = stronger)
     */
    int getRssi() const { return rssi_.load(); }
    
    /**
     * @brief Get detailed signal information
     */
    json getSignalStrength() const;

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    bool connectToBluez();
    bool connectCharacteristic();
    void disconnectCharacteristic();
    void readThread();
    MidiMessage parseBlePacket(const uint8_t* data, size_t len);
    std::vector<uint8_t> encodeBlePacket(const MidiMessage& msg);
    void updateRssi();
    
    /**
     * @brief Helper: Get D-Bus connection
     */
    static GDBusConnection* getDbusConnection();
    
    /**
     * @brief Helper: Check if device has BLE MIDI service
     */
    static bool hasMidiService(GDBusConnection* conn, const std::string& objectPath);
    
    // ========================================================================
    // CONSTANTS
    // ========================================================================
    
    static constexpr const char* BLE_MIDI_SERVICE_UUID = 
        "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
    
    static constexpr const char* BLE_MIDI_CHARACTERISTIC_UUID = 
        "7772e5db-3868-4112-a1a9-f2669d106bf3";
    
    static constexpr const char* BLUEZ_SERVICE = "org.bluez";
    static constexpr const char* DEVICE_INTERFACE = "org.bluez.Device1";
    static constexpr const char* ADAPTER_INTERFACE = "org.bluez.Adapter1";
    static constexpr const char* GATT_CHARACTERISTIC_INTERFACE = 
        "org.bluez.GattCharacteristic1";
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    const std::string address_;
    const std::string objectPath_;
    
    GDBusConnection* dbusConnection_;
    std::string characteristicPath_;
    
    std::atomic<bool> connected_;
    std::atomic<bool> paired_;
    
    std::queue<MidiMessage> messageQueue_;
    mutable std::mutex queueMutex_;
    
    std::thread readThread_;
    std::atomic<bool> readThreadRunning_;
    
    std::atomic<int> rssi_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE BleMidiDevice.h
// ============================================================================
// ============================================================================
// File: backend/src/midi/devices/BleMidiDevice.cpp
// Version: 2.0.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#include "BleMidiDevice.h"
#include "../../core/Logger.h"
#include <gio/gio.h>
#include <algorithm>
#include <cstring>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

BleMidiDevice::BleMidiDevice(const std::string& id,
                             const std::string& name,
                             const std::string& address,
                             const std::string& objectPath)
    : MidiDevice(id, name, DeviceType::BLUETOOTH, DeviceDirection::BIDIRECTIONAL)
    , address_(address)
    , objectPath_(objectPath)
    , dbusConnection_(nullptr)
    , connected_(false)
    , paired_(false)
    , readThreadRunning_(false)
    , rssi_(-100)
{
    Logger::info("BleMidiDevice", "Created device: " + name + " (" + address + ")");
}

BleMidiDevice::~BleMidiDevice() {
    if (connected_.load()) {
        disconnect();
    }
}

// ============================================================================
// STATIC DISCOVERY & PAIRING
// ============================================================================

std::vector<BleDeviceInfo> BleMidiDevice::scanDevices(int durationSeconds,
                                                      const std::string& filter) {
    Logger::info("BleMidiDevice", "Scanning for BLE devices (" + 
                std::to_string(durationSeconds) + "s)...");
    
    std::vector<BleDeviceInfo> devices;
    
    GDBusConnection* conn = getDbusConnection();
    if (!conn) {
        Logger::error("BleMidiDevice", "Failed to get D-Bus connection");
        return devices;
    }
    
    GError* error = nullptr;
    
    // Get adapter
    GDBusProxy* adapterProxy = g_dbus_proxy_new_sync(
        conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        "/org/bluez/hci0",
        ADAPTER_INTERFACE,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiDevice", "Failed to get adapter: " + 
                     std::string(error->message));
        g_error_free(error);
        g_object_unref(conn);
        return devices;
    }
    
    // Start discovery
    GVariant* result = g_dbus_proxy_call_sync(
        adapterProxy,
        "StartDiscovery",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiDevice", "Failed to start discovery: " + 
                     std::string(error->message));
        g_error_free(error);
        g_object_unref(adapterProxy);
        g_object_unref(conn);
        return devices;
    }
    
    if (result) {
        g_variant_unref(result);
    }
    
    // Wait for scan duration
    std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
    
    // Stop discovery
    result = g_dbus_proxy_call_sync(
        adapterProxy,
        "StopDiscovery",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        nullptr
    );
    
    if (result) {
        g_variant_unref(result);
    }
    
    // Get managed objects to find discovered devices
    GDBusProxy* objectManagerProxy = g_dbus_proxy_new_sync(
        conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        g_object_unref(adapterProxy);
        g_object_unref(conn);
        return devices;
    }
    
    result = g_dbus_proxy_call_sync(
        objectManagerProxy,
        "GetManagedObjects",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        g_object_unref(objectManagerProxy);
        g_object_unref(adapterProxy);
        g_object_unref(conn);
        return devices;
    }
    
    if (result) {
        GVariantIter* objectsIter;
        g_variant_get(result, "(a{oa{sa{sv}}})", &objectsIter);
        
        const gchar* objectPath;
        GVariant* interfaces;
        
        while (g_variant_iter_loop(objectsIter, "{&o@a{sa{sv}}}", &objectPath, &interfaces)) {
            GVariantIter* interfacesIter;
            g_variant_get(interfaces, "a{sa{sv}}", &interfacesIter);
            
            const gchar* interfaceName;
            GVariant* properties;
            
            while (g_variant_iter_loop(interfacesIter, "{&s@a{sv}}", &interfaceName, &properties)) {
                if (std::string(interfaceName) == DEVICE_INTERFACE) {
                    // Check if device has MIDI service
                    if (!hasMidiService(conn, objectPath)) {
                        continue;
                    }
                    
                    BleDeviceInfo info;
                    info.objectPath = objectPath;
                    
                    GVariant* nameVar = g_variant_lookup_value(properties, "Name", G_VARIANT_TYPE_STRING);
                    if (nameVar) {
                        info.name = g_variant_get_string(nameVar, nullptr);
                        g_variant_unref(nameVar);
                    }
                    
                    GVariant* addressVar = g_variant_lookup_value(properties, "Address", G_VARIANT_TYPE_STRING);
                    if (addressVar) {
                        info.address = g_variant_get_string(addressVar, nullptr);
                        g_variant_unref(addressVar);
                    }
                    
                    GVariant* rssiVar = g_variant_lookup_value(properties, "RSSI", G_VARIANT_TYPE_INT16);
                    if (rssiVar) {
                        info.rssi = g_variant_get_int16(rssiVar);
                        g_variant_unref(rssiVar);
                    } else {
                        info.rssi = -100;
                    }
                    
                    GVariant* pairedVar = g_variant_lookup_value(properties, "Paired", G_VARIANT_TYPE_BOOLEAN);
                    if (pairedVar) {
                        info.paired = g_variant_get_boolean(pairedVar);
                        g_variant_unref(pairedVar);
                    } else {
                        info.paired = false;
                    }
                    
                    GVariant* connectedVar = g_variant_lookup_value(properties, "Connected", G_VARIANT_TYPE_BOOLEAN);
                    if (connectedVar) {
                        info.connected = g_variant_get_boolean(connectedVar);
                        g_variant_unref(connectedVar);
                    } else {
                        info.connected = false;
                    }
                    
                    // Apply filter if specified
                    if (!filter.empty()) {
                        if (info.name.find(filter) == std::string::npos) {
                            continue;
                        }
                    }
                    
                    devices.push_back(info);
                }
            }
            
            g_variant_iter_free(interfacesIter);
        }
        
        g_variant_iter_free(objectsIter);
        g_variant_unref(result);
    }
    
    g_object_unref(objectManagerProxy);
    g_object_unref(adapterProxy);
    g_object_unref(conn);
    
    Logger::info("BleMidiDevice", "Found " + std::to_string(devices.size()) + 
                " BLE MIDI devices");
    
    return devices;
}

std::vector<BleDeviceInfo> BleMidiDevice::getPairedDevices() {
    Logger::info("BleMidiDevice", "Getting paired BLE devices...");
    
    std::vector<BleDeviceInfo> devices;
    
    GDBusConnection* conn = getDbusConnection();
    if (!conn) {
        Logger::error("BleMidiDevice", "Failed to get D-Bus connection");
        return devices;
    }
    
    GError* error = nullptr;
    
    GDBusProxy* objectManagerProxy = g_dbus_proxy_new_sync(
        conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        g_object_unref(conn);
        return devices;
    }
    
    GVariant* result = g_dbus_proxy_call_sync(
        objectManagerProxy,
        "GetManagedObjects",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        g_object_unref(objectManagerProxy);
        g_object_unref(conn);
        return devices;
    }
    
    if (result) {
        GVariantIter* objectsIter;
        g_variant_get(result, "(a{oa{sa{sv}}})", &objectsIter);
        
        const gchar* objectPath;
        GVariant* interfaces;
        
        while (g_variant_iter_loop(objectsIter, "{&o@a{sa{sv}}}", &objectPath, &interfaces)) {
            GVariantIter* interfacesIter;
            g_variant_get(interfaces, "a{sa{sv}}", &interfacesIter);
            
            const gchar* interfaceName;
            GVariant* properties;
            
            while (g_variant_iter_loop(interfacesIter, "{&s@a{sv}}", &interfaceName, &properties)) {
                if (std::string(interfaceName) == DEVICE_INTERFACE) {
                    GVariant* pairedVar = g_variant_lookup_value(properties, "Paired", G_VARIANT_TYPE_BOOLEAN);
                    bool isPaired = false;
                    
                    if (pairedVar) {
                        isPaired = g_variant_get_boolean(pairedVar);
                        g_variant_unref(pairedVar);
                    }
                    
                    if (!isPaired) {
                        continue;
                    }
                    
                    // Check if device has MIDI service
                    if (!hasMidiService(conn, objectPath)) {
                        continue;
                    }
                    
                    BleDeviceInfo info;
                    info.objectPath = objectPath;
                    info.paired = true;
                    
                    GVariant* nameVar = g_variant_lookup_value(properties, "Name", G_VARIANT_TYPE_STRING);
                    if (nameVar) {
                        info.name = g_variant_get_string(nameVar, nullptr);
                        g_variant_unref(nameVar);
                    }
                    
                    GVariant* addressVar = g_variant_lookup_value(properties, "Address", G_VARIANT_TYPE_STRING);
                    if (addressVar) {
                        info.address = g_variant_get_string(addressVar, nullptr);
                        g_variant_unref(addressVar);
                    }
                    
                    GVariant* rssiVar = g_variant_lookup_value(properties, "RSSI", G_VARIANT_TYPE_INT16);
                    if (rssiVar) {
                        info.rssi = g_variant_get_int16(rssiVar);
                        g_variant_unref(rssiVar);
                    } else {
                        info.rssi = -100;
                    }
                    
                    GVariant* connectedVar = g_variant_lookup_value(properties, "Connected", G_VARIANT_TYPE_BOOLEAN);
                    if (connectedVar) {
                        info.connected = g_variant_get_boolean(connectedVar);
                        g_variant_unref(connectedVar);
                    } else {
                        info.connected = false;
                    }
                    
                    devices.push_back(info);
                }
            }
            
            g_variant_iter_free(interfacesIter);
        }
        
        g_variant_iter_free(objectsIter);
        g_variant_unref(result);
    }
    
    g_object_unref(objectManagerProxy);
    g_object_unref(conn);
    
    Logger::info("BleMidiDevice", "Found " + std::to_string(devices.size()) + 
                " paired BLE MIDI devices");
    
    return devices;
}

// ============================================================================
// INSTANCE PAIRING
// ============================================================================

bool BleMidiDevice::pairDevice(const std::string& pin) {
    Logger::info("BleMidiDevice", "Pairing device: " + name_);
    
    if (paired_.load()) {
        Logger::warning("BleMidiDevice", "Device already paired: " + name_);
        return true;
    }
    
    GDBusConnection* conn = getDbusConnection();
    if (!conn) {
        Logger::error("BleMidiDevice", "Failed to get D-Bus connection");
        return false;
    }
    
    GError* error = nullptr;
    
    GDBusProxy* deviceProxy = g_dbus_proxy_new_sync(
        conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        objectPath_.c_str(),
        DEVICE_INTERFACE,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiDevice", "Failed to create device proxy: " + 
                     std::string(error->message));
        g_error_free(error);
        g_object_unref(conn);
        return false;
    }
    
    // Call Pair method
    GVariant* result = g_dbus_proxy_call_sync(
        deviceProxy,
        "Pair",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        30000,  // 30 second timeout
        nullptr,
        &error
    );
    
    g_object_unref(deviceProxy);
    g_object_unref(conn);
    
    if (error) {
        Logger::error("BleMidiDevice", "Failed to pair: " + 
                     std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    if (result) {
        g_variant_unref(result);
    }
    
    paired_ = true;
    Logger::info("BleMidiDevice", "✓ Device paired: " + name_);
    
    return true;
}

bool BleMidiDevice::unpairDevice() {
    Logger::info("BleMidiDevice", "Unpairing device: " + name_);
    
    if (!paired_.load()) {
        Logger::warning("BleMidiDevice", "Device not paired: " + name_);
        return true;
    }
    
    // Disconnect first if connected
    if (connected_.load()) {
        disconnect();
    }
    
    GDBusConnection* conn = getDbusConnection();
    if (!conn) {
        Logger::error("BleMidiDevice", "Failed to get D-Bus connection");
        return false;
    }
    
    GError* error = nullptr;
    
    // Get adapter
    GDBusProxy* adapterProxy = g_dbus_proxy_new_sync(
        conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        "/org/bluez/hci0",
        ADAPTER_INTERFACE,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiDevice", "Failed to get adapter: " + 
                     std::string(error->message));
        g_error_free(error);
        g_object_unref(conn);
        return false;
    }
    
    // Call RemoveDevice
    GVariant* result = g_dbus_proxy_call_sync(
        adapterProxy,
        "RemoveDevice",
        g_variant_new("(o)", objectPath_.c_str()),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(adapterProxy);
    g_object_unref(conn);
    
    if (error) {
        Logger::error("BleMidiDevice", "Failed to unpair: " + 
                     std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    if (result) {
        g_variant_unref(result);
    }
    
    paired_ = false;
    Logger::info("BleMidiDevice", "✓ Device unpaired: " + name_);
    
    return true;
}

bool BleMidiDevice::forgetDevice() {
    Logger::info("BleMidiDevice", "Forgetting device: " + name_);
    
    // Unpair will also remove from BlueZ cache
    return unpairDevice();
}

bool BleMidiDevice::isPaired() const {
    return paired_.load();
}

json BleMidiDevice::getSignalStrength() const {
    int rssi = rssi_.load();
    
    // Calculate signal quality percentage
    // RSSI typically ranges from -100 (weak) to -30 (strong)
    int quality = 0;
    if (rssi >= -50) {
        quality = 100;
    } else if (rssi >= -60) {
        quality = 80;
    } else if (rssi >= -70) {
        quality = 60;
    } else if (rssi >= -80) {
        quality = 40;
    } else if (rssi >= -90) {
        quality = 20;
    }
    
    // Estimate distance (very rough approximation)
    double estimatedDistance = std::pow(10.0, (-50.0 - rssi) / 20.0);
    
    return json{
        {"rssi", rssi},
        {"quality_percent", quality},
        {"estimated_distance_m", std::round(estimatedDistance * 10) / 10.0},
        {"status", quality >= 60 ? "good" : quality >= 40 ? "fair" : "poor"}
    };
}

// ============================================================================
// CONNECTION
// ============================================================================

bool BleMidiDevice::connect() {
    if (connected_.load()) {
        Logger::warning("BleMidiDevice", "Already connected: " + name_);
        return true;
    }
    
    Logger::info("BleMidiDevice", "Connecting to: " + name_ + " (" + address_ + ")");
    status_ = DeviceStatus::CONNECTING;
    
    if (!connectToBluez()) {
        Logger::error("BleMidiDevice", "Failed to connect to BlueZ");
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    GError* error = nullptr;
    GDBusProxy* deviceProxy = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        objectPath_.c_str(),
        DEVICE_INTERFACE,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiDevice", 
            "Failed to create device proxy: " + std::string(error->message));
        g_error_free(error);
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    GVariant* result = g_dbus_proxy_call_sync(
        deviceProxy,
        "Connect",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        30000,
        nullptr,
        &error
    );
    
    g_object_unref(deviceProxy);
    
    if (error) {
        Logger::error("BleMidiDevice", 
            "Failed to connect device: " + std::string(error->message));
        g_error_free(error);
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    if (result) {
        g_variant_unref(result);
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    if (!connectCharacteristic()) {
        Logger::error("BleMidiDevice", "Failed to connect GATT characteristic");
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    connected_ = true;
    readThreadRunning_ = true;
    readThread_ = std::thread(&BleMidiDevice::readThread, this);
    
    status_ = DeviceStatus::CONNECTED;
    Logger::info("BleMidiDevice", "✓ Connected: " + name_);
    
    return true;
}

bool BleMidiDevice::disconnect() {
    if (!connected_.load()) {
        return true;
    }
    
    Logger::info("BleMidiDevice", "Disconnecting: " + name_);
    
    readThreadRunning_ = false;
    if (readThread_.joinable()) {
        readThread_.join();
    }
    
    disconnectCharacteristic();
    
    if (dbusConnection_) {
        GError* error = nullptr;
        GDBusProxy* deviceProxy = g_dbus_proxy_new_sync(
            dbusConnection_,
            G_DBUS_PROXY_FLAGS_NONE,
            nullptr,
            BLUEZ_SERVICE,
            objectPath_.c_str(),
            DEVICE_INTERFACE,
            nullptr,
            &error
        );
        
        if (!error && deviceProxy) {
            g_dbus_proxy_call_sync(
                deviceProxy,
                "Disconnect",
                nullptr,
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                nullptr,
                nullptr
            );
            g_object_unref(deviceProxy);
        }
        
        if (error) {
            g_error_free(error);
        }
        
        g_object_unref(dbusConnection_);
        dbusConnection_ = nullptr;
    }
    
    connected_ = false;
    status_ = DeviceStatus::DISCONNECTED;
    
    Logger::info("BleMidiDevice", "✓ Disconnected: " + name_);
    
    return true;
}

bool BleMidiDevice::isConnected() const {
    return connected_.load();
}

// ============================================================================
// MESSAGE HANDLING
// ============================================================================

bool BleMidiDevice::sendMessage(const MidiMessage& message) {
    if (!connected_.load()) {
        return false;
    }
    
    if (!dbusConnection_ || characteristicPath_.empty()) {
        return false;
    }
    
    auto packet = encodeBlePacket(message);
    
    if (packet.empty()) {
        return false;
    }
    
    GError* error = nullptr;
    GDBusProxy* charProxy = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        characteristicPath_.c_str(),
        GATT_CHARACTERISTIC_INTERFACE,
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
    for (uint8_t byte : packet) {
        g_variant_builder_add(&builder, "y", byte);
    }
    GVariant* value = g_variant_builder_end(&builder);
    
    GVariant* result = g_dbus_proxy_call_sync(
        charProxy,
        "WriteValue",
        g_variant_new("(@aya{sv})", value, nullptr),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(charProxy);
    
    if (error) {
        Logger::error("BleMidiDevice", 
            "Failed to write: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    if (result) {
        g_variant_unref(result);
    }
    
    messagesSent_++;
    return true;
}

MidiMessage BleMidiDevice::receiveMessage() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    
    if (messageQueue_.empty()) {
        return MidiMessage();
    }
    
    MidiMessage msg = std::move(messageQueue_.front());
    messageQueue_.pop();
    
    return msg;
}

bool BleMidiDevice::hasMessages() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return !messageQueue_.empty();
}

bool BleMidiDevice::requestIdentity() {
    std::vector<uint8_t> sysex = {0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7};
    MidiMessage identityRequest(sysex);
    
    return sendMessage(identityRequest);
}

json BleMidiDevice::getCapabilities() const {
    return json{
        {"protocol", "BLE_MIDI"},
        {"bidirectional", true},
        {"sysex", true},
        {"realtime", true}
    };
}

std::string BleMidiDevice::getPort() const {
    return address_;
}

json BleMidiDevice::getInfo() const {
    return json{
        {"id", id_},
        {"name", name_},
        {"type", "bluetooth"},
        {"address", address_},
        {"object_path", objectPath_},
        {"connected", connected_.load()},
        {"paired", paired_.load()},
        {"rssi", rssi_.load()},
        {"messages_sent", messagesSent_.load()},
        {"messages_received", messagesReceived_.load()}
    };
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

bool BleMidiDevice::connectToBluez() {
    GError* error = nullptr;
    dbusConnection_ = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    
    if (error) {
        Logger::error("BleMidiDevice", 
            "Failed to connect to system bus: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    return true;
}

bool BleMidiDevice::connectCharacteristic() {
    GError* error = nullptr;
    
    GDBusProxy* objectManagerProxy = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    GVariant* result = g_dbus_proxy_call_sync(
        objectManagerProxy,
        "GetManagedObjects",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(objectManagerProxy);
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    bool found = false;
    
    if (result) {
        GVariantIter* objectsIter;
        g_variant_get(result, "(a{oa{sa{sv}}})", &objectsIter);
        
        const gchar* objectPath;
        GVariant* interfaces;
        
        while (g_variant_iter_loop(objectsIter, "{&o@a{sa{sv}}}", &objectPath, &interfaces)) {
            if (std::string(objectPath).find(objectPath_) == std::string::npos) {
                continue;
            }
            
            GVariantIter* interfacesIter;
            g_variant_get(interfaces, "a{sa{sv}}", &interfacesIter);
            
            const gchar* interfaceName;
            GVariant* properties;
            
            while (g_variant_iter_loop(interfacesIter, "{&s@a{sv}}", &interfaceName, &properties)) {
                if (std::string(interfaceName) == GATT_CHARACTERISTIC_INTERFACE) {
                    GVariant* uuidVar = g_variant_lookup_value(properties, "UUID", G_VARIANT_TYPE_STRING);
                    
                    if (uuidVar) {
                        const gchar* uuid = g_variant_get_string(uuidVar, nullptr);
                        
                        if (std::string(uuid) == BLE_MIDI_CHARACTERISTIC_UUID) {
                            characteristicPath_ = objectPath;
                            found = true;
                            g_variant_unref(uuidVar);
                            break;
                        }
                        
                        g_variant_unref(uuidVar);
                    }
                }
            }
            
            g_variant_iter_free(interfacesIter);
            
            if (found) break;
        }
        
        g_variant_iter_free(objectsIter);
        g_variant_unref(result);
    }
    
    if (!found) {
        return false;
    }
    
    GDBusProxy* charProxy = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        characteristicPath_.c_str(),
        GATT_CHARACTERISTIC_INTERFACE,
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    result = g_dbus_proxy_call_sync(
        charProxy,
        "StartNotify",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(charProxy);
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    if (result) {
        g_variant_unref(result);
    }
    
    return true;
}

void BleMidiDevice::disconnectCharacteristic() {
    if (characteristicPath_.empty() || !dbusConnection_) {
        return;
    }
    
    GError* error = nullptr;
    GDBusProxy* charProxy = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        characteristicPath_.c_str(),
        GATT_CHARACTERISTIC_INTERFACE,
        nullptr,
        &error
    );
    
    if (!error && charProxy) {
        g_dbus_proxy_call_sync(
            charProxy,
            "StopNotify",
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            nullptr
        );
        g_object_unref(charProxy);
    }
    
    characteristicPath_.clear();
}

void BleMidiDevice::readThread() {
    Logger::info("BleMidiDevice", "Read thread started for: " + name_);
    
    guint subscriptionId = 0;
    
    if (!characteristicPath_.empty()) {
        subscriptionId = g_dbus_connection_signal_subscribe(
            dbusConnection_,
            BLUEZ_SERVICE,
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged",
            characteristicPath_.c_str(),
            nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE,
            [](GDBusConnection*, const gchar*, const gchar*, const gchar*, 
               const gchar*, GVariant* parameters, gpointer userData) {
                
                auto* self = static_cast<BleMidiDevice*>(userData);
                
                const gchar* interface;
                GVariant* changedProps;
                GVariant* invalidatedProps;
                
                g_variant_get(parameters, "(&s@a{sv}@as)", 
                            &interface, &changedProps, &invalidatedProps);
                
                GVariant* value = nullptr;
                if (g_variant_lookup(changedProps, "Value", "@ay", &value)) {
                    
                    size_t len;
                    const uint8_t* data = static_cast<const uint8_t*>(
                        g_variant_get_fixed_array(value, &len, sizeof(uint8_t))
                    );
                    
                    if (len > 0) {
                        MidiMessage msg = self->parseBlePacket(data, len);
                        if (!msg.getRawData().empty() && msg.getRawData()[0] != 0x00) {
                            std::lock_guard<std::mutex> lock(self->queueMutex_);
                            self->messageQueue_.push(std::move(msg));
                            self->messagesReceived_++;
                        }
                    }
                    
                    g_variant_unref(value);
                }
                
                g_variant_unref(changedProps);
                g_variant_unref(invalidatedProps);
            },
            this,
            nullptr
        );
    }
    
    while (readThreadRunning_.load()) {
        updateRssi();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    if (subscriptionId > 0) {
        g_dbus_connection_signal_unsubscribe(dbusConnection_, subscriptionId);
    }
    
    Logger::info("BleMidiDevice", "Read thread stopped for: " + name_);
}

MidiMessage BleMidiDevice::parseBlePacket(const uint8_t* data, size_t len) {
    if (len < 2) {
        return MidiMessage();  // Empty message
    }
    
    // Skip BLE header
    size_t offset = 1;
    
    uint8_t status = data[offset];
    
    if ((status & 0x80) == 0) {
        return MidiMessage();  // Invalid status byte
    }
    
    uint8_t statusType = status & 0xF0;
    uint8_t channel = status & 0x0F;
    
    switch (statusType) {
        case 0x80:  // Note Off
            if (offset + 2 < len) {
                return MidiMessage::noteOff(channel, data[offset + 1], data[offset + 2]);
            }
            break;
            
        case 0x90:  // Note On
            if (offset + 2 < len) {
                return MidiMessage::noteOn(channel, data[offset + 1], data[offset + 2]);
            }
            break;
            
        case 0xB0:  // Control Change
            if (offset + 2 < len) {
                return MidiMessage::controlChange(channel, data[offset + 1], data[offset + 2]);
            }
            break;
            
        case 0xC0:  // Program Change
            if (offset + 1 < len) {
                return MidiMessage::programChange(channel, data[offset + 1]);
            }
            break;
            
        case 0xF0:  // System
            if (status == 0xF0) {
                // System Exclusive
                std::vector<uint8_t> sysex(data + offset, data + len);
                return MidiMessage(sysex);
            }
            break;
            
        default:
            break;
    }
    
    return MidiMessage();  // Empty message for unrecognized types
}

std::vector<uint8_t> BleMidiDevice::encodeBlePacket(const MidiMessage& msg) {
    std::vector<uint8_t> packet;
    packet.push_back(0x80);  // BLE MIDI header with timestamp
    packet.push_back(0x80);  // Additional timestamp byte
    
    const auto& rawData = msg.getRawData();
    
    if (rawData.empty()) {
        return packet;
    }
    
    uint8_t status = rawData[0];
    uint8_t statusType = status & 0xF0;
    
    switch (statusType) {
        case 0x80:  // Note Off
        case 0x90:  // Note On
        case 0xB0:  // Control Change
            if (rawData.size() >= 3) {
                packet.push_back(rawData[0]);  // Status
                packet.push_back(rawData[1]);  // Data1
                packet.push_back(rawData[2]);  // Data2
            }
            break;
            
        case 0xC0:  // Program Change
        case 0xD0:  // Channel Pressure
            if (rawData.size() >= 2) {
                packet.push_back(rawData[0]);  // Status
                packet.push_back(rawData[1]);  // Data1
            }
            break;
            
        case 0xF0:  // System Exclusive
            // Copy entire SysEx message
            packet.insert(packet.end(), rawData.begin(), rawData.end());
            break;
            
        default:
            // Copy all bytes for other message types
            packet.insert(packet.end(), rawData.begin(), rawData.end());
            break;
    }
    
    return packet;
}

void BleMidiDevice::updateRssi() {
    if (!dbusConnection_ || objectPath_.empty()) {
        return;
    }
    
    GError* error = nullptr;
    GDBusProxy* deviceProxy = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        objectPath_.c_str(),
        "org.freedesktop.DBus.Properties",
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return;
    }
    
    GVariant* result = g_dbus_proxy_call_sync(
        deviceProxy,
        "Get",
        g_variant_new("(ss)", DEVICE_INTERFACE, "RSSI"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(deviceProxy);
    
    if (error) {
        g_error_free(error);
        return;
    }
    
    if (result) {
        GVariant* value = nullptr;
        g_variant_get(result, "(v)", &value);
        
        if (value && g_variant_is_of_type(value, G_VARIANT_TYPE_INT16)) {
            rssi_ = g_variant_get_int16(value);
        }
        
        if (value) {
            g_variant_unref(value);
        }
        g_variant_unref(result);
    }
}

GDBusConnection* BleMidiDevice::getDbusConnection() {
    GError* error = nullptr;
    GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    
    if (error) {
        Logger::error("BleMidiDevice", 
            "Failed to get D-Bus connection: " + std::string(error->message));
        g_error_free(error);
        return nullptr;
    }
    
    return conn;
}

bool BleMidiDevice::hasMidiService(GDBusConnection* conn, const std::string& objectPath) {
    GError* error = nullptr;
    
    GDBusProxy* objectManagerProxy = g_dbus_proxy_new_sync(
        conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    GVariant* result = g_dbus_proxy_call_sync(
        objectManagerProxy,
        "GetManagedObjects",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(objectManagerProxy);
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    bool hasMidi = false;
    
    if (result) {
        GVariantIter* objectsIter;
        g_variant_get(result, "(a{oa{sa{sv}}})", &objectsIter);
        
        const gchar* objPath;
        GVariant* interfaces;
        
        while (g_variant_iter_loop(objectsIter, "{&o@a{sa{sv}}}", &objPath, &interfaces)) {
            if (std::string(objPath).find(objectPath) == std::string::npos) {
                continue;
            }
            
            GVariantIter* interfacesIter;
            g_variant_get(interfaces, "a{sa{sv}}", &interfacesIter);
            
            const gchar* interfaceName;
            GVariant* properties;
            
            while (g_variant_iter_loop(interfacesIter, "{&s@a{sv}}", &interfaceName, &properties)) {
                if (std::string(interfaceName) == "org.bluez.GattService1") {
                    GVariant* uuidVar = g_variant_lookup_value(properties, "UUID", G_VARIANT_TYPE_STRING);
                    
                    if (uuidVar) {
                        const gchar* uuid = g_variant_get_string(uuidVar, nullptr);
                        
                        if (std::string(uuid) == BLE_MIDI_SERVICE_UUID) {
                            hasMidi = true;
                            g_variant_unref(uuidVar);
                            break;
                        }
                        
                        g_variant_unref(uuidVar);
                    }
                }
            }
            
            g_variant_iter_free(interfacesIter);
            
            if (hasMidi) break;
        }
        
        g_variant_iter_free(objectsIter);
        g_variant_unref(result);
    }
    
    return hasMidi;
}

} // namespace midiMind

// ============================================================================
// END OF FILE BleMidiDevice.cpp
// ============================================================================
// ============================================================================
// Fichier: src/network/BluetoothManager.cpp
// Version: 1.0.0
// Date: 2025-10-15
// ============================================================================

#include "BluetoothManager.h"
#include "../core/Logger.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <cstring>

// Headers BlueZ (si disponible)
#ifdef HAS_BLUEZ
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#endif

namespace midiMind {

// ============================================================================
// CONSTANTES
// ============================================================================

namespace BlueZ {
    const char* SERVICE = "org.bluez";
    const char* ADAPTER_INTERFACE = "org.bluez.Adapter1";
    const char* DEVICE_INTERFACE = "org.bluez.Device1";
    const char* OBJECT_MANAGER_INTERFACE = "org.freedesktop.DBus.ObjectManager";
    
    // UUIDs de services connus
    const char* MIDI_SERVICE_UUID = "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
    const char* AUDIO_SINK_UUID = "0000110b-0000-1000-8000-00805f9b34fb";
    const char* HID_UUID = "00001124-0000-1000-8000-00805f9b34fb";
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

BluetoothManager::BluetoothManager()
    : initialized_(false)
    , scanning_(false)
    , powered_(false)
    , discoverable_(false)
    , dbusConnection_(nullptr)
    , adapterProxy_(nullptr)
    , scanDuration_(10) {
    
#ifndef HAS_BLUEZ
    Logger::warn("BluetoothManager", "Built without BlueZ support - functionality limited");
#endif
    
    Logger::info("BluetoothManager", "BluetoothManager constructed");
}

BluetoothManager::~BluetoothManager() {
    Logger::info("BluetoothManager", "Shutting down BluetoothManager...");
    
    stopScan();
    
    if (scanThread_.joinable()) {
        scanThread_.join();
    }
    
#ifdef HAS_BLUEZ
    if (deviceAddedSignal_ > 0) {
        g_dbus_connection_signal_unsubscribe(dbusConnection_, deviceAddedSignal_);
    }
    if (propertiesChangedSignal_ > 0) {
        g_dbus_connection_signal_unsubscribe(dbusConnection_, propertiesChangedSignal_);
    }
    if (adapterProxy_) {
        g_object_unref(adapterProxy_);
    }
    if (dbusConnection_) {
        g_object_unref(dbusConnection_);
    }
#endif
    
    Logger::info("BluetoothManager", "BluetoothManager destroyed");
}

// ============================================================================
// INITIALISATION
// ============================================================================

bool BluetoothManager::initialize() {
    if (initialized_) {
        Logger::warn("BluetoothManager", "Already initialized");
        return true;
    }
    
    Logger::info("BluetoothManager", "Initializing BluetoothManager...");
    
#ifdef HAS_BLUEZ
    if (!connectToDBus()) {
        Logger::error("BluetoothManager", "Failed to connect to D-Bus");
        return false;
    }
    
    if (!getDefaultAdapter()) {
        Logger::error("BluetoothManager", "Failed to get Bluetooth adapter");
        return false;
    }
    
    // S'abonner aux signaux D-Bus
    deviceAddedSignal_ = g_dbus_connection_signal_subscribe(
        dbusConnection_,
        BlueZ::SERVICE,
        BlueZ::OBJECT_MANAGER_INTERFACE,
        "InterfacesAdded",
        nullptr,
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        onDeviceAdded,
        this,
        nullptr
    );
    
    propertiesChangedSignal_ = g_dbus_connection_signal_subscribe(
        dbusConnection_,
        BlueZ::SERVICE,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        nullptr,
        BlueZ::DEVICE_INTERFACE,
        G_DBUS_SIGNAL_FLAGS_NONE,
        onPropertiesChanged,
        this,
        nullptr
    );
    
    initialized_ = true;
    Logger::info("BluetoothManager", "✓ BluetoothManager initialized");
    return true;
    
#else
    // Stub pour compilation sans BlueZ
    initialized_ = true;
    adapterAddress_ = "00:00:00:00:00:00";
    adapterName_ = "Stub Adapter";
    Logger::warn("BluetoothManager", "Initialized with stub (no BlueZ)");
    return true;
#endif
}

bool BluetoothManager::isInitialized() const {
    return initialized_;
}

bool BluetoothManager::isBluetoothAvailable() {
#ifdef HAS_BLUEZ
    int devId = hci_get_route(nullptr);
    if (devId < 0) {
        return false;
    }
    
    int sock = hci_open_dev(devId);
    if (sock < 0) {
        return false;
    }
    
    hci_close_dev(sock);
    return true;
#else
    return false;
#endif
}

// ============================================================================
// SCAN / DÉCOUVERTE
// ============================================================================

bool BluetoothManager::startScan(int duration, const std::vector<std::string>& filterUuids) {
    if (!initialized_) {
        Logger::error("BluetoothManager", "Not initialized");
        return false;
    }
    
    if (scanning_) {
        Logger::warn("BluetoothManager", "Scan already in progress");
        return false;
    }
    
    Logger::info("BluetoothManager", "Starting Bluetooth scan...");
    Logger::info("BluetoothManager", "  Duration: " + std::to_string(duration) + "s");
    
    scanDuration_ = duration;
    filterUuids_ = filterUuids;
    scanning_ = true;
    
    // Lancer le scan dans un thread
    if (scanThread_.joinable()) {
        scanThread_.join();
    }
    
    scanThread_ = std::thread([this]() {
        scanLoop();
    });
    
    return true;
}

void BluetoothManager::stopScan() {
    if (!scanning_) {
        return;
    }
    
    Logger::info("BluetoothManager", "Stopping scan...");
    scanning_ = false;
    
#ifdef HAS_BLUEZ
    if (adapterProxy_) {
        GError* error = nullptr;
        g_dbus_proxy_call_sync(
            adapterProxy_,
            "StopDiscovery",
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error
        );
        
        if (error) {
            Logger::warn("BluetoothManager", "StopDiscovery error: " + std::string(error->message));
            g_error_free(error);
        }
    }
#endif
}

bool BluetoothManager::isScanning() const {
    return scanning_;
}

std::vector<BluetoothDevice> BluetoothManager::getDiscoveredDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<BluetoothDevice> devices;
    devices.reserve(devices_.size());
    
    for (const auto& pair : devices_) {
        devices.push_back(pair.second);
    }
    
    return devices;
}

std::optional<BluetoothDevice> BluetoothManager::getDevice(const std::string& address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = devices_.find(address);
    if (it != devices_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void BluetoothManager::scanLoop() {
    Logger::info("BluetoothManager", "Scan loop started");
    
#ifdef HAS_BLUEZ
    GError* error = nullptr;
    
    // Démarrer discovery
    g_dbus_proxy_call_sync(
        adapterProxy_,
        "StartDiscovery",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BluetoothManager", "StartDiscovery failed: " + std::string(error->message));
        g_error_free(error);
        scanning_ = false;
        return;
    }
    
    // Attendre la durée du scan
    auto startTime = std::chrono::steady_clock::now();
    while (scanning_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime
        ).count();
        
        if (scanDuration_ > 0 && elapsed >= scanDuration_) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Arrêter discovery
    g_dbus_proxy_call_sync(
        adapterProxy_,
        "StopDiscovery",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::warn("BluetoothManager", "StopDiscovery error: " + std::string(error->message));
        g_error_free(error);
    }
#else
    // Stub : simuler un scan
    std::this_thread::sleep_for(std::chrono::seconds(scanDuration_));
    
    // Ajouter un device fictif pour test
    BluetoothDevice stubDevice;
    stubDevice.address = "AA:BB:CC:DD:EE:FF";
    stubDevice.name = "Stub BLE MIDI Device";
    stubDevice.type = BluetoothDeviceType::BLE_MIDI;
    stubDevice.state = BluetoothDeviceState::DISCOVERED;
    stubDevice.rssi = -60;
    stubDevice.paired = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        devices_[stubDevice.address] = stubDevice;
    }
    
    if (onDeviceDiscovered_) {
        onDeviceDiscovered_(stubDevice);
    }
#endif
    
    scanning_ = false;
    
    int devicesFound = devices_.size();
    Logger::info("BluetoothManager", "Scan complete: " + std::to_string(devicesFound) + " devices");
    
    if (onScanComplete_) {
        onScanComplete_(devicesFound);
    }
}

// ============================================================================
// PAIRING
// ============================================================================

bool BluetoothManager::pair(const std::string& address, const std::string& pin) {
    if (!initialized_) {
        Logger::error("BluetoothManager", "Not initialized");
        return false;
    }
    
    Logger::info("BluetoothManager", "Pairing with: " + address);
    
#ifdef HAS_BLUEZ
    GDBusProxy* deviceProxy = getDeviceProxy(address);
    if (!deviceProxy) {
        Logger::error("BluetoothManager", "Device not found: " + address);
        return false;
    }
    
    GError* error = nullptr;
    g_dbus_proxy_call_sync(
        deviceProxy,
        "Pair",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(deviceProxy);
    
    if (error) {
        Logger::error("BluetoothManager", "Pairing failed: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    Logger::info("BluetoothManager", "✓ Paired with: " + address);
    updateDeviceState(address, BluetoothDeviceState::PAIRED);
    return true;
#else
    Logger::warn("BluetoothManager", "Pairing not supported (stub)");
    return false;
#endif
}

bool BluetoothManager::unpair(const std::string& address) {
    if (!initialized_) {
        return false;
    }
    
    Logger::info("BluetoothManager", "Unpairing: " + address);
    
#ifdef HAS_BLUEZ
    // RemoveDevice sur l'adaptateur
    GError* error = nullptr;
    std::string devicePath = "/org/bluez/hci0/dev_" + address;
    std::replace(devicePath.begin(), devicePath.end(), ':', '_');
    
    g_dbus_proxy_call_sync(
        adapterProxy_,
        "RemoveDevice",
        g_variant_new("(o)", devicePath.c_str()),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BluetoothManager", "Unpair failed: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        devices_.erase(address);
    }
    
    Logger::info("BluetoothManager", "✓ Unpaired: " + address);
    return true;
#else
    return false;
#endif
}

std::vector<BluetoothDevice> BluetoothManager::getPairedDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<BluetoothDevice> paired;
    for (const auto& pair : devices_) {
        if (pair.second.paired) {
            paired.push_back(pair.second);
        }
    }
    
    return paired;
}

// ============================================================================
// CONNEXION
// ============================================================================

bool BluetoothManager::connect(const std::string& address) {
    if (!initialized_) {
        return false;
    }
    
    Logger::info("BluetoothManager", "Connecting to: " + address);
    
#ifdef HAS_BLUEZ
    GDBusProxy* deviceProxy = getDeviceProxy(address);
    if (!deviceProxy) {
        Logger::error("BluetoothManager", "Device not found: " + address);
        return false;
    }
    
    updateDeviceState(address, BluetoothDeviceState::CONNECTING);
    
    GError* error = nullptr;
    g_dbus_proxy_call_sync(
        deviceProxy,
        "Connect",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        30000,  // 30s timeout
        nullptr,
        &error
    );
    
    g_object_unref(deviceProxy);
    
    if (error) {
        Logger::error("BluetoothManager", "Connection failed: " + std::string(error->message));
        g_error_free(error);
        updateDeviceState(address, BluetoothDeviceState::DISCOVERED);
        return false;
    }
    
    Logger::info("BluetoothManager", "✓ Connected to: " + address);
    updateDeviceState(address, BluetoothDeviceState::CONNECTED);
    return true;
#else
    return false;
#endif
}

bool BluetoothManager::disconnect(const std::string& address) {
    if (!initialized_) {
        return false;
    }
    
    Logger::info("BluetoothManager", "Disconnecting: " + address);
    
#ifdef HAS_BLUEZ
    GDBusProxy* deviceProxy = getDeviceProxy(address);
    if (!deviceProxy) {
        return false;
    }
    
    updateDeviceState(address, BluetoothDeviceState::DISCONNECTING);
    
    GError* error = nullptr;
    g_dbus_proxy_call_sync(
        deviceProxy,
        "Disconnect",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(deviceProxy);
    
    if (error) {
        Logger::error("BluetoothManager", "Disconnect failed: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    Logger::info("BluetoothManager", "✓ Disconnected: " + address);
    updateDeviceState(address, BluetoothDeviceState::PAIRED);
    return true;
#else
    return false;
#endif
}

bool BluetoothManager::isConnected(const std::string& address) const {
    auto device = getDevice(address);
    return device && device->state == BluetoothDeviceState::CONNECTED;
}

std::vector<BluetoothDevice> BluetoothManager::getConnectedDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<BluetoothDevice> connected;
    for (const auto& pair : devices_) {
        if (pair.second.state == BluetoothDeviceState::CONNECTED) {
            connected.push_back(pair.second);
        }
    }
    
    return connected;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

bool BluetoothManager::setTrusted(const std::string& address, bool trusted) {
#ifdef HAS_BLUEZ
    GDBusProxy* deviceProxy = getDeviceProxy(address);
    if (!deviceProxy) {
        return false;
    }
    
    GError* error = nullptr;
    g_dbus_proxy_call_sync(
        deviceProxy,
        "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", 
                     BlueZ::DEVICE_INTERFACE,
                     "Trusted",
                     g_variant_new_boolean(trusted)),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(deviceProxy);
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

bool BluetoothManager::setBlocked(const std::string& address, bool blocked) {
#ifdef HAS_BLUEZ
    GDBusProxy* deviceProxy = getDeviceProxy(address);
    if (!deviceProxy) {
        return false;
    }
    
    GError* error = nullptr;
    g_dbus_proxy_call_sync(
        deviceProxy,
        "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", 
                     BlueZ::DEVICE_INTERFACE,
                     "Blocked",
                     g_variant_new_boolean(blocked)),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(deviceProxy);
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

bool BluetoothManager::setPowered(bool enabled) {
#ifdef HAS_BLUEZ
    if (!adapterProxy_) {
        return false;
    }
    
    GError* error = nullptr;
    g_dbus_proxy_call_sync(
        adapterProxy_,
        "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", 
                     BlueZ::ADAPTER_INTERFACE,
                     "Powered",
                     g_variant_new_boolean(enabled)),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    powered_ = enabled;
    return true;
#else
    return false;
#endif
}

bool BluetoothManager::isPowered() const {
    return powered_;
}

bool BluetoothManager::setDiscoverable(bool enabled, int timeout) {
#ifdef HAS_BLUEZ
    if (!adapterProxy_) {
        return false;
    }
    
    GError* error = nullptr;
    g_dbus_proxy_call_sync(
        adapterProxy_,
        "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", 
                     BlueZ::ADAPTER_INTERFACE,
                     "Discoverable",
                     g_variant_new_boolean(enabled)),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    if (enabled && timeout > 0) {
        g_dbus_proxy_call_sync(
            adapterProxy_,
            "org.freedesktop.DBus.Properties.Set",
            g_variant_new("(ssv)", 
                         BlueZ::ADAPTER_INTERFACE,
                         "DiscoverableTimeout",
                         g_variant_new_uint32(timeout)),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            nullptr
        );
    }
    
    discoverable_ = enabled;
    return true;
#else
    return false;
#endif
}

bool BluetoothManager::isDiscoverable() const {
    return discoverable_;
}

// ============================================================================
// INFORMATIONS
// ============================================================================

json BluetoothManager::getAdapterInfo() const {
    json info;
    info["address"] = adapterAddress_;
    info["name"] = adapterName_;
    info["powered"] = powered_.load();
    info["discoverable"] = discoverable_.load();
    info["scanning"] = scanning_.load();
    return info;
}

json BluetoothManager::getStatus() const {
    json status;
    status["initialized"] = initialized_.load();
    status["adapter"] = getAdapterInfo();
    status["devices_count"] = devices_.size();
    status["connected_count"] = getConnectedDevices().size();
    return status;
}

std::string BluetoothManager::getAdapterAddress() const {
    return adapterAddress_;
}

std::string BluetoothManager::getAdapterName() const {
    return adapterName_;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void BluetoothManager::setOnDeviceDiscovered(DeviceDiscoveredCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onDeviceDiscovered_ = callback;
}

void BluetoothManager::setOnDeviceStateChanged(DeviceStateChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onDeviceStateChanged_ = callback;
}

void BluetoothManager::setOnScanComplete(ScanCompleteCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onScanComplete_ = callback;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

#ifdef HAS_BLUEZ

bool BluetoothManager::connectToDBus() {
    GError* error = nullptr;
    dbusConnection_ = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    
    if (!dbusConnection_) {
        Logger::error("BluetoothManager", "D-Bus connection failed: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    Logger::debug("BluetoothManager", "✓ Connected to D-Bus");
    return true;
}

bool BluetoothManager::getDefaultAdapter() {
    GError* error = nullptr;
    
    adapterProxy_ = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BlueZ::SERVICE,
        "/org/bluez/hci0",
        BlueZ::ADAPTER_INTERFACE,
        nullptr,
        &error
    );
    
    if (!adapterProxy_) {
        Logger::error("BluetoothManager", "Failed to get adapter: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    adapterPath_ = "/org/bluez/hci0";
    
    // Récupérer l'adresse et le nom
    GVariant* addressVariant = g_dbus_proxy_get_cached_property(adapterProxy_, "Address");
    if (addressVariant) {
        adapterAddress_ = g_variant_get_string(addressVariant, nullptr);
        g_variant_unref(addressVariant);
    }
    
    GVariant* nameVariant = g_dbus_proxy_get_cached_property(adapterProxy_, "Name");
    if (nameVariant) {
        adapterName_ = g_variant_get_string(nameVariant, nullptr);
        g_variant_unref(nameVariant);
    }
    
    Logger::info("BluetoothManager", "✓ Adapter: " + adapterName_ + " (" + adapterAddress_ + ")");
    return true;
}

GDBusProxy* BluetoothManager::getDeviceProxy(const std::string& address) {
    std::string devicePath = "/org/bluez/hci0/dev_" + address;
    std::replace(devicePath.begin(), devicePath.end(), ':', '_');
    
    GError* error = nullptr;
    GDBusProxy* proxy = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BlueZ::SERVICE,
        devicePath.c_str(),
        BlueZ::DEVICE_INTERFACE,
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return nullptr;
    }
    
    return proxy;
}

BluetoothDevice BluetoothManager::parseDeviceFromProxy(GDBusProxy* proxy) {
    BluetoothDevice device;
    
    // Address
    GVariant* addr = g_dbus_proxy_get_cached_property(proxy, "Address");
    if (addr) {
        device.address = g_variant_get_string(addr, nullptr);
        g_variant_unref(addr);
    }
    
    // Name
    GVariant* name = g_dbus_proxy_get_cached_property(proxy, "Name");
    if (name) {
        device.name = g_variant_get_string(name, nullptr);
        g_variant_unref(name);
    } else {
        device.name = "Unknown Device";
    }
    
    // RSSI
    GVariant* rssi = g_dbus_proxy_get_cached_property(proxy, "RSSI");
    if (rssi) {
        device.rssi = g_variant_get_int16(rssi);
        g_variant_unref(rssi);
    }
    
    // Paired
    GVariant* paired = g_dbus_proxy_get_cached_property(proxy, "Paired");
    if (paired) {
        device.paired = g_variant_get_boolean(paired);
        g_variant_unref(paired);
    }
    
    // Connected
    GVariant* connected = g_dbus_proxy_get_cached_property(proxy, "Connected");
    if (connected) {
        bool isConnected = g_variant_get_boolean(connected);
        device.state = isConnected ? BluetoothDeviceState::CONNECTED : 
                      (device.paired ? BluetoothDeviceState::PAIRED : 
                       BluetoothDeviceState::DISCOVERED);
        g_variant_unref(connected);
    }
    
    // UUIDs
    GVariant* uuids = g_dbus_proxy_get_cached_property(proxy, "UUIDs");
    if (uuids) {
        GVariantIter iter;
        g_variant_iter_init(&iter, uuids);
        const gchar* uuid;
        while (g_variant_iter_next(&iter, "&s", &uuid)) {
            device.uuids.push_back(uuid);
        }
        g_variant_unref(uuids);
    }
    
    device.type = detectDeviceType(device.uuids);
    
    return device;
}

BluetoothDeviceType BluetoothManager::detectDeviceType(const std::vector<std::string>& uuids) {
    for (const auto& uuid : uuids) {
        if (uuid.find(BlueZ::MIDI_SERVICE_UUID) != std::string::npos) {
            return BluetoothDeviceType::BLE_MIDI;
        }
        if (uuid.find(BlueZ::AUDIO_SINK_UUID) != std::string::npos) {
            return BluetoothDeviceType::AUDIO;
        }
        if (uuid.find(BlueZ::HID_UUID) != std::string::npos) {
            return BluetoothDeviceType::INPUT;
        }
    }
    
    return BluetoothDeviceType::UNKNOWN;
}

void BluetoothManager::onDeviceAdded(GDBusConnection* connection,
                                    const gchar* sender,
                                    const gchar* objectPath,
                                    const gchar* interfaceName,
                                    const gchar* signalName,
                                    GVariant* parameters,
                                    gpointer userData) {
    auto* manager = static_cast<BluetoothManager*>(userData);
    
    // Créer un proxy pour le nouveau device
    GError* error = nullptr;
    GDBusProxy* deviceProxy = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BlueZ::SERVICE,
        objectPath,
        BlueZ::DEVICE_INTERFACE,
        nullptr,
        &error
    );
    
    if (deviceProxy) {
        BluetoothDevice device = manager->parseDeviceFromProxy(deviceProxy);
        
        {
            std::lock_guard<std::mutex> lock(manager->mutex_);
            manager->devices_[device.address] = device;
        }
        
        if (manager->onDeviceDiscovered_) {
            manager->onDeviceDiscovered_(device);
        }
        
        g_object_unref(deviceProxy);
    }
    
    if (error) {
        g_error_free(error);
    }
}

void BluetoothManager::onPropertiesChanged(GDBusConnection* connection,
                                          const gchar* sender,
                                          const gchar* objectPath,
                                          const gchar* interfaceName,
                                          const gchar* signalName,
                                          GVariant* parameters,
                                          gpointer userData) {
    auto* manager = static_cast<BluetoothManager*>(userData);
    
    // Parser les propriétés modifiées
    const gchar* interface;
    GVariant* changedProps;
    GVariant* invalidatedProps;
    
    g_variant_get(parameters, "(&s@a{sv}@as)", &interface, &changedProps, &invalidatedProps);
    
    // Si c'est un device, mettre à jour l'état
    std::string path(objectPath);
    if (path.find("/org/bluez/hci0/dev_") != std::string::npos) {
        // Extraire l'adresse du path
        size_t pos = path.rfind("dev_");
        if (pos != std::string::npos) {
            std::string address = path.substr(pos + 4);
            std::replace(address.begin(), address.end(), '_', ':');
            
            // Vérifier si "Connected" a changé
            GVariant* connected = g_variant_lookup_value(changedProps, "Connected", G_VARIANT_TYPE_BOOLEAN);
            if (connected) {
                bool isConnected = g_variant_get_boolean(connected);
                BluetoothDeviceState newState = isConnected ? 
                    BluetoothDeviceState::CONNECTED : 
                    BluetoothDeviceState::PAIRED;
                
                manager->updateDeviceState(address, newState);
                g_variant_unref(connected);
            }
        }
    }
    
    g_variant_unref(changedProps);
    g_variant_unref(invalidatedProps);
}

#endif // HAS_BLUEZ

void BluetoothManager::updateDeviceState(const std::string& address, BluetoothDeviceState state) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = devices_.find(address);
        if (it != devices_.end()) {
            it->second.state = state;
        }
    }
    
    if (onDeviceStateChanged_) {
        onDeviceStateChanged_(address, state);
    }
}

std::string BluetoothManager::executeCommand(const std::string& command) const {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    std::ostringstream output;
    char buffer[256];
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output << buffer;
    }
    
    pclose(pipe);
    return output.str();
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER BluetoothManager.cpp v1.0.0
// ============================================================================
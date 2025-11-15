// ============================================================================
// File: backend/src/midi/devices/MidiDeviceManager.cpp
// Version: 4.2.0 - EventBus Integration + BLE MIDI Support
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#include "MidiDeviceManager.h"
#include "UsbMidiDevice.h"
#include "VirtualMidiDevice.h"
#include "BleMidiDevice.h"
#include "../../core/Logger.h"
#include "../../core/EventBus.h"
#include "../../core/TimeUtils.h"
#include "../../events/Events.h"
#include "../sysex/SysExHandler.h"
#include "../sysex/SysExParser.h"
#include "../sysex/MidiManufacturers.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <set>

#ifdef __linux__
#include <alsa/asoundlib.h>
#include <gio/gio.h>
#endif

namespace midiMind {

MidiDeviceManager::MidiDeviceManager(std::shared_ptr<EventBus> eventBus)
    : eventBus_(eventBus)
    , sysexHandler_(std::make_shared<SysExHandler>())
{
    Logger::info("MidiDeviceManager", "Initializing MidiDeviceManager v4.2.0 (EventBus + BLE + SysEx)");

    // Configure SysEx handler callback for device identification
    sysexHandler_->setOnDeviceIdentified([this](const std::string& deviceId, const DeviceIdentity& identity) {
        std::string manufacturer = MidiManufacturers::getName(identity.manufacturerId);
        std::string model = "Family:" + std::to_string(identity.familyCode) +
                          " Model:" + std::to_string(identity.modelNumber);
        std::string version = std::to_string(identity.versionMajor) + "." +
                            std::to_string(identity.versionMinor) + "." +
                            std::to_string(identity.versionPatch);

        Logger::info("MidiDeviceManager",
                    "Device identified: " + deviceId +
                    " - " + manufacturer + " " + model +
                    " v" + version);

        // Update device info in available devices list
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& info : availableDevices_) {
                if (info.id == deviceId) {
                    info.manufacturer = manufacturer;
                    info.model = model;
                    info.version = version;
                    break;
                }
            }
        }

        // Publish event if EventBus available
        if (eventBus_) {
            eventBus_->publish(events::DeviceIdentifiedEvent(
                deviceId,
                manufacturer,
                model,
                version,
                TimeUtils::systemNow()
            ));
        }
    });

    Logger::info("MidiDeviceManager", "SysEx auto-identification enabled");
}

MidiDeviceManager::~MidiDeviceManager() {
    Logger::info("MidiDeviceManager", "Shutting down MidiDeviceManager");
    stopHotPlugMonitoring();
    disconnectAll();
}

std::vector<MidiDeviceInfo> MidiDeviceManager::discoverDevices(bool fullScan) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiDeviceManager", "Starting device discovery (fullScan=" + 
                std::string(fullScan ? "true" : "false") + ")");
    
    if (fullScan) {
        availableDevices_.clear();
    }
    
    auto usbDevices = discoverUsbDevices();
    availableDevices_.insert(availableDevices_.end(), usbDevices.begin(), usbDevices.end());
    
    if (bluetoothEnabled_.load()) {
        auto bleDevices = discoverBluetoothDevices();
        availableDevices_.insert(availableDevices_.end(), bleDevices.begin(), bleDevices.end());
    }
    
    Logger::info("MidiDeviceManager", "Discovery complete: " + 
                std::to_string(availableDevices_.size()) + " devices found");
    
    return availableDevices_;
}

std::vector<MidiDeviceInfo> MidiDeviceManager::getAvailableDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return availableDevices_;
}

int MidiDeviceManager::getDeviceCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(devices_.size());
}

void MidiDeviceManager::setBluetoothEnabled(bool enable) {
    bluetoothEnabled_ = enable;
    Logger::info("MidiDeviceManager", 
        "Bluetooth scanning " + std::string(enable ? "enabled" : "disabled"));
}

bool MidiDeviceManager::isBluetoothEnabled() const {
    return bluetoothEnabled_.load();
}

void MidiDeviceManager::setBluetoothScanTimeout(int seconds) {
    if (seconds < 1) seconds = 1;
    if (seconds > 30) seconds = 30;
    bluetoothScanTimeout_ = seconds;
    Logger::info("MidiDeviceManager", 
        "Bluetooth scan timeout set to " + std::to_string(seconds) + "s");
}

bool MidiDeviceManager::connect(const std::string& deviceId) {
    std::shared_ptr<MidiDevice> device;
    std::string deviceName;
    DeviceType deviceType = DeviceType::USB;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Logger::info("MidiDeviceManager", "Connecting to device: " + deviceId);
        
        for (const auto& dev : devices_) {
            if (dev->getId() == deviceId) {
                Logger::warning("MidiDeviceManager", "Device already connected: " + deviceId);
                return true;
            }
        }
        
        auto it = std::find_if(availableDevices_.begin(), availableDevices_.end(),
                              [&deviceId](const MidiDeviceInfo& info) {
                                  return info.id == deviceId;
                              });
        
        if (it == availableDevices_.end()) {
            Logger::error("MidiDeviceManager", "Device not found: " + deviceId);
            return false;
        }
        
        device = createDevice(*it);
        if (!device) {
            Logger::error("MidiDeviceManager", "Failed to create device: " + deviceId);
            return false;
        }
        
        if (!device->connect()) {
            Logger::error("MidiDeviceManager", "Failed to connect device: " + deviceId);
            return false;
        }
        
        deviceName = device->getName();
        deviceType = device->getType();
        devices_.push_back(device);

        Logger::info("MidiDeviceManager", "✅ Device connected: " + deviceName);

        // Request device identity for auto-identification (USB devices only for now)
        if (deviceType == DeviceType::USB) {
            Logger::debug("MidiDeviceManager", "Requesting identity from: " + deviceName);
            std::thread([device]() {
                // Small delay to let device stabilize
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                device->requestIdentity();
            }).detach();
        }
    }

    std::function<void(const std::string&)> callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = onDeviceConnect_;
    }
    
    if (callback) {
        callback(deviceId);
    }
    
    if (eventBus_) {
        try {
            eventBus_->publish(events::DeviceConnectedEvent(
                deviceId,
                deviceName,
                MidiDevice::deviceTypeToString(deviceType),
                TimeUtils::systemNow()
            ));
            Logger::debug("MidiDeviceManager", "Published DeviceConnectedEvent: " + deviceName);
        } catch (const std::exception& e) {
            Logger::error("MidiDeviceManager", 
                "Failed to publish DeviceConnectedEvent: " + std::string(e.what()));
        }
    }
    
    return true;
}

void MidiDeviceManager::disconnect(const std::string& deviceId) {
    bool found = false;
    std::string deviceName;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Logger::info("MidiDeviceManager", "Disconnecting device: " + deviceId);
        
        auto it = std::find_if(devices_.begin(), devices_.end(),
                              [&deviceId](const std::shared_ptr<MidiDevice>& device) {
                                  return device->getId() == deviceId;
                              });
        
        if (it != devices_.end()) {
            deviceName = (*it)->getName();
            (*it)->disconnect();
            devices_.erase(it);
            found = true;
            
            Logger::info("MidiDeviceManager", "✅ Device disconnected: " + deviceName);
        }
    }
    
    if (found) {
        std::function<void(const std::string&)> callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callback = onDeviceDisconnect_;
        }
        
        if (callback) {
            callback(deviceId);
        }
        
        if (eventBus_) {
            try {
                eventBus_->publish(events::DeviceDisconnectedEvent(
                    deviceId,
                    deviceName,
                    "User disconnected",
                    TimeUtils::systemNow()
                ));
                Logger::debug("MidiDeviceManager", "Published DeviceDisconnectedEvent: " + deviceName);
            } catch (const std::exception& e) {
                Logger::error("MidiDeviceManager", 
                    "Failed to publish DeviceDisconnectedEvent: " + std::string(e.what()));
            }
        }
    }
}

void MidiDeviceManager::disconnectAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiDeviceManager", "Disconnecting all devices...");
    
    for (auto& device : devices_) {
        device->disconnect();
    }
    
    devices_.clear();
    
    Logger::info("MidiDeviceManager", "✅ All devices disconnected");
}

bool MidiDeviceManager::isConnected(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return std::any_of(devices_.begin(), devices_.end(),
                      [&deviceId](const std::shared_ptr<MidiDevice>& device) {
                          return device->getId() == deviceId;
                      });
}

std::shared_ptr<MidiDevice> MidiDeviceManager::getDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(devices_.begin(), devices_.end(),
                          [&deviceId](const std::shared_ptr<MidiDevice>& device) {
                              return device->getId() == deviceId;
                          });
    
    return (it != devices_.end()) ? *it : nullptr;
}

std::vector<std::shared_ptr<MidiDevice>> MidiDeviceManager::getConnectedDevices() {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_;
}

void MidiDeviceManager::startHotPlugMonitoring(int intervalMs) {
    if (hotPlugRunning_) {
        Logger::warning("MidiDeviceManager", "Hot-plug monitoring already active");
        return;
    }
    
    Logger::info("MidiDeviceManager", "Starting hot-plug monitoring (interval=" + 
                std::to_string(intervalMs) + "ms)");
    
    scanIntervalMs_ = intervalMs;
    hotPlugRunning_ = true;
    hotPlugThread_ = std::thread(&MidiDeviceManager::hotPlugThread, this);
    
    Logger::info("MidiDeviceManager", "✅ Hot-plug monitoring started");
}

void MidiDeviceManager::stopHotPlugMonitoring() {
    if (!hotPlugRunning_) {
        return;
    }
    
    Logger::info("MidiDeviceManager", "Stopping hot-plug monitoring...");
    
    hotPlugRunning_ = false;
    
    if (hotPlugThread_.joinable()) {
        hotPlugThread_.join();
    }
    
    Logger::info("MidiDeviceManager", "✅ Hot-plug monitoring stopped");
}

bool MidiDeviceManager::isHotPlugMonitoringActive() const {
    return hotPlugRunning_;
}

void MidiDeviceManager::setHotPlugCallbacks(
    std::function<void(const std::string&)> onConnect,
    std::function<void(const std::string&)> onDisconnect) 
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onDeviceConnect_ = onConnect;
    onDeviceDisconnect_ = onDisconnect;
}

void MidiDeviceManager::setEventBus(std::shared_ptr<EventBus> eventBus) {
    eventBus_ = eventBus;
    Logger::info("MidiDeviceManager", "EventBus configured");
}

void MidiDeviceManager::hotPlugThread() {
    Logger::debug("MidiDeviceManager", "Hot-plug monitor thread started");
    
    while (hotPlugRunning_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(scanIntervalMs_.load()));
        
        if (!hotPlugRunning_) break;
        
        Logger::debug("MidiDeviceManager", "Hot-plug: Rescanning devices...");
        discoverDevices(false);
    }
    
    Logger::debug("MidiDeviceManager", "Hot-plug monitor thread stopped");
}

std::vector<MidiDeviceInfo> MidiDeviceManager::discoverUsbDevices() {
    std::vector<MidiDeviceInfo> devices;

#ifdef __linux__
    Logger::info("MidiDeviceManager", "Scanning USB MIDI devices (ALSA)...");

    // First, get list of ALSA sound cards to verify hardware presence
    std::set<int> hardwareCards;
    int card = -1;
    while (snd_card_next(&card) >= 0 && card >= 0) {
        hardwareCards.insert(card);

        // Get card info for debugging
        snd_ctl_t* ctl;
        char name[32];
        sprintf(name, "hw:%d", card);
        if (snd_ctl_open(&ctl, name, 0) >= 0) {
            snd_ctl_card_info_t* info;
            snd_ctl_card_info_alloca(&info);
            if (snd_ctl_card_info(ctl, info) >= 0) {
                Logger::debug("MidiDeviceManager",
                    std::string("  Hardware card ") + std::to_string(card) + ": " +
                    snd_ctl_card_info_get_name(info));
            }
            snd_ctl_close(ctl);
        }
    }

    Logger::info("MidiDeviceManager",
        "Found " + std::to_string(hardwareCards.size()) + " hardware sound cards");

    snd_seq_t* seq = nullptr;

    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        Logger::warning("MidiDeviceManager", "Failed to open ALSA sequencer");
        return devices;
    }

    snd_seq_client_info_t* cinfo;
    snd_seq_port_info_t* pinfo;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);

    snd_seq_client_info_set_client(cinfo, -1);

    // Track cards we've already processed (one device per card)
    std::set<int> processedCards;

    while (snd_seq_query_next_client(seq, cinfo) >= 0) {
        int client = snd_seq_client_info_get_client(cinfo);

        // Skip system clients (0, 14, etc.)
        if (client == 0 || client == SND_SEQ_CLIENT_SYSTEM) {
            continue;
        }

        const char* clientName = snd_seq_client_info_get_name(cinfo);
        std::string clientNameStr(clientName ? clientName : "");

        // Get card number for this client
        int cardNum = snd_seq_client_info_get_card(cinfo);

        Logger::debug("MidiDeviceManager",
            std::string("Client ") + std::to_string(client) + ": \"" + clientNameStr +
            "\" (card=" + std::to_string(cardNum) + ")");

        // Skip clients without a hardware card association
        if (cardNum < 0) {
            Logger::debug("MidiDeviceManager", "  -> Skipped: no card association");
            continue;
        }

        // Skip if this card was already processed
        if (processedCards.find(cardNum) != processedCards.end()) {
            Logger::debug("MidiDeviceManager", "  -> Skipped: card already processed");
            continue;
        }

        // Verify this card actually exists in hardware
        if (hardwareCards.find(cardNum) == hardwareCards.end()) {
            Logger::debug("MidiDeviceManager", "  -> Skipped: card not in hardware list");
            continue;
        }

        // Additional name-based filtering for known virtual ports
        if (clientNameStr.find("Midi Through") != std::string::npos ||
            clientNameStr.find("MIDI Through") != std::string::npos ||
            clientNameStr == "System" ||
            clientNameStr == "Timer" ||
            clientNameStr == "Announce") {
            Logger::debug("MidiDeviceManager", "  -> Skipped: blacklisted name");
            continue;
        }

        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);

        // Find the first suitable port for this card
        bool foundValidPort = false;
        MidiDeviceInfo info;
        unsigned int combinedCaps = 0;

        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            unsigned int caps = snd_seq_port_info_get_capability(pinfo);

            // Skip ports without READ or WRITE capability
            if (!((caps & SND_SEQ_PORT_CAP_READ) || (caps & SND_SEQ_PORT_CAP_WRITE))) {
                continue;
            }

            // Skip ports that are only for subscription
            if ((caps & SND_SEQ_PORT_CAP_NO_EXPORT)) {
                continue;
            }

            if (!foundValidPort) {
                int port = snd_seq_port_info_get_port(pinfo);
                info.id = "usb_" + std::to_string(client) + "_" + std::to_string(port);
                info.name = snd_seq_port_info_get_name(pinfo);
                info.type = DeviceType::USB;
                info.port = std::to_string(client) + ":" + std::to_string(port);
                info.manufacturer = clientNameStr;
                info.available = true;
                info.status = DeviceStatus::DISCONNECTED;
                info.messagesReceived = 0;
                info.messagesSent = 0;
                foundValidPort = true;
            }

            // Combine capabilities from all ports
            combinedCaps |= caps;
        }

        if (foundValidPort) {
            // Set direction based on combined capabilities
            if ((combinedCaps & SND_SEQ_PORT_CAP_READ) && (combinedCaps & SND_SEQ_PORT_CAP_WRITE)) {
                info.direction = DeviceDirection::BIDIRECTIONAL;
            } else if (combinedCaps & SND_SEQ_PORT_CAP_READ) {
                info.direction = DeviceDirection::INPUT;
            } else {
                info.direction = DeviceDirection::OUTPUT;
            }

            devices.push_back(info);
            processedCards.insert(cardNum);

            Logger::info("MidiDeviceManager", "✓ ACCEPTED: " + info.name +
                        " (card " + std::to_string(cardNum) +
                        ", client " + std::to_string(client) + ")");
        }
    }

    snd_seq_close(seq);

    Logger::info("MidiDeviceManager", "✅ USB scan complete: " +
                std::to_string(devices.size()) + " devices found");
#else
    Logger::warning("MidiDeviceManager", "USB MIDI scanning not supported on this platform");
#endif

    return devices;
}

std::vector<MidiDeviceInfo> MidiDeviceManager::discoverBluetoothDevices() {
    std::vector<MidiDeviceInfo> devices;
    
#ifdef __linux__
    Logger::info("MidiDeviceManager", "Scanning BLE MIDI devices (BlueZ)...");
    
    GError* error = nullptr;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    
    if (error) {
        Logger::warning("MidiDeviceManager", 
            "Failed to connect to D-Bus: " + std::string(error->message));
        g_error_free(error);
        return devices;
    }
    
    GDBusProxy* managerProxy = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (error) {
        Logger::warning("MidiDeviceManager", 
            "BlueZ not available: " + std::string(error->message));
        g_error_free(error);
        g_object_unref(connection);
        return devices;
    }
    
    GVariant* result = g_dbus_proxy_call_sync(
        managerProxy,
        "GetManagedObjects",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        g_error_free(error);
        g_object_unref(managerProxy);
        g_object_unref(connection);
        return devices;
    }
    
    std::string adapterPath;
    GVariantIter* iter;
    const gchar* objectPath;
    GVariant* ifacesAndProperties;
    
    g_variant_get(result, "(a{oa{sa{sv}}})", &iter);
    
    while (g_variant_iter_next(iter, "{&o@a{sa{sv}}}", &objectPath, &ifacesAndProperties)) {
        if (g_variant_lookup(ifacesAndProperties, "org.bluez.Adapter1", "@a{sv}", nullptr)) {
            adapterPath = objectPath;
            g_variant_unref(ifacesAndProperties);
            break;
        }
        g_variant_unref(ifacesAndProperties);
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    
    if (adapterPath.empty()) {
        Logger::warning("MidiDeviceManager", "No Bluetooth adapter found");
        g_object_unref(managerProxy);
        g_object_unref(connection);
        return devices;
    }
    
    GDBusProxy* adapterProxy = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        adapterPath.c_str(),
        "org.bluez.Adapter1",
        nullptr,
        nullptr
    );
    
    if (adapterProxy) {
        g_dbus_proxy_call_sync(
            adapterProxy,
            "StartDiscovery",
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            nullptr
        );
        
        std::this_thread::sleep_for(std::chrono::seconds(bluetoothScanTimeout_.load()));
        
        g_dbus_proxy_call_sync(
            adapterProxy,
            "StopDiscovery",
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            nullptr
        );
        
        g_object_unref(adapterProxy);
    }
    
    result = g_dbus_proxy_call_sync(
        managerProxy,
        "GetManagedObjects",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        nullptr
    );
    
    if (result) {
        g_variant_get(result, "(a{oa{sa{sv}}})", &iter);
        
        while (g_variant_iter_next(iter, "{&o@a{sa{sv}}}", &objectPath, &ifacesAndProperties)) {
            GVariant* deviceProps = nullptr;
            if (g_variant_lookup(ifacesAndProperties, "org.bluez.Device1", "@a{sv}", &deviceProps)) {
                
                GVariant* uuids = nullptr;
                if (g_variant_lookup(deviceProps, "UUIDs", "@as", &uuids)) {
                    GVariantIter uuidIter;
                    const gchar* uuid;
                    bool hasBleMidi = false;
                    
                    g_variant_iter_init(&uuidIter, uuids);
                    while (g_variant_iter_next(&uuidIter, "&s", &uuid)) {
                        std::string uuidStr(uuid);
                        std::transform(uuidStr.begin(), uuidStr.end(), 
                                     uuidStr.begin(), ::tolower);
                        
                        if (uuidStr == "03b80e5a-ede8-4b33-a751-6ce34ec4c700") {
                            hasBleMidi = true;
                            break;
                        }
                    }
                    
                    if (hasBleMidi) {
                        const gchar* name = nullptr;
                        const gchar* address = nullptr;
                        
                        g_variant_lookup(deviceProps, "Name", "&s", &name);
                        g_variant_lookup(deviceProps, "Address", "&s", &address);
                        
                        if (address) {
                            MidiDeviceInfo info;
                            
                            std::string addressStr(address);
                            std::string deviceId = "ble_" + addressStr;
                            std::replace(deviceId.begin(), deviceId.end(), ':', '_');
                            
                            info.id = deviceId;
                            info.name = name ? name : "BLE MIDI Device";
                            info.type = DeviceType::BLUETOOTH;
                            info.direction = DeviceDirection::BIDIRECTIONAL;
                            info.status = DeviceStatus::DISCONNECTED;
                            info.bluetoothAddress = addressStr;
                            info.objectPath = objectPath;
                            info.available = true;
                            info.messagesReceived = 0;
                            info.messagesSent = 0;
                            
                            devices.push_back(info);
                            
                            Logger::info("MidiDeviceManager", "  Found: " + info.name);
                        }
                    }
                    
                    g_variant_unref(uuids);
                }
                
                g_variant_unref(deviceProps);
            }
            
            g_variant_unref(ifacesAndProperties);
        }
        
        g_variant_iter_free(iter);
        g_variant_unref(result);
    }
    
    g_object_unref(managerProxy);
    g_object_unref(connection);
    
    Logger::info("MidiDeviceManager", "✅ BLE scan complete: " + 
                std::to_string(devices.size()) + " devices found");
#else
    Logger::warning("MidiDeviceManager", "BLE MIDI scanning not supported on this platform");
#endif
    
    return devices;
}
std::vector<MidiDeviceInfo> MidiDeviceManager::scanBleDevices(int duration, 
                                                const std::string& nameFilter) {
    if (duration < 1) duration = 1;
    if (duration > 30) duration = 30;
    
    Logger::info("MidiDeviceManager", 
        "Starting BLE scan (duration=" + std::to_string(duration) + "s, filter='" + nameFilter + "')");
    
    int originalTimeout = bluetoothScanTimeout_.load();
    bluetoothScanTimeout_ = duration;
    
    auto devices = discoverBluetoothDevices();
    
    bluetoothScanTimeout_ = originalTimeout;
    
    if (!nameFilter.empty()) {
        devices.erase(
            std::remove_if(devices.begin(), devices.end(),
                [&nameFilter](const MidiDeviceInfo& info) {
                    return info.name.find(nameFilter) == std::string::npos;
                }),
            devices.end()
        );
    }
    
    return devices;
}

bool MidiDeviceManager::pairBleDevice(const std::string& address, const std::string& pin) {
#ifdef __linux__
    Logger::info("MidiDeviceManager", "Pairing BLE device: " + address);
    
    GError* error = nullptr;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    
    if (error) {
        Logger::error("MidiDeviceManager", "D-Bus connection failed: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    std::string objectPath = "/org/bluez/hci0/dev_" + address;
    std::replace(objectPath.begin(), objectPath.end(), ':', '_');
    
    GDBusProxy* deviceProxy = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        objectPath.c_str(),
        "org.bluez.Device1",
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("MidiDeviceManager", "Device proxy failed: " + std::string(error->message));
        g_error_free(error);
        g_object_unref(connection);
        return false;
    }
    
    GVariant* result = g_dbus_proxy_call_sync(
        deviceProxy,
        "Pair",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        30000,
        nullptr,
        &error
    );
    
    bool success = (error == nullptr);
    
    if (error) {
        Logger::error("MidiDeviceManager", "Pairing failed: " + std::string(error->message));
        g_error_free(error);
    } else {
        g_variant_unref(result);
        Logger::info("MidiDeviceManager", "✅ BLE device paired: " + address);
    }
    
    g_object_unref(deviceProxy);
    g_object_unref(connection);
    
    return success;
#else
    Logger::warning("MidiDeviceManager", "BLE pairing not supported on this platform");
    return false;
#endif
}

bool MidiDeviceManager::unpairBleDevice(const std::string& address) {
#ifdef __linux__
    Logger::info("MidiDeviceManager", "Unpairing BLE device: " + address);
    
    GError* error = nullptr;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    
    if (error) {
        g_error_free(error);
        return false;
    }
    
    GDBusProxy* adapterProxy = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        "/org/bluez/hci0",
        "org.bluez.Adapter1",
        nullptr,
        nullptr
    );
    
    if (!adapterProxy) {
        g_object_unref(connection);
        return false;
    }
    
    std::string objectPath = "/org/bluez/hci0/dev_" + address;
    std::replace(objectPath.begin(), objectPath.end(), ':', '_');
    
    GVariant* result = g_dbus_proxy_call_sync(
        adapterProxy,
        "RemoveDevice",
        g_variant_new("(o)", objectPath.c_str()),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    bool success = (error == nullptr);
    
    if (error) {
        Logger::error("MidiDeviceManager", "Unpairing failed: " + std::string(error->message));
        g_error_free(error);
    } else {
        g_variant_unref(result);
        Logger::info("MidiDeviceManager", "✅ BLE device unpaired: " + address);
    }
    
    g_object_unref(adapterProxy);
    g_object_unref(connection);
    
    return success;
#else
    Logger::warning("MidiDeviceManager", "BLE unpairing not supported on this platform");
    return false;
#endif
}

std::vector<MidiDeviceInfo> MidiDeviceManager::getPairedBleDevices() const {
    std::vector<MidiDeviceInfo> pairedDevices;
    
#ifdef __linux__
    Logger::info("MidiDeviceManager", "Retrieving paired BLE devices...");
    
    GError* error = nullptr;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    
    if (error) {
        g_error_free(error);
        return pairedDevices;
    }
    
    GDBusProxy* managerProxy = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        nullptr
    );
    
    if (!managerProxy) {
        g_object_unref(connection);
        return pairedDevices;
    }
    
    GVariant* result = g_dbus_proxy_call_sync(
        managerProxy,
        "GetManagedObjects",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        nullptr
    );
    
    if (result) {
        GVariantIter* iter;
        const gchar* objectPath;
        GVariant* ifacesAndProperties;
        
        g_variant_get(result, "(a{oa{sa{sv}}})", &iter);
        
        while (g_variant_iter_next(iter, "{&o@a{sa{sv}}}", &objectPath, &ifacesAndProperties)) {
            GVariant* deviceProps = nullptr;
            if (g_variant_lookup(ifacesAndProperties, "org.bluez.Device1", "@a{sv}", &deviceProps)) {
                gboolean paired = FALSE;
                g_variant_lookup(deviceProps, "Paired", "b", &paired);
                
                if (paired) {
                    GVariant* uuids = nullptr;
                    if (g_variant_lookup(deviceProps, "UUIDs", "@as", &uuids)) {
                        GVariantIter uuidIter;
                        const gchar* uuid;
                        bool hasBleMidi = false;
                        
                        g_variant_iter_init(&uuidIter, uuids);
                        while (g_variant_iter_next(&uuidIter, "&s", &uuid)) {
                            std::string uuidStr(uuid);
                            std::transform(uuidStr.begin(), uuidStr.end(), 
                                         uuidStr.begin(), ::tolower);
                            
                            if (uuidStr == "03b80e5a-ede8-4b33-a751-6ce34ec4c700") {
                                hasBleMidi = true;
                                break;
                            }
                        }
                        
                        if (hasBleMidi) {
                            const gchar* name = nullptr;
                            const gchar* address = nullptr;
                            gint16 rssi = 0;
                            
                            g_variant_lookup(deviceProps, "Name", "&s", &name);
                            g_variant_lookup(deviceProps, "Address", "&s", &address);
                            g_variant_lookup(deviceProps, "RSSI", "n", &rssi);
                            
                            if (address) {
                                MidiDeviceInfo info;
                                std::string addressStr(address);
                                std::string deviceId = "ble_" + addressStr;
                                std::replace(deviceId.begin(), deviceId.end(), ':', '_');
                                
                                info.id = deviceId;
                                info.name = name ? name : "BLE MIDI Device";
                                info.type = DeviceType::BLUETOOTH;
                                info.direction = DeviceDirection::BIDIRECTIONAL;
                                info.bluetoothAddress = addressStr;
                                info.objectPath = objectPath;
                                info.paired = true;
                                info.signalStrength = rssi;
                                info.available = true;
                                
                                pairedDevices.push_back(info);
                            }
                        }
                        
                        g_variant_unref(uuids);
                    }
                }
                
                g_variant_unref(deviceProps);
            }
            
            g_variant_unref(ifacesAndProperties);
        }
        
        g_variant_iter_free(iter);
        g_variant_unref(result);
    }
    
    g_object_unref(managerProxy);
    g_object_unref(connection);
    
    Logger::info("MidiDeviceManager", "✅ Found " + 
                std::to_string(pairedDevices.size()) + " paired BLE devices");
#else
    Logger::warning("MidiDeviceManager", "BLE not supported on this platform");
#endif
    
    return pairedDevices;
}

bool MidiDeviceManager::forgetBleDevice(const std::string& address) {
    Logger::info("MidiDeviceManager", "Forgetting BLE device: " + address);
    
    std::string deviceId = "ble_" + address;
    std::replace(deviceId.begin(), deviceId.end(), ':', '_');
    
    disconnect(deviceId);
    
    bool success = unpairBleDevice(address);
    
    if (success) {
        std::lock_guard<std::mutex> lock(mutex_);
        availableDevices_.erase(
            std::remove_if(availableDevices_.begin(), availableDevices_.end(),
                [&deviceId](const MidiDeviceInfo& info) {
                    return info.id == deviceId;
                }),
            availableDevices_.end()
        );
    }
    
    return success;
}

int MidiDeviceManager::getBleDeviceSignal(const std::string& deviceId) const {
#ifdef __linux__
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(devices_.begin(), devices_.end(),
                          [&deviceId](const std::shared_ptr<MidiDevice>& device) {
                              return device->getId() == deviceId && 
                                     device->getType() == DeviceType::BLUETOOTH;
                          });
    
    if (it == devices_.end()) {
        return 0;
    }
    
    auto bleDevice = std::dynamic_pointer_cast<BleMidiDevice>(*it);
    if (!bleDevice) {
        return 0;
    }
    
    GError* error = nullptr;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    
    if (error) {
        g_error_free(error);
        return 0;
    }
    
    std::string objectPath = bleDevice->getObjectPath();
    
    GDBusProxy* deviceProxy = g_dbus_proxy_new_sync(
        connection,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        objectPath.c_str(),
        "org.bluez.Device1",
        nullptr,
        nullptr
    );
    
    if (!deviceProxy) {
        g_object_unref(connection);
        return 0;
    }
    
    GVariant* rssiVariant = g_dbus_proxy_get_cached_property(deviceProxy, "RSSI");
    
    gint16 rssi = 0;
    if (rssiVariant) {
        rssi = g_variant_get_int16(rssiVariant);
        g_variant_unref(rssiVariant);
    }
    
    g_object_unref(deviceProxy);
    g_object_unref(connection);
    
    return static_cast<int>(rssi);
#else
    return 0;
#endif
}

std::shared_ptr<MidiDevice> MidiDeviceManager::createDevice(const MidiDeviceInfo& info) {
    Logger::debug("MidiDeviceManager", "Creating device: " + info.name);
    
    try {
        switch (info.type) {
            case DeviceType::USB: {
#ifdef __linux__
                size_t colonPos = info.port.find(':');
                if (colonPos == std::string::npos) {
                    Logger::error("MidiDeviceManager", "Invalid port format: " + info.port);
                    return nullptr;
                }
                
                int client = std::stoi(info.port.substr(0, colonPos));
                int port = std::stoi(info.port.substr(colonPos + 1));

                auto device = std::make_shared<UsbMidiDevice>(info.id, info.name, client, port);

                // Inject SysEx handler for auto-identification
                device->setSysExHandler(sysexHandler_);

                Logger::info("MidiDeviceManager", "✅ Created USB device: " + info.name);
                return device;
#else
                Logger::error("MidiDeviceManager", "USB devices not supported on this platform");
                return nullptr;
#endif
            }
            
            case DeviceType::BLUETOOTH: {
#ifdef __linux__
                auto device = std::make_shared<BleMidiDevice>(
                    info.id,
                    info.name,
                    info.bluetoothAddress,
                    info.objectPath
                );
                Logger::info("MidiDeviceManager", "✅ Created BLE device: " + info.name);
                return device;
#else
                Logger::error("MidiDeviceManager", "BLE devices not supported on this platform");
                return nullptr;
#endif
            }
            
            case DeviceType::VIRTUAL: {
                auto device = std::make_shared<VirtualMidiDevice>(info.id, info.name);
                Logger::info("MidiDeviceManager", "✅ Created virtual device: " + info.name);
                return device;
            }
            
            case DeviceType::NETWORK:
                Logger::warning("MidiDeviceManager", "Network devices not yet supported");
                return nullptr;
            
            default:
                Logger::error("MidiDeviceManager", "Unknown device type");
                return nullptr;
        }
    } catch (const std::exception& e) {
        Logger::error("MidiDeviceManager", 
            "Exception creating device: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace midiMind
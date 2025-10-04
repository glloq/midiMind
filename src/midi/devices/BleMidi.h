// ============================================================================
// src/midi/devices/plugins/BleMidiPlugin.h
// Bluetooth Low Energy MIDI - Standard iOS/Android compatible
// ============================================================================
#pragma once
#include "../DevicePlugin.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

namespace midiMind {

// ============================================================================
// BLE MIDI SPECIFICATION (Official)
// ============================================================================

// Service UUID: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700
constexpr uint8_t BLE_MIDI_SERVICE_UUID[] = {
    0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7,
    0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03
};

// Characteristic UUID: 7772E5DB-3868-4112-A1A9-F2669D106BF3
constexpr uint8_t BLE_MIDI_CHAR_UUID[] = {
    0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1,
    0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77
};

// ============================================================================
// BLE SCANNER
// ============================================================================

class BleScanner {
public:
    struct BleDevice {
        std::string address;
        std::string name;
        int rssi;
        bool isMidiDevice;
    };
    
    static std::vector<BleDevice> scanDevices(int durationSeconds = 10) {
        std::vector<BleDevice> devices;
        
        int deviceId = hci_get_route(NULL);
        if (deviceId < 0) {
            Logger::error("BLE", "No Bluetooth adapter found");
            return devices;
        }
        
        int sock = hci_open_dev(deviceId);
        if (sock < 0) {
            Logger::error("BLE", "Failed to open HCI socket");
            return devices;
        }
        
        Logger::info("BLE", "Scanning for BLE MIDI devices...");
        
        // Configuration du scan BLE
        uint8_t scan_type = 0x01; // Active scan
        uint16_t interval = htobs(0x0010); // 10ms
        uint16_t window = htobs(0x0010);
        uint8_t own_type = LE_PUBLIC_ADDRESS;
        uint8_t filter_policy = 0x00; // Accept all
        
        if (hci_le_set_scan_parameters(sock, scan_type, interval, window,
                                       own_type, filter_policy, 1000) < 0) {
            Logger::error("BLE", "Failed to set scan parameters");
            close(sock);
            return devices;
        }
        
        // Démarrer scan
        if (hci_le_set_scan_enable(sock, 0x01, 0x00, 1000) < 0) {
            Logger::error("BLE", "Failed to enable scan");
            close(sock);
            return devices;
        }
        
        // Collecter résultats pendant durationSeconds
        auto endTime = std::chrono::steady_clock::now() + 
                      std::chrono::seconds(durationSeconds);
        
        std::set<std::string> seenAddresses;
        
        while (std::chrono::steady_clock::now() < endTime) {
            unsigned char buf[HCI_MAX_EVENT_SIZE];
            int len = read(sock, buf, sizeof(buf));
            
            if (len >= HCI_EVENT_HDR_SIZE) {
                evt_le_meta_event* meta = (evt_le_meta_event*)(buf + HCI_EVENT_HDR_SIZE + 1);
                
                if (meta->subevent == EVT_LE_ADVERTISING_REPORT) {
                    le_advertising_info* info = (le_advertising_info*)(meta->data + 1);
                    
                    char addr[18];
                    ba2str(&info->bdaddr, addr);
                    std::string address(addr);
                    
                    // Éviter doublons
                    if (seenAddresses.find(address) != seenAddresses.end()) {
                        continue;
                    }
                    seenAddresses.insert(address);
                    
                    BleDevice device;
                    device.address = address;
                    device.rssi = (int8_t)info->data[info->length];
                    device.isMidiDevice = false;
                    
                    // Parser advertising data pour nom et services
                    uint8_t* data = info->data;
                    uint8_t dataLen = info->length;
                    
                    for (int i = 0; i < dataLen;) {
                        uint8_t len = data[i];
                        if (len == 0) break;
                        
                        uint8_t type = data[i + 1];
                        
                        // Type 0x09 = Complete Local Name
                        if (type == 0x09 && len > 1) {
                            device.name = std::string((char*)&data[i + 2], len - 1);
                        }
                        
                        // Type 0x07 = Complete 128-bit Service UUIDs
                        if (type == 0x07 && len == 17) {
                            // Vérifier si c'est le service MIDI
                            if (memcmp(&data[i + 2], BLE_MIDI_SERVICE_UUID, 16) == 0) {
                                device.isMidiDevice = true;
                            }
                        }
                        
                        i += len + 1;
                    }
                    
                    if (device.name.empty()) {
                        device.name = "Unknown BLE Device";
                    }
                    
                    devices.push_back(device);
                    
                    if (device.isMidiDevice) {
                        Logger::info("BLE", "✓ Found MIDI device: " + device.name + 
                                   " (" + address + ")");
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Arrêter scan
        hci_le_set_scan_enable(sock, 0x00, 0x00, 1000);
        close(sock);
        
        return devices;
    }
};

// ============================================================================
// BLE MIDI PLUGIN
// ============================================================================

class BleMidiPlugin : public IDevicePlugin {
public:
    std::string getName() const override { return "BLE MIDI"; }
    std::string getVersion() const override { return "2.0.0"; }
    DeviceType getType() const override { return DeviceType::BLUETOOTH; }
    
    bool supportsDiscovery() const override { return true; }
    bool supportsHotplug() const override { return true; }
    
    bool initialize() override {
        // Vérifier BlueZ version
        int deviceId = hci_get_route(NULL);
        if (deviceId < 0) {
            Logger::error("BlePlugin", "No Bluetooth adapter found");
            return false;
        }
        
        Logger::info("BlePlugin", "✓ BLE MIDI plugin initialized");
        return true;
    }
    
    void shutdown() override {
        Logger::info("BlePlugin", "BLE MIDI plugin shutdown");
    }
    
    std::vector<DeviceInfo> discover() override {
        std::vector<DeviceInfo> devices;
        
        auto bleDevices = BleScanner::scanDevices(10);
        
        for (const auto& bleDevice : bleDevices) {
            if (!bleDevice.isMidiDevice) continue;
            
            DeviceInfo info;
            info.id = "ble_" + bleDevice.address;
            info.name = bleDevice.name;
            info.type = DeviceType::BLUETOOTH;
            info.metadata["bt_address"] = bleDevice.address;
            info.metadata["rssi"] = bleDevice.rssi;
            info.metadata["protocol"] = "BLE-MIDI";
            
            devices.push_back(info);
        }
        
        Logger::info("BlePlugin", "Found " + std::to_string(devices.size()) + 
                    " BLE MIDI devices");
        
        return devices;
    }
    
    std::shared_ptr<MidiDevice> createDevice(const DeviceInfo& info) override {
        std::string address = info.metadata.value("bt_address", "");
        
        if (address.empty()) {
            Logger::error("BlePlugin", "Invalid BLE device info");
            return nullptr;
        }
        
        return std::make_shared<BleMidiDevice>(info.id, info.name, address);
    }
};

// ============================================================================
// BLE MIDI DEVICE
// ============================================================================

class BleMidiDevice : public MidiDevice {
public:
    BleMidiDevice(const std::string& id, const std::string& name,
                  const std::string& address)
        : MidiDevice(id, name, DeviceType::BLUETOOTH),
          btAddress_(address), gattHandle_(-1) {
    }

    ~BleMidiDevice() {
        disconnect();
    }

    bool connect() override {
        setStatus(DeviceStatus::CONNECTING);
        
        // Ouvrir connexion GATT vers le device BLE
        bdaddr_t addr;
        str2ba(btAddress_.c_str(), &addr);
        
        // TODO: Utiliser bluez D-Bus API pour connexion GATT moderne
        // Pour l'instant, utiliser l'ancien API direct
        
        int deviceId = hci_get_route(NULL);
        if (deviceId < 0) {
            setStatus(DeviceStatus::ERROR);
            return false;
        }
        
        int sock = hci_open_dev(deviceId);
        if (sock < 0) {
            setStatus(DeviceStatus::ERROR);
            return false;
        }
        
        // Créer connexion LE
        uint16_t handle;
        if (hci_le_create_conn(sock, 0x0060, 0x0030, 0x00, 0x00,
                              &addr, 0x00, 0x0006, 0x000C,
                              0x0000, 0x00C8, 0x0004, 0x0006,
                              &handle, 25000) < 0) {
            Logger::error("BleDevice", "Failed to create LE connection");
            close(sock);
            setStatus(DeviceStatus::ERROR);
            return false;
        }
        
        gattHandle_ = handle;
        gattSocket_ = sock;
        
        // Découvrir le service et characteristic MIDI
        if (!discoverMidiCharacteristic()) {
            disconnect();
            return false;
        }
        
        setStatus(DeviceStatus::CONNECTED);
        Logger::info("BleDevice", "✓ Connected to " + name_);
        
        return true;
    }

    void disconnect() override {
        if (gattSocket_ >= 0) {
            close(gattSocket_);
            gattSocket_ = -1;
            gattHandle_ = -1;
            setStatus(DeviceStatus::DISCONNECTED);
        }
    }

    bool sendMessage(const MidiMessage& msg) override {
        if (!isConnected()) return false;

        // Encapsuler message MIDI dans format BLE MIDI
        std::vector<uint8_t> blePacket = encapsulateBleMidi(msg);
        
        // Écrire vers characteristic GATT
        // TODO: Utiliser att_write ou gatt_write_char
        
        return true;
    }

private:
    bool discoverMidiCharacteristic() {
        // TODO: Scanner GATT services pour trouver UUID MIDI
        // Pour l'instant, stub
        Logger::info("BleDevice", "Discovering MIDI characteristic...");
        return true;
    }
    
    std::vector<uint8_t> encapsulateBleMidi(const MidiMessage& msg) {
        // Format BLE MIDI: Header + Timestamp + MIDI data
        std::vector<uint8_t> packet;
        
        // Header byte (bit 7 = 1)
        uint8_t header = 0x80;
        packet.push_back(header);
        
        // Timestamp (13 bits, milliseconds)
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() & 0x1FFF;
        
        packet.push_back(0x80 | ((ms >> 7) & 0x3F));
        packet.push_back(0x80 | (ms & 0x7F));
        
        // MIDI data
        const auto& midiData = msg.getData();
        packet.insert(packet.end(), midiData.begin(), midiData.end());
        
        return packet;
    }

    std::string btAddress_;
    int gattSocket_;
    int gattHandle_;
    uint16_t midiCharHandle_;
};

// Auto-registration
REGISTER_DEVICE_PLUGIN(BleMidiPlugin);

} // namespace midiMind

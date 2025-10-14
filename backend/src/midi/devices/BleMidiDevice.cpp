// ============================================================================
// Fichier: backend/src/midi/devices/BleMidiDevice.cpp
// Version: 3.1.1
// Date: 13 Octobre 2025
// Corrections: Retrait des stubs, implémentation complète BlueZ
// ============================================================================

#include "BleMidiDevice.h"
#include "../../core/Logger.h"
#include "../../core/Error.h"

#ifdef HAS_BLUEZ
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <gio/gio.h>
#endif

namespace midiMind {

// ============================================================================
// CONSTANTES BLUEZ
// ============================================================================

namespace BleMidi {
    constexpr const char* BLUEZ_SERVICE = "org.bluez";
    constexpr const char* ADAPTER_INTERFACE = "org.bluez.Adapter1";
    constexpr const char* DEVICE_INTERFACE = "org.bluez.Device1";
    constexpr const char* GATT_SERVICE_INTERFACE = "org.bluez.GattService1";
    constexpr const char* GATT_CHARACTERISTIC_INTERFACE = "org.bluez.GattCharacteristic1";
    
    // UUID MIDI BLE Service
    constexpr const char* MIDI_SERVICE_UUID = "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
    
    // UUID MIDI BLE Characteristics
    constexpr const char* MIDI_IO_CHARACTERISTIC_UUID = "7772e5db-3868-4112-a1a9-f2669d106bf3";
}

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR
// ============================================================================

BleMidiDevice::BleMidiDevice(const std::string& id, const std::string& name, 
                             const std::string& address)
    : MidiDevice(id, name, DeviceType::BLUETOOTH)
    , address_(address)
    , dbusConnection_(nullptr)
    , deviceProxy_(nullptr)
    , ioCharacteristic_(nullptr)
    , running_(false) {
    
    Logger::info("BleMidiDevice", "Device created: " + name + " (" + address + ")");
}

BleMidiDevice::~BleMidiDevice() {
    disconnect();
    
    // Libérer ressources D-Bus
#ifdef HAS_BLUEZ
    if (ioCharacteristic_) {
        g_object_unref(ioCharacteristic_);
    }
    if (deviceProxy_) {
        g_object_unref(deviceProxy_);
    }
    if (dbusConnection_) {
        g_object_unref(dbusConnection_);
    }
#endif
    
    Logger::info("BleMidiDevice", "Device destroyed: " + name_);
}

// ============================================================================
// CONNEXION / DÉCONNEXION
// ============================================================================

bool BleMidiDevice::connect() {
    if (isConnected()) {
        Logger::warn("BleMidiDevice", "Already connected");
        return true;
    }
    
    Logger::info("BleMidiDevice", "Connecting to " + address_);
    
#ifdef HAS_BLUEZ
    // 1. Initialiser BlueZ si nécessaire
    if (!dbusConnection_ && !initializeBluez()) {
        Logger::error("BleMidiDevice", "Failed to initialize BlueZ");
        return false;
    }
    
    // 2. Vérifier adaptateur Bluetooth
    if (!isBluetoothAvailable()) {
        Logger::error("BleMidiDevice", "No Bluetooth adapter found");
        return false;
    }
    
    // 3. Obtenir proxy du device
    if (!deviceProxy_) {
        std::string devicePath = getDevicePath(address_);
        GError* error = nullptr;
        
        deviceProxy_ = g_dbus_proxy_new_sync(
            dbusConnection_,
            G_DBUS_PROXY_FLAGS_NONE,
            nullptr,
            BleMidi::BLUEZ_SERVICE,
            devicePath.c_str(),
            BleMidi::DEVICE_INTERFACE,
            nullptr,
            &error
        );
        
        if (!deviceProxy_) {
            Logger::error("BleMidiDevice", "Device not found: " + address_ + 
                         (error ? (" - " + std::string(error->message)) : ""));
            if (error) g_error_free(error);
            return false;
        }
    }
    
    // 4. Vérifier si déjà connecté à bas niveau
    GVariant* connectedVariant = g_dbus_proxy_get_cached_property(deviceProxy_, "Connected");
    if (connectedVariant) {
        bool alreadyConnected = g_variant_get_boolean(connectedVariant);
        g_variant_unref(connectedVariant);
        
        if (!alreadyConnected) {
            // Connecter via D-Bus
            GError* error = nullptr;
            GVariant* result = g_dbus_proxy_call_sync(
                deviceProxy_,
                "Connect",
                nullptr,
                G_DBUS_CALL_FLAGS_NONE,
                30000, // 30s timeout
                nullptr,
                &error
            );
            
            if (!result) {
                Logger::error("BleMidiDevice", "Connection failed: " + 
                             std::string(error ? error->message : "unknown"));
                if (error) g_error_free(error);
                return false;
            }
            
            g_variant_unref(result);
        }
    }
    
    // 5. Découvrir services et caractéristiques GATT MIDI
    if (!discoverGattCharacteristics()) {
        Logger::error("BleMidiDevice", "Failed to discover MIDI GATT characteristics");
        return false;
    }
    
    // 6. Démarrer thread de lecture BLE
    running_ = true;
    bleThread_ = std::thread([this]() { bleLoop(); });
    
    setStatus(DeviceStatus::CONNECTED);
    Logger::info("BleMidiDevice", "✓ Connected to " + name_);
    
    return true;
    
#else
    // PAS DE STUB - Retourner false si BlueZ absent
    Logger::error("BleMidiDevice", 
                 "BlueZ support not compiled (HAS_BLUEZ undefined). "
                 "Recompile with -DHAS_BLUEZ to enable Bluetooth MIDI.");
    return false;
#endif
}

void BleMidiDevice::disconnect() {
    if (!isConnected()) {
        return;
    }
    
    Logger::info("BleMidiDevice", "Disconnecting from " + address_);
    
    // Arrêter thread
    running_ = false;
    if (bleThread_.joinable()) {
        bleThread_.join();
    }
    
#ifdef HAS_BLUEZ
    // Déconnecter via D-Bus
    if (deviceProxy_) {
        GError* error = nullptr;
        GVariant* result = g_dbus_proxy_call_sync(
            deviceProxy_,
            "Disconnect",
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            5000,
            nullptr,
            &error
        );
        
        if (result) {
            g_variant_unref(result);
        }
        
        if (error) {
            Logger::warn("BleMidiDevice", "Disconnect error: " + std::string(error->message));
            g_error_free(error);
        }
    }
    
    // Libérer caractéristique
    if (ioCharacteristic_) {
        g_object_unref(ioCharacteristic_);
        ioCharacteristic_ = nullptr;
    }
#endif
    
    setStatus(DeviceStatus::DISCONNECTED);
    Logger::info("BleMidiDevice", "Disconnected from " + name_);
}

// ============================================================================
// ENVOI DE MESSAGES
// ============================================================================

bool BleMidiDevice::sendMessage(const MidiMessage& msg) {
    if (!isConnected()) {
        Logger::warn("BleMidiDevice", "Cannot send: not connected");
        return false;
    }
    
#ifdef HAS_BLUEZ
    if (!ioCharacteristic_) {
        Logger::error("BleMidiDevice", "MIDI I/O characteristic not available");
        return false;
    }
    
    try {
        // Encapsuler message MIDI en format BLE MIDI
        std::vector<uint8_t> blePacket = encodeMidiToBle(msg);
        
        // Écrire via GATT characteristic
        GError* error = nullptr;
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
        
        for (uint8_t byte : blePacket) {
            g_variant_builder_add(&builder, "y", byte);
        }
        
        GVariant* value = g_variant_builder_end(&builder);
        GVariant* result = g_dbus_proxy_call_sync(
            ioCharacteristic_,
            "WriteValue",
            g_variant_new("(@ay@a{sv})", value, g_variant_new_array(G_VARIANT_TYPE("{sv}"), nullptr, 0)),
            G_DBUS_CALL_FLAGS_NONE,
            1000, // 1s timeout
            nullptr,
            &error
        );
        
        if (!result) {
            Logger::error("BleMidiDevice", "Write failed: " + 
                         std::string(error ? error->message : "unknown"));
            if (error) g_error_free(error);
            return false;
        }
        
        g_variant_unref(result);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("BleMidiDevice", "Send exception: " + std::string(e.what()));
        return false;
    }
#else
    Logger::error("BleMidiDevice", "BlueZ support not compiled");
    return false;
#endif
}

// ============================================================================
// MÉTHODES PRIVÉES - BLUEZ
// ============================================================================

#ifdef HAS_BLUEZ

bool BleMidiDevice::initializeBluez() {
    Logger::info("BleMidiDevice", "Initializing BlueZ...");
    
    GError* error = nullptr;
    
    // Connexion au bus D-Bus système
    dbusConnection_ = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (!dbusConnection_) {
        Logger::error("BleMidiDevice", "Failed to connect to D-Bus: " + 
                     std::string(error ? error->message : "unknown"));
        if (error) g_error_free(error);
        return false;
    }
    
    // Vérifier que BlueZ est disponible
    GDBusProxy* managerProxy = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BleMidi::BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (!managerProxy) {
        Logger::error("BleMidiDevice", "BlueZ not available: " + 
                     std::string(error ? error->message : "unknown"));
        if (error) g_error_free(error);
        return false;
    }
    
    g_object_unref(managerProxy);
    
    Logger::info("BleMidiDevice", "✓ BlueZ initialized");
    return true;
}

bool BleMidiDevice::discoverGattCharacteristics() {
    Logger::info("BleMidiDevice", "Discovering GATT MIDI characteristics...");
    
    // Obtenir tous les objets gérés par BlueZ
    GError* error = nullptr;
    GDBusProxy* objectManagerProxy = g_dbus_proxy_new_sync(
        dbusConnection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BleMidi::BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (!objectManagerProxy) {
        Logger::error("BleMidiDevice", "Failed to get ObjectManager");
        if (error) g_error_free(error);
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
    
    if (!result) {
        Logger::error("BleMidiDevice", "Failed to get managed objects");
        if (error) g_error_free(error);
        return false;
    }
    
    // Parser les objets pour trouver la caractéristique MIDI
    // Format: a{oa{sa{sv}}}
    GVariantIter* objects;
    g_variant_get(result, "(a{oa{sa{sv}}})", &objects);
    
    const gchar* objectPath;
    GVariantIter* interfaces;
    bool found = false;
    
    while (g_variant_iter_loop(objects, "{&oa{sa{sv}}}", &objectPath, &interfaces)) {
        const gchar* interfaceName;
        GVariantIter* properties;
        
        while (g_variant_iter_loop(interfaces, "{&sa{sv}}", &interfaceName, &properties)) {
            if (g_strcmp0(interfaceName, BleMidi::GATT_CHARACTERISTIC_INTERFACE) == 0) {
                // Vérifier UUID
                const gchar* propName;
                GVariant* propValue;
                
                while (g_variant_iter_loop(properties, "{&sv}", &propName, &propValue)) {
                    if (g_strcmp0(propName, "UUID") == 0) {
                        const gchar* uuid = g_variant_get_string(propValue, nullptr);
                        
                        if (g_strcmp0(uuid, BleMidi::MIDI_IO_CHARACTERISTIC_UUID) == 0) {
                            // Trouvé !
                            Logger::info("BleMidiDevice", "Found MIDI I/O characteristic: " + 
                                       std::string(objectPath));
                            
                            ioCharacteristic_ = g_dbus_proxy_new_sync(
                                dbusConnection_,
                                G_DBUS_PROXY_FLAGS_NONE,
                                nullptr,
                                BleMidi::BLUEZ_SERVICE,
                                objectPath,
                                BleMidi::GATT_CHARACTERISTIC_INTERFACE,
                                nullptr,
                                nullptr
                            );
                            
                            found = true;
                        }
                    }
                }
            }
        }
        
        if (found) break;
    }
    
    g_variant_iter_free(objects);
    g_variant_unref(result);
    
    if (!found || !ioCharacteristic_) {
        Logger::error("BleMidiDevice", "MIDI GATT characteristic not found");
        return false;
    }
    
    // Activer notifications pour la caractéristique
    GVariant* startNotifyResult = g_dbus_proxy_call_sync(
        ioCharacteristic_,
        "StartNotify",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (!startNotifyResult) {
        Logger::warn("BleMidiDevice", "Failed to start notifications: " +
                    std::string(error ? error->message : "unknown"));
        if (error) g_error_free(error);
        // Non critique
    } else {
        g_variant_unref(startNotifyResult);
    }
    
    Logger::info("BleMidiDevice", "✓ GATT characteristics discovered");
    return true;
}

std::string BleMidiDevice::getDevicePath(const std::string& address) const {
    // Convertir adresse MAC en chemin D-Bus BlueZ
    // Format: /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF
    
    std::string path = "/org/bluez/hci0/dev_";
    std::string addressCopy = address;
    
    // Remplacer ':' par '_'
    for (char& c : addressCopy) {
        if (c == ':') c = '_';
    }
    
    path += addressCopy;
    return path;
}

std::vector<uint8_t> BleMidiDevice::encodeMidiToBle(const MidiMessage& msg) const {
    // BLE MIDI utilise un format spécial avec timestamp
    // Format: [header][timestamp_high][timestamp_low][midi_bytes...]
    
    std::vector<uint8_t> packet;
    
    // Header avec timestamp running
    uint16_t timestamp = static_cast<uint16_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count() & 0x1FFF // 13 bits
    );
    
    uint8_t header = 0x80 | ((timestamp >> 7) & 0x3F);
    uint8_t timestampLow = 0x80 | (timestamp & 0x7F);
    
    packet.push_back(header);
    packet.push_back(timestampLow);
    
    // Ajouter bytes MIDI
    const auto& midiData = msg.getData();
    packet.insert(packet.end(), midiData.begin(), midiData.end());
    
    return packet;
}

void BleMidiDevice::bleLoop() {
    Logger::info("BleMidiDevice", "BLE loop started");
    
    // Créer GMainLoop pour traiter les événements D-Bus
    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    GMainContext* context = g_main_loop_get_context(loop);
    
    while (running_) {
        // Traiter événements D-Bus (notifications GATT)
        g_main_context_iteration(context, FALSE);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    g_main_loop_unref(loop);
    
    Logger::info("BleMidiDevice", "BLE loop stopped");
}

#endif // HAS_BLUEZ

// ============================================================================
// STATIC - VÉRIFICATION DISPONIBILITÉ
// ============================================================================

bool BleMidiDevice::isBluetoothAvailable() {
#ifdef HAS_BLUEZ
    // Vérifier adaptateur via hci
    int devId = hci_get_route(nullptr);
    if (devId < 0) {
        Logger::warn("BleMidiDevice", "No Bluetooth adapter found");
        return false;
    }
    
    int sock = hci_open_dev(devId);
    if (sock < 0) {
        Logger::warn("BleMidiDevice", "Cannot open Bluetooth adapter");
        return false;
    }
    
    hci_close_dev(sock);
    Logger::info("BleMidiDevice", "✓ Bluetooth adapter available (hci" + 
                std::to_string(devId) + ")");
    return true;
#else
    Logger::error("BleMidiDevice", "BlueZ support not compiled");
    return false;
#endif
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER BleMidiDevice.cpp v3.1.1
// ============================================================================
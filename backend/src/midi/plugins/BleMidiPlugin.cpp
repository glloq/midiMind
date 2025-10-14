// ============================================================================
// Fichier: backend/src/midi/devices/plugins/BleMidiPlugin.cpp
// Version: 2.0.0 - ADAPTÉ À DEVICEPLUGIN.H
// Date: 2025-10-13
// ============================================================================
// Description:
//   Implémentation du plugin BLE MIDI avec BlueZ via D-Bus.
//   ✅ Compatible avec DevicePlugin.h (IDevicePlugin interface)
//   ✅ Auto-registration via REGISTER_DEVICE_PLUGIN
//   ✅ DeviceInfo avec metadata JSON
//
// Changements v2.0.0:
//   - Adaptation à IDevicePlugin de DevicePlugin.h
//   - Utilisation de metadata JSON pour DeviceInfo
//   - Auto-registration automatique
//   - Suppression des champs directs (address, connected, etc.)
//
// Auteur: MidiMind Team (Généré par Claude)
// ============================================================================

#include "BleMidiPlugin.h"
#include "../../../core/Logger.h"
#include <gio/gio.h>
#include <algorithm>
#include <thread>
#include <chrono>

namespace midiMind {

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR
// ============================================================================

BleMidiPlugin::BleMidiPlugin()
    : dbus_connection_(nullptr)
    , initialized_(false)
    , scanning_(false)
    , scan_timeout_(5)
    , verbose_(false)
{
    Logger::info("BleMidiPlugin", "BleMidiPlugin v2.0.0 created (adapted to DevicePlugin.h)");
}

BleMidiPlugin::~BleMidiPlugin() {
    if (initialized_) {
        shutdown();
    }
}

// ============================================================================
// INTERFACE IDevicePlugin
// ============================================================================

std::string BleMidiPlugin::getName() const {
    return "BLE MIDI";
}

std::string BleMidiPlugin::getVersion() const {
    return "2.0.0";
}

DeviceType BleMidiPlugin::getType() const {
    return DeviceType::BLUETOOTH;
}

bool BleMidiPlugin::supportsDiscovery() const {
    return true;
}

bool BleMidiPlugin::supportsHotplug() const {
    return true;
}

// ============================================================================
// INITIALISATION
// ============================================================================

bool BleMidiPlugin::initialize() {
    if (initialized_) {
        Logger::warn("BleMidiPlugin", "Already initialized");
        return true;
    }
    
    Logger::info("BleMidiPlugin", "Initializing BLE MIDI plugin...");
    
    // Étape 1: Connexion à D-Bus system bus
    GError* error = nullptr;
    dbus_connection_ = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "Failed to connect to D-Bus: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    if (!dbus_connection_) {
        Logger::error("BleMidiPlugin", "Failed to get D-Bus connection");
        return false;
    }
    
    Logger::info("BleMidiPlugin", "✓ Connected to D-Bus");
    
    // Étape 2: Vérifier que BlueZ est disponible
    GDBusProxy* proxy = g_dbus_proxy_new_sync(
        dbus_connection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "BlueZ not available: " + std::string(error->message));
        g_error_free(error);
        g_object_unref(dbus_connection_);
        dbus_connection_ = nullptr;
        return false;
    }
    
    g_object_unref(proxy);
    Logger::info("BleMidiPlugin", "✓ BlueZ detected");
    
    // Étape 3: Récupérer l'adaptateur Bluetooth par défaut
    if (!getDefaultAdapter()) {
        Logger::error("BleMidiPlugin", "No Bluetooth adapter found");
        g_object_unref(dbus_connection_);
        dbus_connection_ = nullptr;
        return false;
    }
    
    Logger::info("BleMidiPlugin", 
        "✓ Using adapter: " + adapter_name_ + " (" + adapter_address_ + ")");
    
    initialized_ = true;
    Logger::info("BleMidiPlugin", "✓ BleMidiPlugin initialized successfully");
    
    return true;
}

void BleMidiPlugin::shutdown() {
    if (!initialized_) {
        return;
    }
    
    Logger::info("BleMidiPlugin", "Shutting down BLE MIDI plugin...");
    
    // Arrêter le scan si en cours
    if (scanning_) {
        stopScan();
    }
    
    // Libérer la connexion D-Bus
    if (dbus_connection_) {
        g_object_unref(dbus_connection_);
        dbus_connection_ = nullptr;
    }
    
    initialized_ = false;
    Logger::info("BleMidiPlugin", "✓ BleMidiPlugin shutdown complete");
}

// ============================================================================
// DÉCOUVERTE
// ============================================================================

std::vector<DeviceInfo> BleMidiPlugin::discover() {
    if (!initialized_) {
        Logger::error("BleMidiPlugin", "Plugin not initialized");
        return {};
    }
    
    Logger::info("BleMidiPlugin", 
        "Starting BLE MIDI discovery (timeout: " + 
        std::to_string(scan_timeout_) + "s)...");
    
    // Étape 1: Démarrer le scan
    if (!startScan()) {
        Logger::error("BleMidiPlugin", "Failed to start BLE scan");
        return {};
    }
    
    // Étape 2: Attendre le timeout
    std::this_thread::sleep_for(std::chrono::seconds(scan_timeout_));
    
    // Étape 3: Arrêter le scan
    stopScan();
    
    // Étape 4: Récupérer les devices découverts
    auto devices = getDiscoveredDevices();
    
    Logger::info("BleMidiPlugin", 
        "✓ Discovery complete: " + std::to_string(devices.size()) + 
        " BLE MIDI devices found");
    
    return devices;
}

std::shared_ptr<MidiDevice> BleMidiPlugin::createDevice(const DeviceInfo& info) {
    // Pour l'instant, la création est gérée par MidiDeviceManager
    // qui appelle BleMidiDevice directement
    Logger::info("BleMidiPlugin", 
        "createDevice() called for: " + info.name + " (delegated to MidiDeviceManager)");
    return nullptr;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void BleMidiPlugin::setScanTimeout(int seconds) {
    if (seconds < 1) seconds = 1;
    if (seconds > 30) seconds = 30;
    
    scan_timeout_ = seconds;
    
    Logger::info("BleMidiPlugin", 
        "Scan timeout set to " + std::to_string(seconds) + "s");
}

void BleMidiPlugin::setVerbose(bool verbose) {
    verbose_ = verbose;
}

bool BleMidiPlugin::isInitialized() const {
    return initialized_;
}

std::string BleMidiPlugin::getAdapterName() const {
    return adapter_name_;
}

std::string BleMidiPlugin::getAdapterAddress() const {
    return adapter_address_;
}

// ============================================================================
// MÉTHODES PRIVÉES - BLUEZ
// ============================================================================

bool BleMidiPlugin::getDefaultAdapter() {
    GError* error = nullptr;
    
    // Créer proxy pour ObjectManager
    GDBusProxy* manager_proxy = g_dbus_proxy_new_sync(
        dbus_connection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "Failed to create ObjectManager proxy: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    // Appeler GetManagedObjects
    GVariant* result = g_dbus_proxy_call_sync(
        manager_proxy,
        "GetManagedObjects",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(manager_proxy);
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "Failed to get managed objects: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    // Parser le résultat pour trouver le premier adaptateur
    GVariantIter* iter;
    const gchar* object_path;
    GVariant* ifaces_and_properties;
    
    g_variant_get(result, "(a{oa{sa{sv}}})", &iter);
    
    bool found = false;
    
    while (g_variant_iter_next(iter, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties)) {
        // Vérifier si c'est un adaptateur
        if (g_variant_lookup(ifaces_and_properties, ADAPTER_INTERFACE, "*", nullptr)) {
            adapter_path_ = object_path;
            
            // Extraire le nom de l'adaptateur (ex: "/org/bluez/hci0" -> "hci0")
            std::string path_str(object_path);
            size_t last_slash = path_str.find_last_of('/');
            if (last_slash != std::string::npos) {
                adapter_name_ = path_str.substr(last_slash + 1);
            }
            
            // Récupérer l'adresse MAC
            GVariant* props = nullptr;
            if (g_variant_lookup(ifaces_and_properties, ADAPTER_INTERFACE, "@a{sv}", &props)) {
                adapter_address_ = getStringProperty(props, "Address");
                g_variant_unref(props);
            }
            
            found = true;
            g_variant_unref(ifaces_and_properties);
            break;
        }
        
        g_variant_unref(ifaces_and_properties);
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    
    return found;
}

bool BleMidiPlugin::startScan() {
    if (scanning_) {
        Logger::warn("BleMidiPlugin", "Scan already in progress");
        return true;
    }
    
    GError* error = nullptr;
    
    // Créer proxy pour l'adaptateur
    GDBusProxy* adapter_proxy = g_dbus_proxy_new_sync(
        dbus_connection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        adapter_path_.c_str(),
        ADAPTER_INTERFACE,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "Failed to create adapter proxy: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    // Configurer les filtres de scan (optionnel)
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    
    // Filtrer par UUIDs (service BLE MIDI)
    GVariantBuilder uuids_builder;
    g_variant_builder_init(&uuids_builder, G_VARIANT_TYPE("as"));
    g_variant_builder_add(&uuids_builder, "s", BLE_MIDI_SERVICE_UUID);
    
    g_variant_builder_add(&builder, "{sv}", "UUIDs", 
        g_variant_builder_end(&uuids_builder));
    
    GVariant* filters = g_variant_builder_end(&builder);
    
    // Appeler SetDiscoveryFilter
    GVariant* filter_result = g_dbus_proxy_call_sync(
        adapter_proxy,
        "SetDiscoveryFilter",
        g_variant_new("(@a{sv})", filters),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    if (error) {
        if (verbose_) {
            Logger::warn("BleMidiPlugin", 
                "SetDiscoveryFilter failed (continuing anyway): " + 
                std::string(error->message));
        }
        g_error_free(error);
        error = nullptr;
    } else {
        g_variant_unref(filter_result);
    }
    
    // Appeler StartDiscovery
    GVariant* start_result = g_dbus_proxy_call_sync(
        adapter_proxy,
        "StartDiscovery",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(adapter_proxy);
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "Failed to start discovery: " + std::string(error->message));
        g_error_free(error);
        return false;
    }
    
    g_variant_unref(start_result);
    
    scanning_ = true;
    
    if (verbose_) {
        Logger::info("BleMidiPlugin", "✓ BLE scan started");
    }
    
    return true;
}

void BleMidiPlugin::stopScan() {
    if (!scanning_) {
        return;
    }
    
    GError* error = nullptr;
    
    // Créer proxy pour l'adaptateur
    GDBusProxy* adapter_proxy = g_dbus_proxy_new_sync(
        dbus_connection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        adapter_path_.c_str(),
        ADAPTER_INTERFACE,
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "Failed to create adapter proxy: " + std::string(error->message));
        g_error_free(error);
        return;
    }
    
    // Appeler StopDiscovery
    GVariant* result = g_dbus_proxy_call_sync(
        adapter_proxy,
        "StopDiscovery",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(adapter_proxy);
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "Failed to stop discovery: " + std::string(error->message));
        g_error_free(error);
        return;
    }
    
    g_variant_unref(result);
    
    scanning_ = false;
    
    if (verbose_) {
        Logger::info("BleMidiPlugin", "✓ BLE scan stopped");
    }
}

std::vector<DeviceInfo> BleMidiPlugin::getDiscoveredDevices() {
    std::vector<DeviceInfo> devices;
    
    GError* error = nullptr;
    
    // Créer proxy pour ObjectManager
    GDBusProxy* manager_proxy = g_dbus_proxy_new_sync(
        dbus_connection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        nullptr,
        &error
    );
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "Failed to create ObjectManager proxy: " + std::string(error->message));
        g_error_free(error);
        return devices;
    }
    
    // Appeler GetManagedObjects
    GVariant* result = g_dbus_proxy_call_sync(
        manager_proxy,
        "GetManagedObjects",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(manager_proxy);
    
    if (error) {
        Logger::error("BleMidiPlugin", 
            "Failed to get managed objects: " + std::string(error->message));
        g_error_free(error);
        return devices;
    }
    
    // Parser le résultat
    GVariantIter* iter;
    const gchar* object_path;
    GVariant* ifaces_and_properties;
    
    g_variant_get(result, "(a{oa{sa{sv}}})", &iter);
    
    while (g_variant_iter_next(iter, "{&o@a{sa{sv}}}", &object_path, &ifaces_and_properties)) {
        // Vérifier si c'est un device
        GVariant* device_props = nullptr;
        if (g_variant_lookup(ifaces_and_properties, DEVICE_INTERFACE, "@a{sv}", &device_props)) {
            
            // Vérifier si le device a le service BLE MIDI
            if (hasBleMidiService(object_path)) {
                
                // Extraire les propriétés
                std::string name = getStringProperty(device_props, "Name");
                std::string address = getStringProperty(device_props, "Address");
                
                if (name.empty()) {
                    name = "BLE MIDI Device";
                }
                
                // Créer DeviceInfo avec TON STRUCTURE !
                std::string deviceId = "ble_" + address;
                // Remplacer ':' par '_' dans l'ID
                std::replace(deviceId.begin(), deviceId.end(), ':', '_');
                
                DeviceInfo info;
                info.id = deviceId;
                info.name = name;
                info.type = DeviceType::BLUETOOTH;
                
                // ✅ UTILISER METADATA JSON (pas de champs directs)
                info.metadata["address"] = address;
                info.metadata["bt_address"] = address;  // Helper getBluetoothAddress()
                info.metadata["discovery"] = "ble_scan";
                info.metadata["object_path"] = object_path;
                info.metadata["service_uuid"] = BLE_MIDI_SERVICE_UUID;
                
                devices.push_back(info);
                
                if (verbose_) {
                    Logger::info("BleMidiPlugin", 
                        "Found BLE MIDI device: " + name + " (" + address + ")");
                }
            }
            
            g_variant_unref(device_props);
        }
        
        g_variant_unref(ifaces_and_properties);
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
    
    return devices;
}

bool BleMidiPlugin::hasBleMidiService(const std::string& devicePath) {
    GVariant* properties = getDeviceProperties(devicePath);
    
    if (!properties) {
        return false;
    }
    
    // Récupérer la liste des UUIDs
    GVariant* uuids_variant = nullptr;
    bool has_service = false;
    
    if (g_variant_lookup(properties, "UUIDs", "@as", &uuids_variant)) {
        GVariantIter iter;
        const gchar* uuid;
        
        g_variant_iter_init(&iter, uuids_variant);
        
        while (g_variant_iter_next(&iter, "&s", &uuid)) {
            std::string uuid_str(uuid);
            // Convertir en minuscules pour comparaison
            std::transform(uuid_str.begin(), uuid_str.end(), uuid_str.begin(), ::tolower);
            
            if (uuid_str == BLE_MIDI_SERVICE_UUID) {
                has_service = true;
                break;
            }
        }
        
        g_variant_unref(uuids_variant);
    }
    
    g_variant_unref(properties);
    
    return has_service;
}

GVariant* BleMidiPlugin::getDeviceProperties(const std::string& devicePath) {
    GError* error = nullptr;
    
    GDBusProxy* device_proxy = g_dbus_proxy_new_sync(
        dbus_connection_,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        BLUEZ_SERVICE,
        devicePath.c_str(),
        "org.freedesktop.DBus.Properties",
        nullptr,
        &error
    );
    
    if (error) {
        if (verbose_) {
            Logger::error("BleMidiPlugin", 
                "Failed to create device proxy: " + std::string(error->message));
        }
        g_error_free(error);
        return nullptr;
    }
    
    GVariant* result = g_dbus_proxy_call_sync(
        device_proxy,
        "GetAll",
        g_variant_new("(s)", DEVICE_INTERFACE),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );
    
    g_object_unref(device_proxy);
    
    if (error) {
        if (verbose_) {
            Logger::error("BleMidiPlugin", 
                "Failed to get device properties: " + std::string(error->message));
        }
        g_error_free(error);
        return nullptr;
    }
    
    // Extraire le dictionnaire des propriétés
    GVariant* properties = nullptr;
    g_variant_get(result, "(@a{sv})", &properties);
    g_variant_unref(result);
    
    return properties;
}

std::string BleMidiPlugin::getStringProperty(GVariant* properties, const char* key) {
    const gchar* value = nullptr;
    
    if (g_variant_lookup(properties, key, "&s", &value)) {
        return std::string(value);
    }
    
    return "";
}

} // namespace midiMind

// ============================================================================
// AUTO-REGISTRATION
// ============================================================================

// ✅ ENREGISTREMENT AUTOMATIQUE DU PLUGIN
REGISTER_DEVICE_PLUGIN(midiMind::BleMidiPlugin)

// ============================================================================
// FIN DU FICHIER BleMidiPlugin.cpp v2.0.0
// ============================================================================
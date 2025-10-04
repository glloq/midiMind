// ============================================================================
// src/midi/devices/DevicePlugin.h - NOUVELLE ARCHITECTURE
// ============================================================================
#pragma once
#include <memory>
#include <functional>
#include <vector>
#include "MidiDevice.h"

namespace midiMind {

// ============================================================================
// INTERFACE PLUGIN
// ============================================================================

class IDevicePlugin {
public:
    virtual ~IDevicePlugin() = default;
    
    // Métadonnées
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual DeviceType getType() const = 0;
    
    // Capacités
    virtual bool supportsDiscovery() const = 0;
    virtual bool supportsHotplug() const = 0;
    
    // Cycle de vie
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // Découverte et création
    virtual std::vector<DeviceInfo> discover() = 0;
    virtual std::shared_ptr<MidiDevice> createDevice(const DeviceInfo& info) = 0;
};

// ============================================================================
// INFO DEVICE UNIFORME
// ============================================================================

struct DeviceInfo {
    std::string id;
    std::string name;
    DeviceType type;
    
    // Données spécifiques au type (JSON flexible)
    json metadata;
    
    // Helpers
    std::string getAddress() const {
        return metadata.value("address", "");
    }
    
    int getPort() const {
        return metadata.value("port", 0);
    }
    
    std::string getBluetoothAddress() const {
        return metadata.value("bt_address", "");
    }
    
    int getUsbPortNumber() const {
        return metadata.value("usb_port", -1);
    }
};

// ============================================================================
// REGISTRY DES PLUGINS
// ============================================================================

class DevicePluginRegistry {
public:
    static DevicePluginRegistry& instance() {
        static DevicePluginRegistry inst;
        return inst;
    }
    
    // Enregistrer un plugin
    void registerPlugin(std::shared_ptr<IDevicePlugin> plugin) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!plugin) return;
        
        // Vérifier si déjà enregistré
        for (const auto& p : plugins_) {
            if (p->getName() == plugin->getName()) {
                Logger::warn("PluginRegistry", "Plugin already registered: " + plugin->getName());
                return;
            }
        }
        
        if (plugin->initialize()) {
            plugins_.push_back(plugin);
            Logger::info("PluginRegistry", "✓ Registered plugin: " + plugin->getName() + 
                        " v" + plugin->getVersion());
        } else {
            Logger::error("PluginRegistry", "Failed to initialize plugin: " + plugin->getName());
        }
    }
    
    // Découvrir tous les devices disponibles
    std::vector<DeviceInfo> discoverAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<DeviceInfo> allDevices;
        
        for (auto& plugin : plugins_) {
            if (plugin->supportsDiscovery()) {
                try {
                    auto devices = plugin->discover();
                    allDevices.insert(allDevices.end(), devices.begin(), devices.end());
                    
                    Logger::info("PluginRegistry", 
                        plugin->getName() + " found " + std::to_string(devices.size()) + " devices");
                        
                } catch (const std::exception& e) {
                    Logger::error("PluginRegistry", 
                        "Discovery failed for " + plugin->getName() + ": " + e.what());
                }
            }
        }
        
        return allDevices;
    }
    
    // Créer un device à partir d'une DeviceInfo
    std::shared_ptr<MidiDevice> createDevice(const DeviceInfo& info) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& plugin : plugins_) {
            if (plugin->getType() == info.type) {
                try {
                    return plugin->createDevice(info);
                } catch (const std::exception& e) {
                    Logger::error("PluginRegistry", 
                        "Failed to create device: " + std::string(e.what()));
                    return nullptr;
                }
            }
        }
        
        Logger::error("PluginRegistry", "No plugin found for device type");
        return nullptr;
    }
    
    // Lister tous les plugins
    std::vector<std::string> listPlugins() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> names;
        for (const auto& p : plugins_) {
            names.push_back(p->getName() + " v" + p->getVersion());
        }
        return names;
    }
    
    ~DevicePluginRegistry() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& plugin : plugins_) {
            plugin->shutdown();
        }
        plugins_.clear();
    }

private:
    DevicePluginRegistry() = default;
    
    std::vector<std::shared_ptr<IDevicePlugin>> plugins_;
    mutable std::mutex mutex_;
};

// ============================================================================
// MACRO HELPER POUR ENREGISTREMENT AUTO
// ============================================================================

#define REGISTER_DEVICE_PLUGIN(PluginClass) \
    namespace { \
        struct PluginRegistrar_##PluginClass { \
            PluginRegistrar_##PluginClass() { \
                DevicePluginRegistry::instance().registerPlugin( \
                    std::make_shared<PluginClass>() \
                ); \
            } \
        }; \
        static PluginRegistrar_##PluginClass registrar_##PluginClass; \
    }

} // namespace midiMind

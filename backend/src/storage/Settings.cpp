// ============================================================================
// File: backend/src/storage/Settings.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of Settings class.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified implementation
//   - Better error handling
//   - Enhanced logging
//
// ============================================================================

#include "Settings.h"
#include <algorithm>
#include <cctype>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

Settings::Settings(Database& database)
    : database_(database)
{
    Logger::info("Settings", "Settings instance created");
    initializeDefaults();
}

Settings::~Settings() {
    Logger::debug("Settings", "Settings instance destroyed");
}

// ============================================================================
// PERSISTENCE
// ============================================================================

bool Settings::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("Settings", "Loading settings from database...");
    
    try {
        auto result = database_.query("SELECT key, value FROM settings");
        
        if (!result.success) {
            Logger::error("Settings", "Failed to query settings: " + result.error);
            return false;
        }
        
        size_t count = 0;
        for (const auto& row : result.rows) {
            std::string key = row.at("key");
            std::string value = row.at("value");
            
            cache_[key] = value;
            count++;
        }
        
        Logger::info("Settings", "✓ Loaded " + std::to_string(count) + " settings");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to load settings: " + std::string(e.what()));
        return false;
    }
}

bool Settings::save() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isLoaded_) {
        Logger::error("Settings", "Cannot save: not loaded");
        return false;
    }
    
    Logger::info("Settings", "Saving settings...");
    
    int count = 0;  // ✅ Déclarer count AVANT le lambda
    
    bool success = database_.transaction([&]() {  // ✅ Capturer par référence [&]
        for (const auto& [key, value] : settings_) {
            auto result = database_.execute(
                "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)",
                std::vector<std::string>{key, value}  // ✅ Convertir explicitement
            );
            
            if (!result.success) {
                Logger::warning("Settings", "Failed to save: " + key);
            } else {
                count++;  // ✅ count est accessible car capturé par [&]
            }
        }
    });
    
    if (!success) {
        Logger::error("Settings", "Transaction failed");
        return false;
    }
    
    Logger::info("Settings", "✓ Saved " + std::to_string(count) + " settings");
    return true;
}


void Settings::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("Settings", "Resetting to default values...");
    
    cache_.clear();
    initializeDefaults();
    
    Logger::info("Settings", "✓ Settings reset to defaults");
}

// ============================================================================
// GETTERS
// ============================================================================

std::string Settings::getString(const std::string& key, 
                                const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }
    
    return defaultValue;
}

int Settings::getInt(const std::string& key, int defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        try {
            return std::stoi(it->second);
        } catch (const std::exception& e) {
            Logger::warning("Settings", "Invalid int for '" + key + "': " + it->second);
            return defaultValue;
        }
    }
    
    return defaultValue;
}

bool Settings::getBool(const std::string& key, bool defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        std::string value = it->second;
        
        // Convert to lowercase for comparison
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        
        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            return true;
        }
        if (value == "false" || value == "0" || value == "no" || value == "off") {
            return false;
        }
        
        Logger::warning("Settings", "Invalid bool for '" + key + "': " + it->second);
        return defaultValue;
    }
    
    return defaultValue;
}

double Settings::getDouble(const std::string& key, double defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        try {
            return std::stod(it->second);
        } catch (const std::exception& e) {
            Logger::warning("Settings", "Invalid double for '" + key + "': " + it->second);
            return defaultValue;
        }
    }
    
    return defaultValue;
}

json Settings::getJson(const std::string& key, const json& defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        try {
            return json::parse(it->second);
        } catch (const std::exception& e) {
            Logger::warning("Settings", "Invalid JSON for '" + key + "': " + it->second);
            return defaultValue;
        }
    }
    
    return defaultValue;
}

// ============================================================================
// SETTERS
// ============================================================================

void Settings::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[key] = value;
    Logger::debug("Settings", "Set " + key + " = " + value);
}

void Settings::set(const std::string& key, int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[key] = std::to_string(value);
    Logger::debug("Settings", "Set " + key + " = " + std::to_string(value));
}

void Settings::set(const std::string& key, bool value) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[key] = value ? "true" : "false";
    Logger::debug("Settings", "Set " + key + " = " + (value ? "true" : "false"));
}

void Settings::set(const std::string& key, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[key] = std::to_string(value);
    Logger::debug("Settings", "Set " + key + " = " + std::to_string(value));
}

void Settings::set(const std::string& key, const json& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[key] = value.dump();
    Logger::debug("Settings", "Set " + key + " = <JSON>");
}

// ============================================================================
// UTILITIES
// ============================================================================

bool Settings::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.find(key) != cache_.end();
}

void Settings::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        cache_.erase(it);
        Logger::debug("Settings", "Removed: " + key);
    }
}

std::vector<std::string> Settings::getKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    keys.reserve(cache_.size());
    
    for (const auto& [key, _] : cache_) {
        keys.push_back(key);
    }
    
    return keys;
}

json Settings::getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json result;
    
    for (const auto& [key, value] : cache_) {
        result[key] = value;
    }
    
    return result;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void Settings::initializeDefaults() {
    // MIDI settings
    cache_["midi.clock_bpm"] = "120";
    cache_["midi.input_device"] = "";
    cache_["midi.output_device"] = "";
    
    // API settings
    cache_["api.port"] = "8080";
    cache_["api.host"] = "0.0.0.0";
    
    // Logging settings
    cache_["log.level"] = "INFO";
    cache_["log.file_enabled"] = "true";
    cache_["log.console_enabled"] = "true";
    
    // Auto-save settings
    cache_["auto_save.enabled"] = "true";
    cache_["auto_save.interval"] = "300";
    
    // Hot-plug settings
    cache_["hotplug.enabled"] = "true";
    cache_["hotplug.scan_interval"] = "2000";
    
    // Status broadcast settings
    cache_["status.broadcast_enabled"] = "true";
    cache_["status.broadcast_interval"] = "5000";
    
    // System settings
    cache_["system.max_polyphony"] = "128";
    cache_["system.buffer_size"] = "256";
    
    Logger::debug("Settings", "Default settings initialized");
}

} // namespace midiMind

// ============================================================================
// END OF FILE Settings.cpp
// ============================================================================
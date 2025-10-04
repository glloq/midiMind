// ============================================================================
// Fichier: src/storage/Settings.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "Settings.h"
#include "../core/TimeUtils.h"

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

Settings::Settings(std::shared_ptr<Database> database)
    : database_(database) {
    
    Logger::info("Settings", "Settings manager created");
    
    // Initialiser les valeurs par défaut
    initializeDefaults();
}

Settings::~Settings() {
    // Sauvegarder avant destruction
    save();
    Logger::info("Settings", "Settings manager destroyed");
}

// ============================================================================
// CHARGEMENT / SAUVEGARDE
// ============================================================================

void Settings::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("Settings", "Loading settings...");
    
    try {
        auto result = database_->query("SELECT key, value FROM settings");
        
        cache_.clear();
        
        for (const auto& row : result.rows) {
            cache_[row.at("key")] = row.at("value");
        }
        
        Logger::info("Settings", "✓ Loaded " + std::to_string(cache_.size()) + " settings");
        
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to load: " + std::string(e.what()));
    }
}

void Settings::save() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("Settings", "Saving settings...");
    
    try {
        database_->beginTransaction();
        
        for (const auto& [key, value] : cache_) {
            // Upsert
            database_->execute(
                "INSERT OR REPLACE INTO settings (key, value, updated_at) VALUES (?, ?, datetime('now'))",
                {key, value}
            );
        }
        
        database_->commit();
        
        Logger::info("Settings", "✓ Saved " + std::to_string(cache_.size()) + " settings");
        
    } catch (const std::exception& e) {
        database_->rollback();
        Logger::error("Settings", "Failed to save: " + std::string(e.what()));
    }
}

void Settings::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("Settings", "Resetting to defaults...");
    
    cache_.clear();
    initializeDefaults();
    
    // Supprimer de la DB
    try {
        database_->execute("DELETE FROM settings");
        Logger::info("Settings", "✓ Settings reset");
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to reset: " + std::string(e.what()));
    }
}

// ============================================================================
// GETTERS
// ============================================================================

std::string Settings::getString(const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }
    
    return defaultValue;
}

int Settings::getInt(const std::string& key, int defaultValue) {
    std::string value = getString(key);
    
    if (value.empty()) {
        return defaultValue;
    }
    
    try {
        return std::stoi(value);
    } catch (...) {
        return defaultValue;
    }
}

float Settings::getFloat(const std::string& key, float defaultValue) {
    std::string value = getString(key);
    
    if (value.empty()) {
        return defaultValue;
    }
    
    try {
        return std::stof(value);
    } catch (...) {
        return defaultValue;
    }
}

bool Settings::getBool(const std::string& key, bool defaultValue) {
    std::string value = getString(key);
    
    if (value.empty()) {
        return defaultValue;
    }
    
    return value == "true" || value == "1";
}

json Settings::getJson(const std::string& key, const json& defaultValue) {
    std::string value = getString(key);
    
    if (value.empty()) {
        return defaultValue;
    }
    
    try {
        return json::parse(value);
    } catch (...) {
        return defaultValue;
    }
}

// ============================================================================
// SETTERS
// ============================================================================

void Settings::set(const std::string& key, const std::string& value) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[key] = value;
    }
    
    // Notifier le changement
    notifyChanged(key, value);
}

void Settings::set(const std::string& key, int value) {
    set(key, std::to_string(value));
}

void Settings::set(const std::string& key, float value) {
    set(key, std::to_string(value));
}

void Settings::set(const std::string& key, bool value) {
    set(key, value ? "true" : "false");
}

void Settings::set(const std::string& key, const json& value) {
    set(key, value.dump());
}

// ============================================================================
// VÉRIFICATIONS
// ============================================================================

bool Settings::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.find(key) != cache_.end();
}

void Settings::remove(const std::string& key) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.erase(key);
    }
    
    // Supprimer de la DB
    try {
        database_->execute("DELETE FROM settings WHERE key = ?", {key});
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to remove key: " + std::string(e.what()));
    }
}

std::vector<std::string> Settings::getKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    for (const auto& [key, value] : cache_) {
        keys.push_back(key);
    }
    
    return keys;
}

json Settings::getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json j;
    for (const auto& [key, value] : cache_) {
        j[key] = value;
    }
    
    return j;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void Settings::setOnChanged(const std::string& key, ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_[key] = callback;
}

void Settings::removeOnChanged(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.erase(key);
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void Settings::initializeDefaults() {
    // MIDI
    cache_["midi.default_channel"] = "1";
    cache_["midi.clock_enabled"] = "false";
    cache_["midi.clock_tempo"] = "120.0";
    
    // Network
    cache_["network.rtpmidi_enabled"] = "true";
    cache_["network.rtpmidi_port"] = "5004";
    cache_["network.mdns_enabled"] = "true";
    cache_["network.wifi_hotspot_enabled"] = "false";
    
    // Audio
    cache_["audio.sample_rate"] = "48000";
    cache_["audio.buffer_size"] = "256";
    
    // API
    cache_["api.port"] = "8080";
    cache_["api.websocket_enabled"] = "true";
    
    // System
    cache_["system.log_level"] = "INFO";
    cache_["system.auto_backup"] = "true";
    cache_["system.backup_interval_hours"] = "24";
    
    // Optimization
    cache_["optimization.thread_pool_size"] = "4";
    cache_["optimization.memory_pool_enabled"] = "true";
    
    Logger::debug("Settings", "Default settings initialized");
}

void Settings::notifyChanged(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = callbacks_.find(key);
    if (it != callbacks_.end() && it->second) {
        try {
            it->second(key, value);
        } catch (const std::exception& e) {
            Logger::error("Settings", "Callback exception for key '" + key + "': " + 
                         std::string(e.what()));
        }
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Settings.cpp
// ============================================================================
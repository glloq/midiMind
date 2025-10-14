// ============================================================================
// Fichier: backend/src/storage/Settings.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0 - 2025-10-09
// ============================================================================
// Description:
//   Gestionnaire de paramètres applicatifs avec persistence en base de données
//
// Fonctionnalités:
//   - Get/Set pour différents types (string, int, bool, double, json)
//   - Valeurs par défaut
//   - Persistence automatique
//   - Thread-safe
// ============================================================================

#include "Settings.h"
#include "../core/Logger.h"

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

Settings::Settings(std::shared_ptr<Database> database)
    : database_(database) {
    
    Logger::info("Settings", "Settings manager created");
    
    // Initialiser les valeurs par défaut
    initializeDefaults();
    
    // Charger depuis la BDD
    load();
}

Settings::~Settings() {
    // Sauvegarder automatiquement à la destruction
    save();
    
    Logger::info("Settings", "Settings manager destroyed");
}

// ============================================================================
// INITIALISATION
// ============================================================================

void Settings::initializeDefaults() {
    // Paramètres MIDI
    cache_["midi.input_device"] = "default";
    cache_["midi.output_device"] = "default";
    cache_["midi.clock_source"] = "internal";
    cache_["midi.sync_enabled"] = "true";
    
    // Paramètres audio
    cache_["audio.sample_rate"] = "44100";
    cache_["audio.buffer_size"] = "256";
    cache_["audio.latency"] = "low";
    
    // Paramètres réseau
    cache_["network.rtpmidi_enabled"] = "true";
    cache_["network.rtpmidi_port"] = "5004";
    cache_["network.wifi_hotspot"] = "false";
    cache_["network.hostname"] = "midimind";
    
    // Paramètres interface
    cache_["ui.theme"] = "dark";
    cache_["ui.language"] = "en";
    cache_["ui.auto_save"] = "true";
    cache_["ui.auto_save_interval"] = "300";
    
    // Paramètres système
    cache_["system.log_level"] = "info";
    cache_["system.max_polyphony"] = "64";
    cache_["system.cpu_priority"] = "high";
    
    Logger::debug("Settings", "Default settings initialized");
}

// ============================================================================
// CHARGEMENT / SAUVEGARDE
// ============================================================================

void Settings::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("Settings", "Loading settings from database...");
    
    try {
        auto result = database_->query("SELECT key, value FROM settings");
        
        size_t count = 0;
        for (const auto& row : result.rows) {
            std::string key = row.at("key");
            std::string value = row.at("value");
            
            cache_[key] = value;
            count++;
        }
        
        Logger::info("Settings", "✓ Loaded " + std::to_string(count) + " settings");
        
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to load: " + std::string(e.what()));
    }
}

void Settings::save() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("Settings", "Saving settings to database...");
    
    try {
        database_->beginTransaction();
        
        for (const auto& [key, value] : cache_) {
            // Upsert (INSERT OR REPLACE)
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
    
    Logger::info("Settings", "Resetting settings to defaults...");
    
    cache_.clear();
    initializeDefaults();
    
    // Supprimer de la BDD
    try {
        database_->execute("DELETE FROM settings");
        Logger::info("Settings", "✓ Settings reset");
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to reset: " + std::string(e.what()));
    }
}

// ============================================================================
// GETTERS - STRING
// ============================================================================

std::string Settings::getString(const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }
    
    return defaultValue;
}

// ============================================================================
// GETTERS - INT
// ============================================================================

int Settings::getInt(const std::string& key, int defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        try {
            return std::stoi(it->second);
        } catch (const std::exception& e) {
            Logger::warn("Settings", "Invalid int for key '" + key + "': " + it->second);
            return defaultValue;
        }
    }
    
    return defaultValue;
}

// ============================================================================
// GETTERS - BOOL
// ============================================================================

bool Settings::getBool(const std::string& key, bool defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        std::string value = it->second;
        
        // Convertir en lowercase pour comparaison
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        
        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            return true;
        }
        if (value == "false" || value == "0" || value == "no" || value == "off") {
            return false;
        }
        
        Logger::warn("Settings", "Invalid bool for key '" + key + "': " + it->second);
        return defaultValue;
    }
    
    return defaultValue;
}

// ============================================================================
// GETTERS - DOUBLE
// ============================================================================

double Settings::getDouble(const std::string& key, double defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        try {
            return std::stod(it->second);
        } catch (const std::exception& e) {
            Logger::warn("Settings", "Invalid double for key '" + key + "': " + it->second);
            return defaultValue;
        }
    }
    
    return defaultValue;
}

// ============================================================================
// GETTERS - JSON
// ============================================================================

json Settings::getJson(const std::string& key, const json& defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        try {
            return json::parse(it->second);
        } catch (const std::exception& e) {
            Logger::warn("Settings", "Invalid JSON for key '" + key + "': " + it->second);
            return defaultValue;
        }
    }
    
    return defaultValue;
}

// ============================================================================
// SETTERS - STRING
// ============================================================================

void Settings::setString(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_[key] = value;
    
    Logger::debug("Settings", "Set " + key + " = " + value);
}

// ============================================================================
// SETTERS - INT
// ============================================================================

void Settings::setInt(const std::string& key, int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_[key] = std::to_string(value);
    
    Logger::debug("Settings", "Set " + key + " = " + std::to_string(value));
}

// ============================================================================
// SETTERS - BOOL
// ============================================================================

void Settings::setBool(const std::string& key, bool value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_[key] = value ? "true" : "false";
    
    Logger::debug("Settings", "Set " + key + " = " + (value ? "true" : "false"));
}

// ============================================================================
// SETTERS - DOUBLE
// ============================================================================

void Settings::setDouble(const std::string& key, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_[key] = std::to_string(value);
    
    Logger::debug("Settings", "Set " + key + " = " + std::to_string(value));
}

// ============================================================================
// SETTERS - JSON
// ============================================================================

void Settings::setJson(const std::string& key, const json& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_[key] = value.dump();
    
    Logger::debug("Settings", "Set " + key + " = [JSON]");
}

// ============================================================================
// VÉRIFICATION
// ============================================================================

bool Settings::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return cache_.find(key) != cache_.end();
}

void Settings::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_.erase(key);
    
    // Supprimer de la BDD aussi
    try {
        database_->execute("DELETE FROM settings WHERE key = ?", {key});
        Logger::debug("Settings", "Removed key: " + key);
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to remove key: " + std::string(e.what()));
    }
}

// ============================================================================
// EXPORT / IMPORT
// ============================================================================

json Settings::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json result;
    
    for (const auto& [key, value] : cache_) {
        // Essayer de parser comme JSON si possible
        try {
            result[key] = json::parse(value);
        } catch (...) {
            // Sinon, stocker comme string
            result[key] = value;
        }
    }
    
    return result;
}

void Settings::fromJson(const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_.clear();
    
    for (auto& [key, value] : data.items()) {
        if (value.is_string()) {
            cache_[key] = value.get<std::string>();
        } else {
            cache_[key] = value.dump();
        }
    }
    
    Logger::info("Settings", "Imported " + std::to_string(cache_.size()) + " settings");
}

std::map<std::string, std::string> Settings::getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_;
}

size_t Settings::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

void Settings::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    cache_.clear();
    
    try {
        database_->execute("DELETE FROM settings");
        Logger::info("Settings", "✓ All settings cleared");
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to clear: " + std::string(e.what()));
    }
}

// ============================================================================
// GESTION PAR CATÉGORIE
// ============================================================================

std::map<std::string, std::string> Settings::getByPrefix(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, std::string> result;
    
    for (const auto& [key, value] : cache_) {
        if (key.find(prefix) == 0) {
            result[key] = value;
        }
    }
    
    return result;
}

void Settings::removeByPrefix(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Trouver toutes les clés avec ce préfixe
    std::vector<std::string> keysToRemove;
    
    for (const auto& [key, value] : cache_) {
        if (key.find(prefix) == 0) {
            keysToRemove.push_back(key);
        }
    }
    
    // Supprimer
    for (const auto& key : keysToRemove) {
        cache_.erase(key);
        
        try {
            database_->execute("DELETE FROM settings WHERE key = ?", {key});
        } catch (const std::exception& e) {
            Logger::error("Settings", 
                "Failed to remove key '" + key + "': " + std::string(e.what()));
        }
    }
    
    Logger::info("Settings", 
        "Removed " + std::to_string(keysToRemove.size()) + " settings with prefix '" + prefix + "'");
}

// ============================================================================
// VALIDATION
// ============================================================================

bool Settings::validate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool valid = true;
    
    // Vérifier que les paramètres critiques existent
    std::vector<std::string> requiredKeys = {
        "midi.input_device",
        "midi.output_device",
        "system.log_level"
    };
    
    for (const auto& key : requiredKeys) {
        if (cache_.find(key) == cache_.end()) {
            Logger::error("Settings", "Missing required key: " + key);
            valid = false;
        }
    }
    
    // Vérifier les valeurs numériques
    if (has("audio.sample_rate")) {
        int sampleRate = getInt("audio.sample_rate", 0);
        if (sampleRate < 8000 || sampleRate > 192000) {
            Logger::error("Settings", "Invalid sample rate: " + std::to_string(sampleRate));
            valid = false;
        }
    }
    
    if (has("audio.buffer_size")) {
        int bufferSize = getInt("audio.buffer_size", 0);
        if (bufferSize < 32 || bufferSize > 8192) {
            Logger::error("Settings", "Invalid buffer size: " + std::to_string(bufferSize));
            valid = false;
        }
    }
    
    return valid;
}

// ============================================================================
// HELPERS
// ============================================================================

std::string Settings::getLogLevel() const {
    return getString("system.log_level", "info");
}

void Settings::setLogLevel(const std::string& level) {
    setString("system.log_level", level);
}

int Settings::getMaxPolyphony() const {
    return getInt("system.max_polyphony", 64);
}

void Settings::setMaxPolyphony(int polyphony) {
    setInt("system.max_polyphony", polyphony);
}

bool Settings::isAutoSaveEnabled() const {
    return getBool("ui.auto_save", true);
}

void Settings::setAutoSaveEnabled(bool enabled) {
    setBool("ui.auto_save", enabled);
}

int Settings::getAutoSaveInterval() const {
    return getInt("ui.auto_save_interval", 300);
}

void Settings::setAutoSaveInterval(int seconds) {
    setInt("ui.auto_save_interval", seconds);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER Settings.cpp
// ============================================================================

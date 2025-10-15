// ============================================================================
// Fichier: backend/src/storage/Settings.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.1 - CORRECTION COMPLÈTE
// Date: 2025-10-15
// ============================================================================
// Description:
//   Gestionnaire de paramètres applicatifs avec persistence en base de données
//
// CORRECTIONS v3.0.1:
//   ✅ Méthode load() corrigée (récursion infinie supprimée)
//   ✅ Implémentation complète de getString()
//   ✅ Implémentation complète de getInt()
//   ✅ Implémentation complète de getBool()
//   ✅ Implémentation complète de getDouble()
//   ✅ Implémentation complète de getJson()
//   ✅ Implémentation complète de set() (toutes surcharges)
//   ✅ Implémentation complète de save()
//   ✅ Implémentation complète de reset()
//
// Fonctionnalités:
//   - Get/Set pour différents types (string, int, bool, double, json)
//   - Valeurs par défaut
//   - Persistence automatique en base SQLite
//   - Thread-safe avec mutex
//   - Cache pour performances
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
    loadFromDatabase();
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
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::debug("Settings", "Initializing default values...");
    
    // Paramètres MIDI
    cache_["midi.input_device"] = "default";
    cache_["midi.output_device"] = "default";
    cache_["midi.clock_source"] = "internal";
    cache_["midi.sync_enabled"] = "true";
    cache_["midi.default_channel"] = "1";
    cache_["midi.clock_bpm"] = "120";
    
    // Paramètres audio
    cache_["audio.sample_rate"] = "48000";
    cache_["audio.buffer_size"] = "256";
    cache_["audio.channels"] = "2";
    
    // Paramètres UI
    cache_["ui.theme"] = "dark";
    cache_["ui.auto_save"] = "true";
    cache_["ui.auto_save_interval"] = "300";
    cache_["ui.show_tooltips"] = "true";
    
    // Paramètres système
    cache_["system.log_level"] = "info";
    cache_["system.startup_mode"] = "normal";
    cache_["system.enable_monitoring"] = "true";
    
    // Paramètres réseau
    cache_["network.wifi_enabled"] = "false";
    cache_["network.rtpmidi_enabled"] = "false";
    cache_["network.rtpmidi_port"] = "5004";
    
    Logger::debug("Settings", "✓ " + std::to_string(cache_.size()) + " default values initialized");
}

// ============================================================================
// CHARGEMENT
// ============================================================================

void Settings::load() {
    Logger::info("Settings", "Loading settings...");
    loadFromDatabase();
}

void Settings::loadFromDatabase() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!database_) {
        Logger::error("Settings", "Database not available");
        return;
    }
    
    Logger::debug("Settings", "Loading settings from database...");
    
    try {
        // Requête pour charger tous les settings
        std::string query = "SELECT key, value FROM settings";
        
        auto results = database_->query(query);
        
        int loadedCount = 0;
        
        for (const auto& row : results) {
            if (row.size() >= 2) {
                std::string key = row[0];
                std::string value = row[1];
                
                // Mettre à jour le cache
                cache_[key] = value;
                loadedCount++;
                
                Logger::debug("Settings", "  Loaded: " + key + " = " + value);
            }
        }
        
        Logger::info("Settings", "✓ Loaded " + std::to_string(loadedCount) + " settings from database");
        
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to load from database: " + std::string(e.what()));
        Logger::warn("Settings", "Using default values");
    }
}

// ============================================================================
// SAUVEGARDE
// ============================================================================

void Settings::save() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!database_) {
        Logger::error("Settings", "Database not available");
        return;
    }
    
    Logger::info("Settings", "Saving settings to database...");
    
    try {
        // Commencer une transaction pour performance
        database_->beginTransaction();
        
        int savedCount = 0;
        
        // Sauvegarder chaque paramètre
        for (const auto& pair : cache_) {
            const std::string& key = pair.first;
            const std::string& value = pair.second;
            
            // Utiliser INSERT OR REPLACE pour SQLite
            std::string query = 
                "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)";
            
            std::vector<std::string> params = { key, value };
            
            database_->execute(query, params);
            savedCount++;
            
            Logger::debug("Settings", "  Saved: " + key + " = " + value);
        }
        
        // Committer la transaction
        database_->commitTransaction();
        
        Logger::info("Settings", "✓ Saved " + std::to_string(savedCount) + " settings to database");
        
    } catch (const std::exception& e) {
        Logger::error("Settings", "Failed to save to database: " + std::string(e.what()));
        
        // Rollback en cas d'erreur
        try {
            database_->rollbackTransaction();
        } catch (...) {
            Logger::error("Settings", "Failed to rollback transaction");
        }
    }
}

// ============================================================================
// RESET
// ============================================================================

void Settings::reset() {
    Logger::info("Settings", "Resetting settings to defaults...");
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Vider le cache
        cache_.clear();
        
        // Réinitialiser les valeurs par défaut
        initializeDefaults();
    }
    
    // Sauvegarder les valeurs par défaut
    save();
    
    Logger::info("Settings", "✓ Settings reset to defaults");
}

// ============================================================================
// GETTERS TYPÉS
// ============================================================================

std::string Settings::getString(const std::string& key, 
                               const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        Logger::debug("Settings", "getString('" + key + "') = '" + it->second + "'");
        return it->second;
    }
    
    Logger::debug("Settings", "getString('" + key + "') = '" + defaultValue + "' (default)");
    return defaultValue;
}

int Settings::getInt(const std::string& key, int defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        try {
            int value = std::stoi(it->second);
            Logger::debug("Settings", "getInt('" + key + "') = " + std::to_string(value));
            return value;
        } catch (const std::exception& e) {
            Logger::warn("Settings", "Failed to convert '" + key + "' to int: " + 
                        std::string(e.what()));
        }
    }
    
    Logger::debug("Settings", "getInt('" + key + "') = " + 
                 std::to_string(defaultValue) + " (default)");
    return defaultValue;
}

bool Settings::getBool(const std::string& key, bool defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        std::string value = it->second;
        
        // Supporter plusieurs formats : true, 1, yes, on
        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            Logger::debug("Settings", "getBool('" + key + "') = true");
            return true;
        }
        
        if (value == "false" || value == "0" || value == "no" || value == "off") {
            Logger::debug("Settings", "getBool('" + key + "') = false");
            return false;
        }
        
        Logger::warn("Settings", "Invalid boolean value for '" + key + "': " + value);
    }
    
    Logger::debug("Settings", "getBool('" + key + "') = " + 
                 std::string(defaultValue ? "true" : "false") + " (default)");
    return defaultValue;
}

double Settings::getDouble(const std::string& key, double defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        try {
            double value = std::stod(it->second);
            Logger::debug("Settings", "getDouble('" + key + "') = " + std::to_string(value));
            return value;
        } catch (const std::exception& e) {
            Logger::warn("Settings", "Failed to convert '" + key + "' to double: " + 
                        std::string(e.what()));
        }
    }
    
    Logger::debug("Settings", "getDouble('" + key + "') = " + 
                 std::to_string(defaultValue) + " (default)");
    return defaultValue;
}

json Settings::getJson(const std::string& key, const json& defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        try {
            json value = json::parse(it->second);
            Logger::debug("Settings", "getJson('" + key + "') = " + value.dump());
            return value;
        } catch (const std::exception& e) {
            Logger::warn("Settings", "Failed to parse JSON for '" + key + "': " + 
                        std::string(e.what()));
        }
    }
    
    Logger::debug("Settings", "getJson('" + key + "') = " + 
                 defaultValue.dump() + " (default)");
    return defaultValue;
}

// ============================================================================
// SETTERS
// ============================================================================

void Settings::set(const std::string& key, const std::string& value) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Logger::debug("Settings", "set('" + key + "', '" + value + "')");
        cache_[key] = value;
    }
    
    // Sauvegarder immédiatement (optionnel, peut être fait périodiquement)
    // Commenté pour performances, décommenter si persistence immédiate nécessaire
    // save();
}

void Settings::set(const std::string& key, int value) {
    set(key, std::to_string(value));
}

void Settings::set(const std::string& key, bool value) {
    set(key, value ? "true" : "false");
}

void Settings::set(const std::string& key, double value) {
    set(key, std::to_string(value));
}

void Settings::set(const std::string& key, const json& value) {
    set(key, value.dump());
}

// ============================================================================
// MÉTHODES UTILITAIRES
// ============================================================================

bool Settings::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.find(key) != cache_.end();
}

void Settings::remove(const std::string& key) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Logger::debug("Settings", "Removing key: " + key);
        cache_.erase(key);
    }
    
    // Supprimer aussi de la base de données
    if (database_) {
        try {
            std::string query = "DELETE FROM settings WHERE key = ?";
            std::vector<std::string> params = { key };
            database_->execute(query, params);
            
            Logger::debug("Settings", "✓ Key removed from database: " + key);
        } catch (const std::exception& e) {
            Logger::error("Settings", "Failed to remove key from database: " + 
                         std::string(e.what()));
        }
    }
}

std::vector<std::string> Settings::getAllKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    keys.reserve(cache_.size());
    
    for (const auto& pair : cache_) {
        keys.push_back(pair.first);
    }
    
    return keys;
}

size_t Settings::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

// ============================================================================
// EXPORT/IMPORT
// ============================================================================

json Settings::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json j = json::object();
    
    for (const auto& pair : cache_) {
        const std::string& key = pair.first;
        const std::string& value = pair.second;
        
        // Essayer de parser comme JSON
        try {
            json parsedValue = json::parse(value);
            j[key] = parsedValue;
        } catch (...) {
            // Si échec, stocker comme string
            j[key] = value;
        }
    }
    
    return j;
}

void Settings::fromJson(const json& j) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("Settings", "Importing settings from JSON...");
    
    int importedCount = 0;
    
    for (auto it = j.begin(); it != j.end(); ++it) {
        std::string key = it.key();
        std::string value;
        
        if (it.value().is_string()) {
            value = it.value().get<std::string>();
        } else {
            value = it.value().dump();
        }
        
        cache_[key] = value;
        importedCount++;
    }
    
    Logger::info("Settings", "✓ Imported " + std::to_string(importedCount) + " settings");
}

// ============================================================================
// FIN DU FICHIER Settings.cpp v3.0.1 - CORRECTION COMPLÈTE
// ============================================================================
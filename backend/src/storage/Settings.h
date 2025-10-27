// ============================================================================
// File: backend/src/storage/Settings.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Application settings management with database persistence.
//   Key-value store with type-safe getters/setters.
//
// Features:
//   - In-memory cache for fast access (O(1) lookup)
//   - Database persistence (settings table)
//   - Type-safe get/set methods
//   - Default values
//   - Thread-safe operations
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified API
//   - Enhanced type safety
//   - Better default value handling
//   - JSON support for complex settings
//   - Changed cache from std::map to std::unordered_map for O(1) lookup
//
// ============================================================================

#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <nlohmann/json.hpp>
#include "../core/Logger.h"
#include "Database.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @class Settings
 * @brief Application settings manager with database persistence
 * 
 * Provides a key-value store for application settings with:
 * - In-memory cache for performance (O(1) lookup with unordered_map)
 * - Database persistence
 * - Type-safe getters and setters
 * - Thread-safe operations
 * 
 * Settings are stored in the 'settings' table in the database.
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses internal mutex for synchronization
 * - Methods may throw std::system_error on mutex failures (rare)
 * 
 * Exception Safety:
 * - load(): May throw database exceptions
 * - save(): May throw database exceptions
 * - Getters: Safe - catch conversion errors and return defaults
 * - Setters: Safe - no throws (except mutex errors)
 * 
 * Example:
 * ```cpp
 * Settings settings(database);
 * settings.load();
 * 
 * // Get with default
 * int bpm = settings.getInt("midi.clock_bpm", 120);
 * bool enabled = settings.getBool("auto_save", true);
 * 
 * // Set values
 * settings.set("midi.clock_bpm", 140);
 * settings.set("auto_save", false);
 * 
 * // Save to database
 * settings.save();
 * ```
 */
class Settings {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param database Reference to Database instance
     * @note Database reference must remain valid for lifetime of Settings
     */
    explicit Settings(Database& database);
    
    /**
     * @brief Destructor
     */
    ~Settings();
    
    // Disable copy
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    
    // ========================================================================
    // PERSISTENCE
    // ========================================================================
    
    /**
     * @brief Load all settings from database
     * @return true if successful
     * @throws May throw std::exception on database errors
     * @note Thread-safe
     */
    bool load();
    
    /**
     * @brief Save all settings to database
     * @return true if successful
     * @note Thread-safe
     * @note Uses database transaction for atomicity
     */
    bool save();
    
    /**
     * @brief Reset to default values
     * @note Thread-safe
     * @note Does not persist to database - call save() to persist
     */
    void reset();
    
    // ========================================================================
    // GETTERS (TYPE-SAFE)
    // ========================================================================
    
    /**
     * @brief Get string value
     * @param key Setting key
     * @param defaultValue Default if key not found
     * @return String value
     * @note Thread-safe
     * @note Returns defaultValue if key doesn't exist
     */
    std::string getString(const std::string& key, 
                         const std::string& defaultValue = "");
    
    /**
     * @brief Get integer value
     * @param key Setting key
     * @param defaultValue Default if key not found
     * @return Integer value
     * @note Thread-safe
     * @note Returns defaultValue if key doesn't exist or conversion fails
     */
    int getInt(const std::string& key, int defaultValue = 0);
    
    /**
     * @brief Get boolean value
     * @param key Setting key
     * @param defaultValue Default if key not found
     * @return Boolean value
     * @note Thread-safe
     * @note Accepts: true/false, 1/0, yes/no, on/off (case insensitive)
     * @note Returns defaultValue if key doesn't exist or invalid format
     */
    bool getBool(const std::string& key, bool defaultValue = false);
    
    /**
     * @brief Get double value
     * @param key Setting key
     * @param defaultValue Default if key not found
     * @return Double value
     * @note Thread-safe
     * @note Returns defaultValue if key doesn't exist or conversion fails
     */
    double getDouble(const std::string& key, double defaultValue = 0.0);
    
    /**
     * @brief Get JSON value
     * @param key Setting key
     * @param defaultValue Default if key not found
     * @return JSON value
     * @note Thread-safe
     * @note Returns defaultValue if key doesn't exist or JSON parse fails
     */
    json getJson(const std::string& key, const json& defaultValue = json::object());
    
    // ========================================================================
    // SETTERS (TYPE-SAFE)
    // ========================================================================
    
    /**
     * @brief Set string value
     * @param key Setting key
     * @param value String value
     * @note Thread-safe
     * @note Only updates cache - call save() to persist
     */
    void set(const std::string& key, const std::string& value);
    
    /**
     * @brief Set integer value
     * @param key Setting key
     * @param value Integer value
     * @note Thread-safe
     * @note Only updates cache - call save() to persist
     */
    void set(const std::string& key, int value);
    
    /**
     * @brief Set boolean value
     * @param key Setting key
     * @param value Boolean value
     * @note Thread-safe
     * @note Stored as "true" or "false" string
     * @note Only updates cache - call save() to persist
     */
    void set(const std::string& key, bool value);
    
    /**
     * @brief Set double value
     * @param key Setting key
     * @param value Double value
     * @note Thread-safe
     * @note Only updates cache - call save() to persist
     */
    void set(const std::string& key, double value);
    
    /**
     * @brief Set JSON value
     * @param key Setting key
     * @param value JSON value
     * @note Thread-safe
     * @note Stored as JSON string
     * @note Only updates cache - call save() to persist
     */
    void set(const std::string& key, const json& value);
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    /**
     * @brief Check if key exists
     * @param key Setting key
     * @return true if exists
     * @note Thread-safe
     */
    bool has(const std::string& key) const;
    
    /**
     * @brief Remove setting
     * @param key Setting key
     * @note Thread-safe
     * @note Only removes from cache - call save() to persist
     */
    void remove(const std::string& key);
    
    /**
     * @brief Get all keys
     * @return Vector of all keys
     * @note Thread-safe
     */
    std::vector<std::string> getKeys() const;
    
    /**
     * @brief Get all settings as JSON
     * @return JSON object with all settings
     * @note Thread-safe
     */
    json getAll() const;
    
private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Initialize default settings
     * @note Called by constructor and reset()
     */
    void initializeDefaults();
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Reference to database
    Database& database_;
    
    /// In-memory cache (key -> value as string)
    /// Using unordered_map for O(1) average lookup time
    std::unordered_map<std::string, std::string> cache_;
    
    /// Mutex for thread-safety
    mutable std::mutex mutex_;
};

} // namespace midiMind
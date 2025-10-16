// ============================================================================
// File: backend/src/storage/PresetManager.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Manager for MIDI routing and configuration presets
//
// Features:
//   - Create, read, update, delete presets
//   - Category organization
//   - SQLite persistence
//   - Import/export to files
//   - Search and filtering
//   - Statistics
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified preset structure
//   - Enhanced search capabilities
//   - Better JSON serialization
//   - Improved error handling
//
// ============================================================================

#pragma once

#include "../core/Error.h"
#include "Database.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <optional>
#include <ctime>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct PresetEntry
 * @brief Single entry in a routing preset
 */
struct PresetEntry {
    uint8_t channel = 0;           ///< MIDI channel (0-15)
    std::string fileId;            ///< File ID
    std::string deviceId;          ///< Device ID
    std::string deviceName;        ///< Device name
    int32_t offsetMs = 0;          ///< Time offset in ms
    bool muted = false;            ///< Mute state
    bool solo = false;             ///< Solo state
    float volume = 1.0f;           ///< Volume (0.0 - 1.0)
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     */
    static PresetEntry fromJson(const json& j);
};

/**
 * @struct PresetMetadata
 * @brief Metadata for a preset
 */
struct PresetMetadata {
    int id = 0;                    ///< Database ID
    std::string name;              ///< Preset name
    std::string category;          ///< Category
    std::string description;       ///< Description
    int entryCount = 0;            ///< Number of entries
    std::time_t createdAt = 0;     ///< Creation timestamp
    std::time_t modifiedAt = 0;    ///< Modification timestamp
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     */
    static PresetMetadata fromJson(const json& j);
};

/**
 * @struct Preset
 * @brief Complete preset with entries
 */
struct Preset {
    PresetMetadata metadata;
    std::vector<PresetEntry> entries;
    
    /**
     * @brief Add entry
     */
    void addEntry(const PresetEntry& entry);
    
    /**
     * @brief Remove entry
     */
    bool removeEntry(size_t index);
    
    /**
     * @brief Get entry count
     */
    size_t getEntryCount() const { return entries.size(); }
    
    /**
     * @brief Clear all entries
     */
    void clear() { entries.clear(); }
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     */
    static Preset fromJson(const json& j);
};

// ============================================================================
// CLASS: PresetManager
// ============================================================================

/**
 * @class PresetManager
 * @brief Manager for MIDI routing presets
 * 
 * Manages persistence and organization of routing presets.
 * All presets are stored in SQLite database.
 * 
 * Thread Safety: YES (all public methods are thread-safe)
 * 
 * Database Schema:
 * ```sql
 * CREATE TABLE presets (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     name TEXT NOT NULL,
 *     category TEXT DEFAULT '',
 *     description TEXT DEFAULT '',
 *     data TEXT NOT NULL,
 *     entry_count INTEGER DEFAULT 0,
 *     created_at INTEGER NOT NULL,
 *     modified_at INTEGER NOT NULL
 * );
 * ```
 * 
 * Example:
 * ```cpp
 * auto db = std::make_shared<Database>("midimind.db");
 * db->open();
 * 
 * PresetManager manager(db);
 * 
 * // Create preset
 * Preset preset;
 * preset.metadata.name = "Jazz Piano";
 * preset.metadata.category = "Jazz";
 * preset.metadata.description = "Piano setup for jazz";
 * 
 * PresetEntry entry;
 * entry.channel = 0;
 * entry.deviceId = "piano_001";
 * entry.deviceName = "Roland FP-30";
 * preset.addEntry(entry);
 * 
 * int id = manager.create(preset);
 * 
 * // List all presets
 * auto presets = manager.list();
 * for (const auto& p : presets) {
 *     std::cout << p.name << std::endl;
 * }
 * 
 * // Load preset
 * auto loaded = manager.load(id);
 * ```
 */
class PresetManager {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param database Shared pointer to database
     * @throws MidiMindException if database not opened
     * @note Database must be opened before creating manager
     */
    explicit PresetManager(std::shared_ptr<Database> database);
    
    /**
     * @brief Destructor
     */
    ~PresetManager();
    
    // Disable copy
    PresetManager(const PresetManager&) = delete;
    PresetManager& operator=(const PresetManager&) = delete;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    /**
     * @brief Initialize database schema
     * @return true if successful
     * @note Called automatically in constructor
     * @note Thread-safe
     */
    bool initializeSchema();
    
    // ========================================================================
    // CRUD - CREATE
    // ========================================================================
    
    /**
     * @brief Create new preset
     * @param preset Preset to save
     * @return int Preset ID (> 0)
     * @throws MidiMindException on database error
     * @note Thread-safe
     * @note preset.metadata.name must not be empty
     */
    int create(const Preset& preset);
    
    // ========================================================================
    // CRUD - READ
    // ========================================================================
    
    /**
     * @brief Load complete preset
     * @param id Preset ID
     * @return Preset or std::nullopt if not found
     * @note Thread-safe
     */
    std::optional<Preset> load(int id);
    
    /**
     * @brief Get preset metadata only
     * @param id Preset ID
     * @return Metadata or std::nullopt if not found
     * @note Thread-safe
     * @note Faster than load() - doesn't parse entries
     */
    std::optional<PresetMetadata> getMetadata(int id);
    
    /**
     * @brief List all presets
     * @return Vector of preset metadata
     * @note Thread-safe
     * @note Returns metadata only (not full presets)
     */
    std::vector<PresetMetadata> list() const;
    
    /**
     * @brief List presets by category
     * @param category Category name
     * @return Vector of preset metadata
     * @note Thread-safe
     */
    std::vector<PresetMetadata> listByCategory(const std::string& category) const;
    
    /**
     * @brief Search presets by name
     * @param query Search query
     * @return Vector of preset metadata
     * @note Thread-safe
     * @note Case-insensitive search
     */
    std::vector<PresetMetadata> search(const std::string& query) const;
    
    /**
     * @brief Get all categories
     * @return Vector of category names
     * @note Thread-safe
     */
    std::vector<std::string> getCategories() const;
    
    // ========================================================================
    // CRUD - UPDATE
    // ========================================================================
    
    /**
     * @brief Update existing preset
     * @param id Preset ID
     * @param preset Updated preset
     * @throws MidiMindException if preset not found or on database error
     * @note Thread-safe
     */
    void update(int id, const Preset& preset);
    
    // ========================================================================
    // CRUD - DELETE
    // ========================================================================
    
    /**
     * @brief Delete preset
     * @param id Preset ID
     * @return true if deleted
     * @note Thread-safe
     */
    bool remove(int id);
    
    /**
     * @brief Check if preset exists
     * @param id Preset ID
     * @return true if exists
     * @note Thread-safe
     */
    bool exists(int id) const;
    
    // ========================================================================
    // IMPORT / EXPORT
    // ========================================================================
    
    /**
     * @brief Export preset to JSON file
     * @param id Preset ID
     * @param filepath File path
     * @return true if exported
     * @note Thread-safe
     */
    bool exportToFile(int id, const std::string& filepath);
    
    /**
     * @brief Import preset from JSON file
     * @param filepath File path
     * @return int Preset ID or -1 on error
     * @note Thread-safe
     */
    int importFromFile(const std::string& filepath);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get preset count
     * @return int Total number of presets
     * @note Thread-safe
     */
    int count() const;
    
    /**
     * @brief Get statistics
     * @return JSON statistics
     * @note Thread-safe
     */
    json getStatistics() const;

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Serialize preset to JSON string
     */
    std::string serializePreset(const Preset& preset) const;
    
    /**
     * @brief Deserialize preset from JSON string
     */
    Preset deserializePreset(const std::string& data) const;
    
    /**
     * @brief Parse metadata from database row
     */
    PresetMetadata parseMetadata(const std::map<std::string, std::string>& row) const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Database connection
    std::shared_ptr<Database> database_;
    
    /// Thread safety
    mutable std::mutex mutex_;
};

} // namespace midiMind
// ============================================================================
// File: backend/src/storage/PresetManager.h
// Version: 4.2.0 - THREAD-SAFE
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.0:
//   - Moved getEntryCount() and clear() from inline to .cpp
//   - Added documentation about struct thread-safety
//   - toJson() methods now documented as potentially throwing
//   - Added const correctness
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
 * @brief Single entry in a preset
 * @note NOT thread-safe - caller must ensure thread safety
 */
struct PresetEntry {
    uint8_t channel = 0;
    std::string fileId;
    std::string deviceId;
    std::string deviceName;
    int32_t offsetMs = 0;
    bool muted = false;
    bool solo = false;
    float volume = 1.0f;
    
    /**
     * @brief Convert to JSON
     * @throws json::exception on serialization error
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     * @throws json::exception on invalid JSON
     */
    static PresetEntry fromJson(const json& j);
};

/**
 * @struct PresetMetadata
 * @brief Metadata for a preset
 * @note NOT thread-safe - caller must ensure thread safety
 */
struct PresetMetadata {
    int id = 0;
    std::string name;
    std::string category;
    std::string description;
    int entryCount = 0;
    std::time_t createdAt = 0;
    std::time_t modifiedAt = 0;
    
    /**
     * @brief Convert to JSON
     * @throws json::exception on serialization error
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     * @throws json::exception on invalid JSON
     */
    static PresetMetadata fromJson(const json& j);
};

/**
 * @struct Preset
 * @brief Complete preset with metadata and entries
 * @note NOT thread-safe - caller must ensure thread safety
 */
struct Preset {
    PresetMetadata metadata;
    std::vector<PresetEntry> entries;
    
    /**
     * @brief Add entry to preset
     */
    void addEntry(const PresetEntry& entry);
    
    /**
     * @brief Remove entry at index
     * @return true if removed, false if index invalid
     */
    bool removeEntry(size_t index);
    
    /**
     * @brief Get entry count
     */
    size_t getEntryCount() const;
    
    /**
     * @brief Clear all entries
     */
    void clear();
    
    /**
     * @brief Convert to JSON
     * @throws json::exception on serialization error
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     * @throws json::exception on invalid JSON
     */
    static Preset fromJson(const json& j);
};

// ============================================================================
// CLASS: PresetManager
// ============================================================================

/**
 * @class PresetManager
 * @brief Thread-safe preset management system
 * 
 * Manages MIDI presets with database persistence.
 * All public methods are thread-safe.
 * 
 * Thread Safety:
 * - All public methods protected by internal mutex
 * - Returned Preset/PresetEntry/PresetMetadata objects are NOT thread-safe
 * - Caller must ensure thread safety when using returned objects
 */
class PresetManager {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param database Reference to database
     * @throws MidiMindException if database not connected
     */
    explicit PresetManager(Database& database);
    
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
     * @note Thread-safe
     */
    bool initializeSchema();
    
    // ========================================================================
    // CRUD - CREATE
    // ========================================================================
    
    /**
     * @brief Create new preset
     * @param preset Preset to create
     * @return New preset ID
     * @throws MidiMindException on error
     * @note Thread-safe
     */
    int create(const Preset& preset);
    
    // ========================================================================
    // CRUD - READ
    // ========================================================================
    
    /**
     * @brief Load preset by ID
     * @param id Preset ID
     * @return Preset if found
     * @note Thread-safe
     */
    std::optional<Preset> load(int id);
    
    /**
     * @brief Get preset metadata
     * @param id Preset ID
     * @return Metadata if found
     * @note Thread-safe
     */
    std::optional<PresetMetadata> getMetadata(int id);
    
    /**
     * @brief List all presets
     * @return Vector of preset metadata
     * @note Thread-safe
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
     * @brief Search presets
     * @param query Search query
     * @return Vector of matching preset metadata
     * @note Thread-safe
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
     * @brief Update preset
     * @param id Preset ID
     * @param preset New preset data
     * @throws MidiMindException if not found or error
     * @note Thread-safe
     */
    void update(int id, const Preset& preset);
    
    // ========================================================================
    // CRUD - DELETE
    // ========================================================================
    
    /**
     * @brief Remove preset
     * @param id Preset ID
     * @return true if removed
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
     * @param filepath Output file path
     * @return true if successful
     * @note Thread-safe
     */
    bool exportToFile(int id, const std::string& filepath);
    
    /**
     * @brief Import preset from JSON file
     * @param filepath Input file path
     * @return New preset ID, or -1 on error
     * @note Thread-safe
     */
    int importFromFile(const std::string& filepath);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get preset count
     * @return Number of presets
     * @note Thread-safe
     */
    int count() const;
    
    /**
     * @brief Get statistics
     * @return JSON with statistics
     * @note Thread-safe
     */
    json getStatistics() const;

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Serialize preset to JSON string
     * @throws json::exception on error
     */
    std::string serializePreset(const Preset& preset) const;
    
    /**
     * @brief Deserialize preset from JSON string
     * @throws MidiMindException on error
     */
    Preset deserializePreset(const std::string& data) const;
    
    /**
     * @brief Parse metadata from database row
     * @throws std::exception on parse error
     */
    PresetMetadata parseMetadata(const std::map<std::string, std::string>& row) const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Database reference
    Database& database_;
    
    /// Thread synchronization
    mutable std::mutex mutex_;
};

} // namespace midiMind
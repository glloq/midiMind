// ============================================================================
// File: backend/src/storage/InstrumentDatabase.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Database interface for instrument latency profiles.
//   Manages CRUD operations on instruments_latency table.
//
// Features:
//   - CRUD operations for instrument profiles
//   - In-memory cache for fast access
//   - Thread-safe operations
//   - Filtering by device, channel, etc.
//   - Statistics and monitoring
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Initial implementation for instrument-level latency compensation
//   - Integration with Database class
//   - Cache management
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <mutex>
#include <nlohmann/json.hpp>
#include "../core/Logger.h"
#include "Database.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURE: InstrumentLatencyEntry
// ============================================================================

/**
 * @struct InstrumentLatencyEntry
 * @brief Complete instrument latency profile data
 */
struct InstrumentLatencyEntry {
    // Identifiers
    std::string id;
    std::string deviceId;
    int channel;
    
    // Metadata
    std::string name;
    std::string instrumentType;
    
    // Latency measurements (microseconds)
    int64_t avgLatency;
    int64_t minLatency;
    int64_t maxLatency;
    
    // Statistics
    double jitter;
    double stdDeviation;
    int measurementCount;
    
    // Calibration
    double calibrationConfidence;
    std::string lastCalibration;
    std::string calibrationMethod;
    
    // Compensation
    int64_t compensationOffset;
    bool autoCalibration;
    bool enabled;
    
    // History (JSON string)
    std::string measurementHistory;
    
    // Timestamps
    std::string createdAt;
    std::string updatedAt;
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const {
        return {
            {"id", id},
            {"device_id", deviceId},
            {"channel", channel},
            {"name", name},
            {"instrument_type", instrumentType},
            {"avg_latency", avgLatency},
            {"min_latency", minLatency},
            {"max_latency", maxLatency},
            {"jitter", jitter},
            {"std_deviation", stdDeviation},
            {"measurement_count", measurementCount},
            {"calibration_confidence", calibrationConfidence},
            {"last_calibration", lastCalibration},
            {"calibration_method", calibrationMethod},
            {"compensation_offset", compensationOffset},
            {"auto_calibration", autoCalibration},
            {"enabled", enabled},
            {"created_at", createdAt},
            {"updated_at", updatedAt}
        };
    }
};

// ============================================================================
// CLASS: InstrumentDatabase
// ============================================================================

/**
 * @class InstrumentDatabase
 * @brief Database interface for instrument latency profiles
 * 
 * Provides CRUD operations on the instruments_latency table with:
 * - In-memory cache for fast access
 * - Thread-safe operations
 * - Filtering and search capabilities
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses internal mutex for synchronization
 * 
 * Example:
 * ```cpp
 * InstrumentDatabase instrDb(database);
 * 
 * // Create instrument
 * InstrumentLatencyEntry entry;
 * entry.id = "piano_001";
 * entry.deviceId = "device_usb_128_0";
 * entry.channel = 0;
 * entry.name = "Grand Piano";
 * instrDb.createInstrument(entry);
 * 
 * // Get instrument
 * auto piano = instrDb.getInstrument("piano_001");
 * if (piano.has_value()) {
 *     int64_t offset = piano->compensationOffset;
 * }
 * 
 * // Update
 * entry.avgLatency = 8000;
 * instrDb.updateInstrument(entry);
 * ```
 */
class InstrumentDatabase {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param database Reference to Database instance
     */
    explicit InstrumentDatabase(Database& database);
    
    /**
     * @brief Destructor
     */
    ~InstrumentDatabase();
    
    // Disable copy
    InstrumentDatabase(const InstrumentDatabase&) = delete;
    InstrumentDatabase& operator=(const InstrumentDatabase&) = delete;
    
    // ========================================================================
    // CRUD OPERATIONS
    // ========================================================================
    
    /**
     * @brief Create new instrument profile
     * @param entry Instrument data
     * @return true if successful
     * @note Thread-safe
     * @note Updates cache
     */
    bool createInstrument(const InstrumentLatencyEntry& entry);
    
    /**
     * @brief Get instrument by ID
     * @param id Instrument ID
     * @return Optional entry
     * @note Thread-safe
     * @note Uses cache if available
     */
    std::optional<InstrumentLatencyEntry> getInstrument(const std::string& id);
    
    /**
     * @brief Update instrument profile
     * @param entry Updated instrument data
     * @return true if successful
     * @note Thread-safe
     * @note Updates cache
     */
    bool updateInstrument(const InstrumentLatencyEntry& entry);
    
    /**
     * @brief Delete instrument
     * @param id Instrument ID
     * @return true if successful
     * @note Thread-safe
     * @note Clears from cache
     */
    bool deleteInstrument(const std::string& id);
    
    // ========================================================================
    // QUERY OPERATIONS
    // ========================================================================
    
    /**
     * @brief List all instruments
     * @return Vector of all instruments
     * @note Thread-safe
     */
    std::vector<InstrumentLatencyEntry> listAll();
    
    /**
     * @brief List instruments by device
     * @param deviceId Device ID
     * @return Vector of instruments
     * @note Thread-safe
     */
    std::vector<InstrumentLatencyEntry> listByDevice(const std::string& deviceId);
    
    /**
     * @brief List instruments by channel
     * @param channel MIDI channel (0-15)
     * @return Vector of instruments
     * @note Thread-safe
     */
    std::vector<InstrumentLatencyEntry> listByChannel(int channel);
    
    /**
     * @brief List enabled instruments
     * @return Vector of enabled instruments
     * @note Thread-safe
     */
    std::vector<InstrumentLatencyEntry> listEnabled();
    
    /**
     * @brief Get instrument by device and channel
     * @param deviceId Device ID
     * @param channel MIDI channel
     * @return Optional entry
     * @note Thread-safe
     */
    std::optional<InstrumentLatencyEntry> getByDeviceAndChannel(
        const std::string& deviceId, int channel);
    
    // ========================================================================
    // CACHE MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Clear cache and reload from database
     * @note Thread-safe
     */
    void refreshCache();
    
    /**
     * @brief Clear cache only
     * @note Thread-safe
     */
    void clearCache();
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
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
     * @brief Load all instruments into cache
     */
    void loadCache();
    
    /**
     * @brief Parse database row to entry
     */
    InstrumentLatencyEntry parseRow(const DatabaseRow& row);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Reference to database
    Database& database_;
    
    /// In-memory cache (id -> entry)
    std::map<std::string, InstrumentLatencyEntry> cache_;
    
    /// Mutex for thread-safety
    mutable std::mutex mutex_;
};

} // namespace midiMind
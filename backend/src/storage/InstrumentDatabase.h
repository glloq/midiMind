// ============================================================================
// File: backend/src/storage/InstrumentDatabase.h  
// Version: 4.3.0 - THREAD-SAFE
// ============================================================================
//
// Changes v4.3.0:
//   - Fixed: updateLatencyMs() race condition with atomic operation
//   - Fixed: Moved inline methods to .cpp for proper thread-safety
//   - Fixed: Documented mutex protection for cache
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

struct InstrumentLatencyEntry {
    std::string id;
    std::string deviceId;
    int channel;
    std::string name;
    std::string instrumentType;
    int64_t avgLatency;      // µs
    int64_t minLatency;      // µs
    int64_t maxLatency;      // µs
    double jitter;
    double stdDeviation;
    int measurementCount;
    double calibrationConfidence;
    std::string lastCalibration;
    std::string calibrationMethod;
    int64_t compensationOffset;  // µs
    bool autoCalibration;
    bool enabled;
    std::string measurementHistory;
    std::string createdAt;
    std::string updatedAt;
    
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

/**
 * @class InstrumentDatabase
 * @brief Thread-safe instrument latency profile manager with caching
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Internal cache protected by mutex_
 * - Can be safely called from multiple threads
 */
class InstrumentDatabase {
public:
    explicit InstrumentDatabase(Database& database);
    ~InstrumentDatabase();
    
    InstrumentDatabase(const InstrumentDatabase&) = delete;
    InstrumentDatabase& operator=(const InstrumentDatabase&) = delete;
    
    // CRUD operations
    bool createInstrument(const InstrumentLatencyEntry& entry);
    std::optional<InstrumentLatencyEntry> getInstrument(const std::string& id);
    bool updateInstrument(const InstrumentLatencyEntry& entry);
    bool deleteInstrument(const std::string& id);
    
    // Query operations
    std::vector<InstrumentLatencyEntry> listAll();
    std::vector<InstrumentLatencyEntry> listByDevice(const std::string& deviceId);
    std::vector<InstrumentLatencyEntry> listByChannel(int channel);
    std::vector<InstrumentLatencyEntry> listEnabled();
    std::optional<InstrumentLatencyEntry> getByDeviceAndChannel(
        const std::string& deviceId, int channel);
    
    /**
     * @brief Get all instrument profiles (alias for listAll)
     * @return Vector of all instrument entries
     * @note Thread-safe
     */
    std::vector<InstrumentLatencyEntry> getAllProfiles();
    
    /**
     * @brief Update latency for instrument (atomic operation)
     * @param id Instrument ID
     * @param latencyMs Latency in milliseconds
     * @note Thread-safe - performs atomic update with single lock
     * @note Converts ms to µs internally
     */
    void updateLatencyMs(const std::string& id, double latencyMs);
    
    // Cache management
    void refreshCache();
    void clearCache();
    
    // Statistics
    json getStatistics() const;

private:
    /**
     * @brief Load cache from database (assumes mutex already locked)
     */
    void loadCacheInternal();
    
    /**
     * @brief Parse database row to entry
     * @throws std::exception on parse error
     */
    InstrumentLatencyEntry parseRow(const DatabaseRow& row);
    
    Database& database_;
    
    /// Cache of instrument entries (protected by mutex_)
    std::map<std::string, InstrumentLatencyEntry> cache_;
    
    /// Mutex protecting cache_ and database operations
    mutable std::mutex mutex_;
};

} // namespace midiMind
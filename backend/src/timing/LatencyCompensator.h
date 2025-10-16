// ============================================================================
// File: backend/src/timing/LatencyCompensator.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Latency compensation system for MIDI devices and instruments.
//   Provides both device-level and instrument-level compensation.
//
// Features:
//   - Device-level latency tracking (transport)
//   - Instrument-level latency tracking (intrinsic + transport)
//   - Automatic compensation calculation
//   - Manual compensation override
//   - Outlier detection
//   - Database persistence
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Added instrument-level compensation
//   - Integration with InstrumentDatabase
//   - Sync score calculation
//   - Enhanced statistics
//
// ============================================================================

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <nlohmann/json.hpp>
#include "InstrumentLatencyProfile.h"
#include "../storage/InstrumentDatabase.h"
#include "../core/Logger.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURE: DeviceLatencyProfile
// ============================================================================

/**
 * @struct DeviceLatencyProfile
 * @brief Latency profile for a MIDI device (transport layer)
 */
struct DeviceLatencyProfile {
    /// Device identifier
    std::string deviceId;
    
    /// Average latency (microseconds)
    uint64_t averageLatency;
    
    /// Minimum latency
    uint64_t minLatency;
    
    /// Maximum latency
    uint64_t maxLatency;
    
    /// Jitter (standard deviation)
    double jitter;
    
    /// Measurement count
    uint64_t measurementCount;
    
    /// Compensation offset (microseconds)
    int64_t compensationOffset;
    
    /// Auto compensation enabled
    bool autoCompensation;
    
    /// Latency history
    std::deque<uint64_t> latencyHistory;
    
    /**
     * @brief Constructor
     */
    DeviceLatencyProfile()
        : deviceId("")
        , averageLatency(0)
        , minLatency(UINT64_MAX)
        , maxLatency(0)
        , jitter(0.0)
        , measurementCount(0)
        , compensationOffset(0)
        , autoCompensation(true)
    {}
    
    /**
     * @brief Add measurement
     */
    void addMeasurement(uint64_t latency);
    
    /**
     * @brief Calculate optimal compensation
     */
    int64_t calculateOptimalCompensation() const;
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const;
};

// ============================================================================
// CLASS: LatencyCompensator
// ============================================================================

/**
 * @class LatencyCompensator
 * @brief Latency compensation manager for devices and instruments
 * 
 * Provides two-level latency compensation:
 * 1. Device level: Transport latency (USB, network, etc.)
 * 2. Instrument level: Intrinsic latency (VST, plugin, etc.)
 * 
 * Architecture:
 * ```
 * LatencyCompensator
 * ├── Device Profiles (transport latency)
 * │   ├── USB Device: 3ms
 * │   ├── WiFi Device: 15ms
 * │   └── BT Device: 30ms
 * │
 * └── Instrument Profiles (total compensation)
 *     ├── Piano (USB + 5ms VST) = -8ms
 *     ├── Strings (USB + 15ms VST) = -18ms
 *     └── Drums (BT + 3ms HW) = -33ms
 * ```
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Separate mutexes for devices and instruments
 * 
 * Example:
 * ```cpp
 * LatencyCompensator comp(instrumentDb);
 * 
 * // Register device
 * comp.registerDevice("usb_kbd");
 * 
 * // Register instrument
 * InstrumentLatencyProfile piano("piano_001", "usb_kbd", 0);
 * piano.intrinsicLatency = 5000;  // 5ms VST
 * comp.registerInstrument(piano);
 * 
 * // Get compensation
 * int64_t offset = comp.getInstrumentCompensation("piano_001");
 * // Returns: -8ms (3ms device + 5ms VST)
 * ```
 */
class LatencyCompensator {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param instrumentDb Reference to instrument database
     */
    explicit LatencyCompensator(InstrumentDatabase& instrumentDb);
    
    /**
     * @brief Destructor
     */
    ~LatencyCompensator();
    
    // Disable copy
    LatencyCompensator(const LatencyCompensator&) = delete;
    LatencyCompensator& operator=(const LatencyCompensator&) = delete;
    
    // ========================================================================
    // DEVICE MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Register MIDI device
     * @param deviceId Device identifier
     * @return true if registered
     */
    bool registerDevice(const std::string& deviceId);
    
    /**
     * @brief Unregister device
     * @param deviceId Device identifier
     */
    void unregisterDevice(const std::string& deviceId);
    
    /**
     * @brief Check if device is registered
     * @param deviceId Device identifier
     * @return true if registered
     */
    bool isDeviceRegistered(const std::string& deviceId) const;
    
    // ========================================================================
    // INSTRUMENT MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Register instrument
     * @param profile Instrument latency profile
     * @return true if registered
     */
    bool registerInstrument(const InstrumentLatencyProfile& profile);
    
    /**
     * @brief Unregister instrument
     * @param instrumentId Instrument identifier
     */
    void unregisterInstrument(const std::string& instrumentId);
    
    /**
     * @brief Check if instrument is registered
     * @param instrumentId Instrument identifier
     * @return true if registered
     */
    bool isInstrumentRegistered(const std::string& instrumentId) const;
    
    // ========================================================================
    // DEVICE LATENCY MEASUREMENT
    // ========================================================================
    
    /**
     * @brief Record device latency measurement
     * @param deviceId Device identifier
     * @param latencyUs Latency in microseconds
     */
    void recordDeviceLatency(const std::string& deviceId, uint64_t latencyUs);
    
    /**
     * @brief Get device compensation offset
     * @param deviceId Device identifier
     * @return int64_t Offset in microseconds
     */
    int64_t getDeviceCompensation(const std::string& deviceId) const;
    
    /**
     * @brief Set device compensation manually
     * @param deviceId Device identifier
     * @param offsetUs Offset in microseconds
     */
    void setDeviceCompensation(const std::string& deviceId, int64_t offsetUs);
    
    // ========================================================================
    // INSTRUMENT LATENCY MEASUREMENT
    // ========================================================================
    
    /**
     * @brief Record instrument latency measurement
     * @param instrumentId Instrument identifier
     * @param latencyUs Latency in microseconds
     */
    void recordInstrumentLatency(const std::string& instrumentId, uint64_t latencyUs);
    
    /**
     * @brief Get instrument compensation offset
     * @param instrumentId Instrument identifier
     * @return int64_t Total offset in microseconds (device + intrinsic)
     * @note Falls back to device compensation if instrument not found
     */
    int64_t getInstrumentCompensation(const std::string& instrumentId) const;
    
    /**
     * @brief Set instrument compensation manually
     * @param instrumentId Instrument identifier
     * @param offsetUs Total offset in microseconds
     */
    void setInstrumentCompensation(const std::string& instrumentId, int64_t offsetUs);
    
    
    // ========================================================================
    // PROFILES
    // ========================================================================
    
    /**
     * @brief Get device profile
     * @param deviceId Device identifier
     * @return DeviceLatencyProfile Device profile
     */
    DeviceLatencyProfile getDeviceProfile(const std::string& deviceId) const;
    
    /**
     * @brief Get instrument profile
     * @param instrumentId Instrument identifier
     * @return InstrumentLatencyProfile Instrument profile
     */
    InstrumentLatencyProfile getInstrumentProfile(const std::string& instrumentId) const;
    
    /**
     * @brief Get all instrument profiles
     * @return std::vector<InstrumentLatencyProfile> All profiles
     */
    std::vector<InstrumentLatencyProfile> getAllInstrumentProfiles() const;
    
    // ========================================================================
    // PERSISTENCE
    // ========================================================================
    
    /**
     * @brief Save all instrument profiles to database
     * @return true if saved successfully
     */
    bool saveInstrumentProfiles();
    
    /**
     * @brief Load all instrument profiles from database
     * @return true if loaded successfully
     */
    bool loadInstrumentProfiles();
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get device statistics
     * @param deviceId Device identifier
     * @return json Statistics
     */
    json getDeviceStatistics(const std::string& deviceId) const;
    
    /**
     * @brief Get instrument statistics
     * @param instrumentId Instrument identifier
     * @return json Statistics
     */
    json getInstrumentStatistics(const std::string& instrumentId) const;
    
    /**
     * @brief Get all statistics
     * @return json Complete statistics
     */
    json getAllStatistics() const;
    
    /**
     * @brief Calculate synchronization score
     * @return double Score from 0 (bad) to 100 (perfect)
     * @note Based on variance between instrument compensations
     */
    double getSyncScore() const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Set history size
     * @param size Number of measurements to keep
     */
    void setHistorySize(size_t size) {
        historySize_ = size;
    }
    
    /**
     * @brief Enable/disable outlier detection
     * @param enabled true to enable
     */
    void setOutlierDetection(bool enabled) {
        outlierDetectionEnabled_ = enabled;
    }
    
private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Check if measurement is an outlier
     */
    bool isOutlier(const DeviceLatencyProfile& profile, uint64_t latency) const;
    
    /**
     * @brief Update device statistics
     */
    void updateDeviceStatistics(DeviceLatencyProfile& profile);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Device latency profiles (transport layer)
    std::unordered_map<std::string, DeviceLatencyProfile> devices_;
    
    /// Instrument latency profiles (intrinsic + transport)
    std::unordered_map<std::string, InstrumentLatencyProfile> instruments_;
    
    /// Reference to instrument database
    InstrumentDatabase& instrumentDb_;
    
    /// Mutex for device operations
    mutable std::mutex deviceMutex_;
    
    /// Mutex for instrument operations
    mutable std::mutex instrumentMutex_;
    
    /// History size
    size_t historySize_;
    
    /// Outlier detection enabled
    bool outlierDetectionEnabled_;
    
    /// Outlier threshold (standard deviations)
    double outlierThreshold_;
};

} // namespace midiMind
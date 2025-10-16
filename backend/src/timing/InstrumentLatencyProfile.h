// ============================================================================
// File: backend/src/timing/InstrumentLatencyProfile.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Instrument-level latency profile structure.
//   Represents latency characteristics for a specific instrument on a device.
//
// Features:
//   - Per-instrument latency tracking
//   - Intrinsic (VST/plugin) and transport (device) latency separation
//   - Calibration history
//   - Statistics and confidence metrics
//   - JSON serialization
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Initial implementation for instrument-level compensation
//   - Separation of intrinsic and transport latency
//   - Enhanced calibration tracking
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <cstdint>
#include <ctime>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURE: CalibrationPoint
// ============================================================================

/**
 * @struct CalibrationPoint
 * @brief Single calibration measurement point
 */
struct CalibrationPoint {
    /// Timestamp of calibration
    std::time_t timestamp;
    
    /// Measured latency (microseconds)
    uint64_t measuredLatency;
    
    /// Applied compensation (microseconds)
    int64_t appliedCompensation;
    
    /// Confidence level (0.0 - 1.0)
    double confidence;
    
    /// Calibration method used
    std::string method;
    
    /**
     * @brief Default constructor
     */
    CalibrationPoint()
        : timestamp(0)
        , measuredLatency(0)
        , appliedCompensation(0)
        , confidence(0.0)
        , method("unknown")
    {}
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const {
        return {
            {"timestamp", timestamp},
            {"measured_latency", measuredLatency},
            {"applied_compensation", appliedCompensation},
            {"confidence", confidence},
            {"method", method}
        };
    }
    
    /**
     * @brief Create from JSON
     */
    static CalibrationPoint fromJson(const json& j) {
        CalibrationPoint point;
        point.timestamp = j.value("timestamp", 0);
        point.measuredLatency = j.value("measured_latency", 0);
        point.appliedCompensation = j.value("applied_compensation", 0);
        point.confidence = j.value("confidence", 0.0);
        point.method = j.value("method", "unknown");
        return point;
    }
};

// ============================================================================
// STRUCTURE: InstrumentLatencyProfile
// ============================================================================

/**
 * @struct InstrumentLatencyProfile
 * @brief Complete latency profile for a specific instrument
 * 
 * Represents the latency characteristics of a single instrument (e.g., a 
 * specific VST on a specific MIDI channel of a device).
 * 
 * Latency Composition:
 * - intrinsicLatency: VST/plugin processing time
 * - transportLatency: Device/network transmission time
 * - totalCompensation: Combined compensation offset
 * 
 * Example:
 * ```cpp
 * InstrumentLatencyProfile piano;
 * piano.instrumentId = "piano_001";
 * piano.deviceId = "usb_keyboard";
 * piano.midiChannel = 0;
 * piano.intrinsicLatency = 5000;  // 5ms VST latency
 * piano.transportLatency = 3000;  // 3ms USB latency
 * piano.calculateTotalCompensation();  // = -8ms
 * ```
 */
struct InstrumentLatencyProfile {
    // ========================================================================
    // IDENTIFIERS
    // ========================================================================
    
    /// Unique instrument identifier
    std::string instrumentId;
    
    /// Parent device identifier
    std::string deviceId;
    
    /// MIDI channel (0-15)
    int midiChannel;
    
    // ========================================================================
    // METADATA
    // ========================================================================
    
    /// Human-readable instrument name
    std::string instrumentName;
    
    /// Instrument type (synth, sampler, drum, etc.)
    std::string instrumentType;
    
    // ========================================================================
    // LATENCY MEASUREMENTS (microseconds)
    // ========================================================================
    
    /// Intrinsic latency (VST/plugin processing)
    uint64_t intrinsicLatency;
    
    /// Transport latency (device/network)
    uint64_t transportLatency;
    
    /// Total compensation offset (negative to advance)
    int64_t totalCompensation;
    
    /// Average measured latency
    uint64_t avgLatency;
    
    /// Minimum measured latency
    uint64_t minLatency;
    
    /// Maximum measured latency
    uint64_t maxLatency;
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /// Jitter (standard deviation)
    double jitter;
    
    /// Standard deviation
    double stdDeviation;
    
    /// Number of measurements taken
    uint64_t measurementCount;
    
    /// Calibration confidence (0.0 - 1.0)
    double calibrationConfidence;
    
    // ========================================================================
    // HISTORY
    // ========================================================================
    
    /// Recent latency measurements
    std::deque<uint64_t> latencyHistory;
    
    /// Calibration history
    std::deque<CalibrationPoint> calibrationHistory;
    
    /// Maximum history size
    static constexpr size_t MAX_HISTORY_SIZE = 100;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /// Auto-calibration enabled
    bool autoCalibration;
    
    /// Profile enabled
    bool enabled;
    
    /// Last calibration timestamp
    std::time_t lastCalibration;
    
    /// Calibration method
    std::string calibrationMethod;
    
    // ========================================================================
    // CONSTRUCTORS
    // ========================================================================
    
    /**
     * @brief Default constructor
     */
    InstrumentLatencyProfile()
        : instrumentId("")
        , deviceId("")
        , midiChannel(0)
        , instrumentName("")
        , instrumentType("unknown")
        , intrinsicLatency(0)
        , transportLatency(0)
        , totalCompensation(0)
        , avgLatency(0)
        , minLatency(UINT64_MAX)
        , maxLatency(0)
        , jitter(0.0)
        , stdDeviation(0.0)
        , measurementCount(0)
        , calibrationConfidence(0.0)
        , autoCalibration(true)
        , enabled(true)
        , lastCalibration(0)
        , calibrationMethod("none")
    {}
    
    /**
     * @brief Constructor with identifiers
     */
    InstrumentLatencyProfile(const std::string& id, 
                            const std::string& device, 
                            int channel)
        : InstrumentLatencyProfile()
    {
        instrumentId = id;
        deviceId = device;
        midiChannel = channel;
    }
    
    // ========================================================================
    // METHODS
    // ========================================================================
    
    /**
     * @brief Add latency measurement
     * @param latency Measured latency in microseconds
     */
    void addMeasurement(uint64_t latency) {
        // Add to history
        latencyHistory.push_back(latency);
        
        // Limit history size
        if (latencyHistory.size() > MAX_HISTORY_SIZE) {
            latencyHistory.pop_front();
        }
        
        // Update count
        measurementCount++;
        
        // Update min/max
        if (latency < minLatency) {
            minLatency = latency;
        }
        if (latency > maxLatency) {
            maxLatency = latency;
        }
        
        // Recalculate statistics
        updateStatistics();
    }
    
    /**
     * @brief Update statistics from history
     */
    void updateStatistics() {
        if (latencyHistory.empty()) {
            return;
        }
        
        // Calculate average
        uint64_t sum = 0;
        for (uint64_t lat : latencyHistory) {
            sum += lat;
        }
        avgLatency = sum / latencyHistory.size();
        
        // Calculate standard deviation and jitter
        if (latencyHistory.size() > 1) {
            double variance = 0.0;
            for (uint64_t lat : latencyHistory) {
                double diff = static_cast<double>(lat) - static_cast<double>(avgLatency);
                variance += diff * diff;
            }
            variance /= latencyHistory.size();
            stdDeviation = std::sqrt(variance);
            jitter = stdDeviation;
        }
        
        // Update calibration confidence based on measurement count and jitter
        updateCalibrationConfidence();
    }
    
    /**
     * @brief Calculate optimal compensation offset
     * @return int64_t Optimal compensation in microseconds (negative)
     */
    int64_t calculateOptimalCompensation() const {
        // Total compensation = -(intrinsic + transport)
        return -static_cast<int64_t>(intrinsicLatency + transportLatency);
    }
    
    /**
     * @brief Update total compensation based on current measurements
     */
    void calculateTotalCompensation() {
        totalCompensation = calculateOptimalCompensation();
    }
    
    /**
     * @brief Update calibration confidence
     * 
     * Confidence based on:
     * - Number of measurements (more = better)
     * - Jitter (lower = better)
     * - Recency of calibration
     */
    void updateCalibrationConfidence() {
        double confidence = 0.0;
        
        // Factor 1: Measurement count (0-40% of confidence)
        double countFactor = std::min(1.0, measurementCount / 20.0) * 0.4;
        
        // Factor 2: Jitter quality (0-40% of confidence)
        // Good jitter < 500µs, bad jitter > 5000µs
        double jitterFactor = 0.0;
        if (jitter < 500.0) {
            jitterFactor = 0.4;
        } else if (jitter < 5000.0) {
            jitterFactor = 0.4 * (1.0 - (jitter - 500.0) / 4500.0);
        }
        
        // Factor 3: Calibration recency (0-20% of confidence)
        double recencyFactor = 0.0;
        if (lastCalibration > 0) {
            std::time_t now = std::time(nullptr);
            double hoursSinceCalibration = std::difftime(now, lastCalibration) / 3600.0;
            
            // Full confidence if < 24h, decays over 7 days
            if (hoursSinceCalibration < 24.0) {
                recencyFactor = 0.2;
            } else if (hoursSinceCalibration < 168.0) {  // 7 days
                recencyFactor = 0.2 * (1.0 - (hoursSinceCalibration - 24.0) / 144.0);
            }
        }
        
        confidence = countFactor + jitterFactor + recencyFactor;
        calibrationConfidence = std::min(1.0, std::max(0.0, confidence));
    }
    
    /**
     * @brief Add calibration point to history
     */
    void addCalibrationPoint(const CalibrationPoint& point) {
        calibrationHistory.push_back(point);
        
        // Limit calibration history
        if (calibrationHistory.size() > 20) {
            calibrationHistory.pop_front();
        }
        
        lastCalibration = point.timestamp;
        calibrationMethod = point.method;
    }
    
    // ========================================================================
    // SERIALIZATION
    // ========================================================================
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const {
        json j = {
            {"instrument_id", instrumentId},
            {"device_id", deviceId},
            {"channel", midiChannel},
            {"name", instrumentName},
            {"type", instrumentType},
            {"intrinsic_latency", intrinsicLatency},
            {"transport_latency", transportLatency},
            {"total_compensation", totalCompensation},
            {"avg_latency", avgLatency},
            {"min_latency", minLatency},
            {"max_latency", maxLatency},
            {"jitter", jitter},
            {"std_deviation", stdDeviation},
            {"measurement_count", measurementCount},
            {"calibration_confidence", calibrationConfidence},
            {"auto_calibration", autoCalibration},
            {"enabled", enabled},
            {"last_calibration", lastCalibration},
            {"calibration_method", calibrationMethod}
        };
        
        // Add calibration history
        json calibHistory = json::array();
        for (const auto& point : calibrationHistory) {
            calibHistory.push_back(point.toJson());
        }
        j["calibration_history"] = calibHistory;
        
        return j;
    }
    
    /**
     * @brief Create from JSON
     */
    static InstrumentLatencyProfile fromJson(const json& j) {
        InstrumentLatencyProfile profile;
        
        profile.instrumentId = j.value("instrument_id", "");
        profile.deviceId = j.value("device_id", "");
        profile.midiChannel = j.value("channel", 0);
        profile.instrumentName = j.value("name", "");
        profile.instrumentType = j.value("type", "unknown");
        
        profile.intrinsicLatency = j.value("intrinsic_latency", 0);
        profile.transportLatency = j.value("transport_latency", 0);
        profile.totalCompensation = j.value("total_compensation", 0);
        
        profile.avgLatency = j.value("avg_latency", 0);
        profile.minLatency = j.value("min_latency", UINT64_MAX);
        profile.maxLatency = j.value("max_latency", 0);
        
        profile.jitter = j.value("jitter", 0.0);
        profile.stdDeviation = j.value("std_deviation", 0.0);
        profile.measurementCount = j.value("measurement_count", 0);
        profile.calibrationConfidence = j.value("calibration_confidence", 0.0);
        
        profile.autoCalibration = j.value("auto_calibration", true);
        profile.enabled = j.value("enabled", true);
        profile.lastCalibration = j.value("last_calibration", 0);
        profile.calibrationMethod = j.value("calibration_method", "none");
        
        // Load calibration history
        if (j.contains("calibration_history") && j["calibration_history"].is_array()) {
            for (const auto& pointJson : j["calibration_history"]) {
                profile.calibrationHistory.push_back(
                    CalibrationPoint::fromJson(pointJson)
                );
            }
        }
        
        return profile;
    }
    
    /**
     * @brief Convert to database format (simplified)
     */
    json toDatabase() const {
        return {
            {"id", instrumentId},
            {"device_id", deviceId},
            {"channel", midiChannel},
            {"name", instrumentName},
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
            {"compensation_offset", totalCompensation},
            {"auto_calibration", autoCalibration},
            {"enabled", enabled}
        };
    }
};

} // namespace midiMind
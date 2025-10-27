// ============================================================================
// File: backend/src/timing/LatencyCompensator.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of LatencyCompensator.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Added instrument-level compensation
//   - Database integration
//   - Sync score calculation
//   - Fixed thread-safety issues
//
// ============================================================================

#include "LatencyCompensator.h"
#include "TimestampManager.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace midiMind {

// ============================================================================
// DeviceLatencyProfile METHODS
// ============================================================================

void DeviceLatencyProfile::addMeasurement(uint64_t latency) {
    // Add to history
    latencyHistory.push_back(latency);
    
    // Limit history size
    if (latencyHistory.size() > 100) {
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
    
    // Calculate average
    uint64_t sum = 0;
    for (uint64_t l : latencyHistory) {
        sum += l;
    }
    averageLatency = sum / latencyHistory.size();
    
    // Calculate jitter (standard deviation)
    if (latencyHistory.size() > 1) {
        double variance = 0.0;
        for (uint64_t l : latencyHistory) {
            double diff = static_cast<double>(l) - static_cast<double>(averageLatency);
            variance += diff * diff;
        }
        variance /= latencyHistory.size();
        jitter = std::sqrt(variance);
    }
}

int64_t DeviceLatencyProfile::calculateOptimalCompensation() const {
    if (!autoCompensation || measurementCount < 5) {
        return compensationOffset;
    }
    
    // Compensation = negative of average latency
    return -static_cast<int64_t>(averageLatency);
}

json DeviceLatencyProfile::toJson() const {
    return {
        {"device_id", deviceId},
        {"average_latency", averageLatency},
        {"min_latency", minLatency},
        {"max_latency", maxLatency},
        {"jitter", jitter},
        {"measurement_count", measurementCount},
        {"compensation_offset", compensationOffset},
        {"auto_compensation", autoCompensation}
    };
}

// ============================================================================
// LatencyCompensator CONSTRUCTOR / DESTRUCTOR
// ============================================================================

LatencyCompensator::LatencyCompensator(InstrumentDatabase& instrumentDb)
    : instrumentDb_(instrumentDb)
    , historySize_(100)
    , outlierDetectionEnabled_(true)
    , outlierThreshold_(3.0)
    , enabled_(true)
    , globalOffsetMs_(0.0)
{
    Logger::info("LatencyCompensator", "LatencyCompensator created");
    
    // Load instrument profiles from database
    loadInstrumentProfiles();
}

LatencyCompensator::~LatencyCompensator() {
    Logger::info("LatencyCompensator", "LatencyCompensator destroyed");
    
    // Save profiles before destroying
    saveInstrumentProfiles();
}

// ============================================================================
// DEVICE MANAGEMENT
// ============================================================================

bool LatencyCompensator::registerDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    if (devices_.find(deviceId) != devices_.end()) {
        Logger::warning("LatencyCompensator", "Device already registered: " + deviceId);
        return false;
    }
    
    DeviceLatencyProfile profile;
    profile.deviceId = deviceId;
    devices_[deviceId] = profile;
    
    Logger::info("LatencyCompensator", "Device registered: " + deviceId);
    return true;
}

void LatencyCompensator::unregisterDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    auto it = devices_.find(deviceId);
    if (it != devices_.end()) {
        devices_.erase(it);
        Logger::info("LatencyCompensator", "Device unregistered: " + deviceId);
    }
}

bool LatencyCompensator::isDeviceRegistered(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    return devices_.find(deviceId) != devices_.end();
}

// ============================================================================
// INSTRUMENT MANAGEMENT
// ============================================================================

bool LatencyCompensator::registerInstrument(const InstrumentLatencyProfile& profile) {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    if (instruments_.find(profile.instrumentId) != instruments_.end()) {
        Logger::warning("LatencyCompensator", 
                    "Instrument already registered: " + profile.instrumentId);
        return false;
    }
    
    instruments_[profile.instrumentId] = profile;
    
    Logger::info("LatencyCompensator", "Instrument registered: " + profile.instrumentId);
    return true;
}

void LatencyCompensator::unregisterInstrument(const std::string& instrumentId) {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    auto it = instruments_.find(instrumentId);
    if (it != instruments_.end()) {
        instruments_.erase(it);
        Logger::info("LatencyCompensator", "Instrument unregistered: " + instrumentId);
    }
}

bool LatencyCompensator::isInstrumentRegistered(const std::string& instrumentId) const {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    return instruments_.find(instrumentId) != instruments_.end();
}

// ============================================================================
// DEVICE LATENCY MEASUREMENT
// ============================================================================

void LatencyCompensator::recordDeviceLatency(const std::string& deviceId, uint64_t latencyUs) {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    auto it = devices_.find(deviceId);
    if (it == devices_.end()) {
        Logger::warning("LatencyCompensator", "Device not registered: " + deviceId);
        return;
    }
    
    DeviceLatencyProfile& profile = it->second;
    
    // Detect outliers (load atomic value)
    bool outlierDetection = outlierDetectionEnabled_.load();
    if (outlierDetection && isOutlier(profile, latencyUs)) {
        Logger::debug("LatencyCompensator", 
                     "Outlier detected for " + deviceId + ": " + 
                     std::to_string(latencyUs) + "Âµs");
        return;
    }
    
    // Add measurement
    profile.addMeasurement(latencyUs);
    
    // Recalculate compensation if auto
    if (profile.autoCompensation) {
        profile.compensationOffset = profile.calculateOptimalCompensation();
    }
    
    Logger::debug("LatencyCompensator", 
                 deviceId + " latency: " + std::to_string(latencyUs) + "Âµs, " +
                 "avg: " + std::to_string(profile.averageLatency) + "Âµs");
}

int64_t LatencyCompensator::getDeviceCompensation(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    auto it = devices_.find(deviceId);
    if (it == devices_.end()) {
        return 0;
    }
    
    return it->second.compensationOffset;
}

void LatencyCompensator::setDeviceCompensation(const std::string& deviceId, int64_t offsetUs) {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    auto it = devices_.find(deviceId);
    if (it != devices_.end()) {
        it->second.compensationOffset = offsetUs;
        it->second.autoCompensation = false;
        
        Logger::info("LatencyCompensator", 
                    deviceId + " manual compensation set to " + 
                    std::to_string(offsetUs) + "Âµs");
    }
}

// ============================================================================
// INSTRUMENT LATENCY MEASUREMENT
// ============================================================================

void LatencyCompensator::recordInstrumentLatency(const std::string& instrumentId, 
                                                 uint64_t latencyUs) {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    auto it = instruments_.find(instrumentId);
    if (it == instruments_.end()) {
        Logger::warning("LatencyCompensator", "Instrument not registered: " + instrumentId);
        return;
    }
    
    InstrumentLatencyProfile& profile = it->second;
    
    // Add calibration point
    CalibrationPoint point;
    point.timestamp = std::time(nullptr);
    point.measuredLatency = latencyUs;
    point.confidence = 0.9;  // High confidence for manual measurement
    point.method = "manual";
    
    profile.calibrationHistory.push_back(point);
    
    // Limit history size
    size_t historySize = historySize_.load();
    if (profile.calibrationHistory.size() > historySize) {
        profile.calibrationHistory.erase(profile.calibrationHistory.begin());
    }
    
    // Update statistics
    profile.measurementCount++;
    
    // Update min/max
    if (latencyUs < profile.minLatency) {
        profile.minLatency = latencyUs;
    }
    if (latencyUs > profile.maxLatency) {
        profile.maxLatency = latencyUs;
    }
    
    // Recalculate average latency
    uint64_t sum = 0;
    for (const auto& p : profile.calibrationHistory) {
        sum += p.measuredLatency;
    }
    profile.avgLatency = sum / profile.calibrationHistory.size();
    
    // Calculate standard deviation
    if (profile.calibrationHistory.size() > 1) {
        double variance = 0.0;
        for (const auto& p : profile.calibrationHistory) {
            double diff = static_cast<double>(p.measuredLatency) - 
                         static_cast<double>(profile.avgLatency);
            variance += diff * diff;
        }
        variance /= profile.calibrationHistory.size();
        profile.stdDeviation = std::sqrt(variance);
        profile.jitter = profile.stdDeviation;  // Jitter â‰ˆ stddev for now
    }
    
    // Update compensation if auto calibration enabled
    if (profile.autoCalibration) {
        profile.totalCompensation = -static_cast<int64_t>(profile.avgLatency);
    }
    
    // Update calibration confidence
    if (profile.measurementCount >= 10) {
        profile.calibrationConfidence = std::min(1.0, 
            0.5 + (0.5 * std::min(profile.measurementCount, 50UL) / 50.0));
    } else {
        profile.calibrationConfidence = profile.measurementCount * 0.05;
    }
    
    profile.lastCalibration = std::time(nullptr);
    
    Logger::debug("LatencyCompensator", 
                 instrumentId + " latency: " + std::to_string(latencyUs) + "Âµs, " +
                 "avg: " + std::to_string(profile.avgLatency) + "Âµs");
}

int64_t LatencyCompensator::getInstrumentCompensation(const std::string& instrumentId) const {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    auto it = instruments_.find(instrumentId);
    if (it == instruments_.end()) {
        return 0;
    }
    
    const InstrumentLatencyProfile& profile = it->second;
    
    if (!profile.enabled) {
        return 0;
    }
    
    return profile.totalCompensation;
}

void LatencyCompensator::setInstrumentCompensation(const std::string& instrumentId, 
                                                   int64_t offsetUs) {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    auto it = instruments_.find(instrumentId);
    if (it != instruments_.end()) {
        it->second.totalCompensation = offsetUs;
        it->second.autoCalibration = false;
        
        Logger::info("LatencyCompensator", 
                    instrumentId + " manual compensation set to " + 
                    std::to_string(offsetUs) + "Âµs");
    }
}

// ============================================================================
// PROFILES
// ============================================================================

DeviceLatencyProfile LatencyCompensator::getDeviceProfile(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    auto it = devices_.find(deviceId);
    if (it != devices_.end()) {
        return it->second;
    }
    
    return DeviceLatencyProfile();
}

InstrumentLatencyProfile LatencyCompensator::getInstrumentProfile(
    const std::string& instrumentId) const {
    
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    auto it = instruments_.find(instrumentId);
    if (it != instruments_.end()) {
        return it->second;
    }
    
    return InstrumentLatencyProfile();
}

std::vector<InstrumentLatencyProfile> LatencyCompensator::getAllInstrumentProfiles() const {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    std::vector<InstrumentLatencyProfile> profiles;
    profiles.reserve(instruments_.size());
    
    for (const auto& [id, profile] : instruments_) {
        profiles.push_back(profile);
    }
    
    return profiles;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

bool LatencyCompensator::saveInstrumentProfiles() {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    Logger::info("LatencyCompensator", 
                "Saving " + std::to_string(instruments_.size()) + " instrument profiles...");
    
    int savedCount = 0;
    int failedCount = 0;
    
    for (const auto& [id, profile] : instruments_) {
        // Convert to database entry
        InstrumentLatencyEntry entry;
        
        entry.id = profile.instrumentId;
        entry.deviceId = profile.deviceId;
        entry.channel = profile.midiChannel;
        entry.name = profile.instrumentName;
        entry.instrumentType = profile.instrumentType;
        entry.avgLatency = profile.avgLatency;
        entry.minLatency = profile.minLatency;
        entry.maxLatency = profile.maxLatency;
        entry.jitter = profile.jitter;
        entry.stdDeviation = profile.stdDeviation;
        entry.measurementCount = profile.measurementCount;
        entry.calibrationConfidence = profile.calibrationConfidence;
        entry.compensationOffset = profile.totalCompensation;
        entry.autoCalibration = profile.autoCalibration;
        entry.enabled = profile.enabled;
        
        // Format timestamp
        if (profile.lastCalibration > 0) {
            char buffer[80];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", 
                         std::localtime(&profile.lastCalibration));
            entry.lastCalibration = buffer;
        }
        
        entry.calibrationMethod = profile.calibrationMethod;
        
        // Serialize calibration history
        json historyJson = json::array();
        for (const auto& point : profile.calibrationHistory) {
            historyJson.push_back(point.toJson());
        }
        entry.measurementHistory = historyJson.dump();
        
        // Save to database
        if (instrumentDb_.updateInstrument(entry)) {
            savedCount++;
        } else {
            // Try create if update failed
            if (instrumentDb_.createInstrument(entry)) {
                savedCount++;
            } else {
                failedCount++;
                Logger::warning("LatencyCompensator", "Failed to save: " + id);
            }
        }
    }
    
    Logger::info("LatencyCompensator", 
                "âœ“ Saved " + std::to_string(savedCount) + " profiles, " +
                std::to_string(failedCount) + " failed");
    
    return failedCount == 0;
}

bool LatencyCompensator::loadInstrumentProfiles() {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    Logger::info("LatencyCompensator", "Loading instrument profiles from database...");
    
    // Get all instruments from database
    auto entries = instrumentDb_.listAll();
    
    instruments_.clear();
    
    for (const auto& entry : entries) {
        InstrumentLatencyProfile profile;
        
        // Copy basic fields
        profile.instrumentId = entry.id;
        profile.deviceId = entry.deviceId;
        profile.midiChannel = entry.channel;
        profile.instrumentName = entry.name;
        profile.instrumentType = entry.instrumentType;
        profile.avgLatency = entry.avgLatency;
        profile.minLatency = entry.minLatency;
        profile.maxLatency = entry.maxLatency;
        profile.jitter = entry.jitter;
        profile.stdDeviation = entry.stdDeviation;
        profile.measurementCount = entry.measurementCount;
        profile.calibrationConfidence = entry.calibrationConfidence;
        profile.totalCompensation = entry.compensationOffset;
        profile.autoCalibration = entry.autoCalibration;
        profile.enabled = entry.enabled;
        profile.calibrationMethod = entry.calibrationMethod;
        
        // Parse timestamp
        if (!entry.lastCalibration.empty()) {
            std::tm tm = {};
            std::istringstream ss(entry.lastCalibration);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            profile.lastCalibration = std::mktime(&tm);
        }
        
        // Parse calibration history
        if (!entry.measurementHistory.empty()) {
            try {
                json historyJson = json::parse(entry.measurementHistory);
                if (historyJson.is_array()) {
                    for (const auto& pointJson : historyJson) {
                        profile.calibrationHistory.push_back(
                            CalibrationPoint::fromJson(pointJson)
                        );
                    }
                }
            } catch (const std::exception& e) {
                Logger::warning("LatencyCompensator", 
                           "Failed to parse calibration history for " + entry.id);
            }
        }
        
        instruments_[profile.instrumentId] = profile;
    }
    
    Logger::info("LatencyCompensator", 
                "âœ“ Loaded " + std::to_string(instruments_.size()) + " instrument profiles");
    
    return true;
}

// ============================================================================
// STATISTICS
// ============================================================================

json LatencyCompensator::getDeviceStatistics(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    auto it = devices_.find(deviceId);
    if (it != devices_.end()) {
        return it->second.toJson();
    }
    
    return json::object();
}

json LatencyCompensator::getInstrumentStatistics(const std::string& instrumentId) const {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    auto it = instruments_.find(instrumentId);
    if (it != instruments_.end()) {
        return it->second.toJson();
    }
    
    return json::object();
}

json LatencyCompensator::getAllStatistics() const {
    json stats;
    
    // Device statistics
    {
        std::lock_guard<std::mutex> lock(deviceMutex_);
        stats["device_count"] = devices_.size();
        stats["devices"] = json::array();
        
        for (const auto& [id, profile] : devices_) {
            stats["devices"].push_back(profile.toJson());
        }
    }
    
    // Instrument statistics
    {
        std::lock_guard<std::mutex> lock(instrumentMutex_);
        stats["instrument_count"] = instruments_.size();
        stats["instruments"] = json::array();
        
        int enabledCount = 0;
        int calibratedCount = 0;
        
        for (const auto& [id, profile] : instruments_) {
            stats["instruments"].push_back(profile.toJson());
            
            if (profile.enabled) enabledCount++;
            if (profile.calibrationConfidence >= 0.8) calibratedCount++;
        }
        
        stats["enabled_instruments"] = enabledCount;
        stats["calibrated_instruments"] = calibratedCount;
    }
    
    // Sync score
    stats["sync_score"] = getSyncScore();
    
    return stats;
}

double LatencyCompensator::getSyncScore() const {
    std::lock_guard<std::mutex> lock(instrumentMutex_);
    
    if (instruments_.empty()) {
        return 100.0;
    }
    
    // Collect enabled instrument compensations
    std::vector<int64_t> compensations;
    for (const auto& [id, profile] : instruments_) {
        if (profile.enabled) {
            compensations.push_back(profile.totalCompensation);
        }
    }
    
    if (compensations.size() < 2) {
        return 100.0;
    }
    
    // Calculate mean
    double mean = std::accumulate(compensations.begin(), compensations.end(), 0.0) / 
                  compensations.size();
    
    // Calculate standard deviation
    double variance = 0.0;
    for (auto comp : compensations) {
        variance += std::pow(comp - mean, 2);
    }
    variance /= compensations.size();
    double stddev = std::sqrt(variance);
    
    // Convert to score (lower stddev = higher score)
    // Perfect sync: stddev < 1000Âµs (1ms) = 100 points
    // Good sync: stddev < 5000Âµs (5ms) = 90+ points
    // Medium sync: stddev < 10000Âµs (10ms) = 70+ points
    // Poor sync: stddev > 10000Âµs = < 70 points
    
    double score = 100.0 - (stddev / 100.0);  // 1ms stddev = -1 point
    
    return std::max(0.0, std::min(100.0, score));
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool LatencyCompensator::isOutlier(const DeviceLatencyProfile& profile, 
                                   uint64_t latency) const {
    if (profile.measurementCount < 10) {
        return false;  // Not enough data
    }
    
    // Load atomic threshold
    double threshold = outlierThreshold_.load();
    
    // Use 3-sigma rule
    double deviation = std::abs(static_cast<double>(latency) - 
                               static_cast<double>(profile.averageLatency));
    
    return (deviation > threshold * profile.jitter);
}

void LatencyCompensator::updateDeviceStatistics(DeviceLatencyProfile& profile) {
    // Statistics are updated in addMeasurement()
}

} // namespace midiMind

// ============================================================================
// END OF FILE LatencyCompensator.cpp
// ============================================================================
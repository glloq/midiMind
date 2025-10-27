// ============================================================================
// File: backend/src/timing/LatencyCompensator.h
// Version: 4.2.0 - PHASE 1 COMPLETE
// ============================================================================
//
// Changes v4.2.0:
//   ✅ ADDED: enable() / disable() / isEnabled()
//   ✅ ADDED: getGlobalOffset() / setGlobalOffset()
//   ✅ ADDED: enabled_ and globalOffsetMs_ members
//   ✅ FIXED: Thread-safety issues
//
// ============================================================================

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>
#include <nlohmann/json.hpp>
#include "InstrumentLatencyProfile.h"
#include "../storage/InstrumentDatabase.h"
#include "../core/Logger.h"

using json = nlohmann::json;

namespace midiMind {

struct DeviceLatencyProfile {
    std::string deviceId;
    uint64_t averageLatency;
    uint64_t minLatency;
    uint64_t maxLatency;
    double jitter;
    uint64_t measurementCount;
    int64_t compensationOffset;
    bool autoCompensation;
    std::deque<uint64_t> latencyHistory;
    
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
    
    void addMeasurement(uint64_t latency);
    int64_t calculateOptimalCompensation() const;
    json toJson() const;
};

class LatencyCompensator {
public:
    explicit LatencyCompensator(InstrumentDatabase& instrumentDb);
    ~LatencyCompensator();
    
    LatencyCompensator(const LatencyCompensator&) = delete;
    LatencyCompensator& operator=(const LatencyCompensator&) = delete;
    
    // Device management
    bool registerDevice(const std::string& deviceId);
    void unregisterDevice(const std::string& deviceId);
    bool isDeviceRegistered(const std::string& deviceId) const;
    
    // Instrument management
    bool registerInstrument(const InstrumentLatencyProfile& profile);
    void unregisterInstrument(const std::string& instrumentId);
    bool isInstrumentRegistered(const std::string& instrumentId) const;
    
    // Device latency measurement
    void recordDeviceLatency(const std::string& deviceId, uint64_t latencyUs);
    int64_t getDeviceCompensation(const std::string& deviceId) const;
    void setDeviceCompensation(const std::string& deviceId, int64_t offsetUs);
    
    // Instrument latency measurement
    void recordInstrumentLatency(const std::string& instrumentId, uint64_t latencyUs);
    int64_t getInstrumentCompensation(const std::string& instrumentId) const;
    void setInstrumentCompensation(const std::string& instrumentId, int64_t offsetUs);
    
    // Profiles
    DeviceLatencyProfile getDeviceProfile(const std::string& deviceId) const;
    InstrumentLatencyProfile getInstrumentProfile(const std::string& instrumentId) const;
    std::vector<InstrumentLatencyProfile> getAllInstrumentProfiles() const;
    
    // Persistence
    bool saveInstrumentProfiles();
    bool loadInstrumentProfiles();
    
    // Statistics
    json getDeviceStatistics(const std::string& deviceId) const;
    json getInstrumentStatistics(const std::string& instrumentId) const;
    json getAllStatistics() const;
    double getSyncScore() const;
    
    // Configuration (thread-safe with atomic)
    void setHistorySize(size_t size) { 
        historySize_.store(size); 
    }
    
    void setOutlierDetection(bool enabled) { 
        outlierDetectionEnabled_.store(enabled); 
    }
    
    size_t getHistorySize() const {
        return historySize_.load();
    }
    
    bool isOutlierDetectionEnabled() const {
        return outlierDetectionEnabled_.load();
    }
    
    void setOutlierThreshold(double threshold) {
        outlierThreshold_.store(threshold);
    }
    
    double getOutlierThreshold() const {
        return outlierThreshold_.load();
    }
    
    // ✅ NEW: Global control methods for CommandHandler
    void enable() { 
        std::lock_guard<std::mutex> lock(deviceMutex_);
        enabled_ = true; 
        Logger::info("LatencyCompensator", "Compensation enabled");
    }
    
    void disable() { 
        std::lock_guard<std::mutex> lock(deviceMutex_);
        enabled_ = false; 
        Logger::info("LatencyCompensator", "Compensation disabled");
    }
    
    bool isEnabled() const { 
        std::lock_guard<std::mutex> lock(deviceMutex_);
        return enabled_; 
    }
    
    double getGlobalOffset() const { 
        std::lock_guard<std::mutex> lock(deviceMutex_);
        return globalOffsetMs_; 
    }
    
    void setGlobalOffset(double offsetMs) { 
        std::lock_guard<std::mutex> lock(deviceMutex_);
        globalOffsetMs_ = offsetMs; 
        Logger::info("LatencyCompensator", "Global offset set to " + 
                    std::to_string(offsetMs) + " ms");
    }

private:
    bool isOutlier(const DeviceLatencyProfile& profile, uint64_t latency) const;
    void updateDeviceStatistics(DeviceLatencyProfile& profile);
    
    std::unordered_map<std::string, DeviceLatencyProfile> devices_;
    std::unordered_map<std::string, InstrumentLatencyProfile> instruments_;
    InstrumentDatabase& instrumentDb_;
    
    mutable std::mutex deviceMutex_;
    mutable std::mutex instrumentMutex_;
    
    // Atomic configuration parameters
    std::atomic<size_t> historySize_;
    std::atomic<bool> outlierDetectionEnabled_;
    std::atomic<double> outlierThreshold_;
    
    // ✅ Global compensation control
    bool enabled_;           // Compensation on/off
    double globalOffsetMs_;  // Global offset in milliseconds
};

} // namespace midiMind
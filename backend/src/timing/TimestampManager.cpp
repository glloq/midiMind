// ============================================================================
// File: backend/src/timing/TimestampManager.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of TimestampManager.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified implementation
//   - Better drift compensation algorithm
//   - Enhanced logging
//
// ============================================================================

#include "TimestampManager.h"
#include "../core/Logger.h"
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// SINGLETON
// ============================================================================

TimestampManager& TimestampManager::instance() {
    static TimestampManager instance;
    return instance;
}

// ============================================================================
// CONSTRUCTOR
// ============================================================================

TimestampManager::TimestampManager()
    : referencePoint_(0)
    , started_(false)
    , syncOffset_(0)
    , driftCompensationEnabled_(false)
    , driftFactor_(0.0)
    , lastDriftMeasurement_(0)
{
    Logger::info("TimestampManager", "TimestampManager created");
}

// ============================================================================
// CONTROL
// ============================================================================

void TimestampManager::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (started_.load(std::memory_order_acquire)) {
        Logger::warning("TimestampManager", "Already started");
        return;
    }
    
    // Capture reference point
    referencePoint_.store(getRawTimestamp(), std::memory_order_release);
    started_.store(true, std::memory_order_release);
    
    Logger::info("TimestampManager", 
                "Started at timestamp: " + std::to_string(referencePoint_.load()));
}

void TimestampManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("TimestampManager", "Resetting timestamp manager");
    
    // Reset reference point
    referencePoint_.store(getRawTimestamp(), std::memory_order_release);
    
    // Reset corrections
    syncOffset_.store(0, std::memory_order_release);
    driftFactor_.store(0.0, std::memory_order_release);
    lastDriftMeasurement_.store(0, std::memory_order_release);
    
    Logger::info("TimestampManager", "✓ Reset complete");
}

// ============================================================================
// TIMESTAMPS - MICROSECONDS
// ============================================================================

uint64_t TimestampManager::now() const {
    if (!started_.load(std::memory_order_acquire)) {
        return 0;
    }
    
    uint64_t raw = getRawTimestamp();
    uint64_t reference = referencePoint_.load(std::memory_order_acquire);
    
    // Calculate delta since reference point
    uint64_t delta = (raw >= reference) ? (raw - reference) : 0;
    
    // Apply corrections
    return applyCorrections(delta);
}

uint64_t TimestampManager::systemNow() const {
    return getRawTimestamp();
}

// ============================================================================
// DRIFT COMPENSATION
// ============================================================================

double TimestampManager::calculateDrift() const {
    if (!started_.load(std::memory_order_acquire)) {
        return 0.0;
    }
    
    uint64_t currentTime = now();
    uint64_t lastMeasurement = lastDriftMeasurement_.load(std::memory_order_acquire);
    
    if (lastMeasurement == 0) {
        // First measurement
        lastDriftMeasurement_.store(currentTime, std::memory_order_release);
        return 0.0;
    }
    
    // Calculate drift since last measurement
    uint64_t elapsed = currentTime - lastMeasurement;
    
    if (elapsed == 0) {
        return 0.0;
    }
    
    // Get raw timestamp difference
    uint64_t rawNow = getRawTimestamp();
    uint64_t rawRef = referencePoint_.load(std::memory_order_acquire);
    uint64_t rawElapsed = rawNow - rawRef;
    
    // Calculate drift in ppm (parts per million)
    // drift = (rawElapsed - correctedElapsed) / correctedElapsed * 1000000
    double drift = 0.0;
    
    if (currentTime > 0) {
        drift = static_cast<double>(rawElapsed - currentTime) / 
                static_cast<double>(currentTime) * 1000000.0;
    }
    
    // Update last measurement
    lastDriftMeasurement_.store(currentTime, std::memory_order_release);
    
    return drift;
}

// ============================================================================
// STATISTICS
// ============================================================================

std::string TimestampManager::getStats() const {
    std::ostringstream oss;
    
    oss << "TimestampManager Statistics:\n";
    oss << "  Started: " << (started_.load() ? "YES" : "NO") << "\n";
    oss << "  Uptime: " << std::fixed << std::setprecision(3) 
        << getUptimeSeconds() << "s\n";
    oss << "  Current timestamp: " << now() << "µs\n";
    oss << "  System timestamp: " << systemNow() << "µs\n";
    oss << "  Sync offset: " << syncOffset_.load() << "µs\n";
    oss << "  Drift compensation: " 
        << (driftCompensationEnabled_.load() ? "ENABLED" : "DISABLED") << "\n";
    oss << "  Drift factor: " << std::fixed << std::setprecision(2)
        << driftFactor_.load() << " ppm\n";
    
    return oss.str();
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

uint64_t TimestampManager::getRawTimestamp() const {
    // Use high_resolution_clock for maximum precision
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

uint64_t TimestampManager::applyCorrections(uint64_t raw) const {
    // Apply synchronization offset
    int64_t offset = syncOffset_.load(std::memory_order_acquire);
    int64_t corrected = static_cast<int64_t>(raw) + offset;
    
    // Ensure result is positive
    if (corrected < 0) {
        corrected = 0;
    }
    
    // Apply drift compensation if enabled
    if (driftCompensationEnabled_.load(std::memory_order_acquire)) {
        double drift = driftFactor_.load(std::memory_order_acquire);
        
        // drift is in ppm (parts per million)
        // Apply correction: corrected * (1 + drift/1000000)
        corrected = static_cast<int64_t>(
            corrected * (1.0 + drift / 1000000.0)
        );
    }
    
    return static_cast<uint64_t>(corrected);
}

} // namespace midiMind

// ============================================================================
// END OF FILE TimestampManager.cpp
// ============================================================================
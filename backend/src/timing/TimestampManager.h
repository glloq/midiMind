// ============================================================================
// File: backend/src/timing/TimestampManager.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   High-precision timestamp manager for time synchronization.
//   Provides consistent timestamps across all system components.
//
// Features:
//   - Microsecond precision (< 1µs typical)
//   - Thread-safe operations
//   - Drift compensation
//   - Synchronization support
//   - Singleton pattern
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified API
//   - Enhanced precision
//   - Better drift compensation
//   - Removed unused features
//
// ============================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <string>

namespace midiMind {

/**
 * @class TimestampManager
 * @brief High-precision timestamp manager with drift compensation
 * 
 * Provides consistent, high-precision timestamps to all system components.
 * Uses std::chrono::high_resolution_clock for maximum precision.
 * 
 * Architecture:
 * ```
 * high_resolution_clock
 *        ↓
 * TimestampManager (reference)
 *        ↓
 *   ┌────┼────┐
 *   ↓    ↓    ↓
 * MIDI Router Player  (all synchronized)
 * ```
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses atomic operations for performance
 * 
 * Precision:
 * - Typical: < 1µs on Raspberry Pi 4
 * - Used for high-precision latency measurements
 * 
 * Example:
 * ```cpp
 * auto& tm = TimestampManager::instance();
 * tm.start();
 * 
 * // Get current timestamp (microseconds)
 * uint64_t now = tm.now();
 * 
 * // Get current timestamp (milliseconds)
 * uint64_t nowMs = tm.nowMs();
 * 
 * // Calculate elapsed time
 * uint64_t delta = tm.elapsed(t1, t2);
 * ```
 */
class TimestampManager {
public:
    // ========================================================================
    // SINGLETON PATTERN
    // ========================================================================
    
    /**
     * @brief Get singleton instance
     * @return Reference to TimestampManager instance
     */
    static TimestampManager& instance();
    
    // Disable copy and assignment
    TimestampManager(const TimestampManager&) = delete;
    TimestampManager& operator=(const TimestampManager&) = delete;
    
    // ========================================================================
    // CONTROL
    // ========================================================================
    
    /**
     * @brief Start the reference clock
     * @note Sets reference point (t=0)
     * @note Should be called once at application startup
     */
    void start();
    
    /**
     * @brief Reset the clock
     * @note Resets counter to zero
     */
    void reset();
    
    /**
     * @brief Check if clock is started
     * @return true if started
     */
    bool isStarted() const {
        return started_.load(std::memory_order_acquire);
    }
    
    // ========================================================================
    // TIMESTAMPS - MICROSECONDS (µs)
    // ========================================================================
    
    /**
     * @brief Get current timestamp in microseconds
     * @return uint64_t Microseconds since start()
     * @note Typical precision: < 1µs on Raspberry Pi 4
     */
    uint64_t now() const;
    
    /**
     * @brief Get system timestamp in microseconds (Unix epoch)
     * @return uint64_t Microseconds since 1970-01-01 00:00:00 UTC
     */
    uint64_t systemNow() const;
    
    // ========================================================================
    // TIMESTAMPS - MILLISECONDS (ms)
    // ========================================================================
    
    /**
     * @brief Get current timestamp in milliseconds
     * @return uint64_t Milliseconds since start()
     */
    uint64_t nowMs() const {
        return now() / 1000;
    }
    
    /**
     * @brief Get system timestamp in milliseconds (Unix epoch)
     * @return uint64_t Milliseconds since 1970-01-01 00:00:00 UTC
     */
    uint64_t systemNowMs() const {
        return systemNow() / 1000;
    }
    
    // ========================================================================
    // TIME CALCULATIONS
    // ========================================================================
    
    /**
     * @brief Calculate elapsed time between two timestamps (µs)
     * @param start Start timestamp (µs)
     * @param end End timestamp (µs)
     * @return uint64_t Difference in microseconds
     */
    uint64_t elapsed(uint64_t start, uint64_t end) const {
        return (end >= start) ? (end - start) : 0;
    }
    
    /**
     * @brief Calculate elapsed time between two timestamps (ms)
     * @param start Start timestamp (ms)
     * @param end End timestamp (ms)
     * @return uint64_t Difference in milliseconds
     */
    uint64_t elapsedMs(uint64_t start, uint64_t end) const {
        return elapsed(start, end) / 1000;
    }
    
    // ========================================================================
    // SYNCHRONIZATION
    // ========================================================================
    
    /**
     * @brief Set synchronization offset
     * @param offset Offset in microseconds
     * @note Used for multi-device synchronization
     */
    void setSyncOffset(int64_t offset) {
        syncOffset_.store(offset, std::memory_order_release);
    }
    
    /**
     * @brief Get synchronization offset
     * @return int64_t Current offset in microseconds
     */
    int64_t getSyncOffset() const {
        return syncOffset_.load(std::memory_order_acquire);
    }
    
    // ========================================================================
    // DRIFT COMPENSATION
    // ========================================================================
    
    /**
     * @brief Enable drift compensation
     * @param enabled true to enable
     */
    void setDriftCompensation(bool enabled) {
        driftCompensationEnabled_.store(enabled, std::memory_order_release);
    }
    
    /**
     * @brief Check if drift compensation is enabled
     * @return true if enabled
     */
    bool isDriftCompensationEnabled() const {
        return driftCompensationEnabled_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Set drift factor
     * @param driftPpm Drift in parts per million (ppm)
     * @note Typical values on Raspberry Pi: < 50 ppm
     */
    void setDriftFactor(double driftPpm) {
        driftFactor_.store(driftPpm, std::memory_order_release);
    }
    
    /**
     * @brief Get drift factor
     * @return double Drift in parts per million (ppm)
     */
    double getDriftFactor() const {
        return driftFactor_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Calculate current drift
     * @return double Drift in ppm
     */
    double calculateDrift() const;
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get clock statistics
     * @return std::string Formatted statistics
     */
    std::string getStats() const;
    
    /**
     * @brief Get uptime since start() in seconds
     * @return double Elapsed seconds
     */
    double getUptimeSeconds() const {
        return now() / 1000000.0;
    }
    
private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR (SINGLETON)
    // ========================================================================
    
    TimestampManager();
    ~TimestampManager() = default;
    
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Get raw system timestamp (µs)
     */
    uint64_t getRawTimestamp() const;
    
    /**
     * @brief Apply offset and drift compensation
     */
    uint64_t applyCorrections(uint64_t raw) const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Reference point (µs since epoch)
    std::atomic<uint64_t> referencePoint_;
    
    /// Started flag
    std::atomic<bool> started_;
    
    /// Synchronization offset (µs)
    std::atomic<int64_t> syncOffset_;
    
    /// Drift compensation enabled
    std::atomic<bool> driftCompensationEnabled_;
    
    /// Drift factor (ppm)
    std::atomic<double> driftFactor_;
    
    /// Last drift measurement (µs)
    mutable std::atomic<uint64_t> lastDriftMeasurement_;
    
    /// Mutex for critical operations
    mutable std::mutex mutex_;
};

// ============================================================================
// INLINE UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Get fast timestamp (microseconds)
 * @return uint64_t Current timestamp in µs
 * @note Optimized inline version for performance-critical code
 */
inline uint64_t getTimestampUs() {
    return TimestampManager::instance().now();
}

/**
 * @brief Get fast timestamp (milliseconds)
 * @return uint64_t Current timestamp in ms
 * @note Optimized inline version for performance-critical code
 */
inline uint64_t getTimestampMs() {
    return TimestampManager::instance().nowMs();
}

} // namespace midiMind
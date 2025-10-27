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
//   - Fixed atomic<double> → atomic<int64_t> for lock-free guarantee
//   - Documented precision loss in ms conversions
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
 * - Singleton initialization is thread-safe (C++11 guarantee)
 * 
 * Precision:
 * - Typical: < 1µs on Raspberry Pi 4
 * - Used for high-precision latency measurements
 * - Note: ms conversions truncate precision (1000µs → 1ms)
 * 
 * Example:
 * ```cpp
 * auto& tm = TimestampManager::instance();
 * tm.start();
 * 
 * // Get current timestamp (microseconds)
 * uint64_t now = tm.now();
 * 
 * // Get current timestamp (milliseconds - precision loss)
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
     * @note Thread-safe initialization guaranteed by C++11
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
     * @note Thread-safe: multiple calls are safe, only first has effect
     */
    void start();
    
    /**
     * @brief Reset the clock
     * @note Resets counter to zero
     * @note Thread-safe
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
     * @note Returns 0 if not started
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
     * @warning Precision loss: µs → ms (truncates sub-millisecond values)
     * @note For high precision, use now() instead
     */
    uint64_t nowMs() const {
        return now() / 1000;
    }
    
    /**
     * @brief Get system timestamp in milliseconds (Unix epoch)
     * @return uint64_t Milliseconds since 1970-01-01 00:00:00 UTC
     * @warning Precision loss: µs → ms (truncates sub-millisecond values)
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
     * @return uint64_t Difference in microseconds (0 if end < start)
     */
    uint64_t elapsed(uint64_t start, uint64_t end) const {
        return (end >= start) ? (end - start) : 0;
    }
    
    /**
     * @brief Calculate elapsed time between two timestamps (ms)
     * @param start Start timestamp (ms)
     * @param end End timestamp (ms)
     * @return uint64_t Difference in milliseconds
     * @warning Operates on milliseconds, not microseconds
     */
    uint64_t elapsedMs(uint64_t start, uint64_t end) const {
        return elapsed(start, end);
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
     * @note Stored as nano-ppm internally for lock-free atomic operations
     */
    void setDriftFactor(double driftPpm);
    
    /**
     * @brief Get drift factor
     * @return double Drift in parts per million (ppm)
     */
    double getDriftFactor() const;
    
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
     * @param raw Raw timestamp delta
     * @return Corrected timestamp
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
    
    /// Drift factor (stored as nano-ppm: ppm * 1000 for lock-free atomic)
    /// Range: -2^63 to 2^63-1 nano-ppm = ±9.2e18 ppm
    std::atomic<int64_t> driftFactorNanoPpm_;
    
    /// Last drift measurement (µs)
    mutable std::atomic<uint64_t> lastDriftMeasurement_;
    
    /// Mutex for start/reset operations
    std::mutex controlMutex_;
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
 * @warning Precision loss: µs → ms truncation
 * @note Optimized inline version for performance-critical code
 */
inline uint64_t getTimestampMs() {
    return TimestampManager::instance().nowMs();
}

} // namespace midiMind
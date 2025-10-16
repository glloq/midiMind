// ============================================================================
// File: backend/src/core/TimeUtils.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Time utility functions providing high-precision timestamps, time
//   conversions, duration calculations, and time formatting. Essential for
//   MIDI timing, latency measurement, and performance monitoring.
//
// Features:
//   - Microsecond precision timestamps
//   - Multiple time units (us, ms, sec)
//   - Time conversions
//   - Duration calculations
//   - Time formatting
//   - Timer class for performance measurement
//   - Sleep functions
//
// Dependencies:
//   - <chrono> for high-precision timing
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Enhanced microsecond precision support
//   - Added more conversion utilities
//   - Improved Timer class
//   - Added formatted time strings
//   - All inline for maximum performance
//
// ============================================================================

#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace midiMind {
namespace TimeUtils {

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

/**
 * @typedef Timestamp
 * @brief High-precision timestamp type (microseconds)
 */
using Timestamp = uint64_t;

// ============================================================================
// TIMESTAMP FUNCTIONS
// ============================================================================

/**
 * @brief Get current timestamp in microseconds
 * 
 * @return uint64_t Timestamp in microseconds since epoch
 * 
 * @note Uses high_resolution_clock for maximum precision
 * @note Typical precision: < 1µs on modern hardware
 * 
 * @example
 * @code
 * uint64_t start = TimeUtils::nowUs();
 * // ... operation ...
 * uint64_t end = TimeUtils::nowUs();
 * uint64_t elapsed = end - start;
 * @endcode
 */
inline uint64_t nowUs() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

/**
 * @brief Get current timestamp in milliseconds
 * 
 * @return uint64_t Timestamp in milliseconds since epoch
 * 
 * @note Uses steady_clock for monotonic time
 */
inline uint64_t nowMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

/**
 * @brief Get current timestamp in seconds
 * 
 * @return uint64_t Timestamp in seconds since epoch
 */
inline uint64_t nowSec() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

/**
 * @brief Get system timestamp in microseconds (Unix epoch)
 * 
 * @return uint64_t Microseconds since 1970-01-01 00:00:00 UTC
 * 
 * @note Uses system_clock which can be adjusted by NTP
 */
inline uint64_t systemNowUs() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

/**
 * @brief Get system timestamp in milliseconds (Unix epoch)
 * 
 * @return uint64_t Milliseconds since 1970-01-01 00:00:00 UTC
 */
inline uint64_t systemNowMs() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// ============================================================================
// TIME CONVERSIONS
// ============================================================================

/**
 * @brief Convert microseconds to milliseconds
 * @param us Microseconds
 * @return Milliseconds
 */
inline uint64_t usToMs(uint64_t us) {
    return us / 1000;
}

/**
 * @brief Convert milliseconds to microseconds
 * @param ms Milliseconds
 * @return Microseconds
 */
inline uint64_t msToUs(uint64_t ms) {
    return ms * 1000;
}

/**
 * @brief Convert microseconds to seconds
 * @param us Microseconds
 * @return Seconds
 */
inline uint64_t usToSec(uint64_t us) {
    return us / 1000000;
}

/**
 * @brief Convert seconds to microseconds
 * @param sec Seconds
 * @return Microseconds
 */
inline uint64_t secToUs(uint64_t sec) {
    return sec * 1000000;
}

/**
 * @brief Convert milliseconds to seconds
 * @param ms Milliseconds
 * @return Seconds
 */
inline uint64_t msToSec(uint64_t ms) {
    return ms / 1000;
}

/**
 * @brief Convert seconds to milliseconds
 * @param sec Seconds
 * @return Milliseconds
 */
inline uint64_t secToMs(uint64_t sec) {
    return sec * 1000;
}

// ============================================================================
// DURATION CALCULATIONS
// ============================================================================

/**
 * @brief Calculate elapsed time between two timestamps
 * 
 * @param start Start timestamp
 * @param end End timestamp
 * @return Elapsed time (same unit as input)
 * 
 * @note Returns 0 if end < start (prevents underflow)
 */
inline uint64_t elapsed(uint64_t start, uint64_t end) {
    return (end >= start) ? (end - start) : 0;
}

/**
 * @brief Calculate elapsed time since a timestamp
 * 
 * @param start Start timestamp in microseconds
 * @return Elapsed microseconds
 */
inline uint64_t elapsedSince(uint64_t start) {
    return elapsed(start, nowUs());
}

/**
 * @brief Calculate elapsed time since a timestamp (milliseconds)
 * 
 * @param start Start timestamp in milliseconds
 * @return Elapsed milliseconds
 */
inline uint64_t elapsedSinceMs(uint64_t start) {
    return elapsed(start, nowMs());
}

// ============================================================================
// MUSIC TIMING
// ============================================================================

/**
 * @brief Convert BPM to beat duration in milliseconds
 * 
 * @param bpm Tempo in beats per minute
 * @return Beat duration in milliseconds
 * 
 * @example
 * @code
 * uint64_t beatMs = TimeUtils::bpmToBeatDuration(120.0f);
 * // Returns 500ms (120 BPM = 2 beats/sec = 500ms/beat)
 * @endcode
 */
inline uint64_t bpmToBeatDuration(float bpm) {
    if (bpm <= 0.0f) return 0;
    return static_cast<uint64_t>(60000.0f / bpm);
}

/**
 * @brief Convert beat duration to BPM
 * 
 * @param beatDurationMs Beat duration in milliseconds
 * @return Tempo in BPM
 */
inline float beatDurationToBpm(uint64_t beatDurationMs) {
    if (beatDurationMs == 0) return 0.0f;
    return 60000.0f / static_cast<float>(beatDurationMs);
}

// ============================================================================
// TIME FORMATTING
// ============================================================================

/**
 * @brief Convert timestamp to readable string
 * 
 * @param timestampMs Timestamp in milliseconds
 * @param format Time format string (strftime compatible)
 * @return Formatted time string
 * 
 * @example
 * @code
 * auto ts = TimeUtils::nowMs();
 * std::string str = TimeUtils::timestampToString(ts);
 * // "2025-10-16 14:30:45"
 * @endcode
 */
inline std::string timestampToString(uint64_t timestampMs, 
                                     const std::string& format = "%Y-%m-%d %H:%M:%S") {
    time_t seconds = timestampMs / 1000;
    std::tm tm;
    localtime_r(&seconds, &tm);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, format.c_str());
    
    return oss.str();
}

/**
 * @brief Convert duration to human-readable string
 * 
 * @param durationMs Duration in milliseconds
 * @return Formatted duration (e.g., "1h 23m 45s")
 * 
 * @example
 * @code
 * std::string duration = TimeUtils::durationToString(5025000);
 * // Returns "1h 23m 45s"
 * @endcode
 */
inline std::string durationToString(uint64_t durationMs) {
    uint64_t seconds = durationMs / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    
    std::ostringstream oss;
    
    if (hours > 0) {
        oss << hours << "h ";
    }
    if (minutes > 0 || hours > 0) {
        oss << minutes << "m ";
    }
    oss << seconds << "s";
    
    return oss.str();
}

/**
 * @brief Format microseconds as milliseconds with decimal
 * 
 * @param us Microseconds
 * @param precision Decimal precision (default: 3)
 * @return Formatted string (e.g., "1.234ms")
 */
inline std::string formatLatency(uint64_t us, int precision = 3) {
    double ms = static_cast<double>(us) / 1000.0;
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << ms << "ms";
    
    return oss.str();
}

// ============================================================================
// SLEEP FUNCTIONS
// ============================================================================

/**
 * @brief Sleep for specified microseconds
 * 
 * @param us Duration in microseconds
 * 
 * @note Actual sleep time may be longer due to scheduler granularity
 */
inline void sleepUs(uint64_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

/**
 * @brief Sleep for specified milliseconds
 * 
 * @param ms Duration in milliseconds
 */
inline void sleepMs(uint64_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/**
 * @brief Sleep for specified seconds
 * 
 * @param sec Duration in seconds
 */
inline void sleepSec(uint64_t sec) {
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

// ============================================================================
// TIMER CLASS
// ============================================================================

/**
 * @class Timer
 * @brief Simple timer for performance measurement
 * 
 * @details
 * Measures elapsed time with microsecond precision. Useful for profiling
 * and performance monitoring.
 * 
 * @example
 * @code
 * {
 *     TimeUtils::Timer timer;
 *     // ... operation to measure ...
 *     uint64_t elapsed = timer.elapsedMs();
 *     std::cout << "Operation took " << elapsed << "ms" << std::endl;
 * }
 * @endcode
 */
class Timer {
public:
    /**
     * @brief Constructor - starts timer
     */
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    /**
     * @brief Reset timer to current time
     */
    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    /**
     * @brief Get elapsed time in microseconds
     * @return Microseconds elapsed since construction or reset
     */
    uint64_t elapsedUs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now - start_).count();
    }
    
    /**
     * @brief Get elapsed time in milliseconds
     * @return Milliseconds elapsed since construction or reset
     */
    uint64_t elapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_).count();
    }
    
    /**
     * @brief Get elapsed time in seconds
     * @return Seconds elapsed since construction or reset
     */
    uint64_t elapsedSec() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now - start_).count();
    }
    
    /**
     * @brief Get elapsed time as formatted string
     * @return Formatted duration string (e.g., "1h 23m 45s")
     */
    std::string elapsedString() const {
        return durationToString(elapsedMs());
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace TimeUtils
} // namespace midiMind

// ============================================================================
// END OF FILE TimeUtils.h v4.1.0
// ============================================================================
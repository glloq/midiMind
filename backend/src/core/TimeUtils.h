// ============================================================================
// File: backend/src/core/TimeUtils.h
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Time utility functions providing high-precision timestamps, time
//   conversions, duration calculations, and time formatting. Essential for
//   MIDI timing, latency measurement, and performance monitoring.
//
// Features:
//   - Microsecond precision timestamps (monotonic)
//   - Multiple time units (us, ms, sec)
//   - Time conversions
//   - Duration calculations
//   - Time formatting (portable Windows/POSIX)
//   - Timer class for performance measurement
//   - Sleep functions
//
// Dependencies:
//   - <chrono> for high-precision timing
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.1:
//   - Added debug assertions for formatLatency() overflow detection
//   - Improved documentation about precision limits
//
// Changes v4.1.0:
//   - Enhanced microsecond precision support
//   - All timestamps now use steady_clock (monotonic guarantee)
//   - Portable localtime implementation (Windows/POSIX)
//   - Documented precision limits
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
#include <cmath>
#include <cassert>
#include <limits>

namespace midiMind {
namespace TimeUtils {

// ============================================================================
// PLATFORM-SPECIFIC UTILITIES
// ============================================================================

/**
 * @brief Portable localtime implementation
 * @param time_t Time value
 * @param tm Output time structure
 * @return true on success
 * @note Automatically uses localtime_s (Windows) or localtime_r (POSIX)
 */
inline bool portable_localtime(const time_t* timer, std::tm* buf) {
#if defined(_WIN32) || defined(_WIN64)
    return localtime_s(buf, timer) == 0;
#else
    return localtime_r(timer, buf) != nullptr;
#endif
}

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

/**
 * @typedef Timestamp
 * @brief High-precision timestamp type (microseconds)
 */
using Timestamp = uint64_t;

// ============================================================================
// TIMESTAMP FUNCTIONS (MONOTONIC)
// ============================================================================

/**
 * @brief Get current timestamp in microseconds (monotonic)
 * 
 * @return uint64_t Timestamp in microseconds since unspecified epoch
 * 
 * @note Uses steady_clock for monotonic, non-decreasing time
 * @note Typical precision: < 1µs on modern hardware
 * @note NOT comparable to system time - use for elapsed time only
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
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

/**
 * @brief Get current timestamp in milliseconds (monotonic)
 * 
 * @return uint64_t Timestamp in milliseconds since unspecified epoch
 * 
 * @note Uses steady_clock for monotonic, non-decreasing time
 * @note NOT comparable to system time - use for elapsed time only
 */
inline uint64_t nowMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

/**
 * @brief Get current timestamp in seconds (monotonic)
 * 
 * @return uint64_t Timestamp in seconds since unspecified epoch
 * 
 * @note Uses steady_clock for monotonic, non-decreasing time
 */
inline uint64_t nowSec() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

// ============================================================================
// SYSTEM TIMESTAMPS (WALL CLOCK - MAY JUMP)
// ============================================================================

/**
 * @brief Get system timestamp in microseconds (Unix epoch)
 * 
 * @return uint64_t Microseconds since 1970-01-01 00:00:00 UTC
 * 
 * @note Uses system_clock which can be adjusted by NTP
 * @warning May jump backward if system time is adjusted
 * @note Use nowUs() for elapsed time measurements instead
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
 * 
 * @note Uses system_clock which can be adjusted by NTP
 * @warning May jump backward if system time is adjusted
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
 * @note Truncates sub-millisecond precision
 */
inline uint64_t usToMs(uint64_t us) {
    return us / 1000;
}

/**
 * @brief Convert milliseconds to microseconds
 * @param ms Milliseconds
 * @return Microseconds
 * @warning May overflow for very large ms values (> 2^54)
 */
inline uint64_t msToUs(uint64_t ms) {
    return ms * 1000;
}

/**
 * @brief Convert microseconds to seconds
 * @param us Microseconds
 * @return Seconds
 * @note Truncates sub-second precision
 */
inline uint64_t usToSec(uint64_t us) {
    return us / 1000000;
}

/**
 * @brief Convert seconds to microseconds
 * @param sec Seconds
 * @return Microseconds
 * @warning May overflow for very large sec values (> 2^44)
 */
inline uint64_t secToUs(uint64_t sec) {
    return sec * 1000000;
}

/**
 * @brief Convert milliseconds to seconds
 * @param ms Milliseconds
 * @return Seconds
 * @note Truncates sub-second precision
 */
inline uint64_t msToSec(uint64_t ms) {
    return ms / 1000;
}

/**
 * @brief Convert seconds to milliseconds
 * @param sec Seconds
 * @return Milliseconds
 * @warning May overflow for very large sec values (> 2^54)
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
 * @note Returns 0 if end < start to prevent underflow
 * @warning If end < start occurs with monotonic clock, indicates bug
 */
inline uint64_t elapsed(uint64_t start, uint64_t end) {
    return (end >= start) ? (end - start) : 0;
}

/**
 * @brief Calculate elapsed time since a timestamp (microseconds)
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
 * @note Returns 0 if bpm <= 0
 * @warning For very low BPM (< 0.01), result may overflow uint64_t
 * @note Valid range: 0.01 - 10,000 BPM
 * 
 * @example
 * @code
 * uint64_t beatMs = TimeUtils::bpmToBeatDuration(120.0f);
 * // Returns 500ms (120 BPM = 2 beats/sec = 500ms/beat)
 * @endcode
 */
inline uint64_t bpmToBeatDuration(float bpm) {
    if (bpm <= 0.0f) return 0;
    
    // Clamp to reasonable range to prevent overflow
    if (bpm < 0.01f) bpm = 0.01f;
    if (bpm > 10000.0f) bpm = 10000.0f;
    
    return static_cast<uint64_t>(60000.0f / bpm);
}

/**
 * @brief Convert beat duration to BPM
 * 
 * @param beatDurationMs Beat duration in milliseconds
 * @return Tempo in BPM
 * 
 * @note Returns 0.0f if beatDurationMs == 0
 */
inline float beatDurationToBpm(uint64_t beatDurationMs) {
    if (beatDurationMs == 0) return 0.0f;
    return 60000.0f / static_cast<float>(beatDurationMs);
}

// ============================================================================
// TIME FORMATTING
// ============================================================================

/**
 * @brief Convert timestamp to readable string (portable)
 * 
 * @param timestampMs Timestamp in milliseconds
 * @param format Time format string (strftime compatible)
 * @return Formatted time string (empty on error)
 * 
 * @note Portable across Windows and POSIX systems
 * @note Returns empty string if time conversion fails
 * 
 * @example
 * @code
 * auto ts = TimeUtils::systemNowMs();
 * std::string str = TimeUtils::timestampToString(ts);
 * // "2025-10-16 14:30:45"
 * @endcode
 */
inline std::string timestampToString(uint64_t timestampMs, 
                                     const std::string& format = "%Y-%m-%d %H:%M:%S") {
    time_t seconds = static_cast<time_t>(timestampMs / 1000);
    std::tm tm;
    
    if (!portable_localtime(&seconds, &tm)) {
        return "";  // Error converting time
    }
    
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
 * 
 * @note Uses double internally, precision limits:
 *       - Exact representation up to 2^53 µs (~285 years)
 *       - Beyond that, sub-microsecond precision may be lost
 *       - Debug builds assert on values > 2^53 (safe range check)
 * @note For display only - do not parse back to exact values
 * @warning In debug builds, asserts if value exceeds safe precision range
 * 
 * @example
 * @code
 * auto latency = TimeUtils::formatLatency(1234);
 * // "1.234ms"
 * @endcode
 */
inline std::string formatLatency(uint64_t us, int precision = 3) {
    // Maximum safe value for exact double representation
    // 2^53 = 9007199254740992 µs ≈ 285 years
    constexpr uint64_t MAX_SAFE_VALUE = (1ULL << 53);
    
    // Debug assertion to detect precision loss
    // In release builds, this is compiled out
    assert(us <= MAX_SAFE_VALUE && 
           "formatLatency: value exceeds safe double precision (> 2^53 µs)");
    
    // If value exceeds safe range, log warning in debug builds
    #ifndef NDEBUG
    if (us > MAX_SAFE_VALUE) {
        // In debug, this will have already asserted above
        // This is defensive programming for release builds
        static bool warningLogged = false;
        if (!warningLogged) {
            // Note: In production this won't log, but precision may be lost
            warningLogged = true;
        }
    }
    #endif
    
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
 * @note Typical granularity: 1-15ms on Linux, 1ms on Windows
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
 * @brief Simple timer for performance measurement (monotonic)
 * 
 * @details
 * Measures elapsed time with microsecond precision using steady_clock
 * (monotonic guarantee). Useful for profiling and performance monitoring.
 * 
 * Thread Safety: Each Timer instance should be used by single thread
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
    Timer() : start_(std::chrono::steady_clock::now()) {}
    
    /**
     * @brief Reset timer to current time
     */
    void reset() {
        start_ = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Get elapsed time in microseconds
     * @return Microseconds elapsed since construction or reset
     */
    uint64_t elapsedUs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now - start_).count();
    }
    
    /**
     * @brief Get elapsed time in milliseconds
     * @return Milliseconds elapsed since construction or reset
     */
    uint64_t elapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_).count();
    }
    
    /**
     * @brief Get elapsed time in seconds
     * @return Seconds elapsed since construction or reset
     */
    uint64_t elapsedSec() const {
        auto now = std::chrono::steady_clock::now();
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
    std::chrono::steady_clock::time_point start_;
};

} // namespace TimeUtils
} // namespace midiMind

// ============================================================================
// END OF FILE TimeUtils.h v4.1.1
// ============================================================================
// ============================================================================
// File: backend/src/core/TimeUtils.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   High-precision time utilities for MIDI timing and synchronization.
//   Provides microsecond-accurate timing operations with overflow protection.
//
// ============================================================================

#pragma once

#include <chrono>
#include <string>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <thread>

namespace midiMind {

/**
 * @namespace TimeUtils
 * @brief High-precision timing utilities
 * 
 * Provides:
 * - Microsecond timestamps using steady_clock
 * - Protected time arithmetic with overflow detection
 * - Human-readable time formatting
 * - ISO 8601 timestamp generation
 * 
 * @warning All time values are in microseconds unless noted otherwise
 * @note steady_clock is monotonic and not affected by system clock changes
 */
namespace TimeUtils {

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

/**
 * @brief Type for microsecond timestamps
 * Range: 0 to ~584,942 years (2^64 - 1 microseconds)
 */
using Timestamp = uint64_t;

/**
 * @brief Type for time differences (can be negative)
 */
using TimeDelta = int64_t;

// ============================================================================
// CURRENT TIME
// ============================================================================

/**
 * @brief Get current monotonic timestamp in microseconds
 * @return Timestamp in microseconds since epoch
 * @note Uses std::chrono::steady_clock (monotonic, not wall clock)
 * 
 * Example:
 * @code
 * auto start = TimeUtils::now();
 * // ... do work ...
 * auto end = TimeUtils::now();
 * auto elapsed = end - start;  // microseconds
 * @endcode
 */
inline Timestamp now() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

/**
 * @brief Get current system time (wall clock) in microseconds
 * @return Timestamp in microseconds since Unix epoch
 * @note Uses std::chrono::system_clock (wall clock, affected by time changes)
 */
inline Timestamp systemNow() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

// ============================================================================
// TIME ARITHMETIC (OVERFLOW-SAFE)
// ============================================================================

/**
 * @brief Add time values with overflow protection
 * @param a First timestamp (microseconds)
 * @param b Second timestamp (microseconds)
 * @return Sum, or UINT64_MAX if overflow would occur
 * 
 * Example:
 * @code
 * auto future = TimeUtils::add(now, 1000000);  // +1 second
 * @endcode
 */
inline Timestamp add(Timestamp a, Timestamp b) {
    // Check for overflow using the rule: a + b > MAX if b > MAX - a
    if (b > UINT64_MAX - a) {
        return UINT64_MAX;  // Saturating addition
    }
    return a + b;
}

/**
 * @brief Subtract time values with underflow protection
 * @param a Minuend (microseconds)
 * @param b Subtrahend (microseconds)
 * @return Difference, or 0 if underflow would occur
 * 
 * Example:
 * @code
 * auto elapsed = TimeUtils::subtract(end, start);
 * @endcode
 */
inline Timestamp subtract(Timestamp a, Timestamp b) {
    if (b > a) {
        return 0;  // Saturating subtraction
    }
    return a - b;
}

/**
 * @brief Calculate signed time difference
 * @param later Later timestamp
 * @param earlier Earlier timestamp
 * @return Signed difference (can be negative)
 * 
 * Example:
 * @code
 * auto delta = TimeUtils::delta(end, start);
 * if (delta < 0) {
 *     // end was before start!
 * }
 * @endcode
 */
inline TimeDelta delta(Timestamp later, Timestamp earlier) {
    if (later >= earlier) {
        return static_cast<TimeDelta>(later - earlier);
    } else {
        return -static_cast<TimeDelta>(earlier - later);
    }
}

// ============================================================================
// TIME COMPARISONS
// ============================================================================

/**
 * @brief Check if timestamp is in the past
 * @param timestamp Timestamp to check
 * @return true if timestamp < now()
 */
inline bool isPast(Timestamp timestamp) {
    return timestamp < now();
}

/**
 * @brief Check if timestamp is in the future
 * @param timestamp Timestamp to check
 * @return true if timestamp > now()
 */
inline bool isFuture(Timestamp timestamp) {
    return timestamp > now();
}

/**
 * @brief Calculate elapsed time since timestamp
 * @param timestamp Starting timestamp
 * @return Microseconds elapsed (or 0 if timestamp is in future)
 */
inline Timestamp elapsed(Timestamp timestamp) {
    return subtract(now(), timestamp);
}

/**
 * @brief Calculate remaining time until timestamp
 * @param timestamp Target timestamp
 * @return Microseconds remaining (or 0 if timestamp is in past)
 */
inline Timestamp remaining(Timestamp timestamp) {
    return subtract(timestamp, now());
}

// ============================================================================
// TIME UNIT CONVERSIONS
// ============================================================================

/**
 * @brief Convert milliseconds to microseconds
 * @param ms Milliseconds
 * @return Microseconds
 */
inline constexpr Timestamp msToUs(uint32_t ms) {
    return static_cast<Timestamp>(ms) * 1000ULL;
}

/**
 * @brief Convert seconds to microseconds
 * @param seconds Seconds
 * @return Microseconds
 */
inline constexpr Timestamp secondsToUs(uint32_t seconds) {
    return static_cast<Timestamp>(seconds) * 1000000ULL;
}

/**
 * @brief Convert microseconds to milliseconds (rounded down)
 * @param us Microseconds
 * @return Milliseconds
 */
inline constexpr uint32_t usToMs(Timestamp us) {
    return static_cast<uint32_t>(us / 1000ULL);
}

/**
 * @brief Convert microseconds to seconds (rounded down)
 * @param us Microseconds
 * @return Seconds
 */
inline constexpr uint32_t usToSeconds(Timestamp us) {
    return static_cast<uint32_t>(us / 1000000ULL);
}

/**
 * @brief Convert microseconds to fractional seconds
 * @param us Microseconds
 * @return Seconds as double
 */
inline constexpr double usToSecondsDouble(Timestamp us) {
    return static_cast<double>(us) / 1000000.0;
}

/**
 * @brief Convert microseconds to fractional milliseconds
 * @param us Microseconds
 * @return Milliseconds as double
 */
inline constexpr double usToMsDouble(Timestamp us) {
    return static_cast<double>(us) / 1000.0;
}

// ============================================================================
// FORMATTING
// ============================================================================

/**
 * @brief Format timestamp as human-readable string
 * @param us Microseconds
 * @return String like "1h 23m 45.678s" or "123.456ms" or "789us"
 * 
 * Examples:
 * - 789 µs → "789µs"
 * - 123456 µs → "123.456ms"
 * - 5000000 µs → "5.000s"
 * - 3661000000 µs → "1h 1m 1.000s"
 */
inline std::string formatDuration(Timestamp us) {
    std::ostringstream oss;
    oss << std::fixed;
    
    if (us < 1000) {
        // Less than 1ms: show as µs
        oss << us << "µs";
    } else if (us < 1000000) {
        // Less than 1s: show as ms with 3 decimals
        oss << std::setprecision(3) << (static_cast<double>(us) / 1000.0) << "ms";
    } else if (us < 60000000) {
        // Less than 1min: show as seconds with 3 decimals
        oss << std::setprecision(3) << (static_cast<double>(us) / 1000000.0) << "s";
    } else if (us < 3600000000ULL) {
        // Less than 1h: show as minutes and seconds
        uint32_t minutes = static_cast<uint32_t>(us / 60000000ULL);
        uint32_t seconds = static_cast<uint32_t>((us % 60000000ULL) / 1000000ULL);
        uint32_t millis = static_cast<uint32_t>((us % 1000000ULL) / 1000ULL);
        oss << minutes << "m " << seconds << "." 
            << std::setfill('0') << std::setw(3) << millis << "s";
    } else {
        // 1h or more: show as hours, minutes, seconds
        uint32_t hours = static_cast<uint32_t>(us / 3600000000ULL);
        uint32_t minutes = static_cast<uint32_t>((us % 3600000000ULL) / 60000000ULL);
        uint32_t seconds = static_cast<uint32_t>((us % 60000000ULL) / 1000000ULL);
        uint32_t millis = static_cast<uint32_t>((us % 1000000ULL) / 1000ULL);
        oss << hours << "h " << minutes << "m " << seconds << "."
            << std::setfill('0') << std::setw(3) << millis << "s";
    }
    
    return oss.str();
}

/**
 * @brief Format latency value (microseconds) as human-readable string
 * @param us Latency in microseconds
 * @param precision Number of decimal places (default: 3)
 * @return String like "1.234ms" or "567µs"
 * 
 * Examples:
 * - 567 µs → "567µs"
 * - 1234 µs → "1.234ms"
 * - 5000 µs → "5.000ms"
 * 
 * @note Optimized for typical MIDI latency values (0-100ms)
 * 
 * @code
 * auto latency = TimeUtils::formatLatency(1234);
 * // "1.234ms"
 * @endcode
 */
inline std::string formatLatency(uint64_t us, int precision = 3) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);
    
    // If value exceeds safe range, log warning in debug builds
    if (us > (1ULL << 53)) {
        // In debug mode, we could log this, but for a header-only
        // implementation we just clamp to a reasonable max value
        us = (1ULL << 53);
    }
    
    if (us < 1000) {
        // Less than 1ms: show as µs (no decimals)
        oss << std::setprecision(0) << us << "µs";
    } else if (us < 1000000) {
        // Less than 1s: show as ms
        oss << (static_cast<double>(us) / 1000.0) << "ms";
    } else {
        // 1s or more: show as seconds
        oss << (static_cast<double>(us) / 1000000.0) << "s";
    }
    
    return oss.str();
}

/**
 * @brief Format timestamp as ISO 8601 string (system time)
 * @param us Microseconds since Unix epoch
 * @return ISO 8601 string like "2024-01-15T10:30:45.123456Z"
 * 
 * @note Uses system_clock, not steady_clock
 */
inline std::string formatISO8601(Timestamp us) {
    auto tp = std::chrono::system_clock::time_point(
        std::chrono::microseconds(us)
    );
    
    auto tt = std::chrono::system_clock::to_time_t(tp);
    auto tm = *std::gmtime(&tt);
    
    auto micros = us % 1000000;
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(6) << micros << "Z";
    
    return oss.str();
}

/**
 * @brief Format current system time as ISO 8601 string
 * @return ISO 8601 string like "2024-01-15T10:30:45.123456Z"
 */
inline std::string formatISO8601Now() {
    return formatISO8601(systemNow());
}

/**
 * @brief Format timestamp as simple date/time (local time)
 * @param us Microseconds since Unix epoch
 * @return String like "2024-01-15 10:30:45"
 * 
 * @note Uses system_clock and local time zone
 */
inline std::string formatDateTime(Timestamp us) {
    auto tp = std::chrono::system_clock::time_point(
        std::chrono::microseconds(us)
    );
    
    auto tt = std::chrono::system_clock::to_time_t(tp);
    auto tm = *std::localtime(&tt);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    return oss.str();
}

// ============================================================================
// PARSING
// ============================================================================

/**
 * @brief Parse duration string to microseconds
 * @param str Duration string (e.g., "1.5s", "100ms", "500us")
 * @return Microseconds, or 0 if parsing fails
 * 
 * Supported formats:
 * - "123us" or "123µs" → 123 microseconds
 * - "123ms" → 123000 microseconds
 * - "1.5s" or "1.5sec" → 1500000 microseconds
 * - "1m" or "1min" → 60000000 microseconds
 * 
 * @note Case-insensitive, whitespace is ignored
 */
inline Timestamp parseDuration(const std::string& str) {
    if (str.empty()) return 0;
    
    // Extract numeric value
    size_t pos = 0;
    double value = 0.0;
    
    try {
        value = std::stod(str, &pos);
    } catch (...) {
        return 0;
    }
    
    // Extract unit (skip whitespace)
    while (pos < str.size() && std::isspace(str[pos])) {
        ++pos;
    }
    
    std::string unit;
    while (pos < str.size() && !std::isspace(str[pos])) {
        unit += std::tolower(str[pos]);
        ++pos;
    }
    
    // Convert to microseconds based on unit
    if (unit == "us" || unit == "µs" || unit == "usec") {
        return static_cast<Timestamp>(value);
    } else if (unit == "ms" || unit == "msec") {
        return static_cast<Timestamp>(value * 1000.0);
    } else if (unit == "s" || unit == "sec" || unit == "second" || unit == "seconds") {
        return static_cast<Timestamp>(value * 1000000.0);
    } else if (unit == "m" || unit == "min" || unit == "minute" || unit == "minutes") {
        return static_cast<Timestamp>(value * 60000000.0);
    } else if (unit == "h" || unit == "hr" || unit == "hour" || unit == "hours") {
        return static_cast<Timestamp>(value * 3600000000.0);
    }
    
    // No unit or unknown unit: assume microseconds
    return static_cast<Timestamp>(value);
}

// ============================================================================
// SLEEP / WAIT
// ============================================================================

/**
 * @brief Sleep for specified microseconds
 * @param us Microseconds to sleep
 * 
 * @note Actual sleep time may be longer due to scheduler granularity
 */
inline void sleepUs(Timestamp us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

/**
 * @brief Sleep for specified milliseconds
 * @param ms Milliseconds to sleep
 */
inline void sleepMs(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/**
 * @brief Sleep until specified timestamp
 * @param timestamp Target timestamp (microseconds)
 * 
 * @note Returns immediately if timestamp is in the past
 */
inline void sleepUntil(Timestamp timestamp) {
    Timestamp current = now();
    if (timestamp > current) {
        sleepUs(timestamp - current);
    }
}

} // namespace TimeUtils

} // namespace midiMind

// ============================================================================
// END OF FILE TimeUtils.h v4.1.0
// ============================================================================
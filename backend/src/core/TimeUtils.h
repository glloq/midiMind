// ============================================================================
// File: backend/src/core/TimeUtils.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
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

namespace TimeUtils {

using Timestamp = uint64_t;
using TimeDelta = int64_t;

inline Timestamp now() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

inline Timestamp systemNow() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

inline Timestamp add(Timestamp a, Timestamp b) {
    if (b > UINT64_MAX - a) {
        return UINT64_MAX;
    }
    return a + b;
}

inline Timestamp subtract(Timestamp a, Timestamp b) {
    if (b > a) {
        return 0;
    }
    return a - b;
}

inline TimeDelta delta(Timestamp later, Timestamp earlier) {
    if (later >= earlier) {
        return static_cast<TimeDelta>(later - earlier);
    } else {
        return -static_cast<TimeDelta>(earlier - later);
    }
}

inline bool isPast(Timestamp timestamp) {
    return timestamp < now();
}

inline bool isFuture(Timestamp timestamp) {
    return timestamp > now();
}

inline Timestamp elapsed(Timestamp timestamp) {
    return subtract(now(), timestamp);
}

inline Timestamp remaining(Timestamp timestamp) {
    return subtract(timestamp, now());
}

inline constexpr Timestamp msToUs(uint32_t ms) {
    return static_cast<Timestamp>(ms) * 1000ULL;
}

inline constexpr Timestamp secondsToUs(uint32_t seconds) {
    return static_cast<Timestamp>(seconds) * 1000000ULL;
}

inline constexpr uint32_t usToMs(Timestamp us) {
    return static_cast<uint32_t>(us / 1000ULL);
}

inline constexpr uint32_t usToSeconds(Timestamp us) {
    return static_cast<uint32_t>(us / 1000000ULL);
}

inline constexpr double usToSecondsDouble(Timestamp us) {
    return static_cast<double>(us) / 1000000.0;
}

inline constexpr double usToMsDouble(Timestamp us) {
    return static_cast<double>(us) / 1000.0;
}

inline std::string formatDuration(Timestamp us) {
    std::ostringstream oss;
    oss << std::fixed;
    
    if (us < 1000) {
        oss << us << "µs";
    } else if (us < 1000000) {
        oss << std::setprecision(3) << (static_cast<double>(us) / 1000.0) << "ms";
    } else if (us < 60000000) {
        oss << std::setprecision(3) << (static_cast<double>(us) / 1000000.0) << "s";
    } else if (us < 3600000000ULL) {
        uint32_t minutes = static_cast<uint32_t>(us / 60000000ULL);
        uint32_t seconds = static_cast<uint32_t>((us % 60000000ULL) / 1000000ULL);
        uint32_t millis = static_cast<uint32_t>((us % 1000000ULL) / 1000ULL);
        oss << minutes << "m " << seconds << "." 
            << std::setfill('0') << std::setw(3) << millis << "s";
    } else {
        uint32_t hours = static_cast<uint32_t>(us / 3600000000ULL);
        uint32_t minutes = static_cast<uint32_t>((us % 3600000000ULL) / 60000000ULL);
        uint32_t seconds = static_cast<uint32_t>((us % 60000000ULL) / 1000000ULL);
        uint32_t millis = static_cast<uint32_t>((us % 1000000ULL) / 1000ULL);
        oss << hours << "h " << minutes << "m " << seconds << "."
            << std::setfill('0') << std::setw(3) << millis << "s";
    }
    
    return oss.str();
}

inline std::string formatLatency(uint64_t us, int precision = 3) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);
    
    if (us < 1000) {
        oss << std::setprecision(0) << us << "µs";
    } else if (us < 1000000) {
        oss << (static_cast<double>(us) / 1000.0) << "ms";
    } else {
        oss << (static_cast<double>(us) / 1000000.0) << "s";
    }
    
    return oss.str();
}

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

inline std::string formatISO8601Now() {
    return formatISO8601(systemNow());
}

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

inline Timestamp parseDuration(const std::string& str) {
    if (str.empty()) return 0;
    
    size_t pos = 0;
    double value = 0.0;
    
    try {
        value = std::stod(str, &pos);
    } catch (...) {
        return 0;
    }
    
    while (pos < str.size() && std::isspace(str[pos])) {
        ++pos;
    }
    
    std::string unit;
    while (pos < str.size() && !std::isspace(str[pos])) {
        unit += std::tolower(str[pos]);
        ++pos;
    }
    
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
    
    return static_cast<Timestamp>(value);
}

inline void sleepUs(Timestamp us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

inline void sleepMs(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void sleepUntil(Timestamp timestamp) {
    Timestamp current = now();
    if (timestamp > current) {
        sleepUs(timestamp - current);
    }
}

} // namespace TimeUtils

} // namespace midiMind
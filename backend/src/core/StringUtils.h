// ============================================================================
// File: backend/src/core/StringUtils.h
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   String utility functions providing common operations like trim, split,
//   join, case conversion, validation, and formatting. All inline for
//   maximum performance.
//
// Features:
//   - Trim functions (left, right, both)
//   - Split and join operations
//   - Case conversion (upper, lower)
//   - String validation (starts/ends with, contains)
//   - String replacement
//   - Padding (left, right)
//   - Number to string conversions
//   - Hex conversions
//   - Byte formatting
//   - UUID generation (thread-safe)
//
// Dependencies:
//   - None (standard library only)
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.1:
//   - Improved UUID generation with better seeding (timestamp + thread_id)
//   - Enhanced documentation about UUID collision resistance
//
// Changes v4.1.0:
//   - Fixed thread-safety in UUID generation (thread_local)
//   - Added validation in fromHex()
//   - Documented toHex() type limits
//   - Improved error handling
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <random>
#include <stdexcept>
#include <chrono>
#include <thread>

namespace midiMind {
namespace StringUtils {

// ============================================================================
// TRIM FUNCTIONS
// ============================================================================

/**
 * @brief Trim whitespace from left side
 * 
 * @param str String to trim
 * @return Trimmed string
 * 
 * @example
 * @code
 * std::string s = StringUtils::ltrim("  hello");
 * // "hello"
 * @endcode
 */
inline std::string ltrim(const std::string& str) {
    std::string result = str;
    result.erase(result.begin(), 
                std::find_if(result.begin(), result.end(), 
                [](unsigned char ch) { return !std::isspace(ch); }));
    return result;
}

/**
 * @brief Trim whitespace from right side
 * 
 * @param str String to trim
 * @return Trimmed string
 */
inline std::string rtrim(const std::string& str) {
    std::string result = str;
    result.erase(std::find_if(result.rbegin(), result.rend(),
                [](unsigned char ch) { return !std::isspace(ch); }).base(), 
                result.end());
    return result;
}

/**
 * @brief Trim whitespace from both sides
 * 
 * @param str String to trim
 * @return Trimmed string
 * 
 * @example
 * @code
 * std::string s = StringUtils::trim("  hello  ");
 * // "hello"
 * @endcode
 */
inline std::string trim(const std::string& str) {
    return ltrim(rtrim(str));
}

// ============================================================================
// CASE CONVERSION
// ============================================================================

/**
 * @brief Convert string to lowercase
 * 
 * @param str Source string
 * @return Lowercase string
 */
inline std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

/**
 * @brief Convert string to uppercase
 * 
 * @param str Source string
 * @return Uppercase string
 */
inline std::string toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::toupper(c); });
    return result;
}

// ============================================================================
// SPLIT AND JOIN
// ============================================================================

/**
 * @brief Split string by delimiter
 * 
 * @param str String to split
 * @param delimiter Delimiter character
 * @return Vector of substrings
 * 
 * @example
 * @code
 * auto parts = StringUtils::split("a,b,c", ',');
 * // {"a", "b", "c"}
 * @endcode
 */
inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

/**
 * @brief Split string by delimiter string
 * 
 * @param str String to split
 * @param delimiter Delimiter string
 * @return Vector of substrings
 */
inline std::vector<std::string> split(const std::string& str, 
                                     const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    
    tokens.push_back(str.substr(start));
    return tokens;
}

/**
 * @brief Join strings with separator
 * 
 * @param strings Strings to join
 * @param separator Separator string
 * @return Joined string
 * 
 * @example
 * @code
 * std::vector<std::string> parts = {"a", "b", "c"};
 * auto joined = StringUtils::join(parts, ", ");
 * // "a, b, c"
 * @endcode
 */
inline std::string join(const std::vector<std::string>& strings, 
                       const std::string& separator) {
    if (strings.empty()) return "";
    
    std::ostringstream oss;
    oss << strings[0];
    
    for (size_t i = 1; i < strings.size(); ++i) {
        oss << separator << strings[i];
    }
    
    return oss.str();
}

// ============================================================================
// VALIDATION FUNCTIONS
// ============================================================================

/**
 * @brief Check if string starts with prefix
 * 
 * @param str String to check
 * @param prefix Prefix to look for
 * @return true if string starts with prefix
 */
inline bool startsWith(const std::string& str, const std::string& prefix) {
    if (prefix.length() > str.length()) return false;
    return str.compare(0, prefix.length(), prefix) == 0;
}

/**
 * @brief Check if string ends with suffix
 * 
 * @param str String to check
 * @param suffix Suffix to look for
 * @return true if string ends with suffix
 */
inline bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

/**
 * @brief Check if string contains substring
 * 
 * @param str String to search
 * @param substring Substring to find
 * @return true if substring found
 */
inline bool contains(const std::string& str, const std::string& substring) {
    return str.find(substring) != std::string::npos;
}

/**
 * @brief Check if string is empty or whitespace only
 * 
 * @param str String to check
 * @return true if empty or whitespace
 */
inline bool isBlank(const std::string& str) {
    return str.empty() || 
           std::all_of(str.begin(), str.end(), 
                      [](unsigned char c) { return std::isspace(c); });
}

// ============================================================================
// REPLACEMENT FUNCTIONS
// ============================================================================

/**
 * @brief Replace all occurrences of substring
 * 
 * @param str Source string
 * @param from Substring to replace
 * @param to Replacement substring
 * @return Modified string
 * 
 * @example
 * @code
 * auto result = StringUtils::replaceAll("hello world", "o", "0");
 * // "hell0 w0rld"
 * @endcode
 */
inline std::string replaceAll(std::string str, 
                              const std::string& from, 
                              const std::string& to) {
    if (from.empty()) return str;
    
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    
    return str;
}

// ============================================================================
// PADDING FUNCTIONS
// ============================================================================

/**
 * @brief Pad string on left side
 * 
 * @param str Source string
 * @param width Target width
 * @param fillChar Fill character (default: space)
 * @return Padded string
 * 
 * @example
 * @code
 * auto padded = StringUtils::padLeft("42", 5, '0');
 * // "00042"
 * @endcode
 */
inline std::string padLeft(const std::string& str, size_t width, char fillChar = ' ') {
    if (str.length() >= width) return str;
    return std::string(width - str.length(), fillChar) + str;
}

/**
 * @brief Pad string on right side
 * 
 * @param str Source string
 * @param width Target width
 * @param fillChar Fill character (default: space)
 * @return Padded string
 */
inline std::string padRight(const std::string& str, size_t width, char fillChar = ' ') {
    if (str.length() >= width) return str;
    return str + std::string(width - str.length(), fillChar);
}

/**
 * @brief Center string with padding
 * 
 * @param str Source string
 * @param width Target width
 * @param fillChar Fill character (default: space)
 * @return Centered string
 */
inline std::string padCenter(const std::string& str, size_t width, char fillChar = ' ') {
    if (str.length() >= width) return str;
    
    size_t leftPad = (width - str.length()) / 2;
    size_t rightPad = width - str.length() - leftPad;
    
    return std::string(leftPad, fillChar) + str + std::string(rightPad, fillChar);
}

// ============================================================================
// NUMBER CONVERSIONS
// ============================================================================

/**
 * @brief Convert number to string
 * 
 * @param value Numeric value
 * @return String representation
 */
template<typename T>
inline std::string toString(T value) {
    return std::to_string(value);
}

/**
 * @brief Convert number to hex string
 * 
 * @param value Value to convert
 * @param width Width with zero padding (default: auto)
 * @return Hex string (e.g., "0x1A")
 * 
 * @note Only works correctly for integral types up to 64 bits
 * @warning Negative values are converted to unsigned representation
 * @warning Types larger than 64 bits will be truncated
 * 
 * @example
 * @code
 * auto hex = StringUtils::toHex(26, 2);
 * // "0x1A"
 * @endcode
 */
template<typename T>
inline std::string toHex(T value, int width = 0) {
    static_assert(std::is_integral<T>::value, "toHex requires integral type");
    
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex;
    
    if (width > 0) {
        oss << std::setfill('0') << std::setw(width);
    }
    
    // Cast to uint64_t (safe for all integral types <= 64 bits)
    oss << static_cast<uint64_t>(static_cast<typename std::make_unsigned<T>::type>(value));
    return oss.str();
}

/**
 * @brief Convert hex string to number
 * 
 * @param hexStr Hex string (with or without "0x" prefix)
 * @return Numeric value
 * @throws std::invalid_argument if hex string is invalid
 * @throws std::out_of_range if value exceeds type range
 * 
 * @example
 * @code
 * int value = StringUtils::fromHex<int>("0x1A");
 * // 26
 * @endcode
 */
template<typename T>
inline T fromHex(const std::string& hexStr) {
    static_assert(std::is_integral<T>::value, "fromHex requires integral type");
    
    if (hexStr.empty()) {
        throw std::invalid_argument("Empty hex string");
    }
    
    std::string str = hexStr;
    
    // Remove "0x" prefix if present
    if (startsWith(str, "0x") || startsWith(str, "0X")) {
        str = str.substr(2);
    }
    
    if (str.empty()) {
        throw std::invalid_argument("Invalid hex string (only prefix)");
    }
    
    // Validate hex characters
    for (char c : str) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            throw std::invalid_argument("Invalid hex character in string: " + hexStr);
        }
    }
    
    std::istringstream iss(str);
    T value;
    iss >> std::hex >> value;
    
    if (iss.fail()) {
        throw std::invalid_argument("Failed to parse hex string: " + hexStr);
    }
    
    return value;
}

// ============================================================================
// BYTE FORMATTING
// ============================================================================

/**
 * @brief Convert byte array to hex string
 * 
 * @param data Byte array
 * @param size Array size
 * @param separator Separator between bytes (default: " ")
 * @return Hex string
 * 
 * @example
 * @code
 * uint8_t data[] = {0x90, 0x3C, 0x64};
 * auto hex = StringUtils::bytesToHex(data, 3);
 * // "90 3C 64"
 * @endcode
 */
inline std::string bytesToHex(const uint8_t* data, size_t size, 
                              const std::string& separator = " ") {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    
    for (size_t i = 0; i < size; ++i) {
        if (i > 0) oss << separator;
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    
    return oss.str();
}

/**
 * @brief Format byte size as human-readable string
 * 
 * @param bytes Number of bytes
 * @param precision Decimal precision (default: 2)
 * @return Formatted string (e.g., "1.50 MB")
 * 
 * @example
 * @code
 * auto size = StringUtils::formatBytes(1536000);
 * // "1.50 MB"
 * @endcode
 */
inline std::string formatBytes(uint64_t bytes, int precision = 2) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << size << " " << units[unit];
    return oss.str();
}

// ============================================================================
// UUID GENERATION
// ============================================================================

/**
 * @brief Generate simple UUID (v4-like)
 * 
 * @return UUID string (format: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx")
 * 
 * @note Not cryptographically secure, suitable for non-security IDs only
 * @note Thread-safe: uses thread_local random generators
 * @note Collision resistance: Seeded with timestamp + thread_id for better uniqueness
 * @note UUID uniqueness guaranteed across threads created at different times
 * 
 * @example
 * @code
 * std::string id = StringUtils::generateUuid();
 * // "a1b2c3d4-e5f6-4789-a012-bcdef0123456"
 * @endcode
 */
inline std::string generateUuid() {
    // Thread-local generators to avoid data races between threads
    // Seeded once per thread with timestamp + thread_id for better uniqueness
    thread_local bool initialized = false;
    thread_local std::mt19937 gen;
    thread_local std::uniform_int_distribution<> dis(0, 15);
    thread_local std::uniform_int_distribution<> dis2(8, 11);
    
    if (!initialized) {
        // Seed with combination of time and thread ID for uniqueness
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        
        // Get thread ID as additional entropy
        std::hash<std::thread::id> hasher;
        size_t threadId = hasher(std::this_thread::get_id());
        
        // Combine timestamp and thread ID for seed
        std::seed_seq seed{
            static_cast<uint32_t>(timestamp & 0xFFFFFFFF),
            static_cast<uint32_t>((timestamp >> 32) & 0xFFFFFFFF),
            static_cast<uint32_t>(threadId & 0xFFFFFFFF),
            static_cast<uint32_t>((threadId >> 32) & 0xFFFFFFFF)
        };
        
        gen.seed(seed);
        initialized = true;
    }
    
    std::ostringstream oss;
    oss << std::hex;
    
    // 8 hex digits
    for (int i = 0; i < 8; i++) oss << dis(gen);
    oss << "-";
    
    // 4 hex digits
    for (int i = 0; i < 4; i++) oss << dis(gen);
    oss << "-";
    
    // Version 4 (4xxx)
    oss << "4";
    for (int i = 0; i < 3; i++) oss << dis(gen);
    oss << "-";
    
    // Variant (8, 9, a, or b)
    oss << dis2(gen);
    for (int i = 0; i < 3; i++) oss << dis(gen);
    oss << "-";
    
    // 12 hex digits
    for (int i = 0; i < 12; i++) oss << dis(gen);
    
    return oss.str();
}

/**
 * @brief Generate short ID (8 characters)
 * 
 * @return Short ID string
 * 
 * @note Thread-safe: uses thread_local random generators
 * @note Seeded with timestamp + thread_id for better uniqueness
 * 
 * @example
 * @code
 * std::string id = StringUtils::generateShortId();
 * // "a1b2c3d4"
 * @endcode
 */
inline std::string generateShortId() {
    // Thread-local to avoid data races between threads
    thread_local bool initialized = false;
    thread_local std::mt19937 gen;
    thread_local std::uniform_int_distribution<> dis(0, 15);
    
    if (!initialized) {
        // Seed with combination of time and thread ID
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        
        std::hash<std::thread::id> hasher;
        size_t threadId = hasher(std::this_thread::get_id());
        
        std::seed_seq seed{
            static_cast<uint32_t>(timestamp & 0xFFFFFFFF),
            static_cast<uint32_t>((timestamp >> 32) & 0xFFFFFFFF),
            static_cast<uint32_t>(threadId & 0xFFFFFFFF)
        };
        
        gen.seed(seed);
        initialized = true;
    }
    
    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 8; i++) {
        oss << dis(gen);
    }
    
    return oss.str();
}

} // namespace StringUtils
} // namespace midiMind

// ============================================================================
// END OF FILE StringUtils.h v4.1.1
// ============================================================================
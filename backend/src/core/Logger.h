// ============================================================================
// File: backend/src/core/Logger.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Thread-safe logging system with multiple severity levels and automatic
//   log file rotation. Header-only implementation using Meyer's Singleton.
//
// Features:
//   - 5 severity levels (DEBUG, INFO, WARNING, ERROR, CRITICAL)
//   - Console output with ANSI colors
//   - File logging with automatic rotation
//   - Category filtering
//   - Thread-safe (mutex protected)
//   - Metrics tracking
//   - Optional syslog support (Linux)
//
// Thread-safety: YES (all methods are thread-safe)
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// ============================================================================

#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <atomic>
#include <filesystem>
#include <algorithm>

#ifdef __linux__
#include <syslog.h>
#endif

namespace midiMind {

/**
 * @class Logger
 * @brief Thread-safe logging system with automatic rotation
 * 
 * @details
 * Singleton logger providing multi-level logging with color-coded console
 * output and optional file logging with automatic rotation.
 * 
 * Log levels (from least to most severe):
 * - DEBUG: Detailed debugging information
 * - INFO: General informational messages
 * - WARNING: Warning messages (potential issues)
 * - ERROR: Error messages (failures)
 * - CRITICAL: Critical failures (system unstable)
 * 
 * Features:
 * - Automatic log rotation when file size exceeds limit
 * - Keeps multiple backup files (configurable)
 * - Category filtering
 * - ANSI color codes for terminal output
 * - Thread-safe operations
 * - Performance metrics
 * 
 * @example Basic usage
 * @code
 * Logger::info("Application", "Starting MidiMind v4.1.0");
 * Logger::error("Device", "Failed to connect to device");
 * Logger::debug("Router", "Message routed to output port 0");
 * @endcode
 * 
 * @example File logging with rotation
 * @code
 * Logger::enableFileLogging("/var/log/midimind/app.log", 
 *                          10 * 1024 * 1024,  // 10 MB max
 *                          5);                 // Keep 5 backups
 * @endcode
 */
class Logger {
public:
    // ========================================================================
    // LOG LEVELS
    // ========================================================================
    
    /**
     * @enum Level
     * @brief Severity levels for log messages
     */
    enum class Level {
        DEBUG = 0,    ///< Detailed debugging information
        INFO = 1,     ///< General informational messages
        WARNING = 2,  ///< Warning messages
        ERROR = 3,    ///< Error messages
        CRITICAL = 4  ///< Critical failures
    };
    
    // ========================================================================
    // PUBLIC METHODS - LOGGING
    // ========================================================================
    
    /**
     * @brief Log a DEBUG message
     * @param category Message category (e.g., "MIDI", "Database")
     * @param message The message to log
     * @note Thread-safe
     */
    static void debug(const std::string& category, const std::string& message) {
        log(Level::DEBUG, category, message);
    }
    
    /**
     * @brief Log an INFO message
     * @param category Message category
     * @param message The message to log
     * @note Thread-safe
     */
    static void info(const std::string& category, const std::string& message) {
        log(Level::INFO, category, message);
    }
    
    /**
     * @brief Log a WARNING message
     * @param category Message category
     * @param message The message to log
     * @note Thread-safe
     */
    static void warning(const std::string& category, const std::string& message) {
        log(Level::WARNING, category, message);
    }
    
    /**
     * @brief Log an ERROR message
     * @param category Message category
     * @param message The message to log
     * @note Thread-safe
     */
    static void error(const std::string& category, const std::string& message) {
        log(Level::ERROR, category, message);
    }
    
    /**
     * @brief Log a CRITICAL message
     * @param category Message category
     * @param message The message to log
     * @note Thread-safe
     */
    static void critical(const std::string& category, const std::string& message) {
        log(Level::CRITICAL, category, message);
    }
    
    // ========================================================================
    // PUBLIC METHODS - CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Set minimum log level
     * @param level Minimum level to log (messages below this are filtered)
     * @note Thread-safe
     */
    static void setLevel(Level level) {
        std::lock_guard<std::mutex> lock(getMutex());
        getMinLevel() = level;
    }
    
    /**
     * @brief Get current minimum log level
     * @return Current minimum level
     * @note Thread-safe
     */
    static Level getLevel() {
        std::lock_guard<std::mutex> lock(getMutex());
        return getMinLevel();
    }
	
     /**
     * @brief Get recent log messages
     * @param count Number of recent messages to retrieve
     * @return Vector of recent log messages
     * @note Thread-safe
     */
    static std::vector<std::string> getRecentLogs(size_t count = 100) {
        std::lock_guard<std::mutex> lock(getMutex());
        auto& buffer = getLogBuffer();
        
        size_t start = buffer.size() > count ? buffer.size() - count : 0;
        return std::vector<std::string>(buffer.begin() + start, buffer.end());
    }
    
    /**
     * @brief Clear all log messages from buffer
     * @note Thread-safe
     */
    static void clearLogs() {
        std::lock_guard<std::mutex> lock(getMutex());
        getLogBuffer().clear();
    }
    
    /**
     * @brief Export logs to file
     * @param filename Path to export file
     * @return true if successful, false otherwise
     * @note Thread-safe
     */
    static bool exportLogs(const std::string& filename) {
        std::lock_guard<std::mutex> lock(getMutex());
        
        std::ofstream outFile(filename);
        if (!outFile.is_open()) {
            return false;
        }
        
        for (const auto& logEntry : getLogBuffer()) {
            outFile << logEntry << "\n";
        }
        
        outFile.close();
        return true;
    }
    /**
     * @brief Enable file logging with automatic rotation
     * 
     * @param filepath Path to log file
     * @param maxSizeBytes Maximum file size before rotation (default: 10 MB)
     * @param maxBackups Number of backup files to keep (default: 5)
     * 
     * @note When file exceeds maxSizeBytes, it's renamed to filepath.0,
     *       previous backups are renamed to filepath.1, filepath.2, etc.
     *       Oldest backup beyond maxBackups is deleted.
     * 
     * @note Thread-safe
     * 
     * @example
     * @code
     * Logger::enableFileLogging("/var/log/midimind.log");
     * // Creates: midimind.log, midimind.log.0, midimind.log.1, ...
     * @endcode
     */
    static void enableFileLogging(const std::string& filepath, 
                                  size_t maxSizeBytes = 10 * 1024 * 1024,
                                  size_t maxBackups = 5) {
        std::lock_guard<std::mutex> lock(getMutex());
        
        // Close existing log file if open
        if (getLogFile().is_open()) {
            getLogFile().close();
        }
        
        // Store configuration
        getLogFilePath() = filepath;
        getMaxFileSize() = maxSizeBytes;
        getMaxBackups() = maxBackups;
        
        // Create parent directories if needed
        std::filesystem::path logPath(filepath);
        if (logPath.has_parent_path()) {
            std::filesystem::create_directories(logPath.parent_path());
        }
        
        // Open log file
        getLogFile().open(filepath, std::ios::app);
        getFileLoggingEnabled() = getLogFile().is_open();
        
        if (!getFileLoggingEnabled()) {
            std::cerr << "Failed to open log file: " << filepath << std::endl;
        }
    }
    
    /**
     * @brief Disable file logging
     * @note Thread-safe
     */
    static void disableFileLogging() {
        std::lock_guard<std::mutex> lock(getMutex());
        
        if (getLogFile().is_open()) {
            getLogFile().close();
        }
        
        getFileLoggingEnabled() = false;
    }
    
    /**
     * @brief Enable syslog output (Linux only)
     * @param ident Syslog identifier string
     * @note Thread-safe
     */
    static void enableSyslog(const std::string& ident = "midimind") {
#ifdef __linux__
        std::lock_guard<std::mutex> lock(getMutex());
        openlog(ident.c_str(), LOG_PID | LOG_CONS, LOG_USER);
        getSyslogEnabled() = true;
#else
        (void)ident;  // Suppress unused parameter warning
#endif
    }
    
    /**
     * @brief Disable syslog output
     * @note Thread-safe
     */
    static void disableSyslog() {
#ifdef __linux__
        std::lock_guard<std::mutex> lock(getMutex());
        if (getSyslogEnabled()) {
            closelog();
            getSyslogEnabled() = false;
        }
#endif
    }
    
    /**
     * @brief Set category filter (only log these categories)
     * @param categories Vector of category names to include
     * @note Empty filter = log all categories
     * @note Thread-safe
     */
    static void setCategories(const std::vector<std::string>& categories) {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryFilter() = categories;
    }
    
    /**
     * @brief Clear category filter (log all categories)
     * @note Thread-safe
     */
    static void clearCategories() {
        std::lock_guard<std::mutex> lock(getMutex());
        getCategoryFilter().clear();
    }
    
    // ========================================================================
    // PUBLIC METHODS - METRICS
    // ========================================================================
    
    /**
     * @brief Get total message count
     * @return Number of messages logged since startup
     */
    static uint64_t getTotalMessageCount() {
        return getTotalMessages().load();
    }
    
    /**
     * @brief Get message count by level
     * @param level Log level
     * @return Number of messages at this level
     */
    static uint64_t getMessageCountByLevel(Level level) {
        switch (level) {
            case Level::DEBUG:    return getDebugMessages().load();
            case Level::INFO:     return getInfoMessages().load();
            case Level::WARNING:  return getWarningMessages().load();
            case Level::ERROR:    return getErrorMessages().load();
            case Level::CRITICAL: return getCriticalMessages().load();
            default:              return 0;
        }
    }
    
    /**
     * @brief Get filtered message count
     * @return Number of messages filtered by level or category
     */
    static uint64_t getFilteredMessageCount() {
        return getFilteredMessages().load();
    }
    
    /**
     * @brief Get rotation count
     * @return Number of log file rotations
     */
    static uint64_t getRotationCount() {
        return getRotations().load();
    }
    
private:
    // ========================================================================
    // PRIVATE METHODS - CORE LOGGING
    // ========================================================================
    
    /**
     * @brief Core logging function (private, called by public methods)
     * @param level Log level
     * @param category Message category
     * @param message Message text
     * @note Must be called with mutex locked (handled by public methods)
     */
    static void log(Level level, const std::string& category, const std::string& message) {
        std::lock_guard<std::mutex> lock(getMutex());
        
        // Filter by level
        if (level < getMinLevel()) {
            getFilteredMessages()++;
            return;
        }
        
        // Filter by category (if filter is active)
        if (!getCategoryFilter().empty()) {
            bool found = std::find(getCategoryFilter().begin(), 
                                  getCategoryFilter().end(), 
                                  category) != getCategoryFilter().end();
            if (!found) {
                getFilteredMessages()++;
                return;
            }
        }
        
        // Update counters
        getTotalMessages()++;
        switch (level) {
            case Level::DEBUG:    getDebugMessages()++; break;
            case Level::INFO:     getInfoMessages()++; break;
            case Level::WARNING:  getWarningMessages()++; break;
            case Level::ERROR:    getErrorMessages()++; break;
            case Level::CRITICAL: getCriticalMessages()++; break;
        }
        
        // Format message
        std::string timestamp = getCurrentTimestamp();
        std::string levelStr = levelToString(level);
        
        // Store in buffer
        std::ostringstream bufferEntry;
        bufferEntry << "[" << timestamp << "] "
                   << "[" << levelStr << "] "
                   << "[" << category << "] "
                   << message;
        getLogBuffer().push_back(bufferEntry.str());
        
        // Console output with colors
        std::cout << getColorCode(level)
                  << "[" << timestamp << "] "
                  << "[" << levelStr << "] "
                  << "[" << category << "] "
                  << message
                  << "\033[0m" << std::endl;
        
        // Check rotation before writing to file
        checkAndRotateLog();
        
        // File output
        if (getFileLoggingEnabled() && getLogFile().is_open()) {
            auto& file = getLogFile();
            file << "[" << timestamp << "] "
                 << "[" << levelStr << "] "
                 << "[" << category << "] "
                 << message << std::endl;
            file.flush();
        }
        
        // Syslog output
#ifdef __linux__
        if (getSyslogEnabled()) {
            int priority = LOG_INFO;
            switch (level) {
                case Level::DEBUG:    priority = LOG_DEBUG; break;
                case Level::INFO:     priority = LOG_INFO; break;
                case Level::WARNING:  priority = LOG_WARNING; break;
                case Level::ERROR:    priority = LOG_ERR; break;
                case Level::CRITICAL: priority = LOG_CRIT; break;
            }
            syslog(priority, "[%s] %s", category.c_str(), message.c_str());
        }
#endif
    }
    
    // ========================================================================
    // LOG ROTATION
    // ========================================================================
    
    /**
     * @brief Check file size and rotate if necessary
     * @note Must be called with mutex locked
     */
    static void checkAndRotateLog() {
        if (!getFileLoggingEnabled() || !getLogFile().is_open()) {
            return;
        }
        
        // Check current file size
        auto currentPos = getLogFile().tellp();
        if (currentPos < 0 || static_cast<size_t>(currentPos) < getMaxFileSize()) {
            return;
        }
        
        // Rotate log files
        getLogFile().close();
        
        const std::string& path = getLogFilePath();
        size_t maxBackups = getMaxBackups();
        
        // Delete oldest backup if exists
        std::string oldestBackup = path + "." + std::to_string(maxBackups - 1);
        if (std::filesystem::exists(oldestBackup)) {
            std::filesystem::remove(oldestBackup);
        }
        
        // Rename backups (n-1 -> n)
        for (int i = maxBackups - 2; i >= 0; --i) {
            std::string oldName = (i == 0) ? path : path + "." + std::to_string(i - 1);
            std::string newName = path + "." + std::to_string(i);
            
            if (std::filesystem::exists(oldName)) {
                std::filesystem::rename(oldName, newName);
            }
        }
        
        // Rename current log to .0
        if (std::filesystem::exists(path)) {
            std::filesystem::rename(path, path + ".0");
        }
        
        // Open new log file
        getLogFile().open(path, std::ios::app);
        getRotations()++;
        
        // Log rotation event
        if (getLogFile().is_open()) {
            auto& file = getLogFile();
            file << "[" << getCurrentTimestamp() << "] ";
            file << "[INFO] [Logger] Log rotated (rotation #" 
                 << getRotations() << ")" << std::endl;
        }
    }
    
    // ========================================================================
    // UTILITY FUNCTIONS
    // ========================================================================
    
    /**
     * @brief Get current timestamp as string
     * @return Formatted timestamp (YYYY-MM-DD HH:MM:SS.mmm)
     */
    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm tm_now;
        localtime_r(&time_t_now, &tm_now);
        
        std::ostringstream ss;
        ss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S")
           << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        return ss.str();
    }
    
    /**
     * @brief Convert level enum to string
     * @param level Log level
     * @return String representation
     */
    static std::string levelToString(Level level) {
        switch (level) {
            case Level::DEBUG:    return "DEBUG";
            case Level::INFO:     return "INFO ";
            case Level::WARNING:  return "WARN ";
            case Level::ERROR:    return "ERROR";
            case Level::CRITICAL: return "CRIT ";
            default:              return "?????";
        }
    }
    
    /**
     * @brief Get ANSI color code for level
     * @param level Log level
     * @return ANSI escape code
     */
    static const char* getColorCode(Level level) {
        switch (level) {
            case Level::DEBUG:    return "\033[36m";  // Cyan
            case Level::INFO:     return "\033[32m";  // Green
            case Level::WARNING:  return "\033[33m";  // Yellow
            case Level::ERROR:    return "\033[31m";  // Red
            case Level::CRITICAL: return "\033[35m";  // Magenta
            default:              return "\033[0m";   // Reset
        }
    }
    
    // ========================================================================
    // STATIC ACCESSORS (Meyer's Singleton Pattern)
    // ========================================================================
    
    static std::mutex& getMutex() {
        static std::mutex mutex;
        return mutex;
    }
    
    static Level& getMinLevel() {
        static Level level = Level::INFO;
        return level;
    }
    
    static std::ofstream& getLogFile() {
        static std::ofstream file;
        return file;
    }
    
    static std::string& getLogFilePath() {
        static std::string path;
        return path;
    }
    
    static bool& getFileLoggingEnabled() {
        static bool enabled = false;
        return enabled;
    }
    
    static bool& getSyslogEnabled() {
        static bool enabled = false;
        return enabled;
    }
    
    static std::vector<std::string>& getCategoryFilter() {
        static std::vector<std::string> filter;
        return filter;
    }
    
    static size_t& getMaxFileSize() {
        static size_t size = 10 * 1024 * 1024;  // 10 MB default
        return size;
    }
    
    static size_t& getMaxBackups() {
        static size_t count = 5;  // 5 backups default
        return count;
    }
    
    // Atomic counters
    static std::atomic<uint64_t>& getTotalMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getDebugMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getInfoMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getWarningMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getErrorMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getCriticalMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getFilteredMessages() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
    
    static std::atomic<uint64_t>& getRotations() {
        static std::atomic<uint64_t> count{0};
        return count;
    }
	static std::vector<std::string>& getLogBuffer() {
        static std::vector<std::string> buffer;
        static const size_t MAX_BUFFER_SIZE = 1000;
        
        // Limiter la taille du buffer
        if (buffer.size() >= MAX_BUFFER_SIZE) {
            buffer.erase(buffer.begin(), buffer.begin() + 100); // Remove oldest 100 entries
        }
        
        return buffer;
    }
};

} // namespace midiMind

// ============================================================================
// END OF FILE Logger.h v4.1.0
// ============================================================================
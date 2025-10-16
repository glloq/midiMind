// ============================================================================
// File: backend/src/core/ErrorManager.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Centralized error management system using Observer pattern. Tracks error
//   history, maintains statistics, and notifies registered observers of errors.
//   Header-only singleton with thread-safe access.
//
// Features:
//   - Error reporting with severity levels
//   - Observer pattern for error notifications
//   - Error history tracking
//   - Error statistics
//   - Thread-safe operations
//
// Dependencies:
//   - Logger.h
//   - Error.h
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified implementation
//   - Improved error history management
//   - Enhanced callback system
//   - Better statistics tracking
//   - English documentation
//
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "Logger.h"
#include "Error.h"

namespace midiMind {

// ============================================================================
// ERROR MANAGER CLASS
// ============================================================================

/**
 * @class ErrorManager
 * @brief Centralized error management with Observer pattern
 * 
 * @details
 * Singleton that manages all application errors. Modules report errors
 * via reportError(), and registered observers are notified automatically.
 * Maintains error history and statistics.
 * 
 * Typical usage:
 * - ApiServer registers as observer to broadcast errors to WebSocket clients
 * - Modules report errors instead of just logging them
 * - Frontend can display error notifications based on severity
 * 
 * @example Basic usage
 * @code
 * // Register an observer
 * ErrorManager::instance().addObserver([](const auto& error) {
 *     if (error.severity >= ErrorManager::Severity::ERROR) {
 *         // Broadcast to WebSocket clients
 *         apiServer.broadcast("error", error.toJson());
 *     }
 * });
 * 
 * // Report an error from any module
 * ErrorManager::instance().reportError(
 *     "MidiRouter",
 *     "Failed to route message",
 *     ErrorManager::Severity::ERROR,
 *     "Device disconnected unexpectedly"
 * );
 * @endcode
 */
class ErrorManager {
public:
    // ========================================================================
    // SEVERITY LEVELS
    // ========================================================================
    
    /**
     * @enum Severity
     * @brief Error severity levels
     */
    enum class Severity {
        INFO = 0,       ///< Informational (not really an error)
        WARNING = 1,    ///< Warning (potential issue)
        ERROR = 2,      ///< Error (operation failed)
        CRITICAL = 3    ///< Critical error (system unstable)
    };
    
    // ========================================================================
    // ERROR INFO STRUCTURE
    // ========================================================================
    
    /**
     * @struct ErrorInfo
     * @brief Complete error information
     */
    struct ErrorInfo {
        std::string module;         ///< Module that reported the error
        std::string message;        ///< Error message
        Severity severity;          ///< Error severity
        std::string details;        ///< Additional details (optional)
        int64_t timestamp;          ///< Timestamp (milliseconds since epoch)
        
        /**
         * @brief Constructor
         */
        ErrorInfo(const std::string& mod, const std::string& msg,
                 Severity sev, const std::string& det = "")
            : module(mod), message(msg), severity(sev), details(det) {
            // Calculate timestamp
            timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
        
        /**
         * @brief Convert severity to string
         * @return String representation of severity
         */
        std::string severityToString() const {
            switch (severity) {
                case Severity::INFO:     return "INFO";
                case Severity::WARNING:  return "WARNING";
                case Severity::ERROR:    return "ERROR";
                case Severity::CRITICAL: return "CRITICAL";
                default:                 return "UNKNOWN";
            }
        }
        
        /**
         * @brief Format error as string
         * @return Formatted error string
         */
        std::string toString() const {
            std::ostringstream oss;
            oss << "[" << severityToString() << "] "
                << "[" << module << "] "
                << message;
            if (!details.empty()) {
                oss << " - " << details;
            }
            return oss.str();
        }
        
        /**
         * @brief Convert timestamp to readable string
         * @return Formatted timestamp
         */
        std::string getTimestampString() const {
            auto timeT = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(timestamp)
                )
            );
            
            std::tm tm;
            localtime_r(&timeT, &tm);
            
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            
            // Add milliseconds
            int ms = timestamp % 1000;
            oss << "." << std::setfill('0') << std::setw(3) << ms;
            
            return oss.str();
        }
    };
    
    // ========================================================================
    // OBSERVER CALLBACK TYPE
    // ========================================================================
    
    /**
     * @typedef ObserverCallback
     * @brief Observer callback function type
     * 
     * @details
     * Observers are called with complete ErrorInfo when an error is reported.
     */
    using ObserverCallback = std::function<void(const ErrorInfo&)>;
    
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    /**
     * @brief Get singleton instance (thread-safe)
     * @return Reference to ErrorManager singleton
     */
    static ErrorManager& instance() {
        static ErrorManager instance;
        return instance;
    }
    
    // Disable copy and move
    ErrorManager(const ErrorManager&) = delete;
    ErrorManager& operator=(const ErrorManager&) = delete;
    ErrorManager(ErrorManager&&) = delete;
    ErrorManager& operator=(ErrorManager&&) = delete;
    
    // ========================================================================
    // ERROR REPORTING
    // ========================================================================
    
    /**
     * @brief Report an error
     * 
     * @param module Module name reporting the error
     * @param message Error message
     * @param severity Error severity level
     * @param details Additional details (optional)
     * 
     * @note Thread-safe
     * @note Observers are notified synchronously
     * @note Error is added to history and logged
     * 
     * @example
     * @code
     * ErrorManager::instance().reportError(
     *     "Database",
     *     "Failed to execute query",
     *     ErrorManager::Severity::ERROR,
     *     "Connection timeout after 30s"
     * );
     * @endcode
     */
    void reportError(const std::string& module,
                    const std::string& message,
                    Severity severity,
                    const std::string& details = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Create ErrorInfo
        ErrorInfo error(module, message, severity, details);
        
        // Add to history
        history_.push_back(error);
        
        // Limit history size
        if (history_.size() > maxHistorySize_) {
            history_.pop_front();
        }
        
        // Update statistics
        updateStats(severity);
        
        // Log error
        logError(error);
        
        // Notify observers
        notifyObservers(error);
    }
    
    /**
     * @brief Report error from exception
     * 
     * @param module Module name
     * @param exception MidiMindException instance
     * 
     * @note Thread-safe
     * @note Severity is determined based on error code
     */
    void reportError(const std::string& module, 
                    const MidiMindException& exception) {
        // Determine severity based on error code
        Severity severity = Severity::ERROR;
        
        int code = static_cast<int>(exception.code());
        if (code >= 1800) {  // System errors
            severity = Severity::CRITICAL;
        } else if (code >= 1700) {  // Processing errors
            severity = Severity::WARNING;
        }
        
        reportError(module, exception.message(), severity, 
                   "Error code: " + std::to_string(code));
    }
    
    // ========================================================================
    // OBSERVER MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Register an observer
     * 
     * @param observer Callback function to be called on errors
     * 
     * @note Thread-safe
     * @note Observers are called in registration order
     * 
     * @example
     * @code
     * ErrorManager::instance().addObserver([](const auto& error) {
     *     if (error.severity >= ErrorManager::Severity::ERROR) {
     *         notifyUser(error.message);
     *     }
     * });
     * @endcode
     */
    void addObserver(ObserverCallback observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.push_back(observer);
    }
    
    /**
     * @brief Remove all observers
     * @note Thread-safe
     */
    void clearObservers() {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.clear();
    }
    
    /**
     * @brief Get number of registered observers
     * @return Observer count
     * @note Thread-safe
     */
    size_t getObserverCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return observers_.size();
    }
    
    // ========================================================================
    // ERROR HISTORY
    // ========================================================================
    
    /**
     * @brief Get error history
     * 
     * @param maxCount Maximum number of errors to return (0 = all)
     * @return Vector of ErrorInfo (most recent last)
     * 
     * @note Thread-safe
     * @note Returns a COPY of history
     */
    std::vector<ErrorInfo> getHistory(size_t maxCount = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<ErrorInfo> result;
        
        if (maxCount == 0 || maxCount >= history_.size()) {
            // Return all history
            result.assign(history_.begin(), history_.end());
        } else {
            // Return N most recent errors
            auto start = history_.end() - maxCount;
            result.assign(start, history_.end());
        }
        
        return result;
    }
    
    /**
     * @brief Clear error history
     * @note Thread-safe
     * @note Statistics are not reset
     */
    void clearHistory() {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.clear();
    }
    
    /**
     * @brief Set maximum history size
     * @param size Maximum size (default: 100)
     * @note Thread-safe
     */
    void setMaxHistorySize(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        maxHistorySize_ = size;
        
        // Truncate if necessary
        while (history_.size() > maxHistorySize_) {
            history_.pop_front();
        }
    }
    
    /**
     * @brief Get maximum history size
     * @return Maximum history size
     * @note Thread-safe
     */
    size_t getMaxHistorySize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return maxHistorySize_;
    }
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @struct Statistics
     * @brief Error statistics
     */
    struct Statistics {
        size_t totalErrors = 0;         ///< Total errors
        size_t infoCount = 0;           ///< Info count
        size_t warningCount = 0;        ///< Warning count
        size_t errorCount = 0;          ///< Error count
        size_t criticalCount = 0;       ///< Critical count
        int64_t lastErrorTimestamp = 0; ///< Last error timestamp (ms)
    };
    
    /**
     * @brief Get error statistics
     * @return Statistics structure
     * @note Thread-safe
     */
    Statistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
    /**
     * @brief Reset statistics
     * @note Thread-safe
     * @note History is not cleared
     */
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = Statistics();
    }

private:
    // ========================================================================
    // PRIVATE CONSTRUCTOR
    // ========================================================================
    
    /**
     * @brief Private constructor (Singleton)
     */
    ErrorManager() : maxHistorySize_(100) {}
    
    /**
     * @brief Destructor
     */
    ~ErrorManager() = default;
    
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Notify all observers
     * @param error Error information
     * @note Must be called with mutex locked
     */
    void notifyObservers(const ErrorInfo& error) {
        for (const auto& observer : observers_) {
            try {
                observer(error);
            } catch (const std::exception& e) {
                // Observer threw exception, log but continue
                Logger::error("ErrorManager", 
                    "Observer threw exception: " + std::string(e.what()));
            } catch (...) {
                Logger::error("ErrorManager", "Observer threw unknown exception");
            }
        }
    }
    
    /**
     * @brief Update statistics
     * @param severity Error severity
     * @note Must be called with mutex locked
     */
    void updateStats(Severity severity) {
        stats_.totalErrors++;
        stats_.lastErrorTimestamp = std::chrono::duration_cast
            std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        
        switch (severity) {
            case Severity::INFO:
                stats_.infoCount++;
                break;
            case Severity::WARNING:
                stats_.warningCount++;
                break;
            case Severity::ERROR:
                stats_.errorCount++;
                break;
            case Severity::CRITICAL:
                stats_.criticalCount++;
                break;
        }
    }
    
    /**
     * @brief Log error to Logger
     * @param error Error information
     * @note Must be called with mutex locked
     */
    void logError(const ErrorInfo& error) {
        std::string logMessage = error.message;
        if (!error.details.empty()) {
            logMessage += " - " + error.details;
        }
        
        switch (error.severity) {
            case Severity::INFO:
                Logger::info(error.module, logMessage);
                break;
            case Severity::WARNING:
                Logger::warning(error.module, logMessage);
                break;
            case Severity::ERROR:
                Logger::error(error.module, logMessage);
                break;
            case Severity::CRITICAL:
                Logger::critical(error.module, logMessage);
                break;
        }
    }
    
    // ========================================================================
    // MEMBERS
    // ========================================================================
    
    mutable std::mutex mutex_;              ///< Thread-safety
    std::vector<ObserverCallback> observers_; ///< Registered observers
    std::deque<ErrorInfo> history_;         ///< Error history
    size_t maxHistorySize_;                 ///< Max history size
    Statistics stats_;                      ///< Error statistics
};

} // namespace midiMind

// ============================================================================
// END OF FILE ErrorManager.h v4.1.0
// ============================================================================
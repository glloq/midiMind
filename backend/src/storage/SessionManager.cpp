// ============================================================================
// File: backend/src/storage/SessionManager.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Manager for application sessions (complete state snapshots)
//
// Features:
//   - Create, load, save, delete sessions
//   - Complete application state capture
//   - Auto-save with configurable interval
//   - Import/export JSON files
//   - Active session management
//   - Search and statistics
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Enhanced auto-save mechanism
//   - Better state management
//   - Improved error handling
//   - Session duplication
//
// ============================================================================

#pragma once

#include "../core/Error.h"
#include "Database.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct Session
 * @brief Complete application session
 */
struct Session {
    int id = 0;                ///< Database ID
    std::string name;          ///< Session name
    json data;                 ///< Complete configuration
    std::string createdAt;     ///< Creation timestamp
    std::string updatedAt;     ///< Last update timestamp
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     */
    static Session fromJson(const json& j);
};

// ============================================================================
// CLASS: SessionManager
// ============================================================================

/**
 * @class SessionManager
 * @brief Manager for application sessions
 * 
 * Manages complete application state snapshots.
 * A session contains:
 * - All settings
 * - MIDI configuration (routes, devices)
 * - Active presets
 * - Processor states
 * - Player state
 * 
 * Thread Safety: YES (all public methods are thread-safe)
 * 
 * Database Schema:
 * ```sql
 * CREATE TABLE sessions (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT,
 *     name TEXT NOT NULL,
 *     data TEXT NOT NULL,
 *     created_at TEXT NOT NULL,
 *     updated_at TEXT NOT NULL
 * );
 * ```
 * 
 * Example:
 * ```cpp
 * auto db = std::make_shared<Database>("midimind.db");
 * db->open();
 * 
 * SessionManager manager(db);
 * 
 * // Create session
 * json sessionData = {
 *     {"midi_routes", {...}},
 *     {"devices", {...}},
 *     {"settings", {...}}
 * };
 * 
 * int id = manager.create("My Session", sessionData);
 * 
 * // Set as active
 * manager.setActive(id);
 * 
 * // Enable auto-save
 * manager.setAutoSave(true, 300); // Every 5 minutes
 * 
 * // Load session
 * auto session = manager.load(id);
 * ```
 */
class SessionManager {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback for auto-save
     * Should capture current state and return it
     */
    using AutoSaveCallback = std::function<json()>;
    
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param database Shared pointer to database
     * @throws MidiMindException if database not opened
     */
    explicit SessionManager(std::shared_ptr<Database> database);
    
    /**
     * @brief Destructor
     * @note Stops auto-save thread gracefully
     */
    ~SessionManager();
    
    // Disable copy
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    
    // ========================================================================
    // CRUD - CREATE
    // ========================================================================
    
    /**
     * @brief Create new session
     * @param name Session name
     * @param data Session data
     * @return int Session ID
     * @throws MidiMindException on database error
     * @note Thread-safe
     */
    int create(const std::string& name, const json& data);
    
    // ========================================================================
    // CRUD - READ
    // ========================================================================
    
    /**
     * @brief Load session
     * @param id Session ID
     * @return Session or std::nullopt if not found
     * @note Thread-safe
     */
    std::optional<Session> load(int id);
    
    /**
     * @brief List all sessions
     * @return Vector of sessions (without full data)
     * @note Thread-safe
     * @note Returns only metadata for performance
     */
    std::vector<Session> list();
    
    /**
     * @brief Search sessions by name
     * @param query Search query
     * @return Vector of matching sessions
     * @note Thread-safe
     * @note Case-insensitive search
     */
    std::vector<Session> search(const std::string& query);
    
    /**
     * @brief Check if session exists
     * @param id Session ID
     * @return true if exists
     * @note Thread-safe
     */
    bool exists(int id);
    
    // ========================================================================
    // CRUD - UPDATE
    // ========================================================================
    
    /**
     * @brief Update session
     * @param id Session ID
     * @param name New name
     * @param data New data
     * @throws MidiMindException if session not found
     * @note Thread-safe
     */
    void update(int id, const std::string& name, const json& data);
    
    /**
     * @brief Save session data only (keep name)
     * @param id Session ID
     * @param data New data
     * @throws MidiMindException if session not found
     * @note Thread-safe
     */
    void save(int id, const json& data);
    
    // ========================================================================
    // CRUD - DELETE
    // ========================================================================
    
    /**
     * @brief Delete session
     * @param id Session ID
     * @return true if deleted
     * @note Thread-safe
     * @note Cannot delete active session
     */
    bool remove(int id);
    
    /**
     * @brief Cleanup old sessions
     * @param daysOld Delete sessions older than this many days
     * @return int Number of sessions deleted
     * @note Thread-safe
     */
    int cleanup(int daysOld = 30);
    
    // ========================================================================
    // ACTIVE SESSION
    // ========================================================================
    
    /**
     * @brief Set active session
     * @param id Session ID
     * @note Thread-safe
     */
    void setActive(int id);
    
    /**
     * @brief Get active session ID
     * @return int Active session ID (0 if none)
     * @note Thread-safe
     */
    int getActive() const;
    
    /**
     * @brief Get active session data
     * @return json Session data or empty if no active session
     * @note Thread-safe
     */
    json getActiveData();
    
    /**
     * @brief Save current state to active session
     * @param data Current state
     * @note Thread-safe
     * @note Does nothing if no active session
     */
    void saveActive(const json& data);
    
    // ========================================================================
    // AUTO-SAVE
    // ========================================================================
    
    /**
     * @brief Enable/disable auto-save
     * @param enabled Enable auto-save
     * @param intervalSec Interval in seconds (default 300 = 5 min)
     * @note Thread-safe
     * @note Starts/stops auto-save thread
     */
    void setAutoSave(bool enabled, uint32_t intervalSec = 300);
    
    /**
     * @brief Check if auto-save is enabled
     * @return true if enabled
     * @note Thread-safe
     */
    bool isAutoSaveEnabled() const;
    
    /**
     * @brief Get auto-save interval
     * @return uint32_t Interval in seconds
     * @note Thread-safe
     */
    uint32_t getAutoSaveInterval() const;
    
    /**
     * @brief Set auto-save callback
     * @param callback Function that captures current state
     * @note Thread-safe
     * @note Callback will be called from auto-save thread
     */
    void setAutoSaveCallback(AutoSaveCallback callback);
    
    // ========================================================================
    // IMPORT / EXPORT
    // ========================================================================
    
    /**
     * @brief Export session to JSON file
     * @param id Session ID
     * @param filepath File path
     * @return true if exported
     * @note Thread-safe
     */
    bool exportToFile(int id, const std::string& filepath);
    
    /**
     * @brief Import session from JSON file
     * @param filepath File path
     * @return int Session ID or -1 on error
     * @note Thread-safe
     */
    int importFromFile(const std::string& filepath);
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    /**
     * @brief Duplicate session
     * @param id Source session ID
     * @param newName Name for duplicate (optional)
     * @return int New session ID or -1 on error
     * @note Thread-safe
     */
    int duplicate(int id, const std::string& newName = "");
    
    /**
     * @brief Get session count
     * @return size_t Number of sessions
     * @note Thread-safe
     */
    size_t count() const;
    
    /**
     * @brief Get statistics
     * @return json Statistics
     * @note Thread-safe
     */
    json getStatistics() const;

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Auto-save thread function
     */
    void autoSaveThread();
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Database connection
    std::shared_ptr<Database> database_;
    
    /// Active session ID
    std::atomic<int> activeSessionId_;
    
    /// Auto-save configuration
    std::atomic<bool> autoSaveEnabled_;
    std::atomic<uint32_t> autoSaveInterval_;
    std::thread autoSaveThread_;
    std::atomic<bool> stopAutoSave_;
    
    /// Auto-save callback
    AutoSaveCallback autoSaveCallback_;
    
    /// Thread safety
    mutable std::mutex mutex_;
};

} // namespace midiMind
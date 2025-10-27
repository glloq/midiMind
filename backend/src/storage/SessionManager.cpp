// ============================================================================
// File: backend/src/storage/SessionManager.cpp
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.1.1:
//   - Fixed double-locking in update() and save() methods
//   - Added existsUnsafe() to avoid recursive lock acquisition
//   - Replaced polling loop in autoSaveThread with condition_variable
//   - Better thread lifecycle management
//
// ============================================================================

#include "SessionManager.h"
#include "../core/Logger.h"
#include "../core/TimeUtils.h"
#include <fstream>
#include <chrono>
#include <algorithm>

namespace midiMind {

// ============================================================================
// Session Structure Implementation
// ============================================================================

json Session::toJson() const {
    return {
        {"id", id},
        {"name", name},
        {"data", data},
        {"created_at", createdAt},
        {"updated_at", updatedAt}
    };
}

Session Session::fromJson(const json& j) {
    Session session;
    session.id = j.value("id", 0);
    session.name = j.value("name", "");
    session.data = j.value("data", json::object());
    session.createdAt = j.value("created_at", "");
    session.updatedAt = j.value("updated_at", "");
    return session;
}

// ============================================================================
// SessionManager Implementation
// ============================================================================

SessionManager::SessionManager(Database& database)
    : database_(database)
    , activeSessionId_(0)
    , autoSaveEnabled_(false)
    , autoSaveInterval_(300)
    , stopAutoSave_(false)
{
    if (!database_.isConnected()) {
        throw MidiMindException(ErrorCode::DATABASE_ERROR, "Database not opened");
    }
    
    Logger::info("SessionManager", "Initialized");
}

SessionManager::~SessionManager() {
    // Stop auto-save thread
    stopAutoSave_ = true;
    autoSaveCv_.notify_all();  // Wake up thread immediately
    
    if (autoSaveThread_.joinable()) {
        autoSaveThread_.join();
    }
    
    Logger::info("SessionManager", "Destroyed");
}

// ============================================================================
// CRUD - CREATE
// ============================================================================

int SessionManager::create(const std::string& name, const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string timestamp = TimeUtils::formatISO8601Now();
    std::string dataStr = data.dump();
    
    std::string query = 
        "INSERT INTO sessions (name, data, created_at, updated_at) "
        "VALUES (?, ?, ?, ?)";
    
    auto result = database_.execute(query, {name, dataStr, timestamp, timestamp});
    int id = result.lastInsertId;
    
    Logger::info("SessionManager", "Created session: " + name + " (ID: " + std::to_string(id) + ")");
    
    return id;
}

// ============================================================================
// CRUD - READ
// ============================================================================

std::optional<Session> SessionManager::load(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string query = "SELECT id, name, data, created_at, updated_at FROM sessions WHERE id = ?";
    auto results = database_.query(query, {std::to_string(id)});
    
    if (results.empty()) {
        return std::nullopt;
    }
    
    Session session;
    session.id = std::stoi(results.rows[0].at("id"));
    session.name = results.rows[0].at("name");
    session.data = json::parse(results.rows[0].at("data"));
    session.createdAt = results.rows[0].at("created_at");
    session.updatedAt = results.rows[0].at("updated_at");
    
    return session;
}

std::vector<Session> SessionManager::list() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string query = "SELECT id, name, created_at, updated_at FROM sessions ORDER BY updated_at DESC";
    auto results = database_.query(query);
    
    std::vector<Session> sessions;
    for (const auto& row : results.rows) {
        Session session;
        session.id = std::stoi(row.at("id"));
        session.name = row.at("name");
        session.createdAt = row.at("created_at");
        session.updatedAt = row.at("updated_at");
        // data is intentionally not loaded for performance
        sessions.push_back(session);
    }
    
    return sessions;
}

std::vector<Session> SessionManager::search(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string sql = 
        "SELECT id, name, created_at, updated_at FROM sessions "
        "WHERE name LIKE ? ORDER BY updated_at DESC";
    
    std::string searchPattern = "%" + query + "%";
    auto results = database_.query(sql, {searchPattern});
    
    std::vector<Session> sessions;
    for (const auto& row : results.rows) {
        Session session;
        session.id = std::stoi(row.at("id"));
        session.name = row.at("name");
        session.createdAt = row.at("created_at");
        session.updatedAt = row.at("updated_at");
        sessions.push_back(session);
    }
    
    return sessions;
}

bool SessionManager::exists(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return existsUnsafe(id);
}

// ============================================================================
// CRUD - UPDATE
// ============================================================================

void SessionManager::update(int id, const std::string& name, const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Use existsUnsafe to avoid double-locking
    if (!existsUnsafe(id)) {
        throw MidiMindException(ErrorCode::FILE_NOT_FOUND, "Session not found: " + std::to_string(id));
    }
    
    std::string timestamp = TimeUtils::formatISO8601Now();
    std::string dataStr = data.dump();
    
    std::string query = 
        "UPDATE sessions SET name = ?, data = ?, updated_at = ? WHERE id = ?";
    
    database_.execute(query, {name, dataStr, timestamp, std::to_string(id)});
    
    Logger::info("SessionManager", "Updated session: " + name + " (ID: " + std::to_string(id) + ")");
}

void SessionManager::save(int id, const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Use existsUnsafe to avoid double-locking
    if (!existsUnsafe(id)) {
        throw MidiMindException(ErrorCode::FILE_NOT_FOUND, "Session not found: " + std::to_string(id));
    }
    
    std::string timestamp = TimeUtils::formatISO8601Now();
    std::string dataStr = data.dump();
    
    std::string query = 
        "UPDATE sessions SET data = ?, updated_at = ? WHERE id = ?";
    
    database_.execute(query, {dataStr, timestamp, std::to_string(id)});
}

// ============================================================================
// CRUD - DELETE
// ============================================================================

bool SessionManager::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Cannot delete active session
    if (id == activeSessionId_.load()) {
        Logger::warning("SessionManager", "Cannot delete active session");
        return false;
    }
    
    std::string query = "DELETE FROM sessions WHERE id = ?";
    auto execResult = database_.execute(query, {std::to_string(id)});
    int affected = execResult.affectedRows;
    
    if (affected > 0) {
        Logger::info("SessionManager", "Deleted session ID: " + std::to_string(id));
        return true;
    }
    
    return false;
}

int SessionManager::cleanup(int daysOld) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string query = 
        "DELETE FROM sessions WHERE id != ? AND "
        "julianday('now') - julianday(updated_at) > ?";
    
    int activeId = activeSessionId_.load();
    auto execResult = database_.execute(query, {
        std::to_string(activeId),
        std::to_string(daysOld)
    });
    int affected = execResult.affectedRows;
    
    if (affected > 0) {
        Logger::info("SessionManager", "Cleaned up " + std::to_string(affected) + " old sessions");
    }
    
    return affected;
}

// ============================================================================
// ACTIVE SESSION
// ============================================================================

void SessionManager::setActive(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (id != 0 && !existsUnsafe(id)) {
        throw MidiMindException(ErrorCode::FILE_NOT_FOUND, "Session not found: " + std::to_string(id));
    }
    
    activeSessionId_ = id;
    Logger::info("SessionManager", "Active session set to ID: " + std::to_string(id));
}

int SessionManager::getActive() const {
    return activeSessionId_.load();
}

json SessionManager::getActiveData() {
    int activeId = activeSessionId_.load();
    
    if (activeId == 0) {
        return json::object();
    }
    
    auto session = load(activeId);
    if (!session) {
        return json::object();
    }
    
    return session->data;
}

void SessionManager::saveActive(const json& data) {
    int activeId = activeSessionId_.load();
    
    if (activeId == 0) {
        return;
    }
    
    try {
        save(activeId, data);
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Failed to save active session: " + std::string(e.what()));
    }
}

// ============================================================================
// AUTO-SAVE
// ============================================================================

void SessionManager::setAutoSave(bool enabled, uint32_t intervalSec) {
    autoSaveEnabled_ = enabled;
    autoSaveInterval_ = intervalSec;
    
    if (enabled) {
        if (autoSaveThread_.joinable()) {
            stopAutoSave_ = true;
            autoSaveCv_.notify_all();
            autoSaveThread_.join();
            stopAutoSave_ = false;
        }
        
        autoSaveThread_ = std::thread(&SessionManager::autoSaveThread, this);
        Logger::info("SessionManager", "Auto-save enabled (interval: " + std::to_string(intervalSec) + "s)");
    } else {
        if (autoSaveThread_.joinable()) {
            stopAutoSave_ = true;
            autoSaveCv_.notify_all();
            autoSaveThread_.join();
            stopAutoSave_ = false;
        }
        Logger::info("SessionManager", "Auto-save disabled");
    }
}

bool SessionManager::isAutoSaveEnabled() const {
    return autoSaveEnabled_.load();
}

uint32_t SessionManager::getAutoSaveInterval() const {
    return autoSaveInterval_.load();
}

void SessionManager::setAutoSaveCallback(AutoSaveCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    autoSaveCallback_ = callback;
}

void SessionManager::autoSaveThread() {
    Logger::info("SessionManager", "Auto-save thread started");
    
    while (!stopAutoSave_.load()) {
        // Wait for interval or stop signal using condition_variable
        std::unique_lock<std::mutex> lock(autoSaveMutex_);
        auto interval = std::chrono::seconds(autoSaveInterval_.load());
        
        // Wait with timeout - wakes up on timeout or notify
        if (autoSaveCv_.wait_for(lock, interval, [this]() { 
            return stopAutoSave_.load(); 
        })) {
            // Woke up due to stop signal
            break;
        }
        
        // Check if auto-save still enabled
        if (!autoSaveEnabled_.load()) {
            continue;
        }
        
        // Get callback under lock and execute
        AutoSaveCallback callback;
        {
            std::lock_guard<std::mutex> callbackLock(mutex_);
            callback = autoSaveCallback_;
        }
        
        if (callback) {
            try {
                json data = callback();
                saveActive(data);
                Logger::debug("SessionManager", "Auto-save completed");
            } catch (const std::exception& e) {
                Logger::error("SessionManager", "Auto-save failed: " + std::string(e.what()));
            }
        }
    }
    
    Logger::info("SessionManager", "Auto-save thread stopped");
}

// ============================================================================
// IMPORT / EXPORT
// ============================================================================

bool SessionManager::exportToFile(int id, const std::string& filepath) {
    auto session = load(id);
    if (!session) {
        Logger::error("SessionManager", "Session not found for export: " + std::to_string(id));
        return false;
    }
    
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::error("SessionManager", "Failed to open file for export: " + filepath);
            return false;
        }
        
        file << session->toJson().dump(2);
        file.close();
        
        Logger::info("SessionManager", "Exported session to: " + filepath);
        return true;
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Export failed: " + std::string(e.what()));
        return false;
    }
}

int SessionManager::importFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::error("SessionManager", "Failed to open file for import: " + filepath);
            return -1;
        }
        
        json j;
        file >> j;
        file.close();
        
        Session session = Session::fromJson(j);
        
        // Create new session with imported data
        int newId = create(session.name + " (imported)", session.data);
        
        Logger::info("SessionManager", "Imported session from: " + filepath);
        return newId;
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Import failed: " + std::string(e.what()));
        return -1;
    }
}

// ============================================================================
// UTILITIES
// ============================================================================

int SessionManager::duplicate(int id, const std::string& newName) {
    auto session = load(id);
    if (!session) {
        Logger::error("SessionManager", "Session not found for duplication: " + std::to_string(id));
        return -1;
    }
    
    std::string name = newName.empty() ? session->name + " (copy)" : newName;
    
    try {
        int newId = create(name, session->data);
        Logger::info("SessionManager", "Duplicated session ID " + std::to_string(id) + " to " + std::to_string(newId));
        return newId;
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Duplication failed: " + std::string(e.what()));
        return -1;
    }
}

size_t SessionManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string query = "SELECT COUNT(*) FROM sessions";
    auto results = database_.query(query);
    
    if (results.empty()) {
        return 0;
    }
    
    return static_cast<size_t>(std::stoi(results.rows[0].at("count")));
}

json SessionManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats = {
        {"total_sessions", count()},
        {"active_session_id", activeSessionId_.load()},
        {"auto_save_enabled", autoSaveEnabled_.load()},
        {"auto_save_interval", autoSaveInterval_.load()}
    };
    
    // Get most recent session
    std::string query = "SELECT name, updated_at FROM sessions ORDER BY updated_at DESC LIMIT 1";
    auto results = database_.query(query);
    
    if (!results.empty()) {
        stats["most_recent_session"] = results.rows[0].at("name");
        stats["most_recent_update"] = results.rows[0].at("updated_at");
    }
    
    return stats;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool SessionManager::existsUnsafe(int id) const {
    // Must be called with mutex_ locked
    std::string query = "SELECT COUNT(*) FROM sessions WHERE id = ?";
    auto results = database_.query(query, {std::to_string(id)});
    
    return !results.empty() && std::stoi(results.rows[0].at("count")) > 0;
}

} // namespace midiMind
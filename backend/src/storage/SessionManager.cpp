// ============================================================================
// Fichier: backend/src/storage/SessionManager.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// Version: 3.0.0 - 2025-10-09
// ============================================================================
// Description:
//   Gestionnaire de sessions avec auto-save et persistence
//
// Fonctionnalités:
//   - CRUD sessions
//   - Auto-save configurable
//   - Gestion de la session active
//   - Export/Import
// ============================================================================

#include "SessionManager.h"
#include "../core/Logger.h"
#include <fstream>
#include <chrono>
#include <thread>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

SessionManager::SessionManager(std::shared_ptr<Database> database)
    : database_(database)
    , activeSessionId_(0)
    , autoSaveEnabled_(false)
    , autoSaveInterval_(300)
    , stopAutoSave_(false) {
    
    Logger::info("SessionManager", "SessionManager created");
}

SessionManager::~SessionManager() {
    // Arrêter l'auto-save
    setAutoSave(false);
    
    Logger::info("SessionManager", "SessionManager destroyed");
}

// ============================================================================
// CRUD SESSIONS
// ============================================================================

int SessionManager::create(const std::string& name, const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("SessionManager", "Creating session: " + name);
    
    try {
        auto result = database_->execute(
            "INSERT INTO sessions (name, data, created_at, updated_at) "
            "VALUES (?, ?, datetime('now'), datetime('now'))",
            {name, data.dump()}
        );
        
        int sessionId = static_cast<int>(result.lastInsertId);
        
        Logger::info("SessionManager", "✓ Session created (ID: " + std::to_string(sessionId) + ")");
        
        return sessionId;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Create failed: " + std::string(e.what()));
        throw;
    }
}

Session SessionManager::load(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("SessionManager", "Loading session ID: " + std::to_string(id));
    
    try {
        auto row = database_->queryOne(
            "SELECT * FROM sessions WHERE id = ?",
            {std::to_string(id)}
        );
        
        if (row.empty()) {
            throw std::runtime_error("Session not found: " + std::to_string(id));
        }
        
        Session session;
        session.id = std::stoi(row.at("id"));
        session.name = row.at("name");
        session.createdAt = row.at("created_at");
        session.updatedAt = row.at("updated_at");
        
        // Parser les données JSON
        if (row.contains("data") && !row.at("data").empty()) {
            session.data = json::parse(row.at("data"));
        }
        
        Logger::info("SessionManager", "✓ Session loaded: " + session.name);
        
        return session;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Load failed: " + std::string(e.what()));
        throw;
    }
}

void SessionManager::save(int id, const std::string& name, const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::debug("SessionManager", "Saving session ID: " + std::to_string(id));
    
    try {
        database_->execute(
            "UPDATE sessions SET name = ?, data = ?, updated_at = datetime('now') WHERE id = ?",
            {name, data.dump(), std::to_string(id)}
        );
        
        Logger::debug("SessionManager", "✓ Session saved");
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Save failed: " + std::string(e.what()));
        throw;
    }
}

void SessionManager::update(int id, const std::string& name, const json& data) {
    // Alias pour save()
    save(id, name, data);
}

void SessionManager::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("SessionManager", "Removing session ID: " + std::to_string(id));
    
    try {
        database_->execute("DELETE FROM sessions WHERE id = ?", {std::to_string(id)});
        
        // Si c'était la session active, la désactiver
        if (activeSessionId_ == id) {
            activeSessionId_ = 0;
        }
        
        Logger::info("SessionManager", "✓ Session removed");
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Remove failed: " + std::string(e.what()));
        throw;
    }
}

std::vector<Session> SessionManager::list() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->query(
            "SELECT id, name, created_at, updated_at FROM sessions ORDER BY updated_at DESC"
        );
        
        std::vector<Session> sessions;
        
        for (const auto& row : result.rows) {
            Session session;
            session.id = std::stoi(row.at("id"));
            session.name = row.at("name");
            session.createdAt = row.at("created_at");
            session.updatedAt = row.at("updated_at");
            // Ne pas charger les données complètes pour la liste
            
            sessions.push_back(session);
        }
        
        return sessions;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "List failed: " + std::string(e.what()));
        return {};
    }
}

bool SessionManager::exists(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto count = database_->queryScalar(
            "SELECT COUNT(*) FROM sessions WHERE id = ?",
            {std::to_string(id)}
        );
        
        return !count.empty() && std::stoi(count) > 0;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Exists check failed: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// GESTION SESSION ACTIVE
// ============================================================================

void SessionManager::setActive(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!exists(id)) {
        Logger::warn("SessionManager", "Cannot set active: session not found");
        return;
    }
    
    activeSessionId_ = id;
    Logger::info("SessionManager", "Active session set to: " + std::to_string(id));
}

int SessionManager::getActive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeSessionId_;
}

Session SessionManager::loadActive() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (activeSessionId_ == 0) {
        throw std::runtime_error("No active session");
    }
    
    // Unlock temporairement pour appeler load()
    mutex_.unlock();
    auto session = load(activeSessionId_);
    mutex_.lock();
    
    return session;
}

void SessionManager::saveActive(const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (activeSessionId_ == 0) {
        Logger::warn("SessionManager", "Cannot save: no active session");
        return;
    }
    
    try {
        // Récupérer le nom actuel
        auto row = database_->queryOne(
            "SELECT name FROM sessions WHERE id = ?",
            {std::to_string(activeSessionId_)}
        );
        
        if (row.empty()) {
            Logger::error("SessionManager", "Active session not found");
            return;
        }
        
        std::string name = row.at("name");
        
        // Unlock temporairement pour appeler save()
        mutex_.unlock();
        save(activeSessionId_, name, data);
        mutex_.lock();
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "SaveActive failed: " + std::string(e.what()));
    }
}

// ============================================================================
// AUTO-SAVE
// ============================================================================

void SessionManager::setAutoSave(bool enabled, int intervalSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Arrêter le thread actuel si en cours
    if (autoSaveEnabled_ && autoSaveThread_.joinable()) {
        stopAutoSave_ = true;
        mutex_.unlock();
        autoSaveThread_.join();
        mutex_.lock();
        stopAutoSave_ = false;
    }
    
    autoSaveEnabled_ = enabled;
    autoSaveInterval_ = intervalSeconds;
    
    if (enabled) {
        Logger::info("SessionManager", "Auto-save enabled (interval: " + 
                    std::to_string(intervalSeconds) + "s)");
        
        // Démarrer le thread d'auto-save
        autoSaveThread_ = std::thread([this]() {
            autoSaveLoop();
        });
    } else {
        Logger::info("SessionManager", "Auto-save disabled");
    }
}

void SessionManager::autoSaveLoop() {
    Logger::debug("SessionManager", "Auto-save loop started");
    
    while (!stopAutoSave_) {
        // Attendre l'intervalle
        for (int i = 0; i < autoSaveInterval_ && !stopAutoSave_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (stopAutoSave_) break;
        
        // Sauvegarder la session active
        try {
            if (activeSessionId_ != 0) {
                Logger::debug("SessionManager", "Auto-saving session...");
                
                // Note: On ne peut pas appeler saveActive() directement car
                // elle nécessite les données. Cette méthode devrait être appelée
                // par le code qui maintient l'état de la session active.
                
                if (onAutoSave_) {
                    onAutoSave_();
                }
            }
        } catch (const std::exception& e) {
            Logger::error("SessionManager", "Auto-save failed: " + std::string(e.what()));
        }
    }
    
    Logger::debug("SessionManager", "Auto-save loop stopped");
}

void SessionManager::setAutoSaveCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onAutoSave_ = callback;
}

bool SessionManager::isAutoSaveEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return autoSaveEnabled_;
}

int SessionManager::getAutoSaveInterval() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return autoSaveInterval_;
}

// ============================================================================
// EXPORT / IMPORT
// ============================================================================

bool SessionManager::exportToFile(int id, const std::string& filepath) {
    Logger::info("SessionManager", "Exporting session to: " + filepath);
    
    try {
        auto session = load(id);
        
        // Créer le JSON d'export
        json exportData;
        exportData["version"] = "1.0";
        exportData["exported_at"] = std::time(nullptr);
        exportData["session"] = {
            {"name", session.name},
            {"data", session.data},
            {"created_at", session.createdAt},
            {"updated_at", session.updatedAt}
        };
        
        // Écrire dans le fichier
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::error("SessionManager", "Cannot open file: " + filepath);
            return false;
        }
        
        file << exportData.dump(2); // Pretty print avec indentation
        file.close();
        
        Logger::info("SessionManager", "✓ Session exported");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Export failed: " + std::string(e.what()));
        return false;
    }
}

int SessionManager::importFromFile(const std::string& filepath) {
    Logger::info("SessionManager", "Importing session from: " + filepath);
    
    try {
        // Lire le fichier
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::error("SessionManager", "Cannot open file: " + filepath);
            return -1;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        // Parser le JSON
        json importData = json::parse(buffer.str());
        
        // Vérifier la version
        if (!importData.contains("version") || !importData.contains("session")) {
            Logger::error("SessionManager", "Invalid session file format");
            return -1;
        }
        
        auto sessionData = importData["session"];
        
        // Créer une nouvelle session
        std::string name = sessionData.value("name", "Imported Session");
        json data = sessionData.value("data", json::object());
        
        int newId = create(name, data);
        
        Logger::info("SessionManager", "✓ Session imported (ID: " + std::to_string(newId) + ")");
        return newId;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Import failed: " + std::string(e.what()));
        return -1;
    }
}

// ============================================================================
// DUPLICATION
// ============================================================================

int SessionManager::duplicate(int id, const std::string& newName) {
    Logger::info("SessionManager", "Duplicating session ID: " + std::to_string(id));
    
    try {
        // Charger la session source
        auto source = load(id);
        
        // Créer une copie avec un nouveau nom
        std::string copyName = newName.empty() ? (source.name + " (Copy)") : newName;
        
        int newId = create(copyName, source.data);
        
        Logger::info("SessionManager", "✓ Session duplicated (new ID: " + 
                    std::to_string(newId) + ")");
        
        return newId;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Duplicate failed: " + std::string(e.what()));
        return -1;
    }
}

// ============================================================================
// RECHERCHE
// ============================================================================

std::vector<Session> SessionManager::search(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->query(
            "SELECT id, name, created_at, updated_at FROM sessions "
            "WHERE name LIKE ? OR data LIKE ? "
            "ORDER BY updated_at DESC",
            {"%" + query + "%", "%" + query + "%"}
        );
        
        std::vector<Session> sessions;
        
        for (const auto& row : result.rows) {
            Session session;
            session.id = std::stoi(row.at("id"));
            session.name = row.at("name");
            session.createdAt = row.at("created_at");
            session.updatedAt = row.at("updated_at");
            
            sessions.push_back(session);
        }
        
        return sessions;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Search failed: " + std::string(e.what()));
        return {};
    }
}

// ============================================================================
// STATISTIQUES
// ============================================================================

json SessionManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->queryOne(R"(
            SELECT 
                COUNT(*) as total_sessions,
                AVG(LENGTH(data)) as avg_size
            FROM sessions
        )");
        
        int totalSessions = result.contains("total_sessions") ? 
            std::stoi(result.at("total_sessions")) : 0;
        double avgSize = result.contains("avg_size") ? 
            std::stod(result.at("avg_size")) : 0.0;
        
        return {
            {"total_sessions", totalSessions},
            {"avg_size_bytes", avgSize},
            {"active_session_id", activeSessionId_},
            {"auto_save_enabled", autoSaveEnabled_},
            {"auto_save_interval", autoSaveInterval_}
        };
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "GetStatistics failed: " + std::string(e.what()));
        return {{"error", std::string(e.what())}};
    }
}

size_t SessionManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto count = database_->queryScalar("SELECT COUNT(*) FROM sessions");
        return !count.empty() ? std::stoull(count) : 0;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Count failed: " + std::string(e.what()));
        return 0;
    }
}

// ============================================================================
// NETTOYAGE
// ============================================================================

void SessionManager::cleanup(int daysOld) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("SessionManager", "Cleaning up sessions older than " + 
                std::to_string(daysOld) + " days");
    
    try {
        database_->execute(
            "DELETE FROM sessions WHERE "
            "julianday('now') - julianday(updated_at) > ?",
            {std::to_string(daysOld)}
        );
        
        Logger::info("SessionManager", "✓ Cleanup completed");
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Cleanup failed: " + std::string(e.what()));
    }
}

void SessionManager::deleteAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::warn("SessionManager", "Deleting all sessions!");
    
    try {
        database_->execute("DELETE FROM sessions");
        activeSessionId_ = 0;
        
        Logger::info("SessionManager", "✓ All sessions deleted");
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "DeleteAll failed: " + std::string(e.what()));
    }
}

// ============================================================================
// RENOMMAGE
// ============================================================================

bool SessionManager::rename(int id, const std::string& newName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("SessionManager", "Renaming session ID: " + std::to_string(id));
    
    try {
        database_->execute(
            "UPDATE sessions SET name = ?, updated_at = datetime('now') WHERE id = ?",
            {newName, std::to_string(id)}
        );
        
        Logger::info("SessionManager", "✓ Session renamed to: " + newName);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Rename failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SessionManager.cpp
// ============================================================================

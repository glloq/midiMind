// ============================================================================
// Fichier: src/storage/SessionManager.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "SessionManager.h"
#include <fstream>

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
// CRUD
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
        THROW_ERROR(ErrorCode::DATABASE_INSERT_FAILED, 
                   "Failed to create session: " + std::string(e.what()));
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
            THROW_ERROR(ErrorCode::DATABASE_NOT_FOUND, 
                       "Session not found: " + std::to_string(id));
        }
        
        Session session;
        session.id = std::stoi(row.at("id"));
        session.name = row.at("name");
        session.data = json::parse(row.at("data"));
        session.createdAt = row.at("created_at");
        session.updatedAt = row.at("updated_at");
        
        Logger::info("SessionManager", "✓ Session loaded: " + session.name);
        
        return session;
        
    } catch (const MidiMindException&) {
        throw;
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::DATABASE_QUERY_FAILED,
                   "Failed to load session: " + std::string(e.what()));
    }
}

void SessionManager::update(int id, const std::string& name, const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("SessionManager", "Updating session ID: " + std::to_string(id));
    
    try {
        database_->execute(
            "UPDATE sessions SET name = ?, data = ?, updated_at = datetime('now') WHERE id = ?",
            {name, data.dump(), std::to_string(id)}
        );
        
        Logger::info("SessionManager", "✓ Session updated");
        
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::DATABASE_UPDATE_FAILED,
                   "Failed to update session: " + std::string(e.what()));
    }
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
        THROW_ERROR(ErrorCode::DATABASE_DELETE_FAILED,
                   "Failed to remove session: " + std::string(e.what()));
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
        Logger::error("SessionManager", "Failed to list sessions: " + std::string(e.what()));
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
        
    } catch (...) {
        return false;
    }
}

// ============================================================================
// IMPORT / EXPORT
// ============================================================================

bool SessionManager::exportToFile(int id, const std::string& filepath) {
    Logger::info("SessionManager", "Exporting session to: " + filepath);
    
    try {
        Session session = load(id);
        
        json exportData = session.toJson();
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::error("SessionManager", "Cannot create file: " + filepath);
            return false;
        }
        
        file << exportData.dump(2);
        file.close();
        
        Logger::info("SessionManager", "✓ Session exported");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Export failed: " + std::string(e.what()));
        return false;
    }
}

int SessionManager::importFromFile(const std::string& filepath, const std::string& name) {
    Logger::info("SessionManager", "Importing session from: " + filepath);
    
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            THROW_ERROR(ErrorCode::MIDI_FILE_OPEN_FAILED, 
                       "Cannot open file: " + filepath);
        }
        
        json importData;
        file >> importData;
        file.close();
        
        // Extraire les données
        json sessionData = importData.value("data", json::object());
        
        // Créer la session
        int sessionId = create(name, sessionData);
        
        Logger::info("SessionManager", "✓ Session imported (ID: " + std::to_string(sessionId) + ")");
        
        return sessionId;
        
    } catch (const MidiMindException&) {
        throw;
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::MIDI_FILE_READ_FAILED,
                   "Import failed: " + std::string(e.what()));
    }
}

// ============================================================================
// SESSION ACTIVE
// ============================================================================

void SessionManager::setActive(int id) {
    if (id != 0 && !exists(id)) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_FOUND,
                   "Session does not exist: " + std::to_string(id));
    }
    
    activeSessionId_ = id;
    
    Logger::info("SessionManager", "Active session set to: " + std::to_string(id));
}

int SessionManager::getActive() const {
    return activeSessionId_;
}

json SessionManager::getActiveData() {
    int id = activeSessionId_;
    
    if (id == 0) {
        return json::object();
    }
    
    try {
        Session session = load(id);
        return session.data;
    } catch (...) {
        return json::object();
    }
}

// ============================================================================
// AUTO-SAVE
// ============================================================================

void SessionManager::setAutoSave(bool enabled, uint32_t intervalSec) {
    Logger::info("SessionManager", "Auto-save " + std::string(enabled ? "enabled" : "disabled"));
    
    if (enabled && !autoSaveEnabled_) {
        autoSaveEnabled_ = true;
        autoSaveInterval_ = intervalSec;
        stopAutoSave_ = false;
        
        autoSaveThread_ = std::thread([this]() {
            autoSaveThread();
        });
        
    } else if (!enabled && autoSaveEnabled_) {
        autoSaveEnabled_ = false;
        stopAutoSave_ = true;
        
        if (autoSaveThread_.joinable()) {
            autoSaveThread_.join();
        }
    }
}

void SessionManager::saveActive(const json& data) {
    int id = activeSessionId_;
    
    if (id == 0) {
        Logger::warn("SessionManager", "No active session to save");
        return;
    }
    
    try {
        Session session = load(id);
        update(id, session.name, data);
        Logger::debug("SessionManager", "Active session saved");
    } catch (const std::exception& e) {
        Logger::error("SessionManager", "Failed to save active session: " + std::string(e.what()));
    }
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void SessionManager::autoSaveThread() {
    Logger::info("SessionManager", "Auto-save thread started");
    
    while (!stopAutoSave_) {
        // Attendre l'intervalle
        for (uint32_t i = 0; i < autoSaveInterval_ && !stopAutoSave_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (stopAutoSave_) break;
        
        // Auto-save de la session active
        int id = activeSessionId_;
        if (id != 0) {
            Logger::debug("SessionManager", "Auto-saving session ID: " + std::to_string(id));
            
            // TODO: Récupérer l'état actuel du système et sauvegarder
            // Pour l'instant, on ne fait rien car on n'a pas accès à l'Application
        }
    }
    
    Logger::info("SessionManager", "Auto-save thread stopped");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SessionManager.cpp
// ============================================================================
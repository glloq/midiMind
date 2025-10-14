// ============================================================================
// Fichier: backend/src/loop/LoopManager.cpp
// Version: 1.0.0
// Date: 2025-10-10
// ============================================================================

#include "LoopManager.h"
#include "../core/Error.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>

namespace midiMind {

// ============================================================================
// INITIALISATION
// ============================================================================

void LoopManager::initialize(std::shared_ptr<Database> database) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        Logger::warn("LoopManager", "Already initialized");
        return;
    }
    
    if (!database) {
        THROW_ERROR(ErrorCode::NULL_POINTER, "Database is null");
    }
    
    database_ = database;
    
    // Créer la table si nécessaire
    createTableIfNeeded();
    
    initialized_ = true;
    
    Logger::info("LoopManager", "✓ Initialized successfully");
}

void LoopManager::createTableIfNeeded() {
    if (!database_) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_INITIALIZED, "Database not set");
    }
    
    const std::string createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS loops (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            duration INTEGER NOT NULL,
            bars INTEGER NOT NULL,
            tempo INTEGER NOT NULL,
            time_signature TEXT NOT NULL,
            layers TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            last_modified INTEGER NOT NULL
        )
    )";
    
    database_->execute(createTableSQL);
    
    // Créer les index
    const std::string createIndexName = 
        "CREATE INDEX IF NOT EXISTS idx_loops_name ON loops(name)";
    database_->execute(createIndexName);
    
    const std::string createIndexModified = 
        "CREATE INDEX IF NOT EXISTS idx_loops_modified ON loops(last_modified DESC)";
    database_->execute(createIndexModified);
    
    Logger::debug("LoopManager", "Table 'loops' ready");
}

// ============================================================================
// CRUD OPERATIONS
// ============================================================================

json LoopManager::saveLoop(const json& loopData) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_INITIALIZED, 
                   "LoopManager not initialized");
    }
    
    // Valider les données
    validateLoop(loopData);
    
    // Créer la structure Loop
    Loop loop = Loop::fromJson(loopData);
    
    // Générer ID si nouveau loop
    if (loop.id.empty()) {
        loop.id = generateLoopId();
        loop.createdAt = std::chrono::system_clock::now()
            .time_since_epoch().count() / 1000000; // ms
        
        Logger::debug("LoopManager", "Creating new loop: " + loop.id);
    } else {
        // Vérifier si le loop existe
        if (!loopExists(loop.id)) {
            THROW_ERROR(ErrorCode::DATABASE_NOT_FOUND, 
                       "Loop not found: " + loop.id);
        }
        
        Logger::debug("LoopManager", "Updating loop: " + loop.id);
    }
    
    // Mettre à jour lastModified
    loop.lastModified = std::chrono::system_clock::now()
        .time_since_epoch().count() / 1000000;
    
    // Insérer ou mettre à jour
    if (loopExists(loop.id)) {
        updateLoop(loop);
    } else {
        insertLoop(loop);
    }
    
    Logger::info("LoopManager", "✓ Loop saved: " + loop.name + " (" + loop.id + ")");
    
    return loop.toJson();
}

std::optional<json> LoopManager::loadLoop(const std::string& loopId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_INITIALIZED, 
                   "LoopManager not initialized");
    }
    
    if (loopId.empty()) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, "Loop ID is empty");
    }
    
    const std::string query = 
        "SELECT * FROM loops WHERE id = ?";
    
    json result = database_->query(query, {loopId});
    
    if (result.empty()) {
        Logger::debug("LoopManager", "Loop not found: " + loopId);
        return std::nullopt;
    }
    
    Loop loop = rowToLoop(result[0]);
    
    Logger::debug("LoopManager", "✓ Loop loaded: " + loop.id);
    
    return loop.toJson();
}

json LoopManager::listLoops(int limit, int offset, 
                            const std::string& sortBy,
                            const std::string& sortOrder) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_INITIALIZED, 
                   "LoopManager not initialized");
    }
    
    // Valider les paramètres
    if (limit < 1 || limit > 1000) limit = 50;
    if (offset < 0) offset = 0;
    
    // Valider sortBy
    std::string validSortBy = "last_modified";
    if (sortBy == "name" || sortBy == "created_at" || 
        sortBy == "tempo" || sortBy == "bars") {
        validSortBy = sortBy;
    }
    
    // Valider sortOrder
    std::string validSortOrder = (sortOrder == "asc") ? "ASC" : "DESC";
    
    // Construire la requête
    std::stringstream query;
    query << "SELECT * FROM loops ORDER BY " 
          << validSortBy << " " << validSortOrder 
          << " LIMIT ? OFFSET ?";
    
    json result = database_->query(query.str(), {limit, offset});
    
    // Convertir en array de loops
    json loops = json::array();
    for (const auto& row : result) {
        Loop loop = rowToLoop(row);
        loops.push_back(loop.toJson());
    }
    
    Logger::debug("LoopManager", 
                 "✓ Listed " + std::to_string(loops.size()) + " loops");
    
    return loops;
}

bool LoopManager::deleteLoop(const std::string& loopId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_INITIALIZED, 
                   "LoopManager not initialized");
    }
    
    if (!loopExists(loopId)) {
        Logger::debug("LoopManager", "Loop not found for deletion: " + loopId);
        return false;
    }
    
    const std::string query = "DELETE FROM loops WHERE id = ?";
    database_->execute(query, {loopId});
    
    Logger::info("LoopManager", "✓ Loop deleted: " + loopId);
    
    return true;
}

json LoopManager::searchLoops(const std::string& query, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_INITIALIZED, 
                   "LoopManager not initialized");
    }
    
    if (query.empty()) {
        return json::array();
    }
    
    if (limit < 1 || limit > 100) limit = 20;
    
    const std::string searchQuery = 
        "SELECT * FROM loops WHERE name LIKE ? "
        "ORDER BY last_modified DESC LIMIT ?";
    
    std::string searchPattern = "%" + query + "%";
    
    json result = database_->query(searchQuery, {searchPattern, limit});
    
    json loops = json::array();
    for (const auto& row : result) {
        Loop loop = rowToLoop(row);
        loops.push_back(loop.toJson());
    }
    
    Logger::debug("LoopManager", 
                 "✓ Search found " + std::to_string(loops.size()) + 
                 " loops for: " + query);
    
    return loops;
}

int LoopManager::getTotalCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return 0;
    }
    
    const std::string query = "SELECT COUNT(*) as count FROM loops";
    json result = database_->query(query);
    
    if (result.empty()) {
        return 0;
    }
    
    return result[0].value("count", 0);
}

// ============================================================================
// VALIDATION
// ============================================================================

bool LoopManager::validateLoop(const json& loopData) {
    // Vérifier que c'est un objet
    if (!loopData.is_object()) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Loop data must be an object");
    }
    
    // Champs obligatoires
    if (!loopData.contains("name") || !loopData["name"].is_string()) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Loop must have a 'name' (string)");
    }
    
    if (!loopData.contains("duration") || !loopData["duration"].is_number()) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Loop must have a 'duration' (number)");
    }
    
    if (!loopData.contains("bars") || !loopData["bars"].is_number_integer()) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Loop must have 'bars' (integer)");
    }
    
    if (!loopData.contains("tempo") || !loopData["tempo"].is_number_integer()) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Loop must have 'tempo' (integer)");
    }
    
    if (!loopData.contains("timeSignature") || 
        !loopData["timeSignature"].is_string()) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Loop must have 'timeSignature' (string)");
    }
    
    if (!loopData.contains("layers") || !loopData["layers"].is_array()) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Loop must have 'layers' (array)");
    }
    
    // Validation des valeurs
    int bars = loopData["bars"];
    if (bars < 1 || bars > 64) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Bars must be between 1 and 64");
    }
    
    int tempo = loopData["tempo"];
    if (tempo < 20 || tempo > 300) {
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Tempo must be between 20 and 300 BPM");
    }
    
    int64_t duration = loopData["duration"];
    if (duration < 100 || duration > 3600000) { // 100ms à 1 heure
        THROW_ERROR(ErrorCode::INVALID_PARAMETER, 
                   "Duration must be between 100ms and 3600000ms");
    }
    
    return true;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

std::string LoopManager::generateLoopId() {
    // Générer un ID unique: loop_<timestamp>_<random>
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::stringstream ss;
    ss << "loop_" << timestamp << "_" << dis(gen);
    
    return ss.str();
}

bool LoopManager::loopExists(const std::string& loopId) {
    const std::string query = 
        "SELECT COUNT(*) as count FROM loops WHERE id = ?";
    
    json result = database_->query(query, {loopId});
    
    if (result.empty()) {
        return false;
    }
    
    return result[0].value("count", 0) > 0;
}

void LoopManager::insertLoop(const Loop& loop) {
    const std::string query = R"(
        INSERT INTO loops 
        (id, name, duration, bars, tempo, time_signature, layers, 
         created_at, last_modified)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    database_->execute(query, {
        loop.id,
        loop.name,
        loop.duration,
        loop.bars,
        loop.tempo,
        loop.timeSignature,
        loop.layers.dump(),
        loop.createdAt,
        loop.lastModified
    });
}

void LoopManager::updateLoop(const Loop& loop) {
    const std::string query = R"(
        UPDATE loops 
        SET name = ?, duration = ?, bars = ?, tempo = ?, 
            time_signature = ?, layers = ?, last_modified = ?
        WHERE id = ?
    )";
    
    database_->execute(query, {
        loop.name,
        loop.duration,
        loop.bars,
        loop.tempo,
        loop.timeSignature,
        loop.layers.dump(),
        loop.lastModified,
        loop.id
    });
}

Loop LoopManager::rowToLoop(const json& row) {
    Loop loop;
    
    loop.id = row.value("id", "");
    loop.name = row.value("name", "");
    loop.duration = row.value("duration", 0);
    loop.bars = row.value("bars", 4);
    loop.tempo = row.value("tempo", 120);
    loop.timeSignature = row.value("time_signature", "4/4");
    
    // Parser le JSON des layers
    std::string layersStr = row.value("layers", "[]");
    try {
        loop.layers = json::parse(layersStr);
    } catch (const json::exception& e) {
        Logger::error("LoopManager", 
                     "Failed to parse layers JSON: " + std::string(e.what()));
        loop.layers = json::array();
    }
    
    loop.createdAt = row.value("created_at", 0);
    loop.lastModified = row.value("last_modified", 0);
    
    return loop;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER LoopManager.cpp
// ============================================================================

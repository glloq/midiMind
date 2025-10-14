// ============================================================================
// Fichier: backend/src/storage/PresetManager.cpp
// Version: 3.0.0
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Implémentation du gestionnaire de presets de routage MIDI.
//   Gère la persistance en base de données SQLite.
//
// Auteur: MidiMind Team
// Date: 2025-10-13
// Statut: ✅ COMPLET
// ============================================================================

#include "PresetManager.h"
#include <sstream>
#include <algorithm>
#include <fstream>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

PresetManager::PresetManager(std::shared_ptr<Database> database)
    : database_(database) {
    
    if (!database_) {
        THROW_ERROR(ErrorCode::NULL_POINTER, "Database pointer is null");
    }
    
    if (!database_->isOpen()) {
        THROW_ERROR(ErrorCode::DATABASE_OPEN_FAILED, "Database is not open");
    }
    
    Logger::info("PresetManager", "═══════════════════════════════════════");
    Logger::info("PresetManager", "  PresetManager v3.0.0");
    Logger::info("PresetManager", "═══════════════════════════════════════");
    
    // Initialiser le schéma
    if (!initializeSchema()) {
        THROW_ERROR(ErrorCode::DATABASE_QUERY_FAILED, "Failed to initialize schema");
    }
    
    Logger::info("PresetManager", "✓ PresetManager initialized");
}

PresetManager::~PresetManager() {
    Logger::info("PresetManager", "PresetManager destroyed");
}

// ============================================================================
// INITIALISATION
// ============================================================================

bool PresetManager::initializeSchema() {
    Logger::info("PresetManager", "Initializing database schema...");
    
    try {
        // Créer la table presets si elle n'existe pas
        std::string sql = R"(
            CREATE TABLE IF NOT EXISTS presets (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                category TEXT DEFAULT '',
                description TEXT DEFAULT '',
                data TEXT NOT NULL,
                entry_count INTEGER DEFAULT 0,
                created_at INTEGER NOT NULL,
                modified_at INTEGER NOT NULL
            )
        )";
        
        database_->execute(sql);
        
        // Créer des index pour améliorer les performances
        database_->execute("CREATE INDEX IF NOT EXISTS idx_presets_name ON presets(name)");
        database_->execute("CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category)");
        database_->execute("CREATE INDEX IF NOT EXISTS idx_presets_modified ON presets(modified_at DESC)");
        
        Logger::info("PresetManager", "✓ Schema initialized");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Schema initialization failed: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// CRUD - CREATE
// ============================================================================

int PresetManager::create(const Preset& preset, 
                          const std::string& category,
                          const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string name = preset.getName();
    if (name.empty()) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR, "Preset name cannot be empty");
    }
    
    Logger::info("PresetManager", "Creating preset: " + name);
    
    try {
        // Sérialiser le preset
        std::string data = serializePreset(preset);
        int entryCount = static_cast<int>(preset.getEntries().size());
        
        // Timestamp actuel
        std::time_t now = std::time(nullptr);
        
        // Insérer en base
        auto result = database_->execute(
            "INSERT INTO presets (name, category, description, data, entry_count, created_at, modified_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)",
            {
                name,
                category,
                description,
                data,
                std::to_string(entryCount),
                std::to_string(now),
                std::to_string(now)
            }
        );
        
        int id = static_cast<int>(result.lastInsertId);
        
        Logger::info("PresetManager", "✓ Preset created (ID: " + std::to_string(id) + 
                    ", entries: " + std::to_string(entryCount) + ")");
        
        return id;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Create failed: " + std::string(e.what()));
        throw;
    }
}

// ============================================================================
// CRUD - READ
// ============================================================================

PresetRecord PresetManager::load(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::debug("PresetManager", "Loading preset ID: " + std::to_string(id));
    
    try {
        auto row = database_->queryOne(
            "SELECT * FROM presets WHERE id = ?",
            {std::to_string(id)}
        );
        
        if (row.empty()) {
            THROW_ERROR(ErrorCode::DATABASE_NOT_FOUND, 
                       "Preset not found: " + std::to_string(id));
        }
        
        // Parser les métadonnées
        PresetRecord record;
        record.metadata = parseMetadata(row);
        
        // Désérialiser le preset
        record.preset = deserializePreset(row.at("data"));
        
        Logger::debug("PresetManager", "✓ Preset loaded: " + record.metadata.name);
        
        return record;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Load failed: " + std::string(e.what()));
        throw;
    }
}

std::optional<PresetMetadata> PresetManager::getMetadata(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto row = database_->queryOne(
            "SELECT id, name, category, description, entry_count, created_at, modified_at "
            "FROM presets WHERE id = ?",
            {std::to_string(id)}
        );
        
        if (row.empty()) {
            return std::nullopt;
        }
        
        return parseMetadata(row);
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "GetMetadata failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<PresetMetadata> PresetManager::list() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::debug("PresetManager", "Listing all presets...");
    
    try {
        auto result = database_->query(
            "SELECT id, name, category, description, entry_count, created_at, modified_at "
            "FROM presets ORDER BY modified_at DESC"
        );
        
        std::vector<PresetMetadata> presets;
        presets.reserve(result.rows.size());
        
        for (const auto& row : result.rows) {
            presets.push_back(parseMetadata(row));
        }
        
        Logger::debug("PresetManager", "✓ Found " + std::to_string(presets.size()) + " presets");
        
        return presets;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "List failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<PresetMetadata> PresetManager::listByCategory(const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::debug("PresetManager", "Listing presets in category: " + category);
    
    try {
        auto result = database_->query(
            "SELECT id, name, category, description, entry_count, created_at, modified_at "
            "FROM presets WHERE category = ? ORDER BY name",
            {category}
        );
        
        std::vector<PresetMetadata> presets;
        presets.reserve(result.rows.size());
        
        for (const auto& row : result.rows) {
            presets.push_back(parseMetadata(row));
        }
        
        return presets;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "ListByCategory failed: " + std::string(e.what()));
        return {};
    }
}

// ============================================================================
// CRUD - UPDATE
// ============================================================================

void PresetManager::update(int id, 
                           const Preset& preset,
                           const std::string& category,
                           const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string name = preset.getName();
    if (name.empty()) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR, "Preset name cannot be empty");
    }
    
    Logger::info("PresetManager", "Updating preset ID: " + std::to_string(id));
    
    try {
        // Vérifier que le preset existe
        if (!exists(id)) {
            THROW_ERROR(ErrorCode::DATABASE_NOT_FOUND, 
                       "Preset not found: " + std::to_string(id));
        }
        
        // Sérialiser
        std::string data = serializePreset(preset);
        int entryCount = static_cast<int>(preset.getEntries().size());
        std::time_t now = std::time(nullptr);
        
        // Mettre à jour
        database_->execute(
            "UPDATE presets SET name = ?, category = ?, description = ?, "
            "data = ?, entry_count = ?, modified_at = ? WHERE id = ?",
            {
                name,
                category,
                description,
                data,
                std::to_string(entryCount),
                std::to_string(now),
                std::to_string(id)
            }
        );
        
        Logger::info("PresetManager", "✓ Preset updated: " + name);
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Update failed: " + std::string(e.what()));
        throw;
    }
}

// ============================================================================
// CRUD - DELETE
// ============================================================================

void PresetManager::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("PresetManager", "Removing preset ID: " + std::to_string(id));
    
    try {
        database_->execute("DELETE FROM presets WHERE id = ?", {std::to_string(id)});
        
        Logger::info("PresetManager", "✓ Preset removed");
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Remove failed: " + std::string(e.what()));
        throw;
    }
}

void PresetManager::removeAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::warn("PresetManager", "Removing ALL presets (DANGER)");
    
    try {
        auto result = database_->execute("DELETE FROM presets");
        
        Logger::warn("PresetManager", "✓ All presets removed (" + 
                    std::to_string(result.affectedRows) + " rows)");
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "RemoveAll failed: " + std::string(e.what()));
        throw;
    }
}

// ============================================================================
// RECHERCHE & FILTRAGE
// ============================================================================

std::vector<PresetMetadata> PresetManager::search(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::debug("PresetManager", "Searching presets: " + query);
    
    if (query.empty()) {
        return list();
    }
    
    try {
        std::string pattern = "%" + query + "%";
        
        auto result = database_->query(
            "SELECT id, name, category, description, entry_count, created_at, modified_at "
            "FROM presets WHERE name LIKE ? OR description LIKE ? "
            "ORDER BY modified_at DESC",
            {pattern, pattern}
        );
        
        std::vector<PresetMetadata> presets;
        presets.reserve(result.rows.size());
        
        for (const auto& row : result.rows) {
            presets.push_back(parseMetadata(row));
        }
        
        Logger::debug("PresetManager", "✓ Found " + std::to_string(presets.size()) + " results");
        
        return presets;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Search failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<std::string> PresetManager::listCategories() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->query(
            "SELECT DISTINCT category FROM presets WHERE category != '' ORDER BY category"
        );
        
        std::vector<std::string> categories;
        categories.reserve(result.rows.size());
        
        for (const auto& row : result.rows) {
            categories.push_back(row.at("category"));
        }
        
        return categories;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "ListCategories failed: " + std::string(e.what()));
        return {};
    }
}

bool PresetManager::exists(int id) {
    // Note: pas de lock ici car appelé depuis d'autres méthodes déjà lockées
    
    try {
        auto count = database_->queryScalar(
            "SELECT COUNT(*) FROM presets WHERE id = ?",
            {std::to_string(id)}
        );
        
        return !count.empty() && std::stoi(count) > 0;
        
    } catch (const std::exception&) {
        return false;
    }
}

// ============================================================================
// IMPORT/EXPORT
// ============================================================================

bool PresetManager::exportToFile(int id, const std::string& filepath) {
    Logger::info("PresetManager", "Exporting preset " + std::to_string(id) + 
                " to: " + filepath);
    
    try {
        // Charger le preset
        auto record = load(id);
        
        // Sauvegarder avec Preset::saveToFile()
        if (!record.preset.saveToFile(filepath)) {
            Logger::error("PresetManager", "Failed to write file: " + filepath);
            return false;
        }
        
        Logger::info("PresetManager", "✓ Preset exported");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Export failed: " + std::string(e.what()));
        return false;
    }
}

int PresetManager::importFromFile(const std::string& filepath, 
                                   const std::string& category) {
    Logger::info("PresetManager", "Importing preset from: " + filepath);
    
    try {
        // Charger avec Preset::loadFromFile()
        Preset preset;
        if (!preset.loadFromFile(filepath)) {
            THROW_ERROR(ErrorCode::FILE_READ_FAILED, "Failed to read file: " + filepath);
        }
        
        // Créer en base
        int id = create(preset, category, "Imported from " + filepath);
        
        Logger::info("PresetManager", "✓ Preset imported (ID: " + std::to_string(id) + ")");
        
        return id;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Import failed: " + std::string(e.what()));
        throw;
    }
}

// ============================================================================
// STATISTIQUES
// ============================================================================

int PresetManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->queryScalar("SELECT COUNT(*) FROM presets");
        
        if (result.empty()) {
            return 0;
        }
        
        return std::stoi(result);
        
    } catch (const std::exception&) {
        return 0;
    }
}

json PresetManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    
    try {
        // Nombre total
        auto totalStr = database_->queryScalar("SELECT COUNT(*) FROM presets");
        int total = totalStr.empty() ? 0 : std::stoi(totalStr);
        stats["total_presets"] = total;
        
        // Catégories
        auto categoriesResult = database_->query(
            "SELECT DISTINCT category FROM presets WHERE category != '' ORDER BY category"
        );
        
        std::vector<std::string> categories;
        for (const auto& row : categoriesResult.rows) {
            categories.push_back(row.at("category"));
        }
        stats["categories"] = categories;
        
        // Nombre total d'entrées
        auto entriesStr = database_->queryScalar("SELECT SUM(entry_count) FROM presets");
        int totalEntries = entriesStr.empty() ? 0 : std::stoi(entriesStr);
        stats["total_entries"] = totalEntries;
        
        // Moyenne d'entrées par preset
        if (total > 0) {
            stats["average_entries_per_preset"] = static_cast<double>(totalEntries) / total;
        } else {
            stats["average_entries_per_preset"] = 0.0;
        }
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "GetStatistics failed: " + std::string(e.what()));
    }
    
    return stats;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

std::string PresetManager::serializePreset(const Preset& preset) const {
    json j;
    j["name"] = preset.getName();
    j["entries"] = json::array();
    
    for (const auto& entry : preset.getEntries()) {
        json e;
        e["channel"] = entry.channel;
        e["file_id"] = entry.fileId;
        e["device_name"] = entry.deviceName;
        e["offset_ms"] = entry.offsetMs;
        e["muted"] = entry.muted;
        e["solo"] = entry.solo;
        e["volume"] = entry.volume;
        j["entries"].push_back(e);
    }
    
    return j.dump();
}

Preset PresetManager::deserializePreset(const std::string& data) const {
    try {
        json j = json::parse(data);
        
        Preset preset;
        preset.setName(j.value("name", "Unnamed"));
        
        if (j.contains("entries") && j["entries"].is_array()) {
            for (const auto& e : j["entries"]) {
                preset.addEntry(
                    e.value("channel", 0),
                    e.value("file_id", ""),
                    e.value("device_name", ""),
                    e.value("offset_ms", 0),
                    e.value("muted", false),
                    e.value("solo", false),
                    e.value("volume", 1.0f)
                );
            }
        }
        
        return preset;
        
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR, 
                   "Failed to deserialize preset: " + std::string(e.what()));
    }
}

PresetMetadata PresetManager::parseMetadata(const DatabaseRow& row) const {
    PresetMetadata meta;
    
    try {
        meta.id = std::stoi(row.at("id"));
        meta.name = row.at("name");
        meta.category = row.at("category");
        meta.description = row.at("description");
        meta.entryCount = std::stoi(row.at("entry_count"));
        meta.createdAt = std::stoll(row.at("created_at"));
        meta.modifiedAt = std::stoll(row.at("modified_at"));
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Failed to parse metadata: " + std::string(e.what()));
    }
    
    return meta;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER PresetManager.cpp
// ============================================================================

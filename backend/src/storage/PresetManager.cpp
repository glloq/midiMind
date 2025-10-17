// ============================================================================
// File: backend/src/storage/PresetManager.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Complete implementation of PresetManager
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Complete CRUD operations
//   - Enhanced search functionality
//   - Import/export JSON files
//   - Statistics tracking
//
// ============================================================================

#include "PresetManager.h"
#include "../core/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace midiMind {

// ============================================================================
// PRESETENTRY METHODS
// ============================================================================

json PresetEntry::toJson() const {
    return json{
        {"channel", channel},
        {"file_id", fileId},
        {"device_id", deviceId},
        {"device_name", deviceName},
        {"offset_ms", offsetMs},
        {"muted", muted},
        {"solo", solo},
        {"volume", volume}
    };
}

PresetEntry PresetEntry::fromJson(const json& j) {
    PresetEntry entry;
    entry.channel = j.value("channel", 0);
    entry.fileId = j.value("file_id", "");
    entry.deviceId = j.value("device_id", "");
    entry.deviceName = j.value("device_name", "");
    entry.offsetMs = j.value("offset_ms", 0);
    entry.muted = j.value("muted", false);
    entry.solo = j.value("solo", false);
    entry.volume = j.value("volume", 1.0f);
    return entry;
}

// ============================================================================
// PRESETMETADATA METHODS
// ============================================================================

json PresetMetadata::toJson() const {
    return json{
        {"id", id},
        {"name", name},
        {"category", category},
        {"description", description},
        {"entry_count", entryCount},
        {"created_at", static_cast<int64_t>(createdAt)},
        {"modified_at", static_cast<int64_t>(modifiedAt)}
    };
}

PresetMetadata PresetMetadata::fromJson(const json& j) {
    PresetMetadata meta;
    meta.id = j.value("id", 0);
    meta.name = j.value("name", "");
    meta.category = j.value("category", "");
    meta.description = j.value("description", "");
    meta.entryCount = j.value("entry_count", 0);
    meta.createdAt = j.value("created_at", 0);
    meta.modifiedAt = j.value("modified_at", 0);
    return meta;
}

// ============================================================================
// PRESET METHODS
// ============================================================================

void Preset::addEntry(const PresetEntry& entry) {
    entries.push_back(entry);
    metadata.entryCount = static_cast<int>(entries.size());
}

bool Preset::removeEntry(size_t index) {
    if (index >= entries.size()) {
        return false;
    }
    
    entries.erase(entries.begin() + index);
    metadata.entryCount = static_cast<int>(entries.size());
    return true;
}

json Preset::toJson() const {
    json j = metadata.toJson();
    
    j["entries"] = json::array();
    for (const auto& entry : entries) {
        j["entries"].push_back(entry.toJson());
    }
    
    return j;
}

Preset Preset::fromJson(const json& j) {
    Preset preset;
    preset.metadata = PresetMetadata::fromJson(j);
    
    if (j.contains("entries") && j["entries"].is_array()) {
        for (const auto& entryJson : j["entries"]) {
            preset.entries.push_back(PresetEntry::fromJson(entryJson));
        }
        preset.metadata.entryCount = static_cast<int>(preset.entries.size());
    }
    
    return preset;
}

// ============================================================================
// PRESETMANAGER - CONSTRUCTOR / DESTRUCTOR
// ============================================================================

PresetManager::PresetManager(std::shared_ptr<Database> database)
    : database_(database)
{
    if (!database_ || !database_->isConnected()) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_CONNECTED, 
                   "Database must be opened before creating PresetManager");
    }
    
    Logger::info("PresetManager", "========================================");
    Logger::info("PresetManager", "  Initializing PresetManager");
    Logger::info("PresetManager", "========================================");
    
    initializeSchema();
    
    Logger::info("PresetManager", "✓ PresetManager initialized");
}

PresetManager::~PresetManager() {
    Logger::info("PresetManager", "PresetManager destroyed");
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool PresetManager::initializeSchema() {
    Logger::info("PresetManager", "Initializing database schema...");
    
    try {
        // Create presets table
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
        
        // Create indexes for performance
        database_->execute("CREATE INDEX IF NOT EXISTS idx_presets_name "
                          "ON presets(name)");
        database_->execute("CREATE INDEX IF NOT EXISTS idx_presets_category "
                          "ON presets(category)");
        database_->execute("CREATE INDEX IF NOT EXISTS idx_presets_modified "
                          "ON presets(modified_at DESC)");
        
        Logger::info("PresetManager", "✓ Schema initialized");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "Schema initialization failed: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// CRUD - CREATE
// ============================================================================

int PresetManager::create(const Preset& preset) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (preset.metadata.name.empty()) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR, "Preset name cannot be empty");
    }
    
    Logger::info("PresetManager", "Creating preset: " + preset.metadata.name);
    
    try {
        // Serialize preset
        std::string data = serializePreset(preset);
        int entryCount = static_cast<int>(preset.entries.size());
        
        // Current timestamp
        std::time_t now = std::time(nullptr);
        
        // Insert into database
        auto result = database_->execute(
            "INSERT INTO presets (name, category, description, data, "
            "entry_count, created_at, modified_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)",
            {
                preset.metadata.name,
                preset.metadata.category,
                preset.metadata.description,
                data,
                std::to_string(entryCount),
                std::to_string(now),
                std::to_string(now)
            }
        );
        
        int id = static_cast<int>(result.lastInsertId);
        
        Logger::info("PresetManager", 
                    "✓ Preset created (ID: " + std::to_string(id) + 
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

std::optional<Preset> PresetManager::load(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::debug("PresetManager", "Loading preset ID: " + std::to_string(id));
    
    try {
        auto result = database_->query(
            "SELECT * FROM presets WHERE id = ?",
            {std::to_string(id)}
        );
        
        if (!result.success || result.rows.empty()) {
            Logger::warning("PresetManager", "Preset not found: " + std::to_string(id));
            return std::nullopt;
        }
        
        const auto& row = result.rows[0];
        
        // Parse metadata
        Preset preset;
        preset.metadata = parseMetadata(row);
        
        // Deserialize data
        if (row.find("data") != row.end()) {
            Preset deserialized = deserializePreset(row.at("data"));
            preset.entries = deserialized.entries;
        }
        
        Logger::debug("PresetManager", "✓ Preset loaded: " + preset.metadata.name);
        
        return preset;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Load failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<PresetMetadata> PresetManager::getMetadata(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->query(
            "SELECT id, name, category, description, entry_count, "
            "created_at, modified_at FROM presets WHERE id = ?",
            {std::to_string(id)}
        );
        
        if (!result.success || result.rows.empty()) {
            return std::nullopt;
        }
        
        return parseMetadata(result.rows[0]);
        
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<PresetMetadata> PresetManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->query(
            "SELECT id, name, category, description, entry_count, "
            "created_at, modified_at FROM presets "
            "ORDER BY modified_at DESC"
        );
        
        std::vector<PresetMetadata> presets;
        presets.reserve(result.rows.size());
        
        for (const auto& row : result.rows) {
            presets.push_back(parseMetadata(row));
        }
        
        return presets;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "List failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<PresetMetadata> PresetManager::listByCategory(
    const std::string& category) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->query(
            "SELECT id, name, category, description, entry_count, "
            "created_at, modified_at FROM presets "
            "WHERE category = ? ORDER BY name",
            {category}
        );
        
        std::vector<PresetMetadata> presets;
        presets.reserve(result.rows.size());
        
        for (const auto& row : result.rows) {
            presets.push_back(parseMetadata(row));
        }
        
        return presets;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "ListByCategory failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<PresetMetadata> PresetManager::search(
    const std::string& query) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::string searchQuery = "%" + query + "%";
        
        auto result = database_->query(
            "SELECT id, name, category, description, entry_count, "
            "created_at, modified_at FROM presets "
            "WHERE name LIKE ? OR description LIKE ? "
            "ORDER BY name",
            {searchQuery, searchQuery}
        );
        
        std::vector<PresetMetadata> presets;
        presets.reserve(result.rows.size());
        
        for (const auto& row : result.rows) {
            presets.push_back(parseMetadata(row));
        }
        
        Logger::debug("PresetManager", 
                     "Search '" + query + "' found " + 
                     std::to_string(presets.size()) + " results");
        
        return presets;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Search failed: " + std::string(e.what()));
        return {};
    }
}

std::vector<std::string> PresetManager::getCategories() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->query(
            "SELECT DISTINCT category FROM presets "
            "WHERE category != '' ORDER BY category"
        );
        
        std::vector<std::string> categories;
        categories.reserve(result.rows.size());
        
        for (const auto& row : result.rows) {
            if (row.find("category") != row.end()) {
                categories.push_back(row.at("category"));
            }
        }
        
        return categories;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "GetCategories failed: " + std::string(e.what()));
        return {};
    }
}

// ============================================================================
// CRUD - UPDATE
// ============================================================================

void PresetManager::update(int id, const Preset& preset) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (preset.metadata.name.empty()) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR, "Preset name cannot be empty");
    }
    
    Logger::info("PresetManager", "Updating preset ID: " + std::to_string(id));
    
    try {
        // Check if preset exists
        if (!exists(id)) {
            THROW_ERROR(ErrorCode::DATABASE_NOT_FOUND, 
                       "Preset not found: " + std::to_string(id));
        }
        
        // Serialize
        std::string data = serializePreset(preset);
        int entryCount = static_cast<int>(preset.entries.size());
        std::time_t now = std::time(nullptr);
        
        // Update
        database_->execute(
            "UPDATE presets SET name = ?, category = ?, description = ?, "
            "data = ?, entry_count = ?, modified_at = ? WHERE id = ?",
            {
                preset.metadata.name,
                preset.metadata.category,
                preset.metadata.description,
                data,
                std::to_string(entryCount),
                std::to_string(now),
                std::to_string(id)
            }
        );
        
        Logger::info("PresetManager", "✓ Preset updated: " + preset.metadata.name);
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Update failed: " + std::string(e.what()));
        throw;
    }
}

// ============================================================================
// CRUD - DELETE
// ============================================================================

bool PresetManager::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("PresetManager", "Removing preset ID: " + std::to_string(id));
    
    try {
        database_->execute("DELETE FROM presets WHERE id = ?",
                          {std::to_string(id)});
        
        Logger::info("PresetManager", "✓ Preset removed");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Remove failed: " + std::string(e.what()));
        return false;
    }
}

bool PresetManager::exists(int id) const {
    try {
        auto result = database_->query(
            "SELECT COUNT(*) as count FROM presets WHERE id = ?",
            {std::to_string(id)}
        );
        
        if (!result.success || result.rows.empty()) {
            return false;
        }
        
        int count = std::stoi(result.rows[0].at("count"));
        return count > 0;
        
    } catch (const std::exception&) {
        return false;
    }
}

// ============================================================================
// IMPORT / EXPORT
// ============================================================================

bool PresetManager::exportToFile(int id, const std::string& filepath) {
    Logger::info("PresetManager", 
                "Exporting preset " + std::to_string(id) + " to: " + filepath);
    
    try {
        // Load preset
        auto preset = load(id);
        if (!preset.has_value()) {
            Logger::error("PresetManager", "Preset not found: " + std::to_string(id));
            return false;
        }
        
        // Convert to JSON
        json j = preset->toJson();
        
        // Write to file
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::error("PresetManager", "Failed to open file: " + filepath);
            return false;
        }
        
        file << j.dump(2);
        file.close();
        
        Logger::info("PresetManager", "✓ Preset exported");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Export failed: " + std::string(e.what()));
        return false;
    }
}

int PresetManager::importFromFile(const std::string& filepath) {
    Logger::info("PresetManager", "Importing preset from: " + filepath);
    
    try {
        // Read file
        std::ifstream file(filepath);
        if (!file.is_open()) {
            THROW_ERROR(ErrorCode::FILE_NOT_FOUND, 
                       "Failed to open file: " + filepath);
        }
        
        json j;
        file >> j;
        file.close();
        
        // Parse preset
        Preset preset = Preset::fromJson(j);
        
        // Create in database
        int id = create(preset);
        
        Logger::info("PresetManager", 
                    "✓ Preset imported (ID: " + std::to_string(id) + ")");
        
        return id;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Import failed: " + std::string(e.what()));
        return -1;
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

int PresetManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto result = database_->query("SELECT COUNT(*) as count FROM presets");
        
        if (!result.success || result.rows.empty()) {
            return 0;
        }
        
        return std::stoi(result.rows[0].at("count"));
        
    } catch (const std::exception&) {
        return 0;
    }
}

json PresetManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    
    try {
        // Total count
        auto countResult = database_->query("SELECT COUNT(*) as count FROM presets");
        int total = 0;
        if (countResult.success && !countResult.rows.empty()) {
            total = std::stoi(countResult.rows[0].at("count"));
        }
        stats["total"] = total;
        
        // Count by category
        auto categoryResult = database_->query(
            "SELECT category, COUNT(*) as count FROM presets "
            "GROUP BY category ORDER BY count DESC"
        );
        
        json categories = json::array();
        for (const auto& row : categoryResult.rows) {
            categories.push_back({
                {"category", row.at("category")},
                {"count", std::stoi(row.at("count"))}
            });
        }
        stats["by_category"] = categories;
        
        // Total entries
        auto entriesResult = database_->query(
            "SELECT SUM(entry_count) as total FROM presets"
        );
        int totalEntries = 0;
        if (entriesResult.success && !entriesResult.rows.empty()) {
            totalEntries = std::stoi(entriesResult.rows[0].at("total"));
        }
        stats["total_entries"] = totalEntries;
        
        // Average entries per preset
        if (total > 0) {
            stats["avg_entries"] = static_cast<double>(totalEntries) / total;
        } else {
            stats["avg_entries"] = 0.0;
        }
        
        return stats;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "GetStatistics failed: " + std::string(e.what()));
        return json{
            {"total", 0},
            {"by_category", json::array()},
            {"total_entries", 0},
            {"avg_entries", 0.0}
        };
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::string PresetManager::serializePreset(const Preset& preset) const {
    json j = preset.toJson();
    return j.dump();
}

Preset PresetManager::deserializePreset(const std::string& data) const {
    try {
        json j = json::parse(data);
        return Preset::fromJson(j);
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "Deserialization failed: " + std::string(e.what()));
        return Preset();
    }
}

PresetMetadata PresetManager::parseMetadata(
    const std::map<std::string, std::string>& row) const
{
    PresetMetadata meta;
    
    try {
        if (row.find("id") != row.end()) {
            meta.id = std::stoi(row.at("id"));
        }
        if (row.find("name") != row.end()) {
            meta.name = row.at("name");
        }
        if (row.find("category") != row.end()) {
            meta.category = row.at("category");
        }
        if (row.find("description") != row.end()) {
            meta.description = row.at("description");
        }
        if (row.find("entry_count") != row.end()) {
            meta.entryCount = std::stoi(row.at("entry_count"));
        }
        if (row.find("created_at") != row.end()) {
            meta.createdAt = std::stoll(row.at("created_at"));
        }
        if (row.find("modified_at") != row.end()) {
            meta.modifiedAt = std::stoll(row.at("modified_at"));
        }
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "Error parsing metadata: " + std::string(e.what()));
    }
    
    return meta;
}

} // namespace midiMind

// ============================================================================
// END OF FILE PresetManager.cpp v4.1.0
// ============================================================================
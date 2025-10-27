// ============================================================================
// File: backend/src/storage/PresetManager.cpp
// Version: 4.2.0 - THREAD-SAFE
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.0:
//   - Added getEntryCount() and clear() implementations
//   - Wrapped all std::stoi/stoull in try-catch
//   - Improved error handling throughout
//   - Better exception messages
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
    try {
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
    } catch (const json::exception& e) {
        throw std::runtime_error("Failed to serialize PresetEntry: " + std::string(e.what()));
    }
}

PresetEntry PresetEntry::fromJson(const json& j) {
    try {
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
    } catch (const json::exception& e) {
        throw std::runtime_error("Failed to deserialize PresetEntry: " + std::string(e.what()));
    }
}

// ============================================================================
// PRESETMETADATA METHODS
// ============================================================================

json PresetMetadata::toJson() const {
    try {
        return json{
            {"id", id},
            {"name", name},
            {"category", category},
            {"description", description},
            {"entry_count", entryCount},
            {"created_at", static_cast<int64_t>(createdAt)},
            {"modified_at", static_cast<int64_t>(modifiedAt)}
        };
    } catch (const json::exception& e) {
        throw std::runtime_error("Failed to serialize PresetMetadata: " + std::string(e.what()));
    }
}

PresetMetadata PresetMetadata::fromJson(const json& j) {
    try {
        PresetMetadata meta;
        meta.id = j.value("id", 0);
        meta.name = j.value("name", "");
        meta.category = j.value("category", "");
        meta.description = j.value("description", "");
        meta.entryCount = j.value("entry_count", 0);
        meta.createdAt = j.value("created_at", 0);
        meta.modifiedAt = j.value("modified_at", 0);
        return meta;
    } catch (const json::exception& e) {
        throw std::runtime_error("Failed to deserialize PresetMetadata: " + std::string(e.what()));
    }
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

size_t Preset::getEntryCount() const {
    return entries.size();
}

void Preset::clear() {
    entries.clear();
    metadata.entryCount = 0;
}

json Preset::toJson() const {
    try {
        json j = metadata.toJson();
        
        j["entries"] = json::array();
        for (const auto& entry : entries) {
            j["entries"].push_back(entry.toJson());
        }
        
        return j;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to serialize Preset: " + std::string(e.what()));
    }
}

Preset Preset::fromJson(const json& j) {
    try {
        Preset preset;
        preset.metadata = PresetMetadata::fromJson(j);
        
        if (j.contains("entries") && j["entries"].is_array()) {
            for (const auto& entryJson : j["entries"]) {
                preset.entries.push_back(PresetEntry::fromJson(entryJson));
            }
            preset.metadata.entryCount = static_cast<int>(preset.entries.size());
        }
        
        return preset;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to deserialize Preset: " + std::string(e.what()));
    }
}

// ============================================================================
// PRESETMANAGER - CONSTRUCTOR / DESTRUCTOR
// ============================================================================

PresetManager::PresetManager(Database& database)
    : database_(database)
{
    if (!database_.isConnected()) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_CONNECTED, 
                   "Database must be opened before creating PresetManager");
    }
    
    Logger::info("PresetManager", "========================================");
    Logger::info("PresetManager", "  Initializing PresetManager");
    Logger::info("PresetManager", "========================================");
    
    initializeSchema();
    
    Logger::info("PresetManager", "âœ“ PresetManager initialized");
}

PresetManager::~PresetManager() {
    Logger::info("PresetManager", "PresetManager destroyed");
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool PresetManager::initializeSchema() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("PresetManager", "Initializing database schema...");
    
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS presets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            category TEXT DEFAULT '',
            description TEXT DEFAULT '',
            data TEXT NOT NULL,
            entry_count INTEGER DEFAULT 0,
            created_at INTEGER NOT NULL,
            modified_at INTEGER NOT NULL
        );
        
        CREATE INDEX IF NOT EXISTS idx_presets_name ON presets(name);
        CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category);
    )";
    
    try {
        database_.execute(sql);
        Logger::info("PresetManager", "âœ“ Schema initialized");
        return true;
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "Failed to initialize schema: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// CRUD - CREATE
// ============================================================================

int PresetManager::create(const Preset& preset) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (preset.metadata.name.empty()) {
        THROW_ERROR(ErrorCode::INVALID_ARGUMENT, "Preset name cannot be empty");
    }
    
    Logger::info("PresetManager", "Creating preset: " + preset.metadata.name);
    
    std::string data = serializePreset(preset);
    std::time_t now = std::time(nullptr);
    
    const std::string sql = R"(
        INSERT INTO presets (name, category, description, data, entry_count, created_at, modified_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    auto result = database_.execute(sql, {
        preset.metadata.name,
        preset.metadata.category,
        preset.metadata.description,
        data,
        std::to_string(preset.entries.size()),
        std::to_string(now),
        std::to_string(now)
    });
    
    if (!result.success) {
        THROW_ERROR(ErrorCode::DATABASE_ERROR, 
                   "Failed to create preset: " + result.error);
    }
    
    int id = static_cast<int>(result.lastInsertId);
    Logger::info("PresetManager", "âœ“ Preset created with ID: " + std::to_string(id));
    
    return id;
}

// ============================================================================
// CRUD - READ
// ============================================================================

std::optional<Preset> PresetManager::load(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT * FROM presets WHERE id = ?";
    
    auto result = database_.query(sql, {std::to_string(id)});
    
    if (result.rows.empty()) {
        return std::nullopt;
    }
    
    try {
        const auto& row = result.rows[0];
        Preset preset = deserializePreset(row.at("data"));
        preset.metadata = parseMetadata(row);
        return preset;
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "Failed to load preset: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<PresetMetadata> PresetManager::getMetadata(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT * FROM presets WHERE id = ?";
    
    auto result = database_.query(sql, {std::to_string(id)});
    
    if (result.rows.empty()) {
        return std::nullopt;
    }
    
    try {
        return parseMetadata(result.rows[0]);
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "Failed to get metadata: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<PresetMetadata> PresetManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT * FROM presets ORDER BY modified_at DESC";
    
    auto result = database_.query(sql);
    
    std::vector<PresetMetadata> presets;
    for (const auto& row : result.rows) {
        try {
            presets.push_back(parseMetadata(row));
        } catch (const std::exception& e) {
            Logger::warning("PresetManager", 
                          "Skipping invalid preset: " + std::string(e.what()));
        }
    }
    
    return presets;
}

std::vector<PresetMetadata> PresetManager::listByCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT * FROM presets WHERE category = ? ORDER BY modified_at DESC";
    
    auto result = database_.query(sql, {category});
    
    std::vector<PresetMetadata> presets;
    for (const auto& row : result.rows) {
        try {
            presets.push_back(parseMetadata(row));
        } catch (const std::exception& e) {
            Logger::warning("PresetManager", 
                          "Skipping invalid preset: " + std::string(e.what()));
        }
    }
    
    return presets;
}

std::vector<PresetMetadata> PresetManager::search(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = R"(
        SELECT * FROM presets 
        WHERE name LIKE ? OR description LIKE ? 
        ORDER BY modified_at DESC
    )";
    
    std::string pattern = "%" + query + "%";
    
    auto result = database_.query(sql, {pattern, pattern});
    
    std::vector<PresetMetadata> presets;
    for (const auto& row : result.rows) {
        try {
            presets.push_back(parseMetadata(row));
        } catch (const std::exception& e) {
            Logger::warning("PresetManager", 
                          "Skipping invalid preset: " + std::string(e.what()));
        }
    }
    
    return presets;
}

std::vector<std::string> PresetManager::getCategories() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT DISTINCT category FROM presets WHERE category != '' ORDER BY category";
    
    auto result = database_.query(sql);
    
    std::vector<std::string> categories;
    for (const auto& row : result.rows) {
        try {
            categories.push_back(row.at("category"));
        } catch (const std::exception& e) {
            Logger::warning("PresetManager", 
                          "Invalid category row: " + std::string(e.what()));
        }
    }
    
    return categories;
}

// ============================================================================
// CRUD - UPDATE
// ============================================================================

void PresetManager::update(int id, const Preset& preset) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!exists(id)) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND, "Preset not found: " + std::to_string(id));
    }
    
    Logger::info("PresetManager", "Updating preset: " + std::to_string(id));
    
    std::string data = serializePreset(preset);
    std::time_t now = std::time(nullptr);
    
    const std::string sql = R"(
        UPDATE presets 
        SET name = ?, category = ?, description = ?, data = ?, 
            entry_count = ?, modified_at = ?
        WHERE id = ?
    )";
    
    auto result = database_.execute(sql, {
        preset.metadata.name,
        preset.metadata.category,
        preset.metadata.description,
        data,
        std::to_string(preset.entries.size()),
        std::to_string(now),
        std::to_string(id)
    });
    
    if (!result.success) {
        THROW_ERROR(ErrorCode::DATABASE_ERROR, 
                   "Failed to update preset: " + result.error);
    }
    
    Logger::info("PresetManager", "âœ“ Preset updated");
}

// ============================================================================
// CRUD - DELETE
// ============================================================================

bool PresetManager::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("PresetManager", "Deleting preset: " + std::to_string(id));
    
    const std::string sql = "DELETE FROM presets WHERE id = ?";
    
    auto result = database_.execute(sql, {std::to_string(id)});
    
    return result.affectedRows > 0;
}

bool PresetManager::exists(int id) const {
    const std::string sql = "SELECT COUNT(*) as count FROM presets WHERE id = ?";
    
    std::string countStr = database_.queryScalar(sql, {std::to_string(id)});
    
    if (countStr.empty()) {
        return false;
    }
    
    try {
        return std::stoi(countStr) > 0;
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Invalid count value: " + countStr);
        return false;
    }
}

// ============================================================================
// IMPORT / EXPORT
// ============================================================================

bool PresetManager::exportToFile(int id, const std::string& filepath) {
    auto preset = load(id);
    
    if (!preset) {
        Logger::error("PresetManager", "Preset not found for export: " + std::to_string(id));
        return false;
    }
    
    try {
        json j = preset->toJson();
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::error("PresetManager", "Cannot open file for writing: " + filepath);
            return false;
        }
        
        file << j.dump(2);
        
        if (!file.good()) {
            Logger::error("PresetManager", "Write error: " + filepath);
            return false;
        }
        
        file.close();
        
        Logger::info("PresetManager", "âœ“ Preset exported to: " + filepath);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "Failed to export preset: " + std::string(e.what()));
        return false;
    }
}

int PresetManager::importFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::error("PresetManager", "Cannot open file for reading: " + filepath);
            return -1;
        }
        
        json j;
        file >> j;
        
        if (!file.good() && !file.eof()) {
            Logger::error("PresetManager", "Read error: " + filepath);
            return -1;
        }
        
        file.close();
        
        Preset preset = Preset::fromJson(j);
        
        // Reset ID for new preset
        preset.metadata.id = 0;
        preset.metadata.createdAt = 0;
        preset.metadata.modifiedAt = 0;
        
        int id = create(preset);
        
        Logger::info("PresetManager", "âœ“ Preset imported from: " + filepath);
        return id;
        
    } catch (const std::exception& e) {
        Logger::error("PresetManager", 
                     "Failed to import preset: " + std::string(e.what()));
        return -1;
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

int PresetManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT COUNT(*) as count FROM presets";
    
    std::string countStr = database_.queryScalar(sql);
    
    if (countStr.empty()) {
        return 0;
    }
    
    try {
        return std::stoi(countStr);
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Invalid count value: " + countStr);
        return 0;
    }
}

json PresetManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        return json{
            {"total_presets", count()},
            {"categories", getCategories().size()}
        };
    } catch (const std::exception& e) {
        Logger::error("PresetManager", "Failed to get statistics: " + std::string(e.what()));
        return json{
            {"total_presets", 0},
            {"categories", 0},
            {"error", e.what()}
        };
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

std::string PresetManager::serializePreset(const Preset& preset) const {
    try {
        return preset.toJson().dump();
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::VALIDATION_ERROR,
                   "Failed to serialize preset: " + std::string(e.what()));
    }
}

Preset PresetManager::deserializePreset(const std::string& data) const {
    try {
        json j = json::parse(data);
        return Preset::fromJson(j);
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::CONFIG_PARSE_ERROR, 
                   "Failed to deserialize preset: " + std::string(e.what()));
    }
}

PresetMetadata PresetManager::parseMetadata(const std::map<std::string, std::string>& row) const {
    PresetMetadata meta;
    
    try {
        // Required fields
        if (row.count("id") == 0 || row.count("name") == 0) {
            throw std::runtime_error("Missing required fields");
        }
        
        meta.id = std::stoi(row.at("id"));
        meta.name = row.at("name");
        meta.category = row.count("category") ? row.at("category") : "";
        meta.description = row.count("description") ? row.at("description") : "";
        
        if (row.count("entry_count")) {
            meta.entryCount = std::stoi(row.at("entry_count"));
        }
        
        if (row.count("created_at")) {
            meta.createdAt = std::stoull(row.at("created_at"));
        }
        
        if (row.count("modified_at")) {
            meta.modifiedAt = std::stoull(row.at("modified_at"));
        }
        
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("Missing field in row: " + std::string(e.what()));
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Invalid numeric value: " + std::string(e.what()));
    }
    
    return meta;
}

} // namespace midiMind

// ============================================================================
// END OF FILE PresetManager.cpp v4.2.0
// ============================================================================
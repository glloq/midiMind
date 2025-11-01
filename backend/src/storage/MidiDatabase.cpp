// ============================================================================
// File: backend/src/storage/MidiDatabase.cpp
// Version: 4.2.1
// ============================================================================

#include "MidiDatabase.h"
#include "../core/Logger.h"
#include <chrono>

namespace midiMind {

// ============================================================================
// METADATA IMPLEMENTATION
// ============================================================================

json MidiFileMetadata::toJson() const {
    return {
        {"id", id},
        {"filename", filename},
        {"original_filepath", originalFilepath},
        {"duration_ms", durationMs},
        {"track_count", trackCount},
        {"event_count", eventCount},
        {"created_at", static_cast<int64_t>(createdAt)},
        {"modified_at", static_cast<int64_t>(modifiedAt)}
    };
}

MidiFileMetadata MidiFileMetadata::fromJson(const json& j) {
    MidiFileMetadata meta;
    meta.id = j.value("id", 0);
    meta.filename = j.value("filename", "");
    meta.originalFilepath = j.value("original_filepath", "");
    meta.durationMs = j.value("duration_ms", 0);
    meta.trackCount = j.value("track_count", 0);
    meta.eventCount = j.value("event_count", 0);
    meta.createdAt = j.value("created_at", 0);
    meta.modifiedAt = j.value("modified_at", 0);
    return meta;
}

// ============================================================================
// ROUTING IMPLEMENTATION
// ============================================================================

json MidiInstrumentRouting::toJson() const {
    return {
        {"id", id},
        {"midi_file_id", midiFileId},
        {"track_id", trackId},
        {"instrument_name", instrumentName},
        {"device_id", deviceId},
        {"channel", channel},
        {"enabled", enabled},
        {"created_at", static_cast<int64_t>(createdAt)}
    };
}

MidiInstrumentRouting MidiInstrumentRouting::fromJson(const json& j) {
    MidiInstrumentRouting routing;
    routing.id = j.value("id", 0);
    routing.midiFileId = j.value("midi_file_id", 0);
    routing.trackId = j.value("track_id", 0);
    routing.instrumentName = j.value("instrument_name", "");
    routing.deviceId = j.value("device_id", "");
    routing.channel = j.value("channel", 0);
    routing.enabled = j.value("enabled", true);
    routing.createdAt = j.value("created_at", 0);
    return routing;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

MidiDatabase::MidiDatabase(Database& database)
    : database_(database)
{
    if (!database_.isConnected()) {
        THROW_ERROR(ErrorCode::DATABASE_NOT_CONNECTED,
                   "Database must be connected");
    }
    
    Logger::info("MidiDatabase", "Initializing MidiDatabase...");
    initializeSchema();
    Logger::info("MidiDatabase", "✓ MidiDatabase initialized");
}

MidiDatabase::~MidiDatabase() {
    Logger::info("MidiDatabase", "MidiDatabase destroyed");
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool MidiDatabase::initializeSchema() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // La migration 006 sera exécutée automatiquement
    // par le système de migration lors du démarrage
    
    Logger::info("MidiDatabase", "✓ Schema ready");
    return true;
}

// ============================================================================
// CRUD - CREATE
// ============================================================================

int MidiDatabase::save(const std::string& filename, const json& midiJson) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiDatabase", "Saving MIDI JSON: " + filename);
    
    // Extraire métadonnées du JSON
    uint32_t durationMs = midiJson["metadata"].value("duration", 0);
    uint16_t trackCount = midiJson.value("tracks", json::array()).size();
    uint32_t eventCount = midiJson.value("timeline", json::array()).size();
    
    std::string midiJsonStr = midiJson.dump();
    json metadataExtract = midiJson["metadata"];
    std::string metadataStr = metadataExtract.dump();
    
    std::time_t now = std::time(nullptr);
    
    // Vérifier si existe déjà
    const std::string checkSql = "SELECT id FROM midi_files WHERE filename = ?";
    auto checkResult = database_.query(checkSql, {filename});
    
    if (!checkResult.rows.empty()) {
        // Update
        int existingId = std::stoi(checkResult.rows[0].at("id"));
        
        const std::string updateSql = R"(
            UPDATE midi_files 
            SET midi_json = ?, metadata = ?, duration_ms = ?, 
                track_count = ?, event_count = ?, modified_at = ?
            WHERE id = ?
        )";
        
        auto result = database_.execute(updateSql, {
            midiJsonStr,
            metadataStr,
            std::to_string(durationMs),
            std::to_string(trackCount),
            std::to_string(eventCount),
            std::to_string(now),
            std::to_string(existingId)
        });
        
        if (!result.success) {
            THROW_ERROR(ErrorCode::DATABASE_ERROR,
                       "Failed to update MIDI file: " + result.error);
        }
        
        Logger::info("MidiDatabase", "✓ MIDI JSON updated (ID: " + std::to_string(existingId) + ")");
        return existingId;
    } else {
        // Insert
        const std::string insertSql = R"(
            INSERT INTO midi_files 
            (filename, midi_json, metadata, duration_ms, track_count, event_count, created_at, modified_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        auto result = database_.execute(insertSql, {
            filename,
            midiJsonStr,
            metadataStr,
            std::to_string(durationMs),
            std::to_string(trackCount),
            std::to_string(eventCount),
            std::to_string(now),
            std::to_string(now)
        });
        
        if (!result.success) {
            THROW_ERROR(ErrorCode::DATABASE_ERROR,
                       "Failed to save MIDI file: " + result.error);
        }
        
        int id = static_cast<int>(result.lastInsertId);
        Logger::info("MidiDatabase", "✓ MIDI JSON saved (ID: " + std::to_string(id) + ")");
        return id;
    }
}

// ============================================================================
// CRUD - READ
// ============================================================================

std::optional<MidiFileData> MidiDatabase::load(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT * FROM midi_files WHERE id = ?";
    auto result = database_.query(sql, {std::to_string(id)});
    
    if (result.rows.empty()) {
        return std::nullopt;
    }
    
    try {
        MidiFileData data;
        data.metadata = parseMetadata(result.rows[0]);
        data.midiJson = json::parse(result.rows[0].at("midi_json"));
        data.routings = getRoutings(id);
        return data;
    } catch (const std::exception& e) {
        Logger::error("MidiDatabase", 
                     "Failed to load MIDI file: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<MidiFileData> MidiDatabase::loadByFilename(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT * FROM midi_files WHERE filename = ?";
    auto result = database_.query(sql, {filename});
    
    if (result.rows.empty()) {
        return std::nullopt;
    }
    
    try {
        MidiFileData data;
        data.metadata = parseMetadata(result.rows[0]);
        data.midiJson = json::parse(result.rows[0].at("midi_json"));
        data.routings = getRoutings(data.metadata.id);
        return data;
    } catch (const std::exception& e) {
        Logger::error("MidiDatabase",
                     "Failed to load MIDI file: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<MidiFileMetadata> MidiDatabase::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT * FROM midi_files ORDER BY modified_at DESC";
    auto result = database_.query(sql);
    
    std::vector<MidiFileMetadata> files;
    for (const auto& row : result.rows) {
        try {
            files.push_back(parseMetadata(row));
        } catch (const std::exception& e) {
            Logger::warning("MidiDatabase",
                          "Skipping invalid file: " + std::string(e.what()));
        }
    }
    
    return files;
}

// ============================================================================
// CRUD - DELETE
// ============================================================================

bool MidiDatabase::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "DELETE FROM midi_files WHERE id = ?";
    auto result = database_.execute(sql, {std::to_string(id)});
    
    return result.affectedRows > 0;
}

bool MidiDatabase::exists(const std::string& filename) const {
    const std::string sql = "SELECT COUNT(*) as count FROM midi_files WHERE filename = ?";
    std::string countStr = database_.queryScalar(sql, {filename});
    
    if (countStr.empty()) return false;
    
    try {
        return std::stoi(countStr) > 0;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// ROUTINGS
// ============================================================================

int MidiDatabase::addRouting(const MidiInstrumentRouting& routing) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::time_t now = std::time(nullptr);
    
    const std::string sql = R"(
        INSERT INTO midi_instrument_routings 
        (midi_file_id, track_id, instrument_name, device_id, channel, enabled, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    auto result = database_.execute(sql, {
        std::to_string(routing.midiFileId),
        std::to_string(routing.trackId),
        routing.instrumentName,
        routing.deviceId,
        std::to_string(routing.channel),
        routing.enabled ? "1" : "0",
        std::to_string(now)
    });
    
    if (!result.success) {
        THROW_ERROR(ErrorCode::DATABASE_ERROR,
                   "Failed to add routing: " + result.error);
    }
    
    return static_cast<int>(result.lastInsertId);
}

bool MidiDatabase::updateRouting(const MidiInstrumentRouting& routing) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = R"(
        UPDATE midi_instrument_routings 
        SET instrument_name = ?, device_id = ?, channel = ?, enabled = ?
        WHERE id = ?
    )";
    
    auto result = database_.execute(sql, {
        routing.instrumentName,
        routing.deviceId,
        std::to_string(routing.channel),
        routing.enabled ? "1" : "0",
        std::to_string(routing.id)
    });
    
    return result.affectedRows > 0;
}

bool MidiDatabase::removeRouting(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "DELETE FROM midi_instrument_routings WHERE id = ?";
    auto result = database_.execute(sql, {std::to_string(id)});
    
    return result.affectedRows > 0;
}

std::vector<MidiInstrumentRouting> MidiDatabase::getRoutings(int midiFileId) const {
    const std::string sql = "SELECT * FROM midi_instrument_routings WHERE midi_file_id = ?";
    auto result = database_.query(sql, {std::to_string(midiFileId)});
    
    std::vector<MidiInstrumentRouting> routings;
    for (const auto& row : result.rows) {
        try {
            routings.push_back(parseRouting(row));
        } catch (const std::exception& e) {
            Logger::warning("MidiDatabase",
                          "Skipping invalid routing: " + std::string(e.what()));
        }
    }
    
    return routings;
}

bool MidiDatabase::clearRoutings(int midiFileId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "DELETE FROM midi_instrument_routings WHERE midi_file_id = ?";
    auto result = database_.execute(sql, {std::to_string(midiFileId)});
    
    return result.success;
}

// ============================================================================
// STATISTICS
// ============================================================================

int MidiDatabase::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const std::string sql = "SELECT COUNT(*) as count FROM midi_files";
    std::string countStr = database_.queryScalar(sql);
    
    if (countStr.empty()) return 0;
    
    try {
        return std::stoi(countStr);
    } catch (...) {
        return 0;
    }
}

json MidiDatabase::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return {
        {"total_files", count()},
        {"total_routings", 0}  // TODO: compter les routings
    };
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

MidiFileMetadata MidiDatabase::parseMetadata(
    const std::map<std::string, std::string>& row) const {
    
    MidiFileMetadata meta;
    
    try {
        meta.id = std::stoi(row.at("id"));
        meta.filename = row.at("filename");
        meta.originalFilepath = row.count("original_filepath") ? 
                               row.at("original_filepath") : "";
        meta.durationMs = row.count("duration_ms") ? 
                         std::stoul(row.at("duration_ms")) : 0;
        meta.trackCount = row.count("track_count") ? 
                         std::stoul(row.at("track_count")) : 0;
        meta.eventCount = row.count("event_count") ? 
                         std::stoul(row.at("event_count")) : 0;
        meta.createdAt = row.count("created_at") ? 
                        std::stoull(row.at("created_at")) : 0;
        meta.modifiedAt = row.count("modified_at") ? 
                         std::stoull(row.at("modified_at")) : 0;
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid metadata row: " + std::string(e.what()));
    }
    
    return meta;
}

MidiInstrumentRouting MidiDatabase::parseRouting(
    const std::map<std::string, std::string>& row) const {
    
    MidiInstrumentRouting routing;
    
    try {
        routing.id = std::stoi(row.at("id"));
        routing.midiFileId = std::stoi(row.at("midi_file_id"));
        routing.trackId = std::stoul(row.at("track_id"));
        routing.instrumentName = row.count("instrument_name") ? 
                                row.at("instrument_name") : "";
        routing.deviceId = row.count("device_id") ? 
                          row.at("device_id") : "";
        routing.channel = row.count("channel") ? 
                         std::stoul(row.at("channel")) : 0;
        routing.enabled = row.count("enabled") ? 
                         (row.at("enabled") == "1") : true;
        routing.createdAt = row.count("created_at") ? 
                           std::stoull(row.at("created_at")) : 0;
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid routing row: " + std::string(e.what()));
    }
    
    return routing;
}

} // namespace midiMind
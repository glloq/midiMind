// ============================================================================
// File: backend/src/storage/MidiDatabase.h
// Version: 4.2.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#pragma once

#include "Database.h"
#include "../core/Error.h"
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURES
// ============================================================================

struct MidiFileMetadata {
    int id;
    std::string filename;
    std::string originalFilepath;
    uint32_t durationMs;
    uint16_t trackCount;
    uint32_t eventCount;
    uint64_t createdAt;
    uint64_t modifiedAt;
    
    json toJson() const;
    static MidiFileMetadata fromJson(const json& j);
};

struct MidiInstrumentRouting {
    int id;
    int midiFileId;
    uint16_t trackId;
    std::string instrumentName;
    std::string deviceId;
    uint8_t channel;
    bool enabled;
    uint64_t createdAt;
    
    json toJson() const;
    static MidiInstrumentRouting fromJson(const json& j);
};

struct MidiFileData {
    MidiFileMetadata metadata;
    json midiJson;
    std::vector<MidiInstrumentRouting> routings;
};

// ============================================================================
// CLASS: MidiDatabase
// ============================================================================

class MidiDatabase {
public:
    explicit MidiDatabase(Database& database);
    ~MidiDatabase();
    
    MidiDatabase(const MidiDatabase&) = delete;
    MidiDatabase& operator=(const MidiDatabase&) = delete;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    bool initializeSchema();
    
    // ========================================================================
    // MIDI FILES CRUD
    // ========================================================================
    
    int save(const std::string& filename, const json& midiJson);
    std::optional<MidiFileData> load(int id);
    std::optional<MidiFileData> loadByFilename(const std::string& filename);
    bool remove(int id);
    bool exists(const std::string& filename) const;
    
    std::vector<MidiFileMetadata> list() const;
    
    // ========================================================================
    // ROUTINGS CRUD
    // ========================================================================
    
    int addRouting(const MidiInstrumentRouting& routing);
    bool removeRouting(int id);
    bool updateRouting(const MidiInstrumentRouting& routing);
    
    std::vector<MidiInstrumentRouting> getRoutings(int midiFileId) const;
    bool clearRoutings(int midiFileId);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    int count() const;
    json getStatistics() const;

private:
    Database& database_;
    mutable std::mutex mutex_;
    
    MidiFileMetadata parseMetadata(const std::map<std::string, std::string>& row) const;
    MidiInstrumentRouting parseRouting(const std::map<std::string, std::string>& row) const;
};

} // namespace midiMind
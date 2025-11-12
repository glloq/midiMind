// ============================================================================
// File: backend/src/midi/JsonMidiConverter.h
// Version: 4.2.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Bidirectional converter between binary MIDI (SMF) and JsonMidi format.
//   Enables easy editing and manipulation of MIDI data in JSON.
//
// Features:
//   - MIDI File â†’ JsonMidi conversion
//   - JsonMidi â†’ MIDI File conversion
//   - Automatic note duration calculation
//   - Metadata extraction
//   - Track merging/splitting
//   - Time conversion (ticks â†” milliseconds)
//
// Author: MidiMind Team
// Date: 2025-10-31
//
// Changes v4.2.1:
//   - Added fromMidiFile() implementation support
//   - Added helper methods for MIDI file conversion
//   - Enhanced metadata extraction from MIDI files
//
// ============================================================================

#pragma once

#include "MidiMessage.h"
#include "file/MidiFileReader.h"
#include <string>
#include <fstream>
#include <vector>
#include <optional>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// STRUCTURES: JsonMidi Format
// ============================================================================

/**
 * @struct JsonMidiEvent
 * @brief Single MIDI event in JsonMidi format
 */
struct JsonMidiEvent {
    /// Unique event ID
    std::string id;
    
    /// Event type (noteOn, noteOff, cc, pc, etc.)
    std::string type;
    
    /// Time in milliseconds
    uint32_t time;
    
    /// MIDI channel (1-16)
    uint8_t channel;
    
    // Optional fields (depending on type)
    std::optional<uint8_t> note;
    std::optional<uint8_t> velocity;
    std::optional<uint32_t> duration;  // For notes
    std::optional<uint8_t> controller;
    std::optional<uint8_t> value;
    std::optional<int16_t> pitchBend;
    std::optional<uint8_t> program;
    std::optional<uint32_t> tempo;
    std::optional<std::string> text;
    std::optional<std::vector<uint8_t>> data;
    
    /**
     * @brief Convert to JSON
     * @throws json::exception if conversion fails
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     * @throws json::exception if JSON is invalid
     */
    static JsonMidiEvent fromJson(const json& j);
};

/**
 * @struct JsonMidiMetadata
 * @brief Metadata for JsonMidi file
 */
struct JsonMidiMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string copyright;
    std::string comment;
    
    uint32_t tempo;              // BPM
    std::string timeSignature;   // e.g., "4/4"
    std::string keySignature;    // e.g., "C major"
    uint32_t duration;           // milliseconds
    
    uint16_t ticksPerBeat;
    uint16_t midiFormat;
    uint16_t trackCount;
    
    std::string createdAt;
    std::string modifiedAt;
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     */
    static JsonMidiMetadata fromJson(const json& j);
};

/**
 * @struct JsonMidiTrack
 * @brief Track information
 */
struct JsonMidiTrack {
    uint16_t id;
    std::string name;
    uint8_t channel;
    bool muted;
    bool solo;
    uint8_t volume;
    uint8_t pan;
    int8_t transpose;
    std::string color;
    
    struct {
        uint8_t program;
        uint8_t bank;
        std::string name;
    } instrument;
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     */
    static JsonMidiTrack fromJson(const json& j);
};

/**
 * @struct JsonMidiMarker
 * @brief Timeline marker
 */
struct JsonMidiMarker {
    std::string id;
    uint32_t time;
    std::string label;
    std::string color;
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     */
    static JsonMidiMarker fromJson(const json& j);
};

/**
 * @struct JsonMidi
 * @brief Complete JsonMidi structure
 */
struct JsonMidi {
    std::string format;          // "jsonmidi-v1.0"
    std::string version;         // "1.0.0"
    JsonMidiMetadata metadata;
    std::vector<JsonMidiEvent> timeline;
    std::vector<JsonMidiTrack> tracks;
    std::vector<JsonMidiMarker> markers;
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     * @throws json::exception if JSON is invalid
     */
    static JsonMidi fromJson(const json& j);
    
    /**
     * @brief Create from JSON string
     * @throws json::parse_error if parsing fails
     */
    static JsonMidi fromString(const std::string& jsonStr);
    
    /**
     * @brief Convert to JSON string
     */
    std::string toString(int indent = 2) const;
};

// ============================================================================
// CLASS: JsonMidiConverter
// ============================================================================

/**
 * @class JsonMidiConverter
 * @brief Bidirectional MIDI â†” JsonMidi converter
 * 
 * Converts between binary MIDI files (Standard MIDI File format)
 * and editable JSON format (JsonMidi).
 * 
 * Features:
 * - Track merging (MIDI â†’ JsonMidi)
 * - Track splitting (JsonMidi â†’ MIDI)
 * - Automatic note duration calculation
 * - Metadata extraction/embedding
 * - Time conversion (ticks â†” ms)
 * 
 * Thread Safety: YES (stateless)
 * 
 * Example:
 * ```cpp
 * JsonMidiConverter converter;
 * 
 * // MIDI â†’ JSON
 * JsonMidi jsonMidi = converter.fromMidiFile("input.mid");
 * std::string jsonStr = jsonMidi.toString();
 * 
 * // JSON â†’ MIDI
 * JsonMidi loaded = JsonMidi::fromString(jsonStr);
 * converter.toMidiFile(loaded, "output.mid");
 * ```
 */
class JsonMidiConverter {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     */
    JsonMidiConverter();
    
    /**
     * @brief Destructor
     */
    ~JsonMidiConverter() = default;
    
    // ========================================================================
    // CONVERSION: MIDI â†’ JsonMidi
    // ========================================================================
    
    /**
     * @brief Convert MIDI messages to JsonMidi
     * @param messages Vector of MIDI messages
     * @param ticksPerBeat Ticks per quarter note
     * @param tempo Tempo in BPM
     * @return JsonMidi Converted structure
     * @note Message timestamps are in microseconds, converted to milliseconds in output
     */
    JsonMidi fromMidiMessages(
        const std::vector<MidiMessage>& messages,
        uint16_t ticksPerBeat = 480,
        uint32_t tempo = 120
    );
    
    /**
     * @brief Convert MIDI file to JsonMidi
     * @param filepath Path to MIDI file
     * @return JsonMidi Converted structure
     * @throws std::runtime_error if file cannot be read or is invalid
     */
    JsonMidi fromMidiFile(const std::string& filepath);
    
    // ========================================================================
    // CONVERSION: JsonMidi â†’ MIDI
    // ========================================================================
    
    /**
     * @brief Convert JsonMidi to MIDI messages
     * @param jsonMidi JsonMidi structure
     * @return std::vector<MidiMessage> MIDI messages with timestamps
     * @note Event times (ms) are converted to microseconds for message timestamps
     */
    std::vector<MidiMessage> toMidiMessages(const JsonMidi& jsonMidi);
    
    /**
     * @brief Convert JsonMidi to MIDI file
     * @param jsonMidi JsonMidi structure
     * @param filepath Output path
     * @return true if successful
     */
    bool toMidiFile(const JsonMidi& jsonMidi, const std::string& filepath);
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Validate JsonMidi structure
     * @param jsonMidi Structure to validate
     * @param errorMessage Error message output
     * @return true if valid
     */
    bool validate(const JsonMidi& jsonMidi, std::string& errorMessage) const;
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    /**
     * @brief Calculate note durations from timeline
     * @param timeline Event timeline
     * @note Matches Note On/Off pairs and sets duration
     */
    static void calculateNoteDurations(std::vector<JsonMidiEvent>& timeline);
    
    /**
     * @brief Convert ticks to milliseconds
     * @param ticks Tick value
     * @param ticksPerBeat Ticks per quarter note
     * @param tempo Tempo in BPM
     * @return uint32_t Milliseconds
     */
    static uint32_t ticksToMs(uint32_t ticks, uint16_t ticksPerBeat, uint32_t tempo);
    
    /**
     * @brief Convert milliseconds to ticks
     * @param ms Milliseconds
     * @param ticksPerBeat Ticks per quarter note
     * @param tempo Tempo in BPM
     * @return uint32_t Ticks
     */
    static uint32_t msToTicks(uint32_t ms, uint16_t ticksPerBeat, uint32_t tempo);
    
private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Convert MidiMessage to JsonMidiEvent
     */
    JsonMidiEvent messageToEvent(const MidiMessage& message, uint32_t timeMs);
    
    /**
     * @brief Convert JsonMidiEvent to MidiMessage
     */
    MidiMessage eventToMessage(const JsonMidiEvent& event);
    
    /**
     * @brief Generate unique event ID
     */
    std::string generateEventId(const std::string& type, uint32_t time, 
                               uint8_t channel, uint8_t data1);
    
    /**
     * @brief Extract metadata from messages
     * @note Message timestamps are converted from microseconds to milliseconds
     */
    JsonMidiMetadata extractMetadata(const std::vector<MidiMessage>& messages);
    
    /**
     * @brief Extract tempo from timeline
     */
    uint32_t extractTempo(const std::vector<JsonMidiEvent>& timeline) const;
    
    // ========================================================================
    // PHASE 2: MIDI FILE CONVERSION HELPERS
    // ========================================================================
    
    /**
     * @brief Format time signature as string
     * @param ts TimeSignature structure
     * @return Formatted string (e.g., "4/4")
     */
    std::string formatTimeSignature(const TimeSignature& ts) const;
    
    /**
     * @brief Convert MidiTrack to JsonMidiTrack
     * @param track Source MIDI track
     * @param trackId Track identifier
     * @return Converted JsonMidiTrack
     */
    JsonMidiTrack convertMidiTrackToJsonTrack(const MidiTrack& track, uint16_t trackId);
    
    /**
     * @brief Convert all MIDI events to unified timeline
     * @param midiFile Source MIDI file
     * @param ticksPerBeat Ticks per quarter note
     * @return Vector of JsonMidiEvents in chronological order
     */
    std::vector<JsonMidiEvent> convertMidiEventsToTimeline(
        const MidiFile& midiFile, 
        uint16_t ticksPerBeat
    );
    
    /**
     * @brief Convert single MidiEvent to JsonMidiEvent
     * @param event Source MIDI event
     * @param timeMs Time in milliseconds
     * @param trackChannel Track's primary channel
     * @return Converted JsonMidiEvent
     */
    JsonMidiEvent convertMidiEventToJsonEvent(
        const MidiEvent& event, 
        uint32_t timeMs, 
        uint8_t trackChannel
    );
    
    /**
     * @brief Convert ticks to milliseconds using tempo
     * @param ticks Tick value
     * @param ticksPerBeat Ticks per quarter note
     * @param tempo Tempo in microseconds per quarter note
     * @return Milliseconds
     */
    uint32_t ticksToMilliseconds(uint32_t ticks, uint16_t ticksPerBeat, uint32_t tempo) const;
    
    /**
     * @brief Extract tempo (BPM) from MIDI file Meta events
     * @param midiFile MIDI file to extract tempo from
     * @return Tempo in BPM (default 120 if not found)
     * @note Searches for first Meta Tempo event (0x51) and converts µs/qn -> BPM
     */
    uint32_t extractTempoFromMidiFile(const MidiFile& midiFile) const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Default tempo (BPM)
    static constexpr uint32_t defaultTempo_ = 120;
    
    /// Default time signature
    static constexpr const char* defaultTimeSignature_ = "4/4";
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Load JsonMidi from file
 * @throws std::runtime_error if file cannot be opened
 * @throws json::exception if parsing fails
 */
inline JsonMidi loadJsonMidi(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    
    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        throw std::runtime_error("Invalid JSON in file " + filepath + ": " + e.what());
    }
    
    return JsonMidi::fromJson(j);
}

/**
 * @brief Save JsonMidi to file
 * @throws std::runtime_error if file cannot be created
 */
inline void saveJsonMidi(const JsonMidi& jsonMidi, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create file: " + filepath);
    }
    
    file << jsonMidi.toString(2);
}

} // namespace midiMind
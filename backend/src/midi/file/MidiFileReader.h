// ============================================================================
// File: backend/src/midi/file/MidiFileReader.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   MIDI Standard MIDI File (SMF) reader
//   Supports formats 0, 1, and 2
//
// Features:
//   - Read .mid/.midi files
//   - Parse MThd header
//   - Parse MTrk tracks
//   - Support running status
//   - Complete meta-events
//   - SysEx messages
//   - Format validation
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Aligned with MidiMessage v4.1.0
//   - Enhanced error handling
//   - Better memory management
//
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include "../../core/Error.h"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace midiMind {

// ============================================================================
// ENUMS
// ============================================================================

/**
 * @enum MidiEventType
 * @brief Type of MIDI event in file
 */
enum class MidiEventType {
    MIDI_CHANNEL,    ///< Channel messages (Note On/Off, CC, etc.)
    META,            ///< Meta-events (tempo, time signature, etc.)
    SYSEX            ///< System Exclusive messages
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct TimeSignature
 * @brief MIDI time signature
 */
struct TimeSignature {
    uint8_t numerator = 4;
    uint8_t denominator = 4;
    uint8_t clocksPerClick = 24;
    uint8_t notated32ndNotesPerBeat = 8;
};

/**
 * @struct KeySignature
 * @brief MIDI key signature
 */
struct KeySignature {
    int8_t sharpsFlats = 0;  ///< -7 to +7 (negative = flats, positive = sharps)
    uint8_t majorMinor = 0;   ///< 0 = major, 1 = minor
};

/**
 * @struct MidiEvent
 * @brief MIDI event in a file
 */
struct MidiEvent {
    uint32_t deltaTime = 0;          ///< Delta time in ticks
    uint64_t absoluteTime = 0;       ///< Absolute time in ticks
    MidiEventType type = MidiEventType::MIDI_CHANNEL;
    uint8_t status = 0;
    uint8_t channel = 0;             ///< MIDI channel (0-15)
    std::vector<uint8_t> data;
    
    // Meta-events
    uint8_t metaType = 0;
    std::string metaName;
    std::string text;
    std::string trackName;
    uint32_t tempo = 500000;         ///< Microseconds per quarter note
    TimeSignature timeSignature;
    KeySignature keySignature;
    
    // MIDI channel events
    std::string messageType;
    uint8_t note = 0;
    uint8_t velocity = 0;
    uint8_t controller = 0;
    uint8_t value = 0;
    uint8_t program = 0;
    uint8_t pressure = 0;
    uint16_t pitchBend = 8192;       ///< 0-16383, center = 8192
};

/**
 * @struct MidiTrack
 * @brief MIDI track container
 */
struct MidiTrack {
    std::vector<MidiEvent> events;
    std::string name;
    uint8_t channel = 0;             ///< Primary channel (0-15)
    uint16_t noteCount = 0;
};

/**
 * @struct MidiHeader
 * @brief MIDI file header (MThd chunk)
 */
struct MidiHeader {
    uint16_t format = 1;             ///< 0, 1, or 2
    uint16_t numTracks = 0;
    uint16_t division = 480;         ///< Ticks per quarter note
};

/**
 * @struct MidiFile
 * @brief Complete MIDI file structure
 */
struct MidiFile {
    MidiHeader header;
    std::vector<MidiTrack> tracks;
    
    // Computed values
    uint32_t durationTicks = 0;
    uint32_t durationMs = 0;
    uint16_t tempo = 120;            ///< BPM
    TimeSignature timeSignature;
    
    /**
     * @brief Convert to JSON
     */
    nlohmann::json toJson() const;
    
    /**
     * @brief Get track count
     */
    size_t getTrackCount() const { return tracks.size(); }
    
    /**
     * @brief Check if file is valid
     */
    bool isValid() const { 
        return header.numTracks > 0 && tracks.size() > 0; 
    }
};

// ============================================================================
// CLASS: MidiFileReader
// ============================================================================

/**
 * @class MidiFileReader
 * @brief Read and parse MIDI Standard MIDI Files
 * 
 * Supports SMF formats 0, 1, and 2.
 * Parses all MIDI events, meta-events, and SysEx messages.
 * 
 * Thread Safety: NO (create one instance per thread)
 * 
 * Example:
 * ```cpp
 * MidiFileReader reader;
 * 
 * try {
 *     MidiFile file = reader.readFromFile("/path/to/file.mid");
 *     
 *     std::cout << "Format: " << file.header.format << std::endl;
 *     std::cout << "Tracks: " << file.header.numTracks << std::endl;
 *     std::cout << "Duration: " << file.durationMs << " ms" << std::endl;
 *     
 *     for (const auto& track : file.tracks) {
 *         std::cout << "Track: " << track.name << std::endl;
 *         std::cout << "Events: " << track.events.size() << std::endl;
 *     }
 *     
 * } catch (const MidiMindException& e) {
 *     std::cerr << "Error: " << e.what() << std::endl;
 * }
 * ```
 */
class MidiFileReader {
public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     */
    MidiFileReader();
    
    /**
     * @brief Destructor
     */
    ~MidiFileReader() = default;
    
    // Disable copy
    MidiFileReader(const MidiFileReader&) = delete;
    MidiFileReader& operator=(const MidiFileReader&) = delete;
    
    // ========================================================================
    // PUBLIC METHODS
    // ========================================================================
    
    /**
     * @brief Read MIDI file from disk
     * @param filepath Path to .mid/.midi file
     * @return Parsed MIDI file structure
     * @throws MidiMindException on error
     */
    MidiFile readFromFile(const std::string& filepath);
    
    /**
     * @brief Read MIDI file from memory buffer
     * @param data Pointer to MIDI file data
     * @param size Size of buffer in bytes
     * @return Parsed MIDI file structure
     * @throws MidiMindException on error
     */
    MidiFile readFromBuffer(const uint8_t* data, size_t size);
    
    /**
     * @brief Validate MIDI file without full parsing
     * @param filepath Path to file
     * @return true if valid MIDI file
     * @note Does not throw, returns false on error
     */
    bool validate(const std::string& filepath);

private:
    // ========================================================================
    // PRIVATE METHODS - PARSING
    // ========================================================================
    
    /**
     * @brief Parse track from buffer
     */
    MidiTrack parseTrackFromBuffer(
        const uint8_t* data, 
        size_t offset, 
        uint32_t length
    );
    
    /**
     * @brief Parse track events
     */
    std::vector<MidiEvent> parseTrackEvents(
        const uint8_t* data, 
        size_t& offset, 
        size_t trackEnd
    );
    
    /**
     * @brief Parse meta-event
     */
    void parseMetaEvent(
        const uint8_t* data, 
        size_t& offset, 
        size_t trackEnd, 
        MidiEvent& event
    );
    
    /**
     * @brief Parse SysEx event
     */
    void parseSysExEvent(
        const uint8_t* data, 
        size_t& offset, 
        size_t trackEnd,
        MidiEvent& event, 
        uint8_t statusByte
    );
    
    /**
     * @brief Parse MIDI channel event
     */
    void parseMidiChannelEvent(
        const uint8_t* data, 
        size_t& offset, 
        size_t trackEnd,
        MidiEvent& event, 
        uint8_t statusByte
    );
    
    // ========================================================================
    // PRIVATE METHODS - UTILITIES
    // ========================================================================
    
    /**
     * @brief Read variable-length quantity
     * @param data Data buffer
     * @param offset Current offset (will be advanced)
     * @return Value
     */
    uint32_t readVariableLength(const uint8_t* data, size_t& offset);
    
    /**
     * @brief Read 32-bit big-endian unsigned integer
     */
    uint32_t readUint32BE(const uint8_t* data, size_t offset);
    
    /**
     * @brief Read 16-bit big-endian unsigned integer
     */
    uint16_t readUint16BE(const uint8_t* data, size_t offset);
    
    /**
     * @brief Get number of data bytes for status byte
     * @return Number of data bytes (0-2)
     */
    int getDataBytesCount(uint8_t statusByte);
    
    /**
     * @brief Calculate file duration
     */
    void calculateDuration(MidiFile& file);
    
    /**
     * @brief Extract metadata from tracks
     */
    void extractMetadata(MidiFile& file);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Last running status byte (for running status optimization)
    uint8_t lastRunningStatus_;
    
    /// Current absolute time in ticks
    uint64_t currentAbsoluteTime_;
};

} // namespace midiMind
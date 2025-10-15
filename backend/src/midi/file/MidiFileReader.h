// ============================================================================
// Fichier: src/midi/file/MidiFileReader.h
// Version: 4.0.1
// Date: 2025-10-15
// ============================================================================
// Description:
//   Lecteur de fichiers MIDI Standard MIDI File (SMF)
//   Supporte formats 0, 1, et 2
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include "../../core/Error.h"
#include <string>
#include <vector>
#include <cstdint>

namespace midiMind {

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @enum MidiEventType
 */
enum class MidiEventType {
    MIDI_CHANNEL,    ///< Note On, Note Off, CC, etc.
    META,            ///< Meta-événements (tempo, time sig, etc.)
    SYSEX            ///< Messages SysEx
};

/**
 * @struct MidiEvent
 * @brief Événement MIDI dans un fichier
 */
struct MidiEvent {
    uint32_t deltaTime;         ///< Delta time (ticks)
    MidiEventType type;
    uint8_t status;
    uint8_t channel;            ///< Canal MIDI (1-16)
    std::vector<uint8_t> data;
    
    // Meta-événements
    uint8_t metaType;
    std::string metaName;
    std::string text;
    std::string trackName;
    uint32_t tempo;
    
    struct {
        uint8_t numerator;
        uint8_t denominator;
        uint8_t clocksPerClick;
        uint8_t notated32ndNotesPerBeat;
    } timeSignature;
    
    struct {
        int8_t sharpsFlats;
        uint8_t majorMinor;
    } keySignature;
    
    // MIDI channel events
    std::string messageType;
    uint8_t note;
    uint8_t velocity;
    uint8_t controller;
    uint8_t value;
    uint8_t program;
    uint8_t pressure;
    uint16_t pitchBend;
};

/**
 * @struct MidiTrack
 */
struct MidiTrack {
    std::vector<MidiEvent> events;
};

/**
 * @struct MidiHeader
 */
struct MidiHeader {
    uint16_t format;      ///< 0, 1, ou 2
    uint16_t numTracks;
    uint16_t division;    ///< Ticks per quarter note
};

/**
 * @struct MidiFile
 */
struct MidiFile {
    MidiHeader header;
    std::vector<MidiTrack> tracks;
};

// ============================================================================
// CLASSE: MidiFileReader
// ============================================================================

/**
 * @class MidiFileReader
 * @brief Lecteur de fichiers MIDI
 */
class MidiFileReader {
public:
    MidiFileReader();
    ~MidiFileReader() = default;
    
    /**
     * @brief Lit un fichier MIDI
     */
    MidiFile readFromFile(const std::string& filepath);
    
    /**
     * @brief Lit depuis un buffer
     */
    MidiFile readFromBuffer(const uint8_t* data, size_t size);

private:
    // Parsing
    std::vector<MidiEvent> parseTrackEvents(
        const uint8_t* data, size_t& offset, size_t trackEnd
    );
    
    void parseMetaEvent(
        const uint8_t* data, size_t& offset, size_t trackEnd, MidiEvent& event
    );
    
    void parseSysExEvent(
        const uint8_t* data, size_t& offset, size_t trackEnd, 
        MidiEvent& event, uint8_t statusByte
    );
    
    void parseMidiChannelEvent(
        const uint8_t* data, size_t& offset, size_t trackEnd,
        MidiEvent& event, uint8_t statusByte
    );
    
    // Utilitaires
    uint32_t readVariableLength(const uint8_t* data, size_t& offset);
    uint32_t readUint32BE(const uint8_t* data, size_t offset);
    uint16_t readUint16BE(const uint8_t* data, size_t offset);
    int getDataBytesCount(uint8_t statusByte);
};

} // namespace midiMind
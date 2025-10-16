// ============================================================================
// File: backend/src/midi/file/MidiFileReader.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Complete implementation of MIDI file reader
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Complete implementation of all methods
//   - Enhanced error handling
//   - Better validation
//   - Duration calculation
//   - Metadata extraction
//
// ============================================================================

#include "MidiFileReader.h"
#include "../../core/Logger.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

MidiFileReader::MidiFileReader()
    : lastRunningStatus_(0)
    , currentAbsoluteTime_(0)
{
    Logger::debug("MidiFileReader", "MidiFileReader created");
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

MidiFile MidiFileReader::readFromFile(const std::string& filepath) {
    Logger::info("MidiFileReader", "Reading MIDI file: " + filepath);
    
    // Open file
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND, "Cannot open file: " + filepath);
    }
    
    // Get file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read entire file
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        THROW_ERROR(ErrorCode::FILE_READ_ERROR, "Failed to read file: " + filepath);
    }
    
    file.close();
    
    // Parse from buffer
    MidiFile result = readFromBuffer(buffer.data(), buffer.size());
    
    Logger::info("MidiFileReader", "✓ File read successfully: " + filepath);
    
    return result;
}

MidiFile MidiFileReader::readFromBuffer(const uint8_t* data, size_t size) {
    Logger::info("MidiFileReader", 
                "Reading MIDI from buffer (" + std::to_string(size) + " bytes)");
    
    // Validation
    if (!data || size == 0) {
        THROW_ERROR(ErrorCode::INVALID_ARGUMENT, "Invalid buffer");
    }
    
    if (size < 14) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                   "Buffer too small for MIDI file (need at least 14 bytes)");
    }
    
    try {
        MidiFile midiFile;
        size_t offset = 0;
        
        // Reset state
        lastRunningStatus_ = 0;
        currentAbsoluteTime_ = 0;
        
        // ====================================================================
        // PARSE HEADER CHUNK (MThd)
        // ====================================================================
        
        // Check signature "MThd"
        if (std::memcmp(data + offset, "MThd", 4) != 0) {
            THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                       "Invalid MIDI signature (expected 'MThd')");
        }
        offset += 4;
        
        // Read header length
        uint32_t headerLength = readUint32BE(data, offset);
        offset += 4;
        
        if (headerLength != 6) {
            THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                       "Invalid header length (expected 6, got " + 
                       std::to_string(headerLength) + ")");
        }
        
        // Read format (0, 1, or 2)
        midiFile.header.format = readUint16BE(data, offset);
        offset += 2;
        
        if (midiFile.header.format > 2) {
            THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                       "Unsupported MIDI format: " + 
                       std::to_string(midiFile.header.format));
        }
        
        // Read number of tracks
        midiFile.header.numTracks = readUint16BE(data, offset);
        offset += 2;
        
        // Read division (ticks per quarter note)
        midiFile.header.division = readUint16BE(data, offset);
        offset += 2;
        
        Logger::info("MidiFileReader", 
                    "Format: " + std::to_string(midiFile.header.format) + 
                    ", Tracks: " + std::to_string(midiFile.header.numTracks) +
                    ", Division: " + std::to_string(midiFile.header.division));
        
        // ====================================================================
        // PARSE TRACK CHUNKS (MTrk)
        // ====================================================================
        
        midiFile.tracks.reserve(midiFile.header.numTracks);
        
        for (uint16_t i = 0; i < midiFile.header.numTracks; ++i) {
            Logger::debug("MidiFileReader", 
                         "Parsing track " + std::to_string(i + 1) + "/" + 
                         std::to_string(midiFile.header.numTracks));
            
            // Check we have enough data
            if (offset + 8 > size) {
                THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                           "Unexpected end of file in track " + std::to_string(i));
            }
            
            // Check signature "MTrk"
            if (std::memcmp(data + offset, "MTrk", 4) != 0) {
                THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                           "Invalid track signature (expected 'MTrk')");
            }
            offset += 4;
            
            // Read track length
            uint32_t trackLength = readUint32BE(data, offset);
            offset += 4;
            
            // Check track doesn't exceed buffer
            if (offset + trackLength > size) {
                THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                           "Track length exceeds buffer size");
            }
            
            // Parse track
            MidiTrack track = parseTrackFromBuffer(data, offset, trackLength);
            midiFile.tracks.push_back(track);
            
            // Advance to next track
            offset += trackLength;
            
            // Reset running status between tracks
            lastRunningStatus_ = 0;
            currentAbsoluteTime_ = 0;
        }
        
        // ====================================================================
        // POST-PROCESSING
        // ====================================================================
        
        calculateDuration(midiFile);
        extractMetadata(midiFile);
        
        Logger::info("MidiFileReader", 
                    "✓ MIDI file read successfully (" + 
                    std::to_string(midiFile.tracks.size()) + " tracks, " +
                    std::to_string(midiFile.durationMs) + " ms)");
        
        return midiFile;
        
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::MIDI_FILE_READ_FAILED, 
                   "Failed to read MIDI from buffer: " + std::string(e.what()));
    }
}

bool MidiFileReader::validate(const std::string& filepath) {
    try {
        // Try to read file
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        // Check minimum size
        file.seekg(0, std::ios::end);
        if (file.tellg() < 14) {
            return false;
        }
        file.seekg(0, std::ios::beg);
        
        // Check MThd signature
        char signature[4];
        file.read(signature, 4);
        if (std::memcmp(signature, "MThd", 4) != 0) {
            return false;
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

// ============================================================================
// PRIVATE METHODS - PARSING
// ============================================================================

MidiTrack MidiFileReader::parseTrackFromBuffer(
    const uint8_t* data, 
    size_t offset, 
    uint32_t length) 
{
    MidiTrack track;
    size_t trackEnd = offset + length;
    currentAbsoluteTime_ = 0;
    lastRunningStatus_ = 0;
    
    try {
        track.events = parseTrackEvents(data, offset, trackEnd);
        
        // Count notes
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::MIDI_CHANNEL && 
                (event.status & 0xF0) == 0x90 && 
                event.velocity > 0) {
                track.noteCount++;
            }
        }
        
    } catch (const std::exception& e) {
        Logger::error("MidiFileReader", 
                     "Error parsing track: " + std::string(e.what()));
        throw;
    }
    
    return track;
}

std::vector<MidiEvent> MidiFileReader::parseTrackEvents(
    const uint8_t* data, 
    size_t& offset, 
    size_t trackEnd)
{
    std::vector<MidiEvent> events;
    events.reserve(1000); // Pre-allocate
    
    while (offset < trackEnd) {
        MidiEvent event;
        
        // Read delta time
        event.deltaTime = readVariableLength(data, offset);
        currentAbsoluteTime_ += event.deltaTime;
        event.absoluteTime = currentAbsoluteTime_;
        
        // Read status byte
        uint8_t statusByte = data[offset];
        
        // Handle running status
        if (statusByte < 0x80) {
            // Running status: reuse last status
            if (lastRunningStatus_ == 0) {
                THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                           "Running status without previous status");
            }
            statusByte = lastRunningStatus_;
        } else {
            // New status byte
            offset++;
            
            // System Real-Time messages don't affect running status
            if (statusByte < 0xF8) {
                lastRunningStatus_ = statusByte;
            }
        }
        
        event.status = statusByte;
        
        // Parse based on status type
        if (statusByte == 0xFF) {
            // Meta-event
            parseMetaEvent(data, offset, trackEnd, event);
        } else if (statusByte == 0xF0 || statusByte == 0xF7) {
            // SysEx
            parseSysExEvent(data, offset, trackEnd, event, statusByte);
        } else {
            // MIDI channel event
            parseMidiChannelEvent(data, offset, trackEnd, event, statusByte);
        }
        
        events.push_back(event);
    }
    
    return events;
}

void MidiFileReader::parseMetaEvent(
    const uint8_t* data, 
    size_t& offset, 
    size_t trackEnd,
    MidiEvent& event)
{
    event.type = MidiEventType::META;
    
    // Read meta type
    if (offset >= trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Unexpected end in meta event");
    }
    event.metaType = data[offset++];
    
    // Read length
    uint32_t length = readVariableLength(data, offset);
    
    if (offset + length > trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                   "Meta event length exceeds track");
    }
    
    // Copy data
    event.data.assign(data + offset, data + offset + length);
    offset += length;
    
    // Parse specific meta events
    switch (event.metaType) {
        case 0x00: // Sequence Number
            event.metaName = "Sequence Number";
            break;
            
        case 0x01: // Text Event
            event.metaName = "Text";
            event.text.assign(event.data.begin(), event.data.end());
            break;
            
        case 0x02: // Copyright
            event.metaName = "Copyright";
            event.text.assign(event.data.begin(), event.data.end());
            break;
            
        case 0x03: // Track Name
            event.metaName = "Track Name";
            event.trackName.assign(event.data.begin(), event.data.end());
            break;
            
        case 0x04: // Instrument Name
            event.metaName = "Instrument Name";
            event.text.assign(event.data.begin(), event.data.end());
            break;
            
        case 0x05: // Lyric
            event.metaName = "Lyric";
            event.text.assign(event.data.begin(), event.data.end());
            break;
            
        case 0x06: // Marker
            event.metaName = "Marker";
            event.text.assign(event.data.begin(), event.data.end());
            break;
            
        case 0x07: // Cue Point
            event.metaName = "Cue Point";
            event.text.assign(event.data.begin(), event.data.end());
            break;
            
        case 0x20: // MIDI Channel Prefix
            event.metaName = "Channel Prefix";
            if (length == 1) {
                event.channel = event.data[0];
            }
            break;
            
        case 0x2F: // End of Track
            event.metaName = "End of Track";
            break;
            
        case 0x51: // Set Tempo
            event.metaName = "Set Tempo";
            if (length == 3) {
                event.tempo = (static_cast<uint32_t>(event.data[0]) << 16) |
                             (static_cast<uint32_t>(event.data[1]) << 8) |
                              static_cast<uint32_t>(event.data[2]);
            }
            break;
            
        case 0x54: // SMPTE Offset
            event.metaName = "SMPTE Offset";
            break;
            
        case 0x58: // Time Signature
            event.metaName = "Time Signature";
            if (length == 4) {
                event.timeSignature.numerator = event.data[0];
                event.timeSignature.denominator = 1 << event.data[1];
                event.timeSignature.clocksPerClick = event.data[2];
                event.timeSignature.notated32ndNotesPerBeat = event.data[3];
            }
            break;
            
        case 0x59: // Key Signature
            event.metaName = "Key Signature";
            if (length == 2) {
                event.keySignature.sharpsFlats = static_cast<int8_t>(event.data[0]);
                event.keySignature.majorMinor = event.data[1];
            }
            break;
            
        case 0x7F: // Sequencer Specific
            event.metaName = "Sequencer Specific";
            break;
            
        default:
            event.metaName = "Unknown Meta Event";
            break;
    }
}

void MidiFileReader::parseSysExEvent(
    const uint8_t* data, 
    size_t& offset, 
    size_t trackEnd,
    MidiEvent& event, 
    uint8_t statusByte)
{
    event.type = MidiEventType::SYSEX;
    event.status = statusByte;
    
    // Read length
    uint32_t length = readVariableLength(data, offset);
    
    if (offset + length > trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                   "SysEx length exceeds track");
    }
    
    // Copy data
    event.data.assign(data + offset, data + offset + length);
    offset += length;
}

void MidiFileReader::parseMidiChannelEvent(
    const uint8_t* data, 
    size_t& offset, 
    size_t trackEnd,
    MidiEvent& event, 
    uint8_t statusByte)
{
    event.type = MidiEventType::MIDI_CHANNEL;
    event.status = statusByte;
    event.channel = statusByte & 0x0F;
    
    uint8_t messageType = statusByte & 0xF0;
    
    // Get number of data bytes
    int dataBytes = getDataBytesCount(statusByte);
    
    if (offset + dataBytes > trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                   "Not enough data bytes for MIDI event");
    }
    
    // Read data bytes
    event.data.reserve(dataBytes);
    for (int i = 0; i < dataBytes; ++i) {
        event.data.push_back(data[offset++]);
    }
    
    // Parse specific message types
    switch (messageType) {
        case 0x80: // Note Off
            event.messageType = "Note Off";
            if (dataBytes >= 2) {
                event.note = event.data[0];
                event.velocity = event.data[1];
            }
            break;
            
        case 0x90: // Note On
            event.messageType = "Note On";
            if (dataBytes >= 2) {
                event.note = event.data[0];
                event.velocity = event.data[1];
            }
            break;
            
        case 0xA0: // Polyphonic Aftertouch
            event.messageType = "Polyphonic Aftertouch";
            if (dataBytes >= 2) {
                event.note = event.data[0];
                event.pressure = event.data[1];
            }
            break;
            
        case 0xB0: // Control Change
            event.messageType = "Control Change";
            if (dataBytes >= 2) {
                event.controller = event.data[0];
                event.value = event.data[1];
            }
            break;
            
        case 0xC0: // Program Change
            event.messageType = "Program Change";
            if (dataBytes >= 1) {
                event.program = event.data[0];
            }
            break;
            
        case 0xD0: // Channel Aftertouch
            event.messageType = "Channel Aftertouch";
            if (dataBytes >= 1) {
                event.pressure = event.data[0];
            }
            break;
            
        case 0xE0: // Pitch Bend
            event.messageType = "Pitch Bend";
            if (dataBytes >= 2) {
                event.pitchBend = event.data[0] | (event.data[1] << 7);
            }
            break;
            
        default:
            event.messageType = "Unknown";
            break;
    }
}

// ============================================================================
// PRIVATE METHODS - UTILITIES
// ============================================================================

uint32_t MidiFileReader::readVariableLength(const uint8_t* data, size_t& offset) {
    uint32_t value = 0;
    uint8_t byte;
    
    do {
        byte = data[offset++];
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);
    
    return value;
}

uint32_t MidiFileReader::readUint32BE(const uint8_t* data, size_t offset) {
    return (static_cast<uint32_t>(data[offset + 0]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
            static_cast<uint32_t>(data[offset + 3]);
}

uint16_t MidiFileReader::readUint16BE(const uint8_t* data, size_t offset) {
    return (static_cast<uint16_t>(data[offset + 0]) << 8) |
            static_cast<uint16_t>(data[offset + 1]);
}

int MidiFileReader::getDataBytesCount(uint8_t statusByte) {
    uint8_t messageType = statusByte & 0xF0;
    
    switch (messageType) {
        case 0x80: return 2; // Note Off
        case 0x90: return 2; // Note On
        case 0xA0: return 2; // Polyphonic Aftertouch
        case 0xB0: return 2; // Control Change
        case 0xC0: return 1; // Program Change
        case 0xD0: return 1; // Channel Aftertouch
        case 0xE0: return 2; // Pitch Bend
        default:   return 0;
    }
}

void MidiFileReader::calculateDuration(MidiFile& file) {
    if (file.tracks.empty()) {
        file.durationTicks = 0;
        file.durationMs = 0;
        return;
    }
    
    // Find last event time
    uint64_t maxTicks = 0;
    uint32_t currentTempo = 500000; // Default: 120 BPM
    
    for (const auto& track : file.tracks) {
        for (const auto& event : track.events) {
            maxTicks = std::max(maxTicks, event.absoluteTime);
            
            // Update tempo if we find a tempo change
            if (event.type == MidiEventType::META && event.metaType == 0x51) {
                currentTempo = event.tempo;
            }
        }
    }
    
    file.durationTicks = static_cast<uint32_t>(maxTicks);
    
    // Convert ticks to milliseconds
    // Formula: ms = (ticks * tempo) / (division * 1000)
    if (file.header.division > 0) {
        double ticksPerMs = (file.header.division * 1000.0) / currentTempo;
        file.durationMs = static_cast<uint32_t>(maxTicks / ticksPerMs);
    }
    
    // Calculate BPM
    file.tempo = static_cast<uint16_t>(60000000.0 / currentTempo);
}

void MidiFileReader::extractMetadata(MidiFile& file) {
    if (file.tracks.empty()) {
        return;
    }
    
    // Extract tempo and time signature from first track
    for (const auto& event : file.tracks[0].events) {
        if (event.type == MidiEventType::META) {
            if (event.metaType == 0x51) { // Tempo
                file.tempo = static_cast<uint16_t>(60000000.0 / event.tempo);
            } else if (event.metaType == 0x58) { // Time Signature
                file.timeSignature = event.timeSignature;
            }
        }
    }
    
    // Extract track names
    for (auto& track : file.tracks) {
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::META && event.metaType == 0x03) {
                track.name = event.trackName;
                break;
            }
        }
        
        // Find primary channel
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::MIDI_CHANNEL) {
                track.channel = event.channel;
                break;
            }
        }
    }
}

// ============================================================================
// MIDIFILE JSON CONVERSION
// ============================================================================

nlohmann::json MidiFile::toJson() const {
    nlohmann::json j;
    
    // Header
    j["header"] = {
        {"format", header.format},
        {"num_tracks", header.numTracks},
        {"division", header.division}
    };
    
    // Metadata
    j["duration_ticks"] = durationTicks;
    j["duration_ms"] = durationMs;
    j["tempo"] = tempo;
    j["time_signature"] = {
        {"numerator", timeSignature.numerator},
        {"denominator", timeSignature.denominator}
    };
    
    // Tracks
    j["tracks"] = nlohmann::json::array();
    for (const auto& track : tracks) {
        nlohmann::json trackJson = {
            {"name", track.name},
            {"channel", track.channel},
            {"note_count", track.noteCount},
            {"event_count", track.events.size()}
        };
        j["tracks"].push_back(trackJson);
    }
    
    return j;
}

} // namespace midiMind

// ============================================================================
// END OF FILE MidiFileReader.cpp v4.1.0
// ============================================================================
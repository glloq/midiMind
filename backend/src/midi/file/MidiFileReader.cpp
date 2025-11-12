// ============================================================================
// File: backend/src/midi/file/MidiFileReader.cpp
// Version: 4.3.0 - CHANNEL STANDARDIZATION
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.3.0:
//   - FIX: Channels standardisés sur 1-16 (convention MIDI standard)
//   - parseMidiChannelEvent(): channel = (status & 0x0F) + 1
//   - Meta events gardent channel = 0 (pas de channel MIDI)
//
// ============================================================================

#include "MidiFileReader.h"
#include "../../core/Logger.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <limits>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

MidiFileReader::MidiFileReader()
    : lastRunningStatus_(0)
    , currentAbsoluteTime_(0)
    , bufferSize_(0)
{
    Logger::debug("MidiFileReader", "MidiFileReader created");
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

MidiFile MidiFileReader::readFromFile(const std::string& filepath) {
    Logger::info("MidiFileReader", "Reading MIDI file: " + filepath);
    
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND, "Cannot open file: " + filepath);
    }
    
    std::streampos pos = file.tellg();
    if (pos < 0) {
        THROW_ERROR(ErrorCode::FILE_READ_ERROR, "Failed to get file size: " + filepath);
    }
    std::streamsize size = pos;
    file.seekg(0, std::ios::beg);
    
    if (size < 14) {
        THROW_ERROR(ErrorCode::FILE_READ_ERROR, "File too small to be valid MIDI: " + filepath);
    }
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        THROW_ERROR(ErrorCode::FILE_READ_ERROR, "Failed to read file: " + filepath);
    }
    
    file.close();
    
    MidiFile result = readFromBuffer(buffer.data(), buffer.size());
    
    Logger::info("MidiFileReader", "✓ File read successfully: " + filepath);
    
    return result;
}

MidiFile MidiFileReader::readFromBuffer(const uint8_t* data, size_t size) {
    Logger::info("MidiFileReader", 
                "Reading MIDI from buffer (" + std::to_string(size) + " bytes)");
    
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
        
        lastRunningStatus_ = 0;
        currentAbsoluteTime_ = 0;
        bufferSize_ = size;
        
        // PARSE HEADER CHUNK (MThd)
        if (std::memcmp(data + offset, "MThd", 4) != 0) {
            THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                       "Invalid MIDI signature (expected 'MThd')");
        }
        offset += 4;
        
        uint32_t headerLength = readUint32BE(data, offset);
        offset += 4;
        
        if (headerLength != 6) {
            THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                       "Invalid header length (expected 6, got " + 
                       std::to_string(headerLength) + ")");
        }
        
        midiFile.header.format = readUint16BE(data, offset);
        offset += 2;
        
        if (midiFile.header.format > 2) {
            THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                       "Unsupported MIDI format: " + 
                       std::to_string(midiFile.header.format));
        }
        
        midiFile.header.numTracks = readUint16BE(data, offset);
        offset += 2;
        
        midiFile.header.division = readUint16BE(data, offset);
        offset += 2;
        
        Logger::info("MidiFileReader", 
                    "Format: " + std::to_string(midiFile.header.format) + 
                    ", Tracks: " + std::to_string(midiFile.header.numTracks) +
                    ", Division: " + std::to_string(midiFile.header.division));
        
        // PARSE TRACK CHUNKS (MTrk)
        midiFile.tracks.reserve(midiFile.header.numTracks);
        
        for (uint16_t i = 0; i < midiFile.header.numTracks; ++i) {
            Logger::debug("MidiFileReader", 
                         "Parsing track " + std::to_string(i + 1) + "/" + 
                         std::to_string(midiFile.header.numTracks));
            
            if (offset + 8 > size) {
                THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                           "Unexpected end of file in track " + std::to_string(i));
            }
            
            if (std::memcmp(data + offset, "MTrk", 4) != 0) {
                THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                           "Invalid track signature (expected 'MTrk')");
            }
            offset += 4;
            
            uint32_t trackLength = readUint32BE(data, offset);
            offset += 4;
            
            if (offset + trackLength > size) {
                THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                           "Track length exceeds buffer size");
            }
            
            MidiTrack track = parseTrackFromBuffer(data, offset, trackLength);
            midiFile.tracks.push_back(track);
            
            offset += trackLength;
            
            lastRunningStatus_ = 0;
            currentAbsoluteTime_ = 0;
        }
        
        // POST-PROCESSING
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
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        file.seekg(0, std::ios::end);
        std::streampos pos = file.tellg();
        if (pos < 0 || pos < 14) {
            return false;
        }
        file.seekg(0, std::ios::beg);
        
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
    size_t estimatedEvents = std::min(static_cast<size_t>(100), (trackEnd - offset) / 4);
    events.reserve(estimatedEvents);
    
    while (offset < trackEnd) {
        MidiEvent event;
        
        event.deltaTime = readVariableLength(data, offset, trackEnd);
        currentAbsoluteTime_ += event.deltaTime;
        event.absoluteTime = currentAbsoluteTime_;
        
        if (offset >= trackEnd) {
            THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Unexpected end of track");
        }
        
        uint8_t statusByte = data[offset];
        
        if (statusByte < 0x80) {
            if (lastRunningStatus_ == 0) {
                THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                           "Running status without previous status");
            }
            statusByte = lastRunningStatus_;
        } else {
            offset++;
            if (statusByte < 0xF0) {
                lastRunningStatus_ = statusByte;
            }
        }
        
        if (statusByte == 0xFF) {
            parseMetaEvent(data, offset, trackEnd, event);
        } else if (statusByte == 0xF0 || statusByte == 0xF7) {
            parseSysExEvent(data, offset, trackEnd, event, statusByte);
        } else if (statusByte >= 0x80 && statusByte < 0xF0) {
            parseMidiChannelEvent(data, offset, trackEnd, event, statusByte);
        } else {
            THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                       "Unknown status byte: " + std::to_string(statusByte));
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
    
    if (offset >= trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Unexpected end reading meta type");
    }
    
    event.metaType = data[offset++];
    
    if (offset >= trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Unexpected end reading meta length");
    }
    
    uint32_t length = readVariableLength(data, offset, trackEnd);
    
    if (offset + length > trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                   "Meta event length exceeds track");
    }
    
    event.data.assign(data + offset, data + offset + length);
    offset += length;
    
    // FIX v4.2.9: messageType en camelCase pour tous les meta-events
    switch (event.metaType) {
        case 0x01: // Text Event
            event.metaName = "Text";
            event.messageType = "text";
            if (length > 0) {
                event.text = std::string(event.data.begin(), event.data.end());
            }
            break;
            
        case 0x02: // Copyright Notice
            event.metaName = "Copyright Notice";
            event.messageType = "copyright";
            if (length > 0) {
                event.text = std::string(event.data.begin(), event.data.end());
            }
            break;
            
        case 0x03: // Track Name
            event.metaName = "Track Name";
            event.messageType = "trackName";
            if (length > 0) {
                event.trackName = std::string(event.data.begin(), event.data.end());
            }
            break;
            
        case 0x04: // Instrument Name
            event.metaName = "Instrument Name";
            event.messageType = "instrumentName";
            if (length > 0) {
                event.text = std::string(event.data.begin(), event.data.end());
            }
            break;
            
        case 0x05: // Lyric
            event.metaName = "Lyric";
            event.messageType = "lyric";
            if (length > 0) {
                event.text = std::string(event.data.begin(), event.data.end());
            }
            break;
            
        case 0x06: // Marker
            event.metaName = "Marker";
            event.messageType = "marker";
            if (length > 0) {
                event.text = std::string(event.data.begin(), event.data.end());
            }
            break;
            
        case 0x07: // Cue Point
            event.metaName = "Cue Point";
            event.messageType = "cuePoint";
            if (length > 0) {
                event.text = std::string(event.data.begin(), event.data.end());
            }
            break;
            
        case 0x20: // MIDI Channel Prefix
            event.metaName = "MIDI Channel Prefix";
            event.messageType = "channelPrefix";
            break;
            
        case 0x2F: // End of Track
            event.metaName = "End of Track";
            event.messageType = "endOfTrack";
            break;
            
        case 0x51: // Set Tempo
            event.metaName = "Set Tempo";
            event.messageType = "tempo";
            if (length == 3) {
                event.tempo = (static_cast<uint32_t>(event.data[0]) << 16) |
                             (static_cast<uint32_t>(event.data[1]) << 8) |
                              static_cast<uint32_t>(event.data[2]);
            }
            break;
            
        case 0x54: // SMPTE Offset
            event.metaName = "SMPTE Offset";
            event.messageType = "smpteOffset";
            break;
            
        case 0x58: // Time Signature
            event.metaName = "Time Signature";
            event.messageType = "timeSignature";
            if (length == 4) {
                event.timeSignature.numerator = event.data[0];
                if (event.data[1] < 8) {
                    event.timeSignature.denominator = 1 << event.data[1];
                } else {
                    event.timeSignature.denominator = 128;
                }
                event.timeSignature.clocksPerClick = event.data[2];
                event.timeSignature.notated32ndNotesPerBeat = event.data[3];
            }
            break;
            
        case 0x59: // Key Signature
            event.metaName = "Key Signature";
            event.messageType = "keySignature";
            if (length == 2) {
                event.keySignature.sharpsFlats = static_cast<int8_t>(event.data[0]);
                event.keySignature.majorMinor = event.data[1];
            }
            break;
            
        case 0x7F: // Sequencer Specific
            event.metaName = "Sequencer Specific";
            event.messageType = "sequencerSpecific";
            break;
            
        default:
            event.metaName = "Unknown Meta Event";
            event.messageType = "unknownMeta";
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
    event.messageType = "sysex";
    
    if (offset >= trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Unexpected end reading SysEx length");
    }
    
    uint32_t length = readVariableLength(data, offset, trackEnd);
    
    if (offset + length > trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                   "SysEx length exceeds track");
    }
    
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
    
    // FIX v4.3.0: Channels standardisés 1-16 (convention MIDI standard)
    // MIDI internal: 0x0-0xF, JSON/User-facing: 1-16
    event.channel = (statusByte & 0x0F) + 1;
    
    uint8_t messageType = statusByte & 0xF0;
    
    int dataBytes = getDataBytesCount(statusByte);
    
    if (offset + dataBytes > trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                   "Not enough data bytes for MIDI event");
    }
    
    event.data.reserve(dataBytes);
    for (int i = 0; i < dataBytes; ++i) {
        event.data.push_back(data[offset++]);
    }
    
    // FIX v4.2.9: messageType en camelCase dès le parsing
    switch (messageType) {
        case 0x80: // Note Off
            event.messageType = "noteOff";
            if (dataBytes >= 2) {
                event.note = event.data[0];
                event.velocity = event.data[1];
            }
            break;
            
        case 0x90: // Note On
            event.messageType = "noteOn";
            if (dataBytes >= 2) {
                event.note = event.data[0];
                event.velocity = event.data[1];
            }
            break;
            
        case 0xA0: // Polyphonic Aftertouch
            event.messageType = "polyPressure";
            if (dataBytes >= 2) {
                event.note = event.data[0];
                event.pressure = event.data[1];
            }
            break;
            
        case 0xB0: // Control Change
            event.messageType = "controlChange";
            if (dataBytes >= 2) {
                event.controller = event.data[0];
                event.value = event.data[1];
            }
            break;
            
        case 0xC0: // Program Change
            event.messageType = "programChange";
            if (dataBytes >= 1) {
                event.program = event.data[0];
            }
            break;
            
        case 0xD0: // Channel Aftertouch
            event.messageType = "channelPressure";
            if (dataBytes >= 1) {
                event.pressure = event.data[0];
            }
            break;
            
        case 0xE0: // Pitch Bend
            event.messageType = "pitchBend";
            if (dataBytes >= 2) {
                event.pitchBend = event.data[0] | (event.data[1] << 7);
            }
            break;
            
        default:
            event.messageType = "unknown";
            break;
    }
}

// ============================================================================
// PRIVATE METHODS - UTILITIES
// ============================================================================

uint32_t MidiFileReader::readVariableLength(
    const uint8_t* data, 
    size_t& offset, 
    size_t limit)
{
    uint32_t value = 0;
    uint8_t byte;
    int count = 0;
    
    do {
        if (offset >= limit) {
            THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                       "Unexpected end reading variable length");
        }
        
        byte = data[offset++];
        
        if (count >= 4) {
            THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                       "Variable length value too large");
        }
        
        if (value & 0xFE000000) {
            THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                       "Variable length overflow");
        }
        
        value = (value << 7) | (byte & 0x7F);
        count++;
        
    } while (byte & 0x80);
    
    return value;
}

uint16_t MidiFileReader::readUint16BE(const uint8_t* data, size_t offset) {
    return (static_cast<uint16_t>(data[offset]) << 8) |
            static_cast<uint16_t>(data[offset + 1]);
}

uint32_t MidiFileReader::readUint32BE(const uint8_t* data, size_t offset) {
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
            static_cast<uint32_t>(data[offset + 3]);
}

int MidiFileReader::getDataBytesCount(uint8_t statusByte) {
    uint8_t messageType = statusByte & 0xF0;
    
    switch (messageType) {
        case 0x80: // Note Off
        case 0x90: // Note On
        case 0xA0: // Polyphonic Aftertouch
        case 0xB0: // Control Change
        case 0xE0: // Pitch Bend
            return 2;
            
        case 0xC0: // Program Change
        case 0xD0: // Channel Aftertouch
            return 1;
            
        default:
            return 0;
    }
}

void MidiFileReader::calculateDuration(MidiFile& file) {
    if (file.tracks.empty()) {
        return;
    }
    
    uint32_t maxTicks = 0;
    uint32_t currentTempo = 500000;
    
    for (const auto& track : file.tracks) {
        for (const auto& event : track.events) {
            if (event.absoluteTime > maxTicks) {
                maxTicks = event.absoluteTime;
            }
            
            if (event.type == MidiEventType::META && event.metaType == 0x51) {
                currentTempo = event.tempo;
            }
        }
    }
    
    file.durationTicks = maxTicks;
    
    if (file.header.division > 0) {
        double microseconds = (static_cast<double>(maxTicks) * currentTempo) / 
                             file.header.division;
        double durationMsDouble = microseconds / 1000.0;
        
        if (durationMsDouble > std::numeric_limits<uint32_t>::max()) {
            file.durationMs = std::numeric_limits<uint32_t>::max();
        } else {
            file.durationMs = static_cast<uint32_t>(durationMsDouble);
        }
    }
    
    file.tempo = static_cast<uint16_t>(60000000.0 / currentTempo);
}

void MidiFileReader::extractMetadata(MidiFile& file) {
    if (file.tracks.empty()) {
        return;
    }
    
    for (const auto& event : file.tracks[0].events) {
        if (event.type == MidiEventType::META) {
            if (event.metaType == 0x51) {
                file.tempo = static_cast<uint16_t>(60000000.0 / event.tempo);
            } else if (event.metaType == 0x58) {
                file.timeSignature = event.timeSignature;
            }
        }
    }
    
    for (auto& track : file.tracks) {
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::META && event.metaType == 0x03) {
                track.name = event.trackName;
                break;
            }
        }
        
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
    
    j["header"] = {
        {"format", header.format},
        {"num_tracks", header.numTracks},
        {"division", header.division}
    };
    
    j["duration_ticks"] = durationTicks;
    j["duration_ms"] = durationMs;
    j["tempo"] = tempo;
    j["time_signature"] = {
        {"numerator", timeSignature.numerator},
        {"denominator", timeSignature.denominator}
    };
    
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
// END OF FILE MidiFileReader.cpp v4.3.0
// ============================================================================
// ============================================================================
// Fichier: backend/src/midi/file/MidiFileReader.cpp
// Version: 3.0.1 - COMPLET
// Date: 2025-10-13
// ============================================================================
// Description:
//   Lecteur de fichiers MIDI Standard MIDI File (SMF)
//   Supporte formats 0, 1, et 2
//
// CORRECTIONS v3.0.1:
//   ✅ parseTrackEvents() complète
//   ✅ parseMetaEvent() complète
//   ✅ parseSysExEvent() ajoutée
//   ✅ parseMidiChannelEvent() ajoutée
//   ✅ readVariableLength() complète
//   ✅ readFromBuffer() complète
//   ✅ Toutes méthodes utilitaires
//
// Fonctionnalités:
//   - Lecture fichiers .mid/.midi
//   - Parsing header MThd
//   - Parsing tracks MTrk
//   - Support running status
//   - Méta-événements complets
//   - Messages SysEx
//   - Validation format
//
// Auteur: midiMind Team
// ============================================================================

#include "MidiFileReader.h"
#include "../../core/Logger.h"
#include "../../core/Error.h"
#include <fstream>
#include <sstream>
#include <cstring>

namespace midiMind {

// ============================================================================
// CONSTRUCTEUR
// ============================================================================

MidiFileReader::MidiFileReader() {
    Logger::debug("MidiFileReader", "MidiFileReader created");
}

// ============================================================================
// LECTURE DEPUIS FICHIER
// ============================================================================

MidiFile MidiFileReader::readFromFile(const std::string& filepath) {
    Logger::info("MidiFileReader", "Reading MIDI file: " + filepath);
    
    // Ouvrir le fichier
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        THROW_ERROR(ErrorCode::FILE_NOT_FOUND, "Cannot open file: " + filepath);
    }
    
    // Lire la taille
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Lire tout le contenu
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        THROW_ERROR(ErrorCode::FILE_READ_ERROR, "Failed to read file: " + filepath);
    }
    
    file.close();
    
    // Parser depuis le buffer
    return readFromBuffer(buffer.data(), buffer.size());
}

// ============================================================================
// LECTURE DEPUIS BUFFER
// ============================================================================

MidiFile MidiFileReader::readFromBuffer(const uint8_t* data, size_t size) {
    Logger::info("MidiFileReader", "Reading MIDI from buffer (" + 
                std::to_string(size) + " bytes)");
    
    // Validation
    if (!data || size == 0) {
        THROW_ERROR(ErrorCode::INVALID_ARGUMENT, "Invalid buffer");
    }
    
    if (size < 14) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Buffer too small for MIDI file");
    }
    
    MidiFile midiFile;
    size_t offset = 0;
    
    // ========================================================================
    // HEADER CHUNK (MThd)
    // ========================================================================
    
    // Vérifier signature "MThd"
    if (std::memcmp(data + offset, "MThd", 4) != 0) {
        THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                   "Invalid MIDI signature (expected MThd)");
    }
    offset += 4;
    
    // Lire longueur du header
    uint32_t headerLength = readUint32BE(data, offset);
    offset += 4;
    
    if (headerLength != 6) {
        THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                   "Invalid header length (expected 6, got " + 
                   std::to_string(headerLength) + ")");
    }
    
    // Lire format (0, 1, ou 2)
    midiFile.header.format = readUint16BE(data, offset);
    offset += 2;
    
    if (midiFile.header.format > 2) {
        THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                   "Unsupported MIDI format: " + 
                   std::to_string(midiFile.header.format));
    }
    
    // Lire nombre de tracks
    midiFile.header.numTracks = readUint16BE(data, offset);
    offset += 2;
    
    // Lire division (ticks per quarter note)
    midiFile.header.division = readUint16BE(data, offset);
    offset += 2;
    
    Logger::info("MidiFileReader", "Format: " + std::to_string(midiFile.header.format));
    Logger::info("MidiFileReader", "Tracks: " + std::to_string(midiFile.header.numTracks));
    Logger::info("MidiFileReader", "Division: " + std::to_string(midiFile.header.division));
    
    // ========================================================================
    // TRACK CHUNKS (MTrk)
    // ========================================================================
    
    midiFile.tracks.reserve(midiFile.header.numTracks);
    
    for (uint16_t trackNum = 0; trackNum < midiFile.header.numTracks; ++trackNum) {
        Logger::debug("MidiFileReader", "Parsing track " + std::to_string(trackNum + 1));
        
        // Vérifier qu'il reste assez de données
        if (offset + 8 > size) {
            THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                       "Incomplete track header for track " + std::to_string(trackNum));
        }
        
        // Vérifier le magic number "MTrk"
        if (data[offset] != 'M' || data[offset + 1] != 'T' || 
            data[offset + 2] != 'r' || data[offset + 3] != 'k') {
            THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                       "Invalid track header (expected 'MTrk') for track " + 
                       std::to_string(trackNum));
        }
        offset += 4;
        
        // Lire la longueur du track
        uint32_t trackLength = readUint32BE(data, offset);
        offset += 4;
        
        size_t trackEnd = offset + trackLength;
        
        if (trackEnd > size) {
            THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                       "Track length exceeds file size for track " + 
                       std::to_string(trackNum));
        }
        
        // Parser les événements du track
        MidiTrack track;
        track.events = parseTrackEvents(data, offset, trackEnd);
        
        midiFile.tracks.push_back(track);
        
        Logger::debug("MidiFileReader", 
            "Track " + std::to_string(trackNum + 1) + 
            " parsed (" + std::to_string(track.events.size()) + " events)");
    }
    
    Logger::info("MidiFileReader", "✓ MIDI file parsed successfully");
    
    return midiFile;
}

// ============================================================================
// PARSING TRACK EVENTS
// ============================================================================

std::vector<MidiEvent> MidiFileReader::parseTrackEvents(
    const uint8_t* data,
    size_t& offset,
    size_t trackEnd) {
    
    std::vector<MidiEvent> events;
    uint8_t runningStatus = 0;
    
    while (offset < trackEnd) {
        MidiEvent event;
        
        // Lire delta time (variable length)
        event.deltaTime = readVariableLength(data, offset);
        
        if (offset >= trackEnd) {
            Logger::warn("MidiFileReader", "Unexpected end of track");
            break;
        }
        
        // Lire le status byte
        uint8_t statusByte = data[offset];
        
        // Gestion du running status
        if (statusByte < 0x80) {
            // C'est un data byte, utiliser le running status
            if (runningStatus == 0) {
                THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                           "Running status without previous status");
            }
            statusByte = runningStatus;
        } else {
            offset++;
            runningStatus = statusByte;
        }
        
        // Parser selon le type d'événement
        if (statusByte == 0xFF) {
            // Méta-événement
            parseMetaEvent(data, offset, trackEnd, event);
            runningStatus = 0; // Les méta-événements ne participent pas au running status
            
        } else if (statusByte == 0xF0 || statusByte == 0xF7) {
            // SysEx event
            parseSysExEvent(data, offset, trackEnd, event, statusByte);
            runningStatus = 0;
            
        } else {
            // Événement MIDI channel
            parseMidiChannelEvent(data, offset, trackEnd, event, statusByte);
        }
        
        events.push_back(event);
    }
    
    return events;
}

// ============================================================================
// PARSING META EVENT
// ============================================================================

void MidiFileReader::parseMetaEvent(
    const uint8_t* data,
    size_t& offset,
    size_t trackEnd,
    MidiEvent& event) {
    
    if (offset >= trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Incomplete meta event");
    }
    
    uint8_t metaType = data[offset++];
    uint32_t length = readVariableLength(data, offset);
    
    if (offset + length > trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Meta event length exceeds track");
    }
    
    event.type = MidiEventType::META;
    event.metaType = metaType;
    event.data.assign(data + offset, data + offset + length);
    
    offset += length;
    
    // Parser les méta-événements courants
    switch (metaType) {
        case 0x00: // Sequence Number
            event.metaName = "Sequence Number";
            break;
            
        case 0x01: // Text Event
            event.metaName = "Text";
            event.text = std::string(event.data.begin(), event.data.end());
            break;
            
        case 0x02: // Copyright Notice
            event.metaName = "Copyright";
            event.text = std::string(event.data.begin(), event.data.end());
            break;
            
        case 0x03: // Track Name
            event.metaName = "Track Name";
            event.trackName = std::string(event.data.begin(), event.data.end());
            break;
            
        case 0x04: // Instrument Name
            event.metaName = "Instrument Name";
            event.text = std::string(event.data.begin(), event.data.end());
            break;
            
        case 0x05: // Lyric
            event.metaName = "Lyric";
            event.text = std::string(event.data.begin(), event.data.end());
            break;
            
        case 0x06: // Marker
            event.metaName = "Marker";
            event.text = std::string(event.data.begin(), event.data.end());
            break;
            
        case 0x07: // Cue Point
            event.metaName = "Cue Point";
            event.text = std::string(event.data.begin(), event.data.end());
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
                event.timeSignature.denominator = 1 << event.data[1]; // 2^denominator
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

// ============================================================================
// PARSING SYSEX EVENT
// ============================================================================

void MidiFileReader::parseSysExEvent(
    const uint8_t* data,
    size_t& offset,
    size_t trackEnd,
    MidiEvent& event,
    uint8_t statusByte) {
    
    event.type = MidiEventType::SYSEX;
    event.status = statusByte;
    
    // Lire la longueur
    uint32_t length = readVariableLength(data, offset);
    
    if (offset + length > trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "SysEx length exceeds track");
    }
    
    // Copier les données
    event.data.assign(data + offset, data + offset + length);
    offset += length;
}

// ============================================================================
// PARSING MIDI CHANNEL EVENT
// ============================================================================

void MidiFileReader::parseMidiChannelEvent(
    const uint8_t* data,
    size_t& offset,
    size_t trackEnd,
    MidiEvent& event,
    uint8_t statusByte) {
    
    event.type = MidiEventType::MIDI_CHANNEL;
    event.status = statusByte;
    event.channel = (statusByte & 0x0F) + 1; // Canal 1-16
    
    uint8_t messageType = statusByte & 0xF0;
    int dataBytes = getDataBytesCount(statusByte);
    
    if (offset + dataBytes > trackEnd) {
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Incomplete MIDI channel event");
    }
    
    // Lire les data bytes
    for (int i = 0; i < dataBytes; i++) {
        event.data.push_back(data[offset++]);
    }
    
    // Parser selon le type de message
    switch (messageType) {
        case 0x80: // Note Off
            event.messageType = "Note Off";
            if (event.data.size() >= 2) {
                event.note = event.data[0];
                event.velocity = event.data[1];
            }
            break;
            
        case 0x90: // Note On
            event.messageType = "Note On";
            if (event.data.size() >= 2) {
                event.note = event.data[0];
                event.velocity = event.data[1];
                // Note On avec velocity 0 = Note Off
                if (event.velocity == 0) {
                    event.messageType = "Note Off";
                }
            }
            break;
            
        case 0xA0: // Polyphonic Aftertouch
            event.messageType = "Polyphonic Aftertouch";
            if (event.data.size() >= 2) {
                event.note = event.data[0];
                event.pressure = event.data[1];
            }
            break;
            
        case 0xB0: // Control Change
            event.messageType = "Control Change";
            if (event.data.size() >= 2) {
                event.controller = event.data[0];
                event.value = event.data[1];
            }
            break;
            
        case 0xC0: // Program Change
            event.messageType = "Program Change";
            if (event.data.size() >= 1) {
                event.program = event.data[0];
            }
            break;
            
        case 0xD0: // Channel Aftertouch
            event.messageType = "Channel Aftertouch";
            if (event.data.size() >= 1) {
                event.pressure = event.data[0];
            }
            break;
            
        case 0xE0: // Pitch Bend
            event.messageType = "Pitch Bend";
            if (event.data.size() >= 2) {
                event.pitchBend = (event.data[1] << 7) | event.data[0];
            }
            break;
            
        default:
            event.messageType = "Unknown";
            break;
    }
}

// ============================================================================
// MÉTHODES UTILITAIRES
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
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

uint16_t MidiFileReader::readUint16BE(const uint8_t* data, size_t offset) {
    return (static_cast<uint16_t>(data[offset]) << 8) |
           static_cast<uint16_t>(data[offset + 1]);
}

int MidiFileReader::getDataBytesCount(uint8_t statusByte) {
    uint8_t messageType = statusByte & 0xF0;
    
    switch (messageType) {
        case 0x80:  // Note Off
        case 0x90:  // Note On
        case 0xA0:  // Polyphonic Aftertouch
        case 0xB0:  // Control Change
        case 0xE0:  // Pitch Bend
            return 2;
            
        case 0xC0:  // Program Change
        case 0xD0:  // Channel Aftertouch
            return 1;
            
        default:
            return 0;
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiFileReader.cpp v3.0.1 - COMPLET
// ============================================================================

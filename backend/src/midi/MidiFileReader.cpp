// ============================================================================
// Fichier: backend/src/midi/MidiFileReader.cpp
// Ajout de l'implémentation complète de readFromBuffer()
// ============================================================================

#include "MidiFileReader.h"
#include "../core/Logger.h"
#include "../core/Error.h"
#include <sstream>
#include <cstring>

namespace midiMind {

// ============================================================================
// LECTURE DEPUIS BUFFER - IMPLÉMENTATION COMPLÈTE
// ============================================================================

/**
 * @brief Lit un fichier MIDI depuis un buffer mémoire
 * 
 * @param data Pointeur vers les données du fichier MIDI
 * @param size Taille du buffer en octets
 * @return MidiFile Structure MIDI parsée
 * @throws MidiMindException en cas d'erreur
 */
MidiFile MidiFileReader::readFromBuffer(const uint8_t* data, size_t size) {
    Logger::info("MidiFileReader", "Reading MIDI from buffer (" + 
                std::to_string(size) + " bytes)");
    
    // Validation
    if (!data || size == 0) {
        THROW_ERROR(ErrorCode::INVALID_ARGUMENT, "Invalid buffer");
    }
    
    if (size < 14) { // Taille minimale : 4 (MThd) + 4 (length) + 6 (header)
        THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, "Buffer too small for MIDI file");
    }
    
    try {
        MidiFile midiFile;
        size_t offset = 0;
        
        // ====================================================================
        // HEADER CHUNK
        // ====================================================================
        
        // Vérifier signature "MThd"
        if (std::memcmp(data + offset, "MThd", 4) != 0) {
            THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                       "Invalid MIDI signature (expected MThd)");
        }
        offset += 4;
        
        // Lire longueur du header (big-endian)
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
        
        // ====================================================================
        // TRACK CHUNKS
        // ====================================================================
        
        midiFile.tracks.reserve(midiFile.header.numTracks);
        
        for (uint16_t i = 0; i < midiFile.header.numTracks; ++i) {
            Logger::debug("MidiFileReader", "Parsing track " + std::to_string(i + 1));
            
            // Vérifier qu'il reste assez de données
            if (offset + 8 > size) {
                THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                           "Unexpected end of buffer in track " + std::to_string(i));
            }
            
            // Vérifier signature "MTrk"
            if (std::memcmp(data + offset, "MTrk", 4) != 0) {
                THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT, 
                           "Invalid track signature (expected MTrk)");
            }
            offset += 4;
            
            // Lire longueur du track
            uint32_t trackLength = readUint32BE(data, offset);
            offset += 4;
            
            // Vérifier que le track ne dépasse pas le buffer
            if (offset + trackLength > size) {
                THROW_ERROR(ErrorCode::MIDI_FILE_CORRUPTED, 
                           "Track length exceeds buffer size");
            }
            
            // Parser le track
            MidiTrack track = parseTrackFromBuffer(data, offset, trackLength);
            midiFile.tracks.push_back(track);
            
            // Avancer au prochain track
            offset += trackLength;
        }
        
        Logger::info("MidiFileReader", "✓ MIDI file read from buffer successfully");
        
        return midiFile;
        
    } catch (const std::exception& e) {
        THROW_ERROR(ErrorCode::MIDI_FILE_READ_FAILED, 
                   "Failed to read MIDI from buffer: " + std::string(e.what()));
    }
}

// ============================================================================
// MÉTHODES PRIVÉES - LECTURE BUFFER
// ============================================================================

/**
 * @brief Parse un track depuis un buffer
 */
MidiTrack MidiFileReader::parseTrackFromBuffer(const uint8_t* data, 
                                               size_t offset, 
                                               uint32_t length) {
    MidiTrack track;
    size_t endOffset = offset + length;
    uint8_t runningStatus = 0;
    uint32_t currentTime = 0;
    
    while (offset < endOffset) {
        // Lire delta-time (variable length)
        uint32_t deltaTime = 0;
        offset = readVariableLengthFromBuffer(data, offset, deltaTime);
        currentTime += deltaTime;
        
        // Lire status byte
        uint8_t statusByte = data[offset];
        
        // Gestion du running status
        if (statusByte < 0x80) {
            // Running status : réutiliser le dernier status
            statusByte = runningStatus;
        } else {
            offset++;
            runningStatus = statusByte;
        }
        
        // Parser l'événement selon le type
        MidiEvent event;
        event.deltaTime = deltaTime;
        event.absoluteTime = currentTime;
        
        if (statusByte == 0xFF) {
            // Meta event
            event.type = MidiEventType::META;
            event.metaType = data[offset++];
            
            uint32_t metaLength = 0;
            offset = readVariableLengthFromBuffer(data, offset, metaLength);
            
            event.data.assign(data + offset, data + offset + metaLength);
            offset += metaLength;
            
        } else if (statusByte == 0xF0 || statusByte == 0xF7) {
            // SysEx event
            event.type = MidiEventType::SYSEX;
            
            uint32_t sysexLength = 0;
            offset = readVariableLengthFromBuffer(data, offset, sysexLength);
            
            event.data.assign(data + offset, data + offset + sysexLength);
            offset += sysexLength;
            
        } else {
            // MIDI channel event
            event.type = MidiEventType::MIDI;
            event.status = statusByte;
            
            // Nombre de data bytes selon le type de message
            int dataBytes = getDataBytesCount(statusByte);
            
            for (int i = 0; i < dataBytes; i++) {
                event.data.push_back(data[offset++]);
            }
        }
        
        track.events.push_back(event);
    }
    
    return track;
}

/**
 * @brief Lit un nombre variable length depuis un buffer
 */
size_t MidiFileReader::readVariableLengthFromBuffer(const uint8_t* data, 
                                                    size_t offset, 
                                                    uint32_t& value) {
    value = 0;
    uint8_t byte;
    
    do {
        byte = data[offset++];
        value = (value << 7) | (byte & 0x7F);
    } while (byte & 0x80);
    
    return offset;
}

/**
 * @brief Lit un uint32 big-endian depuis un buffer
 */
uint32_t MidiFileReader::readUint32BE(const uint8_t* data, size_t offset) {
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

/**
 * @brief Lit un uint16 big-endian depuis un buffer
 */
uint16_t MidiFileReader::readUint16BE(const uint8_t* data, size_t offset) {
    return (static_cast<uint16_t>(data[offset]) << 8) |
           static_cast<uint16_t>(data[offset + 1]);
}

/**
 * @brief Retourne le nombre de data bytes pour un status byte
 */
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

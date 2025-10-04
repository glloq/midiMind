// ============================================================================
// Fichier: src/midi/MidiFileWriter.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "MidiFileWriter.h"
#include <sstream>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

MidiFileWriter::MidiFileWriter() {
    Logger::debug("MidiFileWriter", "MidiFileWriter constructed");
}

MidiFileWriter::~MidiFileWriter() {
    Logger::debug("MidiFileWriter", "MidiFileWriter destroyed");
}

// ============================================================================
// ÉCRITURE
// ============================================================================

void MidiFileWriter::write(const std::string& filepath, const MidiFile& midiFile) {
    Logger::info("MidiFileWriter", "Writing MIDI file: " + filepath);
    
    std::ofstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        THROW_ERROR(ErrorCode::MIDI_FILE_OPEN_FAILED,
                   "Cannot create file: " + filepath);
    }
    
    try {
        // Écrire header
        writeHeader(file, midiFile.header);
        
        // Écrire tracks
        for (const auto& track : midiFile.tracks) {
            writeTrack(file, track);
        }
        
        file.close();
        
        Logger::info("MidiFileWriter", "✓ MIDI file written successfully");
        
    } catch (const std::exception& e) {
        file.close();
        throw;
    }
}

std::vector<uint8_t> MidiFileWriter::writeToBuffer(const MidiFile& midiFile) {
    // TODO: Implémenter écriture dans buffer
    THROW_ERROR(ErrorCode::SYSTEM_NOT_SUPPORTED,
               "writeToBuffer not yet implemented");
}

// ============================================================================
// ÉCRITURE - HEADER
// ============================================================================

void MidiFileWriter::writeHeader(std::ofstream& file, const MidiFileHeader& header) {
    // Signature "MThd"
    writeSignature(file, "MThd");
    
    // Longueur du header (toujours 6)
    writeUInt32(file, 6);
    
    // Format
    writeUInt16(file, header.format);
    
    // Nombre de pistes
    writeUInt16(file, header.numTracks);
    
    // Division
    writeUInt16(file, header.division);
}

// ============================================================================
// ÉCRITURE - TRACK
// ============================================================================

void MidiFileWriter::writeTrack(std::ofstream& file, const MidiTrack& track) {
    // Signature "MTrk"
    writeSignature(file, "MTrk");
    
    // Écrire les événements dans un buffer temporaire
    std::ostringstream trackBuffer;
    uint8_t runningStatus = 0;
    
    for (const auto& event : track.events) {
        // Delta time
        writeVariableLength(trackBuffer, event.deltaTime);
        
        // Message MIDI
        const uint8_t* data = event.message.getData();
        size_t size = event.message.getSize();
        
        for (size_t i = 0; i < size; ++i) {
            trackBuffer.put(data[i]);
        }
    }
    
    // Ajouter End of Track si pas déjà présent
    bool hasEndOfTrack = false;
    if (!track.events.empty()) {
        const auto& lastEvent = track.events.back();
        if (lastEvent.message.getSize() >= 2 &&
            lastEvent.message.getData()[0] == 0xFF &&
            lastEvent.message.getData()[1] == 0x2F) {
            hasEndOfTrack = true;
        }
    }
    
    if (!hasEndOfTrack) {
        // Delta time = 0
        trackBuffer.put(0x00);
        // End of Track meta-event
        trackBuffer.put(0xFF);
        trackBuffer.put(0x2F);
        trackBuffer.put(0x00);
    }
    
    // Écrire la longueur de la piste
    std::string trackData = trackBuffer.str();
    writeUInt32(file, trackData.size());
    
    // Écrire les données
    file.write(trackData.data(), trackData.size());
}

// ============================================================================
// ÉCRITURE - EVENT
// ============================================================================

void MidiFileWriter::writeEvent(std::ofstream& file, const MidiEvent& event, uint8_t& runningStatus) {
    // Delta time
    writeVariableLength(file, event.deltaTime);
    
    // Message MIDI
    const uint8_t* data = event.message.getData();
    size_t size = event.message.getSize();
    
    // Running status optimization
    if (size > 0 && (data[0] & 0x80) != 0) {
        // Status byte
        if (data[0] != runningStatus || (data[0] & 0xF0) >= 0xF0) {
            writeUInt8(file, data[0]);
            runningStatus = data[0];
        }
        
        // Data bytes
        for (size_t i = 1; i < size; ++i) {
            writeUInt8(file, data[i]);
        }
    } else {
        // Pas de status byte (running status)
        for (size_t i = 0; i < size; ++i) {
            writeUInt8(file, data[i]);
        }
    }
}

// ============================================================================
// UTILITAIRES D'ÉCRITURE
// ============================================================================

void MidiFileWriter::writeVariableLength(std::ofstream& file, uint32_t value) {
    uint32_t buffer = value & 0x7F;
    
    while ((value >>= 7) > 0) {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7F);
    }
    
    while (true) {
        writeUInt8(file, buffer & 0xFF);
        if (buffer & 0x80) {
            buffer >>= 8;
        } else {
            break;
        }
    }
}

void MidiFileWriter::writeUInt16(std::ofstream& file, uint16_t value) {
    uint16_t bigEndian = __builtin_bswap16(value);
    file.write(reinterpret_cast<const char*>(&bigEndian), 2);
}

void MidiFileWriter::writeUInt32(std::ofstream& file, uint32_t value) {
    uint32_t bigEndian = __builtin_bswap32(value);
    file.write(reinterpret_cast<const char*>(&bigEndian), 4);
}

void MidiFileWriter::writeUInt8(std::ofstream& file, uint8_t value) {
    file.write(reinterpret_cast<const char*>(&value), 1);
}

void MidiFileWriter::writeSignature(std::ofstream& file, const std::string& signature) {
    file.write(signature.c_str(), 4);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiFileWriter.cpp
// ============================================================================
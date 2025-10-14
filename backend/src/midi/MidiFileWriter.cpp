// ============================================================================
// Fichier: src/midi/MidiFileWriter.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "MidiFileWriter.h"
#include <sstream>
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MidiFileWriter::MidiFileWriter() 
    : runningStatusEnabled_(true)
    , autoEndOfTrack_(true)
    , defaultFormat_(1)
    , defaultDivision_(480)
    , bytesWritten_(0)
    , eventsWritten_(0) {
    
    Logger::debug("MidiFileWriter", "MidiFileWriter constructed");
}

MidiFileWriter::~MidiFileWriter() {
    Logger::debug("MidiFileWriter", "MidiFileWriter destroyed");
}

// ============================================================================
// ÉCRITURE VERS FICHIER
// ============================================================================

void MidiFileWriter::write(const std::string& filepath, const MidiFile& midiFile) {
    Logger::info("MidiFileWriter", "Writing MIDI file: " + filepath);
    
    std::ofstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        THROW_ERROR(ErrorCode::MIDI_FILE_OPEN_FAILED,
                   "Cannot create file: " + filepath);
    }
    
    try {
        bytesWritten_ = 0;
        eventsWritten_ = 0;
        
        writeToStream(file, midiFile);
        
        file.close();
        
        Logger::info("MidiFileWriter", 
                    "✓ MIDI file written successfully (" + 
                    std::to_string(bytesWritten_) + " bytes, " +
                    std::to_string(eventsWritten_) + " events)");
        
    } catch (const std::exception& e) {
        file.close();
        THROW_ERROR(ErrorCode::MIDI_FILE_WRITE_FAILED,
                   "Failed to write MIDI file: " + std::string(e.what()));
    }
}

bool MidiFileWriter::writeWithValidation(const std::string& filepath, 
                                        const MidiFile& midiFile, 
                                        bool validate) {
    if (validate) {
        std::string errorMessage;
        if (!this->validate(midiFile, errorMessage)) {
            Logger::error("MidiFileWriter", "Validation failed: " + errorMessage);
            return false;
        }
    }
    
    try {
        write(filepath, midiFile);
        return true;
    } catch (const std::exception& e) {
        Logger::error("MidiFileWriter", "Write failed: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// ÉCRITURE VERS BUFFER
// ============================================================================

std::vector<uint8_t> MidiFileWriter::writeToBuffer(const MidiFile& midiFile) {
    std::ostringstream buffer(std::ios::binary);
    
    writeToStream(buffer, midiFile);
    
    std::string str = buffer.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MidiFileWriter::writeToStream(std::ostream& stream, const MidiFile& midiFile) {
    // Écrire header
    writeHeader(stream, midiFile.header);
    
    // Écrire tracks
    for (const auto& track : midiFile.tracks) {
        // Optimiser la piste si nécessaire
        MidiTrack optimizedTrack = runningStatusEnabled_ ? optimizeTrack(track) : track;
        
        // Ajouter End-of-Track si nécessaire
        if (autoEndOfTrack_ && !hasEndOfTrack(optimizedTrack)) {
            uint32_t lastTime = 0;
            if (!optimizedTrack.events.empty()) {
                lastTime = 0; // Delta time = 0 après le dernier événement
            }
            optimizedTrack.events.push_back(createEndOfTrackEvent(lastTime));
        }
        
        writeTrack(stream, optimizedTrack);
    }
}

// ============================================================================
// VALIDATION
// ============================================================================

bool MidiFileWriter::validate(const MidiFile& midiFile, std::string& errorMessage) const {
    // Vérifier le format
    if (midiFile.header.format > 2) {
        errorMessage = "Invalid format: " + std::to_string(midiFile.header.format);
        return false;
    }
    
    // Vérifier le nombre de pistes
    if (midiFile.header.numTracks != midiFile.tracks.size()) {
        errorMessage = "Track count mismatch: header=" + 
                      std::to_string(midiFile.header.numTracks) + 
                      " actual=" + std::to_string(midiFile.tracks.size());
        return false;
    }
    
    // Format 0 doit avoir exactement 1 piste
    if (midiFile.header.format == 0 && midiFile.tracks.size() != 1) {
        errorMessage = "Format 0 must have exactly 1 track";
        return false;
    }
    
    // Vérifier chaque piste
    for (size_t i = 0; i < midiFile.tracks.size(); ++i) {
        const auto& track = midiFile.tracks[i];
        
        // Vérifier que la piste a des événements
        if (track.events.empty()) {
            Logger::warn("MidiFileWriter", 
                        "Track " + std::to_string(i) + " is empty");
        }
        
        // Vérifier End-of-Track
        if (!hasEndOfTrack(track) && !autoEndOfTrack_) {
            errorMessage = "Track " + std::to_string(i) + " missing End-of-Track";
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// ÉCRITURE - HEADER
// ============================================================================

void MidiFileWriter::writeHeader(std::ostream& stream, const MidiFileHeader& header) {
    // Signature "MThd"
    writeSignature(stream, "MThd");
    
    // Longueur du header (toujours 6)
    writeUInt32(stream, 6);
    
    // Format
    writeUInt16(stream, header.format);
    
    // Nombre de pistes
    writeUInt16(stream, header.numTracks);
    
    // Division
    writeUInt16(stream, header.division);
    
    bytesWritten_ += 14; // 4 + 4 + 2 + 2 + 2
}

// ============================================================================
// ÉCRITURE - TRACK
// ============================================================================

void MidiFileWriter::writeTrack(std::ostream& stream, const MidiTrack& track) {
    // Signature "MTrk"
    writeSignature(stream, "MTrk");
    
    // Calculer la taille de la piste
    uint32_t trackSize = calculateTrackSize(track);
    
    // Écrire la longueur
    writeUInt32(stream, trackSize);
    
    // Écrire les événements
    uint8_t runningStatus = 0;
    
    for (const auto& event : track.events) {
        writeEvent(stream, event, runningStatus);
        eventsWritten_++;
    }
    
    bytesWritten_ += 8 + trackSize; // Signature + length + data
}

// ============================================================================
// ÉCRITURE - EVENT
// ============================================================================

void MidiFileWriter::writeEvent(std::ostream& stream, 
                                const MidiEvent& event, 
                                uint8_t& runningStatus) {
    // Delta time
    writeVariableLength(stream, event.deltaTime);
    
    // Message MIDI
    const uint8_t* data = event.message.getData();
    size_t size = event.message.getSize();
    
    if (size == 0) return;
    
    uint8_t status = data[0];
    
    // Meta-event ou SysEx
    if (status == 0xFF || status == 0xF0 || status == 0xF7) {
        // Pas de running status pour meta-events et SysEx
        runningStatus = 0;
        
        for (size_t i = 0; i < size; ++i) {
            writeUInt8(stream, data[i]);
        }
        return;
    }
    
    // Running status optimization
    if ((status & 0x80) != 0) {
        // Status byte présent
        bool canUseRunningStatus = runningStatusEnabled_ && 
                                   (status == runningStatus) && 
                                   ((status & 0xF0) < 0xF0);
        
        if (!canUseRunningStatus) {
            writeUInt8(stream, status);
            runningStatus = status;
        }
        
        // Data bytes
        for (size_t i = 1; i < size; ++i) {
            writeUInt8(stream, data[i]);
        }
    } else {
        // Pas de status byte (running status déjà actif)
        for (size_t i = 0; i < size; ++i) {
            writeUInt8(stream, data[i]);
        }
    }
}

void MidiFileWriter::writeMetaEvent(std::ostream& stream, 
                                   uint8_t type, 
                                   const std::vector<uint8_t>& data) {
    writeUInt8(stream, 0xFF);
    writeUInt8(stream, type);
    writeVariableLength(stream, static_cast<uint32_t>(data.size()));
    writeBytes(stream, data.data(), data.size());
}

void MidiFileWriter::writeSysExEvent(std::ostream& stream, 
                                    const std::vector<uint8_t>& data) {
    writeUInt8(stream, 0xF0);
    writeVariableLength(stream, static_cast<uint32_t>(data.size()));
    writeBytes(stream, data.data(), data.size());
}

// ============================================================================
// UTILITAIRES D'ÉCRITURE
// ============================================================================

void MidiFileWriter::writeVariableLength(std::ostream& stream, uint32_t value) {
    uint32_t buffer = value & 0x7F;
    
    while ((value >>= 7) > 0) {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7F);
    }
    
    while (true) {
        writeUInt8(stream, static_cast<uint8_t>(buffer & 0xFF));
        if (buffer & 0x80) {
            buffer >>= 8;
        } else {
            break;
        }
    }
}

void MidiFileWriter::writeUInt32(std::ostream& stream, uint32_t value) {
    // Big-endian
    writeUInt8(stream, static_cast<uint8_t>((value >> 24) & 0xFF));
    writeUInt8(stream, static_cast<uint8_t>((value >> 16) & 0xFF));
    writeUInt8(stream, static_cast<uint8_t>((value >> 8) & 0xFF));
    writeUInt8(stream, static_cast<uint8_t>(value & 0xFF));
}

void MidiFileWriter::writeUInt16(std::ostream& stream, uint16_t value) {
    // Big-endian
    writeUInt8(stream, static_cast<uint8_t>((value >> 8) & 0xFF));
    writeUInt8(stream, static_cast<uint8_t>(value & 0xFF));
}

void MidiFileWriter::writeUInt8(std::ostream& stream, uint8_t value) {
    stream.put(static_cast<char>(value));
}

void MidiFileWriter::writeSignature(std::ostream& stream, const std::string& signature) {
    if (signature.length() != 4) {
        THROW_ERROR(ErrorCode::MIDI_FILE_INVALID_FORMAT,
                   "Signature must be 4 characters");
    }
    stream.write(signature.c_str(), 4);
}

void MidiFileWriter::writeBytes(std::ostream& stream, 
                               const uint8_t* data, 
                               size_t length) {
    for (size_t i = 0; i < length; ++i) {
        writeUInt8(stream, data[i]);
    }
}

// ============================================================================
// HELPERS
// ============================================================================

uint32_t MidiFileWriter::calculateTrackSize(const MidiTrack& track) const {
    uint32_t size = 0;
    
    for (const auto& event : track.events) {
        // Delta time size
        uint32_t delta = event.deltaTime;
        if (delta == 0) {
            size += 1;
        } else {
            uint32_t temp = delta;
            while (temp > 0) {
                size++;
                temp >>= 7;
            }
        }
        
        // Event size
        size += event.message.getSize();
    }
    
    return size;
}

bool MidiFileWriter::hasEndOfTrack(const MidiTrack& track) const {
    if (track.events.empty()) {
        return false;
    }
    
    const auto& lastEvent = track.events.back();
    const uint8_t* data = lastEvent.message.getData();
    size_t size = lastEvent.message.getSize();
    
    // End of Track: FF 2F 00
    return (size >= 3 && 
            data[0] == 0xFF && 
            data[1] == 0x2F && 
            data[2] == 0x00);
}

MidiEvent MidiFileWriter::createEndOfTrackEvent(uint32_t deltaTime) const {
    // End of Track meta-event: FF 2F 00
    std::vector<uint8_t> data = {0xFF, 0x2F, 0x00};
    MidiMessage msg(data.data(), data.size());
    return MidiEvent(deltaTime, msg);
}

MidiTrack MidiFileWriter::optimizeTrack(const MidiTrack& track) const {
    MidiTrack optimized = track;
    
    // TODO: Implémenter optimisations supplémentaires
    // - Fusionner delta times consécutifs
    // - Optimiser running status
    // - Supprimer événements redondants
    
    return optimized;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiFileWriter.cpp
// ============================================================================
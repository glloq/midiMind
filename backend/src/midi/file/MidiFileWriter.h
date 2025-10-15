// ============================================================================
// Fichier: src/midi/file/MidiFileWriter.h
// Version: 4.0.1
// Date: 2025-10-15
// ============================================================================
// Description:
//   Écrivain de fichiers MIDI Standard MIDI File (SMF)
// ============================================================================

#pragma once

#include "MidiFileReader.h" // Pour réutiliser les structures
#include <string>
#include <vector>
#include <iostream>

namespace midiMind {

/**
 * @class MidiFileWriter
 * @brief Écrivain de fichiers MIDI
 */
class MidiFileWriter {
public:
    MidiFileWriter();
    ~MidiFileWriter();
    
    /**
     * @brief Écrit un fichier MIDI
     */
    void write(const std::string& filepath, const MidiFile& midiFile);
    
    /**
     * @brief Écrit avec validation optionnelle
     */
    bool writeWithValidation(
        const std::string& filepath,
        const MidiFile& midiFile,
        bool validate = true
    );
    
    /**
     * @brief Écrit dans un buffer
     */
    std::vector<uint8_t> writeToBuffer(const MidiFile& midiFile);
    
    /**
     * @brief Valide un fichier MIDI avant écriture
     */
    bool validate(const MidiFile& midiFile, std::string& errorMessage) const;
    
    // Configuration
    void setRunningStatusEnabled(bool enabled);
    void setAutoEndOfTrack(bool enabled);
    
    // Statistiques
    uint32_t getBytesWritten() const;
    uint32_t getEventsWritten() const;

private:
    void writeToStream(std::ostream& stream, const MidiFile& midiFile);
    void writeHeader(std::ostream& stream, const MidiHeader& header);
    void writeTrack(std::ostream& stream, const MidiTrack& track);
    
    void writeUint32BE(std::ostream& stream, uint32_t value);
    void writeUint16BE(std::ostream& stream, uint16_t value);
    void writeVLQ(std::ostream& stream, uint32_t value);
    
    uint32_t calculateTrackSize(const MidiTrack& track) const;
    bool hasEndOfTrack(const MidiTrack& track) const;
    MidiEvent createEndOfTrackEvent(uint32_t deltaTime) const;
    MidiTrack optimizeTrack(const MidiTrack& track) const;
    
    bool runningStatusEnabled_;
    bool autoEndOfTrack_;
    uint16_t defaultFormat_;
    uint16_t defaultDivision_;
    
    uint32_t bytesWritten_;
    uint32_t eventsWritten_;
};

} // namespace midiMind
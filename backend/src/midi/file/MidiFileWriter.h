// ============================================================================
// File: backend/src/midi/file/MidiFileWriter.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   MIDI Standard MIDI File (SMF) writer
//   Compatible with MidiFileReader structures
//
// Features:
//   - Write .mid/.midi files
//   - Support formats 0, 1, and 2
//   - Running status optimization
//   - Automatic End-of-Track insertion
//   - Buffer write support
//   - Validation
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Uses MidiFileReader structures (MidiFile, MidiTrack, MidiEvent)
//   - Enhanced error handling
//   - Better memory management
//
// ============================================================================

#pragma once

#include "MidiFileReader.h"  // Use same structures
#include <string>
#include <vector>
#include <cstdint>

namespace midiMind {

// ============================================================================
// CLASS: MidiFileWriter
// ============================================================================

/**
 * @class MidiFileWriter
 * @brief Write MIDI Standard MIDI Files
 * 
 * Writes SMF formats 0, 1, and 2.
 * Compatible with structures from MidiFileReader.
 * 
 * Thread Safety: NO (create one instance per thread)
 * 
 * Example:
 * ```cpp
 * MidiFileWriter writer;
 * 
 * // Create a MIDI file
 * MidiFile file;
 * file.header.format = 1;
 * file.header.numTracks = 2;
 * file.header.division = 480;
 * 
 * // Add tracks...
 * 
 * try {
 *     writer.writeToFile("/path/to/output.mid", file);
 *     std::cout << "Written: " << writer.getBytesWritten() << " bytes" << std::endl;
 *     
 * } catch (const MidiMindException& e) {
 *     std::cerr << "Error: " << e.what() << std::endl;
 * }
 * ```
 */
class MidiFileWriter {
public:
    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     */
    MidiFileWriter();
    
    /**
     * @brief Destructor
     */
    ~MidiFileWriter();
    
    // Disable copy
    MidiFileWriter(const MidiFileWriter&) = delete;
    MidiFileWriter& operator=(const MidiFileWriter&) = delete;
    
    // ========================================================================
    // PUBLIC METHODS - WRITE
    // ========================================================================
    
    /**
     * @brief Write MIDI file to disk
     * @param filepath Path to output .mid/.midi file
     * @param midiFile MIDI file structure to write
     * @throws MidiMindException on error
     */
    void writeToFile(const std::string& filepath, const MidiFile& midiFile);
    
    /**
     * @brief Write MIDI file to memory buffer
     * @param midiFile MIDI file structure to write
     * @return Buffer containing complete MIDI file
     * @throws MidiMindException on error
     */
    std::vector<uint8_t> writeToBuffer(const MidiFile& midiFile);
    
    /**
     * @brief Validate MIDI file before writing
     * @param midiFile File to validate
     * @param errorMessage Output error message if validation fails
     * @return true if valid
     */
    bool validate(const MidiFile& midiFile, std::string& errorMessage) const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Enable/disable running status optimization
     * @param enabled true to enable (default: true)
     * 
     * Running status removes redundant status bytes to reduce file size.
     */
    void setRunningStatusEnabled(bool enabled);
    
    /**
     * @brief Enable/disable automatic End-of-Track insertion
     * @param enabled true to enable (default: true)
     * 
     * Automatically adds End-of-Track meta-event if missing.
     */
    void setAutoEndOfTrack(bool enabled);
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get number of bytes written in last operation
     */
    uint32_t getBytesWritten() const;
    
    /**
     * @brief Get number of events written in last operation
     */
    uint32_t getEventsWritten() const;

private:
    // ========================================================================
    // PRIVATE METHODS - WRITING
    // ========================================================================
    
    /**
     * @brief Write to output stream
     */
    void writeToStream(std::ostream& stream, const MidiFile& midiFile);
    
    /**
     * @brief Write MIDI header chunk
     */
    void writeHeader(std::ostream& stream, const MidiHeader& header);
    
    /**
     * @brief Write track chunk
     */
    void writeTrack(std::ostream& stream, const MidiTrack& track);
    
    // ========================================================================
    // PRIVATE METHODS - LOW LEVEL
    // ========================================================================
    
    /**
     * @brief Write 32-bit big-endian unsigned integer
     */
    void writeUint32BE(std::ostream& stream, uint32_t value);
    
    /**
     * @brief Write 16-bit big-endian unsigned integer
     */
    void writeUint16BE(std::ostream& stream, uint16_t value);
    
    /**
     * @brief Write variable-length quantity (VLQ)
     */
    void writeVLQ(std::ostream& stream, uint32_t value);
    
    // ========================================================================
    // PRIVATE METHODS - HELPERS
    // ========================================================================
    
    /**
     * @brief Check if track has End-of-Track event
     */
    bool hasEndOfTrack(const MidiTrack& track) const;
    
    /**
     * @brief Create End-of-Track event
     */
    MidiEvent createEndOfTrackEvent(uint32_t deltaTime) const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Enable running status optimization
    bool runningStatusEnabled_;
    
    /// Automatically add End-of-Track if missing
    bool autoEndOfTrack_;
    
    /// Statistics: bytes written
    uint32_t bytesWritten_;
    
    /// Statistics: events written
    uint32_t eventsWritten_;
};

} // namespace midiMind

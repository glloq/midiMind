// ============================================================================
// File: backend/src/midi/file/MidiFileWriter.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   MIDI file writer - writes Standard MIDI Files (SMF) format 0 and 1.
//   Compatible with MidiFileReader structures.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Adapted to use MidiFileReader structures (MidiFile, MidiTrack, MidiEvent)
//   - Complete validation implementation
//   - Automatic End-of-Track insertion
//   - Running status optimization
//   - Performance improvements (reduced copies)
//   - Safety improvements in VLQ encoding
//
// ============================================================================

#include "MidiFileWriter.h"
#include "../../core/Logger.h"
#include "../../core/Error.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MidiFileWriter::MidiFileWriter()
    : runningStatusEnabled_(true)
    , autoEndOfTrack_(true)
    , bytesWritten_(0)
    , eventsWritten_(0)
{
    Logger::debug("MidiFileWriter", "MidiFileWriter initialized");
}

MidiFileWriter::~MidiFileWriter() {
    Logger::debug("MidiFileWriter", "MidiFileWriter destroyed");
}

// ============================================================================
// MAIN WRITE FUNCTIONS
// ============================================================================

void MidiFileWriter::writeToFile(const std::string& filepath, 
                                  const MidiFile& midiFile) {
    Logger::info("MidiFileWriter", "Writing MIDI file: " + filepath);
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        THROW_ERROR(ErrorCode::FILE_WRITE_ERROR, 
                   "Cannot create file: " + filepath);
    }
    
    try {
        bytesWritten_ = 0;
        eventsWritten_ = 0;
        
        writeToStream(file, midiFile);
        
        file.close();
        
        Logger::info("MidiFileWriter",
            "✓ File written successfully (" +
            std::to_string(bytesWritten_) + " bytes, " +
            std::to_string(eventsWritten_) + " events)");
            
    } catch (const std::exception& e) {
        file.close();
        THROW_ERROR(ErrorCode::FILE_WRITE_ERROR,
                   "Failed to write MIDI file: " + std::string(e.what()));
    }
}

std::vector<uint8_t> MidiFileWriter::writeToBuffer(const MidiFile& midiFile) {
    Logger::debug("MidiFileWriter", "Writing MIDI to buffer");
    
    std::ostringstream buffer(std::ios::binary);
    
    bytesWritten_ = 0;
    eventsWritten_ = 0;
    
    writeToStream(buffer, midiFile);
    
    // Use move semantics to avoid copy
    std::string str = std::move(buffer.str());
    std::vector<uint8_t> result(str.begin(), str.end());
    
    Logger::debug("MidiFileWriter",
        "✓ Buffer written: " + std::to_string(result.size()) + " bytes");
    
    return result;
}

bool MidiFileWriter::validate(const MidiFile& midiFile, 
                              std::string& errorMessage) const {
    // Clear error message at start
    errorMessage.clear();
    
    // Check format
    if (midiFile.header.format > 2) {
        errorMessage = "Invalid format: " + 
                      std::to_string(midiFile.header.format);
        return false;
    }
    
    // Check number of tracks
    if (midiFile.header.numTracks != midiFile.tracks.size()) {
        errorMessage = "Track count mismatch: header=" +
                      std::to_string(midiFile.header.numTracks) +
                      " actual=" + std::to_string(midiFile.tracks.size());
        return false;
    }
    
    // Format 0 should have exactly 1 track
    if (midiFile.header.format == 0 && midiFile.tracks.size() != 1) {
        errorMessage = "Format 0 must have exactly 1 track";
        return false;
    }
    
    // Check division
    if (midiFile.header.division == 0) {
        errorMessage = "Invalid division: 0";
        return false;
    }
    
    // Check each track
    for (size_t i = 0; i < midiFile.tracks.size(); ++i) {
        const auto& track = midiFile.tracks[i];
        
        // Check if track is empty
        if (track.events.empty()) {
            errorMessage = "Track " + std::to_string(i) + " is empty";
            return false;
        }
        
        // Check for End-of-Track if not auto-adding
        if (!autoEndOfTrack_ && !hasEndOfTrack(track)) {
            errorMessage = "Track " + std::to_string(i) + 
                          " missing End-of-Track";
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// INTERNAL WRITE FUNCTIONS
// ============================================================================

void MidiFileWriter::writeToStream(std::ostream& stream, 
                                   const MidiFile& midiFile) {
    // Write header
    writeHeader(stream, midiFile.header);
    
    // Write tracks
    for (const auto& track : midiFile.tracks) {
        // Check if we need to add End-of-Track
        if (autoEndOfTrack_ && !hasEndOfTrack(track)) {
            Logger::debug("MidiFileWriter", "Adding End-of-Track");
            
            // Create modified track with End-of-Track
            MidiTrack trackCopy = track;
            MidiEvent endEvent = createEndOfTrackEvent(0);
            trackCopy.events.push_back(endEvent);
            
            writeTrack(stream, trackCopy);
        } else {
            // Write track as-is without copy
            writeTrack(stream, track);
        }
    }
}

void MidiFileWriter::writeHeader(std::ostream& stream, 
                                 const MidiHeader& header) {
    // Write "MThd"
    stream.write("MThd", 4);
    bytesWritten_ += 4;
    
    // Write header length (always 6)
    writeUint32BE(stream, 6);
    
    // Write format
    writeUint16BE(stream, header.format);
    
    // Write number of tracks
    writeUint16BE(stream, header.numTracks);
    
    // Write division
    writeUint16BE(stream, header.division);
    
    Logger::debug("MidiFileWriter",
        "Header written: format=" + std::to_string(header.format) +
        ", tracks=" + std::to_string(header.numTracks) +
        ", division=" + std::to_string(header.division));
}

void MidiFileWriter::writeTrack(std::ostream& stream, const MidiTrack& track) {
    // Write "MTrk"
    stream.write("MTrk", 4);
    bytesWritten_ += 4;
    
    // Build track data in a buffer first
    std::ostringstream trackData(std::ios::binary);
    
    uint8_t lastStatus = 0;
    
    for (const auto& event : track.events) {
        // Write delta time (Variable Length Quantity)
        writeVLQ(trackData, event.deltaTime);
        
        // Write event based on type
        if (event.type == MidiEventType::META) {
            // Meta event: FF <type> <length> <data>
            trackData.put(0xFF);
            trackData.put(event.metaType);
            writeVLQ(trackData, static_cast<uint32_t>(event.data.size()));
            trackData.write(reinterpret_cast<const char*>(event.data.data()), 
                           event.data.size());
            lastStatus = 0; // Reset running status
            
        } else if (event.type == MidiEventType::SYSEX) {
            // SysEx: F0 <length> <data> or F7 <length> <data>
            trackData.put(event.status);
            writeVLQ(trackData, static_cast<uint32_t>(event.data.size()));
            trackData.write(reinterpret_cast<const char*>(event.data.data()), 
                           event.data.size());
            lastStatus = 0; // Reset running status
            
        } else {
            // MIDI channel event
            uint8_t status = event.status;
            
            // Apply running status optimization if enabled
            bool useRunningStatus = false;
            if (runningStatusEnabled_ && 
                status >= 0x80 && status <= 0xEF &&
                status == lastStatus) {
                useRunningStatus = true;
            }
            
            if (!useRunningStatus) {
                trackData.put(status);
                lastStatus = status;
            }
            
            // Write data bytes
            trackData.write(reinterpret_cast<const char*>(event.data.data()), 
                           event.data.size());
        }
        
        eventsWritten_++;
    }
    
    // Get track data as string (use move to avoid copy)
    std::string trackDataStr = std::move(trackData.str());
    
    // Write track length
    writeUint32BE(stream, static_cast<uint32_t>(trackDataStr.size()));
    
    // Write track data
    stream.write(trackDataStr.data(), trackDataStr.size());
    
    bytesWritten_ += trackDataStr.size() + 4; // +4 for length field
    
    Logger::debug("MidiFileWriter",
        "Track written: " + std::to_string(track.events.size()) + " events, " +
        std::to_string(trackDataStr.size()) + " bytes");
}

// ============================================================================
// LOW-LEVEL WRITE HELPERS
// ============================================================================

void MidiFileWriter::writeUint32BE(std::ostream& stream, uint32_t value) {
    stream.put(static_cast<char>((value >> 24) & 0xFF));
    stream.put(static_cast<char>((value >> 16) & 0xFF));
    stream.put(static_cast<char>((value >> 8) & 0xFF));
    stream.put(static_cast<char>(value & 0xFF));
    bytesWritten_ += 4;
}

void MidiFileWriter::writeUint16BE(std::ostream& stream, uint16_t value) {
    stream.put(static_cast<char>((value >> 8) & 0xFF));
    stream.put(static_cast<char>(value & 0xFF));
    bytesWritten_ += 2;
}

void MidiFileWriter::writeVLQ(std::ostream& stream, uint32_t value) {
    // Variable Length Quantity encoding (MIDI standard)
    // Maximum 4 bytes: 0x0FFFFFFF (268,435,455)
    
    // Check if value is within valid range for VLQ
    if (value > 0x0FFFFFFF) {
        THROW_ERROR(ErrorCode::INVALID_ARGUMENT, 
                   "Value too large for MIDI VLQ encoding: " + std::to_string(value));
    }
    
    uint32_t buffer = value & 0x7F;
    
    while ((value >>= 7) > 0) {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7F);
    }
    
    // Write bytes with safety counter to prevent infinite loop
    int bytesWritten = 0;
    const int MAX_VLQ_BYTES = 4;
    
    while (bytesWritten < MAX_VLQ_BYTES) {
        stream.put(static_cast<char>(buffer & 0xFF));
        bytesWritten_++;
        bytesWritten++;
        
        if (buffer & 0x80) {
            buffer >>= 8;
        } else {
            break;
        }
    }
    
    // This should never happen with proper input validation, but catch it
    if (bytesWritten >= MAX_VLQ_BYTES && (buffer & 0x80)) {
        THROW_ERROR(ErrorCode::INTERNAL_ERROR, 
                   "VLQ encoding exceeded maximum bytes");
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

bool MidiFileWriter::hasEndOfTrack(const MidiTrack& track) const {
    if (track.events.empty()) {
        return false;
    }
    
    const auto& lastEvent = track.events.back();
    
    // Check for Meta Event End-of-Track: FF 2F 00
    return lastEvent.type == MidiEventType::META &&
           lastEvent.metaType == 0x2F;
}

MidiEvent MidiFileWriter::createEndOfTrackEvent(uint32_t deltaTime) const {
    MidiEvent event;
    event.deltaTime = deltaTime;
    event.type = MidiEventType::META;
    event.status = 0xFF;
    event.metaType = 0x2F;
    event.metaName = "End of Track";
    event.data.clear(); // Length = 0
    
    return event;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void MidiFileWriter::setRunningStatusEnabled(bool enabled) {
    runningStatusEnabled_ = enabled;
    Logger::debug("MidiFileWriter",
        "Running status " + std::string(enabled ? "enabled" : "disabled"));
}

void MidiFileWriter::setAutoEndOfTrack(bool enabled) {
    autoEndOfTrack_ = enabled;
    Logger::debug("MidiFileWriter",
        "Auto End-of-Track " + std::string(enabled ? "enabled" : "disabled"));
}

// ============================================================================
// STATISTICS
// ============================================================================

uint32_t MidiFileWriter::getBytesWritten() const {
    return bytesWritten_;
}

uint32_t MidiFileWriter::getEventsWritten() const {
    return eventsWritten_;
}

} // namespace midiMind

// ============================================================================
// END OF FILE MidiFileWriter.cpp v4.1.0
// ============================================================================
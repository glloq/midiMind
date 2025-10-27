// ============================================================================
// File: backend/src/midi/MidiValidator.cpp
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of MidiValidator.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.1:
//   - Optimized vector operations
//   - Better memory efficiency
//
// Changes v4.1.0:
//   - Complete validation implementation
//   - Enhanced error messages
//
// ============================================================================

#include "MidiValidator.h"
#include "../core/Logger.h"

namespace midiMind {

// ============================================================================
// NOTE NAMES
// ============================================================================

static const char* NOTE_NAMES[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

MidiValidator::MidiValidator() {
    // Stateless validator
}

// ============================================================================
// MESSAGE VALIDATION
// ============================================================================

ValidationResult MidiValidator::validateMessage(const MidiMessage& message) const {
    ValidationResult result;
    
    // Check if message has data
    if (message.getSize() == 0) {
        addError(result, "message", "Empty message");
        return result;
    }
    
    // Validate using MidiMessage's own validation
    if (!message.isValid()) {
        addError(result, "message", "Invalid MIDI message format");
    }
    
    // Get message properties
    uint8_t status = message.getStatus();
    int channel = message.getChannel();
    
    // Validate status byte
    if (!isValidStatusByte(status)) {
        addError(result, "status", "Invalid status byte: 0x" + 
                std::to_string(status));
    }
    
    // Validate channel for channel messages
    if (message.isChannelMessage() && !isValidChannel(channel)) {
        addError(result, "channel", "Invalid channel: " + std::to_string(channel));
    }
    
    // Validate data bytes
    if (message.getSize() > 1) {
        uint8_t data1 = message.getData1();
        if (!isValidDataByte(data1)) {
            addError(result, "data", "Invalid data byte 1: 0x" + 
                    std::to_string(data1));
        }
    }
    
    if (message.getSize() > 2) {
        uint8_t data2 = message.getData2();
        if (!isValidDataByte(data2)) {
            addError(result, "data", "Invalid data byte 2: 0x" + 
                    std::to_string(data2));
        }
    }
    
    // Type-specific validation
    if (message.isNoteOn() || message.isNoteOff()) {
        int note = message.getData1();
        int velocity = message.getData2();
        
        if (!isValidNote(note)) {
            addError(result, "note", "Invalid note: " + std::to_string(note));
        } else {
            // Check piano range
            if (!isInPianoRange(note)) {
                addWarning(result, "note", 
                          "Note " + getNoteName(note) + " is outside piano range");
            }
            
            // Check audible range
            if (!isInAudibleRange(note)) {
                addWarning(result, "note", 
                          "Note " + getNoteName(note) + " is outside typical audible range");
            }
        }
        
        if (!isValidVelocity(velocity)) {
            addError(result, "velocity", "Invalid velocity: " + std::to_string(velocity));
        }
        
        // Warning for Note On with velocity 0
        if (message.isNoteOn() && velocity == 0) {
            addWarning(result, "velocity", 
                      "Note On with velocity 0 (equivalent to Note Off)");
        }
        
    } else if (message.isControlChange()) {
        int controller = message.getData1();
        int value = message.getData2();
        
        if (!isValidController(controller)) {
            addError(result, "controller", "Invalid controller: " + 
                    std::to_string(controller));
        }
        
        if (!isValidMidiValue(value)) {
            addError(result, "value", "Invalid CC value: " + std::to_string(value));
        }
        
    } else if (message.isProgramChange()) {
        int program = message.getData1();
        
        if (!isValidProgram(program)) {
            addError(result, "program", "Invalid program: " + std::to_string(program));
        }
        
    } else if (message.isPitchBend()) {
        int16_t bend = (message.getData2() << 7) | message.getData1();
        int bendValue = bend - 8192;
        
        if (!isValidPitchBend(bendValue)) {
            addError(result, "pitch_bend", "Invalid pitch bend: " + 
                    std::to_string(bendValue));
        }
    }
    
    return result;
}

ValidationResult MidiValidator::validateRawData(const std::vector<uint8_t>& data) const {
    ValidationResult result;
    
    if (data.empty()) {
        addError(result, "data", "Empty data");
        return result;
    }
    
    // Check status byte
    if (!isValidStatusByte(data[0])) {
        addError(result, "status", "Invalid status byte: 0x" + 
                std::to_string(data[0]));
    }
    
    // Check data bytes
    for (size_t i = 1; i < data.size(); ++i) {
        if (!isValidDataByte(data[i])) {
            addError(result, "data", "Invalid data byte at position " + 
                    std::to_string(i) + ": 0x" + std::to_string(data[i]));
        }
    }
    
    // Create message and validate
    MidiMessage msg(data);
    auto msgResult = validateMessage(msg);
    
    // Merge results efficiently
    result.errors.reserve(result.errors.size() + msgResult.errors.size());
    result.warnings.reserve(result.warnings.size() + msgResult.warnings.size());
    result.infos.reserve(result.infos.size() + msgResult.infos.size());
    
    for (auto& error : msgResult.errors) {
        result.errors.emplace_back(std::move(error));
    }
    for (auto& warning : msgResult.warnings) {
        result.warnings.emplace_back(std::move(warning));
    }
    for (auto& info : msgResult.infos) {
        result.infos.emplace_back(std::move(info));
    }
    
    if (!msgResult.isValid) {
        result.isValid = false;
    }
    
    return result;
}

// ============================================================================
// RANGE VALIDATION
// ============================================================================

ValidationResult MidiValidator::validateNoteRange(int note, int minNote, int maxNote) const {
    ValidationResult result;
    
    if (!isValidNote(note)) {
        addError(result, "note", "Invalid note: " + std::to_string(note) + 
                " (must be 0-127)");
    } else if (note < minNote || note > maxNote) {
        addError(result, "note", "Note " + getNoteName(note) + 
                " is outside allowed range [" + getNoteName(minNote) + 
                " - " + getNoteName(maxNote) + "]");
    }
    
    return result;
}

ValidationResult MidiValidator::validateVelocityRange(int velocity, 
                                                      int minVelocity, 
                                                      int maxVelocity) const {
    ValidationResult result;
    
    if (!isValidVelocity(velocity)) {
        addError(result, "velocity", "Invalid velocity: " + std::to_string(velocity) + 
                " (must be 0-127)");
    } else if (velocity < minVelocity || velocity > maxVelocity) {
        addError(result, "velocity", "Velocity " + std::to_string(velocity) + 
                " is outside allowed range [" + std::to_string(minVelocity) + 
                " - " + std::to_string(maxVelocity) + "]");
    }
    
    return result;
}

// ============================================================================
// UTILITIES
// ============================================================================

std::string MidiValidator::getNoteName(int note) {
    if (note < 0 || note > 127) {
        return "Invalid";
    }
    
    int octave = (note / 12) - 1;
    int noteIndex = note % 12;
    
    return std::string(NOTE_NAMES[noteIndex]) + std::to_string(octave);
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

void MidiValidator::addError(ValidationResult& result,
                            const std::string& category,
                            const std::string& message,
                            const std::string& location) const {
    result.errors.emplace_back(ValidationSeverity::ERROR, category, message, location);
    result.isValid = false;
}

void MidiValidator::addWarning(ValidationResult& result,
                              const std::string& category,
                              const std::string& message,
                              const std::string& location) const {
    result.warnings.emplace_back(ValidationSeverity::WARNING, category, message, location);
}

void MidiValidator::addInfo(ValidationResult& result,
                           const std::string& category,
                           const std::string& message,
                           const std::string& location) const {
    result.infos.emplace_back(ValidationSeverity::INFO, category, message, location);
}

} // namespace midiMind

// ============================================================================
// END OF FILE MidiValidator.cpp
// ============================================================================
// ============================================================================
// File: backend/src/midi/MidiValidator.h
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   MIDI data validation utilities.
//   Validates MIDI messages, files, and data structures.
//
// Features:
//   - MIDI message validation
//   - Value range checking (0-127, channels, etc.)
//   - File format validation
//   - Detailed error reporting
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.1:
//   - Added constexpr and noexcept where appropriate
//   - Optimized static validation methods
//
// Changes v4.1.0:
//   - Enhanced validation rules
//   - Better error messages
//   - Support for all MIDI message types
//
// ============================================================================

#pragma once

#include "MidiMessage.h"
#include <string>
#include <vector>
#include <set>

namespace midiMind {

/**
 * @enum ValidationSeverity
 * @brief Severity levels for validation issues
 */
enum class ValidationSeverity {
    ERROR,      ///< Blocking error (invalid data)
    WARNING,    ///< Warning (valid but suspicious)
    INFO        ///< Information (suggestion)
};

/**
 * @struct ValidationIssue
 * @brief Represents a validation issue
 */
struct ValidationIssue {
    /// Severity level
    ValidationSeverity severity;
    
    /// Category (e.g., "message", "value", "range")
    std::string category;
    
    /// Issue description
    std::string message;
    
    /// Location in data (optional)
    std::string location;
    
    /**
     * @brief Constructor
     */
    ValidationIssue(ValidationSeverity sev,
                   const std::string& cat,
                   const std::string& msg,
                   const std::string& loc = "")
        : severity(sev)
        , category(cat)
        , message(msg)
        , location(loc)
    {}
    
    /**
     * @brief Convert to string
     */
    std::string toString() const {
        std::string prefix;
        switch (severity) {
            case ValidationSeverity::ERROR: prefix = "ERROR"; break;
            case ValidationSeverity::WARNING: prefix = "WARNING"; break;
            case ValidationSeverity::INFO: prefix = "INFO"; break;
        }
        
        std::string result = prefix + ": [" + category + "] " + message;
        if (!location.empty()) {
            result += " (at: " + location + ")";
        }
        return result;
    }
};

/**
 * @struct ValidationResult
 * @brief Result of a validation operation
 */
struct ValidationResult {
    /// Validation passed
    bool isValid;
    
    /// List of errors
    std::vector<ValidationIssue> errors;
    
    /// List of warnings
    std::vector<ValidationIssue> warnings;
    
    /// List of info messages
    std::vector<ValidationIssue> infos;
    
    /**
     * @brief Constructor
     */
    ValidationResult()
        : isValid(true)
    {}
    
    /**
     * @brief Check if has any issues
     */
    bool hasIssues() const noexcept {
        return !errors.empty() || !warnings.empty() || !infos.empty();
    }
    
    /**
     * @brief Get total issue count
     */
    size_t getIssueCount() const noexcept {
        return errors.size() + warnings.size() + infos.size();
    }
    
    /**
     * @brief Get all issues as strings
     */
    std::vector<std::string> getAllMessages() const {
        std::vector<std::string> messages;
        messages.reserve(getIssueCount());
        
        for (const auto& error : errors) {
            messages.push_back(error.toString());
        }
        for (const auto& warning : warnings) {
            messages.push_back(warning.toString());
        }
        for (const auto& info : infos) {
            messages.push_back(info.toString());
        }
        
        return messages;
    }
};

/**
 * @class MidiValidator
 * @brief MIDI data validation utilities
 * 
 * Provides comprehensive validation for MIDI messages and data.
 * 
 * Thread Safety: YES (stateless)
 * 
 * Example:
 * ```cpp
 * MidiValidator validator;
 * 
 * // Validate message
 * auto msg = MidiMessage::noteOn(0, 60, 100);
 * auto result = validator.validateMessage(msg);
 * 
 * if (!result.isValid) {
 *     for (const auto& error : result.errors) {
 *         std::cout << error.toString() << "\n";
 *     }
 * }
 * 
 * // Validate values
 * if (validator.isValidNote(60)) {
 *     // Valid note
 * }
 * ```
 */
class MidiValidator {
public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     */
    MidiValidator();
    
    /**
     * @brief Destructor
     */
    ~MidiValidator() = default;
    
    // ========================================================================
    // MESSAGE VALIDATION
    // ========================================================================
    
    /**
     * @brief Validate MIDI message
     * @param message Message to validate
     * @return ValidationResult Validation result
     */
    ValidationResult validateMessage(const MidiMessage& message) const;
    
    /**
     * @brief Validate raw MIDI data
     * @param data Raw MIDI bytes
     * @return ValidationResult Validation result
     */
    ValidationResult validateRawData(const std::vector<uint8_t>& data) const;
    
    // ========================================================================
    // VALUE VALIDATION
    // ========================================================================
    
    /**
     * @brief Check if value is valid 7-bit MIDI value (0-127)
     * @param value Value to check
     * @return true if valid
     */
    static constexpr bool isValidMidiValue(int value) noexcept {
        return value >= 0 && value <= 127;
    }
    
    /**
     * @brief Check if note number is valid (0-127)
     * @param note Note number
     * @return true if valid
     */
    static constexpr bool isValidNote(int note) noexcept {
        return isValidMidiValue(note);
    }
    
    /**
     * @brief Check if velocity is valid (0-127)
     * @param velocity Velocity value
     * @return true if valid
     */
    static constexpr bool isValidVelocity(int velocity) noexcept {
        return isValidMidiValue(velocity);
    }
    
    /**
     * @brief Check if channel is valid (0-15)
     * @param channel Channel number
     * @return true if valid
     */
    static constexpr bool isValidChannel(int channel) noexcept {
        return channel >= 0 && channel <= 15;
    }
    
    /**
     * @brief Check if controller number is valid (0-127)
     * @param controller Controller number
     * @return true if valid
     */
    static constexpr bool isValidController(int controller) noexcept {
        return isValidMidiValue(controller);
    }
    
    /**
     * @brief Check if program number is valid (0-127)
     * @param program Program number
     * @return true if valid
     */
    static constexpr bool isValidProgram(int program) noexcept {
        return isValidMidiValue(program);
    }
    
    /**
     * @brief Check if pitch bend value is valid (-8192 to 8191)
     * @param value Pitch bend value
     * @return true if valid
     */
    static constexpr bool isValidPitchBend(int value) noexcept {
        return value >= -8192 && value <= 8191;
    }
    
    /**
     * @brief Check if status byte is valid (0x80-0xFF)
     * @param status Status byte
     * @return true if valid
     */
    static constexpr bool isValidStatusByte(uint8_t status) noexcept {
        return status >= 0x80;
    }
    
    /**
     * @brief Check if data byte is valid (0x00-0x7F)
     * @param data Data byte
     * @return true if valid
     */
    static constexpr bool isValidDataByte(uint8_t data) noexcept {
        return data <= 0x7F;
    }
    
    // ========================================================================
    // RANGE VALIDATION
    // ========================================================================
    
    /**
     * @brief Validate note range
     * @param note Note number
     * @param minNote Minimum note (default: 0)
     * @param maxNote Maximum note (default: 127)
     * @return ValidationResult Validation result
     */
    ValidationResult validateNoteRange(int note, int minNote = 0, int maxNote = 127) const;
    
    /**
     * @brief Validate velocity range
     * @param velocity Velocity value
     * @param minVelocity Minimum velocity (default: 1)
     * @param maxVelocity Maximum velocity (default: 127)
     * @return ValidationResult Validation result
     */
    ValidationResult validateVelocityRange(int velocity, 
                                          int minVelocity = 1, 
                                          int maxVelocity = 127) const;
    
    // ========================================================================
    // SEMANTIC VALIDATION
    // ========================================================================
    
    /**
     * @brief Check if note is in standard piano range (21-108, A0-C8)
     * @param note Note number
     * @return true if in piano range
     */
    static constexpr bool isInPianoRange(int note) noexcept {
        return note >= 21 && note <= 108;
    }
    
    /**
     * @brief Check if note is in audible range (approximately)
     * @param note Note number
     * @return true if likely audible
     */
    static constexpr bool isInAudibleRange(int note) noexcept {
        return note >= 12 && note <= 120;  // ~16Hz to ~8.4kHz
    }
    
    /**
     * @brief Get note name
     * @param note Note number (0-127)
     * @return std::string Note name (e.g., "C4", "A#5")
     */
    static std::string getNoteName(int note);
    
private:
    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    /**
     * @brief Add error to result
     */
    void addError(ValidationResult& result,
                 const std::string& category,
                 const std::string& message,
                 const std::string& location = "") const;
    
    /**
     * @brief Add warning to result
     */
    void addWarning(ValidationResult& result,
                   const std::string& category,
                   const std::string& message,
                   const std::string& location = "") const;
    
    /**
     * @brief Add info to result
     */
    void addInfo(ValidationResult& result,
                const std::string& category,
                const std::string& message,
                const std::string& location = "") const;
};

} // namespace midiMind
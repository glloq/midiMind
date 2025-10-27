// ============================================================================
// File: backend/src/midi/sysex/SysExParser.h
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Parser for SysEx messages. Handles standard MIDI SysEx (Identity) and
//   Custom SysEx protocol (0x7D). Focuses on Blocks 1-2 for v4.1.0.
//
// Dependencies:
//   - None (header-only utilities, implementation in .cpp)
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.1:
//   - Added bounds checking to hasFeature()
//   - Improved documentation for findMapping() lifetime
//
// Changes v4.1.0:
//   - Simplified to Blocks 1-2 only
//   - Added static helper methods
//   - Removed unused block parsers
//
// ============================================================================

#pragma once

#include <vector>
#include <string>
#include <optional>
#include <cstdint>

namespace midiMind {

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @struct DeviceIdentity
 * @brief Standard MIDI Device Identity (Universal SysEx)
 */
struct DeviceIdentity {
    uint16_t manufacturerId;    // MIDI Manufacturer ID (0x00-0x7F or extended)
    uint16_t familyCode;        // Device family (0x0000-0x3FFF)
    uint16_t modelNumber;       // Model number (0x0000-0x3FFF)
    uint8_t versionMajor;       // Firmware version major
    uint8_t versionMinor;       // Firmware version minor
    uint8_t versionPatch;       // Firmware version patch
    uint8_t versionBuild;       // Firmware version build
    
    /**
     * @brief Convert to human-readable string
     * @return String representation
     */
    std::string toString() const;
};

/**
 * @struct CustomDeviceIdentity
 * @brief Custom Device Identity (Block 1)
 */
struct CustomDeviceIdentity {
    uint8_t blockVersion;           // Block format version
    uint32_t deviceId;              // Unique device ID (32-bit)
    std::string deviceName;         // Device name (max 32 chars)
    uint8_t firmwareVersion[3];     // Firmware [major, minor, patch]
    uint32_t featureFlags;          // Supported features bitmask
    
    /**
     * @brief Convert to human-readable string
     * @return String representation
     */
    std::string toString() const;
    
    /**
     * @brief Check if feature is supported
     * @param bit Feature bit (0-31)
     * @return True if feature enabled, false if bit out of range or disabled
     */
    bool hasFeature(uint8_t bit) const {
        if (bit >= 32) {
            return false;  // Out of range
        }
        return (featureFlags & (1u << bit)) != 0;
    }
};

/**
 * @struct NoteMappingEntry
 * @brief Single note mapping entry
 */
struct NoteMappingEntry {
    uint8_t midiNote;       // MIDI note number (0-127)
    uint8_t channel;        // MIDI channel (0-15)
    std::string name;       // Human-readable name (e.g., "Kick", "Snare")
    uint8_t velocity;       // Default velocity (0-127)
};

/**
 * @struct NoteMap
 * @brief Complete note mapping (Block 2)
 */
struct NoteMap {
    uint8_t blockVersion;                   // Block format version
    uint8_t minNote;                        // Minimum supported note
    uint8_t maxNote;                        // Maximum supported note
    uint8_t polyphony;                      // Max simultaneous notes
    std::vector<NoteMappingEntry> mappings; // Note mappings
    
    /**
     * @brief Convert to human-readable string
     * @return String representation
     */
    std::string toString() const;
    
    /**
     * @brief Find mapping for a MIDI note
     * @param note MIDI note number
     * @return Pointer to mapping or nullptr
     * @warning Pointer is valid only while NoteMap exists and mappings vector is not modified
     */
    const NoteMappingEntry* findMapping(uint8_t note) const;
};

// ============================================================================
// SYSEX PARSER
// ============================================================================

/**
 * @class SysExParser
 * @brief Static parser for SysEx messages
 * 
 * @details
 * Provides static methods to parse various SysEx message types.
 * All methods are thread-safe (no internal state).
 */
class SysExParser {
public:
    // ========================================================================
    // MESSAGE TYPE DETECTION
    // ========================================================================
    
    /**
     * @brief Check if data is valid SysEx message
     * @param data Raw MIDI data
     * @return True if valid SysEx (starts with F0, ends with F7)
     */
    static bool isValidSysEx(const std::vector<uint8_t>& data);
    
    /**
     * @brief Check if message is Identity Reply
     * @param data Raw SysEx data
     * @return True if Identity Reply (F0 7E ... 06 02 ...)
     */
    static bool isIdentityReply(const std::vector<uint8_t>& data);
    
    /**
     * @brief Check if message is Custom SysEx (0x7D)
     * @param data Raw SysEx data
     * @return True if Custom SysEx
     */
    static bool isCustomSysEx(const std::vector<uint8_t>& data);
    
    /**
     * @brief Get Custom SysEx block ID
     * @param data Raw SysEx data
     * @return Block ID (1-8) or nullopt if invalid
     */
    static std::optional<uint8_t> getCustomBlockId(const std::vector<uint8_t>& data);
    
    /**
     * @brief Get Custom SysEx block version
     * @param data Raw SysEx data
     * @return Block version or nullopt if invalid
     */
    static std::optional<uint8_t> getCustomBlockVersion(const std::vector<uint8_t>& data);
    
    // ========================================================================
    // STANDARD SYSEX PARSING
    // ========================================================================
    
    /**
     * @brief Parse Identity Reply message
     * @param data Raw SysEx data (F0 7E ... 06 02 ... F7)
     * @return DeviceIdentity or nullopt if parsing fails
     * 
     * @details Format:
     *   F0 7E <device> 06 02 <manufacturer> <family> <model> <version> F7
     */
    static std::optional<DeviceIdentity> parseIdentityReply(
        const std::vector<uint8_t>& data);
    
    // ========================================================================
    // CUSTOM SYSEX PARSING (BLOCKS 1-2)
    // ========================================================================
    
    /**
     * @brief Parse Custom Identification (Block 1)
     * @param data Raw SysEx data (F0 7D 00 01 01 ... F7)
     * @return CustomDeviceIdentity or nullopt if parsing fails
     * 
     * @details Format:
     *   F0 7D 00 01 01 <version> <id[4]> <name[32]> <fw[3]> <features[4]> F7
     */
    static std::optional<CustomDeviceIdentity> parseCustomIdentification(
        const std::vector<uint8_t>& data);
    
    /**
     * @brief Parse Note Map (Block 2)
     * @param data Raw SysEx data (F0 7D 00 02 01 ... F7)
     * @return NoteMap or nullopt if parsing fails
     * 
     * @details Format:
     *   F0 7D 00 02 01 <version> <min> <max> <poly> <count> [entries...] F7
     */
    static std::optional<NoteMap> parseNoteMap(
        const std::vector<uint8_t>& data);
    
private:
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    /**
     * @brief Decode 7-bit encoded 32-bit value
     * @param data Source data
     * @param offset Offset to start reading
     * @return Decoded 32-bit value, or 0 if insufficient data
     * 
     * @details MIDI SysEx uses 7-bit encoding (MSB = 0)
     *   5 bytes of 7-bit data = 35 bits (we use 32 bits)
     */
    static uint32_t decode7BitTo32Bit(const std::vector<uint8_t>& data, 
                                      size_t offset);
    
    /**
     * @brief Extract null-terminated string
     * @param data Source data
     * @param offset Offset to start reading
     * @param maxLen Maximum string length
     * @return Extracted string (printable ASCII only)
     */
    static std::string extractString(const std::vector<uint8_t>& data,
                                    size_t offset,
                                    size_t maxLen);
};

// ============================================================================
// SYSEX BUILDER (for requests)
// ============================================================================

/**
 * @class SysExBuilder
 * @brief Helper to build SysEx request messages
 */
class SysExBuilder {
public:
    /**
     * @brief Build Identity Request
     * @return Raw SysEx data (F0 7E 7F 06 01 F7)
     */
    static std::vector<uint8_t> buildIdentityRequest();
    
    /**
     * @brief Build Custom Identification Request (Block 1)
     * @return Raw SysEx data (F0 7D 00 01 00 F7)
     */
    static std::vector<uint8_t> buildCustomIdentificationRequest();
    
    /**
     * @brief Build Note Map Request (Block 2)
     * @return Raw SysEx data (F0 7D 00 02 00 F7)
     */
    static std::vector<uint8_t> buildNoteMapRequest();
    
private:
    /**
     * @brief Build generic Custom SysEx request
     * @param blockId Block ID (1-8)
     * @return Raw SysEx data
     */
    static std::vector<uint8_t> buildCustomRequest(uint8_t blockId);
};

} // namespace midiMind
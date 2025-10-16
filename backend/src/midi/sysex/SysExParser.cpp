// ============================================================================
// File: backend/src/midi/sysex/SysExParser.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of SysEx parser for standard and custom messages.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// ============================================================================

#include "SysExParser.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace midiMind {

// ============================================================================
// DEVICE IDENTITY
// ============================================================================

std::string DeviceIdentity::toString() const {
    std::stringstream ss;
    ss << "Manufacturer: 0x" << std::hex << std::setw(4) << std::setfill('0') 
       << manufacturerId
       << ", Family: 0x" << std::setw(4) << familyCode
       << ", Model: 0x" << std::setw(4) << modelNumber
       << ", Version: " << std::dec 
       << static_cast<int>(versionMajor) << "."
       << static_cast<int>(versionMinor) << "."
       << static_cast<int>(versionPatch) << "."
       << static_cast<int>(versionBuild);
    return ss.str();
}

// ============================================================================
// CUSTOM DEVICE IDENTITY
// ============================================================================

std::string CustomDeviceIdentity::toString() const {
    std::stringstream ss;
    ss << "Name: '" << deviceName << "'"
       << ", ID: 0x" << std::hex << std::setw(8) << std::setfill('0') << deviceId
       << ", Firmware: " << std::dec
       << static_cast<int>(firmwareVersion[0]) << "."
       << static_cast<int>(firmwareVersion[1]) << "."
       << static_cast<int>(firmwareVersion[2])
       << ", Features: 0x" << std::hex << std::setw(8) << featureFlags;
    return ss.str();
}

// ============================================================================
// NOTE MAP
// ============================================================================

std::string NoteMap::toString() const {
    std::stringstream ss;
    ss << "Note Range: " << static_cast<int>(minNote) 
       << "-" << static_cast<int>(maxNote)
       << ", Polyphony: " << static_cast<int>(polyphony)
       << ", Mappings: " << mappings.size();
    return ss.str();
}

const NoteMappingEntry* NoteMap::findMapping(uint8_t note) const {
    auto it = std::find_if(mappings.begin(), mappings.end(),
        [note](const NoteMappingEntry& entry) {
            return entry.midiNote == note;
        });
    
    return (it != mappings.end()) ? &(*it) : nullptr;
}

// ============================================================================
// MESSAGE TYPE DETECTION
// ============================================================================

bool SysExParser::isValidSysEx(const std::vector<uint8_t>& data) {
    return data.size() >= 3 && 
           data[0] == 0xF0 && 
           data[data.size() - 1] == 0xF7;
}

bool SysExParser::isIdentityReply(const std::vector<uint8_t>& data) {
    // F0 7E <device> 06 02 ...
    return isValidSysEx(data) &&
           data.size() >= 6 &&
           data[1] == 0x7E &&  // Universal Non-Real Time
           data[3] == 0x06 &&  // General Information
           data[4] == 0x02;    // Identity Reply
}

bool SysExParser::isCustomSysEx(const std::vector<uint8_t>& data) {
    // F0 7D 00 <block> ...
    return isValidSysEx(data) &&
           data.size() >= 5 &&
           data[1] == 0x7D &&  // Educational/Development
           data[2] == 0x00;    // MidiMind Manufacturer ID
}

std::optional<uint8_t> SysExParser::getCustomBlockId(
    const std::vector<uint8_t>& data) {
    if (!isCustomSysEx(data)) {
        return std::nullopt;
    }
    
    uint8_t blockId = data[3];
    if (blockId < 1 || blockId > 8) {
        return std::nullopt;
    }
    
    return blockId;
}

std::optional<uint8_t> SysExParser::getCustomBlockVersion(
    const std::vector<uint8_t>& data) {
    if (!isCustomSysEx(data) || data.size() < 6) {
        return std::nullopt;
    }
    
    // Block version is after block ID and request/reply flag
    return data[5];
}

// ============================================================================
// STANDARD SYSEX PARSING
// ============================================================================

std::optional<DeviceIdentity> SysExParser::parseIdentityReply(
    const std::vector<uint8_t>& data) {
    
    if (!isIdentityReply(data) || data.size() < 15) {
        return std::nullopt;
    }
    
    DeviceIdentity identity;
    
    // Parse manufacturer ID (1 or 3 bytes)
    size_t pos = 5;
    if (data[pos] == 0x00) {
        // Extended manufacturer ID (3 bytes)
        if (data.size() < 17) return std::nullopt;
        identity.manufacturerId = (data[pos] << 14) | 
                                 (data[pos + 1] << 7) | 
                                 data[pos + 2];
        pos += 3;
    } else {
        // Standard manufacturer ID (1 byte)
        identity.manufacturerId = data[pos];
        pos += 1;
    }
    
    // Parse family code (2 bytes, LSB first)
    if (pos + 1 >= data.size()) return std::nullopt;
    identity.familyCode = data[pos] | (data[pos + 1] << 7);
    pos += 2;
    
    // Parse model number (2 bytes, LSB first)
    if (pos + 1 >= data.size()) return std::nullopt;
    identity.modelNumber = data[pos] | (data[pos + 1] << 7);
    pos += 2;
    
    // Parse version (4 bytes)
    if (pos + 3 >= data.size()) return std::nullopt;
    identity.versionMajor = data[pos++];
    identity.versionMinor = data[pos++];
    identity.versionPatch = data[pos++];
    identity.versionBuild = data[pos];
    
    return identity;
}

// ============================================================================
// CUSTOM SYSEX PARSING
// ============================================================================

std::optional<CustomDeviceIdentity> SysExParser::parseCustomIdentification(
    const std::vector<uint8_t>& data) {
    
    // Format: F0 7D 00 01 01 <version> <id[4]> <name[32]> <fw[3]> <features[4]> F7
    // Minimum size: 1 + 1 + 1 + 1 + 1 + 1 + 4 + 32 + 3 + 4 + 1 = 50 bytes
    
    if (!isCustomSysEx(data) || data.size() < 50) {
        return std::nullopt;
    }
    
    if (data[3] != 0x01) {  // Block 1
        return std::nullopt;
    }
    
    if (data[4] != 0x01) {  // Reply flag
        return std::nullopt;
    }
    
    CustomDeviceIdentity identity;
    size_t pos = 5;
    
    // Block version
    identity.blockVersion = data[pos++];
    
    // Device ID (32-bit, 7-bit encoded = 5 bytes)
    identity.deviceId = decode7BitTo32Bit(data, pos);
    pos += 5;
    
    // Device name (null-terminated, max 32 chars)
    identity.deviceName = extractString(data, pos, 32);
    pos += 32;
    
    // Firmware version (3 bytes)
    identity.firmwareVersion[0] = data[pos++];
    identity.firmwareVersion[1] = data[pos++];
    identity.firmwareVersion[2] = data[pos++];
    
    // Feature flags (32-bit, 7-bit encoded = 5 bytes)
    identity.featureFlags = decode7BitTo32Bit(data, pos);
    
    return identity;
}

std::optional<NoteMap> SysExParser::parseNoteMap(
    const std::vector<uint8_t>& data) {
    
    // Format: F0 7D 00 02 01 <version> <min> <max> <poly> <count> [entries...] F7
    // Minimum size: 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 = 11 bytes
    
    if (!isCustomSysEx(data) || data.size() < 11) {
        return std::nullopt;
    }
    
    if (data[3] != 0x02) {  // Block 2
        return std::nullopt;
    }
    
    if (data[4] != 0x01) {  // Reply flag
        return std::nullopt;
    }
    
    NoteMap noteMap;
    size_t pos = 5;
    
    // Block version
    noteMap.blockVersion = data[pos++];
    
    // Note range
    noteMap.minNote = data[pos++];
    noteMap.maxNote = data[pos++];
    
    // Polyphony
    noteMap.polyphony = data[pos++];
    
    // Entry count
    uint8_t count = data[pos++];
    
    // Parse entries
    // Each entry: <note> <channel> <nameLen> <name...> <velocity>
    noteMap.mappings.reserve(count);
    
    for (uint8_t i = 0; i < count && pos < data.size() - 1; ++i) {
        NoteMappingEntry entry;
        
        // MIDI note
        if (pos >= data.size() - 1) break;
        entry.midiNote = data[pos++];
        
        // Channel
        if (pos >= data.size() - 1) break;
        entry.channel = data[pos++];
        
        // Name length
        if (pos >= data.size() - 1) break;
        uint8_t nameLen = data[pos++];
        
        // Name
        if (nameLen > 0) {
            entry.name = extractString(data, pos, nameLen);
            pos += nameLen;
        }
        
        // Velocity
        if (pos >= data.size() - 1) break;
        entry.velocity = data[pos++];
        
        noteMap.mappings.push_back(entry);
    }
    
    return noteMap;
}

// ============================================================================
// HELPERS
// ============================================================================

uint32_t SysExParser::decode7BitTo32Bit(const std::vector<uint8_t>& data,
                                        size_t offset) {
    if (offset + 4 >= data.size()) {
        return 0;
    }
    
    // 5 bytes of 7-bit data = 35 bits (we use 32 bits)
    uint32_t value = 0;
    value |= static_cast<uint32_t>(data[offset] & 0x7F);
    value |= static_cast<uint32_t>(data[offset + 1] & 0x7F) << 7;
    value |= static_cast<uint32_t>(data[offset + 2] & 0x7F) << 14;
    value |= static_cast<uint32_t>(data[offset + 3] & 0x7F) << 21;
    value |= static_cast<uint32_t>(data[offset + 4] & 0x07) << 28;  // Only 3 bits needed
    
    return value;
}

std::string SysExParser::extractString(const std::vector<uint8_t>& data,
                                       size_t offset,
                                       size_t maxLen) {
    std::string result;
    result.reserve(maxLen);
    
    for (size_t i = 0; i < maxLen && (offset + i) < data.size() - 1; ++i) {
        uint8_t c = data[offset + i];
        if (c == 0 || c == 0xF7) {
            break;
        }
        if (c >= 32 && c < 127) {  // Printable ASCII
            result.push_back(static_cast<char>(c));
        }
    }
    
    return result;
}

// ============================================================================
// SYSEX BUILDER
// ============================================================================

std::vector<uint8_t> SysExBuilder::buildIdentityRequest() {
    return {
        0xF0,  // SysEx Start
        0x7E,  // Universal Non-Real Time
        0x7F,  // All devices
        0x06,  // General Information
        0x01,  // Identity Request
        0xF7   // SysEx End
    };
}

std::vector<uint8_t> SysExBuilder::buildCustomIdentificationRequest() {
    return buildCustomRequest(0x01);
}

std::vector<uint8_t> SysExBuilder::buildNoteMapRequest() {
    return buildCustomRequest(0x02);
}

std::vector<uint8_t> SysExBuilder::buildCustomRequest(uint8_t blockId) {
    return {
        0xF0,     // SysEx Start
        0x7D,     // Educational/Development use
        0x00,     // MidiMind Manufacturer ID
        blockId,  // Block ID (1-8)
        0x00,     // Request flag (0x00 = request, 0x01 = reply)
        0xF7      // SysEx End
    };
}

} // namespace midiMind
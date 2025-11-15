// ============================================================================
// File: backend/src/midi/sysex/MidiManufacturers.h
// Version: 1.0.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   MIDI Manufacturer ID to name mapping for device identification.
//   Based on MIDI Manufacturers Association (MMA) official list.
//
// Author: MidiMind Team
// Date: 2025-11-15
//
// ============================================================================

#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

namespace midiMind {

/**
 * @class MidiManufacturers
 * @brief Mapping of MIDI Manufacturer IDs to company names
 *
 * @details
 * MIDI Manufacturer IDs can be:
 * - Standard (1 byte): 0x01-0x7F
 * - Extended (3 bytes): 0x00 XX XX
 *
 * This class provides lookup functionality to get manufacturer names
 * from their IDs, useful for device identification via SysEx.
 */
class MidiManufacturers {
public:
    /**
     * @brief Get manufacturer name from ID
     * @param id Manufacturer ID (1-byte or extended)
     * @return Manufacturer name or "Unknown (0xXXXX)"
     */
    static std::string getName(uint16_t id);

    /**
     * @brief Check if manufacturer ID is known
     * @param id Manufacturer ID
     * @return True if manufacturer is in database
     */
    static bool isKnown(uint16_t id);

private:
    // Standard IDs (1 byte: 0x01-0x7F)
    static const std::unordered_map<uint16_t, std::string> manufacturers_;
};

} // namespace midiMind

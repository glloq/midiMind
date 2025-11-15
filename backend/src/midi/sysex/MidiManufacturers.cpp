// ============================================================================
// File: backend/src/midi/sysex/MidiManufacturers.cpp
// Version: 1.0.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#include "MidiManufacturers.h"
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// MANUFACTURER DATABASE
// ============================================================================

const std::unordered_map<uint16_t, std::string> MidiManufacturers::manufacturers_ = {
    // ========== Standard IDs (1 byte) ==========

    // Japanese manufacturers (0x40-0x5F)
    {0x40, "Kawai"},
    {0x41, "Roland"},
    {0x42, "Korg"},
    {0x43, "Yamaha"},
    {0x44, "Casio"},
    {0x47, "Akai"},
    {0x4C, "Sony"},
    {0x4E, "Teac"},
    {0x54, "Matsushita Electric (Technics)"},
    {0x57, "Fostex"},
    {0x5A, "Zoom"},

    // American manufacturers (0x01-0x1F)
    {0x01, "Sequential"},
    {0x02, "IDP (Big Briar)"},
    {0x03, "Voyetra (Octave-Plateau)"},
    {0x04, "Moog"},
    {0x05, "Passport Designs"},
    {0x06, "Lexicon"},
    {0x07, "Kurzweil"},
    {0x08, "Fender"},
    {0x09, "Gulbransen"},
    {0x0A, "AKG Acoustics"},
    {0x0B, "Voyce Music"},
    {0x0C, "Waveframe"},
    {0x0D, "ADA"},
    {0x0E, "Garfield Electronics"},
    {0x0F, "Ensoniq"},
    {0x10, "Oberheim"},
    {0x11, "Apple"},
    {0x12, "Grey Matter Response"},
    {0x13, "Digidesign"},
    {0x14, "Palmtree Instruments"},
    {0x15, "JL Cooper"},
    {0x16, "Lowrey"},
    {0x17, "Adams-Smith"},
    {0x18, "Emu Systems"},
    {0x19, "Harmony Systems"},
    {0x1A, "ART"},
    {0x1B, "Baldwin"},
    {0x1C, "Eventide"},
    {0x1D, "Inventronics"},
    {0x1F, "Clarity"},

    // European manufacturers (0x20-0x3F)
    {0x20, "Passac"},
    {0x21, "SIEL"},
    {0x22, "Synthaxe"},
    {0x24, "Hohner"},
    {0x25, "Twister"},
    {0x26, "Solton"},
    {0x27, "Jellinghaus MS"},
    {0x28, "Southworth Music Systems"},
    {0x29, "PPG"},
    {0x2A, "JEN"},
    {0x2B, "Solid State Logic"},
    {0x2C, "Audio Veritrieb"},
    {0x2D, "Hinton Instruments"},
    {0x2E, "Soundtracs"},
    {0x2F, "Elka"},
    {0x30, "Dynacord"},
    {0x31, "Viscount"},
    {0x33, "Clavia (Nord)"},
    {0x36, "Cheetah"},
    {0x3E, "Waldorf"},

    // Others
    {0x7D, "Educational/DIY"},  // Non-commercial use
    {0x7E, "Universal Non-Real Time"},
    {0x7F, "Universal Real Time"},

    // ========== Extended IDs (3 bytes: 0x00 XX XX) ==========
    // Format: (0x00 << 14) | (byte2 << 7) | byte3

    // American extended (0x00 0x00-0x1F XX)
    {0x0000, "Warner Bros"},
    {0x0001, "Unique Technologies"},
    {0x0002, "Moog Music"},
    {0x0007, "Digital Music Corporation"},
    {0x000F, "Alesis"},
    {0x0013, "Digidesign"},
    {0x0015, "Jellinghaus MS"},
    {0x0016, "Peavey"},
    {0x001C, "Numark"},
    {0x0020, "Presonus"},

    // European extended (0x00 0x20-0x3F XX)
    {0x2000, "Dream"},
    {0x2002, "Quasimidi"},
    {0x2007, "Allen & Heath"},
    {0x2009, "Akai"},
    {0x2010, "Sequencer Systems"},
    {0x2011, "Viscount"},
    {0x2015, "Novation"},
    {0x2029, "Focusrite/Novation"},
    {0x202B, "Behringer"},
    {0x2032, "TC Electronic"},
    {0x2033, "Assmann"},
    {0x203A, "Realsound"},

    // Japanese extended (0x00 0x40-0x5F XX)
    {0x4000, "Crimson Technology"},
    {0x4003, "Akai"},
    {0x400B, "Roland"},
    {0x400C, "Korg"},
    {0x400D, "Yamaha"},
    {0x4010, "Elektron"},
    {0x4013, "Quasar"},
    {0x4015, "M-Audio"},
    {0x4016, "Vermona"},
    {0x4020, "Arturia"},
    {0x4027, "Cakewalk"},
    {0x4029, "Native Instruments"},
    {0x4033, "Elektron"},
    {0x4034, "Mutable Instruments"},
    {0x4041, "Teenage Engineering"},
    {0x4049, "Singular Sound"},
};

// ============================================================================
// PUBLIC METHODS
// ============================================================================

std::string MidiManufacturers::getName(uint16_t id) {
    auto it = manufacturers_.find(id);
    if (it != manufacturers_.end()) {
        return it->second;
    }

    // Unknown manufacturer - return hex ID
    std::stringstream ss;
    ss << "Unknown (0x" << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << id << ")";
    return ss.str();
}

bool MidiManufacturers::isKnown(uint16_t id) {
    return manufacturers_.find(id) != manufacturers_.end();
}

} // namespace midiMind

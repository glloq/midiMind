// ============================================================================
// Fichier: src/midi/sysex/UniversalSysEx.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Constantes et définitions pour les messages Universal System Exclusive
//   selon la spécification MIDI (Universal SysEx Messages).
//
// Référence: MIDI 1.0 Detailed Specification - Universal System Exclusive Messages
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <cstdint>

namespace midiMind {
namespace SysEx {

// ============================================================================
// CONSTANTES DE BASE
// ============================================================================

/// Start of Exclusive (SOX)
constexpr uint8_t SOX = 0xF0;

/// End of Exclusive (EOX)
constexpr uint8_t EOX = 0xF7;

// ============================================================================
// UNIVERSAL SYSTEM EXCLUSIVE
// ============================================================================

/// Universal Real Time SysEx (Real-time messages)
constexpr uint8_t UNIVERSAL_REALTIME = 0x7F;

/// Universal Non-Real Time SysEx (Non-realtime messages)
constexpr uint8_t UNIVERSAL_NON_REALTIME = 0x7E;

// ============================================================================
// SUB-ID #1 (CATÉGORIES DE MESSAGES)
// ============================================================================

namespace RealTime {
    constexpr uint8_t MIDI_TIME_CODE = 0x01;
    constexpr uint8_t MIDI_SHOW_CONTROL = 0x02;
    constexpr uint8_t NOTATION_INFO = 0x03;
    constexpr uint8_t DEVICE_CONTROL = 0x04;
    constexpr uint8_t REALTIME_MTC_CUEING = 0x05;
    constexpr uint8_t MIDI_MACHINE_CONTROL = 0x06;
    constexpr uint8_t MIDI_MACHINE_CONTROL_RESPONSES = 0x07;
    constexpr uint8_t MIDI_TUNING_STANDARD = 0x08;
    constexpr uint8_t CONTROLLER_DESTINATION = 0x09;
    constexpr uint8_t KEY_BASED_INSTRUMENT = 0x0A;
    constexpr uint8_t SCALABLE_POLYPHONY_MIP = 0x0B;
    constexpr uint8_t MOBILE_PHONE_CONTROL = 0x0C;
}

namespace NonRealTime {
    constexpr uint8_t SAMPLE_DUMP_HEADER = 0x01;
    constexpr uint8_t SAMPLE_DATA_PACKET = 0x02;
    constexpr uint8_t SAMPLE_DUMP_REQUEST = 0x03;
    constexpr uint8_t MIDI_TIME_CODE = 0x04;
    constexpr uint8_t SAMPLE_DUMP_EXTENSIONS = 0x05;
    constexpr uint8_t GENERAL_INFO = 0x06;
    constexpr uint8_t FILE_DUMP = 0x07;
    constexpr uint8_t MIDI_TUNING_STANDARD = 0x08;
    constexpr uint8_t GENERAL_MIDI = 0x09;
    constexpr uint8_t DOWNLOADABLE_SOUNDS = 0x0A;
    constexpr uint8_t FILE_REFERENCE = 0x0B;
    constexpr uint8_t MIDI_VISUAL_CONTROL = 0x0C;
    constexpr uint8_t MIDI_CAPABILITY_INQUIRY = 0x0D;
    constexpr uint8_t END_OF_FILE = 0x7B;
    constexpr uint8_t WAIT = 0x7C;
    constexpr uint8_t CANCEL = 0x7D;
    constexpr uint8_t NAK = 0x7E;
    constexpr uint8_t ACK = 0x7F;
}

// ============================================================================
// GENERAL INFORMATION (SUB-ID #2)
// ============================================================================

namespace GeneralInfo {
    constexpr uint8_t IDENTITY_REQUEST = 0x01;
    constexpr uint8_t IDENTITY_REPLY = 0x02;
}

// ============================================================================
// DEVICE CONTROL (SUB-ID #2)
// ============================================================================

namespace DeviceControl {
    constexpr uint8_t MASTER_VOLUME = 0x01;
    constexpr uint8_t MASTER_BALANCE = 0x02;
    constexpr uint8_t MASTER_FINE_TUNING = 0x03;
    constexpr uint8_t MASTER_COARSE_TUNING = 0x04;
    constexpr uint8_t GLOBAL_PARAMETER_CONTROL = 0x05;
}

// ============================================================================
// GENERAL MIDI (SUB-ID #2)
// ============================================================================

namespace GeneralMidi {
    constexpr uint8_t GM_SYSTEM_ON = 0x01;
    constexpr uint8_t GM_SYSTEM_OFF = 0x02;
    constexpr uint8_t GM2_SYSTEM_ON = 0x03;
}

// ============================================================================
// DEVICE ID
// ============================================================================

/// All devices (broadcast)
constexpr uint8_t DEVICE_ID_ALL = 0x7F;

// ============================================================================
// MANUFACTURER IDs (Exemples courants)
// ============================================================================

namespace ManufacturerId {
    // American Group
    constexpr uint8_t SEQUENTIAL_CIRCUITS = 0x01;
    constexpr uint8_t BIG_BRIAR = 0x02;
    constexpr uint8_t OCTAVE_PLATEAU = 0x03;
    constexpr uint8_t MOOG = 0x04;
    constexpr uint8_t PASSPORT_DESIGNS = 0x05;
    constexpr uint8_t LEXICON = 0x06;
    constexpr uint8_t KURZWEIL = 0x07;
    constexpr uint8_t FENDER = 0x08;
    constexpr uint8_t GULBRANSEN = 0x09;
    constexpr uint8_t AKG_ACOUSTICS = 0x0A;
    constexpr uint8_t VOYCE_MUSIC = 0x0B;
    constexpr uint8_t WAVEFRAME = 0x0C;
    constexpr uint8_t ADA = 0x0D;
    constexpr uint8_t GARFIELD = 0x0E;
    constexpr uint8_t ENSONIQ = 0x0F;
    
    // European Group
    constexpr uint8_t OBERHEIM = 0x10;
    constexpr uint8_t APPLE = 0x11;
    constexpr uint8_t GREY_MATTER = 0x12;
    
    // Extended IDs (3 bytes)
    constexpr uint8_t EXTENDED_ID = 0x00;
    
    // Quelques IDs étendus populaires
    namespace Extended {
        // Format: 0x00 + 2 bytes
        constexpr uint8_t AMERICAN_GROUP = 0x00;
        constexpr uint8_t EUROPEAN_GROUP = 0x20;
        constexpr uint8_t JAPANESE_GROUP = 0x40;
        constexpr uint8_t OTHER_GROUP = 0x7D;
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Vérifie si un byte est un statut SysEx valide
 */
inline bool isSysExStart(uint8_t byte) {
    return byte == SOX;
}

/**
 * @brief Vérifie si un byte est une fin de SysEx
 */
inline bool isSysExEnd(uint8_t byte) {
    return byte == EOX;
}

/**
 * @brief Vérifie si c'est un Universal SysEx
 */
inline bool isUniversalSysEx(uint8_t manufacturerId) {
    return manufacturerId == UNIVERSAL_REALTIME || 
           manufacturerId == UNIVERSAL_NON_REALTIME;
}

/**
 * @brief Vérifie si c'est un ID manufacturier étendu (3 bytes)
 */
inline bool isExtendedManufacturerId(uint8_t firstByte) {
    return firstByte == ManufacturerId::EXTENDED_ID;
}

} // namespace SysEx
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER UniversalSysEx.h
// ============================================================================
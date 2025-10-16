// ============================================================================
// Fichier: backend/src/midi/sysex/CustomSysExTypes.h
// Version: 1.0.0
// Description: Structures et constantes pour Custom SysEx
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace midiMind {
namespace CustomSysEx {

// ========================================================================
// CONSTANTES
// ========================================================================

// Manufacturer ID (3 bytes pour ID étendu)
constexpr uint8_t MANUFACTURER_ID_1 = 0x00;  // ID étendu (educational/development)
constexpr uint8_t MANUFACTURER_ID_2 = 0x21;
constexpr uint8_t MANUFACTURER_ID_3 = 0x7D;
constexpr uint32_t MANUFACTURER_ID = 0x00217D;

// Device ID
constexpr uint8_t DEVICE_ID = 0x01;

// Message markers
constexpr uint8_t SOX = 0xF0;  // System Exclusive Start
constexpr uint8_t EOX = 0xF7;  // End of Exclusive

// Taille minimum d'un message SysEx
constexpr size_t MIN_MESSAGE_SIZE = 8;  // SOX + Manuf(3) + Device + Bloc + EOX

// Nombre maximum de valves
constexpr uint8_t MAX_VALVES = 16;

// Bloc IDs
constexpr uint8_t BLOC_IDENTITY_CONFIG = 0x01;
constexpr uint8_t BLOC_VALVE_STATES = 0x02;
constexpr uint8_t BLOC_REGULATION = 0x03;

// Feature flags (pour Bloc 0)
constexpr uint8_t FEATURE_REGULATION = 0x01;
constexpr uint8_t FEATURE_CC_MAPPING = 0x02;
constexpr uint8_t FEATURE_AIR_CONTROL = 0x04;
constexpr uint8_t FEATURE_TUNING = 0x08;

// Status flags (pour Bloc 1)
constexpr uint8_t STATUS_MOVING = 0x01;
constexpr uint8_t STATUS_CALIBRATED = 0x02;
constexpr uint8_t STATUS_ERROR = 0x04;
constexpr uint8_t STATUS_ENABLED = 0x08;

// ========================================================================
// STRUCTURES
// ========================================================================

/**
 * @struct Version
 * @brief Version firmware/hardware
 */
struct Version {
    uint8_t major = 0;
    uint8_t minor = 0;
    uint8_t patch = 0;
    
    std::string toString() const {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch);
    }
    
    bool operator==(const Version& other) const {
        return major == other.major &&
               minor == other.minor &&
               patch == other.patch;
    }
};

/**
 * @struct Bloc0Identity
 * @brief Identité et configuration du dispositif (Bloc 0)
 */
struct Bloc0Identity {
    uint32_t uniqueId = 0;                    // ID unique 28-bit
    std::string name;                         // Nom du dispositif (max 16 chars)
    Version firmwareVersion;                  // Version firmware
    Version hardwareVersion;                  // Version hardware
    uint8_t numValves = 0;                    // Nombre de valves
    uint8_t midiChannelIn = 0;                // Canal MIDI entrée (0-15)
    uint8_t midiChannelOut = 0;               // Canal MIDI sortie (0-15)
    uint8_t features = 0;                     // Bitmap des fonctionnalités
    
    // Flags de fonctionnalités décodés
    bool hasRegulation = false;
    bool hasCCMapping = false;
    bool hasAirControl = false;
    bool hasTuning = false;
    
    Bloc0Identity() = default;
};

/**
 * @struct Bloc1ValveState
 * @brief État d'une valve (Bloc 1)
 */
struct Bloc1ValveState {
    uint8_t valveId = 0;                      // ID de la valve
    uint16_t currentPosition = 0;             // Position actuelle (14-bit)
    uint16_t targetPosition = 0;              // Position cible (14-bit)
    uint8_t status = 0;                       // Flags de status
    
    // Flags de status décodés
    bool isMoving = false;
    bool isCalibrated = false;
    bool hasError = false;
    bool isEnabled = false;
    
    Bloc1ValveState() = default;
};

/**
 * @struct Bloc2Regulation
 * @brief Paramètres de régulation PID (Bloc 2)
 */
struct Bloc2Regulation {
    uint16_t gainP = 0;                       // Gain proportionnel (14-bit)
    uint16_t gainI = 0;                       // Gain intégral (14-bit)
    uint16_t gainD = 0;                       // Gain dérivé (14-bit)
    uint8_t responseSpeed = 0;                // Vitesse de réponse (0-127)
    uint8_t deadzone = 0;                     // Zone morte (0-127)
    uint8_t smoothing = 0;                    // Lissage (0-127)
    
    Bloc2Regulation() = default;
};

// ========================================================================
// FONCTIONS D'ENCODAGE/DÉCODAGE
// ========================================================================

/**
 * @brief Encode une valeur 14-bit en 2 bytes 7-bit
 * @param value Valeur 14-bit (0-16383)
 * @param msb Byte de poids fort (sortie)
 * @param lsb Byte de poids faible (sortie)
 */
inline void encode14BitTo7Bit(uint16_t value, uint8_t& msb, uint8_t& lsb) {
    msb = (value >> 7) & 0x7F;
    lsb = value & 0x7F;
}

/**
 * @brief Décode 2 bytes 7-bit en valeur 14-bit
 * @param msb Byte de poids fort
 * @param lsb Byte de poids faible
 * @return Valeur 14-bit
 */
inline uint16_t decode14BitFrom7Bit(uint8_t msb, uint8_t lsb) {
    return ((msb & 0x7F) << 7) | (lsb & 0x7F);
}

/**
 * @brief Encode une valeur 28-bit en 4 bytes 7-bit
 * @param value Valeur 28-bit
 * @param b3 Byte 3 (sortie)
 * @param b2 Byte 2 (sortie)
 * @param b1 Byte 1 (sortie)
 * @param b0 Byte 0 (sortie)
 */
inline void encode28BitTo7Bit(uint32_t value, uint8_t& b3, uint8_t& b2, uint8_t& b1, uint8_t& b0) {
    b3 = (value >> 21) & 0x7F;
    b2 = (value >> 14) & 0x7F;
    b1 = (value >> 7) & 0x7F;
    b0 = value & 0x7F;
}

/**
 * @brief Décode 4 bytes 7-bit en valeur 28-bit
 * @param b3 Byte 3
 * @param b2 Byte 2
 * @param b1 Byte 1
 * @param b0 Byte 0
 * @return Valeur 28-bit
 */
inline uint32_t decode28BitFrom7Bit(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0) {
    return ((b3 & 0x7F) << 21) |
           ((b2 & 0x7F) << 14) |
           ((b1 & 0x7F) << 7) |
           (b0 & 0x7F);
}

} // namespace CustomSysEx
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CustomSysExTypes.h
// ============================================================================

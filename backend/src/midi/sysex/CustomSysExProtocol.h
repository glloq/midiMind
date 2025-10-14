// ============================================================================
// Fichier: src/midi/sysex/CustomSysExProtocol.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Protocole SysEx personnalisé pour instruments DIY
//   Format: F0 7D <DeviceID> <LayerID> <CommandID> <Payload> F7
//
//   VERSION ACTUELLE: v1.0
//   - Layer 01: Identification (IMPLÉMENTÉ)
//   - Layer 02: Mapping Notes (IMPLÉMENTÉ)
//   - Layers 03-07: Réservés pour versions futures
//
// Auteur: MidiMind Team
// Date: 2025-10-06
// Version: 1.0.0
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace midiMind {
namespace CustomSysEx {

// ============================================================================
// CONSTANTES DE BASE
// ============================================================================

/// Manufacturer ID pour usage éducatif (0x7D = Educational Use)
constexpr uint8_t MANUFACTURER_ID = 0x7D;

/// ID Request universel
constexpr uint8_t ID_REQUEST_COMMAND = 0x00;

// ============================================================================
// LAYER IDs - GROUPES LOGIQUES
// ============================================================================

namespace Layer {
    constexpr uint8_t IDENTIFICATION    = 0x01;  ///< ✅ IMPLÉMENTÉ - Identification instrument
    constexpr uint8_t MAPPING_NOTES     = 0x02;  ///< ✅ IMPLÉMENTÉ - Mapping des notes
    
    // 🔮 RÉSERVÉ POUR VERSIONS FUTURES
    constexpr uint8_t MAPPING_CC        = 0x03;  ///< TODO v2.0 - Mapping des CC
    constexpr uint8_t AIR_MANAGEMENT    = 0x04;  ///< TODO v2.0 - Gestion air/pompe
    constexpr uint8_t TUNING            = 0x05;  ///< TODO v3.0 - Accordage
    constexpr uint8_t SAFETY_ARU        = 0x06;  ///< TODO v3.0 - Arrêt d'urgence/sécurité
    constexpr uint8_t SENSORS_FEEDBACK  = 0x07;  ///< TODO v3.0 - Capteurs/monitoring
}

// ============================================================================
// COMMAND IDs - LAYER 01 (IDENTIFICATION) ✅ IMPLÉMENTÉ
// ============================================================================

namespace Command {
    namespace Identification {
        constexpr uint8_t REQUEST       = 0x00;  ///< Demande d'identification
        constexpr uint8_t REPLY         = 0x01;  ///< Réponse d'identification
    }
    
    // Layer 02 - Mapping Notes ✅ IMPLÉMENTÉ
    namespace NotesMapping {
        constexpr uint8_t REQUEST_ALL   = 0x00;  ///< Demander toutes les notes
        constexpr uint8_t REPLY_NOTE    = 0x01;  ///< Réponse pour une note
        constexpr uint8_t SET_NOTE      = 0x02;  ///< Configurer une note
        constexpr uint8_t GET_NOTE      = 0x03;  ///< Obtenir config d'une note
    }
}

// ============================================================================
// TYPES D'INSTRUMENTS
// ============================================================================

enum class InstrumentType : uint8_t {
    UNKNOWN         = 0x00,
    WIND            = 0x01,  ///< Instrument à vent
    STRING          = 0x02,  ///< Instrument à cordes
    PERCUSSION      = 0x03,  ///< Percussion
    KEYBOARD        = 0x04,  ///< Clavier
    CUSTOM          = 0xFF   ///< Type personnalisé
};

// ============================================================================
// TYPES D'ACCORDAGE
// ============================================================================

enum class TuningType : uint8_t {
    CHROMATIC       = 0x00,  ///< Chromatique standard
    DIATONIC        = 0x01,  ///< Diatonique
    PENTATONIC      = 0x02,  ///< Pentatonique
    CUSTOM          = 0xFF   ///< Accordage personnalisé
};

// ============================================================================
// STRUCTURES - LAYER 01 (IDENTIFICATION) ✅
// ============================================================================

/**
 * @struct CustomDeviceIdentity
 * @brief Identité complète d'un instrument DIY
 * 
 * Format du message d'identification:
 * F0 7D <DeviceID> 01 01 <Name...> 00 <NoteCount> <Type> <Tuning> 
 * <Polyphony> <DelayLSB> <DelayMSB> <FwV1> <FwV2> <FwV3> <FwV4> <Flags> <Programs> F7
 */
struct CustomDeviceIdentity {
    uint8_t deviceId;           ///< ID MIDI de l'instrument (0x00-0x7F)
    std::string name;           ///< Nom ASCII de l'instrument (max 16 chars)
    uint8_t noteCount;          ///< Nombre de notes disponibles
    InstrumentType type;        ///< Type d'instrument
    TuningType tuningType;      ///< Type d'accordage
    uint8_t maxPolyphony;       ///< Polyphonie maximale
    uint16_t mechanicalDelay;   ///< Délai mécanique en ms
    uint32_t firmwareVersion;   ///< Version firmware (4 bytes)
    
    // Capacités
    bool supportsPitchBend;     ///< Support pitch bend
    bool supportsAftertouch;    ///< Support aftertouch
    uint8_t programCount;       ///< Nombre de programs (0 = aucun)
    
    CustomDeviceIdentity()
        : deviceId(0)
        , noteCount(0)
        , type(InstrumentType::UNKNOWN)
        , tuningType(TuningType::CHROMATIC)
        , maxPolyphony(1)
        , mechanicalDelay(0)
        , firmwareVersion(0x01000000) // v1.0.0.0 par défaut
        , supportsPitchBend(false)
        , supportsAftertouch(false)
        , programCount(0) {}
        
    /**
     * @brief Convertit en chaîne lisible
     */
    std::string toString() const {
        std::string result = name + " [ID:" + std::to_string(deviceId) + "]";
        result += " Notes:" + std::to_string(noteCount);
        result += " Poly:" + std::to_string(maxPolyphony);
        result += " Delay:" + std::to_string(mechanicalDelay) + "ms";
        return result;
    }
    
    /**
     * @brief Retourne la version firmware formatée
     */
    std::string getFirmwareVersionString() const {
        uint8_t v1 = (firmwareVersion >> 24) & 0xFF;
        uint8_t v2 = (firmwareVersion >> 16) & 0xFF;
        uint8_t v3 = (firmwareVersion >> 8) & 0xFF;
        uint8_t v4 = firmwareVersion & 0xFF;
        
        char buf[32];
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", v1, v2, v3, v4);
        return std::string(buf);
    }
};

// ============================================================================
// STRUCTURES - LAYER 02 (MAPPING NOTES) ✅
// ============================================================================

/**
 * @struct NoteMapping
 * @brief Configuration d'une note MIDI → Actionneur
 * 
 * Format du message de configuration:
 * F0 7D <DeviceID> 02 02 <MidiNote> <ActuatorID> <MinVel> <MaxVel>
 * <AttackLSB> <AttackMSB> <ReleaseLSB> <ReleaseMSB> <Enabled> F7
 */
struct NoteMapping {
    uint8_t midiNote;           ///< Note MIDI (0-127)
    uint8_t actuatorId;         ///< ID de l'actionneur physique
    uint8_t minVelocity;        ///< Vélocité minimale (1-127)
    uint8_t maxVelocity;        ///< Vélocité maximale (1-127)
    uint16_t attackTime;        ///< Temps d'attaque en ms (0-16383)
    uint16_t releaseTime;       ///< Temps de relâchement en ms (0-16383)
    bool enabled;               ///< Note activée/désactivée
    
    NoteMapping()
        : midiNote(0)
        , actuatorId(0)
        , minVelocity(1)
        , maxVelocity(127)
        , attackTime(0)
        , releaseTime(0)
        , enabled(true) {}
    
    /**
     * @brief Valide la configuration
     */
    bool isValid() const {
        return midiNote <= 127 &&
               minVelocity > 0 && minVelocity <= 127 &&
               maxVelocity > 0 && maxVelocity <= 127 &&
               minVelocity <= maxVelocity &&
               attackTime <= 16383 &&
               releaseTime <= 16383;
    }
    
    /**
     * @brief Convertit en chaîne lisible
     */
    std::string toString() const {
        std::string result = "Note " + std::to_string(midiNote);
        result += " → Actuator " + std::to_string(actuatorId);
        result += " [" + std::to_string(minVelocity) + "-" + std::to_string(maxVelocity) + "]";
        result += " Attack:" + std::to_string(attackTime) + "ms";
        result += " Release:" + std::to_string(releaseTime) + "ms";
        result += enabled ? " ENABLED" : " DISABLED";
        return result;
    }
};

// ============================================================================
// STRUCTURES FUTURES (STUBS) 🔮
// ============================================================================

// TODO v2.0: Implémenter CCMapping (Layer 03)
struct CCMapping {
    uint8_t ccNumber;
    uint8_t minValue;
    uint8_t maxValue;
    bool enabled;
};

// TODO v2.0: Implémenter AirParameters (Layer 04)
struct AirParameters {
    uint16_t nominalPressure;
    uint16_t maxFlow;
    uint16_t pumpLatency;
    uint8_t dynamicProfile;
};

// TODO v3.0: Implémenter TuningOffset (Layer 05)
struct TuningOffset {
    uint8_t note;
    int16_t offsetCents;
};

// TODO v3.0: Implémenter SafetyParameters (Layer 06)
struct SafetyParameters {
    uint32_t timeout;
    uint8_t safetyLevel;
    uint16_t maxTemperature;
    uint16_t maxCurrent;
    bool emergencyStopActive;
};

// TODO v3.0: Implémenter SensorData (Layer 07)
struct SensorData {
    uint8_t sensorId;
    uint8_t sensorType;
    uint16_t value;
    uint32_t timestamp;
};

} // namespace CustomSysEx
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CustomSysExProtocol.h
// ============================================================================

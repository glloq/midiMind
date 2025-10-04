// ============================================================================
// Fichier: src/midi/MidiMessage.h
// Projet: midiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structure de données pour représenter les messages MIDI.
//   Contient le type de message, le canal, et les données (1 ou 2 bytes).
//
// Types de messages supportés:
//   - Note On/Off
//   - Control Change
//   - Program Change
//   - Pitch Bend
//   - System messages (Clock, Start, Stop, etc.)
//
// Auteur: midiMind Team
// Date: 2025-10-02
// Version: 3.0.0
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <cstdint>     // Pour uint8_t, uint16_t
#include <string>      // Pour std::string
#include <sstream>     // Pour std::ostringstream
#include <iomanip>     // Pour std::hex

namespace midiMind {

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

/**
 * @enum MidiMessageType
 * @brief Types de messages MIDI
 */
enum class MidiMessageType : uint8_t {
    // Messages de canal (Channel Voice Messages)
    NOTE_OFF          = 0x80,  ///< Note Off (canal + note + vélocité)
    NOTE_ON           = 0x90,  ///< Note On (canal + note + vélocité)
    POLY_AFTERTOUCH   = 0xA0,  ///< Polyphonic Aftertouch (canal + note + pression)
    CONTROL_CHANGE    = 0xB0,  ///< Control Change (canal + contrôleur + valeur)
    PROGRAM_CHANGE    = 0xC0,  ///< Program Change (canal + programme)
    CHANNEL_AFTERTOUCH = 0xD0, ///< Channel Aftertouch (canal + pression)
    PITCH_BEND        = 0xE0,  ///< Pitch Bend (canal + LSB + MSB)
    
    // Messages système (System Messages)
    SYSTEM_EXCLUSIVE  = 0xF0,  ///< System Exclusive (SysEx)
    TIME_CODE         = 0xF1,  ///< MIDI Time Code Quarter Frame
    SONG_POSITION     = 0xF2,  ///< Song Position Pointer
    SONG_SELECT       = 0xF3,  ///< Song Select
    TUNE_REQUEST      = 0xF6,  ///< Tune Request
    EOX               = 0xF7,  ///< End of Exclusive
    
    // Messages de timing (System Real-Time Messages)
    TIMING_CLOCK      = 0xF8,  ///< Timing Clock
    START             = 0xFA,  ///< Start
    CONTINUE          = 0xFB,  ///< Continue
    STOP              = 0xFC,  ///< Stop
    ACTIVE_SENSING    = 0xFE,  ///< Active Sensing
    SYSTEM_RESET      = 0xFF,  ///< System Reset
    
    // Type invalide
    INVALID           = 0x00   ///< Message invalide
};

// ============================================================================
// STRUCTURE: MidiMessage
// ============================================================================

/**
 * @struct MidiMessage
 * @brief Représente un message MIDI complet
 * 
 * Cette structure contient toutes les informations d'un message MIDI:
 * - Type de message (Note On, Control Change, etc.)
 * - Canal MIDI (0-15, ou 0xFF pour messages système)
 * - Données (1 ou 2 bytes selon le type)
 * - Timestamp (optionnel, pour le séquençage)
 * 
 * @details
 * Format standard d'un message MIDI:
 * - Status byte: [Type (4 bits) | Canal (4 bits)]
 * - Data byte 1: Paramètre 1 (note, contrôleur, etc.)
 * - Data byte 2: Paramètre 2 (vélocité, valeur, etc.) - optionnel
 * 
 * @note Tous les champs sont publics pour faciliter l'accès
 * @note La structure est trivially copyable (peut être memcpy)
 * 
 * @example Créer un Note On:
 * @code
 * MidiMessage msg;
 * msg.type = MidiMessageType::NOTE_ON;
 * msg.channel = 0;  // Canal 1 (0-indexé)
 * msg.data1 = 60;   // Middle C
 * msg.data2 = 100;  // Vélocité
 * msg.timestamp = 0;
 * @endcode
 * 
 * @example Utiliser les helpers:
 * @code
 * auto noteOn = MidiMessage::noteOn(0, 60, 100);
 * auto noteOff = MidiMessage::noteOff(0, 60, 0);
 * auto cc = MidiMessage::controlChange(0, 7, 127);  // Volume max
 * @endcode
 */
struct MidiMessage {
    // ========================================================================
    // CHAMPS
    // ========================================================================
    
    /**
     * @brief Type de message MIDI
     */
    MidiMessageType type = MidiMessageType::INVALID;
    
    /**
     * @brief Canal MIDI (0-15 pour messages de canal, 0xFF pour système)
     */
    uint8_t channel = 0xFF;
    
    /**
     * @brief Premier byte de données
     * 
     * Selon le type:
     * - NOTE_ON/OFF: Numéro de note (0-127)
     * - CONTROL_CHANGE: Numéro de contrôleur (0-127)
     * - PROGRAM_CHANGE: Numéro de programme (0-127)
     * - PITCH_BEND: LSB (7 bits bas)
     */
    uint8_t data1 = 0;
    
    /**
     * @brief Second byte de données (optionnel selon le type)
     * 
     * Selon le type:
     * - NOTE_ON/OFF: Vélocité (0-127)
     * - CONTROL_CHANGE: Valeur du contrôleur (0-127)
     * - PITCH_BEND: MSB (7 bits hauts)
     */
    uint8_t data2 = 0;
    
    /**
     * @brief Timestamp du message (en millisecondes)
     * 
     * Utilisé pour le séquençage et la synchronisation.
     * 0 = pas de timestamp / immédiat
     */
    uint64_t timestamp = 0;
    
    // ========================================================================
    // CONSTRUCTEURS
    // ========================================================================
    
    /**
     * @brief Constructeur par défaut
     * 
     * Crée un message invalide.
     */
    MidiMessage() = default;
    
    /**
     * @brief Constructeur avec tous les paramètres
     * 
     * @param type Type de message
     * @param channel Canal MIDI (0-15)
     * @param data1 Premier byte de données
     * @param data2 Second byte de données
     * @param timestamp Timestamp optionnel
     */
    MidiMessage(MidiMessageType type, 
                uint8_t channel, 
                uint8_t data1, 
                uint8_t data2 = 0,
                uint64_t timestamp = 0)
        : type(type)
        , channel(channel)
        , data1(data1)
        , data2(data2)
        , timestamp(timestamp) {}
    
    // ========================================================================
    // MÉTHODES - VALIDATION
    // ========================================================================
    
    /**
     * @brief Vérifie si le message est valide
     * 
     * Un message est valide si:
     * - Le type n'est pas INVALID
     * - Le canal est valide pour les messages de canal
     * - Les données sont dans les plages correctes
     * 
     * @return true Si le message est valide
     * @return false Si le message est invalide
     */
    bool isValid() const {
        if (type == MidiMessageType::INVALID) {
            return false;
        }
        
        // Vérifier le canal pour les messages de canal
        if (isChannelMessage() && channel > 15) {
            return false;
        }
        
        // Les données doivent être sur 7 bits (0-127)
        if (data1 > 127 || data2 > 127) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Vérifie si c'est un message de canal
     * 
     * @return true Si c'est un message de canal (0x80-0xEF)
     * @return false Si c'est un message système (0xF0-0xFF)
     */
    bool isChannelMessage() const {
        uint8_t typeValue = static_cast<uint8_t>(type);
        return (typeValue >= 0x80 && typeValue <= 0xEF);
    }
    
    /**
     * @brief Vérifie si c'est un message système
     * 
     * @return true Si c'est un message système (0xF0-0xFF)
     */
    bool isSystemMessage() const {
        uint8_t typeValue = static_cast<uint8_t>(type);
        return (typeValue >= 0xF0 && typeValue <= 0xFF);
    }
    
    /**
     * @brief Vérifie si c'est un message de timing
     * 
     * @return true Si c'est Clock, Start, Continue, Stop, etc.
     */
    bool isRealtimeMessage() const {
        return (type == MidiMessageType::TIMING_CLOCK ||
                type == MidiMessageType::START ||
                type == MidiMessageType::CONTINUE ||
                type == MidiMessageType::STOP ||
                type == MidiMessageType::ACTIVE_SENSING ||
                type == MidiMessageType::SYSTEM_RESET);
    }
    
    // ========================================================================
    // MÉTHODES - CONVERSION
    // ========================================================================
    
    /**
     * @brief Récupère le status byte complet (type + canal)
     * 
     * @return uint8_t Status byte (0x80-0xFF)
     */
    uint8_t getStatusByte() const {
        if (isChannelMessage()) {
            return static_cast<uint8_t>(type) | (channel & 0x0F);
        } else {
            return static_cast<uint8_t>(type);
        }
    }
    
    /**
     * @brief Convertit le message en string lisible (debug)
     * 
     * @return std::string Représentation textuelle
     * 
     * @example
     * "Note On [Ch:1] Note:60 Vel:100"
     * "Control Change [Ch:1] CC:7 Val:127"
     */
    std::string toString() const {
        std::ostringstream oss;
        
        // Type de message
        oss << typeToString();
        
        // Canal (si message de canal)
        if (isChannelMessage()) {
            oss << " [Ch:" << (int)(channel + 1) << "]";  // Afficher 1-16
        }
        
        // Données selon le type
        switch (type) {
            case MidiMessageType::NOTE_ON:
            case MidiMessageType::NOTE_OFF:
                oss << " Note:" << (int)data1 << " Vel:" << (int)data2;
                break;
                
            case MidiMessageType::CONTROL_CHANGE:
                oss << " CC:" << (int)data1 << " Val:" << (int)data2;
                break;
                
            case MidiMessageType::PROGRAM_CHANGE:
                oss << " Program:" << (int)data1;
                break;
                
            case MidiMessageType::PITCH_BEND:
                oss << " Bend:" << getPitchBendValue();
                break;
                
            default:
                if (data2 != 0) {
                    oss << " Data:" << (int)data1 << "," << (int)data2;
                } else {
                    oss << " Data:" << (int)data1;
                }
                break;
        }
        
        return oss.str();
    }
    
    /**
     * @brief Convertit le type en string
     * 
     * @return std::string Nom du type
     */
    std::string typeToString() const {
        switch (type) {
            case MidiMessageType::NOTE_OFF:          return "Note Off";
            case MidiMessageType::NOTE_ON:           return "Note On";
            case MidiMessageType::POLY_AFTERTOUCH:   return "Poly Aftertouch";
            case MidiMessageType::CONTROL_CHANGE:    return "Control Change";
            case MidiMessageType::PROGRAM_CHANGE:    return "Program Change";
            case MidiMessageType::CHANNEL_AFTERTOUCH: return "Channel Aftertouch";
            case MidiMessageType::PITCH_BEND:        return "Pitch Bend";
            case MidiMessageType::SYSTEM_EXCLUSIVE:  return "SysEx";
            case MidiMessageType::TIMING_CLOCK:      return "Clock";
            case MidiMessageType::START:             return "Start";
            case MidiMessageType::CONTINUE:          return "Continue";
            case MidiMessageType::STOP:              return "Stop";
            case MidiMessageType::ACTIVE_SENSING:    return "Active Sensing";
            case MidiMessageType::SYSTEM_RESET:      return "System Reset";
            default:                                 return "Unknown";
        }
    }
    
    /**
     * @brief Récupère la valeur de pitch bend (-8192 à +8191)
     * 
     * Combine data1 (LSB) et data2 (MSB) en valeur signée centrée sur 0.
     * 
     * @return int Valeur de pitch bend
     */
    int getPitchBendValue() const {
        // Combiner les 7 bits de data1 et data2 en 14 bits
        int value = (data2 << 7) | data1;
        // Centrer sur 0 (-8192 à +8191)
        return value - 8192;
    }
    
    /**
     * @brief Convertit en bytes bruts (pour envoi MIDI)
     * 
     * @param bytes Buffer pour stocker les bytes (min 3 bytes)
     * @return size_t Nombre de bytes écrits (1, 2 ou 3)
     */
    size_t toBytes(uint8_t* bytes) const {
        if (!bytes) return 0;
        
        bytes[0] = getStatusByte();
        
        // Messages système real-time : 1 byte seulement
        if (isRealtimeMessage()) {
            return 1;
        }
        
        // Program Change et Channel Aftertouch : 2 bytes
        if (type == MidiMessageType::PROGRAM_CHANGE || 
            type == MidiMessageType::CHANNEL_AFTERTOUCH) {
            bytes[1] = data1;
            return 2;
        }
        
        // Autres messages : 3 bytes
        bytes[1] = data1;
        bytes[2] = data2;
        return 3;
    }
    
    // ========================================================================
    // FACTORY METHODS - Helpers pour créer des messages
    // ========================================================================
    
    /**
     * @brief Crée un message Note On
     * 
     * @param channel Canal MIDI (0-15)
     * @param note Numéro de note (0-127)
     * @param velocity Vélocité (0-127)
     * @param timestamp Timestamp optionnel
     * @return MidiMessage Message Note On
     */
    static MidiMessage noteOn(uint8_t channel, uint8_t note, 
                             uint8_t velocity, uint64_t timestamp = 0) {
        return MidiMessage(MidiMessageType::NOTE_ON, channel, note, velocity, timestamp);
    }
    
    /**
     * @brief Crée un message Note Off
     * 
     * @param channel Canal MIDI (0-15)
     * @param note Numéro de note (0-127)
     * @param velocity Vélocité de release (0-127, souvent 0)
     * @param timestamp Timestamp optionnel
     * @return MidiMessage Message Note Off
     */
    static MidiMessage noteOff(uint8_t channel, uint8_t note, 
                              uint8_t velocity = 0, uint64_t timestamp = 0) {
        return MidiMessage(MidiMessageType::NOTE_OFF, channel, note, velocity, timestamp);
    }
    
    /**
     * @brief Crée un message Control Change
     * 
     * @param channel Canal MIDI (0-15)
     * @param controller Numéro de contrôleur (0-127)
     * @param value Valeur (0-127)
     * @param timestamp Timestamp optionnel
     * @return MidiMessage Message Control Change
     */
    static MidiMessage controlChange(uint8_t channel, uint8_t controller, 
                                     uint8_t value, uint64_t timestamp = 0) {
        return MidiMessage(MidiMessageType::CONTROL_CHANGE, channel, 
                          controller, value, timestamp);
    }
    
    /**
     * @brief Crée un message Program Change
     * 
     * @param channel Canal MIDI (0-15)
     * @param program Numéro de programme (0-127)
     * @param timestamp Timestamp optionnel
     * @return MidiMessage Message Program Change
     */
    static MidiMessage programChange(uint8_t channel, uint8_t program, 
                                     uint64_t timestamp = 0) {
        return MidiMessage(MidiMessageType::PROGRAM_CHANGE, channel, 
                          program, 0, timestamp);
    }
    
    /**
     * @brief Crée un message Pitch Bend
     * 
     * @param channel Canal MIDI (0-15)
     * @param value Valeur de bend (-8192 à +8191, 0 = centré)
     * @param timestamp Timestamp optionnel
     * @return MidiMessage Message Pitch Bend
     */
    static MidiMessage pitchBend(uint8_t channel, int value, 
                                 uint64_t timestamp = 0) {
        // Convertir -8192/+8191 en 0-16383
        uint16_t bendValue = static_cast<uint16_t>(value + 8192);
        uint8_t lsb = bendValue & 0x7F;
        uint8_t msb = (bendValue >> 7) & 0x7F;
        
        return MidiMessage(MidiMessageType::PITCH_BEND, channel, 
                          lsb, msb, timestamp);
    }
    
    /**
     * @brief Crée un message Clock
     * 
     * @param timestamp Timestamp optionnel
     * @return MidiMessage Message Clock
     */
    static MidiMessage clock(uint64_t timestamp = 0) {
        return MidiMessage(MidiMessageType::TIMING_CLOCK, 0xFF, 0, 0, timestamp);
    }
    
    /**
     * @brief Crée un message Start
     * 
     * @param timestamp Timestamp optionnel
     * @return MidiMessage Message Start
     */
    static MidiMessage start(uint64_t timestamp = 0) {
        return MidiMessage(MidiMessageType::START, 0xFF, 0, 0, timestamp);
    }
    
    /**
     * @brief Crée un message Stop
     * 
     * @param timestamp Timestamp optionnel
     * @return MidiMessage Message Stop
     */
    static MidiMessage stop(uint64_t timestamp = 0) {
        return MidiMessage(MidiMessageType::STOP, 0xFF, 0, 0, timestamp);
    }
    
    /**
     * @brief Crée un message Continue
     * 
     * @param timestamp Timestamp optionnel
     * @return MidiMessage Message Continue
     */
    static MidiMessage continue_(uint64_t timestamp = 0) {
        return MidiMessage(MidiMessageType::CONTINUE, 0xFF, 0, 0, timestamp);
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiMessage.h
// ============================================================================

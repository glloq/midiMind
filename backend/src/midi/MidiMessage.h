// ============================================================================
// Fichier: src/midi/MidiMessage.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Classe représentant un message MIDI avec factory methods.
//   Encapsule les données MIDI et fournit des méthodes de création/analyse.
//
// Responsabilités:
//   - Encapsulation des données MIDI
//   - Factory methods pour types de messages
//   - Parsing et validation
//   - Conversion vers/depuis bytes
//
// Thread-safety: OUI (immutable après création)
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <vector>
#include <cstdint>
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

/**
 * @enum MidiMessageType
 * @brief Types de messages MIDI (status bytes)
 */
enum class MidiMessageType {
    // Channel Voice Messages (0x80-0xEF)
    NOTE_OFF         = 0x80,  ///< Note Off
    NOTE_ON          = 0x90,  ///< Note On
    POLY_PRESSURE    = 0xA0,  ///< Polyphonic Key Pressure
    CONTROL_CHANGE   = 0xB0,  ///< Control Change
    PROGRAM_CHANGE   = 0xC0,  ///< Program Change
    CHANNEL_PRESSURE = 0xD0,  ///< Channel Pressure
    PITCH_BEND       = 0xE0,  ///< Pitch Bend
    
    // System Common Messages (0xF0-0xF7)
    SYSTEM_EXCLUSIVE = 0xF0,  ///< System Exclusive
    TIME_CODE        = 0xF1,  ///< MIDI Time Code
    SONG_POSITION    = 0xF2,  ///< Song Position Pointer
    SONG_SELECT      = 0xF3,  ///< Song Select
    TUNE_REQUEST     = 0xF6,  ///< Tune Request
    EOX              = 0xF7,  ///< End of Exclusive
    
    // System Real-Time Messages (0xF8-0xFF)
    CLOCK            = 0xF8,  ///< Timing Clock
    START            = 0xFA,  ///< Start
    CONTINUE         = 0xFB,  ///< Continue
    STOP             = 0xFC,  ///< Stop
    ACTIVE_SENSING   = 0xFE,  ///< Active Sensing
    SYSTEM_RESET     = 0xFF,  ///< System Reset
    
    UNKNOWN          = 0x00   ///< Type inconnu
};

/**
 * @enum ControllerType
 * @brief Types de contrôleurs MIDI (CC)
 */
enum class ControllerType : uint8_t {
    MODULATION       = 0x01,
    BREATH           = 0x02,
    FOOT             = 0x04,
    PORTAMENTO_TIME  = 0x05,
    VOLUME           = 0x07,
    BALANCE          = 0x08,
    PAN              = 0x0A,
    EXPRESSION       = 0x0B,
    
    SUSTAIN          = 0x40,
    PORTAMENTO       = 0x41,
    SOSTENUTO        = 0x42,
    SOFT_PEDAL       = 0x43,
    
    REVERB           = 0x5B,
    TREMOLO          = 0x5C,
    CHORUS           = 0x5D,
    CELESTE          = 0x5E,
    PHASER           = 0x5F,
    
    ALL_SOUND_OFF    = 0x78,
    RESET_CONTROLLERS = 0x79,
    ALL_NOTES_OFF    = 0x7B,
    OMNI_OFF         = 0x7C,
    OMNI_ON          = 0x7D,
    MONO_MODE        = 0x7E,
    POLY_MODE        = 0x7F
};

// ============================================================================
// CLASSE MidiMessage
// ============================================================================

/**
 * @class MidiMessage
 * @brief Représentation d'un message MIDI
 * 
 * @details
 * Encapsule les données d'un message MIDI avec:
 * - Factory methods pour création facile
 * - Validation automatique
 * - Parsing et analyse
 * - Conversion JSON
 * 
 * Immutable après création pour thread-safety.
 * 
 * @example Création de messages
 * ```cpp
 * // Note On
 * auto msg = MidiMessage::noteOn(0, 60, 100);  // Channel 1, Middle C, velocity 100
 * 
 * // Control Change
 * auto cc = MidiMessage::controlChange(0, ControllerType::VOLUME, 80);
 * 
 * // System Real-Time
 * auto clock = MidiMessage::clock();
 * ```
 */
class MidiMessage {
public:
    // ========================================================================
    // CONSTRUCTEURS
    // ========================================================================
    
    /**
     * @brief Constructeur par défaut (message vide)
     */
    MidiMessage(){
		: status_(0)
        , data1_(0)
        , data2_(0)
        , timestamp_(0)
	};
    
    /**
     * @brief Constructeur avec données brutes
     * 
     * @param data Données MIDI brutes
     */
    explicit MidiMessage(const std::vector<uint8_t>& data);
    
    /**
     * @brief Constructeur pour messages 3 octets
     * 
     * @param status Status byte
     * @param data1 Data byte 1
     * @param data2 Data byte 2
     */
    MidiMessage(uint8_t status, uint8_t data1, uint8_t data2);
    
    uint8_t data2_;         ///< Deuxième byte de données
    
    /// Timestamp du message (microsecondes)
    /// 0 = pas de timestamp
    uint64_t timestamp_;    
	
	
    /**
     * @brief Constructeur pour messages 2 octets
     * 
     * @param status Status byte
     * @param data1 Data byte 1
     */
    MidiMessage(uint8_t status, uint8_t data1);
    
    /**
     * @brief Constructeur pour messages 1 octet
     * 
     * @param status Status byte seul
     */
    explicit MidiMessage(uint8_t status);
    
    // ========================================================================
    // FACTORY METHODS - CHANNEL VOICE
    // ========================================================================
    
    /**
     * @brief Crée un message Note On
     * 
     * @param channel Canal MIDI (0-15)
     * @param note Numéro de note (0-127)
     * @param velocity Vélocité (0-127)
     * @return MidiMessage Message créé
     */
    static MidiMessage noteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    
    /**
     * @brief Crée un message Note Off
     * 
     * @param channel Canal MIDI (0-15)
     * @param note Numéro de note (0-127)
     * @param velocity Vélocité de release (0-127, défaut: 0)
     * @return MidiMessage Message créé
     */
    static MidiMessage noteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0);
    
    /**
     * @brief Crée un message Control Change
     * 
     * @param channel Canal MIDI (0-15)
     * @param controller Numéro de contrôleur (0-127)
     * @param value Valeur (0-127)
     * @return MidiMessage Message créé
     */
    static MidiMessage controlChange(uint8_t channel, uint8_t controller, uint8_t value);
    
    /**
     * @brief Crée un message Control Change (avec enum)
     * 
     * @param channel Canal MIDI (0-15)
     * @param type Type de contrôleur
     * @param value Valeur (0-127)
     * @return MidiMessage Message créé
     */
    static MidiMessage controlChange(uint8_t channel, ControllerType type, uint8_t value);
    
    /**
     * @brief Crée un message Program Change
     * 
     * @param channel Canal MIDI (0-15)
     * @param program Numéro de programme (0-127)
     * @return MidiMessage Message créé
     */
    static MidiMessage programChange(uint8_t channel, uint8_t program);
    
    /**
     * @brief Crée un message Channel Pressure
     * 
     * @param channel Canal MIDI (0-15)
     * @param pressure Pression (0-127)
     * @return MidiMessage Message créé
     */
    static MidiMessage channelPressure(uint8_t channel, uint8_t pressure);
    
    /**
     * @brief Crée un message Pitch Bend
     * 
     * @param channel Canal MIDI (0-15)
     * @param value Valeur (-8192 à 8191, 0 = centre)
     * @return MidiMessage Message créé
     */
    static MidiMessage pitchBend(uint8_t channel, int16_t value);
    
    // ========================================================================
    // FACTORY METHODS - SYSTEM
    // ========================================================================
    
    /**
     * @brief Crée un message Clock
     */
    static MidiMessage clock();
    
    /**
     * @brief Crée un message Start
     */
    static MidiMessage start();
    
    /**
     * @brief Crée un message Continue
     */
    static MidiMessage continueMsg();
    
    /**
     * @brief Crée un message Stop
     */
    static MidiMessage stop();
    
    /**
     * @brief Crée un message Active Sensing
     */
    static MidiMessage activeSensing();
    
    /**
     * @brief Crée un message System Reset
     */
    static MidiMessage systemReset();
    
    /**
     * @brief Crée un message All Notes Off
     * 
     * @param channel Canal MIDI (0-15)
     */
    static MidiMessage allNotesOff(uint8_t channel);
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * @brief Récupère le type de message
     */
    MidiMessageType getType() const;
    
    /**
     * @brief Récupère le status byte
     */
    uint8_t getStatus() const;
    
    /**
     * @brief Récupère le canal (0-15, ou -1 si pas de canal)
     */
    int getChannel() const;
    
    /**
     * @brief Récupère le premier data byte
     */
    uint8_t getData1() const;
    
    /**
     * @brief Récupère le deuxième data byte
     */
    uint8_t getData2() const;
	
    /**
	 * @brief Récupère le numéro de note (pour Note On/Off)
	 * Alias pour getData1()
	 */
	uint8_t getNote() const { return getData1(); }

	/**
	 * @brief Récupère la vélocité (pour Note On/Off)
	 * Alias pour getData2()
	 */
	uint8_t getVelocity() const { return getData2(); }

	/**
	 * @brief Récupère les données brutes (alias pour compatibilité)
	 */
	const std::vector<uint8_t>& getRawData() const { return getData(); }
    /**
     * @brief Récupère les données brutes
     */
    const std::vector<uint8_t>& getData() const { return data_; }
    
    /**
     * @brief Récupère la taille du message
     */
    size_t getSize() const { return data_.size(); }
    
    /**
     * @brief Récupère le timestamp (µs)
     */
    uint64_t getTimestamp() const { return timestamp_; }
    
    /**
     * @brief Définit le timestamp
     */
    void setTimestamp(uint64_t timestamp) { timestamp_ = timestamp; }
    
    // ========================================================================
    // PRÉDICATS
    // ========================================================================
    
    bool isNoteOn() const;
    bool isNoteOff() const;
    bool isNote() const { return isNoteOn() || isNoteOff(); }
    bool isControlChange() const;
    bool isProgramChange() const;
    bool isPitchBend() const;
    bool isChannelMessage() const;
    bool isSystemMessage() const;
    bool isRealTimeMessage() const;
    bool isSysEx() const;
    bool isValid() const;
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Convertit le type en string
     */
    static std::string messageTypeToString(MidiMessageType type);
    
    /**
     * @brief Récupère le nom du type de ce message
     */
    std::string getTypeName() const;
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const;
    
    /**
     * @brief Crée depuis JSON
     */
    static MidiMessage fromJson(const json& j);
    
    /**
     * @brief Convertit en string hexadécimal
     */
    std::string toHexString() const;
    
    /**
     * @brief Opérateur de comparaison
     */
    bool operator==(const MidiMessage& other) const;
    bool operator!=(const MidiMessage& other) const { return !(*this == other); }


  
    /**
     * @brief Capture le timestamp actuel
     * 
     * @details
     * Capture le timestamp avec précision microseconde.
     * Utilisé pour mesurer la latence de bout en bout.
     */
    void captureTimestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        timestamp_ = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }
    
    /**
     * @brief Définit le timestamp manuellement
     * @param timestamp Timestamp en microsecondes
     */
    void setTimestamp(uint64_t timestamp) {
        timestamp_ = timestamp;
    }
    
    /**
     * @brief Récupère le timestamp
     * @return uint64_t Timestamp en microsecondes (0 si pas de timestamp)
     */
    uint64_t getTimestamp() const {
        return timestamp_;
    }
    
    /**
     * @brief Vérifie si le message a un timestamp
     * @return bool true si timestamp présent
     */
    bool hasTimestamp() const {
        return timestamp_ > 0;
    }
    
    /**
     * @brief Calcule le temps écoulé depuis le timestamp (µs)
     * @return uint64_t Microsecondes écoulées
     */
    uint64_t getAge() const {
        if (timestamp_ == 0) return 0;
        
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        uint64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        
        return (nowUs >= timestamp_) ? (nowUs - timestamp_) : 0;
    }
    
    /**
     * @brief Réinitialise le timestamp
     */
    void resetTimestamp() {
        timestamp_ = 0;
    }


    /**
     * @brief Crée un message avec timestamp actuel
     * @param status Byte de status
     * @param data1 Premier byte
     * @param data2 Deuxième byte
     * @return MidiMessage Message avec timestamp
     */
    static MidiMessage createWithTimestamp(uint8_t status, uint8_t data1, uint8_t data2) {
        MidiMessage msg(status, data1, data2);
        msg.captureTimestamp();
        return msg;
    }
    
    /**
     * @brief Calcule la latence entre deux messages (µs)
     * @param msg1 Premier message
     * @param msg2 Deuxième message
     * @return uint64_t Latence en microsecondes
     */
    static uint64_t calculateLatency(const MidiMessage& msg1, const MidiMessage& msg2) {
        if (!msg1.hasTimestamp() || !msg2.hasTimestamp()) {
            return 0;
        }
        
        uint64_t t1 = msg1.getTimestamp();
        uint64_t t2 = msg2.getTimestamp();
        
        return (t2 >= t1) ? (t2 - t1) : (t1 - t2);
    }








private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Données MIDI brutes
    std::vector<uint8_t> data_;
    
    /// Timestamp en microsecondes
    uint64_t timestamp_;
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Valide et limite une valeur 7-bit
     */
    static uint8_t clamp7bit(uint8_t value);
    
    /**
     * @brief Valide et limite un canal
     */
    static uint8_t clampChannel(uint8_t channel);
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiMessage.h
// ============================================================================
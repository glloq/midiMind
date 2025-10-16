// ============================================================================
// Fichier: backend/src/midi/MidiMessage.h
// Version: 3.0.3 - CORRECTION DES DUPLICATIONS
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - Suppression des duplications de setTimestamp() et getTimestamp()
// - Suppression de la duplication de timestamp_
// - Correction de la syntaxe du constructeur par défaut
// - Ajout de setNote() et setVelocity() manquants
// ============================================================================

#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ========================================================================
// ÉNUMÉRATIONS
// ========================================================================

enum class MidiMessageType {
    NOTE_OFF         = 0x80,
    NOTE_ON          = 0x90,
    POLY_PRESSURE    = 0xA0,
    CONTROL_CHANGE   = 0xB0,
    PROGRAM_CHANGE   = 0xC0,
    CHANNEL_PRESSURE = 0xD0,
    PITCH_BEND       = 0xE0,
    SYSTEM           = 0xF0,
    // System Real-Time
    CLOCK            = 0xF8,
    START            = 0xFA,
    CONTINUE         = 0xFB,
    STOP             = 0xFC,
    ACTIVE_SENSING   = 0xFE,
    RESET            = 0xFF,
    // System Exclusive
    SOX              = 0xF0,
    EOX              = 0xF7
};

/**
 * @class MidiMessage
 * @brief Message MIDI avec données brutes et timestamp
 */
class MidiMessage {
public:
    // ========================================================================
    // CONSTRUCTEURS
    // ========================================================================
    
    /**
     * @brief ✅ CORRECTION: Constructeur par défaut
     */
    MidiMessage()
        : timestamp_(0) {
        data_.reserve(3);
    }
    
    /**
     * @brief Constructeur avec données brutes
     */
    explicit MidiMessage(const std::vector<uint8_t>& data)
        : data_(data)
        , timestamp_(0) {
    }
    
    /**
     * @brief Constructeur pour messages 3 octets
     */
    MidiMessage(uint8_t status, uint8_t data1, uint8_t data2)
        : data_{status, data1, data2}
        , timestamp_(0) {
    }
    
    /**
     * @brief Constructeur pour messages 2 octets
     */
    MidiMessage(uint8_t status, uint8_t data1)
        : data_{status, data1}
        , timestamp_(0) {
    }
    
    /**
     * @brief Constructeur pour messages 1 octet
     */
    explicit MidiMessage(uint8_t status)
        : data_{status}
        , timestamp_(0) {
    }
    
    // ========================================================================
    // FACTORY METHODS - CHANNEL VOICE
    // ========================================================================
    
    static MidiMessage noteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    static MidiMessage noteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0);
    static MidiMessage controlChange(uint8_t channel, uint8_t controller, uint8_t value);
    static MidiMessage programChange(uint8_t channel, uint8_t program);
    static MidiMessage pitchBend(uint8_t channel, int16_t value);
    static MidiMessage channelPressure(uint8_t channel, uint8_t pressure);
    static MidiMessage polyPressure(uint8_t channel, uint8_t note, uint8_t pressure);
    
    // ========================================================================
    // FACTORY METHODS - SYSTEM REAL-TIME
    // ========================================================================
    
    static MidiMessage clock();
    static MidiMessage start();
    static MidiMessage continue_();
    static MidiMessage stop();
    static MidiMessage activeSensing();
    static MidiMessage reset();
    
    // ========================================================================
    // ACCESSEURS
    // ========================================================================
    
    uint8_t getStatus() const {
        return data_.empty() ? 0 : data_[0];
    }
    
    uint8_t getChannel() const {
        return getStatus() & 0x0F;
    }
    
    MidiMessageType getType() const {
        return static_cast<MidiMessageType>(getStatus() & 0xF0);
    }
    
    uint8_t getData1() const {
        return data_.size() > 1 ? data_[1] : 0;
    }
    
    uint8_t getData2() const {
        return data_.size() > 2 ? data_[2] : 0;
    }
    
    uint8_t getNote() const {
        return getData1();
    }
    
    uint8_t getVelocity() const {
        return getData2();
    }
    
    uint8_t getController() const {
        return getData1();
    }
    
    uint8_t getValue() const {
        return getData2();
    }
    
    uint8_t getProgram() const {
        return getData1();
    }
    
    int16_t getPitchBend() const {
        return ((getData2() << 7) | getData1()) - 8192;
    }
    
    const std::vector<uint8_t>& getData() const {
        return data_;
    }
    
    size_t getSize() const {
        return data_.size();
    }
    
    /**
     * @brief ✅ PAS DE DUPLICATION: Une seule définition de getTimestamp()
     */
    uint64_t getTimestamp() const {
        return timestamp_;
    }
    
    // ========================================================================
    // MUTATEURS
    // ========================================================================
    
    /**
     * @brief ✅ PAS DE DUPLICATION: Une seule définition de setTimestamp()
     */
    void setTimestamp(uint64_t timestamp) {
        timestamp_ = timestamp;
    }
    
    /**
     * @brief ✅ AJOUT: Méthode setNote() manquante
     */
    void setNote(uint8_t note) {
        if (data_.size() > 1) {
            data_[1] = note & 0x7F;
        }
    }
    
    /**
     * @brief ✅ AJOUT: Méthode setVelocity() manquante
     */
    void setVelocity(uint8_t velocity) {
        if (data_.size() > 2) {
            data_[2] = velocity & 0x7F;
        }
    }
    
    /**
     * @brief Capture le timestamp actuel
     */
    void captureTimestamp() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        timestamp_ = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }
    
    /**
     * @brief Réinitialise le timestamp
     */
    void resetTimestamp() {
        timestamp_ = 0;
    }
    
    // ========================================================================
    // PRÉDICATS
    // ========================================================================
    
    bool isNoteOn() const;
    bool isNoteOff() const;
    bool isNote() const {
        return isNoteOn() || isNoteOff();
    }
    bool isControlChange() const;
    bool isProgramChange() const;
    bool isPitchBend() const;
    bool isChannelMessage() const;
    bool isSystemMessage() const;
    bool isRealTimeMessage() const;
    bool isSysEx() const;
    bool isValid() const;
    
    /**
     * @brief Vérifie si le message a un timestamp
     */
    bool hasTimestamp() const {
        return timestamp_ > 0;
    }
    
    /**
     * @brief Calcule le temps écoulé depuis le timestamp (µs)
     */
    uint64_t getAge() const {
        if (timestamp_ == 0) return 0;
        
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        uint64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        
        return (nowUs >= timestamp_) ? (nowUs - timestamp_) : 0;
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    static std::string messageTypeToString(MidiMessageType type);
    std::string getTypeName() const;
    json toJson() const;
    static MidiMessage fromJson(const json& j);
    std::string toHexString() const;
    
    bool operator==(const MidiMessage& other) const;
    bool operator!=(const MidiMessage& other) const {
        return !(*this == other);
    }
    
    // ========================================================================
    // FACTORY METHODS AVEC TIMESTAMP
    // ========================================================================
    
    static MidiMessage createWithTimestamp(uint8_t status, uint8_t data1, uint8_t data2) {
        MidiMessage msg(status, data1, data2);
        msg.captureTimestamp();
        return msg;
    }
    
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
    
    /// ✅ PAS DE DUPLICATION: Une seule définition de timestamp_
    /// Timestamp en microsecondes (0 = pas de timestamp)
    uint64_t timestamp_;
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    static uint8_t clamp7bit(uint8_t value);
    static uint8_t clampChannel(uint8_t channel);
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiMessage.h
// ============================================================================

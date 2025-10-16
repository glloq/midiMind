// ============================================================================
// File: backend/src/midi/MidiMessage.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   MIDI message representation and manipulation.
//   Provides type-safe MIDI message creation and parsing.
//
// Features:
//   - All standard MIDI message types
//   - Factory methods for easy creation
//   - Type checking and validation
//   - JSON serialization
//   - Timestamp support
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Enhanced type safety
//   - Better validation
//   - Improved JSON support
//
// ============================================================================

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * @enum MidiMessageType
 * @brief MIDI message types
 */
enum class MidiMessageType : uint8_t {
    // Channel Voice Messages
    NOTE_OFF            = 0x80,
    NOTE_ON             = 0x90,
    POLY_PRESSURE       = 0xA0,
    CONTROL_CHANGE      = 0xB0,
    PROGRAM_CHANGE      = 0xC0,
    CHANNEL_PRESSURE    = 0xD0,
    PITCH_BEND          = 0xE0,
    
    // System Common Messages
    SYSTEM_EXCLUSIVE    = 0xF0,
    TIME_CODE           = 0xF1,
    SONG_POSITION       = 0xF2,
    SONG_SELECT         = 0xF3,
    TUNE_REQUEST        = 0xF6,
    EOX                 = 0xF7,
    
    // System Real-Time Messages
    CLOCK               = 0xF8,
    START               = 0xFA,
    CONTINUE            = 0xFB,
    STOP                = 0xFC,
    ACTIVE_SENSING      = 0xFE,
    SYSTEM_RESET        = 0xFF,
    
    UNKNOWN             = 0x00
};

/**
 * @enum ControllerType
 * @brief Standard MIDI controller numbers
 */
enum class ControllerType : uint8_t {
    BANK_SELECT         = 0,
    MODULATION          = 1,
    BREATH_CONTROLLER   = 2,
    FOOT_CONTROLLER     = 4,
    PORTAMENTO_TIME     = 5,
    DATA_ENTRY_MSB      = 6,
    VOLUME              = 7,
    BALANCE             = 8,
    PAN                 = 10,
    EXPRESSION          = 11,
    EFFECT_CONTROL_1    = 12,
    EFFECT_CONTROL_2    = 13,
    
    SUSTAIN_PEDAL       = 64,
    PORTAMENTO          = 65,
    SOSTENUTO           = 66,
    SOFT_PEDAL          = 67,
    LEGATO              = 68,
    HOLD_2              = 69,
    
    SOUND_CONTROLLER_1  = 70,
    SOUND_CONTROLLER_2  = 71,
    SOUND_CONTROLLER_3  = 72,
    SOUND_CONTROLLER_4  = 73,
    SOUND_CONTROLLER_5  = 74,
    SOUND_CONTROLLER_6  = 75,
    SOUND_CONTROLLER_7  = 76,
    SOUND_CONTROLLER_8  = 77,
    SOUND_CONTROLLER_9  = 78,
    SOUND_CONTROLLER_10 = 79,
    
    EFFECTS_DEPTH_1     = 91,
    EFFECTS_DEPTH_2     = 92,
    EFFECTS_DEPTH_3     = 93,
    EFFECTS_DEPTH_4     = 94,
    EFFECTS_DEPTH_5     = 95,
    
    ALL_SOUND_OFF       = 120,
    RESET_ALL_CONTROLLERS = 121,
    LOCAL_CONTROL       = 122,
    ALL_NOTES_OFF       = 123,
    OMNI_OFF            = 124,
    OMNI_ON             = 125,
    MONO_ON             = 126,
    POLY_ON             = 127
};

// ============================================================================
// CLASS: MidiMessage
// ============================================================================

/**
 * @class MidiMessage
 * @brief MIDI message representation
 * 
 * Represents a complete MIDI message with data and timestamp.
 * Provides type-safe creation and manipulation of MIDI messages.
 * 
 * Thread Safety: YES (immutable after creation)
 * 
 * Example:
 * ```cpp
 * // Create note on
 * auto noteOn = MidiMessage::noteOn(0, 60, 100);  // Channel 0, C4, velocity 100
 * 
 * // Check type
 * if (noteOn.isNoteOn()) {
 *     int note = noteOn.getData1();
 *     int velocity = noteOn.getData2();
 * }
 * 
 * // Create control change
 * auto cc = MidiMessage::controlChange(0, ControllerType::VOLUME, 100);
 * 
 * // Serialize to JSON
 * json j = noteOn.toJson();
 * ```
 */
class MidiMessage {
public:
    // ========================================================================
    // CONSTRUCTORS
    // ========================================================================
    
    /**
     * @brief Default constructor (empty message)
     */
    MidiMessage();
    
    /**
     * @brief Constructor from raw data
     * @param data Raw MIDI data bytes
     */
    explicit MidiMessage(const std::vector<uint8_t>& data);
    
    /**
     * @brief Constructor from single byte (system real-time)
     * @param status Status byte
     */
    explicit MidiMessage(uint8_t status);
    
    /**
     * @brief Constructor from 2 bytes
     * @param status Status byte
     * @param data1 First data byte
     */
    MidiMessage(uint8_t status, uint8_t data1);
    
    /**
     * @brief Constructor from 3 bytes
     * @param status Status byte
     * @param data1 First data byte
     * @param data2 Second data byte
     */
    MidiMessage(uint8_t status, uint8_t data1, uint8_t data2);
    
    // ========================================================================
    // FACTORY METHODS - CHANNEL VOICE
    // ========================================================================
    
    /**
     * @brief Create Note On message
     * @param channel MIDI channel (0-15)
     * @param note Note number (0-127)
     * @param velocity Velocity (0-127)
     * @return MidiMessage Note On message
     */
    static MidiMessage noteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    
    /**
     * @brief Create Note Off message
     * @param channel MIDI channel (0-15)
     * @param note Note number (0-127)
     * @param velocity Release velocity (0-127, default: 0)
     * @return MidiMessage Note Off message
     */
    static MidiMessage noteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0);
    
    /**
     * @brief Create Control Change message
     * @param channel MIDI channel (0-15)
     * @param controller Controller number (0-127)
     * @param value Controller value (0-127)
     * @return MidiMessage Control Change message
     */
    static MidiMessage controlChange(uint8_t channel, uint8_t controller, uint8_t value);
    
    /**
     * @brief Create Control Change message (with enum)
     * @param channel MIDI channel (0-15)
     * @param type Controller type
     * @param value Controller value (0-127)
     * @return MidiMessage Control Change message
     */
    static MidiMessage controlChange(uint8_t channel, ControllerType type, uint8_t value);
    
    /**
     * @brief Create Program Change message
     * @param channel MIDI channel (0-15)
     * @param program Program number (0-127)
     * @return MidiMessage Program Change message
     */
    static MidiMessage programChange(uint8_t channel, uint8_t program);
    
    /**
     * @brief Create Channel Pressure message
     * @param channel MIDI channel (0-15)
     * @param pressure Pressure value (0-127)
     * @return MidiMessage Channel Pressure message
     */
    static MidiMessage channelPressure(uint8_t channel, uint8_t pressure);
    
    /**
     * @brief Create Pitch Bend message
     * @param channel MIDI channel (0-15)
     * @param value Pitch bend value (-8192 to 8191, 0 = center)
     * @return MidiMessage Pitch Bend message
     */
    static MidiMessage pitchBend(uint8_t channel, int16_t value);
    
    /**
     * @brief Create Polyphonic Aftertouch message
     * @param channel MIDI channel (0-15)
     * @param note Note number (0-127)
     * @param pressure Pressure value (0-127)
     * @return MidiMessage Poly Pressure message
     */
    static MidiMessage polyPressure(uint8_t channel, uint8_t note, uint8_t pressure);
    
    // ========================================================================
    // FACTORY METHODS - SYSTEM REAL-TIME
    // ========================================================================
    
    /**
     * @brief Create Clock message
     */
    static MidiMessage clock();
    
    /**
     * @brief Create Start message
     */
    static MidiMessage start();
    
    /**
     * @brief Create Continue message
     */
    static MidiMessage continueMsg();
    
    /**
     * @brief Create Stop message
     */
    static MidiMessage stop();
    
    /**
     * @brief Create Active Sensing message
     */
    static MidiMessage activeSensing();
    
    /**
     * @brief Create System Reset message
     */
    static MidiMessage systemReset();
    
    // ========================================================================
    // FACTORY METHODS - HELPERS
    // ========================================================================
    
    /**
     * @brief Create All Notes Off message
     * @param channel MIDI channel (0-15)
     * @return MidiMessage All Notes Off CC message
     */
    static MidiMessage allNotesOff(uint8_t channel);
    
    /**
     * @brief Create All Sound Off message
     * @param channel MIDI channel (0-15)
     * @return MidiMessage All Sound Off CC message
     */
    static MidiMessage allSoundOff(uint8_t channel);
    
    /**
     * @brief Create Reset All Controllers message
     * @param channel MIDI channel (0-15)
     * @return MidiMessage Reset All Controllers CC message
     */
    static MidiMessage resetAllControllers(uint8_t channel);
    
    // ========================================================================
    // GETTERS
    // ========================================================================
    
    /**
     * @brief Get message type
     * @return MidiMessageType Message type
     */
    MidiMessageType getType() const;
    
    /**
     * @brief Get status byte
     * @return uint8_t Status byte
     */
    uint8_t getStatus() const;
    
    /**
     * @brief Get MIDI channel
     * @return int Channel (0-15), or -1 if not a channel message
     */
    int getChannel() const;
    
    /**
     * @brief Get first data byte
     * @return uint8_t First data byte (or 0 if none)
     */
    uint8_t getData1() const;
    
    /**
     * @brief Get second data byte
     * @return uint8_t Second data byte (or 0 if none)
     */
    uint8_t getData2() const;
    
    /**
     * @brief Get raw data
     * @return const std::vector<uint8_t>& Raw MIDI bytes
     */
    const std::vector<uint8_t>& getRawData() const { return data_; }
    
    /**
     * @brief Get data size
     * @return size_t Number of bytes
     */
    size_t getSize() const { return data_.size(); }
    
    /**
     * @brief Get timestamp
     * @return uint64_t Timestamp in microseconds
     */
    uint64_t getTimestamp() const { return timestamp_; }
    
    /**
     * @brief Set timestamp
     * @param timestamp Timestamp in microseconds
     */
    void setTimestamp(uint64_t timestamp) { timestamp_ = timestamp; }
    
    // ========================================================================
    // PREDICATES
    // ========================================================================
    
    /**
     * @brief Check if Note On
     * @return true if Note On with velocity > 0
     */
    bool isNoteOn() const;
    
    /**
     * @brief Check if Note Off
     * @return true if Note Off or Note On with velocity 0
     */
    bool isNoteOff() const;
    
    /**
     * @brief Check if Control Change
     */
    bool isControlChange() const;
    
    /**
     * @brief Check if Program Change
     */
    bool isProgramChange() const;
    
    /**
     * @brief Check if Pitch Bend
     */
    bool isPitchBend() const;
    
    /**
     * @brief Check if channel message
     */
    bool isChannelMessage() const;
    
    /**
     * @brief Check if system message
     */
    bool isSystemMessage() const;
    
    /**
     * @brief Check if real-time message
     */
    bool isRealTimeMessage() const;
    
    /**
     * @brief Check if SysEx
     */
    bool isSysEx() const;
    
    /**
     * @brief Check if valid message
     * @return true if message is valid MIDI
     */
    bool isValid() const;
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    /**
     * @brief Get type name as string
     * @return std::string Type name
     */
    std::string getTypeName() const;
    
    /**
     * @brief Convert to JSON
     * @return json JSON representation
     */
    json toJson() const;
    
    /**
     * @brief Create from JSON
     * @param j JSON object
     * @return MidiMessage Message
     */
    static MidiMessage fromJson(const json& j);
    
    /**
     * @brief Convert to hex string
     * @return std::string Hex representation (e.g., "90 3C 64")
     */
    std::string toHexString() const;
    
    /**
     * @brief Compare messages
     */
    bool operator==(const MidiMessage& other) const;
    bool operator!=(const MidiMessage& other) const { return !(*this == other); }
    
    /**
     * @brief Convert message type to string
     */
    static std::string messageTypeToString(MidiMessageType type);
    
private:
    // ========================================================================
    // PRIVATE HELPERS
    // ========================================================================
    
    /**
     * @brief Clamp value to 7-bit (0-127)
     */
    static uint8_t clamp7bit(uint8_t value);
    
    /**
     * @brief Clamp channel to 4-bit (0-15)
     */
    static uint8_t clampChannel(uint8_t channel);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Raw MIDI data bytes
    std::vector<uint8_t> data_;
    
    /// Timestamp (microseconds)
    uint64_t timestamp_;
};

} // namespace midiMind
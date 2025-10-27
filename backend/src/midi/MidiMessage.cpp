// ============================================================================
// File: backend/src/midi/MidiMessage.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of MidiMessage class.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Complete implementation
//   - Enhanced validation
//   - JSON support
//
// ============================================================================

#include "MidiMessage.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTORS
// ============================================================================

MidiMessage::MidiMessage()
    : timestamp_(0)
{
}

MidiMessage::MidiMessage(const std::vector<uint8_t>& data)
    : data_(data)
    , timestamp_(0)
{
}

MidiMessage::MidiMessage(std::vector<uint8_t>&& data)
    : data_(std::move(data))
    , timestamp_(0)
{
}

MidiMessage::MidiMessage(uint8_t status)
    : timestamp_(0)
{
    data_.push_back(status);
}

MidiMessage::MidiMessage(uint8_t status, uint8_t data1)
    : timestamp_(0)
{
    data_.push_back(status);
    data_.push_back(data1);
}

MidiMessage::MidiMessage(uint8_t status, uint8_t data1, uint8_t data2)
    : timestamp_(0)
{
    data_.push_back(status);
    data_.push_back(data1);
    data_.push_back(data2);
}

// ============================================================================
// FACTORY METHODS - CHANNEL VOICE
// ============================================================================

MidiMessage MidiMessage::noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    return MidiMessage(
        0x90 | clampChannel(channel),
        clamp7bit(note),
        clamp7bit(velocity)
    );
}

MidiMessage MidiMessage::noteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    return MidiMessage(
        0x80 | clampChannel(channel),
        clamp7bit(note),
        clamp7bit(velocity)
    );
}

MidiMessage MidiMessage::controlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    return MidiMessage(
        0xB0 | clampChannel(channel),
        clamp7bit(controller),
        clamp7bit(value)
    );
}

MidiMessage MidiMessage::controlChange(uint8_t channel, ControllerType type, uint8_t value) {
    return controlChange(channel, static_cast<uint8_t>(type), value);
}

MidiMessage MidiMessage::programChange(uint8_t channel, uint8_t program) {
    return MidiMessage(
        0xC0 | clampChannel(channel),
        clamp7bit(program)
    );
}

MidiMessage MidiMessage::channelPressure(uint8_t channel, uint8_t pressure) {
    return MidiMessage(
        0xD0 | clampChannel(channel),
        clamp7bit(pressure)
    );
}

MidiMessage MidiMessage::pitchBend(uint8_t channel, int16_t value) {
    // Clamp value
    if (value < -8192) value = -8192;
    if (value > 8191) value = 8191;
    
    // Convert to 14-bit value (0-16383)
    uint16_t bendValue = static_cast<uint16_t>(value + 8192);
    
    return MidiMessage(
        0xE0 | clampChannel(channel),
        bendValue & 0x7F,         // LSB
        (bendValue >> 7) & 0x7F   // MSB
    );
}

MidiMessage MidiMessage::polyPressure(uint8_t channel, uint8_t note, uint8_t pressure) {
    return MidiMessage(
        0xA0 | clampChannel(channel),
        clamp7bit(note),
        clamp7bit(pressure)
    );
}

// ============================================================================
// FACTORY METHODS - SYSTEM REAL-TIME
// ============================================================================

MidiMessage MidiMessage::clock() {
    return MidiMessage(0xF8);
}

MidiMessage MidiMessage::start() {
    return MidiMessage(0xFA);
}

MidiMessage MidiMessage::continueMsg() {
    return MidiMessage(0xFB);
}

MidiMessage MidiMessage::stop() {
    return MidiMessage(0xFC);
}

MidiMessage MidiMessage::activeSensing() {
    return MidiMessage(0xFE);
}

MidiMessage MidiMessage::systemReset() {
    return MidiMessage(0xFF);
}

// ============================================================================
// FACTORY METHODS - HELPERS
// ============================================================================

MidiMessage MidiMessage::allNotesOff(uint8_t channel) {
    return controlChange(channel, ControllerType::ALL_NOTES_OFF, 0);
}

MidiMessage MidiMessage::allSoundOff(uint8_t channel) {
    return controlChange(channel, ControllerType::ALL_SOUND_OFF, 0);
}

MidiMessage MidiMessage::resetAllControllers(uint8_t channel) {
    return controlChange(channel, ControllerType::RESET_ALL_CONTROLLERS, 0);
}

// ============================================================================
// GETTERS
// ============================================================================

MidiMessageType MidiMessage::getType() const {
    if (data_.empty()) return MidiMessageType::UNKNOWN;
    
    uint8_t status = data_[0];
    
    // Real-time messages
    if (status >= 0xF8) {
        switch (status) {
            case 0xF8: return MidiMessageType::CLOCK;
            case 0xFA: return MidiMessageType::START;
            case 0xFB: return MidiMessageType::CONTINUE;
            case 0xFC: return MidiMessageType::STOP;
            case 0xFE: return MidiMessageType::ACTIVE_SENSING;
            case 0xFF: return MidiMessageType::SYSTEM_RESET;
            default: return MidiMessageType::UNKNOWN;
        }
    }
    
    // System common messages
    if (status >= 0xF0) {
        switch (status) {
            case 0xF0: return MidiMessageType::SYSTEM_EXCLUSIVE;
            case 0xF1: return MidiMessageType::TIME_CODE;
            case 0xF2: return MidiMessageType::SONG_POSITION;
            case 0xF3: return MidiMessageType::SONG_SELECT;
            case 0xF6: return MidiMessageType::TUNE_REQUEST;
            case 0xF7: return MidiMessageType::EOX;
            default: return MidiMessageType::UNKNOWN;
        }
    }
    
    // Channel messages
    return static_cast<MidiMessageType>(status & 0xF0);
}

uint8_t MidiMessage::getStatus() const {
    return data_.empty() ? 0 : data_[0];
}

int MidiMessage::getChannel() const {
    if (data_.empty()) return -1;
    
    uint8_t status = data_[0];
    
    // System messages don't have channel
    if (status >= 0xF0) return -1;
    
    return status & 0x0F;
}

uint8_t MidiMessage::getData1() const {
    return data_.size() > 1 ? data_[1] : 0;
}

uint8_t MidiMessage::getData2() const {
    return data_.size() > 2 ? data_[2] : 0;
}

// ============================================================================
// PREDICATES
// ============================================================================

bool MidiMessage::isNoteOn() const {
    return getType() == MidiMessageType::NOTE_ON && getData2() > 0;
}

bool MidiMessage::isNoteOff() const {
    return getType() == MidiMessageType::NOTE_OFF ||
           (getType() == MidiMessageType::NOTE_ON && getData2() == 0);
}

bool MidiMessage::isControlChange() const {
    return getType() == MidiMessageType::CONTROL_CHANGE;
}

bool MidiMessage::isProgramChange() const {
    return getType() == MidiMessageType::PROGRAM_CHANGE;
}

bool MidiMessage::isPitchBend() const {
    return getType() == MidiMessageType::PITCH_BEND;
}

bool MidiMessage::isChannelMessage() const {
    if (data_.empty()) return false;
    uint8_t status = data_[0];
    return status >= 0x80 && status < 0xF0;
}

bool MidiMessage::isSystemMessage() const {
    if (data_.empty()) return false;
    return data_[0] >= 0xF0;
}

bool MidiMessage::isRealTimeMessage() const {
    if (data_.empty()) return false;
    return data_[0] >= 0xF8;
}

bool MidiMessage::isSysEx() const {
    return getType() == MidiMessageType::SYSTEM_EXCLUSIVE;
}

bool MidiMessage::isValid() const {
    if (data_.empty()) return false;
    
    uint8_t status = data_[0];
    
    // Must be a status byte
    if (status < 0x80) return false;
    
    // Validate data bytes are < 0x80 (except for SysEx)
    if (status != 0xF0 && status != 0xF7) {
        for (size_t i = 1; i < data_.size(); ++i) {
            if (data_[i] >= 0x80) return false;
        }
    }
    
    // Check size based on type
    if (status >= 0xF8) return data_.size() == 1;  // Real-time
    if (status == 0xF6) return data_.size() == 1;  // Tune Request
    if (status == 0xF1 || status == 0xF3) return data_.size() == 2;  // MTC, Song Select
    if (status == 0xF2) return data_.size() == 3;  // Song Position
    
    // Channel messages
    if ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) {
        return data_.size() == 2;  // Program Change, Channel Pressure
    }
    
    if (status < 0xF0) {
        return data_.size() == 3;  // Other channel messages
    }
    
    return true;
}

// ============================================================================
// UTILITIES
// ============================================================================

std::string MidiMessage::getTypeName() const {
    return messageTypeToString(getType());
}

std::string MidiMessage::messageTypeToString(MidiMessageType type) {
    switch (type) {
        case MidiMessageType::NOTE_OFF: return "NOTE_OFF";
        case MidiMessageType::NOTE_ON: return "NOTE_ON";
        case MidiMessageType::POLY_PRESSURE: return "POLY_PRESSURE";
        case MidiMessageType::CONTROL_CHANGE: return "CONTROL_CHANGE";
        case MidiMessageType::PROGRAM_CHANGE: return "PROGRAM_CHANGE";
        case MidiMessageType::CHANNEL_PRESSURE: return "CHANNEL_PRESSURE";
        case MidiMessageType::PITCH_BEND: return "PITCH_BEND";
        case MidiMessageType::SYSTEM_EXCLUSIVE: return "SYSEX";
        case MidiMessageType::TIME_CODE: return "TIME_CODE";
        case MidiMessageType::SONG_POSITION: return "SONG_POSITION";
        case MidiMessageType::SONG_SELECT: return "SONG_SELECT";
        case MidiMessageType::TUNE_REQUEST: return "TUNE_REQUEST";
        case MidiMessageType::EOX: return "EOX";
        case MidiMessageType::CLOCK: return "CLOCK";
        case MidiMessageType::START: return "START";
        case MidiMessageType::CONTINUE: return "CONTINUE";
        case MidiMessageType::STOP: return "STOP";
        case MidiMessageType::ACTIVE_SENSING: return "ACTIVE_SENSING";
        case MidiMessageType::SYSTEM_RESET: return "SYSTEM_RESET";
        default: return "UNKNOWN";
    }
}

json MidiMessage::toJson() const {
    json j;
    
    j["type"] = getTypeName();
    j["status"] = getStatus();
    j["size"] = data_.size();
    j["timestamp"] = timestamp_;
    
    // Channel if applicable
    int channel = getChannel();
    if (channel >= 0) {
        j["channel"] = channel + 1;  // Display as 1-16
    }
    
    // Type-specific data
    if (isNoteOn() || isNoteOff()) {
        j["note"] = getData1();
        j["velocity"] = getData2();
    } else if (isControlChange()) {
        j["controller"] = getData1();
        j["value"] = getData2();
    } else if (isProgramChange()) {
        j["program"] = getData1();
    } else if (isPitchBend()) {
        int16_t bend = (getData2() << 7) | getData1();
        j["pitch_bend"] = bend - 8192;
    }
    
    // Raw hex
    j["hex"] = toHexString();
    
    return j;
}

MidiMessage MidiMessage::fromJson(const json& j) {
    try {
        // Validation
        if (!j.is_object()) {
            return MidiMessage();
        }
        
        // Try to reconstruct from type
        if (j.contains("type")) {
            std::string type = j["type"];
            
            if (type == "NOTE_ON") {
                if (!j.contains("channel") || !j.contains("note") || !j.contains("velocity")) {
                    return MidiMessage();
                }
                int channel = j["channel"].get<int>() - 1;
                if (channel < 0 || channel > 15) return MidiMessage();
                
                return noteOn(channel, j["note"], j["velocity"]);
            } else if (type == "NOTE_OFF") {
                if (!j.contains("channel") || !j.contains("note")) {
                    return MidiMessage();
                }
                int channel = j["channel"].get<int>() - 1;
                if (channel < 0 || channel > 15) return MidiMessage();
                
                return noteOff(channel, j["note"], j.value("velocity", 0));
            } else if (type == "CONTROL_CHANGE") {
                if (!j.contains("channel") || !j.contains("controller") || !j.contains("value")) {
                    return MidiMessage();
                }
                int channel = j["channel"].get<int>() - 1;
                if (channel < 0 || channel > 15) return MidiMessage();
                
                return controlChange(channel,
                        j["controller"].get<uint8_t>(), 
                        j["value"].get<uint8_t>());
            } else if (type == "PROGRAM_CHANGE") {
                if (!j.contains("channel") || !j.contains("program")) {
                    return MidiMessage();
                }
                int channel = j["channel"].get<int>() - 1;
                if (channel < 0 || channel > 15) return MidiMessage();
                
                return programChange(channel, j["program"]);
            } else if (type == "PITCH_BEND") {
                if (!j.contains("channel") || !j.contains("pitch_bend")) {
                    return MidiMessage();
                }
                int channel = j["channel"].get<int>() - 1;
                if (channel < 0 || channel > 15) return MidiMessage();
                
                return pitchBend(channel, j["pitch_bend"]);
            }
        }
        
        // Fallback: reconstruct from hex
        if (j.contains("hex")) {
            std::string hex = j["hex"];
            std::vector<uint8_t> data;
            
            std::istringstream iss(hex);
            std::string byteStr;
            
            while (iss >> byteStr) {
                try {
                    int value = std::stoi(byteStr, nullptr, 16);
                    if (value < 0 || value > 255) {
                        return MidiMessage();
                    }
                    data.push_back(static_cast<uint8_t>(value));
                } catch (...) {
                    return MidiMessage();
                }
            }
            
            if (data.empty()) {
                return MidiMessage();
            }
            
            MidiMessage msg(std::move(data));
            if (j.contains("timestamp")) {
                msg.setTimestamp(j["timestamp"]);
            }
            return msg;
        }
    } catch (...) {
        // Any exception during parsing returns empty message
        return MidiMessage();
    }
    
    return MidiMessage();
}

std::string MidiMessage::toHexString() const {
    std::ostringstream oss;
    
    for (size_t i = 0; i < data_.size(); ++i) {
        if (i > 0) oss << " ";
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
            << static_cast<int>(data_[i]);
    }
    
    return oss.str();
}

bool MidiMessage::operator==(const MidiMessage& other) const {
    return data_ == other.data_ && timestamp_ == other.timestamp_;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

uint8_t MidiMessage::clamp7bit(uint8_t value) {
    return value & 0x7F;
}

uint8_t MidiMessage::clampChannel(uint8_t channel) {
    return channel & 0x0F;
}

} // namespace midiMind

// ============================================================================
// END OF FILE MidiMessage.cpp
// ============================================================================
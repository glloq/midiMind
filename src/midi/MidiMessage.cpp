// ============================================================================
// Fichier: src/midi/MidiMessage.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "MidiMessage.h"
#include "../core/Error.h"
#include "../core/StringUtils.h"
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

MidiMessage::MidiMessage()
    : timestamp_(0)
    , size_(0) {
    std::fill(data_.begin(), data_.end(), 0);
}

MidiMessage::MidiMessage(const std::vector<uint8_t>& data)
    : timestamp_(0)
    , size_(std::min(data.size(), MAX_MESSAGE_SIZE)) {
    
    std::copy(data.begin(), data.begin() + size_, data_.begin());
    std::fill(data_.begin() + size_, data_.end(), 0);
}

MidiMessage::MidiMessage(const uint8_t* data, size_t size)
    : timestamp_(0)
    , size_(std::min(size, MAX_MESSAGE_SIZE)) {
    
    std::copy(data, data + size_, data_.begin());
    std::fill(data_.begin() + size_, data_.end(), 0);
}

// ============================================================================
// CRÉATION DE MESSAGES
// ============================================================================

MidiMessage MidiMessage::noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    CHECK(channel >= 1 && channel <= 16, ErrorCode::MIDI_INVALID_CHANNEL, 
          "Invalid MIDI channel: " + std::to_string(channel));
    CHECK(note <= 127, ErrorCode::MIDI_INVALID_NOTE, 
          "Invalid MIDI note: " + std::to_string(note));
    CHECK(velocity <= 127, ErrorCode::MIDI_INVALID_VELOCITY,
          "Invalid velocity: " + std::to_string(velocity));
    
    MidiMessage msg;
    msg.data_[0] = 0x90 | (channel - 1); // Note On + channel
    msg.data_[1] = note;
    msg.data_[2] = velocity;
    msg.size_ = 3;
    return msg;
}

MidiMessage MidiMessage::noteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    CHECK(channel >= 1 && channel <= 16, ErrorCode::MIDI_INVALID_CHANNEL,
          "Invalid MIDI channel: " + std::to_string(channel));
    CHECK(note <= 127, ErrorCode::MIDI_INVALID_NOTE,
          "Invalid MIDI note: " + std::to_string(note));
    
    MidiMessage msg;
    msg.data_[0] = 0x80 | (channel - 1); // Note Off + channel
    msg.data_[1] = note;
    msg.data_[2] = velocity;
    msg.size_ = 3;
    return msg;
}

MidiMessage MidiMessage::controlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    CHECK(channel >= 1 && channel <= 16, ErrorCode::MIDI_INVALID_CHANNEL,
          "Invalid MIDI channel: " + std::to_string(channel));
    CHECK(controller <= 127, ErrorCode::MIDI_INVALID_ARGUMENT,
          "Invalid controller: " + std::to_string(controller));
    CHECK(value <= 127, ErrorCode::MIDI_INVALID_ARGUMENT,
          "Invalid value: " + std::to_string(value));
    
    MidiMessage msg;
    msg.data_[0] = 0xB0 | (channel - 1); // CC + channel
    msg.data_[1] = controller;
    msg.data_[2] = value;
    msg.size_ = 3;
    return msg;
}

MidiMessage MidiMessage::programChange(uint8_t channel, uint8_t program) {
    CHECK(channel >= 1 && channel <= 16, ErrorCode::MIDI_INVALID_CHANNEL,
          "Invalid MIDI channel: " + std::to_string(channel));
    CHECK(program <= 127, ErrorCode::MIDI_INVALID_ARGUMENT,
          "Invalid program: " + std::to_string(program));
    
    MidiMessage msg;
    msg.data_[0] = 0xC0 | (channel - 1); // PC + channel
    msg.data_[1] = program;
    msg.size_ = 2;
    return msg;
}

MidiMessage MidiMessage::pitchBend(uint8_t channel, uint16_t value) {
    CHECK(channel >= 1 && channel <= 16, ErrorCode::MIDI_INVALID_CHANNEL,
          "Invalid MIDI channel: " + std::to_string(channel));
    CHECK(value <= 16383, ErrorCode::MIDI_INVALID_ARGUMENT,
          "Invalid pitch bend value: " + std::to_string(value));
    
    MidiMessage msg;
    msg.data_[0] = 0xE0 | (channel - 1); // Pitch Bend + channel
    msg.data_[1] = value & 0x7F;         // LSB
    msg.data_[2] = (value >> 7) & 0x7F;  // MSB
    msg.size_ = 3;
    return msg;
}

MidiMessage MidiMessage::aftertouch(uint8_t channel, uint8_t pressure) {
    CHECK(channel >= 1 && channel <= 16, ErrorCode::MIDI_INVALID_CHANNEL,
          "Invalid MIDI channel: " + std::to_string(channel));
    CHECK(pressure <= 127, ErrorCode::MIDI_INVALID_ARGUMENT,
          "Invalid pressure: " + std::to_string(pressure));
    
    MidiMessage msg;
    msg.data_[0] = 0xD0 | (channel - 1); // Channel Aftertouch + channel
    msg.data_[1] = pressure;
    msg.size_ = 2;
    return msg;
}

MidiMessage MidiMessage::polyAftertouch(uint8_t channel, uint8_t note, uint8_t pressure) {
    CHECK(channel >= 1 && channel <= 16, ErrorCode::MIDI_INVALID_CHANNEL,
          "Invalid MIDI channel: " + std::to_string(channel));
    CHECK(note <= 127, ErrorCode::MIDI_INVALID_NOTE,
          "Invalid MIDI note: " + std::to_string(note));
    CHECK(pressure <= 127, ErrorCode::MIDI_INVALID_ARGUMENT,
          "Invalid pressure: " + std::to_string(pressure));
    
    MidiMessage msg;
    msg.data_[0] = 0xA0 | (channel - 1); // Poly Aftertouch + channel
    msg.data_[1] = note;
    msg.data_[2] = pressure;
    msg.size_ = 3;
    return msg;
}

MidiMessage MidiMessage::clock() {
    MidiMessage msg;
    msg.data_[0] = 0xF8; // Clock
    msg.size_ = 1;
    return msg;
}

MidiMessage MidiMessage::start() {
    MidiMessage msg;
    msg.data_[0] = 0xFA; // Start
    msg.size_ = 1;
    return msg;
}

MidiMessage MidiMessage::stop() {
    MidiMessage msg;
    msg.data_[0] = 0xFC; // Stop
    msg.size_ = 1;
    return msg;
}

MidiMessage MidiMessage::continueMsg() {
    MidiMessage msg;
    msg.data_[0] = 0xFB; // Continue
    msg.size_ = 1;
    return msg;
}

MidiMessage MidiMessage::activeSensing() {
    MidiMessage msg;
    msg.data_[0] = 0xFE; // Active Sensing
    msg.size_ = 1;
    return msg;
}

MidiMessage MidiMessage::systemReset() {
    MidiMessage msg;
    msg.data_[0] = 0xFF; // System Reset
    msg.size_ = 1;
    return msg;
}

MidiMessage MidiMessage::allNotesOff(uint8_t channel) {
    return controlChange(channel, 123, 0); // CC 123 = All Notes Off
}

MidiMessage MidiMessage::allSoundOff(uint8_t channel) {
    return controlChange(channel, 120, 0); // CC 120 = All Sound Off
}

// ============================================================================
// SETTERS
// ============================================================================

void MidiMessage::setNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    *this = noteOn(channel, note, velocity);
}

void MidiMessage::setNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    *this = noteOff(channel, note, velocity);
}

void MidiMessage::setControlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    *this = controlChange(channel, controller, value);
}

void MidiMessage::setProgramChange(uint8_t channel, uint8_t program) {
    *this = programChange(channel, program);
}

void MidiMessage::setChannel(uint8_t channel) {
    CHECK(channel >= 1 && channel <= 16, ErrorCode::MIDI_INVALID_CHANNEL,
          "Invalid MIDI channel: " + std::to_string(channel));
    
    if (size_ > 0 && (data_[0] & 0xF0) != 0xF0) { // Si message channel
        data_[0] = (data_[0] & 0xF0) | (channel - 1);
    }
}

void MidiMessage::setTimestamp(uint64_t timestamp) {
    timestamp_ = timestamp;
}

// ============================================================================
// GETTERS
// ============================================================================

MidiMessageType MidiMessage::getType() const {
    if (size_ == 0) return MidiMessageType::INVALID;
    
    uint8_t status = data_[0];
    
    // Messages système
    if (status >= 0xF0) {
        switch (status) {
            case 0xF0: return MidiMessageType::SYSEX;
            case 0xF1: return MidiMessageType::SYSTEM_EXCLUSIVE;
            case 0xF2: return MidiMessageType::SONG_POSITION;
            case 0xF3: return MidiMessageType::SONG_SELECT;
            case 0xF6: return MidiMessageType::TUNE_REQUEST;
            case 0xF7: return MidiMessageType::SYSEX_END;
            case 0xF8: return MidiMessageType::CLOCK;
            case 0xFA: return MidiMessageType::START;
            case 0xFB: return MidiMessageType::CONTINUE;
            case 0xFC: return MidiMessageType::STOP;
            case 0xFE: return MidiMessageType::ACTIVE_SENSING;
            case 0xFF: return MidiMessageType::SYSTEM_RESET;
            default: return MidiMessageType::SYSTEM_EXCLUSIVE;
        }
    }
    
    // Messages channel
    uint8_t type = status & 0xF0;
    switch (type) {
        case 0x80: return MidiMessageType::NOTE_OFF;
        case 0x90: return data_[2] > 0 ? MidiMessageType::NOTE_ON : MidiMessageType::NOTE_OFF;
        case 0xA0: return MidiMessageType::POLY_AFTERTOUCH;
        case 0xB0: return MidiMessageType::CONTROL_CHANGE;
        case 0xC0: return MidiMessageType::PROGRAM_CHANGE;
        case 0xD0: return MidiMessageType::AFTERTOUCH;
        case 0xE0: return MidiMessageType::PITCH_BEND;
        default: return MidiMessageType::INVALID;
    }
}

uint8_t MidiMessage::getChannel() const {
    if (size_ == 0) return 0;
    
    uint8_t status = data_[0];
    
    // Messages système n'ont pas de channel
    if (status >= 0xF0) return 0;
    
    // Canal = bits 0-3, + 1 (canaux 1-16)
    return (status & 0x0F) + 1;
}

uint8_t MidiMessage::getNote() const {
    if (size_ < 2) return 0;
    
    MidiMessageType type = getType();
    if (type == MidiMessageType::NOTE_ON || 
        type == MidiMessageType::NOTE_OFF ||
        type == MidiMessageType::POLY_AFTERTOUCH) {
        return data_[1];
    }
    
    return 0;
}

uint8_t MidiMessage::getVelocity() const {
    if (size_ < 3) return 0;
    
    MidiMessageType type = getType();
    if (type == MidiMessageType::NOTE_ON || type == MidiMessageType::NOTE_OFF) {
        return data_[2];
    }
    
    return 0;
}

uint8_t MidiMessage::getController() const {
    if (size_ < 2) return 0;
    
    if (getType() == MidiMessageType::CONTROL_CHANGE) {
        return data_[1];
    }
    
    return 0;
}

uint8_t MidiMessage::getValue() const {
    if (size_ < 2) return 0;
    
    MidiMessageType type = getType();
    
    if (type == MidiMessageType::CONTROL_CHANGE ||
        type == MidiMessageType::PROGRAM_CHANGE ||
        type == MidiMessageType::AFTERTOUCH) {
        return size_ >= 3 ? data_[2] : data_[1];
    }
    
    return 0;
}

uint16_t MidiMessage::getPitchBend() const {
    if (size_ < 3) return 8192; // Centre
    
    if (getType() == MidiMessageType::PITCH_BEND) {
        return data_[1] | (data_[2] << 7);
    }
    
    return 8192;
}

const uint8_t* MidiMessage::getData() const {
    return data_.data();
}

size_t MidiMessage::getSize() const {
    return size_;
}

uint64_t MidiMessage::getTimestamp() const {
    return timestamp_;
}

// ============================================================================
// VÉRIFICATIONS
// ============================================================================

bool MidiMessage::isNoteOn() const {
    return getType() == MidiMessageType::NOTE_ON && getVelocity() > 0;
}

bool MidiMessage::isNoteOff() const {
    MidiMessageType type = getType();
    return type == MidiMessageType::NOTE_OFF || 
           (type == MidiMessageType::NOTE_ON && getVelocity() == 0);
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

bool MidiMessage::isAftertouch() const {
    MidiMessageType type = getType();
    return type == MidiMessageType::AFTERTOUCH || type == MidiMessageType::POLY_AFTERTOUCH;
}

bool MidiMessage::isClock() const {
    return getType() == MidiMessageType::CLOCK;
}

bool MidiMessage::isStart() const {
    return getType() == MidiMessageType::START;
}

bool MidiMessage::isStop() const {
    return getType() == MidiMessageType::STOP;
}

bool MidiMessage::isContinue() const {
    return getType() == MidiMessageType::CONTINUE;
}

bool MidiMessage::isSysEx() const {
    return getType() == MidiMessageType::SYSEX;
}

bool MidiMessage::isSystemMessage() const {
    return size_ > 0 && data_[0] >= 0xF0;
}

bool MidiMessage::isChannelMessage() const {
    return size_ > 0 && data_[0] < 0xF0;
}

bool MidiMessage::isValid() const {
    return size_ > 0 && getType() != MidiMessageType::INVALID;
}

// ============================================================================
// OPÉRATIONS
// ============================================================================

void MidiMessage::clear() {
    size_ = 0;
    timestamp_ = 0;
    std::fill(data_.begin(), data_.end(), 0);
}

MidiMessage MidiMessage::clone() const {
    MidiMessage copy;
    copy.data_ = data_;
    copy.size_ = size_;
    copy.timestamp_ = timestamp_;
    return copy;
}

// ============================================================================
// CONVERSION
// ============================================================================

std::string MidiMessage::toString() const {
    if (!isValid()) {
        return "INVALID";
    }
    
    std::ostringstream oss;
    
    MidiMessageType type = getType();
    
    switch (type) {
        case MidiMessageType::NOTE_ON:
            oss << "Note On  Ch:" << static_cast<int>(getChannel())
                << " Note:" << static_cast<int>(getNote())
                << " Vel:" << static_cast<int>(getVelocity());
            break;
            
        case MidiMessageType::NOTE_OFF:
            oss << "Note Off Ch:" << static_cast<int>(getChannel())
                << " Note:" << static_cast<int>(getNote())
                << " Vel:" << static_cast<int>(getVelocity());
            break;
            
        case MidiMessageType::CONTROL_CHANGE:
            oss << "CC       Ch:" << static_cast<int>(getChannel())
                << " CC:" << static_cast<int>(getController())
                << " Val:" << static_cast<int>(getValue());
            break;
            
        case MidiMessageType::PROGRAM_CHANGE:
            oss << "PC       Ch:" << static_cast<int>(getChannel())
                << " Prog:" << static_cast<int>(getValue());
            break;
            
        case MidiMessageType::PITCH_BEND:
            oss << "PB       Ch:" << static_cast<int>(getChannel())
                << " Val:" << getPitchBend();
            break;
            
        case MidiMessageType::AFTERTOUCH:
            oss << "AT       Ch:" << static_cast<int>(getChannel())
                << " Press:" << static_cast<int>(getValue());
            break;
            
        case MidiMessageType::POLY_AFTERTOUCH:
            oss << "Poly AT  Ch:" << static_cast<int>(getChannel())
                << " Note:" << static_cast<int>(getNote())
                << " Press:" << static_cast<int>(getValue());
            break;
            
        case MidiMessageType::CLOCK:
            oss << "Clock";
            break;
            
        case MidiMessageType::START:
            oss << "Start";
            break;
            
        case MidiMessageType::STOP:
            oss << "Stop";
            break;
            
        case MidiMessageType::CONTINUE:
            oss << "Continue";
            break;
            
        case MidiMessageType::SYSEX:
            oss << "SysEx (" << size_ << " bytes)";
            break;
            
        default:
            oss << "Unknown";
            break;
    }
    
    return oss.str();
}

std::string MidiMessage::toHexString() const {
    return StringUtils::bytesToHex(data_.data(), size_, " ");
}

json MidiMessage::toJson() const {
    json j;
    
    j["type"] = messageTypeToString(getType());
    j["timestamp"] = timestamp_;
    j["size"] = size_;
    j["data"] = toHexString();
    
    if (isChannelMessage()) {
        j["channel"] = getChannel();
        
        if (isNoteOn() || isNoteOff()) {
            j["note"] = getNote();
            j["velocity"] = getVelocity();
        } else if (isControlChange()) {
            j["controller"] = getController();
            j["value"] = getValue();
        } else if (isProgramChange()) {
            j["program"] = getValue();
        } else if (isPitchBend()) {
            j["pitch_bend"] = getPitchBend();
        }
    }
    
    return j;
}

std::string MidiMessage::messageTypeToString(MidiMessageType type) {
    switch (type) {
        case MidiMessageType::NOTE_OFF: return "NOTE_OFF";
        case MidiMessageType::NOTE_ON: return "NOTE_ON";
        case MidiMessageType::POLY_AFTERTOUCH: return "POLY_AFTERTOUCH";
        case MidiMessageType::CONTROL_CHANGE: return "CONTROL_CHANGE";
        case MidiMessageType::PROGRAM_CHANGE: return "PROGRAM_CHANGE";
        case MidiMessageType::AFTERTOUCH: return "AFTERTOUCH";
        case MidiMessageType::PITCH_BEND: return "PITCH_BEND";
        case MidiMessageType::SYSEX: return "SYSEX";
        case MidiMessageType::CLOCK: return "CLOCK";
        case MidiMessageType::START: return "START";
        case MidiMessageType::CONTINUE: return "CONTINUE";
        case MidiMessageType::STOP: return "STOP";
        case MidiMessageType::ACTIVE_SENSING: return "ACTIVE_SENSING";
        case MidiMessageType::SYSTEM_RESET: return "SYSTEM_RESET";
        default: return "UNKNOWN";
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiMessage.cpp
// ============================================================================
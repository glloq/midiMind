// ============================================================================
// Fichier: src/midi/MidiMessage.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "MidiMessage.h"
#include "../core/StringUtils.h"
#include "../core/TimeUtils.h"
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// CONSTRUCTEURS
// ============================================================================

MidiMessage::MidiMessage() 
    : timestamp_(TimeUtils::getCurrentTimestampUs()) {
    data_.reserve(3);  // Plupart des messages = 3 octets
}

MidiMessage::MidiMessage(const std::vector<uint8_t>& data)
    : data_(data)
    , timestamp_(TimeUtils::getCurrentTimestampUs()) {
}

MidiMessage::MidiMessage(uint8_t status, uint8_t data1, uint8_t data2)
    : data_{status, data1, data2, timestamp_(0)}
	captureTimestamp();
    , timestamp_(TimeUtils::getCurrentTimestampUs()) {
}

MidiMessage::MidiMessage(uint8_t status, uint8_t data1)
    : data_{status, data1}
    , timestamp_(TimeUtils::getCurrentTimestampUs()) {
}

MidiMessage::MidiMessage(uint8_t status)
    : data_{status}
    , timestamp_(TimeUtils::getCurrentTimestampUs()) {
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
    // Limiter la valeur
    if (value < -8192) value = -8192;
    if (value > 8191) value = 8191;
    
    // Convertir en valeur 14-bit (0-16383)
    uint16_t bendValue = static_cast<uint16_t>(value + 8192);
    
    return MidiMessage(
        0xE0 | clampChannel(channel),
        bendValue & 0x7F,         // LSB
        (bendValue >> 7) & 0x7F   // MSB
    );
}

// ============================================================================
// FACTORY METHODS - SYSTEM
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

MidiMessage MidiMessage::allNotesOff(uint8_t channel) {
    return controlChange(channel, ControllerType::ALL_NOTES_OFF, 0);
}

// ============================================================================
// GETTERS
// ============================================================================

MidiMessageType MidiMessage::getType() const {
    if (data_.empty()) return MidiMessageType::UNKNOWN;
    
    uint8_t status = data_[0];
    
    // Messages système en temps réel
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
    
    // Messages système communs
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
    
    // Messages canal
    return static_cast<MidiMessageType>(status & 0xF0);
}

uint8_t MidiMessage::getStatus() const {
    return data_.empty() ? 0 : data_[0];
}

int MidiMessage::getChannel() const {
    if (data_.empty()) return -1;
    
    uint8_t status = data_[0];
    
    // Les messages système n'ont pas de canal
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
// PRÉDICATS
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
    
    // Vérifier que c'est un status byte valide
    if (status < 0x80) return false;
    
    // Vérifier la taille selon le type
    if (status >= 0xF8) return data_.size() == 1;  // Real-time
    if (status == 0xF6) return data_.size() == 1;  // Tune Request
    if (status == 0xF1 || status == 0xF3) return data_.size() == 2;  // MTC, Song Select
    if (status == 0xF2) return data_.size() == 3;  // Song Position
    
    // Messages canal
    if ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) {
        return data_.size() == 2;  // Program Change, Channel Pressure
    }
    
    if (status < 0xF0) {
        return data_.size() == 3;  // Autres messages canal
    }
    
    return true;
}

// ============================================================================
// UTILITAIRES
// ============================================================================

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

std::string MidiMessage::getTypeName() const {
    return messageTypeToString(getType());
}

json MidiMessage::toJson() const {
    json j;
    
    j["type"] = getTypeName();
    j["status"] = getStatus();
    j["size"] = data_.size();
    j["timestamp"] = timestamp_;
    
    // Canal si applicable
    int channel = getChannel();
    if (channel >= 0) {
        j["channel"] = channel + 1;  // Afficher 1-16
    }
    
    // Données spécifiques selon le type
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
    
    // Données brutes en hex
    j["hex"] = toHexString();
    
    return j;
}

MidiMessage MidiMessage::fromJson(const json& j) {
    if (j.contains("hex")) {
        // Reconstruire depuis hex
        std::string hex = j["hex"];
        std::vector<uint8_t> data;
        
        // Parser l'hex string
        std::istringstream iss(hex);
        std::string byte;
        
        while (iss >> byte) {
            data.push_back(static_cast<uint8_t>(std::stoi(byte, nullptr, 16)));
        }
        
        return MidiMessage(data);
    }
    
    // Reconstruire depuis les champs
    std::string type = j["type"];
    
    if (type == "NOTE_ON") {
        return noteOn(j["channel"] - 1, j["note"], j["velocity"]);
    } else if (type == "NOTE_OFF") {
        return noteOff(j["channel"] - 1, j["note"], j.value("velocity", 0));
    } else if (type == "CONTROL_CHANGE") {
        return controlChange(j["channel"] - 1, j["controller"], j["value"]);
    } else if (type == "PROGRAM_CHANGE") {
        return programChange(j["channel"] - 1, j["program"]);
    }
     MidiMessage msg;
        
        if (j.contains("status")) msg.status_ = j["status"];
        if (j.contains("data1")) msg.data1_ = j["data1"];
        if (j.contains("data2")) msg.data2_ = j["data2"];
        
        // <-- AJOUTER CES LIGNES
        if (j.contains("timestamp")) {
            msg.timestamp_ = j["timestamp"];
        }
        
        return msg;
    // Type non supporté
    return MidiMessage();
}

std::string MidiMessage::toHexString() const {
    return StringUtils::bytesToHex(data_.data(), data_.size());
}

bool MidiMessage::operator==(const MidiMessage& other) const {
    return data_ == other.data_;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

uint8_t MidiMessage::clamp7bit(uint8_t value) {
    return value & 0x7F;
}

uint8_t MidiMessage::clampChannel(uint8_t channel) {
    return channel & 0x0F;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiMessage.cpp
// ============================================================================
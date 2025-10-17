// ============================================================================
// File: backend/src/midi/devices/VirtualMidiDevice.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of VirtualMidiDevice.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Complete implementation
//   - ALSA virtual port support
//   - Fallback for non-ALSA platforms
//
// ============================================================================

#include "VirtualMidiDevice.h"
#include "../../core/Logger.h"
#include <chrono>
#include <thread>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

VirtualMidiDevice::VirtualMidiDevice(const std::string& id, const std::string& name)
    : MidiDevice(id, name, DeviceType::VIRTUAL, DeviceDirection::BIDIRECTIONAL)
    , alsaSeq_(nullptr)
    , virtualPort_(-1)
    , isInput_(true)
    , isOutput_(true)
    , shouldStop_(false)
{
    Logger::info("VirtualMidiDevice", "Created: " + name);
}

VirtualMidiDevice::~VirtualMidiDevice() {
    disconnect();
}

// ============================================================================
// CONNECTION
// ============================================================================

bool VirtualMidiDevice::connect() {
    if (isConnected()) {
        Logger::warning("VirtualMidiDevice", "Already connected: " + name_);
        return true;
    }
    
    Logger::info("VirtualMidiDevice", "Creating virtual port: " + name_);
    
    status_ = DeviceStatus::CONNECTING;
    
#ifdef __linux__
    // 1. Open ALSA sequencer
    if (!openSequencer()) {
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    // 2. Create virtual port
    if (!createVirtualPort()) {
        closeSequencer();
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    // 3. Start receive thread
    shouldStop_ = false;
    receiveThread_ = std::thread(&VirtualMidiDevice::receiveThreadFunc, this);
    
    status_ = DeviceStatus::CONNECTED;
    
    Logger::info("VirtualMidiDevice", "✓ Virtual port created: " + name_);
    
    return true;
#else
    // Non-ALSA fallback: use queues only
    status_ = DeviceStatus::CONNECTED;
    
    Logger::warning("VirtualMidiDevice", 
                "ALSA not available, using queue-only mode");
    
    return true;
#endif
}

bool VirtualMidiDevice::disconnect() {
    if (status_ == DeviceStatus::DISCONNECTED) {
        return true;
    }
    
    Logger::info("VirtualMidiDevice", "Disconnecting virtual port: " + name_);
    
#ifdef __linux__
    // 1. Stop receive thread
    shouldStop_ = true;
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
    
    // 2. Delete virtual port
    deleteVirtualPort();
    
    // 3. Close sequencer
    closeSequencer();
#endif
    
    status_ = DeviceStatus::DISCONNECTED;
    
    Logger::info("VirtualMidiDevice", "✓ Virtual port disconnected: " + name_);
    
    return true;
}

bool VirtualMidiDevice::isConnected() const {
    return status_.load() == DeviceStatus::CONNECTED;
}

// ============================================================================
// MESSAGING
// ============================================================================

bool VirtualMidiDevice::sendMessage(const MidiMessage& message) {
    if (!isConnected()) {
        Logger::warning("VirtualMidiDevice", "Cannot send: not connected");
        return false;
    }
    
    if (!isOutput_) {
        Logger::warning("VirtualMidiDevice", "Cannot send: port is input-only");
        return false;
    }
    
#ifdef __linux__
    if (alsaSeq_ && virtualPort_ >= 0) {
        // Send via ALSA
        snd_seq_event_t ev;
        snd_seq_ev_clear(&ev);
        midiMessageToAlsaEvent(message, &ev);
        
        // Set source
        snd_seq_ev_set_source(&ev, virtualPort_);
        snd_seq_ev_set_subs(&ev);
        snd_seq_ev_set_direct(&ev);
        
        // Send event
        int result = snd_seq_event_output(alsaSeq_, &ev);
        if (result < 0) {
            Logger::error("VirtualMidiDevice", "Failed to send event: " + 
                         std::string(snd_strerror(result)));
            return false;
        }
        
        snd_seq_drain_output(alsaSeq_);
        
        messagesSent_++;
        return true;
    }
#endif
    
    // Fallback: queue mode
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    if (sendQueue_.size() >= MAX_QUEUE_SIZE) {
        Logger::warning("VirtualMidiDevice", "Send queue full, dropping message");
        return false;
    }
    
    sendQueue_.push(message);
    messagesSent_++;
    
    return true;
}

MidiMessage VirtualMidiDevice::receiveMessage() {
    std::lock_guard<std::mutex> lock(receiveMutex_);
    
    if (receiveQueue_.empty()) {
        return MidiMessage();
    }
    
    MidiMessage msg = receiveQueue_.front();
    receiveQueue_.pop();
    
    return msg;
}

bool VirtualMidiDevice::hasMessages() const {
    std::lock_guard<std::mutex> lock(receiveMutex_);
    return !receiveQueue_.empty();
}

// ============================================================================
// INFORMATION
// ============================================================================

std::string VirtualMidiDevice::getPort() const {
    return "virtual:" + std::to_string(virtualPort_);
}

json VirtualMidiDevice::getInfo() const {
    json info = MidiDevice::getInfo();
    
    info["virtual_port"] = virtualPort_;
    info["is_input"] = isInput_;
    info["is_output"] = isOutput_;
    info["receive_queue_size"] = receiveQueue_.size();
    info["send_queue_size"] = sendQueue_.size();
    
    return info;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void VirtualMidiDevice::setPortDirection(bool input, bool output) {
    isInput_ = input;
    isOutput_ = output;
    
    std::string dirStr;
    if (input && output) dirStr = "BIDIRECTIONAL";
    else if (input) dirStr = "INPUT";
    else if (output) dirStr = "OUTPUT";
    else dirStr = "NONE";
    
    Logger::info("VirtualMidiDevice", "Port direction set: " + dirStr);
}

size_t VirtualMidiDevice::getMessageCount() const {
    std::lock_guard<std::mutex> lock(receiveMutex_);
    return receiveQueue_.size();
}

void VirtualMidiDevice::clearMessages() {
    {
        std::lock_guard<std::mutex> lock(receiveMutex_);
        while (!receiveQueue_.empty()) {
            receiveQueue_.pop();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        while (!sendQueue_.empty()) {
            sendQueue_.pop();
        }
    }
    
    Logger::debug("VirtualMidiDevice", "Message queues cleared");
}

void VirtualMidiDevice::setMessageCallback(std::function<void(const MidiMessage&)> callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    messageCallback_ = callback;
}

// ============================================================================
// PRIVATE METHODS - ALSA
// ============================================================================

bool VirtualMidiDevice::openSequencer() {
#ifdef __linux__
    int result = snd_seq_open(&alsaSeq_, "default", SND_SEQ_OPEN_DUPLEX, 0);
    
    if (result < 0) {
        Logger::error("VirtualMidiDevice", 
            "Failed to open ALSA sequencer: " + std::string(snd_strerror(result)));
        return false;
    }
    
    // Set client name
    snd_seq_set_client_name(alsaSeq_, ("MidiMind_" + name_).c_str());
    
    Logger::debug("VirtualMidiDevice", "ALSA sequencer opened");
    return true;
#else
    return false;
#endif
}

void VirtualMidiDevice::closeSequencer() {
#ifdef __linux__
    if (alsaSeq_) {
        snd_seq_close(alsaSeq_);
        alsaSeq_ = nullptr;
        Logger::debug("VirtualMidiDevice", "ALSA sequencer closed");
    }
#endif
}

bool VirtualMidiDevice::createVirtualPort() {
#ifdef __linux__
    if (!alsaSeq_) {
        return false;
    }
    
    // Determine capabilities
    unsigned int caps = 0;
    if (isInput_) {
        caps |= SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
    }
    if (isOutput_) {
        caps |= SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
    }
    
    // Create virtual port
    virtualPort_ = snd_seq_create_simple_port(
        alsaSeq_,
        name_.c_str(),
        caps,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION
    );
    
    if (virtualPort_ < 0) {
        Logger::error("VirtualMidiDevice", 
            "Failed to create virtual port: " + std::string(snd_strerror(virtualPort_)));
        return false;
    }
    
    Logger::debug("VirtualMidiDevice", "Created virtual port: " + std::to_string(virtualPort_));
    return true;
#else
    return false;
#endif
}

void VirtualMidiDevice::deleteVirtualPort() {
#ifdef __linux__
    if (alsaSeq_ && virtualPort_ >= 0) {
        snd_seq_delete_simple_port(alsaSeq_, virtualPort_);
        virtualPort_ = -1;
        Logger::debug("VirtualMidiDevice", "Deleted virtual port");
    }
#endif
}

// ============================================================================
// PRIVATE METHODS - THREADING
// ============================================================================

void VirtualMidiDevice::receiveThreadFunc() {
    Logger::debug("VirtualMidiDevice", "Receive thread started");
    
#ifdef __linux__
    while (!shouldStop_) {
        if (!alsaSeq_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Poll for events
        snd_seq_event_t* ev = nullptr;
        int result = snd_seq_event_input(alsaSeq_, &ev);
        
        if (result < 0) {
            if (result == -EAGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            Logger::error("VirtualMidiDevice", 
                "Error receiving event: " + std::string(snd_strerror(result)));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        if (ev) {
            processAlsaEvent(ev);
        }
    }
#endif
    
    Logger::debug("VirtualMidiDevice", "Receive thread stopped");
}

void VirtualMidiDevice::processAlsaEvent(const snd_seq_event_t* ev) {
#ifdef __linux__
    if (!ev) return;
    
    // Convert to MidiMessage
    MidiMessage msg = alsaEventToMidiMessage(ev);
    
    if (msg.isValid()) {
        // Add to queue
        {
            std::lock_guard<std::mutex> lock(receiveMutex_);
            
            if (receiveQueue_.size() >= MAX_QUEUE_SIZE) {
                Logger::warning("VirtualMidiDevice", "Receive queue full, dropping message");
                return;
            }
            
            receiveQueue_.push(msg);
        }
        
        messagesReceived_++;
        
        // Call callback
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (messageCallback_) {
                messageCallback_(msg);
            }
        }
    }
#endif
}

// ============================================================================
// PRIVATE METHODS - CONVERSION
// ============================================================================

void VirtualMidiDevice::midiMessageToAlsaEvent(const MidiMessage& msg, snd_seq_event_t* ev) {
#ifdef __linux__
    if (!ev) return;
    
    int channel = msg.getChannel();
    if (channel < 0) channel = 0;
    
    switch (msg.getType()) {
        case MidiMessageType::NOTE_ON:
            snd_seq_ev_set_noteon(ev, channel, msg.getData1(), msg.getData2());
            break;
            
        case MidiMessageType::NOTE_OFF:
            snd_seq_ev_set_noteoff(ev, channel, msg.getData1(), msg.getData2());
            break;
            
        case MidiMessageType::CONTROL_CHANGE:
            snd_seq_ev_set_controller(ev, channel, msg.getData1(), msg.getData2());
            break;
            
        case MidiMessageType::PROGRAM_CHANGE:
            snd_seq_ev_set_pgmchange(ev, channel, msg.getData1());
            break;
            
        case MidiMessageType::CHANNEL_PRESSURE:
            snd_seq_ev_set_chanpress(ev, channel, msg.getData1());
            break;
            
        case MidiMessageType::PITCH_BEND: {
            int value = (msg.getData2() << 7) | msg.getData1();
            snd_seq_ev_set_pitchbend(ev, channel, value - 8192);
            break;
        }
            
        case MidiMessageType::POLY_PRESSURE:
            snd_seq_ev_set_keypress(ev, channel, msg.getData1(), msg.getData2());
            break;
            
        default:
            break;
    }
#endif
}

MidiMessage VirtualMidiDevice::alsaEventToMidiMessage(const snd_seq_event_t* ev) {
#ifdef __linux__
    if (!ev) return MidiMessage();
    
    uint8_t channel = ev->data.note.channel;
    
    switch (ev->type) {
        case SND_SEQ_EVENT_NOTEON:
            return MidiMessage::noteOn(channel, 
                                      ev->data.note.note, 
                                      ev->data.note.velocity);
            
        case SND_SEQ_EVENT_NOTEOFF:
            return MidiMessage::noteOff(channel, 
                                       ev->data.note.note, 
                                       ev->data.note.velocity);
            
        case SND_SEQ_EVENT_CONTROLLER:
            return MidiMessage::controlChange(channel,
                                             ev->data.control.param,
                                             ev->data.control.value);
            
        case SND_SEQ_EVENT_PGMCHANGE:
            return MidiMessage::programChange(channel, ev->data.control.value);
            
        case SND_SEQ_EVENT_CHANPRESS:
            return MidiMessage::channelPressure(channel, ev->data.control.value);
            
        case SND_SEQ_EVENT_PITCHBEND: {
            int16_t bend = ev->data.control.value;
            return MidiMessage::pitchBend(channel, bend);
        }
            
        case SND_SEQ_EVENT_KEYPRESS:
            return MidiMessage::polyPressure(channel,
                                            ev->data.note.note,
                                            ev->data.note.velocity);
            
        default:
            return MidiMessage();
    }
#else
    return MidiMessage();
#endif
}

} // namespace midiMind

// ============================================================================
// END OF FILE VirtualMidiDevice.cpp
// ============================================================================
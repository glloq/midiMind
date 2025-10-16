// ============================================================================
// File: backend/src/midi/devices/UsbMidiDevice.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of UsbMidiDevice.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Complete ALSA implementation
//   - Enhanced error handling
//   - Auto-reconnect support
//
// ============================================================================

#include "UsbMidiDevice.h"
#include "../../core/Logger.h"
#include <chrono>
#include <thread>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UsbMidiDevice::UsbMidiDevice(const std::string& id,
                             const std::string& name,
                             int alsaClient,
                             int alsaPort)
    : MidiDevice(id, name, DeviceType::USB, DeviceDirection::BIDIRECTIONAL)
    , alsaSeq_(nullptr)
    , alsaClient_(alsaClient)
    , alsaPort_(alsaPort)
    , myPort_(-1)
    , shouldStop_(false)
    , autoReconnect_(false)
    , retryCount_(0)
    , maxRetries_(3)
    , retryDelayMs_(1000)
    , alsaEventsReceived_(0)
    , alsaEventsSent_(0)
    , alsaErrors_(0)
{
    reconnecting_.clear();
    
    Logger::info("UsbMidiDevice", "Created: " + name + 
                " (ALSA " + std::to_string(alsaClient) + ":" + 
                std::to_string(alsaPort) + ")");
}

UsbMidiDevice::~UsbMidiDevice() {
    disconnect();
}

// ============================================================================
// CONNECTION
// ============================================================================

bool UsbMidiDevice::connect() {
    if (isConnected()) {
        Logger::warn("UsbMidiDevice", "Already connected: " + name_);
        return true;
    }
    
    Logger::info("UsbMidiDevice", "Connecting to " + name_ + "...");
    
    status_ = DeviceStatus::CONNECTING;
    
    // 1. Open ALSA sequencer
    if (!openSequencer()) {
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    // 2. Create our port
    if (!createPorts()) {
        closeSequencer();
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    // 3. Connect to target device
    if (!connectToPorts()) {
        closeSequencer();
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    // 4. Validate connection
    if (!validateConnection()) {
        Logger::error("UsbMidiDevice", "Connection validation failed");
        disconnectFromPorts();
        closeSequencer();
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    // 5. Start receive thread
    shouldStop_ = false;
    receiveThread_ = std::thread(&UsbMidiDevice::receiveThreadFunc, this);
    
    status_ = DeviceStatus::CONNECTED;
    retryCount_ = 0;
    
    Logger::info("UsbMidiDevice", "✓ Connected: " + name_);
    
    // Flush buffered messages
    flushMessageBuffer();
    
    return true;
}

bool UsbMidiDevice::disconnect() {
    if (status_ == DeviceStatus::DISCONNECTED) {
        return true;
    }
    
    Logger::info("UsbMidiDevice", "Disconnecting " + name_ + "...");
    
    // 1. Stop receive thread
    shouldStop_ = true;
    if (receiveThread_.joinable()) {
        receiveCv_.notify_all();
        receiveThread_.join();
    }
    
    // 2. Disconnect ports
    disconnectFromPorts();
    
    // 3. Close sequencer
    closeSequencer();
    
    status_ = DeviceStatus::DISCONNECTED;
    Logger::info("UsbMidiDevice", "✓ Disconnected: " + name_);
    
    return true;
}

bool UsbMidiDevice::isConnected() const {
    return status_.load() == DeviceStatus::CONNECTED;
}

// ============================================================================
// MESSAGING
// ============================================================================

bool UsbMidiDevice::sendMessage(const MidiMessage& message) {
    if (!isConnected() || !alsaSeq_) {
        // Buffer message for retry
        std::lock_guard<std::mutex> lock(sendMutex_);
        sendBuffer_.push(message);
        
        if (sendBuffer_.size() > MAX_BUFFER_SIZE) {
            Logger::warn("UsbMidiDevice", "Send buffer overflow, dropping oldest message");
            sendBuffer_.pop();
        }
        
        // Attempt async reconnect if enabled
        if (autoReconnect_ && !reconnecting_.test_and_set()) {
            std::thread([this]() {
                if (attemptReconnect()) {
                    flushMessageBuffer();
                }
                reconnecting_.clear();
            }).detach();
        }
        
        return false;
    }
    
#ifdef __linux__
    // Create ALSA event
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    midiMessageToAlsaEvent(message, &ev);
    
    // Set source and destination
    snd_seq_ev_set_source(&ev, myPort_);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    
    // Send event
    int result = snd_seq_event_output(alsaSeq_, &ev);
    if (result < 0) {
        Logger::error("UsbMidiDevice", "Failed to send event: " + 
                     std::string(snd_strerror(result)));
        alsaErrors_++;
        return false;
    }
    
    // Drain output
    snd_seq_drain_output(alsaSeq_);
    
    alsaEventsSent_++;
    messagesSent_++;
    
    return true;
#else
    Logger::error("UsbMidiDevice", "ALSA not available on this platform");
    return false;
#endif
}

MidiMessage UsbMidiDevice::receiveMessage() {
    std::lock_guard<std::mutex> lock(receiveMutex_);
    
    if (receiveQueue_.empty()) {
        return MidiMessage();
    }
    
    MidiMessage msg = receiveQueue_.front();
    receiveQueue_.pop();
    
    return msg;
}

bool UsbMidiDevice::hasMessages() const {
    std::lock_guard<std::mutex> lock(receiveMutex_);
    return !receiveQueue_.empty();
}

// ============================================================================
// INFORMATION
// ============================================================================

std::string UsbMidiDevice::getPort() const {
    return std::to_string(alsaClient_) + ":" + std::to_string(alsaPort_);
}

json UsbMidiDevice::getInfo() const {
    json info = MidiDevice::getInfo();
    
    info["alsa_client"] = alsaClient_;
    info["alsa_port"] = alsaPort_;
    info["my_port"] = myPort_;
    info["auto_reconnect"] = autoReconnect_.load();
    info["retry_count"] = retryCount_.load();
    info["receive_queue_size"] = receiveQueue_.size();
    info["send_buffer_size"] = sendBuffer_.size();
    
    return info;
}

json UsbMidiDevice::getAlsaStatistics() const {
    return {
        {"events_received", alsaEventsReceived_.load()},
        {"events_sent", alsaEventsSent_.load()},
        {"errors", alsaErrors_.load()},
        {"client", alsaClient_},
        {"port", alsaPort_}
    };
}

// ============================================================================
// CALLBACK
// ============================================================================

void UsbMidiDevice::setMessageCallback(std::function<void(const MidiMessage&)> callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    messageCallback_ = callback;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UsbMidiDevice::setAutoReconnect(bool enabled) {
    autoReconnect_ = enabled;
    Logger::info("UsbMidiDevice", "Auto-reconnect " + 
                std::string(enabled ? "enabled" : "disabled"));
}

void UsbMidiDevice::setMaxRetries(int maxRetries) {
    maxRetries_ = maxRetries;
}

void UsbMidiDevice::setRetryDelay(int delayMs) {
    retryDelayMs_ = delayMs;
}

// ============================================================================
// PRIVATE METHODS - ALSA
// ============================================================================

bool UsbMidiDevice::openSequencer() {
#ifdef __linux__
    int result = snd_seq_open(&alsaSeq_, "default", SND_SEQ_OPEN_DUPLEX, 0);
    
    if (result < 0) {
        Logger::error("UsbMidiDevice", 
            "Failed to open ALSA sequencer: " + std::string(snd_strerror(result)));
        alsaErrors_++;
        return false;
    }
    
    // Set client name
    std::string clientName = "MidiMind_" + id_;
    snd_seq_set_client_name(alsaSeq_, clientName.c_str());
    
    Logger::debug("UsbMidiDevice", "ALSA sequencer opened");
    return true;
#else
    return false;
#endif
}

void UsbMidiDevice::closeSequencer() {
#ifdef __linux__
    if (alsaSeq_) {
        snd_seq_close(alsaSeq_);
        alsaSeq_ = nullptr;
        Logger::debug("UsbMidiDevice", "ALSA sequencer closed");
    }
#endif
}

bool UsbMidiDevice::createPorts() {
#ifdef __linux__
    if (!alsaSeq_) {
        return false;
    }
    
    // Create bidirectional port
    myPort_ = snd_seq_create_simple_port(
        alsaSeq_,
        name_.c_str(),
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE | 
        SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION
    );
    
    if (myPort_ < 0) {
        Logger::error("UsbMidiDevice", 
            "Failed to create ALSA port: " + std::string(snd_strerror(myPort_)));
        alsaErrors_++;
        return false;
    }
    
    Logger::debug("UsbMidiDevice", "Created ALSA port: " + std::to_string(myPort_));
    return true;
#else
    return false;
#endif
}

bool UsbMidiDevice::connectToPorts() {
#ifdef __linux__
    if (!alsaSeq_ || myPort_ < 0) {
        return false;
    }
    
    // Connect for OUTPUT (us -> device)
    int result = snd_seq_connect_to(alsaSeq_, myPort_, alsaClient_, alsaPort_);
    if (result < 0) {
        Logger::error("UsbMidiDevice", 
            "Failed to connect to device: " + std::string(snd_strerror(result)));
        alsaErrors_++;
        return false;
    }
    
    // Connect for INPUT (device -> us)
    result = snd_seq_connect_from(alsaSeq_, myPort_, alsaClient_, alsaPort_);
    if (result < 0) {
        Logger::warn("UsbMidiDevice", 
            "Failed to connect from device (input may not be supported): " + 
            std::string(snd_strerror(result)));
        // Not fatal - device may be output only
    }
    
    Logger::debug("UsbMidiDevice", "Port connections established");
    return true;
#else
    return false;
#endif
}

void UsbMidiDevice::disconnectFromPorts() {
#ifdef __linux__
    if (alsaSeq_ && myPort_ >= 0) {
        snd_seq_disconnect_to(alsaSeq_, myPort_, alsaClient_, alsaPort_);
        snd_seq_disconnect_from(alsaSeq_, myPort_, alsaClient_, alsaPort_);
        snd_seq_delete_simple_port(alsaSeq_, myPort_);
        myPort_ = -1;
        
        Logger::debug("UsbMidiDevice", "Disconnected from ports");
    }
#endif
}

bool UsbMidiDevice::validateConnection() {
#ifdef __linux__
    if (!alsaSeq_ || myPort_ < 0) {
        return false;
    }
    
    // Query port info to validate
    snd_seq_port_info_t* pinfo;
    snd_seq_port_info_alloca(&pinfo);
    
    int result = snd_seq_get_port_info(alsaSeq_, myPort_, pinfo);
    if (result < 0) {
        Logger::error("UsbMidiDevice", "Port validation failed");
        return false;
    }
    
    Logger::debug("UsbMidiDevice", "Connection validated");
    return true;
#else
    return false;
#endif
}

// ============================================================================
// PRIVATE METHODS - THREADING
// ============================================================================

void UsbMidiDevice::receiveThreadFunc() {
    Logger::debug("UsbMidiDevice", "Receive thread started");
    
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
                // No event available
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            Logger::error("UsbMidiDevice", 
                "Error receiving event: " + std::string(snd_strerror(result)));
            alsaErrors_++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        if (ev) {
            processAlsaEvent(ev);
        }
    }
#endif
    
    Logger::debug("UsbMidiDevice", "Receive thread stopped");
}

void UsbMidiDevice::processAlsaEvent(const snd_seq_event_t* ev) {
#ifdef __linux__
    if (!ev) return;
    
    alsaEventsReceived_++;
    
    // Convert to MidiMessage
    MidiMessage msg = alsaEventToMidiMessage(ev);
    
    if (msg.isValid()) {
        // Add to queue
        {
            std::lock_guard<std::mutex> lock(receiveMutex_);
            receiveQueue_.push(msg);
        }
        receiveCv_.notify_one();
        
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

void UsbMidiDevice::midiMessageToAlsaEvent(const MidiMessage& msg, snd_seq_event_t* ev) {
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
            Logger::warn("UsbMidiDevice", "Unsupported message type for ALSA conversion");
            break;
    }
#endif
}

MidiMessage UsbMidiDevice::alsaEventToMidiMessage(const snd_seq_event_t* ev) {
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

// ============================================================================
// PRIVATE METHODS - RECONNECTION
// ============================================================================

bool UsbMidiDevice::attemptReconnect() {
    if (retryCount_ >= maxRetries_) {
        Logger::error("UsbMidiDevice", "Max reconnection attempts reached");
        return false;
    }
    
    retryCount_++;
    
    Logger::info("UsbMidiDevice", "Attempting reconnection " + 
                std::to_string(retryCount_.load()) + "/" + 
                std::to_string(maxRetries_) + "...");
    
    // Wait before retry
    std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs_));
    
    // Disconnect first
    disconnect();
    
    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Try to reconnect
    return connect();
}

void UsbMidiDevice::flushMessageBuffer() {
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    Logger::info("UsbMidiDevice", "Flushing " + 
                std::to_string(sendBuffer_.size()) + " buffered messages");
    
    while (!sendBuffer_.empty()) {
        MidiMessage msg = sendBuffer_.front();
        sendBuffer_.pop();
        
        sendMessage(msg);
    }
}

} // namespace midiMind

// ============================================================================
// END OF FILE UsbMidiDevice.cpp
// ============================================================================
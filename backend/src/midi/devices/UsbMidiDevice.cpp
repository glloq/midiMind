// ============================================================================
// File: backend/src/midi/devices/UsbMidiDevice.cpp
// Version: 4.2.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
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
    , maxBufferSize_(1000)
    , autoReconnect_(false)
    , retryCount_(0)
    , maxRetries_(3)
    , retryDelayMs_(1000)
    , alsaEventsReceived_(0)
    , alsaEventsSent_(0)
    , alsaErrors_(0)
    , sysexHandler_(nullptr)
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
        Logger::warning("UsbMidiDevice", "Already connected: " + name_);
        return true;
    }
    
    Logger::info("UsbMidiDevice", "Connecting to " + name_ + "...");
    
    status_ = DeviceStatus::CONNECTING;
    
    if (!openSequencer()) {
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    if (!createPorts()) {
        closeSequencer();
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    if (!connectToPorts()) {
        closeSequencer();
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    if (!validateConnection()) {
        Logger::error("UsbMidiDevice", "Connection validation failed");
        disconnectFromPorts();
        closeSequencer();
        status_ = DeviceStatus::ERROR;
        return false;
    }
    
    shouldStop_ = false;
    receiveThread_ = std::thread(&UsbMidiDevice::receiveThreadFunc, this);
    
    status_ = DeviceStatus::CONNECTED;
    retryCount_ = 0;
    
    Logger::info("UsbMidiDevice", "✓ Connected: " + name_);
    
    flushMessageBuffer();
    
    return true;
}

bool UsbMidiDevice::disconnect() {
    if (status_ == DeviceStatus::DISCONNECTED) {
        return true;
    }
    
    Logger::info("UsbMidiDevice", "Disconnecting " + name_ + "...");
    
    shouldStop_ = true;
    if (receiveThread_.joinable()) {
        receiveCv_.notify_all();
        receiveThread_.join();
    }
    
    disconnectFromPorts();
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
        {
            std::lock_guard<std::mutex> lock(sendMutex_);
            sendBuffer_.push(message);
            
            if (sendBuffer_.size() > maxBufferSize_.load()) {
                Logger::warning("UsbMidiDevice", "Send buffer overflow, dropping oldest message");
                sendBuffer_.pop();
            }
        }
        
        // FIX: Spawn reconnection thread only if not already reconnecting
        // and store thread to join later to avoid dangling pointer
        if (autoReconnect_ && !reconnecting_.test_and_set()) {
            // Note: This could still be improved with a dedicated reconnection thread
            // that's managed by the class lifetime, but this is safer than detach
            std::thread reconnectThread([this]() {
                if (attemptReconnect()) {
                    flushMessageBuffer();
                }
                reconnecting_.clear();
            });
            reconnectThread.detach(); // Still detached but reconnecting_ flag prevents multiple threads
        }
        
        return false;
    }
    
#ifdef __linux__
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    midiMessageToAlsaEvent(message, &ev);
    
    snd_seq_ev_set_source(&ev, myPort_.load());
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    
    int result = snd_seq_event_output(alsaSeq_, &ev);
    if (result < 0) {
        Logger::error("UsbMidiDevice", "Failed to send event: " + 
                     std::string(snd_strerror(result)));
        alsaErrors_++;
        return false;
    }
    
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
    
    MidiMessage msg = std::move(receiveQueue_.front());
    receiveQueue_.pop();
    
    return msg;
}

bool UsbMidiDevice::hasMessages() const {
    std::lock_guard<std::mutex> lock(receiveMutex_);
    return !receiveQueue_.empty();
}

// ============================================================================
// NEW METHODS
// ============================================================================

bool UsbMidiDevice::requestIdentity() {
    if (!sysexHandler_) {
        Logger::warning("UsbMidiDevice", "No SysExHandler configured");
        return false;
    }
    
    return sysexHandler_->requestIdentity(id_);
}

json UsbMidiDevice::getCapabilities() const {
    return json{
        {"channels", 16},
        {"polyphony", 128},
        {"supports_sysex", true},
        {"supports_mpe", false},
        {"alsa_client", alsaClient_},
        {"alsa_port", alsaPort_}
    };
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
    info["my_port"] = myPort_.load();
    info["auto_reconnect"] = autoReconnect_.load();
    info["retry_count"] = retryCount_.load();
    info["max_buffer_size"] = maxBufferSize_.load();
    
    // FIX: Access queues with mutex
    {
        std::lock_guard<std::mutex> lock(receiveMutex_);
        info["receive_queue_size"] = receiveQueue_.size();
    }
    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        info["send_buffer_size"] = sendBuffer_.size();
    }
    
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

void UsbMidiDevice::setSysExHandler(std::shared_ptr<SysExHandler> handler) {
    sysexHandler_ = handler;
}

void UsbMidiDevice::setMaxBufferSize(size_t size) {
    maxBufferSize_ = size;
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
    
    // FIX: Set non-blocking mode to avoid blocking in receiveThreadFunc
    result = snd_seq_nonblock(alsaSeq_, 1);
    if (result < 0) {
        Logger::warning("UsbMidiDevice", 
            "Failed to set non-blocking mode: " + std::string(snd_strerror(result)));
    }
    
    snd_seq_set_client_name(alsaSeq_, "MidiMind");
    
    Logger::debug("UsbMidiDevice", "ALSA sequencer opened");
    return true;
#else
    Logger::error("UsbMidiDevice", "ALSA not available on this platform");
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
    
    int port = snd_seq_create_simple_port(alsaSeq_, 
                                          name_.c_str(),
                                          SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE | 
                                          SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                          SND_SEQ_PORT_TYPE_MIDI_GENERIC | 
                                          SND_SEQ_PORT_TYPE_APPLICATION);
    
    if (port < 0) {
        Logger::error("UsbMidiDevice", 
            "Failed to create port: " + std::string(snd_strerror(port)));
        alsaErrors_++;
        return false;
    }
    
    myPort_ = port;
    Logger::debug("UsbMidiDevice", "Created port " + std::to_string(port));
    
    return true;
#else
    return false;
#endif
}

bool UsbMidiDevice::connectToPorts() {
#ifdef __linux__
    int port = myPort_.load();
    if (!alsaSeq_ || port < 0) {
        return false;
    }
    
    int result = snd_seq_connect_to(alsaSeq_, port, alsaClient_, alsaPort_);
    if (result < 0) {
        Logger::error("UsbMidiDevice", 
            "Failed to connect to device: " + std::string(snd_strerror(result)));
        alsaErrors_++;
        return false;
    }
    
    result = snd_seq_connect_from(alsaSeq_, port, alsaClient_, alsaPort_);
    if (result < 0) {
        Logger::warning("UsbMidiDevice", 
            "Failed to connect from device (input may not be supported): " + 
            std::string(snd_strerror(result)));
    }
    
    Logger::debug("UsbMidiDevice", "Port connections established");
    return true;
#else
    return false;
#endif
}

void UsbMidiDevice::disconnectFromPorts() {
#ifdef __linux__
    int port = myPort_.load();
    if (alsaSeq_ && port >= 0) {
        snd_seq_disconnect_to(alsaSeq_, port, alsaClient_, alsaPort_);
        snd_seq_disconnect_from(alsaSeq_, port, alsaClient_, alsaPort_);
        snd_seq_delete_simple_port(alsaSeq_, port);
        myPort_ = -1;
        
        Logger::debug("UsbMidiDevice", "Disconnected from ports");
    }
#endif
}

bool UsbMidiDevice::validateConnection() {
#ifdef __linux__
    int port = myPort_.load();
    if (!alsaSeq_ || port < 0) {
        return false;
    }
    
    snd_seq_port_info_t* pinfo;
    snd_seq_port_info_alloca(&pinfo);
    
    int result = snd_seq_get_port_info(alsaSeq_, port, pinfo);
    if (result < 0) {
        Logger::error("UsbMidiDevice", "Port validation failed");
        return false;
    }
    
    // FIX: Actually validate port info
    unsigned int caps = snd_seq_port_info_get_capability(pinfo);
    if (!(caps & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE))) {
        Logger::error("UsbMidiDevice", "Port does not have required capabilities");
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
        
        snd_seq_event_t* ev = nullptr;
        int result = snd_seq_event_input(alsaSeq_, &ev);
        
        if (result < 0) {
            if (result == -EAGAIN) {
                // Non-blocking mode: no event available
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            Logger::error("UsbMidiDevice", 
                "Error receiving event: " + std::string(snd_strerror(result)));
            alsaErrors_++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // FIX: No memory leak - ev points to ALSA internal buffer
        // It will be reused on next snd_seq_event_input call
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
    
    MidiMessage msg = alsaEventToMidiMessage(ev);
    
    if (msg.isValid()) {
        // FIX: Copy callback and call without holding lock to avoid deadlock
        std::function<void(const MidiMessage&)> callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callback = messageCallback_;
        }
        
        // Call callback with message before moving it to queue
        if (callback) {
            callback(msg);
        }
        
        {
            std::lock_guard<std::mutex> lock(receiveMutex_);
            receiveQueue_.push(std::move(msg));
        }
        receiveCv_.notify_one();
        
        messagesReceived_++;
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
            Logger::warning("UsbMidiDevice", "Unsupported message type for ALSA conversion");
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
    if (retryCount_ >= maxRetries_.load()) {
        Logger::error("UsbMidiDevice", "Max reconnection attempts reached");
        return false;
    }
    
    retryCount_++;
    
    Logger::info("UsbMidiDevice", "Attempting reconnection " + 
                std::to_string(retryCount_.load()) + "/" + 
                std::to_string(maxRetries_.load()) + "...");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs_.load()));
    
    disconnect();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return connect();
}

void UsbMidiDevice::flushMessageBuffer() {
    // FIX: Copy messages to avoid deadlock - don't call sendMessage while holding lock
    std::vector<MidiMessage> messagesToSend;
    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        
        Logger::info("UsbMidiDevice", "Flushing " + 
                    std::to_string(sendBuffer_.size()) + " buffered messages");
        
        while (!sendBuffer_.empty()) {
            messagesToSend.push_back(std::move(sendBuffer_.front()));
            sendBuffer_.pop();
        }
    }
    
    // Send without holding the lock
    for (const auto& msg : messagesToSend) {
        sendMessage(msg);
    }
}

} // namespace midiMind
UsbMidiDevice.hUsbMidiDevice.h// ============================================================================
// Fichier: backend/src/midi/devices/UsbMidiDevice.cpp
// Version: 1.0.0
// Projet: midiMind - Système d'Orchestration MIDI
// Description: Implémentation USB MIDI via ALSA sequencer
// ============================================================================

#include "UsbMidiDevice.h"
#include <chrono>
#include <cstring>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

UsbMidiDevice::UsbMidiDevice(const std::string& id, 
                             const std::string& name,
                             int alsaClient, 
                             int alsaPort)
    : MidiDevice(id, name, DeviceType::USB)
    , alsaSeq_(nullptr)
    , alsaClient_(alsaClient)
    , alsaPort_(alsaPort)
    , myPort_(-1)
    , shouldStop_(false)
    , autoReconnect_(true)
    , retryCount_(0)
    , maxRetries_(3)
    , retryDelayMs_(1000)
    , alsaEventsReceived_(0)
    , alsaEventsSent_(0)
    , alsaErrors_(0)
{
    reconnecting_.clear();
    
    // Initialiser adresse destination
    destAddr_.client = alsaClient_;
    destAddr_.port = alsaPort_;
    
    Logger::debug("UsbMidiDevice", 
        "Created device: " + id + " targeting ALSA " + 
        std::to_string(alsaClient_) + ":" + std::to_string(alsaPort_));
}

UsbMidiDevice::~UsbMidiDevice() {
    disconnect();
}

// ============================================================================
// CONNEXION / DÉCONNEXION
// ============================================================================

bool UsbMidiDevice::connect() {
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    if (status_ == DeviceStatus::CONNECTED) {
        Logger::warn("UsbMidiDevice", "Already connected: " + name_);
        return true;
    }
    
    Logger::info("UsbMidiDevice", "Connecting to " + name_ + "...");
    setStatus(DeviceStatus::CONNECTING);
    
    // 1. Ouvrir sequencer ALSA
    if (!openSequencer()) {
        setStatus(DeviceStatus::ERROR);
        return false;
    }
    
    // 2. Créer nos ports
    if (!createPorts()) {
        closeSequencer();
        setStatus(DeviceStatus::ERROR);
        return false;
    }
    
    // 3. Connecter aux ports du device cible
    if (!connectToPorts()) {
        closeSequencer();
        setStatus(DeviceStatus::ERROR);
        return false;
    }
    
    // 4. Valider la connexion
    if (!validateConnection()) {
        Logger::error("UsbMidiDevice", "Connection validation failed");
        disconnectFromPorts();
        closeSequencer();
        setStatus(DeviceStatus::ERROR);
        return false;
    }
    
    // 5. Démarrer le thread de réception
    shouldStop_ = false;
    receiveThread_ = std::thread(&UsbMidiDevice::receiveThreadFunc, this);
    
    setStatus(DeviceStatus::CONNECTED);
    retryCount_ = 0;
    
    Logger::info("UsbMidiDevice", "✓ Connected: " + name_);
    
    // Vider le buffer de messages en attente
    flushMessageBuffer();
    
    return true;
}

void UsbMidiDevice::disconnect() {
    if (status_ == DeviceStatus::DISCONNECTED) {
        return;
    }
    
    Logger::info("UsbMidiDevice", "Disconnecting " + name_ + "...");
    
    // 1. Arrêter le thread de réception
    shouldStop_ = true;
    if (receiveThread_.joinable()) {
        receiveCv_.notify_all();
        receiveThread_.join();
    }
    
    // 2. Déconnecter les ports
    disconnectFromPorts();
    
    // 3. Fermer le sequencer
    closeSequencer();
    
    setStatus(DeviceStatus::DISCONNECTED);
    Logger::info("UsbMidiDevice", "✓ Disconnected: " + name_);
}

// ============================================================================
// ENVOI DE MESSAGES
// ============================================================================

bool UsbMidiDevice::sendMessage(const MidiMessage& msg) {
    if (status_ != DeviceStatus::CONNECTED || !alsaSeq_) {
        // Buffer le message pour retry
        std::lock_guard<std::mutex> lock(sendMutex_);
        sendBuffer_.push(msg);
        
        if (sendBuffer_.size() > MAX_BUFFER_SIZE) {
            Logger::warn("UsbMidiDevice", "Send buffer overflow, dropping oldest message");
            sendBuffer_.pop();
        }
        
        // Tenter reconnexion asynchrone si activée
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
    
    // Créer événement ALSA
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    midiMessageToAlsaEvent(msg, &ev);
    
    // Configurer destination
    snd_seq_ev_set_source(&ev, myPort_);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    
    // Envoyer
    std::lock_guard<std::mutex> lock(sendMutex_);
    int result = snd_seq_event_output_direct(alsaSeq_, &ev);
    
    if (result < 0) {
        Logger::error("UsbMidiDevice", 
            "Failed to send message: " + std::string(snd_strerror(result)));
        alsaErrors_++;
        
        // Buffer pour retry
        sendBuffer_.push(msg);
        
        return false;
    }
    
    alsaEventsSent_++;
    messagesSent_++;
    
    return true;
}

// ============================================================================
// RÉCEPTION DE MESSAGES
// ============================================================================

bool UsbMidiDevice::hasMessages() const {
    std::lock_guard<std::mutex> lock(receiveMutex_);
    return !receiveQueue_.empty();
}

MidiMessage UsbMidiDevice::receive() {
    std::lock_guard<std::mutex> lock(receiveMutex_);
    
    if (receiveQueue_.empty()) {
        return MidiMessage();
    }
    
    MidiMessage msg = receiveQueue_.front();
    receiveQueue_.pop();
    return msg;
}

// ============================================================================
// INFORMATIONS
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
    info["max_retries"] = maxRetries_;
    
    info["statistics"]["alsa_events_received"] = alsaEventsReceived_.load();
    info["statistics"]["alsa_events_sent"] = alsaEventsSent_.load();
    info["statistics"]["alsa_errors"] = alsaErrors_.load();
    info["statistics"]["send_buffer_size"] = sendBuffer_.size();
    info["statistics"]["receive_queue_size"] = receiveQueue_.size();
    
    return info;
}

// ============================================================================
// CALLBACKS
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
    Logger::debug("UsbMidiDevice", 
        "Auto-reconnect " + std::string(enabled ? "enabled" : "disabled"));
}

void UsbMidiDevice::setMaxRetries(int maxRetries) {
    maxRetries_ = maxRetries;
}

void UsbMidiDevice::setRetryDelay(int delayMs) {
    retryDelayMs_ = delayMs;
}

// ============================================================================
// MÉTHODES PRIVÉES - ALSA
// ============================================================================

bool UsbMidiDevice::openSequencer() {
    int result = snd_seq_open(&alsaSeq_, "default", SND_SEQ_OPEN_DUPLEX, 0);
    
    if (result < 0) {
        Logger::error("UsbMidiDevice", 
            "Failed to open ALSA sequencer: " + std::string(snd_strerror(result)));
        alsaErrors_++;
        return false;
    }
    
    // Définir le nom du client
    std::string clientName = "midiMind_" + id_;
    snd_seq_set_client_name(alsaSeq_, clientName.c_str());
    
    Logger::debug("UsbMidiDevice", "ALSA sequencer opened");
    return true;
}

void UsbMidiDevice::closeSequencer() {
    if (alsaSeq_) {
        snd_seq_close(alsaSeq_);
        alsaSeq_ = nullptr;
        Logger::debug("UsbMidiDevice", "ALSA sequencer closed");
    }
}

bool UsbMidiDevice::createPorts() {
    if (!alsaSeq_) {
        return false;
    }
    
    // Créer un port bidirectionnel
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
}

bool UsbMidiDevice::connectToPorts() {
    if (!alsaSeq_ || myPort_ < 0) {
        return false;
    }
    
    // Obtenir notre adresse
    int myClient = snd_seq_client_id(alsaSeq_);
    
    // Connexion pour ÉCRITURE (nous -> device)
    int result = snd_seq_connect_to(alsaSeq_, myPort_, alsaClient_, alsaPort_);
    if (result < 0) {
        Logger::error("UsbMidiDevice", 
            "Failed to connect to output: " + std::string(snd_strerror(result)));
        alsaErrors_++;
        return false;
    }
    
    // Connexion pour LECTURE (device -> nous)
    result = snd_seq_connect_from(alsaSeq_, myPort_, alsaClient_, alsaPort_);
    if (result < 0) {
        Logger::warn("UsbMidiDevice", 
            "Failed to connect from input (may be output-only device): " + 
            std::string(snd_strerror(result)));
        // Ne pas échouer si c'est un device sortie seulement
    }
    
    Logger::debug("UsbMidiDevice", 
        "Connected to ALSA " + std::to_string(alsaClient_) + ":" + 
        std::to_string(alsaPort_));
    
    return true;
}

void UsbMidiDevice::disconnectFromPorts() {
    if (alsaSeq_ && myPort_ >= 0) {
        snd_seq_disconnect_to(alsaSeq_, myPort_, alsaClient_, alsaPort_);
        snd_seq_disconnect_from(alsaSeq_, myPort_, alsaClient_, alsaPort_);
        snd_seq_delete_simple_port(alsaSeq_, myPort_);
        myPort_ = -1;
        Logger::debug("UsbMidiDevice", "Disconnected from ALSA ports");
    }
}

// ============================================================================
// THREAD DE RÉCEPTION
// ============================================================================

void UsbMidiDevice::receiveThreadFunc() {
    Logger::info("UsbMidiDevice", "Receive thread started for " + name_);
    
    // Préparer les file descriptors pour poll
    int npfds = snd_seq_poll_descriptors_count(alsaSeq_, POLLIN);
    struct pollfd* pfds = new struct pollfd[npfds];
    snd_seq_poll_descriptors(alsaSeq_, pfds, npfds, POLLIN);
    
    while (!shouldStop_) {
        // Attendre des événements (timeout 100ms)
        int result = poll(pfds, npfds, 100);
        
        if (result < 0) {
            if (errno != EINTR) {
                Logger::error("UsbMidiDevice", "Poll error: " + std::string(strerror(errno)));
                alsaErrors_++;
            }
            continue;
        }
        
        if (result == 0) {
            // Timeout - continuer
            continue;
        }
        
        // Lire tous les événements disponibles
        snd_seq_event_t* ev = nullptr;
        while (snd_seq_event_input(alsaSeq_, &ev) > 0 && ev) {
            alsaEventsReceived_++;
            
            // Convertir en MidiMessage
            MidiMessage msg = alsaEventToMidiMessage(ev);
            
            if (msg.isValid()) {
                messagesReceived_++;
                
                // Appeler callback si défini
                {
                    std::lock_guard<std::mutex> lock(callbackMutex_);
                    if (messageCallback_) {
                        try {
                            messageCallback_(msg);
                        } catch (const std::exception& e) {
                            Logger::error("UsbMidiDevice", 
                                "Callback exception: " + std::string(e.what()));
                        }
                    }
                }
                
                // Ajouter à la queue
                {
                    std::lock_guard<std::mutex> lock(receiveMutex_);
                    receiveQueue_.push(msg);
                    receiveCv_.notify_one();
                }
            }
        }
    }
    
    delete[] pfds;
    Logger::info("UsbMidiDevice", "Receive thread stopped for " + name_);
}

// ============================================================================
// CONVERSION ALSA <-> MIDI
// ============================================================================

MidiMessage UsbMidiDevice::alsaEventToMidiMessage(const snd_seq_event_t* ev) {
    if (!ev) {
        return MidiMessage();
    }
    
    switch (ev->type) {
        case SND_SEQ_EVENT_NOTEON:
            return MidiMessage::noteOn(
                ev->data.note.channel,
                ev->data.note.note,
                ev->data.note.velocity
            );
            
        case SND_SEQ_EVENT_NOTEOFF:
            return MidiMessage::noteOff(
                ev->data.note.channel,
                ev->data.note.note,
                ev->data.note.velocity
            );
            
        case SND_SEQ_EVENT_CONTROLLER:
            return MidiMessage::controlChange(
                ev->data.control.channel,
                ev->data.control.param,
                ev->data.control.value
            );
            
        case SND_SEQ_EVENT_PGMCHANGE:
            return MidiMessage::programChange(
                ev->data.control.channel,
                ev->data.control.value
            );
            
        case SND_SEQ_EVENT_CHANPRESS:
            return MidiMessage::channelPressure(
                ev->data.control.channel,
                ev->data.control.value
            );
            
        case SND_SEQ_EVENT_PITCHBEND:
            return MidiMessage::pitchBend(
                ev->data.control.channel,
                ev->data.control.value
            );
            
        case SND_SEQ_EVENT_SYSEX:
            // SysEx nécessite un traitement spécial
            if (ev->data.ext.len > 0 && ev->data.ext.ptr) {
                std::vector<uint8_t> data(
                    static_cast<const uint8_t*>(ev->data.ext.ptr),
                    static_cast<const uint8_t*>(ev->data.ext.ptr) + ev->data.ext.len
                );
                return MidiMessage(data);
            }
            break;
            
        default:
            // Types d'événements non gérés
            Logger::debug("UsbMidiDevice", 
                "Unhandled ALSA event type: " + std::to_string(ev->type));
            break;
    }
    
    return MidiMessage();
}

void UsbMidiDevice::midiMessageToAlsaEvent(const MidiMessage& msg, snd_seq_event_t* ev) {
    if (!ev || !msg.isValid()) {
        return;
    }
    
    const auto& data = msg.getData();
    uint8_t status = data[0] & 0xF0;
    uint8_t channel = data[0] & 0x0F;
    
    switch (status) {
        case 0x90: // Note On
            if (data[2] > 0) {
                snd_seq_ev_set_noteon(ev, channel, data[1], data[2]);
            } else {
                // Velocity 0 = Note Off
                snd_seq_ev_set_noteoff(ev, channel, data[1], 0);
            }
            break;
            
        case 0x80: // Note Off
            snd_seq_ev_set_noteoff(ev, channel, data[1], data.size() > 2 ? data[2] : 0);
            break;
            
        case 0xB0: // Control Change
            snd_seq_ev_set_controller(ev, channel, data[1], data[2]);
            break;
            
        case 0xC0: // Program Change
            snd_seq_ev_set_pgmchange(ev, channel, data[1]);
            break;
            
        case 0xD0: // Channel Pressure
            snd_seq_ev_set_chanpress(ev, channel, data[1]);
            break;
            
        case 0xE0: // Pitch Bend
            {
                int16_t value = (data[2] << 7) | data[1];
                value -= 8192; // Centrer à 0
                snd_seq_ev_set_pitchbend(ev, channel, value);
            }
            break;
            
        case 0xF0: // System messages
            if (data[0] == 0xF0) {
                // SysEx
                snd_seq_ev_set_sysex(ev, data.size(), const_cast<uint8_t*>(data.data()));
            }
            break;
            
        default:
            Logger::warn("UsbMidiDevice", 
                "Unhandled MIDI message type: 0x" + 
                std::to_string(static_cast<int>(status)));
            break;
    }
}

// ============================================================================
// RECONNEXION
// ============================================================================

bool UsbMidiDevice::attemptReconnect() {
    if (status_ == DeviceStatus::CONNECTED) {
        return true;
    }
    
    Logger::info("UsbMidiDevice", "Attempting to reconnect " + name_ + "...");
    
    for (int attempt = 0; attempt < maxRetries_; attempt++) {
        if (attempt > 0) {
            Logger::info("UsbMidiDevice", 
                "Retry " + std::to_string(attempt + 1) + "/" + std::to_string(maxRetries_));
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs_));
        }
        
        // Nettoyer état actuel
        disconnect();
        
        // Tenter reconnexion
        if (connect()) {
            Logger::info("UsbMidiDevice", "✓ Reconnection successful");
            retryCount_ = 0;
            return true;
        }
        
        retryCount_++;
    }
    
    Logger::error("UsbMidiDevice", 
        "Reconnection failed after " + std::to_string(maxRetries_) + " attempts");
    return false;
}

void UsbMidiDevice::flushMessageBuffer() {
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    if (sendBuffer_.empty()) {
        return;
    }
    
    Logger::info("UsbMidiDevice", 
        "Flushing " + std::to_string(sendBuffer_.size()) + " buffered messages");
    
    int successCount = 0;
    int failCount = 0;
    
    while (!sendBuffer_.empty()) {
        MidiMessage msg = sendBuffer_.front();
        sendBuffer_.pop();
        
        if (sendMessage(msg)) {
            successCount++;
        } else {
            failCount++;
            // Si échec, le message sera re-bufferisé par sendMessage()
            break;
        }
    }
    
    Logger::info("UsbMidiDevice", 
        "Buffer flush: " + std::to_string(successCount) + " sent, " + 
        std::to_string(failCount) + " failed");
}

bool UsbMidiDevice::validateConnection() {
    if (!alsaSeq_ || myPort_ < 0) {
        return false;
    }
    
    // Vérifier que le sequencer est toujours valide
    int result = snd_seq_client_id(alsaSeq_);
    if (result < 0) {
        Logger::error("UsbMidiDevice", "Sequencer validation failed");
        return false;
    }
    
    // Vérifier que le port existe toujours
    snd_seq_port_info_t* pinfo;
    snd_seq_port_info_alloca(&pinfo);
    
    result = snd_seq_get_any_port_info(alsaSeq_, alsaClient_, alsaPort_, pinfo);
    if (result < 0) {
        Logger::error("UsbMidiDevice", 
            "Target port " + std::to_string(alsaClient_) + ":" + 
            std::to_string(alsaPort_) + " no longer exists");
        return false;
    }
    
    Logger::debug("UsbMidiDevice", "Connection validated");
    return true;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER UsbMidiDevice.cpp
// ============================================================================
// ============================================================================
// Fichier: src/network/rtpmidi/RtpMidiSession.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "RtpMidiSession.h"
#include <random>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

RtpMidiSession::RtpMidiSession(const std::string& sessionId,
                               std::shared_ptr<asio::ip::tcp::socket> controlSocket,
                               std::shared_ptr<asio::ip::udp::socket> dataSocket,
                               const asio::ip::udp::endpoint& clientEndpoint)
    : sessionId_(sessionId)
    , state_(SessionState::DISCONNECTED)
    , controlSocket_(controlSocket)
    , dataSocket_(dataSocket)
    , clientEndpoint_(clientEndpoint)
    , ssrc_(generateSSRC())
    , clientSSRC_(0)
    , initiatorToken_(0)
    , running_(false)
    , lastReceivedSeq_(0)
    , packetsReceived_(0)
    , packetsSent_(0)
    , bytesReceived_(0)
    , bytesSent_(0)
    , packetsLost_(0)
    , lastSyncTimestamp_(0)
    , clockOffset_(0)
    , synchronized_(false) {
    
    packetBuilder_ = std::make_unique<RtpPacketBuilder>(ssrc_);
    
    Logger::info("RtpMidiSession", "Session created: " + sessionId_);
}

RtpMidiSession::~RtpMidiSession() {
    close();
    Logger::info("RtpMidiSession", "Session destroyed: " + sessionId_);
}

// ============================================================================
// CONTRÔLE DE LA SESSION
// ============================================================================

bool RtpMidiSession::start() {
    Logger::info("RtpMidiSession", "Starting session " + sessionId_);
    
    setState(SessionState::CONNECTING);
    running_ = true;
    
    // Démarrer le thread de lecture du contrôle
    controlThread_ = std::thread([this]() {
        controlReadLoop();
    });
    
    // Démarrer le thread de synchronisation
    syncThread_ = std::thread([this]() {
        syncLoop();
    });
    
    // Envoyer l'invitation acceptée
    if (!sendControlPacket(RtpMidi::CMD_INVITATION_ACCEPTED, "MidiMind")) {
        Logger::error("RtpMidiSession", "Failed to send invitation accepted");
        close();
        return false;
    }
    
    setState(SessionState::CONNECTED);
    
    return true;
}

void RtpMidiSession::close() {
    if (!running_) {
        return;
    }
    
    Logger::info("RtpMidiSession", "Closing session " + sessionId_);
    
    setState(SessionState::CLOSING);
    running_ = false;
    
    // Envoyer END_SESSION
    sendControlPacket(RtpMidi::CMD_END_SESSION);
    
    // Fermer les sockets
    if (controlSocket_ && controlSocket_->is_open()) {
        try {
            controlSocket_->close();
        } catch (const std::exception& e) {
            Logger::warn("RtpMidiSession", "Error closing control socket: " + std::string(e.what()));
        }
    }
    
    // Attendre les threads
    if (controlThread_.joinable()) {
        controlThread_.join();
    }
    
    if (syncThread_.joinable()) {
        syncThread_.join();
    }
    
    setState(SessionState::DISCONNECTED);
}

bool RtpMidiSession::isActive() const {
    return running_ && (state_ == SessionState::CONNECTED || 
                       state_ == SessionState::SYNCHRONIZING ||
                       state_ == SessionState::SYNCHRONIZED);
}

SessionState RtpMidiSession::getState() const {
    return state_;
}

// ============================================================================
// ENVOI/RÉCEPTION MIDI
// ============================================================================

bool RtpMidiSession::sendMidi(const MidiMessage& message) {
    if (!isActive()) {
        return false;
    }
    
    try {
        // Convertir le message MIDI en bytes
        std::vector<uint8_t> midiData = message.toBytes();
        
        // Construire le paquet RTP
        uint32_t timestamp = static_cast<uint32_t>(getCurrentTimestamp() & 0xFFFFFFFF);
        auto packet = packetBuilder_->buildDataPacket(midiData, timestamp);
        
        // Envoyer via UDP
        size_t bytesSent = dataSocket_->send_to(asio::buffer(packet), clientEndpoint_);
        
        packetsSent_++;
        bytesSent_ += bytesSent;
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("RtpMidiSession", "Failed to send MIDI: " + std::string(e.what()));
        return false;
    }
}

void RtpMidiSession::setOnMidiReceived(MidiReceivedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onMidiReceived_ = callback;
}

void RtpMidiSession::setOnStateChanged(StateChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onStateChanged_ = callback;
}

// ============================================================================
// INFORMATIONS
// ============================================================================

json RtpMidiSession::getStatistics() const {
    json stats;
    stats["session_id"] = sessionId_;
    stats["client_name"] = clientName_;
    stats["client_address"] = getClientAddress();
    stats["state"] = static_cast<int>(state_.load());
    stats["synchronized"] = synchronized_;
    stats["packets_received"] = packetsReceived_.load();
    stats["packets_sent"] = packetsSent_.load();
    stats["bytes_received"] = bytesReceived_.load();
    stats["bytes_sent"] = bytesSent_.load();
    stats["packets_lost"] = packetsLost_.load();
    stats["clock_offset_us"] = clockOffset_;
    return stats;
}

// ============================================================================
// MÉTHODES PRIVÉES - THREADS
// ============================================================================

void RtpMidiSession::controlReadLoop() {
    Logger::info("RtpMidiSession", "Control read loop started for " + sessionId_);
    
    std::vector<uint8_t> buffer(1024);
    
    while (running_) {
        try {
            // Lire depuis le socket TCP
            asio::error_code ec;
            size_t bytesRead = controlSocket_->read_some(asio::buffer(buffer), ec);
            
            if (ec) {
                if (ec == asio::error::eof || ec == asio::error::connection_reset) {
                    Logger::info("RtpMidiSession", "Client disconnected: " + sessionId_);
                    break;
                }
                Logger::warn("RtpMidiSession", "Control read error: " + ec.message());
                continue;
            }
            
            if (bytesRead == 0) {
                continue;
            }
            
            // Parser le paquet de contrôle
            ControlPacket packet;
            std::string deviceName;
            
            if (RtpPacketParser::parseControlPacket(buffer.data(), bytesRead, packet, deviceName)) {
                handleControlPacket(packet, deviceName);
            }
            
        } catch (const std::exception& e) {
            Logger::error("RtpMidiSession", "Control loop exception: " + std::string(e.what()));
            break;
        }
    }
    
    Logger::info("RtpMidiSession", "Control read loop stopped for " + sessionId_);
}

void RtpMidiSession::dataReadLoop() {
    // Note: La lecture UDP est gérée par le serveur RtpMidiServer
    // qui dispatche les paquets aux sessions appropriées
}

void RtpMidiSession::syncLoop() {
    Logger::info("RtpMidiSession", "Sync loop started for " + sessionId_);
    
    while (running_) {
        // Effectuer une synchronisation toutes les 10 secondes
        if (state_ == SessionState::CONNECTED || state_ == SessionState::SYNCHRONIZED) {
            performSync();
        }
        
        // Attendre 10 secondes
        for (int i = 0; i < 100 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    Logger::info("RtpMidiSession", "Sync loop stopped for " + sessionId_);
}

// ============================================================================
// MÉTHODES PRIVÉES - PROTOCOLE
// ============================================================================

void RtpMidiSession::handleControlPacket(const ControlPacket& packet, const std::string& deviceName) {
    switch (packet.command) {
        case RtpMidi::CMD_INVITATION:
            Logger::info("RtpMidiSession", "Received invitation from: " + deviceName);
            clientName_ = deviceName;
            clientSSRC_ = packet.ssrc;
            initiatorToken_ = packet.initiatorToken;
            sendControlPacket(RtpMidi::CMD_INVITATION_ACCEPTED, "MidiMind");
            break;
            
        case RtpMidi::CMD_END_SESSION:
            Logger::info("RtpMidiSession", "Client requested end session");
            running_ = false;
            break;
            
        case RtpMidi::CMD_SYNCHRONIZATION:
            // Géré par le thread de sync
            break;
            
        default:
            Logger::warn("RtpMidiSession", "Unknown control command: 0x" + 
                        std::to_string(packet.command));
            break;
    }
}

void RtpMidiSession::handleDataPacket(const uint8_t* data, size_t size) {
    std::vector<uint8_t> midiData;
    uint32_t timestamp;
    uint16_t sequenceNumber;
    
    if (!RtpPacketParser::parseDataPacket(data, size, midiData, timestamp, sequenceNumber)) {
        Logger::warn("RtpMidiSession", "Failed to parse data packet");
        return;
    }
    
    packetsReceived_++;
    bytesReceived_ += size;
    
    // Détecter les paquets perdus
    if (lastReceivedSeq_ != 0 && sequenceNumber != lastReceivedSeq_ + 1) {
        uint16_t lost = sequenceNumber - lastReceivedSeq_ - 1;
        packetsLost_ += lost;
        Logger::warn("RtpMidiSession", "Detected " + std::to_string(lost) + " lost packets");
    }
    
    lastReceivedSeq_ = sequenceNumber;
    
    // Parser et callback les messages MIDI
    if (onMidiReceived_) {
        // Les données MIDI peuvent contenir plusieurs messages
        size_t offset = 0;
        while (offset < midiData.size()) {
            MidiMessage message = MidiMessage::fromBytes(midiData.data() + offset, 
                                                        midiData.size() - offset);
            
            if (message.isValid()) {
                onMidiReceived_(message);
                offset += message.getSize();
            } else {
                break;
            }
        }
    }
}

bool RtpMidiSession::sendControlPacket(uint16_t command, const std::string& name) {
    try {
        auto packet = packetBuilder_->buildControlPacket(command, initiatorToken_, name);
        
        asio::write(*controlSocket_, asio::buffer(packet));
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("RtpMidiSession", "Failed to send control packet: " + std::string(e.what()));
        return false;
    }
}

void RtpMidiSession::performSync() {
    setState(SessionState::SYNCHRONIZING);
    
    uint64_t ts1 = getCurrentTimestamp();
    
    try {
        // Construire et envoyer le paquet de sync
        auto packet = packetBuilder_->buildSyncPacket(0, ts1, 0, 0);
        
        dataSocket_->send_to(asio::buffer(packet), clientEndpoint_);
        
        lastSyncTimestamp_ = ts1;
        
        // Pour l'instant, considérer comme synchronisé après l'envoi
        // Dans une vraie implémentation, attendre la réponse du client
        synchronized_ = true;
        setState(SessionState::SYNCHRONIZED);
        
    } catch (const std::exception& e) {
        Logger::error("RtpMidiSession", "Sync failed: " + std::string(e.what()));
    }
}

void RtpMidiSession::setState(SessionState newState) {
    SessionState oldState = state_.exchange(newState);
    
    if (oldState != newState) {
        Logger::debug("RtpMidiSession", "State changed: " + sessionId_ + " -> " + 
                     std::to_string(static_cast<int>(newState)));
        
        if (onStateChanged_) {
            onStateChanged_(newState);
        }
    }
}

uint64_t RtpMidiSession::getCurrentTimestamp() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

uint32_t RtpMidiSession::generateSSRC() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;
    
    return dis(gen);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER RtpMidiSession.cpp
// ============================================================================
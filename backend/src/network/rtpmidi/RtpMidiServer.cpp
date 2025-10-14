// ============================================================================
// Fichier: src/network/rtpmidi/RtpMidiServer.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "RtpMidiServer.h"
#include "../../core/Logger.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

RtpMidiServer::RtpMidiServer()
    : running_(false)
    , controlPort_(0)
    , dataPort_(0)
    , packetsReceived_(0)
    , packetsSent_(0)
    , bytesReceived_(0)
    , bytesSent_(0) {
    
    Logger::info("RtpMidiServer", "RtpMidiServer constructed");
}

RtpMidiServer::~RtpMidiServer() {
    stop();
    Logger::info("RtpMidiServer", "RtpMidiServer destroyed");
}

// ============================================================================
// CONTRÔLE DU SERVEUR
// ============================================================================

bool RtpMidiServer::start(uint16_t controlPort, const std::string& serviceName) {
    if (running_) {
        Logger::warn("RtpMidiServer", "Server already running");
        return false;
    }
    
    Logger::info("RtpMidiServer", "═══════════════════════════════════════");
    Logger::info("RtpMidiServer", "  Starting RTP-MIDI Server");
    Logger::info("RtpMidiServer", "═══════════════════════════════════════");
    
    controlPort_ = controlPort;
    dataPort_ = controlPort + 1;
    serviceName_ = serviceName;
    
    try {
        // Créer l'accepteur TCP pour le control port
        asio::ip::tcp::endpoint controlEndpoint(asio::ip::tcp::v4(), controlPort_);
        controlAcceptor_ = std::make_unique<asio::ip::tcp::acceptor>(ioContext_, controlEndpoint);
        
        Logger::info("RtpMidiServer", "Control port: " + std::to_string(controlPort_));
        
        // Créer le socket UDP pour le data port
        asio::ip::udp::endpoint dataEndpoint(asio::ip::udp::v4(), dataPort_);
        dataSocket_ = std::make_unique<asio::ip::udp::socket>(ioContext_, dataEndpoint);
        
        Logger::info("RtpMidiServer", "Data port: " + std::to_string(dataPort_));
        
    } catch (const std::exception& e) {
        Logger::error("RtpMidiServer", "Failed to bind ports: " + std::string(e.what()));
        return false;
    }
    
    running_ = true;
    
    // Démarrer les threads I/O
    ioThreads_.emplace_back([this]() {
        acceptLoop();
    });
    
    ioThreads_.emplace_back([this]() {
        receiveLoop();
    });
    
    // Thread pour le contexte asio
    ioThreads_.emplace_back([this]() {
        ioContext_.run();
    });
    
    Logger::info("RtpMidiServer", "✓ RTP-MIDI Server started");
    Logger::info("RtpMidiServer", "  Service: " + serviceName_);
    Logger::info("RtpMidiServer", "  Listening on ports " + std::to_string(controlPort_) + 
                                " (TCP) and " + std::to_string(dataPort_) + " (UDP)");
    
    return true;
}

void RtpMidiServer::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("RtpMidiServer", "Stopping RTP-MIDI Server...");
    
    running_ = false;
    
    // Fermer toutes les sessions
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& session : sessions_) {
            session->close();
        }
        sessions_.clear();
    }
    
    // Fermer les sockets
    if (controlAcceptor_) {
        controlAcceptor_->close();
    }
    
    if (dataSocket_) {
        dataSocket_->close();
    }
    
    // Arrêter le contexte asio
    ioContext_.stop();
    
    // Attendre les threads
    for (auto& thread : ioThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    ioThreads_.clear();
    
    Logger::info("RtpMidiServer", "✓ RTP-MIDI Server stopped");
}

bool RtpMidiServer::isRunning() const {
    return running_;
}

// ============================================================================
// ENVOI DE MESSAGES
// ============================================================================

void RtpMidiServer::sendToAll(const MidiMessage& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& session : sessions_) {
        if (session->isActive()) {
            session->sendMidi(message);
        }
    }
}

bool RtpMidiServer::sendToSession(const MidiMessage& message, const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(sessions_.begin(), sessions_.end(),
        [&sessionId](const auto& session) {
            return session->getId() == sessionId;
        });
    
    if (it != sessions_.end() && (*it)->isActive()) {
        return (*it)->sendMidi(message);
    }
    
    return false;
}

// ============================================================================
// GESTION DES SESSIONS
// ============================================================================

std::vector<std::string> RtpMidiServer::listSessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> result;
    result.reserve(sessions_.size());
    
    for (const auto& session : sessions_) {
        if (session->isActive()) {
            result.push_back(session->getId());
        }
    }
    
    return result;
}

json RtpMidiServer::getSessionInfo(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(sessions_.begin(), sessions_.end(),
        [&sessionId](const auto& session) {
            return session->getId() == sessionId;
        });
    
    if (it != sessions_.end()) {
        return (*it)->getStatistics();
    }
    
    return json::object();
}

bool RtpMidiServer::closeSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(sessions_.begin(), sessions_.end(),
        [&sessionId](const auto& session) {
            return session->getId() == sessionId;
        });
    
    if (it != sessions_.end()) {
        (*it)->close();
        sessions_.erase(it);
        Logger::info("RtpMidiServer", "Session closed: " + sessionId);
        return true;
    }
    
    return false;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void RtpMidiServer::setOnMidiReceived(MidiReceivedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onMidiReceived_ = callback;
}

void RtpMidiServer::setOnClientConnected(ClientConnectedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onClientConnected_ = callback;
}

void RtpMidiServer::setOnClientDisconnected(ClientDisconnectedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onClientDisconnected_ = callback;
}

// ============================================================================
// STATISTIQUES
// ============================================================================


// ============================================================================
// AMÉLIORATION: getStatistics() avec calcul du packet loss
// ============================================================================

json RtpMidiServer::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    
    // Compter les sessions actives
    int activeSessions = 0;
    for (const auto& session : sessions_) {
        if (session->isActive()) {
            activeSessions++;
        }
    }
    stats["active_sessions"] = activeSessions;
    
    // Statistiques de base
    stats["packets_received"] = packetsReceived_.load();
    stats["packets_sent"] = packetsSent_.load();
    stats["bytes_received"] = bytesReceived_.load();
    stats["bytes_sent"] = bytesSent_.load();
    
    // ========================================================================
    // AMÉLIORATION: Calcul réel du packet loss
    // ========================================================================
    
    uint64_t totalPackets = packetsReceived_.load();
    uint64_t lostPackets = packetsLost_.load();
    
    if (totalPackets > 0) {
        // Packet loss rate en pourcentage
        double lossRate = (static_cast<double>(lostPackets) / 
                          static_cast<double>(totalPackets + lostPackets)) * 100.0;
        
        stats["packet_loss_rate"] = lossRate;
        stats["packets_lost"] = lostPackets;
    } else {
        stats["packet_loss_rate"] = 0.0;
        stats["packets_lost"] = 0;
    }
    
    // ========================================================================
    // Statistiques par session
    // ========================================================================
    
    json sessionsStats = json::array();
    for (const auto& session : sessions_) {
        if (session->isActive()) {
            json sessionInfo;
            sessionInfo["id"] = session->getId();
            sessionInfo["name"] = session->getName();
            sessionInfo["connected_time"] = session->getConnectedTime();
            sessionInfo["packets_rx"] = session->getPacketsReceived();
            sessionInfo["packets_tx"] = session->getPacketsSent();
            sessionInfo["latency_ms"] = session->getLatency();
            sessionInfo["jitter_ms"] = session->getJitter();
            
            sessionsStats.push_back(sessionInfo);
        }
    }
    stats["sessions"] = sessionsStats;
    
    // ========================================================================
    // Qualité de connexion moyenne
    // ========================================================================
    
    if (activeSessions > 0) {
        double avgLatency = 0.0;
        double avgJitter = 0.0;
        
        for (const auto& session : sessions_) {
            if (session->isActive()) {
                avgLatency += session->getLatency();
                avgJitter += session->getJitter();
            }
        }
        
        avgLatency /= activeSessions;
        avgJitter /= activeSessions;
        
        stats["avg_latency_ms"] = avgLatency;
        stats["avg_jitter_ms"] = avgJitter;
        
        // Déterminer la qualité globale
        std::string quality = "excellent";
        if (avgLatency > 100 || avgJitter > 20 || lostPackets > 0) {
            quality = "poor";
        } else if (avgLatency > 50 || avgJitter > 10) {
            quality = "fair";
        } else if (avgLatency > 20 || avgJitter > 5) {
            quality = "good";
        }
        
        stats["connection_quality"] = quality;
    } else {
        stats["avg_latency_ms"] = 0.0;
        stats["avg_jitter_ms"] = 0.0;
        stats["connection_quality"] = "no_connection";
    }
    
    // ========================================================================
    // Uptime du serveur
    // ========================================================================
    
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - startTime_
    ).count();
    
    stats["uptime_seconds"] = uptime;
    
    // ========================================================================
    // Taux de throughput (bytes/sec)
    // ========================================================================
    
    if (uptime > 0) {
        stats["rx_throughput_bps"] = (bytesReceived_.load() * 8) / uptime;
        stats["tx_throughput_bps"] = (bytesSent_.load() * 8) / uptime;
    } else {
        stats["rx_throughput_bps"] = 0;
        stats["tx_throughput_bps"] = 0;
    }
    
    return stats;
}

// ============================================================================
// NOUVELLE MÉTHODE: Tracking des sequences de packets
// ============================================================================

void RtpMidiServer::processIncomingPacket(const uint8_t* data, size_t length, 
                                          const std::string& sessionId) {
    if (length < 12) { // RTP header minimum size
        Logger::warn("RtpMidiServer", "Packet too small");
        return;
    }
    
    // ========================================================================
    // Parser le header RTP pour extraire le sequence number
    // ========================================================================
    
    // RTP Header format:
    // 0                   1                   2                   3
    // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |V=2|P|X|  CC   |M|     PT      |       sequence number         |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    
    uint16_t sequenceNumber = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    
    // ========================================================================
    // Détecter les packets perdus
    // ========================================================================
    
    {
        std::lock_guard<std::mutex> lock(sequenceMutex_);
        
        uint64_t expected = expectedSequenceNumber_.load();
        
        if (expected > 0) {
            // Calculer combien de packets ont été perdus
            int32_t diff = static_cast<int32_t>(sequenceNumber) - 
                           static_cast<int32_t>(expected);
            
            // Gérer le wrap-around du uint16_t
            if (diff < -30000) {
                diff += 65536;
            } else if (diff > 30000) {
                diff -= 65536;
            }
            
            if (diff > 1) {
                // On a perdu (diff - 1) packets
                uint64_t lost = diff - 1;
                packetsLost_ += lost;
                
                Logger::warn("RtpMidiServer", 
                    "Detected " + std::to_string(lost) + 
                    " lost packets (expected " + std::to_string(expected) + 
                    ", got " + std::to_string(sequenceNumber) + ")");
            } else if (diff < 0) {
                // Packet reordering ou duplicate - ignorer pour le calcul de perte
                Logger::debug("RtpMidiServer", 
                    "Out-of-order or duplicate packet: " + std::to_string(sequenceNumber));
            }
        }
        
        // Mettre à jour le prochain sequence number attendu
        expectedSequenceNumber_ = (sequenceNumber + 1) % 65536;
    }
    
    // Incrémenter les compteurs
    packetsReceived_++;
    bytesReceived_ += length;
    
    // ========================================================================
    // Traiter le payload MIDI
    // ========================================================================
    
    // ... (reste du traitement MIDI existant)
}

// ============================================================================
// AMÉLIORATION: Reset des statistiques
// ============================================================================

void RtpMidiServer::resetStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    packetsReceived_ = 0;
    packetsSent_ = 0;
    bytesReceived_ = 0;
    bytesSent_ = 0;
    packetsLost_ = 0;
    expectedSequenceNumber_ = 0;
    
    Logger::info("RtpMidiServer", "Statistics reset");
}

// ============================================================================
// AMÉLIORATION: Méthode helper pour obtenir uniquement le packet loss
// ============================================================================

double RtpMidiServer::getPacketLossRate() const {
    uint64_t totalPackets = packetsReceived_.load();
    uint64_t lostPackets = packetsLost_.load();
    
    if (totalPackets + lostPackets == 0) {
        return 0.0;
    }
    
    return (static_cast<double>(lostPackets) / 
            static_cast<double>(totalPackets + lostPackets)) * 100.0;
}
// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void RtpMidiServer::acceptLoop() {
    Logger::info("RtpMidiServer", "Accept loop started");
    
    while (running_) {
        try {
            // Créer un socket pour la nouvelle connexion
            auto socket = std::make_shared<asio::ip::tcp::socket>(ioContext_);
            
            // Accepter la connexion (bloquant)
            asio::error_code ec;
            controlAcceptor_->accept(*socket, ec);
            
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    break;
                }
                Logger::warn("RtpMidiServer", "Accept error: " + ec.message());
                continue;
            }
            
            // Gérer la nouvelle connexion
            handleNewConnection(socket);
            
        } catch (const std::exception& e) {
            if (running_) {
                Logger::error("RtpMidiServer", "Accept loop exception: " + std::string(e.what()));
            }
        }
    }
    
    Logger::info("RtpMidiServer", "Accept loop stopped");
}

void RtpMidiServer::handleNewConnection(std::shared_ptr<asio::ip::tcp::socket> socket) {
    // Récupérer l'adresse du client
    std::string clientAddr = socket->remote_endpoint().address().to_string();
    uint16_t clientPort = socket->remote_endpoint().port();
    
    Logger::info("RtpMidiServer", "New connection from " + clientAddr + ":" + 
                std::to_string(clientPort));
    
    // Créer un endpoint UDP pour le client (même IP, data port)
    asio::ip::udp::endpoint clientUdpEndpoint(
        asio::ip::address::from_string(clientAddr),
        dataPort_
    );
    
    // Générer un ID de session unique
    std::string sessionId = "session_" + std::to_string(sessions_.size() + 1);
    
    // Créer la session
    auto session = std::make_shared<RtpMidiSession>(
        sessionId,
        socket,
        dataSocket_,
        clientUdpEndpoint
    );
    
    // Configurer les callbacks
    session->setOnMidiReceived([this, sessionId](const MidiMessage& msg) {
        if (onMidiReceived_) {
            onMidiReceived_(msg, sessionId);
        }
    });
    
    session->setOnStateChanged([this, sessionId](SessionState state) {
        if (state == SessionState::CONNECTED && onClientConnected_) {
            // Récupérer le nom du client
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = std::find_if(sessions_.begin(), sessions_.end(),
                [&sessionId](const auto& s) { return s->getId() == sessionId; });
            
            if (it != sessions_.end()) {
                onClientConnected_(sessionId, (*it)->getClientName());
            }
        }
        else if (state == SessionState::DISCONNECTED && onClientDisconnected_) {
            onClientDisconnected_(sessionId);
        }
    });
    
    // Démarrer la session
    if (session->start()) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.push_back(session);
        Logger::info("RtpMidiServer", "Session started: " + sessionId);
    } else {
        Logger::error("RtpMidiServer", "Failed to start session");
    }
}

void RtpMidiServer::receiveLoop() {
    Logger::info("RtpMidiServer", "Receive loop started");
    
    std::vector<uint8_t> buffer(2048);
    asio::ip::udp::endpoint senderEndpoint;
    
    while (running_) {
        try {
            asio::error_code ec;
            size_t bytesReceived = dataSocket_->receive_from(
                asio::buffer(buffer),
                senderEndpoint,
                0,
                ec
            );
            
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    break;
                }
                Logger::warn("RtpMidiServer", "Receive error: " + ec.message());
                continue;
            }
            
            if (bytesReceived == 0) {
                continue;
            }
            
            packetsReceived_++;
            bytesReceived_ += bytesReceived;
            
            // Dispatcher le paquet à la session appropriée
            std::lock_guard<std::mutex> lock(mutex_);
            
            for (auto& session : sessions_) {
                if (session->isActive() && 
                    session->getClientAddress() == senderEndpoint.address().to_string()) {
                    
                    // Note: La session devrait avoir une méthode handleDataPacket publique
                    // Pour l'instant, le parsing est interne
                    break;
                }
            }
            
        } catch (const std::exception& e) {
            if (running_) {
                Logger::error("RtpMidiServer", "Receive loop exception: " + std::string(e.what()));
            }
        }
    }
    
    Logger::info("RtpMidiServer", "Receive loop stopped");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER RtpMidiServer.cpp
// ============================================================================
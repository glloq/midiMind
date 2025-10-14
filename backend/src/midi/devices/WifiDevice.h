// ============================================================================
// Fichier: backend/src/midi/devices/WifiDevice.h
// Version: 3.1.1
// Date: 13 Octobre 2025
// Corrections: Ajout reconnexion auto, buffer, métriques
// ============================================================================

#pragma once
#include "MidiDevice.h"
#include "../../network/discovery/MdnsDiscovery.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <queue>
#include <atomic>

namespace midiMind {

// ============================================================================
// WIFI MIDI DEVICE v3.1.1
// ============================================================================

class WifiMidiDevice : public MidiDevice {
public:
    WifiMidiDevice(const std::string& id, const std::string& name, 
                   const std::string& address, int port)
        : MidiDevice(id, name, DeviceType::NETWORK)
        , address_(address)
        , port_(port)
        , socket_(-1)
        , reconnectAttempts_(0)
        , maxReconnectAttempts_(5) {
        
        reconnecting_.clear();
    }
    
    ~WifiMidiDevice() {
        disconnect();
    }
    
    bool connect() override {
        if (isConnected()) {
            return true;
        }
        
        Logger::info("WifiDevice", "Connecting to " + address_ + ":" + std::to_string(port_));
        
        // Créer socket TCP
        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0) {
            Logger::error("WifiDevice", "Failed to create socket");
            return false;
        }
        
        // Configurer timeout
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        // Configurer adresse
        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port_);
        inet_pton(AF_INET, address_.c_str(), &serverAddr.sin_addr);
        
        // Connexion
        if (::connect(socket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            Logger::error("WifiDevice", "Connection failed: " + std::string(strerror(errno)));
            close(socket_);
            socket_ = -1;
            return false;
        }
        
        setStatus(DeviceStatus::CONNECTED);
        reconnectAttempts_ = 0;
        lastSuccessfulSend_ = std::chrono::steady_clock::now();
        
        Logger::info("WifiDevice", "✓ Connected to " + name_);
        
        return true;
    }
    
    void disconnect() override {
        if (socket_ >= 0) {
            close(socket_);
            socket_ = -1;
            setStatus(DeviceStatus::DISCONNECTED);
            Logger::info("WifiDevice", "Disconnected from " + name_);
        }
    }
    
    bool sendMessage(const MidiMessage& msg) override {
        if (!isConnected()) {
            // Buffer pour retry après reconnexion
            if (messageBuffer_.size() < MAX_BUFFER_SIZE) {
                messageBuffer_.push(msg);
                Logger::debug("WifiDevice", "Message buffered (not connected)");
            } else {
                Logger::warn("WifiDevice", "Buffer full, dropping message");
            }
            
            // Tenter reconnexion asynchrone
            if (!reconnecting_.test_and_set()) {
                std::thread([this]() {
                    if (reconnect()) {
                        flushBuffer();
                    }
                    reconnecting_.clear();
                }).detach();
            }
            
            return false;
        }
        
        const auto& data = msg.getData();
        ssize_t sent = send(socket_, data.data(), data.size(), MSG_NOSIGNAL);
        
        if (sent < 0) {
            // Erreur réseau détectée
            if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN) {
                Logger::warn("WifiDevice", "Connection lost: " + std::string(strerror(errno)));
                setStatus(DeviceStatus::DISCONNECTED);
                
                // Buffer le message
                if (messageBuffer_.size() < MAX_BUFFER_SIZE) {
                    messageBuffer_.push(msg);
                }
                
                // Tenter reconnexion
                if (!reconnecting_.test_and_set()) {
                    std::thread([this]() {
                        if (reconnect()) {
                            flushBuffer();
                        }
                        reconnecting_.clear();
                    }).detach();
                }
            }
            
            Logger::error("WifiDevice", "Send failed: " + std::string(strerror(errno)));
            return false;
        }
        
        if (sent == static_cast<ssize_t>(data.size())) {
            lastSuccessfulSend_ = std::chrono::steady_clock::now();
            return true;
        }
        
        Logger::warn("WifiDevice", "Partial send: " + std::to_string(sent) + 
                    "/" + std::to_string(data.size()) + " bytes");
        return false;
    }
    
    // Nouvelles méthodes v3.1.1
    bool reconnect() {
        if (reconnectAttempts_ >= maxReconnectAttempts_) {
            Logger::error("WifiDevice", "Max reconnect attempts reached");
            return false;
        }
        
        Logger::info("WifiDevice", "Reconnecting (attempt " + 
                    std::to_string(reconnectAttempts_ + 1) + ")...");
        
        disconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(500 * (reconnectAttempts_ + 1)));
        
        reconnectAttempts_++;
        
        if (connect()) {
            Logger::info("WifiDevice", "✓ Reconnected successfully");
            return true;
        }
        
        return false;
    }
    
    void flushBuffer() {
        Logger::info("WifiDevice", "Flushing " + std::to_string(messageBuffer_.size()) + 
                    " buffered messages");
        
        size_t successCount = 0;
        size_t failCount = 0;
        
        while (!messageBuffer_.empty() && isConnected()) {
            MidiMessage msg = messageBuffer_.front();
            messageBuffer_.pop();
            
            if (sendMessage(msg)) {
                successCount++;
            } else {
                failCount++;
                break; // Stop si échec
            }
        }
        
        Logger::info("WifiDevice", "Buffer flush: " + std::to_string(successCount) + 
                    " sent, " + std::to_string(failCount) + " failed");
    }

private:
    std::string address_;
    int port_;
    int socket_;
    
    // Reconnexion automatique v3.1.1
    std::queue<MidiMessage> messageBuffer_;
    static constexpr size_t MAX_BUFFER_SIZE = 1000;
    std::atomic_flag reconnecting_ = ATOMIC_FLAG_INIT;
    int reconnectAttempts_;
    int maxReconnectAttempts_;
    std::chrono::steady_clock::time_point lastSuccessfulSend_;
};

// ============================================================================
// mDNS DISCOVERY SERVICE v3.1.0 (inchangé, voir fichier 3 pour optimisations)
// ============================================================================

class MdnsDiscoveryHelper {
public:
    struct MidiService {
        std::string name;
        std::string hostname;
        std::string address;
        int port;
    };
    
    static std::vector<MidiService> discoverServices(int timeoutSeconds = 5);
    static bool testConnection(const std::string& ip, int port, int timeoutMs);
    static std::string getLocalIpAddress();
};

// ============================================================================
// WIFI DEVICE PLUGIN
// ============================================================================

class WifiDevicePlugin : public DevicePlugin {
public:
    std::string getName() const override {
        return "WiFi MIDI";
    }
    
    std::vector<DeviceInfo> scan() override {
        std::vector<DeviceInfo> devices;
        
        Logger::info("WifiPlugin", "Scanning for WiFi MIDI devices...");
        
        auto services = MdnsDiscoveryHelper::discoverServices(3);
        
        for (const auto& service : services) {
            DeviceInfo info;
            info.id = "wifi_" + service.address + "_" + std::to_string(service.port);
            info.name = service.name;
            info.type = DeviceType::NETWORK;
            info.metadata["address"] = service.address;
            info.metadata["port"] = service.port;
            info.metadata["hostname"] = service.hostname;
            
            devices.push_back(info);
        }
        
        Logger::info("WifiPlugin", "Found " + std::to_string(devices.size()) + " WiFi devices");
        
        return devices;
    }
    
    std::shared_ptr<MidiDevice> create(const DeviceInfo& info) override {
        std::string address = info.metadata.value("address", "");
        int port = info.metadata.value("port", 5004);
        
        return std::make_shared<WifiMidiDevice>(info.id, info.name, address, port);
    }
};

// Auto-registration
REGISTER_DEVICE_PLUGIN(WifiDevicePlugin);

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER WifiDevice.h v3.1.1
// ============================================================================
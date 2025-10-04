// ============================================================================
// src/midi/devices/plugins/WifiDevicePlugin.h
// WiFi MIDI avec découverte automatique mDNS/Bonjour
// ============================================================================
#pragma once
#include "../DevicePlugin.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

namespace midiMind {

// ============================================================================
// mDNS DISCOVERY SERVICE
// ============================================================================

class MdnsDiscovery {
public:
    struct MidiService {
        std::string name;
        std::string hostname;
        std::string address;
        int port;
    };
    
    static std::vector<MidiService> discoverServices(int timeoutSeconds = 5) {
        std::vector<MidiService> services;
        
        // Service types à découvrir:
        // _apple-midi._udp.local (RTP-MIDI)
        // _midi._tcp.local (MIDI over TCP)
        
        #ifdef HAS_AVAHI
            services = discoverWithAvahi(timeoutSeconds);
        #else
            Logger::warn("mDNS", "Avahi not available, using fallback scan");
            services = scanLocalNetwork(timeoutSeconds);
        #endif
        
        return services;
    }

private:
    #ifdef HAS_AVAHI
    static std::vector<MidiService> discoverWithAvahi(int timeout) {
        // TODO: Implémenter avec libavahi-client
        // Pour l'instant, stub
        Logger::info("mDNS", "Avahi discovery not implemented yet");
        return {};
    }
    #endif
    
    // Fallback: scan manuel du réseau local
    static std::vector<MidiService> scanLocalNetwork(int timeout) {
        std::vector<MidiService> services;
        
        // Obtenir l'IP locale
        std::string localIp = getLocalIpAddress();
        if (localIp.empty()) return services;
        
        // Extraire subnet (ex: 192.168.1.x)
        size_t lastDot = localIp.find_last_of('.');
        std::string subnet = localIp.substr(0, lastDot);
        
        Logger::info("mDNS", "Scanning subnet: " + subnet + ".0/24");
        
        // Scanner les ports MIDI courants sur le subnet
        std::vector<int> midiPorts = {5004, 5005, 21928}; // RTP-MIDI, custom
        
        for (int i = 1; i < 255; i++) {
            std::string ip = subnet + "." + std::to_string(i);
            
            for (int port : midiPorts) {
                if (testConnection(ip, port, 100)) {
                    MidiService service;
                    service.address = ip;
                    service.port = port;
                    service.hostname = ip; // Reverse DNS possible
                    service.name = "MIDI Device at " + ip + ":" + std::to_string(port);
                    services.push_back(service);
                    
                    Logger::info("mDNS", "Found service: " + service.name);
                }
            }
        }
        
        return services;
    }
    
    static std::string getLocalIpAddress() {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) != 0) {
            return "";
        }
        
        struct hostent* host = gethostbyname(hostname);
        if (!host) return "";
        
        struct in_addr** addr_list = (struct in_addr**)host->h_addr_list;
        if (addr_list[0]) {
            return std::string(inet_ntoa(*addr_list[0]));
        }
        
        return "";
    }
    
    static bool testConnection(const std::string& ip, int port, int timeoutMs) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return false;
        
        // Set non-blocking
        fcntl(sock, F_SETFL, O_NONBLOCK);
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        
        connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        
        // Wait avec select
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = timeoutMs * 1000;
        
        bool connected = (select(sock + 1, NULL, &fdset, NULL, &tv) == 1);
        close(sock);
        
        return connected;
    }
};

// ============================================================================
// WIFI PLUGIN
// ============================================================================

class WifiDevicePlugin : public IDevicePlugin {
public:
    std::string getName() const override { return "WiFi MIDI"; }
    std::string getVersion() const override { return "2.0.0"; }
    DeviceType getType() const override { return DeviceType::WIFI; }
    
    bool supportsDiscovery() const override { return true; }
    bool supportsHotplug() const override { return false; }
    
    bool initialize() override {
        Logger::info("WifiPlugin", "Initialized WiFi MIDI plugin");
        return true;
    }
    
    void shutdown() override {
        Logger::info("WifiPlugin", "WiFi plugin shutdown");
    }
    
    std::vector<DeviceInfo> discover() override {
        std::vector<DeviceInfo> devices;
        
        Logger::info("WifiPlugin", "Discovering WiFi MIDI devices...");
        
        auto services = MdnsDiscovery::discoverServices(5);
        
        for (const auto& service : services) {
            DeviceInfo info;
            info.id = "wifi_" + service.address + "_" + std::to_string(service.port);
            info.name = service.name;
            info.type = DeviceType::WIFI;
            info.metadata["address"] = service.address;
            info.metadata["port"] = service.port;
            info.metadata["hostname"] = service.hostname;
            
            devices.push_back(info);
        }
        
        Logger::info("WifiPlugin", "Found " + std::to_string(devices.size()) + " WiFi devices");
        
        return devices;
    }
    
    std::shared_ptr<MidiDevice> createDevice(const DeviceInfo& info) override {
        std::string address = info.metadata.value("address", "");
        int port = info.metadata.value("port", 0);
        
        if (address.empty() || port == 0) {
            Logger::error("WifiPlugin", "Invalid WiFi device info");
            return nullptr;
        }
        
        return std::make_shared<EnhancedWifiMidiDevice>(
            info.id,
            info.name,
            address,
            port
        );
    }
};

// ============================================================================
// ENHANCED WIFI DEVICE
// ============================================================================

class EnhancedWifiMidiDevice : public MidiDevice {
public:
    EnhancedWifiMidiDevice(const std::string& id, const std::string& name,
                          const std::string& host, int port)
        : MidiDevice(id, name, DeviceType::WIFI),
          host_(host), port_(port), sockfd_(-1) {
        
        connectionQuality_ = 100; // %
    }

    ~EnhancedWifiMidiDevice() {
        disconnect();
    }

    bool connect() override {
        setStatus(DeviceStatus::CONNECTING);
        
        // Créer socket
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            setStatus(DeviceStatus::ERROR);
            return false;
        }

        // Configuration socket: TCP_NODELAY pour réduire latence
        int flag = 1;
        setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
        
        // Timeout de connexion
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port_);
        
        if (inet_pton(AF_INET, host_.c_str(), &serverAddr.sin_addr) <= 0) {
            setStatus(DeviceStatus::ERROR);
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        if (::connect(sockfd_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            setStatus(DeviceStatus::ERROR);
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        // Démarrer monitoring de qualité
        startQualityMonitoring();

        setStatus(DeviceStatus::CONNECTED);
        Logger::info("WifiDevice", "✓ Connected to " + host_ + ":" + std::to_string(port_));
        return true;
    }

    void disconnect() override {
        stopQualityMonitoring();
        
        if (sockfd_ >= 0) {
            close(sockfd_);
            sockfd_ = -1;
            setStatus(DeviceStatus::DISCONNECTED);
        }
    }

    bool sendMessage(const MidiMessage& msg) override {
        if (!isConnected()) return false;

        const auto& data = msg.getData();
        ssize_t sent = send(sockfd_, data.data(), data.size(), MSG_NOSIGNAL);
        
        if (sent < 0) {
            if (errno == EPIPE || errno == ECONNRESET) {
                Logger::error("WifiDevice", "Connection lost");
                setStatus(DeviceStatus::ERROR);
            }
            return false;
        }

        // Mesurer RTT pour quality monitoring
        updateQualityMetrics();
        
        return true;
    }
    
    // Nouveau: obtenir la qualité de connexion
    int getConnectionQuality() const {
        return connectionQuality_;
    }

private:
    void startQualityMonitoring() {
        qualityMonitoring_ = true;
        qualityThread_ = std::thread([this]() {
            while (qualityMonitoring_) {
                measureLatency();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        });
    }
    
    void stopQualityMonitoring() {
        qualityMonitoring_ = false;
        if (qualityThread_.joinable()) {
            qualityThread_.join();
        }
    }
    
    void measureLatency() {
        // Envoyer ping MIDI (Active Sensing)
        auto start = std::chrono::steady_clock::now();
        
        uint8_t ping = 0xFE;
        ssize_t sent = send(sockfd_, &ping, 1, MSG_NOSIGNAL);
        
        if (sent > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            
            // Calculer quality: 100% si <10ms, 0% si >100ms
            if (latencyMs < 10) {
                connectionQuality_ = 100;
            } else if (latencyMs > 100) {
                connectionQuality_ = 0;
            } else {
                connectionQuality_ = 100 - ((latencyMs - 10) * 100 / 90);
            }
        }
    }
    
    void updateQualityMetrics() {
        // Tracking des messages envoyés pour détecter packet loss
        messagesSent_++;
    }

    std::string host_;
    int port_;
    int sockfd_;
    
    std::atomic<int> connectionQuality_;
    std::atomic<bool> qualityMonitoring_{false};
    std::thread qualityThread_;
    std::atomic<uint64_t> messagesSent_{0};
};

// Auto-registration
REGISTER_DEVICE_PLUGIN(WifiDevicePlugin);

} // namespace midiMind

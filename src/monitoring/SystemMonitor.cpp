// ============================================================================
// Fichier: src/monitoring/SystemMonitor.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "SystemMonitor.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <sys/statvfs.h>
#include <unistd.h>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

SystemMonitor::SystemMonitor()
    : running_(false)
    , updateIntervalMs_(1000)
    , prevCpuTotal_(0)
    , prevCpuIdle_(0)
    , prevNetworkBytesRx_(0)
    , prevNetworkBytesTx_(0)
    , prevNetworkTimestamp_(0) {
    
    Logger::info("SystemMonitor", "SystemMonitor constructed");
}

SystemMonitor::~SystemMonitor() {
    stop();
    Logger::info("SystemMonitor", "SystemMonitor destroyed");
}

// ============================================================================
// CONTRÔLE
// ============================================================================

void SystemMonitor::start() {
    if (running_) {
        Logger::warn("SystemMonitor", "Already running");
        return;
    }
    
    Logger::info("SystemMonitor", "Starting system monitoring...");
    Logger::info("SystemMonitor", "  Update interval: " + 
                std::to_string(updateIntervalMs_) + "ms");
    
    running_ = true;
    
    monitoringThread_ = std::thread([this]() {
        monitoringLoop();
    });
    
    Logger::info("SystemMonitor", "✓ System monitoring started");
}

void SystemMonitor::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("SystemMonitor", "Stopping system monitoring...");
    
    running_ = false;
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    
    Logger::info("SystemMonitor", "✓ System monitoring stopped");
}

bool SystemMonitor::isRunning() const {
    return running_;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void SystemMonitor::setUpdateInterval(uint32_t intervalMs) {
    // Minimum 100ms pour éviter une surcharge
    updateIntervalMs_ = std::max(intervalMs, 100u);
    
    Logger::info("SystemMonitor", "Update interval set to " + 
                std::to_string(updateIntervalMs_) + "ms");
}

uint32_t SystemMonitor::getUpdateInterval() const {
    return updateIntervalMs_;
}

// ============================================================================
// RÉCUPÉRATION DES MÉTRIQUES
// ============================================================================

SystemMetrics SystemMonitor::getCurrentMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentMetrics_;
}

void SystemMonitor::updateNow() {
    auto metrics = collectMetrics();
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentMetrics_ = metrics;
    }
    
    // Callback
    if (metricsUpdateCallback_) {
        metricsUpdateCallback_(metrics);
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void SystemMonitor::setMetricsUpdateCallback(MetricsUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    metricsUpdateCallback_ = callback;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void SystemMonitor::monitoringLoop() {
    Logger::info("SystemMonitor", "Monitoring loop started");
    
    while (running_) {
        // Collecter les métriques
        updateNow();
        
        // Attendre l'intervalle
        uint32_t interval = updateIntervalMs_;
        for (uint32_t i = 0; i < interval / 10 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    Logger::info("SystemMonitor", "Monitoring loop stopped");
}

SystemMetrics SystemMonitor::collectMetrics() {
    SystemMetrics metrics;
    
    // CPU
    metrics.cpuUsagePercent = readCpuUsage();
    metrics.cpuTemperature = readCpuTemperature();
    metrics.cpuFrequency = readCpuFrequency();
    
    // Mémoire
    readMemoryInfo(metrics.ramTotalBytes, metrics.ramUsedBytes, metrics.ramFreeBytes);
    if (metrics.ramTotalBytes > 0) {
        metrics.ramUsagePercent = (metrics.ramUsedBytes * 100.0f) / metrics.ramTotalBytes;
    }
    
    // Disque
    readDiskInfo(metrics.diskTotalBytes, metrics.diskUsedBytes, metrics.diskFreeBytes);
    if (metrics.diskTotalBytes > 0) {
        metrics.diskUsagePercent = (metrics.diskUsedBytes * 100.0f) / metrics.diskTotalBytes;
    }
    
    // Réseau
    readNetworkStats(metrics.networkBytesReceived, metrics.networkBytesSent);
    
    // Timestamp
    metrics.timestamp = getCurrentTimestamp();
    
    return metrics;
}

float SystemMonitor::readCpuUsage() {
    // Lire /proc/stat
    std::string stat = readSysFile("/proc/stat");
    if (stat.empty()) {
        return 0.0f;
    }
    
    std::istringstream iss(stat);
    std::string cpu;
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
    
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    
    if (cpu != "cpu") {
        return 0.0f;
    }
    
    // Calculer le total et l'idle
    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
    uint64_t idleTime = idle + iowait;
    
    // Calculer l'utilisation depuis la dernière mesure
    float usage = 0.0f;
    
    if (prevCpuTotal_ > 0) {
        uint64_t totalDiff = total - prevCpuTotal_;
        uint64_t idleDiff = idleTime - prevCpuIdle_;
        
        if (totalDiff > 0) {
            usage = 100.0f * (totalDiff - idleDiff) / totalDiff;
        }
    }
    
    prevCpuTotal_ = total;
    prevCpuIdle_ = idleTime;
    
    return usage;
}

float SystemMonitor::readCpuTemperature() {
    // Raspberry Pi: /sys/class/thermal/thermal_zone0/temp
    std::string temp = readSysFile("/sys/class/thermal/thermal_zone0/temp");
    
    if (temp.empty()) {
        return 0.0f;
    }
    
    try {
        // La température est en millidegrés
        int millidegrees = std::stoi(temp);
        return millidegrees / 1000.0f;
    } catch (...) {
        return 0.0f;
    }
}

uint32_t SystemMonitor::readCpuFrequency() {
    // Raspberry Pi: /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq
    std::string freq = readSysFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    
    if (freq.empty()) {
        return 0;
    }
    
    try {
        // La fréquence est en kHz
        uint32_t khz = std::stoul(freq);
        return khz / 1000; // Convertir en MHz
    } catch (...) {
        return 0;
    }
}

void SystemMonitor::readMemoryInfo(uint64_t& total, uint64_t& used, uint64_t& free) {
    // Lire /proc/meminfo
    std::string meminfo = readSysFile("/proc/meminfo");
    
    if (meminfo.empty()) {
        total = used = free = 0;
        return;
    }
    
    uint64_t memTotal = 0;
    uint64_t memFree = 0;
    uint64_t memAvailable = 0;
    uint64_t buffers = 0;
    uint64_t cached = 0;
    
    std::istringstream iss(meminfo);
    std::string line;
    
    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string key;
        uint64_t value;
        std::string unit;
        
        lineStream >> key >> value >> unit;
        
        if (key == "MemTotal:") {
            memTotal = value * 1024; // kB → bytes
        } else if (key == "MemFree:") {
            memFree = value * 1024;
        } else if (key == "MemAvailable:") {
            memAvailable = value * 1024;
        } else if (key == "Buffers:") {
            buffers = value * 1024;
        } else if (key == "Cached:") {
            cached = value * 1024;
        }
    }
    
    total = memTotal;
    free = memAvailable > 0 ? memAvailable : (memFree + buffers + cached);
    used = total - free;
}

void SystemMonitor::readDiskInfo(uint64_t& total, uint64_t& used, uint64_t& free) {
    // Utiliser statvfs pour obtenir les informations du disque
    struct statvfs stat;
    
    if (statvfs("/", &stat) != 0) {
        total = used = free = 0;
        return;
    }
    
    // Calculer les tailles
    uint64_t blockSize = stat.f_frsize;
    total = stat.f_blocks * blockSize;
    free = stat.f_bfree * blockSize;
    used = total - (stat.f_bavail * blockSize);
}

void SystemMonitor::readNetworkStats(uint64_t& bytesRx, uint64_t& bytesTx) {
    // Lire /proc/net/dev
    std::string netdev = readSysFile("/proc/net/dev");
    
    if (netdev.empty()) {
        bytesRx = bytesTx = 0;
        return;
    }
    
    bytesRx = 0;
    bytesTx = 0;
    
    std::istringstream iss(netdev);
    std::string line;
    
    // Ignorer les 2 premières lignes (headers)
    std::getline(iss, line);
    std::getline(iss, line);
    
    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string interface;
        uint64_t rx, tx;
        
        lineStream >> interface;
        
        // Ignorer l'interface loopback
        if (interface.find("lo:") != std::string::npos) {
            continue;
        }
        
        // Lire les bytes reçus (2ème colonne)
        lineStream >> rx;
        
        // Ignorer les colonnes 3-9
        for (int i = 0; i < 7; ++i) {
            uint64_t dummy;
            lineStream >> dummy;
        }
        
        // Lire les bytes transmis (10ème colonne)
        lineStream >> tx;
        
        bytesRx += rx;
        bytesTx += tx;
    }
}

std::string SystemMonitor::readSysFile(const std::string& path) const {
    std::ifstream file(path);
    
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return buffer.str();
}

uint64_t SystemMonitor::getCurrentTimestamp() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SystemMonitor.cpp
// ============================================================================
// ============================================================================
// Fichier: src/monitoring/MetricsCollector.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "MetricsCollector.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MetricsCollector::MetricsCollector(size_t historySize)
    : historySize_(historySize)
    , running_(false) {
    
    Logger::info("MetricsCollector", "MetricsCollector constructed");
    Logger::info("MetricsCollector", "  History size: " + std::to_string(historySize));
}

MetricsCollector::~MetricsCollector() {
    stop();
    Logger::info("MetricsCollector", "MetricsCollector destroyed");
}

// ============================================================================
// CONTRÔLE
// ============================================================================

void MetricsCollector::start() {
    if (running_) {
        Logger::warn("MetricsCollector", "Already running");
        return;
    }
    
    Logger::info("MetricsCollector", "Starting metrics collection...");
    
    running_ = true;
    
    // Démarrer les monitors si enregistrés
    if (systemMonitor_) {
        systemMonitor_->start();
    }
    
    Logger::info("MetricsCollector", "✓ Metrics collection started");
}

void MetricsCollector::stop() {
    if (!running_) {
        return;
    }
    
    Logger::info("MetricsCollector", "Stopping metrics collection...");
    
    running_ = false;
    
    // Arrêter les monitors
    if (systemMonitor_) {
        systemMonitor_->stop();
    }
    
    Logger::info("MetricsCollector", "✓ Metrics collection stopped");
}

bool MetricsCollector::isRunning() const {
    return running_;
}

// ============================================================================
// ENREGISTREMENT DES MONITORS
// ============================================================================

void MetricsCollector::registerSystemMonitor(std::shared_ptr<SystemMonitor> monitor) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    systemMonitor_ = monitor;
    
    // Configurer le callback
    if (systemMonitor_) {
        systemMonitor_->setMetricsUpdateCallback([this](const SystemMetrics& metrics) {
            std::lock_guard<std::mutex> lock(mutex_);
            currentMetrics_.system = metrics;
            
            // Ajouter à l'historique
            addToHistory(currentMetrics_);
            
            // Callback
            if (metricsUpdateCallback_) {
                metricsUpdateCallback_(currentMetrics_);
            }
        });
    }
    
    Logger::info("MetricsCollector", "SystemMonitor registered");
}

void MetricsCollector::registerLatencyMonitor(std::shared_ptr<LatencyMonitor> monitor) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    latencyMonitor_ = monitor;
    
    // Configurer le callback
    if (latencyMonitor_) {
        latencyMonitor_->setMetricsUpdateCallback([this](const LatencyMetrics& metrics) {
            std::lock_guard<std::mutex> lock(mutex_);
            currentMetrics_.latency = metrics;
        });
    }
    
    Logger::info("MetricsCollector", "LatencyMonitor registered");
}

// ============================================================================
// MISE À JOUR MANUELLE DES MÉTRIQUES
// ============================================================================

void MetricsCollector::updateMidiMetrics(const MidiMetrics& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentMetrics_.midi = metrics;
}

void MetricsCollector::updateApplicationMetrics(const ApplicationMetrics& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentMetrics_.application = metrics;
}

// ============================================================================
// RÉCUPÉRATION DES MÉTRIQUES
// ============================================================================

AggregatedMetrics MetricsCollector::getCurrentMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AggregatedMetrics metrics = currentMetrics_;
    metrics.timestamp = getCurrentTimestamp();
    
    return metrics;
}

std::vector<AggregatedMetrics> MetricsCollector::getHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<AggregatedMetrics>(history_.begin(), history_.end());
}

std::vector<AggregatedMetrics> MetricsCollector::getHistory(
    uint64_t startTimestamp, 
    uint64_t endTimestamp) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<AggregatedMetrics> filtered;
    
    for (const auto& metrics : history_) {
        if (metrics.timestamp >= startTimestamp && metrics.timestamp <= endTimestamp) {
            filtered.push_back(metrics);
        }
    }
    
    return filtered;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void MetricsCollector::setHistorySize(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    historySize_ = size;
    
    // Ajuster l'historique si nécessaire
    while (history_.size() > historySize_) {
        history_.pop_front();
    }
    
    Logger::info("MetricsCollector", "History size set to " + std::to_string(size));
}

size_t MetricsCollector::getHistorySize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return historySize_;
}

void MetricsCollector::clearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t count = history_.size();
    history_.clear();
    
    Logger::info("MetricsCollector", "Cleared history (" + std::to_string(count) + " entries)");
}

// ============================================================================
// EXPORT
// ============================================================================

bool MetricsCollector::exportToJson(const std::string& filepath) const {
    Logger::info("MetricsCollector", "Exporting to JSON: " + filepath);
    
    try {
        auto metrics = getCurrentMetrics();
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::error("MetricsCollector", "Cannot open file: " + filepath);
            return false;
        }
        
        json j = metrics.toJson();
        file << j.dump(2);
        file.close();
        
        Logger::info("MetricsCollector", "✓ Exported to JSON");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MetricsCollector", "Export failed: " + std::string(e.what()));
        return false;
    }
}

bool MetricsCollector::exportToCsv(const std::string& filepath) const {
    Logger::info("MetricsCollector", "Exporting to CSV: " + filepath);
    
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::error("MetricsCollector", "Cannot open file: " + filepath);
            return false;
        }
        
        // Header
        file << "timestamp,cpu_usage,cpu_temp,ram_usage,disk_usage,";
        file << "latency_avg,latency_min,latency_max,jitter,";
        file << "midi_messages_rx,midi_messages_tx\n";
        
        // Data
        for (const auto& metrics : history_) {
            file << metrics.timestamp << ",";
            file << std::fixed << std::setprecision(2);
            file << metrics.system.cpuUsagePercent << ",";
            file << metrics.system.cpuTemperature << ",";
            file << metrics.system.ramUsagePercent << ",";
            file << metrics.system.diskUsagePercent << ",";
            file << metrics.latency.averageLatencyUs << ",";
            file << metrics.latency.minLatencyUs << ",";
            file << metrics.latency.maxLatencyUs << ",";
            file << metrics.latency.jitterUs << ",";
            file << metrics.midi.messagesReceived << ",";
            file << metrics.midi.messagesSent << "\n";
        }
        
        file.close();
        
        Logger::info("MetricsCollector", "✓ Exported to CSV (" + 
                    std::to_string(history_.size()) + " entries)");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MetricsCollector", "Export failed: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

void MetricsCollector::setMetricsUpdateCallback(MetricsUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    metricsUpdateCallback_ = callback;
}

// ============================================================================
// STATISTIQUES
// ============================================================================

json MetricsCollector::calculateStatistics(uint64_t startTimestamp, 
                                           uint64_t endTimestamp) const {
    auto filtered = getHistory(startTimestamp, endTimestamp);
    
    if (filtered.empty()) {
        return json::object();
    }
    
    json stats;
    
    // CPU
    float cpuSum = 0.0f;
    float cpuMin = 100.0f;
    float cpuMax = 0.0f;
    
    // RAM
    float ramSum = 0.0f;
    
    // Latence
    float latencySum = 0.0f;
    float latencyMin = 999999.0f;
    float latencyMax = 0.0f;
    
    for (const auto& m : filtered) {
        cpuSum += m.system.cpuUsagePercent;
        cpuMin = std::min(cpuMin, m.system.cpuUsagePercent);
        cpuMax = std::max(cpuMax, m.system.cpuUsagePercent);
        
        ramSum += m.system.ramUsagePercent;
        
        latencySum += m.latency.averageLatencyUs;
        latencyMin = std::min(latencyMin, m.latency.averageLatencyUs);
        latencyMax = std::max(latencyMax, m.latency.averageLatencyUs);
    }
    
    size_t count = filtered.size();
    
    stats["period"]["start"] = startTimestamp;
    stats["period"]["end"] = endTimestamp;
    stats["period"]["sample_count"] = count;
    
    stats["cpu"]["average"] = cpuSum / count;
    stats["cpu"]["min"] = cpuMin;
    stats["cpu"]["max"] = cpuMax;
    
    stats["ram"]["average"] = ramSum / count;
    
    stats["latency"]["average"] = latencySum / count;
    stats["latency"]["min"] = latencyMin;
    stats["latency"]["max"] = latencyMax;
    
    return stats;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

AggregatedMetrics MetricsCollector::collectMetrics() {
    AggregatedMetrics metrics;
    
    // Collecter depuis les monitors enregistrés
    if (systemMonitor_) {
        metrics.system = systemMonitor_->getCurrentMetrics();
    }
    
    if (latencyMonitor_) {
        metrics.latency = latencyMonitor_->getCurrentMetrics();
    }
    
    // Les métriques MIDI et application sont mises à jour manuellement
    metrics.midi = currentMetrics_.midi;
    metrics.application = currentMetrics_.application;
    
    metrics.timestamp = getCurrentTimestamp();
    
    return metrics;
}

void MetricsCollector::addToHistory(const AggregatedMetrics& metrics) {
    history_.push_back(metrics);
    
    // Limiter la taille
    if (history_.size() > historySize_) {
        history_.pop_front();
    }
}

uint64_t MetricsCollector::getCurrentTimestamp() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MetricsCollector.cpp
// ============================================================================
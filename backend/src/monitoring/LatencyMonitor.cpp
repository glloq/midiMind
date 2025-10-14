// ============================================================================
// Fichier: src/monitoring/LatencyMonitor.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "LatencyMonitor.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

LatencyMonitor::LatencyMonitor(size_t windowSize)
    : windowSize_(windowSize)
    , measurementStartUs_(0)
    , messageCount_(0)
    , droppedMessages_(0) {
    
    Logger::info("LatencyMonitor", "LatencyMonitor constructed (window: " + 
                std::to_string(windowSize) + ")");
}

LatencyMonitor::~LatencyMonitor() {
    Logger::info("LatencyMonitor", "LatencyMonitor destroyed");
}

// ============================================================================
// MESURES
// ============================================================================

void LatencyMonitor::startMeasurement() {
    measurementStartUs_ = getCurrentTimestampUs();
}

void LatencyMonitor::endMeasurement() {
    uint64_t start = measurementStartUs_;
    
    if (start == 0) {
        Logger::warn("LatencyMonitor", "endMeasurement called without startMeasurement");
        return;
    }
    
    uint64_t end = getCurrentTimestampUs();
    float latencyUs = static_cast<float>(end - start);
    
    // Ajouter à l'historique
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        latencyHistory_.push_back(latencyUs);
        
        // Limiter la taille de l'historique
        if (latencyHistory_.size() > windowSize_) {
            latencyHistory_.pop_front();
        }
        
        // Calculer les métriques
        calculateMetrics();
    }
    
    messageCount_++;
    
    // Réinitialiser le timestamp de départ
    measurementStartUs_ = 0;
    
    // Callback
    if (metricsUpdateCallback_) {
        metricsUpdateCallback_(currentMetrics_);
    }
}

void LatencyMonitor::recordDroppedMessage() {
    droppedMessages_++;
}

void LatencyMonitor::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    latencyHistory_.clear();
    currentMetrics_ = LatencyMetrics();
    messageCount_ = 0;
    droppedMessages_ = 0;
    
    Logger::info("LatencyMonitor", "Statistics reset");
}

// ============================================================================
// RÉCUPÉRATION DES MÉTRIQUES
// ============================================================================

LatencyMetrics LatencyMonitor::getCurrentMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LatencyMetrics metrics = currentMetrics_;
    metrics.messageCount = messageCount_;
    metrics.droppedMessages = droppedMessages_;
    metrics.timestamp = getCurrentTimestampMs();
    
    return metrics;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void LatencyMonitor::setWindowSize(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    windowSize_ = size;
    
    // Ajuster l'historique si nécessaire
    while (latencyHistory_.size() > windowSize_) {
        latencyHistory_.pop_front();
    }
    
    Logger::info("LatencyMonitor", "Window size set to " + std::to_string(size));
}

size_t LatencyMonitor::getWindowSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return windowSize_;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void LatencyMonitor::setMetricsUpdateCallback(MetricsUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    metricsUpdateCallback_ = callback;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void LatencyMonitor::calculateMetrics() {
    // Doit être appelé avec le mutex verrouillé
    
    if (latencyHistory_.empty()) {
        currentMetrics_ = LatencyMetrics();
        return;
    }
    
    // Latence actuelle
    currentMetrics_.currentLatencyUs = latencyHistory_.back();
    
    // Moyenne
    float sum = std::accumulate(latencyHistory_.begin(), latencyHistory_.end(), 0.0f);
    currentMetrics_.averageLatencyUs = sum / latencyHistory_.size();
    
    // Min / Max
    auto minmax = std::minmax_element(latencyHistory_.begin(), latencyHistory_.end());
    currentMetrics_.minLatencyUs = *minmax.first;
    currentMetrics_.maxLatencyUs = *minmax.second;
    
    // Jitter (écart-type)
    if (latencyHistory_.size() > 1) {
        float variance = 0.0f;
        for (float latency : latencyHistory_) {
            float diff = latency - currentMetrics_.averageLatencyUs;
            variance += diff * diff;
        }
        variance /= latencyHistory_.size();
        currentMetrics_.jitterUs = std::sqrt(variance);
    } else {
        currentMetrics_.jitterUs = 0.0f;
    }
}

uint64_t LatencyMonitor::getCurrentTimestampUs() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

uint64_t LatencyMonitor::getCurrentTimestampMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER LatencyMonitor.cpp
// ============================================================================
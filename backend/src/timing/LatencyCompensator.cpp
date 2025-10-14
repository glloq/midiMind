
// ============================================================================
// Fichier: backend/src/timing/LatencyCompensator.cpp
// Version: 3.0.0 - Phase 2
// ============================================================================

#include "LatencyCompensator.h"
#include "TimestampManager.h"
#include "../core/Logger.h"
#include <algorithm>
#include <cmath>

namespace midiMind {

// ============================================================================
// DeviceLatencyProfile - Méthodes
// ============================================================================

void DeviceLatencyProfile::addMeasurement(uint64_t latency) {
    // Ajouter à l'historique
    latencyHistory.push_back(latency);
    
    // Limiter la taille de l'historique
    if (latencyHistory.size() > 100) {
        latencyHistory.pop_front();
    }
    
    // Mettre à jour les statistiques
    measurementCount++;
    
    if (latency < minLatency) {
        minLatency = latency;
    }
    
    if (latency > maxLatency) {
        maxLatency = latency;
    }
    
    // Calculer moyenne
    uint64_t sum = 0;
    for (uint64_t l : latencyHistory) {
        sum += l;
    }
    averageLatency = sum / latencyHistory.size();
    
    // Calculer jitter (écart-type)
    if (latencyHistory.size() > 1) {
        double variance = 0.0;
        for (uint64_t l : latencyHistory) {
            double diff = static_cast<double>(l) - static_cast<double>(averageLatency);
            variance += diff * diff;
        }
        variance /= latencyHistory.size();
        jitter = std::sqrt(variance);
    }
}

int64_t DeviceLatencyProfile::calculateOptimalCompensation() const {
    if (!autoCompensation || measurementCount < 5) {
        return compensationOffset;
    }
    
    // Compensation = négatif de la latence moyenne
    // (on veut envoyer plus tôt pour compenser le délai)
    return -static_cast<int64_t>(averageLatency);
}

json DeviceLatencyProfile::toJson() const {
    json j;
    j["device_id"] = deviceId;
    j["average_latency_us"] = averageLatency;
    j["min_latency_us"] = minLatency;
    j["max_latency_us"] = maxLatency;
    j["jitter_us"] = jitter;
    j["measurement_count"] = measurementCount;
    j["compensation_offset_us"] = compensationOffset;
    j["auto_compensation"] = autoCompensation;
    return j;
}

// ============================================================================
// LatencyCompensator - Constructeur / Destructeur
// ============================================================================

LatencyCompensator::LatencyCompensator()
    : historySize_(100)
    , outlierDetectionEnabled_(true)
    , outlierThreshold_(3.0)  // 3 écarts-types
{
    Logger::info("LatencyCompensator", "LatencyCompensator constructed");
}

LatencyCompensator::~LatencyCompensator() {
    Logger::info("LatencyCompensator", "LatencyCompensator destroyed");
}

// ============================================================================
// Gestion des Devices
// ============================================================================

bool LatencyCompensator::registerDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (profiles_.find(deviceId) != profiles_.end()) {
        Logger::warn("LatencyCompensator", "Device already registered: " + deviceId);
        return false;
    }
    
    DeviceLatencyProfile profile;
    profile.deviceId = deviceId;
    profiles_[deviceId] = profile;
    
    Logger::info("LatencyCompensator", "Device registered: " + deviceId);
    return true;
}

void LatencyCompensator::unregisterDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = profiles_.find(deviceId);
    if (it != profiles_.end()) {
        profiles_.erase(it);
        Logger::info("LatencyCompensator", "Device unregistered: " + deviceId);
    }
}

bool LatencyCompensator::isDeviceRegistered(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return profiles_.find(deviceId) != profiles_.end();
}

// ============================================================================
// Mesure de Latence
// ============================================================================

void LatencyCompensator::recordLatency(const std::string& deviceId, uint64_t latencyUs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = profiles_.find(deviceId);
    if (it == profiles_.end()) {
        Logger::warn("LatencyCompensator", "Device not registered: " + deviceId);
        return;
    }
    
    DeviceLatencyProfile& profile = it->second;
    
    // Détecter outliers
    if (outlierDetectionEnabled_ && isOutlier(profile, latencyUs)) {
        Logger::debug("LatencyCompensator", 
            "Outlier detected for " + deviceId + ": " + std::to_string(latencyUs) + "µs");
        return;  // Ignorer les outliers
    }
    
    // Ajouter la mesure
    profile.addMeasurement(latencyUs);
    
    // Recalculer compensation si auto
    if (profile.autoCompensation) {
        profile.compensationOffset = profile.calculateOptimalCompensation();
    }
    
    Logger::debug("LatencyCompensator", 
        deviceId + " latency: " + std::to_string(latencyUs) + "µs, " +
        "avg: " + std::to_string(profile.averageLatency) + "µs, " +
        "compensation: " + std::to_string(profile.compensationOffset) + "µs");
}

uint64_t LatencyCompensator::startMeasurement() const {
    return TimestampManager::instance().now();
}

void LatencyCompensator::endMeasurement(const std::string& deviceId, uint64_t startTime) {
    uint64_t endTime = TimestampManager::instance().now();
    uint64_t latency = endTime - startTime;
    
    recordLatency(deviceId, latency);
}

// ============================================================================
// Compensation
// ============================================================================

int64_t LatencyCompensator::getCompensation(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = profiles_.find(deviceId);
    if (it == profiles_.end()) {
        return 0;
    }
    
    return it->second.compensationOffset;
}

void LatencyCompensator::setCompensation(const std::string& deviceId, int64_t offsetUs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = profiles_.find(deviceId);
    if (it != profiles_.end()) {
        it->second.compensationOffset = offsetUs;
        it->second.autoCompensation = false;  // Désactiver auto si manuel
        
        Logger::info("LatencyCompensator", 
            "Manual compensation set for " + deviceId + ": " + std::to_string(offsetUs) + "µs");
    }
}

void LatencyCompensator::setAutoCompensation(const std::string& deviceId, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = profiles_.find(deviceId);
    if (it != profiles_.end()) {
        it->second.autoCompensation = enabled;
        
        Logger::info("LatencyCompensator", 
            "Auto compensation " + std::string(enabled ? "enabled" : "disabled") + 
            " for " + deviceId);
    }
}

void LatencyCompensator::resetCompensation(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = profiles_.find(deviceId);
    if (it != profiles_.end()) {
        it->second.compensationOffset = 0;
        it->second.latencyHistory.clear();
        it->second.measurementCount = 0;
        it->second.averageLatency = 0;
        it->second.minLatency = UINT64_MAX;
        it->second.maxLatency = 0;
        it->second.jitter = 0.0;
        
        Logger::info("LatencyCompensator", "Compensation reset for " + deviceId);
    }
}

// ============================================================================
// Profils
// ============================================================================

DeviceLatencyProfile LatencyCompensator::getProfile(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = profiles_.find(deviceId);
    if (it != profiles_.end()) {
        return it->second;
    }
    
    return DeviceLatencyProfile();
}

std::vector<DeviceLatencyProfile> LatencyCompensator::getAllProfiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<DeviceLatencyProfile> profiles;
    profiles.reserve(profiles_.size());
    
    for (const auto& [id, profile] : profiles_) {
        profiles.push_back(profile);
    }
    
    return profiles;
}

json LatencyCompensator::exportProfiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json j = json::array();
    
    for (const auto& [id, profile] : profiles_) {
        j.push_back(profile.toJson());
    }
    
    return j;
}

bool LatencyCompensator::importProfiles(const json& profiles) {
    // TODO: Implémenter import
    return true;
}

// ============================================================================
// Statistiques
// ============================================================================

json LatencyCompensator::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["device_count"] = profiles_.size();
    stats["history_size"] = historySize_;
    stats["outlier_detection"] = outlierDetectionEnabled_;
    stats["devices"] = json::array();
    
    for (const auto& [id, profile] : profiles_) {
        stats["devices"].push_back(profile.toJson());
    }
    
    return stats;
}

// ============================================================================
// Méthodes Privées
// ============================================================================

bool LatencyCompensator::isOutlier(const DeviceLatencyProfile& profile, 
                                   uint64_t latency) const {
    if (profile.measurementCount < 10) {
        return false;  // Pas assez de données
    }
    
    // Utiliser la règle des 3-sigma
    double deviation = std::abs(static_cast<double>(latency) - 
                               static_cast<double>(profile.averageLatency));
    
    return (deviation > outlierThreshold_ * profile.jitter);
}

void LatencyCompensator::updateStatistics(DeviceLatencyProfile& profile) {
    // Déjà fait dans addMeasurement()
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER LatencyCompensator.cpp
// ============================================================================
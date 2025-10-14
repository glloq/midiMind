// ============================================================================
// Fichier: backend/src/timing/TimestampManager.cpp
// Version: 3.0.0 - Phase 2
// Date: 2025-10-09
// ============================================================================

#include "TimestampManager.h"
#include "../core/Logger.h"
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// CONSTRUCTEUR
// ============================================================================

TimestampManager::TimestampManager()
    : referencePoint_(0)
    , started_(false)
    , syncOffset_(0)
    , driftCompensationEnabled_(false)
    , driftFactor_(0.0)
    , lastDriftMeasurement_(0)
{
    Logger::info("TimestampManager", "TimestampManager constructed");
}

// ============================================================================
// CONTRÔLE
// ============================================================================

void TimestampManager::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (started_.load(std::memory_order_acquire)) {
        Logger::warn("TimestampManager", "Already started");
        return;
    }
    
    // Capturer le point de référence
    referencePoint_.store(getRawTimestamp(), std::memory_order_release);
    started_.store(true, std::memory_order_release);
    
    Logger::info("TimestampManager", "Started at timestamp: " + 
                std::to_string(referencePoint_.load()));
}

void TimestampManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("TimestampManager", "Resetting timestamp manager");
    
    // Réinitialiser le point de référence
    referencePoint_.store(getRawTimestamp(), std::memory_order_release);
    
    // Réinitialiser les corrections
    syncOffset_.store(0, std::memory_order_release);
    driftFactor_.store(0.0, std::memory_order_release);
    lastDriftMeasurement_.store(0, std::memory_order_release);
}

// ============================================================================
// TIMESTAMPS - MICROSECONDES
// ============================================================================

uint64_t TimestampManager::now() const {
    if (!started_.load(std::memory_order_acquire)) {
        Logger::warn("TimestampManager", "Called now() before start()");
        return 0;
    }
    
    uint64_t raw = getRawTimestamp();
    uint64_t reference = referencePoint_.load(std::memory_order_acquire);
    
    // Calculer le delta depuis le point de référence
    uint64_t delta = (raw >= reference) ? (raw - reference) : 0;
    
    // Appliquer les corrections
    return applyCorrections(delta);
}

uint64_t TimestampManager::systemNow() const {
    return getRawTimestamp();
}

// ============================================================================
// TIMESTAMPS - MILLISECONDES
// ============================================================================

uint64_t TimestampManager::nowMs() const {
    return usToMs(now());
}

uint64_t TimestampManager::systemNowMs() const {
    return usToMs(systemNow());
}

// ============================================================================
// SYNCHRONISATION
// ============================================================================

void TimestampManager::setSyncOffset(int64_t offset) {
    syncOffset_.store(offset, std::memory_order_release);
    
    Logger::debug("TimestampManager", 
        "Sync offset set to: " + std::to_string(offset) + "µs");
}

// ============================================================================
// COMPENSATION DE DÉRIVE
// ============================================================================

double TimestampManager::calculateDrift() const {
    // Mesurer la dérive en comparant l'horloge système avec une référence externe
    // Pour simplifier, on retourne 0 pour l'instant
    // TODO: Implémenter mesure de dérive réelle
    
    return driftFactor_.load(std::memory_order_acquire);
}

// ============================================================================
// STATISTIQUES
// ============================================================================

std::string TimestampManager::getStats() const {
    std::ostringstream oss;
    
    oss << "TimestampManager Statistics:\n";
    oss << "  Started: " << (started_.load() ? "YES" : "NO") << "\n";
    oss << "  Uptime: " << std::fixed << std::setprecision(3) 
        << getUptimeSeconds() << "s\n";
    oss << "  Current timestamp: " << now() << "µs\n";
    oss << "  Sync offset: " << syncOffset_.load() << "µs\n";
    oss << "  Drift compensation: " 
        << (driftCompensationEnabled_.load() ? "ENABLED" : "DISABLED") << "\n";
    oss << "  Drift factor: " << std::fixed << std::setprecision(2)
        << driftFactor_.load() << " ppm\n";
    
    return oss.str();
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

uint64_t TimestampManager::getRawTimestamp() const {
    // Utiliser high_resolution_clock pour précision maximale
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

uint64_t TimestampManager::applyCorrections(uint64_t raw) const {
    // Appliquer l'offset de synchronisation
    int64_t offset = syncOffset_.load(std::memory_order_acquire);
    int64_t corrected = static_cast<int64_t>(raw) + offset;
    
    // S'assurer que le résultat est positif
    if (corrected < 0) {
        corrected = 0;
    }
    
    // Appliquer la compensation de dérive si activée
    if (driftCompensationEnabled_.load(std::memory_order_acquire)) {
        double drift = driftFactor_.load(std::memory_order_acquire);
        
        // drift est en ppm (parties par million)
        // Appliquer la correction: corrected * (1 + drift/1000000)
        corrected = static_cast<int64_t>(
            corrected * (1.0 + drift / 1000000.0)
        );
    }
    
    return static_cast<uint64_t>(corrected);
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER TimestampManager.cpp
// ============================================================================

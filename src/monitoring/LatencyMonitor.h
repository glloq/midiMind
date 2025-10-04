// ============================================================================
// Fichier: src/monitoring/LatencyMonitor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Monitoring de la latence MIDI en temps réel.
//   Mesure le temps de traitement des messages MIDI.
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <mutex>
#include <atomic>
#include <deque>
#include <chrono>
#include <functional>

#include "PerformanceMetrics.h"
#include "../core/Logger.h"

namespace midiMind {

/**
 * @class LatencyMonitor
 * @brief Monitoring de latence MIDI
 * 
 * @details
 * Mesure la latence de traitement des messages MIDI:
 * - Latence actuelle
 * - Latence moyenne, min, max
 * - Jitter (variation de latence)
 * - Messages perdus
 * 
 * Utilisation:
 * 1. Appeler startMeasurement() avant le traitement
 * 2. Appeler endMeasurement() après le traitement
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * LatencyMonitor monitor;
 * 
 * // Avant traitement
 * monitor.startMeasurement();
 * 
 * // Traitement MIDI
 * processMidiMessage(msg);
 * 
 * // Après traitement
 * monitor.endMeasurement();
 * 
 * // Récupérer les métriques
 * auto metrics = monitor.getCurrentMetrics();
 * ```
 */
class LatencyMonitor {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors d'une mise à jour des métriques
     */
    using MetricsUpdateCallback = std::function<void(const LatencyMetrics&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param windowSize Taille de la fenêtre d'historique (défaut: 100)
     */
    explicit LatencyMonitor(size_t windowSize = 100);
    
    /**
     * @brief Destructeur
     */
    ~LatencyMonitor();
    
    // Désactiver copie
    LatencyMonitor(const LatencyMonitor&) = delete;
    LatencyMonitor& operator=(const LatencyMonitor&) = delete;
    
    // ========================================================================
    // MESURES
    // ========================================================================
    
    /**
     * @brief Démarre une mesure de latence
     * 
     * @note Thread-safe
     */
    void startMeasurement();
    
    /**
     * @brief Termine une mesure de latence
     * 
     * @note Thread-safe
     */
    void endMeasurement();
    
    /**
     * @brief Enregistre un message perdu
     * 
     * @note Thread-safe
     */
    void recordDroppedMessage();
    
    /**
     * @brief Réinitialise les statistiques
     * 
     * @note Thread-safe
     */
    void reset();
    
    // ========================================================================
    // RÉCUPÉRATION DES MÉTRIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les métriques actuelles
     * 
     * @return LatencyMetrics Métriques de latence
     * 
     * @note Thread-safe
     */
    LatencyMetrics getCurrentMetrics() const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit la taille de la fenêtre d'historique
     * 
     * @param size Taille de la fenêtre
     * 
     * @note Thread-safe
     */
    void setWindowSize(size_t size);
    
    /**
     * @brief Récupère la taille de la fenêtre
     * 
     * @note Thread-safe
     */
    size_t getWindowSize() const;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de mise à jour
     * 
     * @note Thread-safe
     */
    void setMetricsUpdateCallback(MetricsUpdateCallback callback);

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Calcule les métriques
     */
    void calculateMetrics();
    
    /**
     * @brief Récupère le timestamp actuel (microsecondes)
     */
    uint64_t getCurrentTimestampUs() const;
    
    /**
     * @brief Récupère le timestamp actuel (millisecondes)
     */
    uint64_t getCurrentTimestampMs() const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Timestamp de début de mesure (thread_local)
    std::atomic<uint64_t> measurementStartUs_;
    
    /// Historique des latences (µs)
    std::deque<float> latencyHistory_;
    
    /// Taille de la fenêtre d'historique
    size_t windowSize_;
    
    /// Métriques actuelles
    LatencyMetrics currentMetrics_;
    
    /// Compteurs
    std::atomic<uint64_t> messageCount_;
    std::atomic<uint64_t> droppedMessages_;
    
    /// Callback de mise à jour
    MetricsUpdateCallback metricsUpdateCallback_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER LatencyMonitor.h
// ============================================================================
// ============================================================================
// Fichier: src/monitoring/MetricsCollector.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Collecteur centralisé de toutes les métriques.
//   Agrège les données de tous les monitors et fournit une vue unifiée.
//
// Responsabilités:
//   - Collecter les métriques de tous les monitors
//   - Maintenir un historique
//   - Exporter les données (JSON, CSV)
//   - Fournir des statistiques agrégées
//
// Thread-safety: OUI
//
// Patterns: Facade Pattern, Observer Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <mutex>
#include <deque>
#include <string>
#include <functional>

#include "PerformanceMetrics.h"
#include "SystemMonitor.h"
#include "LatencyMonitor.h"
#include "../core/Logger.h"

namespace midiMind {

/**
 * @struct AggregatedMetrics
 * @brief Métriques agrégées de tous les monitors
 */
struct AggregatedMetrics {
    SystemMetrics system;
    LatencyMetrics latency;
    MidiMetrics midi;
    ApplicationMetrics application;
    uint64_t timestamp;
    
    AggregatedMetrics() : timestamp(0) {}
    
    json toJson() const {
        json j;
        j["system"] = system.toJson();
        j["latency"] = latency.toJson();
        j["midi"] = midi.toJson();
        j["application"] = application.toJson();
        j["timestamp"] = timestamp;
        return j;
    }
};

/**
 * @class MetricsCollector
 * @brief Collecteur centralisé de métriques
 * 
 * @details
 * Point d'entrée unique pour toutes les métriques du système.
 * Collecte, agrège et exporte les données de monitoring.
 * 
 * Architecture:
 * ```
 * SystemMonitor   ┐
 * LatencyMonitor  ├─→ MetricsCollector → Historique → Export (JSON/CSV)
 * MidiRouter      ┘
 * ```
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * auto collector = std::make_shared<MetricsCollector>();
 * 
 * // Démarrer la collecte
 * collector->start();
 * 
 * // Récupérer les métriques
 * auto metrics = collector->getCurrentMetrics();
 * 
 * // Exporter
 * collector->exportToJson("metrics.json");
 * ```
 */
class MetricsCollector {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors d'une mise à jour des métriques
     */
    using MetricsUpdateCallback = std::function<void(const AggregatedMetrics&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param historySize Taille de l'historique (défaut: 3600 = 1h à 1s/sample)
     */
    explicit MetricsCollector(size_t historySize = 3600);
    
    /**
     * @brief Destructeur
     */
    ~MetricsCollector();
    
    // Désactiver copie
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre la collecte
     * 
     * @note Thread-safe
     */
    void start();
    
    /**
     * @brief Arrête la collecte
     * 
     * @note Thread-safe
     */
    void stop();
    
    /**
     * @brief Vérifie si la collecte est active
     * 
     * @note Thread-safe
     */
    bool isRunning() const;
    
    // ========================================================================
    // ENREGISTREMENT DES MONITORS
    // ========================================================================
    
    /**
     * @brief Enregistre le SystemMonitor
     * 
     * @param monitor Pointeur vers le monitor
     * 
     * @note Thread-safe
     */
    void registerSystemMonitor(std::shared_ptr<SystemMonitor> monitor);
    
    /**
     * @brief Enregistre le LatencyMonitor
     * 
     * @param monitor Pointeur vers le monitor
     * 
     * @note Thread-safe
     */
    void registerLatencyMonitor(std::shared_ptr<LatencyMonitor> monitor);
    
    // ========================================================================
    // MISE À JOUR MANUELLE DES MÉTRIQUES
    // ========================================================================
    
    /**
     * @brief Met à jour les métriques MIDI
     * 
     * @param metrics Métriques MIDI
     * 
     * @note Thread-safe
     */
    void updateMidiMetrics(const MidiMetrics& metrics);
    
    /**
     * @brief Met à jour les métriques d'application
     * 
     * @param metrics Métriques d'application
     * 
     * @note Thread-safe
     */
    void updateApplicationMetrics(const ApplicationMetrics& metrics);
    
    // ========================================================================
    // RÉCUPÉRATION DES MÉTRIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les métriques actuelles
     * 
     * @return AggregatedMetrics Métriques agrégées
     * 
     * @note Thread-safe
     */
    AggregatedMetrics getCurrentMetrics() const;
    
    /**
     * @brief Récupère l'historique complet
     * 
     * @return std::vector<AggregatedMetrics> Historique
     * 
     * @note Thread-safe
     */
    std::vector<AggregatedMetrics> getHistory() const;
    
    /**
     * @brief Récupère l'historique sur une période
     * 
     * @param startTimestamp Timestamp de début (ms)
     * @param endTimestamp Timestamp de fin (ms)
     * @return std::vector<AggregatedMetrics> Historique filtré
     * 
     * @note Thread-safe
     */
    std::vector<AggregatedMetrics> getHistory(uint64_t startTimestamp, 
                                              uint64_t endTimestamp) const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit la taille de l'historique
     * 
     * @param size Taille maximale
     * 
     * @note Thread-safe
     */
    void setHistorySize(size_t size);
    
    /**
     * @brief Récupère la taille de l'historique
     * 
     * @note Thread-safe
     */
    size_t getHistorySize() const;
    
    /**
     * @brief Efface l'historique
     * 
     * @note Thread-safe
     */
    void clearHistory();
    
    // ========================================================================
    // EXPORT
    // ========================================================================
    
    /**
     * @brief Exporte les métriques actuelles en JSON
     * 
     * @param filepath Chemin du fichier
     * @return true Si l'export a réussi
     * 
     * @note Thread-safe
     */
    bool exportToJson(const std::string& filepath) const;
    
    /**
     * @brief Exporte l'historique en CSV
     * 
     * @param filepath Chemin du fichier
     * @return true Si l'export a réussi
     * 
     * @note Thread-safe
     */
    bool exportToCsv(const std::string& filepath) const;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de mise à jour
     * 
     * @note Thread-safe
     */
    void setMetricsUpdateCallback(MetricsUpdateCallback callback);
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Calcule des statistiques sur une période
     * 
     * @param startTimestamp Timestamp de début (ms)
     * @param endTimestamp Timestamp de fin (ms)
     * @return json Statistiques agrégées
     * 
     * @note Thread-safe
     */
    json calculateStatistics(uint64_t startTimestamp, uint64_t endTimestamp) const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Collecte toutes les métriques
     */
    AggregatedMetrics collectMetrics();
    
    /**
     * @brief Ajoute une entrée à l'historique
     */
    void addToHistory(const AggregatedMetrics& metrics);
    
    /**
     * @brief Récupère le timestamp actuel (ms)
     */
    uint64_t getCurrentTimestamp() const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Monitors enregistrés
    std::shared_ptr<SystemMonitor> systemMonitor_;
    std::shared_ptr<LatencyMonitor> latencyMonitor_;
    
    /// Métriques actuelles
    AggregatedMetrics currentMetrics_;
    
    /// Historique des métriques
    std::deque<AggregatedMetrics> history_;
    
    /// Taille maximale de l'historique
    size_t historySize_;
    
    /// Flag de collecte active
    std::atomic<bool> running_;
    
    /// Callback de mise à jour
    MetricsUpdateCallback metricsUpdateCallback_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MetricsCollector.h
// ============================================================================
// ============================================================================
// Fichier: src/monitoring/SystemMonitor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Monitoring des ressources système (CPU, RAM, disque, température).
//   Spécifiquement optimisé pour Raspberry Pi.
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
#include <thread>
#include <functional>

#include "PerformanceMetrics.h"
#include "../core/Logger.h"

namespace midiMind {

/**
 * @class SystemMonitor
 * @brief Monitoring des ressources système
 * 
 * @details
 * Surveille les ressources système du Raspberry Pi:
 * - CPU: Utilisation, fréquence, température
 * - RAM: Utilisation, disponible
 * - Disque: Espace utilisé/disponible
 * - Réseau: Débit
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * SystemMonitor monitor;
 * 
 * monitor.setUpdateInterval(1000); // 1 seconde
 * monitor.start();
 * 
 * // Récupérer les métriques
 * auto metrics = monitor.getCurrentMetrics();
 * Logger::info("CPU: " + std::to_string(metrics.cpuUsagePercent) + "%");
 * ```
 */
class SystemMonitor {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors d'une mise à jour des métriques
     */
    using MetricsUpdateCallback = std::function<void(const SystemMetrics&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    SystemMonitor();
    
    /**
     * @brief Destructeur
     */
    ~SystemMonitor();
    
    // Désactiver copie
    SystemMonitor(const SystemMonitor&) = delete;
    SystemMonitor& operator=(const SystemMonitor&) = delete;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre le monitoring
     * 
     * @note Thread-safe
     */
    void start();
    
    /**
     * @brief Arrête le monitoring
     * 
     * @note Thread-safe
     */
    void stop();
    
    /**
     * @brief Vérifie si le monitoring est actif
     * 
     * @note Thread-safe
     */
    bool isRunning() const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit l'intervalle de mise à jour
     * 
     * @param intervalMs Intervalle en millisecondes (min: 100ms)
     * 
     * @note Thread-safe
     */
    void setUpdateInterval(uint32_t intervalMs);
    
    /**
     * @brief Récupère l'intervalle de mise à jour
     * 
     * @note Thread-safe
     */
    uint32_t getUpdateInterval() const;
    
    // ========================================================================
    // RÉCUPÉRATION DES MÉTRIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les métriques actuelles
     * 
     * @return SystemMetrics Métriques système
     * 
     * @note Thread-safe
     */
    SystemMetrics getCurrentMetrics() const;
    
    /**
     * @brief Force une mise à jour immédiate des métriques
     * 
     * @note Thread-safe
     */
    void updateNow();
    
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
     * @brief Thread de monitoring
     */
    void monitoringLoop();
    
    /**
     * @brief Collecte les métriques système
     */
    SystemMetrics collectMetrics();
    
    /**
     * @brief Lit l'utilisation CPU
     */
    float readCpuUsage();
    
    /**
     * @brief Lit la température CPU
     */
    float readCpuTemperature();
    
    /**
     * @brief Lit la fréquence CPU
     */
    uint32_t readCpuFrequency();
    
    /**
     * @brief Lit les informations mémoire
     */
    void readMemoryInfo(uint64_t& total, uint64_t& used, uint64_t& free);
    
    /**
     * @brief Lit les informations disque
     */
    void readDiskInfo(uint64_t& total, uint64_t& used, uint64_t& free);
    
    /**
     * @brief Lit les statistiques réseau
     */
    void readNetworkStats(uint64_t& bytesRx, uint64_t& bytesTx);
    
    /**
     * @brief Lit un fichier système
     */
    std::string readSysFile(const std::string& path) const;
    
    /**
     * @brief Récupère le timestamp actuel
     */
    uint64_t getCurrentTimestamp() const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Thread de monitoring
    std::thread monitoringThread_;
    
    /// Flag d'arrêt
    std::atomic<bool> running_;
    
    /// Intervalle de mise à jour (ms)
    std::atomic<uint32_t> updateIntervalMs_;
    
    /// Métriques actuelles
    SystemMetrics currentMetrics_;
    
    /// Callback de mise à jour
    MetricsUpdateCallback metricsUpdateCallback_;
    
    /// État CPU précédent (pour calcul utilisation)
    uint64_t prevCpuTotal_;
    uint64_t prevCpuIdle_;
    
    /// État réseau précédent (pour calcul débit)
    uint64_t prevNetworkBytesRx_;
    uint64_t prevNetworkBytesTx_;
    uint64_t prevNetworkTimestamp_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SystemMonitor.h
// ============================================================================
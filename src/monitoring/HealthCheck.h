// ============================================================================
// Fichier: src/monitoring/HealthCheck.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Système de vérification de santé du système.
//   Vérifie que tous les composants fonctionnent correctement.
//
// Responsabilités:
//   - Vérifier l'état des composants critiques
//   - Détecter les anomalies (CPU élevé, RAM saturée, etc.)
//   - Fournir un statut de santé global
//   - Alerter en cas de problème
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
#include <vector>
#include <functional>
#include <string>

#include "PerformanceMetrics.h"
#include "MetricsCollector.h"
#include "../core/Logger.h"

namespace midiMind {

/**
 * @struct HealthCheckRule
 * @brief Règle de vérification de santé
 */
struct HealthCheckRule {
    std::string name;                           ///< Nom de la règle
    std::function<bool(const AggregatedMetrics&)> check; ///< Fonction de vérification
    HealthLevel severity;                       ///< Sévérité si échec
    std::string message;                        ///< Message si échec
    
    HealthCheckRule() : severity(HealthLevel::WARNING) {}
    
    HealthCheckRule(const std::string& n,
                   std::function<bool(const AggregatedMetrics&)> c,
                   HealthLevel sev,
                   const std::string& msg)
        : name(n), check(c), severity(sev), message(msg) {}
};

/**
 * @class HealthCheck
 * @brief Système de vérification de santé
 * 
 * @details
 * Vérifie régulièrement l'état du système et détecte les problèmes.
 * 
 * Vérifications par défaut:
 * - CPU > 90% (WARNING)
 * - Température CPU > 80°C (CRITICAL)
 * - RAM > 90% (WARNING)
 * - Disque > 95% (CRITICAL)
 * - Latence MIDI > 10ms (WARNING)
 * - Messages perdus (WARNING)
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * HealthCheck healthCheck;
 * healthCheck.registerMetricsCollector(collector);
 * 
 * healthCheck.setOnHealthChanged([](const HealthStatus& status) {
 *     if (!status.isHealthy()) {
 *         Logger::warn("Health check failed: " + status.message);
 *     }
 * });
 * 
 * healthCheck.start();
 * 
 * // Vérifier l'état
 * auto status = healthCheck.getCurrentStatus();
 * ```
 */
class HealthCheck {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé lors d'un changement d'état de santé
     */
    using HealthChangedCallback = std::function<void(const HealthStatus&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    HealthCheck();
    
    /**
     * @brief Destructeur
     */
    ~HealthCheck();
    
    // Désactiver copie
    HealthCheck(const HealthCheck&) = delete;
    HealthCheck& operator=(const HealthCheck&) = delete;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre les vérifications de santé
     * 
     * @note Thread-safe
     */
    void start();
    
    /**
     * @brief Arrête les vérifications
     * 
     * @note Thread-safe
     */
    void stop();
    
    /**
     * @brief Vérifie si les vérifications sont actives
     * 
     * @note Thread-safe
     */
    bool isRunning() const;
    
    // ========================================================================
    // ENREGISTREMENT
    // ========================================================================
    
    /**
     * @brief Enregistre le MetricsCollector
     * 
     * @param collector Pointeur vers le collector
     * 
     * @note Thread-safe
     */
    void registerMetricsCollector(std::shared_ptr<MetricsCollector> collector);
    
    // ========================================================================
    // RÈGLES DE VÉRIFICATION
    // ========================================================================
    
    /**
     * @brief Ajoute une règle de vérification personnalisée
     * 
     * @param rule Règle à ajouter
     * 
     * @note Thread-safe
     */
    void addRule(const HealthCheckRule& rule);
    
    /**
     * @brief Retire une règle
     * 
     * @param name Nom de la règle
     * @return true Si la règle a été retirée
     * 
     * @note Thread-safe
     */
    bool removeRule(const std::string& name);
    
    /**
     * @brief Efface toutes les règles personnalisées
     * 
     * @note Thread-safe
     */
    void clearCustomRules();
    
    // ========================================================================
    // VÉRIFICATION
    // ========================================================================
    
    /**
     * @brief Effectue une vérification immédiate
     * 
     * @return HealthStatus État de santé
     * 
     * @note Thread-safe
     */
    HealthStatus checkNow();
    
    /**
     * @brief Récupère le statut actuel
     * 
     * @return HealthStatus État de santé
     * 
     * @note Thread-safe
     */
    HealthStatus getCurrentStatus() const;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit l'intervalle de vérification
     * 
     * @param intervalMs Intervalle en millisecondes (min: 1000ms)
     * 
     * @note Thread-safe
     */
    void setCheckInterval(uint32_t intervalMs);
    
    /**
     * @brief Récupère l'intervalle de vérification
     * 
     * @note Thread-safe
     */
    uint32_t getCheckInterval() const;
    
    /**
     * @brief Active/désactive les règles par défaut
     * 
     * @param enabled true pour activer
     * 
     * @note Thread-safe
     */
    void setDefaultRulesEnabled(bool enabled);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de changement d'état
     * 
     * @note Thread-safe
     */
    void setOnHealthChanged(HealthChangedCallback callback);

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread de vérification
     */
    void checkLoop();
    
    /**
     * @brief Initialise les règles par défaut
     */
    void initializeDefaultRules();
    
    /**
     * @brief Effectue les vérifications
     */
    HealthStatus performChecks();
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Thread de vérification
    std::thread checkThread_;
    
    /// Flag d'arrêt
    std::atomic<bool> running_;
    
    /// Intervalle de vérification (ms)
    std::atomic<uint32_t> checkIntervalMs_;
    
    /// MetricsCollector enregistré
    std::shared_ptr<MetricsCollector> metricsCollector_;
    
    /// Règles par défaut
    std::vector<HealthCheckRule> defaultRules_;
    
    /// Règles personnalisées
    std::vector<HealthCheckRule> customRules_;
    
    /// Règles par défaut activées
    bool defaultRulesEnabled_;
    
    /// Statut actuel
    HealthStatus currentStatus_;
    
    /// Callback de changement d'état
    HealthChangedCallback onHealthChanged_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER HealthCheck.h
// ============================================================================
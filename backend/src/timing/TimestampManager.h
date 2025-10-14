// ============================================================================
// Fichier: backend/src/timing/TimestampManager.h
// Version: 3.0.0 - Phase 2
// Date: 2025-10-09
// ============================================================================
// Description:
//   Gestionnaire de synchronisation d'horloges haute précision.
//   Fournit des timestamps cohérents avec compensation de dérive.
//
// Objectifs:
//   - Précision < 1ms
//   - Synchronisation entre composants
//   - Compensation de dérive
//   - Thread-safe
//
// Auteur: midiMind Team
// ============================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <cstdint>

namespace midiMind {

/**
 * @class TimestampManager
 * @brief Gestionnaire d'horloges synchronisées haute précision
 * 
 * Fournit des timestamps cohérents à tous les composants du système
 * avec compensation automatique de la dérive d'horloge.
 * 
 * @details
 * Utilise std::chrono::high_resolution_clock pour une précision maximale.
 * Maintient une horloge de référence et calcule les offsets pour
 * synchroniser toutes les mesures de temps.
 * 
 * Architecture:
 * ```
 * high_resolution_clock
 *        ↓
 * TimestampManager (référence)
 *        ↓
 *   ┌────┼────┐
 *   ↓    ↓    ↓
 * MIDI Router Player  (tous synchronisés)
 * ```
 * 
 * Thread-safety: OUI - Toutes les méthodes sont thread-safe
 * 
 * @example Utilisation
 * @code
 * auto& tsManager = TimestampManager::instance();
 * 
 * // Démarrer l'horloge
 * tsManager.start();
 * 
 * // Obtenir timestamp actuel (microsecondes)
 * uint64_t now = tsManager.now();
 * 
 * // Obtenir timestamp (millisecondes)
 * uint64_t nowMs = tsManager.nowMs();
 * 
 * // Calculer différence entre deux timestamps
 * uint64_t delta = tsManager.elapsed(t1, t2);
 * @endcode
 */
class TimestampManager {
public:
    // ========================================================================
    // SINGLETON
    // ========================================================================
    
    /**
     * @brief Récupère l'instance unique
     * @return TimestampManager& Instance
     */
    static TimestampManager& instance() {
        static TimestampManager instance;
        return instance;
    }
    
    // Désactiver copie et assignation
    TimestampManager(const TimestampManager&) = delete;
    TimestampManager& operator=(const TimestampManager&) = delete;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre l'horloge de référence
     * 
     * Initialise le point de référence temporel (t=0).
     * Doit être appelé une fois au démarrage de l'application.
     */
    void start();
    
    /**
     * @brief Réinitialise l'horloge
     * 
     * Remet le compteur à zéro. Utile pour les tests ou
     * lors d'un reset complet du système.
     */
    void reset();
    
    /**
     * @brief Vérifie si l'horloge est démarrée
     * @return bool true si démarrée
     */
    bool isStarted() const {
        return started_.load(std::memory_order_acquire);
    }
    
    // ========================================================================
    // TIMESTAMPS - MICROSECONDES (µs)
    // ========================================================================
    
    /**
     * @brief Timestamp actuel en microsecondes
     * @return uint64_t Microsecondes depuis start()
     * 
     * @details
     * Précision typique: < 1µs sur Raspberry Pi 4
     * Utilisé pour mesures de latence haute précision
     */
    uint64_t now() const;
    
    /**
     * @brief Timestamp système en microsecondes (epoch Unix)
     * @return uint64_t Microsecondes depuis 1970-01-01 00:00:00 UTC
     */
    uint64_t systemNow() const;
    
    // ========================================================================
    // TIMESTAMPS - MILLISECONDES (ms)
    // ========================================================================
    
    /**
     * @brief Timestamp actuel en millisecondes
     * @return uint64_t Millisecondes depuis start()
     * 
     * @details
     * Version moins précise mais suffisante pour la plupart des usages.
     * Utilisé pour positions de lecture, durées, etc.
     */
    uint64_t nowMs() const;
    
    /**
     * @brief Timestamp système en millisecondes (epoch Unix)
     * @return uint64_t Millisecondes depuis 1970-01-01 00:00:00 UTC
     */
    uint64_t systemNowMs() const;
    
    // ========================================================================
    // CALCULS TEMPORELS
    // ========================================================================
    
    /**
     * @brief Calcule le temps écoulé entre deux timestamps (µs)
     * @param start Timestamp de début (µs)
     * @param end Timestamp de fin (µs)
     * @return uint64_t Différence en microsecondes
     */
    uint64_t elapsed(uint64_t start, uint64_t end) const {
        return (end >= start) ? (end - start) : 0;
    }
    
    /**
     * @brief Calcule le temps écoulé depuis un timestamp (µs)
     * @param start Timestamp de début (µs)
     * @return uint64_t Microsecondes écoulées depuis start
     */
    uint64_t elapsedSince(uint64_t start) const {
        return elapsed(start, now());
    }
    
    /**
     * @brief Convertit microsecondes → millisecondes
     * @param us Microsecondes
     * @return uint64_t Millisecondes
     */
    static uint64_t usToMs(uint64_t us) {
        return us / 1000;
    }
    
    /**
     * @brief Convertit millisecondes → microsecondes
     * @param ms Millisecondes
     * @return uint64_t Microsecondes
     */
    static uint64_t msToUs(uint64_t ms) {
        return ms * 1000;
    }
    
    // ========================================================================
    // SYNCHRONISATION
    // ========================================================================
    
    /**
     * @brief Définit un offset de synchronisation (µs)
     * @param offset Offset en microsecondes
     * 
     * @details
     * Permet de compenser un délai constant (ex: latence réseau).
     * L'offset est ajouté à tous les timestamps retournés.
     */
    void setSyncOffset(int64_t offset);
    
    /**
     * @brief Récupère l'offset de synchronisation actuel
     * @return int64_t Offset en microsecondes
     */
    int64_t getSyncOffset() const {
        return syncOffset_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Réinitialise l'offset de synchronisation
     */
    void resetSyncOffset() {
        syncOffset_.store(0, std::memory_order_release);
    }
    
    // ========================================================================
    // COMPENSATION DE DÉRIVE
    // ========================================================================
    
    /**
     * @brief Active/désactive la compensation de dérive
     * @param enabled true pour activer
     */
    void setDriftCompensation(bool enabled) {
        driftCompensationEnabled_.store(enabled, std::memory_order_release);
    }
    
    /**
     * @brief Vérifie si la compensation est activée
     * @return bool true si activée
     */
    bool isDriftCompensationEnabled() const {
        return driftCompensationEnabled_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Calcule la dérive actuelle de l'horloge (ppm)
     * @return double Dérive en parties par million
     * 
     * @details
     * Une dérive de 10 ppm signifie 10µs de décalage par seconde.
     * Valeur typique sur Raspberry Pi: < 50 ppm
     */
    double calculateDrift() const;
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère des statistiques sur l'horloge
     * @return std::string Statistiques formatées
     */
    std::string getStats() const;
    
    /**
     * @brief Récupère l'uptime depuis start() (secondes)
     * @return double Secondes écoulées
     */
    double getUptimeSeconds() const {
        return now() / 1000000.0;
    }

private:
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR PRIVÉS (Singleton)
    // ========================================================================
    
    TimestampManager();
    ~TimestampManager() = default;
    
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Récupère le timestamp brut de l'horloge système (µs)
     */
    uint64_t getRawTimestamp() const;
    
    /**
     * @brief Applique l'offset et la compensation de dérive
     */
    uint64_t applyCorrections(uint64_t raw) const;
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Point de référence temporel (µs depuis epoch)
    std::atomic<uint64_t> referencePoint_;
    
    /// Flag indiquant si l'horloge est démarrée
    std::atomic<bool> started_;
    
    /// Offset de synchronisation (µs)
    std::atomic<int64_t> syncOffset_;
    
    /// Compensation de dérive activée
    std::atomic<bool> driftCompensationEnabled_;
    
    /// Facteur de dérive calculé (ppm)
    std::atomic<double> driftFactor_;
    
    /// Dernière mesure de dérive (µs)
    mutable std::atomic<uint64_t> lastDriftMeasurement_;
    
    /// Mutex pour opérations critiques
    mutable std::mutex mutex_;
};

// ============================================================================
// FONCTIONS UTILITAIRES INLINE
// ============================================================================

/**
 * @brief Obtient un timestamp rapide (microsecondes)
 * 
 * Version inline optimisée pour performance maximale.
 * Utilisée dans les boucles critiques.
 */
inline uint64_t getTimestampUs() {
    return TimestampManager::instance().now();
}

/**
 * @brief Obtient un timestamp rapide (millisecondes)
 * 
 * Version inline optimisée pour performance maximale.
 */
inline uint64_t getTimestampMs() {
    return TimestampManager::instance().nowMs();
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER TimestampManager.h
// ============================================================================

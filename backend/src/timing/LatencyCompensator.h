// ============================================================================
// Fichier: backend/src/timing/LatencyCompensator.h
// Version: 3.0.0 - Phase 2
// Date: 2025-10-09
// ============================================================================
// Description:
//   Compensateur de latence MIDI adaptatif.
//   Ajuste automatiquement les délais pour minimiser la latence perçue.
//
// Objectifs:
//   - Compensation automatique < 5ms
//   - Apprentissage des latences par device
//   - Prédiction et ajustement
//
// Auteur: midiMind Team
// ============================================================================

#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <deque>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct DeviceLatencyProfile
 * @brief Profil de latence d'un périphérique
 */
struct DeviceLatencyProfile {
    std::string deviceId;
    
    // Latences mesurées (microsecondes)
    uint64_t averageLatency;      ///< Latence moyenne
    uint64_t minLatency;           ///< Latence minimale observée
    uint64_t maxLatency;           ///< Latence maximale observée
    
    // Statistiques
    uint64_t measurementCount;     ///< Nombre de mesures
    double jitter;                 ///< Variance (écart-type)
    
    // Compensation
    int64_t compensationOffset;    ///< Offset appliqué (µs)
    bool autoCompensation;         ///< Compensation auto activée
    
    // Historique
    std::deque<uint64_t> latencyHistory;  ///< Dernières mesures
    
    DeviceLatencyProfile()
        : averageLatency(0)
        , minLatency(UINT64_MAX)
        , maxLatency(0)
        , measurementCount(0)
        , jitter(0.0)
        , compensationOffset(0)
        , autoCompensation(true) {}
    
    /**
     * @brief Met à jour avec une nouvelle mesure
     * @param latency Latence mesurée (µs)
     */
    void addMeasurement(uint64_t latency);
    
    /**
     * @brief Calcule la compensation optimale
     * @return int64_t Offset de compensation (µs)
     */
    int64_t calculateOptimalCompensation() const;
    
    /**
     * @brief Exporte en JSON
     */
    json toJson() const;
};

/**
 * @class LatencyCompensator
 * @brief Compensateur adaptatif de latence MIDI
 * 
 * @details
 * Mesure, apprend et compense automatiquement les latences
 * de chaque périphérique MIDI pour minimiser la latence perçue.
 * 
 * Architecture:
 * ```
 * Message MIDI →
 *   ↓
 * [Mesure timestamp T1]
 *   ↓
 * Envoi au device →
 *   ↓
 * [Mesure timestamp T2]
 *   ↓
 * Latency = T2 - T1
 *   ↓
 * Ajustement profil
 *   ↓
 * Calcul compensation
 * ```
 * 
 * Algorithmes:
 * - Moving average sur dernières N mesures
 * - Détection anomalies (outliers)
 * - Prédiction basée sur historique
 * - Ajustement progressif
 * 
 * Thread-safety: OUI
 * 
 * @example Utilisation
 * @code
 * LatencyCompensator compensator;
 * 
 * // Enregistrer un device
 * compensator.registerDevice("piano_001");
 * 
 * // Mesurer latence
 * uint64_t t1 = getTimestampUs();
 * sendMidiMessage(device, msg);
 * uint64_t t2 = getTimestampUs();
 * 
 * compensator.recordLatency("piano_001", t2 - t1);
 * 
 * // Obtenir compensation
 * int64_t offset = compensator.getCompensation("piano_001");
 * @endcode
 */
class LatencyCompensator {
public:
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    LatencyCompensator();
    ~LatencyCompensator();
    
    // Désactiver copie et assignation
    LatencyCompensator(const LatencyCompensator&) = delete;
    LatencyCompensator& operator=(const LatencyCompensator&) = delete;
    
    // ========================================================================
    // GESTION DES DEVICES
    // ========================================================================
    
    /**
     * @brief Enregistre un nouveau périphérique
     * @param deviceId ID du périphérique
     * @return bool true si enregistré
     */
    bool registerDevice(const std::string& deviceId);
    
    /**
     * @brief Désenregistre un périphérique
     * @param deviceId ID du périphérique
     */
    void unregisterDevice(const std::string& deviceId);
    
    /**
     * @brief Vérifie si un device est enregistré
     * @param deviceId ID du périphérique
     * @return bool true si enregistré
     */
    bool isDeviceRegistered(const std::string& deviceId) const;
    
    // ========================================================================
    // MESURE DE LATENCE
    // ========================================================================
    
    /**
     * @brief Enregistre une mesure de latence
     * @param deviceId ID du périphérique
     * @param latencyUs Latence en microsecondes
     */
    void recordLatency(const std::string& deviceId, uint64_t latencyUs);
    
    /**
     * @brief Démarre une mesure (retourne timestamp)
     * @return uint64_t Timestamp de début (µs)
     */
    uint64_t startMeasurement() const;
    
    /**
     * @brief Termine une mesure et enregistre
     * @param deviceId ID du périphérique
     * @param startTime Timestamp de début
     */
    void endMeasurement(const std::string& deviceId, uint64_t startTime);
    
    // ========================================================================
    // COMPENSATION
    // ========================================================================
    
    /**
     * @brief Récupère l'offset de compensation pour un device
     * @param deviceId ID du périphérique
     * @return int64_t Offset en microsecondes (positif = retarder)
     */
    int64_t getCompensation(const std::string& deviceId) const;
    
    /**
     * @brief Définit manuellement la compensation
     * @param deviceId ID du périphérique
     * @param offsetUs Offset en microsecondes
     */
    void setCompensation(const std::string& deviceId, int64_t offsetUs);
    
    /**
     * @brief Active/désactive la compensation auto pour un device
     * @param deviceId ID du périphérique
     * @param enabled true pour activer
     */
    void setAutoCompensation(const std::string& deviceId, bool enabled);
    
    /**
     * @brief Réinitialise la compensation d'un device
     * @param deviceId ID du périphérique
     */
    void resetCompensation(const std::string& deviceId);
    
    // ========================================================================
    // PROFILS
    // ========================================================================
    
    /**
     * @brief Récupère le profil d'un device
     * @param deviceId ID du périphérique
     * @return DeviceLatencyProfile Profil de latence
     */
    DeviceLatencyProfile getProfile(const std::string& deviceId) const;
    
    /**
     * @brief Liste tous les profils
     * @return std::vector<DeviceLatencyProfile> Liste des profils
     */
    std::vector<DeviceLatencyProfile> getAllProfiles() const;
    
    /**
     * @brief Exporte tous les profils en JSON
     * @return json Profils au format JSON
     */
    json exportProfiles() const;
    
    /**
     * @brief Importe des profils depuis JSON
     * @param profiles Profils au format JSON
     * @return bool true si importé avec succès
     */
    bool importProfiles(const json& profiles);
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit la taille de l'historique
     * @param size Nombre de mesures conservées
     */
    void setHistorySize(size_t size) {
        historySize_ = size;
    }
    
    /**
     * @brief Récupère la taille de l'historique
     * @return size_t Nombre de mesures
     */
    size_t getHistorySize() const {
        return historySize_;
    }
    
    /**
     * @brief Active/désactive la détection d'outliers
     * @param enabled true pour activer
     */
    void setOutlierDetection(bool enabled) {
        outlierDetectionEnabled_ = enabled;
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère des statistiques globales
     * @return json Statistiques au format JSON
     */
    json getStatistics() const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Détecte si une mesure est un outlier
     * @param profile Profil du device
     * @param latency Latence mesurée
     * @return bool true si outlier
     */
    bool isOutlier(const DeviceLatencyProfile& profile, uint64_t latency) const;
    
    /**
     * @brief Recalcule les statistiques d'un profil
     * @param profile Profil à mettre à jour
     */
    void updateStatistics(DeviceLatencyProfile& profile);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Profils de latence par device
    std::unordered_map<std::string, DeviceLatencyProfile> profiles_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Taille de l'historique (nombre de mesures conservées)
    size_t historySize_;
    
    /// Détection d'outliers activée
    bool outlierDetectionEnabled_;
    
    /// Seuil de détection d'outliers (écarts-types)
    double outlierThreshold_;
};

} // namespace midiMind

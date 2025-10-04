// ============================================================================
// Fichier: src/core/TimeUtils.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Utilitaires pour la gestion du temps et des timestamps.
//   Fournit des fonctions haute précision pour le timing MIDI.
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace midiMind {
namespace TimeUtils {

/**
 * @brief Type pour les timestamps haute précision (microsecondes)
 */
using Timestamp = uint64_t;

/**
 * @brief Récupère le timestamp actuel en microsecondes
 * 
 * @return uint64_t Timestamp en µs
 */
inline uint64_t getCurrentTimestampUs() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

/**
 * @brief Récupère le timestamp actuel en millisecondes
 * 
 * @return uint64_t Timestamp en ms
 */
inline uint64_t getCurrentTimestampMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

/**
 * @brief Récupère le timestamp actuel en secondes
 * 
 * @return uint64_t Timestamp en secondes
 */
inline uint64_t getCurrentTimestampSec() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

/**
 * @brief Convertit un timestamp en string lisible
 * 
 * @param timestamp Timestamp en millisecondes
 * @param format Format (défaut: "%Y-%m-%d %H:%M:%S")
 * @return std::string Date/heure formatée
 * 
 * @example
 * ```cpp
 * auto ts = TimeUtils::getCurrentTimestampMs();
 * std::string str = TimeUtils::timestampToString(ts);
 * // "2025-10-03 14:30:45"
 * ```
 */
inline std::string timestampToString(uint64_t timestamp, 
                                     const std::string& format = "%Y-%m-%d %H:%M:%S") {
    time_t seconds = timestamp / 1000;
    std::tm* tm = std::localtime(&seconds);
    
    std::ostringstream oss;
    oss << std::put_time(tm, format.c_str());
    
    return oss.str();
}

/**
 * @brief Convertit une durée en millisecondes en string lisible
 * 
 * @param durationMs Durée en millisecondes
 * @return std::string Durée formatée (ex: "1h 23m 45s")
 */
inline std::string durationToString(uint64_t durationMs) {
    uint64_t seconds = durationMs / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    
    std::ostringstream oss;
    
    if (hours > 0) {
        oss << hours << "h ";
    }
    if (minutes > 0 || hours > 0) {
        oss << minutes << "m ";
    }
    oss << seconds << "s";
    
    return oss.str();
}

/**
 * @brief Calcule la différence entre deux timestamps
 * 
 * @param start Timestamp de départ (ms)
 * @param end Timestamp de fin (ms)
 * @return uint64_t Différence en ms
 */
inline uint64_t timeDiff(uint64_t start, uint64_t end) {
    return end > start ? end - start : 0;
}

/**
 * @brief Convertit des microsecondes en millisecondes
 */
inline uint64_t usToMs(uint64_t us) {
    return us / 1000;
}

/**
 * @brief Convertit des millisecondes en microsecondes
 */
inline uint64_t msToUs(uint64_t ms) {
    return ms * 1000;
}

/**
 * @brief Convertit des secondes en millisecondes
 */
inline uint64_t secToMs(uint64_t sec) {
    return sec * 1000;
}

/**
 * @brief Convertit des millisecondes en secondes
 */
inline uint64_t msToSec(uint64_t ms) {
    return ms / 1000;
}

/**
 * @brief Convertit un tempo (BPM) en durée d'un beat (ms)
 * 
 * @param bpm Tempo en BPM
 * @return uint64_t Durée d'un beat en ms
 * 
 * @example
 * ```cpp
 * uint64_t beatDuration = TimeUtils::bpmToBeatDuration(120);
 * // 500ms (pour 120 BPM)
 * ```
 */
inline uint64_t bpmToBeatDuration(float bpm) {
    if (bpm <= 0.0f) return 0;
    return static_cast<uint64_t>(60000.0f / bpm);
}

/**
 * @brief Convertit une durée de beat (ms) en tempo (BPM)
 * 
 * @param beatDurationMs Durée d'un beat en ms
 * @return float Tempo en BPM
 */
inline float beatDurationToBpm(uint64_t beatDurationMs) {
    if (beatDurationMs == 0) return 0.0f;
    return 60000.0f / static_cast<float>(beatDurationMs);
}

/**
 * @brief Sleep haute précision en microsecondes
 * 
 * @param us Durée en microsecondes
 */
inline void sleepUs(uint64_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

/**
 * @brief Sleep en millisecondes
 * 
 * @param ms Durée en millisecondes
 */
inline void sleepMs(uint64_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/**
 * @brief Classe pour mesurer le temps d'exécution
 * 
 * @example
 * ```cpp
 * {
 *     TimeUtils::Timer timer;
 *     // Code à mesurer...
 *     uint64_t elapsed = timer.elapsedMs();
 * }
 * ```
 */
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    /**
     * @brief Réinitialise le timer
     */
    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    /**
     * @brief Récupère le temps écoulé en microsecondes
     */
    uint64_t elapsedUs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - start_).count();
    }
    
    /**
     * @brief Récupère le temps écoulé en millisecondes
     */
    uint64_t elapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }
    
    /**
     * @brief Récupère le temps écoulé en secondes
     */
    uint64_t elapsedSec() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - start_).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace TimeUtils
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER TimeUtils.h
// ============================================================================
// ============================================================================
// Fichier: src/midi/MidiClock.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Horloge MIDI pour la synchronisation tempo et timing.
//   Génère les messages Clock (0xF8) à 24 PPQN (Pulses Per Quarter Note).
//
// Responsabilités:
//   - Génération précise de clock MIDI
//   - Gestion du tempo (BPM)
//   - Synchronisation start/stop/continue
//   - Callbacks à chaque pulse et beat
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

#include "MidiMessage.h"
#include "../core/Logger.h"
#include "../core/TimeUtils.h"

namespace midiMind {

/**
 * @enum ClockState
 * @brief État de l'horloge MIDI
 */
enum class ClockState {
    STOPPED,    ///< Arrêtée
    PLAYING,    ///< En lecture
    PAUSED      ///< En pause
};

/**
 * @class MidiClock
 * @brief Horloge MIDI haute précision
 * 
 * @details
 * Génère des messages MIDI Clock à 24 PPQN selon le standard MIDI.
 * Utilise un thread haute priorité pour garantir la précision.
 * 
 * Timing MIDI:
 * - 24 pulses par quarter note (PPQN)
 * - À 120 BPM: 1 beat = 500ms, 1 pulse = 20.83ms
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * MidiClock clock;
 * 
 * // Configurer le tempo
 * clock.setTempo(120.0f);
 * 
 * // Callback à chaque beat
 * clock.setOnBeat([](uint32_t beat) {
 *     Logger::info("Beat: " + std::to_string(beat));
 * });
 * 
 * // Démarrer
 * clock.start();
 * ```
 */
class MidiClock {
public:
    // ========================================================================
    // CONSTANTES
    // ========================================================================
    
    static constexpr uint8_t PPQN = 24;              ///< Pulses per quarter note
    static constexpr float DEFAULT_TEMPO = 120.0f;   ///< Tempo par défaut (BPM)
    static constexpr float MIN_TEMPO = 20.0f;        ///< Tempo minimum
    static constexpr float MAX_TEMPO = 300.0f;       ///< Tempo maximum
    
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé à chaque pulse (24 fois par beat)
     */
    using PulseCallback = std::function<void(uint32_t pulse)>;
    
    /**
     * @brief Callback appelé à chaque beat
     */
    using BeatCallback = std::function<void(uint32_t beat)>;
    
    /**
     * @brief Callback appelé pour envoyer un message MIDI
     */
    using SendMessageCallback = std::function<void(const MidiMessage&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    MidiClock();
    
    /**
     * @brief Destructeur
     */
    ~MidiClock();
    
    // Désactiver copie
    MidiClock(const MidiClock&) = delete;
    MidiClock& operator=(const MidiClock&) = delete;
    
    // ========================================================================
    // CONTRÔLE
    // ========================================================================
    
    /**
     * @brief Démarre l'horloge
     * 
     * Envoie un message Start (0xFA) et commence à générer les pulses.
     * 
     * @note Thread-safe
     */
    void start();
    
    /**
     * @brief Arrête l'horloge
     * 
     * Envoie un message Stop (0xFC) et arrête la génération de pulses.
     * Réinitialise la position à 0.
     * 
     * @note Thread-safe
     */
    void stop();
    
    /**
     * @brief Met en pause l'horloge
     * 
     * Arrête la génération de pulses mais conserve la position.
     * 
     * @note Thread-safe
     */
    void pause();
    
    /**
     * @brief Reprend l'horloge
     * 
     * Envoie un message Continue (0xFB) et reprend depuis la position actuelle.
     * 
     * @note Thread-safe
     */
    void resume();
    
    /**
     * @brief Vérifie si l'horloge est en cours d'exécution
     * 
     * @note Thread-safe
     */
    bool isRunning() const;
    
    /**
     * @brief Récupère l'état actuel
     * 
     * @note Thread-safe
     */
    ClockState getState() const;
    
    // ========================================================================
    // TEMPO
    // ========================================================================
    
    /**
     * @brief Définit le tempo
     * 
     * @param bpm Tempo en BPM (20-300)
     * 
     * @note Thread-safe
     */
    void setTempo(float bpm);
    
    /**
     * @brief Récupère le tempo actuel
     * 
     * @return float Tempo en BPM
     * 
     * @note Thread-safe
     */
    float getTempo() const;
    
    /**
     * @brief Ajuste le tempo relatif (ex: +10 BPM)
     * 
     * @param delta Changement en BPM
     * 
     * @note Thread-safe
     */
    void adjustTempo(float delta);
    
    // ========================================================================
    // POSITION
    // ========================================================================
    
    /**
     * @brief Récupère la position actuelle en pulses
     * 
     * @note Thread-safe
     */
    uint32_t getPulse() const;
    
    /**
     * @brief Récupère la position actuelle en beats
     * 
     * @note Thread-safe
     */
    uint32_t getBeat() const;
    
    /**
     * @brief Définit la position en pulses
     * 
     * @param pulse Position en pulses
     * 
     * @note Thread-safe
     */
    void setPulse(uint32_t pulse);
    
    /**
     * @brief Définit la position en beats
     * 
     * @param beat Position en beats
     * 
     * @note Thread-safe
     */
    void setBeat(uint32_t beat);
    
    /**
     * @brief Réinitialise la position à 0
     * 
     * @note Thread-safe
     */
    void reset();
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de pulse
     * 
     * Appelé 24 fois par beat.
     * 
     * @note Thread-safe
     */
    void setOnPulse(PulseCallback callback);
    
    /**
     * @brief Définit le callback de beat
     * 
     * Appelé à chaque quarter note.
     * 
     * @note Thread-safe
     */
    void setOnBeat(BeatCallback callback);
    
    /**
     * @brief Définit le callback d'envoi de message
     * 
     * Utilisé pour envoyer les messages Clock, Start, Stop, Continue.
     * 
     * @note Thread-safe
     */
    void setOnSendMessage(SendMessageCallback callback);
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques
     * 
     * @return json Statistiques
     * 
     * @note Thread-safe
     */
    json getStatistics() const;

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread de génération de clock
     */
    void clockThread();
    
    /**
     * @brief Calcule l'intervalle entre pulses (µs)
     */
    uint64_t calculatePulseInterval() const;
    
    /**
     * @brief Envoie un message MIDI
     */
    void sendMessage(const MidiMessage& msg);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Thread de génération de clock
    std::thread clockThread_;
    
    /// État de l'horloge
    std::atomic<ClockState> state_;
    
    /// Tempo actuel (BPM)
    std::atomic<float> tempo_;
    
    /// Position actuelle (pulses)
    std::atomic<uint32_t> pulse_;
    
    /// Flag d'arrêt
    std::atomic<bool> stop_;
    
    /// Callbacks
    PulseCallback onPulse_;
    BeatCallback onBeat_;
    SendMessageCallback onSendMessage_;
    
    /// Statistiques
    uint64_t totalPulses_;
    uint64_t totalBeats_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiClock.h
// ============================================================================
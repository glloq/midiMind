// ============================================================================
// Fichier: backend/src/midi/player/MidiPlayer.h
// Version: 3.0.2 - CORRECTIONS COMPLÈTES
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - ✅ Forward declaration struct MidiFile
// - ✅ Définition de StateCallback
// - ✅ Suppression des redéfinitions de TempoChange et TrackInfo (déjà dans MidiFileAnalyzer.h)
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include "../MidiFile.h"  // ✅ Include pour MidiFile
#include "../../core/Logger.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace midiMind {

using json = nlohmann::json;

// ========================================================================
// ÉNUMÉRATIONS
// ========================================================================

/**
 * @enum PlaybackState
 * @brief État de la lecture
 */
enum class PlaybackState {
    STOPPED,
    PLAYING,
    PAUSED
};

/**
 * @enum RepeatMode
 * @brief Mode de répétition
 */
enum class RepeatMode {
    NONE,
    ONE,
    ALL
};

// ========================================================================
// TYPES
// ========================================================================

/**
 * @brief ✅ AJOUT: Callback de changement d'état
 */
using StateCallback = std::function<void(PlaybackState oldState, PlaybackState newState)>;

/**
 * @brief Callback de position
 */
using PositionCallback = std::function<void(double positionMs, double durationMs)>;

/**
 * @brief Callback de message
 */
using MessageCallback = std::function<void(const MidiMessage&)>;

// ========================================================================
// CLASSE MIDIPLAYER
// ========================================================================

/**
 * @class MidiPlayer
 * @brief Lecteur de fichiers MIDI avec contrôle de lecture
 */
class MidiPlayer {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    MidiPlayer();
    ~MidiPlayer();
    
    // Non-copiable
    MidiPlayer(const MidiPlayer&) = delete;
    MidiPlayer& operator=(const MidiPlayer&) = delete;
    
    // ========================================================================
    // CHARGEMENT
    // ========================================================================
    
    /**
     * @brief Charge un fichier MIDI
     * @param filepath Chemin du fichier
     * @return true si succès
     */
    bool load(const std::string& filepath);
    
    /**
     * @brief Charge depuis un MidiFile
     * @param midiFile Structure MidiFile
     * @return true si succès
     */
    bool loadFromMidiFile(const MidiFile& midiFile);
    
    /**
     * @brief Décharge le fichier actuel
     */
    void unload();
    
    /**
     * @brief Vérifie si un fichier est chargé
     */
    bool isLoaded() const;
    
    // ========================================================================
    // CONTRÔLE DE LECTURE
    // ========================================================================
    
    /**
     * @brief Démarre la lecture
     * @return true si démarré
     */
    bool play();
    
    /**
     * @brief Met en pause
     */
    void pause();
    
    /**
     * @brief Arrête la lecture
     */
    void stop();
    
    /**
     * @brief Récupère l'état
     */
    PlaybackState getState() const;
    
    /**
     * @brief Vérifie si en cours de lecture
     */
    bool isPlaying() const;
    
    /**
     * @brief Vérifie si en pause
     */
    bool isPaused() const;
    
    // ========================================================================
    // NAVIGATION
    // ========================================================================
    
    /**
     * @brief Se déplace à une position en millisecondes
     * @param positionMs Position en ms
     */
    void seek(double positionMs);
    
    /**
     * @brief Se déplace à une position en ticks
     * @param tick Position en ticks
     */
    void seekToTick(uint32_t tick);
    
    /**
     * @brief Récupère la position actuelle en ms
     */
    double getPosition() const;
    
    /**
     * @brief Récupère la durée totale en ms
     */
    double getDuration() const;
    
    /**
     * @brief Récupère le pourcentage de progression (0-100)
     */
    double getProgressPercent() const;
    
    // ========================================================================
    // TEMPO ET TIMING
    // ========================================================================
    
    /**
     * @brief Définit la vitesse de lecture (1.0 = normal)
     * @param speed Vitesse (0.25 à 4.0)
     */
    void setSpeed(float speed);
    
    /**
     * @brief Récupère la vitesse actuelle
     */
    float getSpeed() const;
    
    /**
     * @brief Récupère le tempo actuel en BPM
     */
    double getCurrentTempo() const;
    
    // ========================================================================
    // RÉPÉTITION
    // ========================================================================
    
    /**
     * @brief Définit le mode de répétition
     */
    void setRepeatMode(RepeatMode mode);
    
    /**
     * @brief Récupère le mode de répétition
     */
    RepeatMode getRepeatMode() const;
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de changement d'état
     */
    void setStateCallback(StateCallback callback);
    
    /**
     * @brief Définit le callback de position
     */
    void setPositionCallback(PositionCallback callback);
    
    /**
     * @brief Définit le callback de message MIDI
     */
    void setMessageCallback(MessageCallback callback);
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère les informations du fichier
     */
    json getFileInfo() const;
    
    /**
     * @brief Récupère le nombre de pistes
     */
    int getTrackCount() const;
    
    /**
     * @brief Récupère le format MIDI (0, 1, ou 2)
     */
    int getMidiFormat() const;
    
    /**
     * @brief Récupère la résolution en ticks par quarter note
     */
    int getTicksPerQuarterNote() const;
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques de lecture
     */
    json getStats() const;
    
    /**
     * @brief Réinitialise les statistiques
     */
    void resetStats();

private:
    // ========================================================================
    // MÉTHODES PRIVÉES
    // ========================================================================
    
    /**
     * @brief Thread de lecture
     */
    void playbackThread();
    
    /**
     * @brief Calcule le timestamp du prochain événement
     */
    uint64_t calculateNextEventTime(uint32_t deltaTicks);
    
    /**
     * @brief Traite un événement MIDI
     */
    void processEvent(const MidiMessage& message);
    
    /**
     * @brief Met à jour le tempo actuel
     */
    void updateTempo(double newTempo);
    
    /**
     * @brief Notifie le changement d'état
     */
    void notifyStateChange(PlaybackState newState);
    
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// État de lecture
    std::atomic<PlaybackState> state_;
    
    /// Fichier MIDI chargé
    MidiFile midiFile_;
    
    /// Thread de lecture
    std::thread playbackThread_;
    
    /// Flag d'arrêt
    std::atomic<bool> stopFlag_;
    
    /// Position actuelle (ms)
    std::atomic<double> position_;
    
    /// Durée totale (ms)
    double duration_;
    
    /// Vitesse de lecture
    std::atomic<float> speed_;
    
    /// Tempo actuel (BPM)
    std::atomic<double> currentTempo_;
    
    /// Mode de répétition
    std::atomic<RepeatMode> repeatMode_;
    
    /// Callbacks
    StateCallback stateCallback_;
    PositionCallback positionCallback_;
    MessageCallback messageCallback_;
    
    /// Statistiques
    uint64_t totalMessagesPlayed_;
    uint64_t totalNotesPlayed_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiPlayer.h
// ============================================================================

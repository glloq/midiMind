// ============================================================================
// Fichier: backend/src/midi/player/MidiPlayer.h
// Version: 3.1.0 - PHASE 1 COMPLETE - Structures enrichies
// Date: 2025-10-10
// ============================================================================
// Description:
//   Lecteur de fichiers MIDI avec contrôles avancés et métadonnées complètes.
//   Supporte bar/beat positioning, métadonnées enrichies, track info détaillées.
//
// Modifications Phase 1:
//   ✅ Ajout structure MusicalPosition (bar/beat/tick)
//   ✅ Enrichissement TrackInfo (channel, program, instrument, notes)
//   ✅ Ajout membres privés métadonnées (copyright, time signature, key, tempo)
//
// Auteur: MidiMind Team
// ============================================================================

#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <memory>              // Pour std::shared_ptr
#include <string>              // Pour std::string
#include <vector>              // Pour std::vector
#include <thread>              // Pour std::thread
#include <atomic>              // Pour std::atomic
#include <mutex>               // Pour std::mutex
#include <functional>          // Pour std::function
#include <chrono>              // Pour timing
#include <nlohmann/json.hpp>   // Pour getStatus()

#include "../MidiMessage.h"
#include "../MidiRouter.h"
#include "../../core/Logger.h"

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

/**
 * @enum PlayerState
 * @brief États du lecteur MIDI
 */
enum class PlayerState {
    STOPPED,       ///< Arrêté (pas de fichier ou stop appelé)
    PLAYING,       ///< En cours de lecture
    PAUSED         ///< En pause
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct MusicalPosition
 * @brief Position musicale (bar:beat:tick)
 * 
 * @details
 * Représente une position dans le fichier MIDI en notation musicale
 * plutôt qu'en temps absolu. Utile pour la navigation et l'édition.
 * 
 * @note Toutes les valeurs sont 1-based (bar 1 = première mesure)
 * 
 * @example
 * MusicalPosition pos;
 * pos.bar = 12;       // Mesure 12
 * pos.beat = 3;       // 3ème temps
 * pos.tick = 120;     // Tick 120 dans ce beat
 * pos.formatted = "12:3:120";
 */
struct MusicalPosition {
    int bar;              ///< Numéro de mesure (1-based)
    int beat;             ///< Numéro de temps dans la mesure (1-based)
    int tick;             ///< Tick dans le beat (0-based)
    int numerator;        ///< Numérateur de la signature (ex: 4 dans 4/4)
    int denominator;      ///< Dénominateur de la signature (ex: 4 dans 4/4)
    std::string formatted; ///< Position formatée "bar:beat:tick"
    
    /**
     * @brief Constructeur par défaut
     */
    MusicalPosition()
        : bar(1)
        , beat(1)
        , tick(0)
        , numerator(4)
        , denominator(4)
        , formatted("1:1:0") {}
    
    /**
     * @brief Constructeur avec valeurs
     */
    MusicalPosition(int b, int bt, int t, int num = 4, int den = 4)
        : bar(b)
        , beat(bt)
        , tick(t)
        , numerator(num)
        , denominator(den) {
        formatted = std::to_string(bar) + ":" + 
                   std::to_string(beat) + ":" + 
                   std::to_string(tick);
    }
};

/**
 * @struct TempoChange
 * @brief Représente un changement de tempo dans le fichier
 */
struct TempoChange {
    uint64_t tick;        ///< Position en ticks
    uint32_t timeMs;      ///< Position en millisecondes
    float bpm;            ///< Tempo en BPM
    
    TempoChange() : tick(0), timeMs(0), bpm(120.0f) {}
    TempoChange(uint64_t t, uint32_t ms, float b) 
        : tick(t), timeMs(ms), bpm(b) {}
};

/**
 * @struct TrackInfo
 * @brief Informations complètes sur une piste MIDI
 * 
 * @details
 * Structure enrichie fusionnant les informations de playback et d'analyse.
 * Combine les données nécessaires pour la lecture (mute/solo/volume) et
 * les métadonnées pour l'affichage (instrument, notes, densité).
 */
struct TrackInfo {
    // ========================================================================
    // IDENTIFIANTS ET MÉTADONNÉES
    // ========================================================================
    int trackNumber;           ///< Numéro de piste (0-based)
    std::string name;          ///< Nom de la piste (meta event 0x03)
    int eventCount;            ///< Nombre total d'événements
    
    // ========================================================================
    // CONTRÔLES DE PLAYBACK
    // ========================================================================
    bool muted;                ///< Piste mutée
    bool solo;                 ///< Piste en solo
    int transpose;             ///< Transposition (demi-tons, -24 à +24)
    float volume;              ///< Volume (0.0 à 2.0, défaut 1.0)
    
    // ========================================================================
    // INFORMATIONS MIDI (NOUVEAU - Phase 1.2)
    // ========================================================================
    uint8_t channel;           ///< Canal MIDI principal (0-15, 255 = multiple)
    uint8_t programChange;     ///< Numéro d'instrument (0-127, 255 = none)
    std::string instrumentName; ///< Nom de l'instrument (General MIDI)
    
    // ========================================================================
    // STATISTIQUES NOTES (NOUVEAU - Phase 1.2)
    // ========================================================================
    uint32_t noteCount;        ///< Nombre de notes dans la piste
    uint8_t minNote;           ///< Note la plus basse (0-127)
    uint8_t maxNote;           ///< Note la plus haute (0-127)
    uint8_t avgVelocity;       ///< Vélocité moyenne (0-127)
    float noteDensity;         ///< Notes par seconde
    
    /**
     * @brief Constructeur par défaut
     * @param num Numéro de piste
     */
    TrackInfo(int num)
        : trackNumber(num)
        , name("Track " + std::to_string(num + 1))
        , eventCount(0)
        , muted(false)
        , solo(false)
        , transpose(0)
        , volume(1.0f)
        , channel(255)  // 255 = non défini ou multiple
        , programChange(255)  // 255 = non défini
        , instrumentName("Unknown")
        , noteCount(0)
        , minNote(127)
        , maxNote(0)
        , avgVelocity(64)
        , noteDensity(0.0f) {}
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["track_number"] = trackNumber;
        j["name"] = name;
        j["event_count"] = eventCount;
        j["muted"] = muted;
        j["solo"] = solo;
        j["transpose"] = transpose;
        j["volume"] = volume;
        
        // Nouvelles infos
        if (channel != 255) j["channel"] = channel;
        if (programChange != 255) j["program_change"] = programChange;
        j["instrument_name"] = instrumentName;
        j["note_count"] = noteCount;
        if (noteCount > 0) {
            j["min_note"] = minNote;
            j["max_note"] = maxNote;
            j["avg_velocity"] = avgVelocity;
            j["note_density"] = noteDensity;
        }
        
        return j;
    }
};

// ============================================================================
// CLASSE: MidiPlayer
// ============================================================================

/**
 * @class MidiPlayer
 * @brief Lecteur de fichiers MIDI avec contrôles avancés et métadonnées complètes
 * 
 * Le MidiPlayer charge des fichiers MIDI, les parse, et joue les événements
 * en temps réel via le MidiRouter. Support complet de playback avec tempo,
 * transposition, mute/solo, et synchronisation précise.
 * 
 * @details
 * Architecture:
 * - Thread de lecture qui envoie les événements au bon moment
 * - Synchronisation précise via high_resolution_clock
 * - Support des changements de tempo en temps réel
 * - Mute/solo par piste
 * - Transposition globale et par piste
 * - Navigation bar/beat (NOUVEAU Phase 1)
 * - Métadonnées enrichies (NOUVEAU Phase 1)
 * 
 * @note Thread-safe : toutes les méthodes publiques sont thread-safe
 * 
 * @example Utilisation:
 * @code
 * auto player = std::make_shared<MidiPlayer>(router);
 * 
 * // Charger un fichier
 * if (player->load("/path/to/file.mid")) {
 *     // Récupérer métadonnées
 *     auto metadata = player->getMetadata();
 *     std::cout << "Tempo: " << metadata["initial_tempo"] << " BPM" << std::endl;
 *     
 *     // Jouer
 *     player->play();
 *     
 *     // Seek vers mesure 12, temps 3
 *     player->seekToBar(12, 3);
 *     
 *     // Récupérer position musicale
 *     auto pos = player->getMusicalPosition();
 *     std::cout << "Position: " << pos.formatted << std::endl;
 *     
 *     // Arrêter
 *     player->stop();
 * }
 * @endcode
 */
class MidiPlayer {
public:
    // ========================================================================
    // TYPES - CALLBACKS
    // ========================================================================
    
    /**
     * @typedef StateCallback
     * @brief Callback appelé lors de changements d'état
     */
    using StateCallback = std::function<void(const std::string& newState)>;
    
    // ========================================================================
    // CONSTRUCTEUR / DESTRUCTEUR
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param router Routeur MIDI pour envoyer les messages
     */
    explicit MidiPlayer(std::shared_ptr<MidiRouter> router);
    
    /**
     * @brief Destructeur
     */
    ~MidiPlayer();
    
    // Désactiver copie et assignation
    MidiPlayer(const MidiPlayer&) = delete;
    MidiPlayer& operator=(const MidiPlayer&) = delete;
    
    // ========================================================================
    // CHARGEMENT DE FICHIERS
    // ========================================================================
    
    /**
     * @brief Charge un fichier MIDI
     * 
     * @param filepath Chemin du fichier MIDI
     * @return true Si le chargement a réussi
     * @return false Si le chargement a échoué
     */
    bool load(const std::string& filepath);
    
    /**
     * @brief Récupère le chemin du fichier actuellement chargé
     * 
     * @return std::string Chemin du fichier, ou vide si aucun fichier
     */
    std::string getCurrentFile() const;
    
    /**
     * @brief Vérifie si un fichier est chargé
     * 
     * @return true Si un fichier est chargé
     */
    bool hasFile() const;
    
    // ========================================================================
    // CONTRÔLES DE LECTURE
    // ========================================================================
    
    /**
     * @brief Démarre ou reprend la lecture
     * 
     * @return true Si la lecture a démarré
     */
    bool play();
    
    /**
     * @brief Met la lecture en pause
     * 
     * @return true Si la pause a été appliquée
     */
    bool pause();
    
    /**
     * @brief Arrête la lecture
     */
    void stop();
    
    /**
     * @brief Déplace la position de lecture (en millisecondes)
     * 
     * @param positionMs Position en millisecondes
     * @return true Si le seek a réussi
     */
    bool seek(uint64_t positionMs);
    
    /**
     * @brief Déplace la lecture à une position musicale (NOUVEAU Phase 1)
     * 
     * @param bar Numéro de mesure (1-based)
     * @param beat Numéro de temps (1-based, défaut = 1)
     * @param tick Tick dans le beat (défaut = 0)
     * @return true Si le seek a réussi
     * 
     * @note Valide les paramètres selon la time signature
     * 
     * @example
     * player->seekToBar(12, 3);  // Mesure 12, temps 3
     * player->seekToBar(5);      // Mesure 5, début (beat 1)
     */
    bool seekToBar(int bar, int beat = 1, int tick = 0);
    
    // ========================================================================
    // ÉTAT ET INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère l'état actuel
     * 
     * @return PlayerState État
     */
    PlayerState getState() const;
    
    /**
     * @brief Vérifie si le player est en cours de lecture
     * 
     * @return true Si en lecture
     */
    bool isPlaying() const;
    
    /**
     * @brief Récupère la position actuelle (ms)
     * 
     * @return uint64_t Position en millisecondes
     */
    uint64_t getPosition() const;
    
    /**
     * @brief Récupère la durée totale (ms)
     * 
     * @return uint64_t Durée en millisecondes
     */
    uint64_t getDuration() const;
    
    /**
     * @brief Récupère la position actuelle en ticks
     * 
     * @return uint64_t Position en ticks MIDI
     */
    uint64_t getCurrentTick() const;
    
    /**
     * @brief Récupère la durée totale en ticks
     * 
     * @return uint64_t Durée en ticks MIDI
     */
    uint64_t getTotalTicks() const;
    
    /**
     * @brief Récupère la position musicale actuelle (NOUVEAU Phase 1)
     * 
     * @return MusicalPosition Structure avec bar, beat, tick
     * 
     * @example
     * auto pos = player->getMusicalPosition();
     * std::cout << "Bar: " << pos.bar << ", Beat: " << pos.beat << std::endl;
     * std::cout << "Position: " << pos.formatted << std::endl;
     */
    MusicalPosition getMusicalPosition() const;
    
    /**
     * @brief Récupère les métadonnées du fichier chargé (NOUVEAU Phase 1)
     * 
     * @return json Objet JSON avec métadonnées complètes
     * 
     * @details Contenu retourné:
     * - filename: Nom du fichier
     * - format: Format MIDI (0, 1, ou 2)
     * - ticks_per_quarter_note: Résolution temporelle
     * - initial_tempo: Tempo initial en BPM
     * - time_signature: Signature rythmique (ex: "4/4")
     * - key_signature: Tonalité (ex: "C major")
     * - copyright: Copyright du fichier
     * - has_tempo_changes: Présence de changements de tempo
     * - tempo_changes_count: Nombre de changements
     */
    json getMetadata() const;
    
    /**
     * @brief Récupère le statut complet (JSON)
     * 
     * @return json Objet JSON avec toutes les infos
     * 
     * @details Inclut maintenant (Phase 1):
     * - bar, beat, tick: Position musicale
     * - time_signature, key_signature: Métadonnées
     * - copyright: Copyright
     */
    json getStatus() const;
    
    // ========================================================================
    // CONTRÔLES AVANCÉS
    // ========================================================================
    
    /**
     * @brief Définit le tempo (multiplicateur)
     * 
     * @param tempo Tempo (0.5 = moitié, 2.0 = double, etc.)
     */
    void setTempo(double tempo);
    
    /**
     * @brief Récupère le tempo actuel
     * 
     * @return double Tempo
     */
    double getTempo() const;
    
    /**
     * @brief Active/désactive le mode boucle
     * 
     * @param enabled true pour activer la boucle
     */
    void setLoop(bool enabled);
    
    /**
     * @brief Vérifie si le mode boucle est activé
     * 
     * @return true Si la boucle est activée
     */
    bool isLooping() const;
    
    /**
     * @brief Définit la transposition globale
     * 
     * @param semitones Nombre de demi-tons (-24 à +24)
     */
    void setTranspose(int semitones);
    
    /**
     * @brief Récupère la transposition globale
     * 
     * @return int Demi-tons
     */
    int getTranspose() const;
    
    // ========================================================================
    // GESTION DES PISTES
    // ========================================================================
    
    /**
     * @brief Récupère la liste des pistes
     * 
     * @return std::vector<TrackInfo> Liste des pistes avec infos enrichies
     */
    std::vector<TrackInfo> getTracks() const;
    
    /**
     * @brief Mute/unmute une piste
     * 
     * @param trackNumber Numéro de piste (0-based)
     * @param muted true pour muter
     * @return true Si la piste a été modifiée
     */
    bool setTrackMute(int trackNumber, bool muted);
    
    /**
     * @brief Active/désactive le solo sur une piste
     * 
     * @param trackNumber Numéro de piste (0-based)
     * @param solo true pour solo
     * @return true Si la piste a été modifiée
     */
    bool setTrackSolo(int trackNumber, bool solo);
    
    /**
     * @brief Définit la transposition d'une piste
     * 
     * @param trackNumber Numéro de piste (0-based)
     * @param semitones Demi-tons (-24 à +24)
     * @return true Si la modification a réussi
     */
    bool setTrackTranspose(int trackNumber, int semitones);
    
    /**
     * @brief Définit le volume d'une piste
     * 
     * @param trackNumber Numéro de piste (0-based)
     * @param volume Volume (0.0 à 2.0)
     * @return true Si la modification a réussi
     */
    bool setTrackVolume(int trackNumber, float volume);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de changement d'état
     * 
     * @param callback Fonction appelée lors des changements d'état
     */
    void onStateChanged(StateCallback callback);
    
    /**
     * @brief Alias pour setStateCallback (compatibilité API)
     */
    void setStateCallback(StateCallback callback) {
        onStateChanged(callback);
    }

  
    // ========================================================================
    // ✅ NOUVELLES MÉTHODES - VOLUME CONTROL
    // ========================================================================
    
    /**
     * @brief Définit le volume master du player
     * 
     * Contrôle le volume global de sortie. Affecte tous les messages MIDI
     * en ajustant les vélocités proportionnellement.
     * 
     * @param volume Volume (0.0 à 1.0)
     *               0.0 = muet
     *               0.5 = 50%
     *               1.0 = volume original (100%)
     * 
     * @note Le volume est appliqué en temps réel pendant la lecture
     * @note Les vélocités sont limitées à 127 (max MIDI)
     * 
     * @example
     * ```cpp
     * player->setMasterVolume(0.8f);  // 80% du volume
     * player->setMasterVolume(0.0f);  // Muet
     * player->setMasterVolume(1.0f);  // Volume original
     * ```
     */
    void setMasterVolume(float volume);
    
    /**
     * @brief Récupère le volume master actuel
     * 
     * @return float Volume actuel (0.0 à 1.0)
     * 
     * @example
     * ```cpp
     * float currentVolume = player->getMasterVolume();
     * std::cout << "Volume: " << (currentVolume * 100) << "%" << std::endl;
     * ```
     */
    float getMasterVolume() const;
    
    // ========================================================================
    // MÉTHODES AUXILIAIRES VOLUME (optionnelles mais recommandées)
    // ========================================================================
    
    /**
     * @brief Augmente le volume de 10%
     * 
     * @return float Nouveau volume après augmentation
     */
    float increaseVolume();
    
    /**
     * @brief Diminue le volume de 10%
     * 
     * @return float Nouveau volume après diminution
     */
    float decreaseVolume();
    
    /**
     * @brief Mute/unmute le son
     * 
     * @param mute true pour muter, false pour unmute
     */
    void setMute(bool mute);
    
    /**
     * @brief Vérifie si le son est muté
     * 
     * @return true Si muté
     */
    bool isMuted() const;







private:
    // ========================================================================
    // MÉTHODES PRIVÉES - CONVERSION TEMPORELLE (NOUVEAU Phase 1)
    // ========================================================================
    
    /**
     * @brief Convertit ticks → position musicale (bar:beat:tick)
     */
    MusicalPosition ticksToMusicalPosition(uint64_t ticks) const;
    
    /**
     * @brief Convertit position musicale → ticks
     */
    uint64_t musicalPositionToTicks(int bar, int beat, int tick) const;
    
    // ========================================================================
    // MÉTHODES PRIVÉES - PARSING ET ANALYSE
    // ========================================================================
    
    /**
     * @brief Parse toutes les pistes et fusionne les événements
     */
    void parseAllTracks();
    
    /**
     * @brief Calcule la durée totale en ticks
     */
    uint64_t calculateTotalTicks() const;
    
    /**
     * @brief Extrait le nom d'une piste
     */
    std::string getTrackName(size_t trackIndex) const;
    
    /**
     * @brief Extrait les métadonnées du fichier (NOUVEAU Phase 1)
     */
    void extractMetadata();
    
    /**
     * @brief Analyse une piste pour extraire infos enrichies (NOUVEAU Phase 1)
     */
    void analyzeTrack(size_t trackIndex);
    
    /**
     * @brief Mapper Program Change → Instrument Name (NOUVEAU Phase 1)
     */
    std::string getInstrumentName(uint8_t programChange) const;
    
    // ========================================================================
    // MÉTHODES PRIVÉES - PLAYBACK
    // ========================================================================
    
    /**
     * @brief Boucle de lecture (thread)
     */
    void playbackLoop();
    
    /**
     * @brief Joue les événements jusqu'à un tick donné
     */
    void playEventsUntil(uint64_t targetTick);
    
    /**
     * @brief Vérifie si un événement doit être joué (mute/solo)
     */
    bool shouldPlayEvent(const struct ScheduledEvent& event) const;
    
    /**
     * @brief Traite et envoie un événement
     */
    void processEvent(const struct ScheduledEvent& scheduled);
    
    /**
     * @brief Gère les meta events
     */
    void handleMetaEvent(const struct MidiEvent& event);
    
    /**
     * @brief Envoie All Notes Off sur tous les canaux
     */
    void sendAllNotesOff();
    
    /**
     * @brief Arrête la lecture (interne, sans lock)
     */
    void stopPlayback();
    
    /**
     * @brief Convertit l'état en string
     */
    std::string stateToString(PlayerState state) const;
    
    // ========================================================================
    // MEMBRES PRIVÉS - CORE
    // ========================================================================
    
    /// Routeur MIDI
    std::shared_ptr<MidiRouter> router_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Thread de lecture
    std::thread playbackThread_;
    
    /// État actuel
    PlayerState state_;
    
    /// Flag de run du thread
    bool running_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - FICHIER ET DONNÉES
    // ========================================================================
    
    /// Fichier actuellement chargé
    std::string currentFile_;
    
    /// Structure MidiFile parsée
    struct MidiFile midiFile_;
    
    /// Tous les événements triés par tick
    std::vector<struct ScheduledEvent> allEvents_;
    
    /// Informations des pistes (enrichies Phase 1)
    std::vector<TrackInfo> tracks_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - POSITION ET TIMING
    // ========================================================================
    
    /// Position actuelle en ticks
    uint64_t currentTick_;
    
    /// Durée totale en ticks
    uint64_t totalTicks_;
    
    /// Résolution temporelle (ticks per quarter note)
    uint16_t ticksPerQuarterNote_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - CONTRÔLES
    // ========================================================================
    
    /// Tempo (BPM)
    float tempo_;
    
    /// Transposition globale (demi-tons)
    int transpose_;
    
    /// Mode boucle activé
    bool loopEnabled_;
    
    // ========================================================================
    // MEMBRES PRIVÉS - MÉTADONNÉES (NOUVEAU Phase 1.3)
    // ========================================================================
    
    /// Copyright du fichier (meta event 0x02)
    std::string copyright_;
    
    /// Tonalité (ex: "C major", "A minor")
    std::string keySignature_;
    
    /// Signature rythmique formatée (ex: "4/4", "3/4")
    std::string timeSignatureStr_;
    
    /// Numérateur de la signature (ex: 4 dans 4/4)
    uint8_t timeSignatureNum_;
    
    /// Dénominateur de la signature (ex: 4 dans 4/4)
    uint8_t timeSignatureDen_;
    
    /// Tempo initial du fichier (BPM)
    float initialTempo_;
    
    /// Map des changements de tempo dans le fichier
    std::vector<TempoChange> tempoChanges_;
    
	 // ========================================================================
    // NOUVEAUX MEMBRES PRIVÉS - VOLUME
    // ========================================================================
    
    float masterVolume_;      ///< Volume master (0.0 - 1.0)
    bool isMuted_;            ///< État mute
    float volumeBeforeMute_;  ///< Volume avant mute (pour unmute)
    
    /**
     * @brief Applique le volume master à un message MIDI
     * 
     * Modifie la vélocité des notes en fonction du volume master.
     * 
     * @param message Message MIDI à modifier
     * @return MidiMessage Message avec vélocité ajustée
     */
    MidiMessage applyMasterVolume(const MidiMessage& message) const;
};
	
	
	
    // ========================================================================
    // MEMBRES PRIVÉS - CALLBACKS
    // ========================================================================
    
    /// Callback de changement d'état
    StateCallback stateCallback_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiPlayer.h - Version 3.1.0 Phase 1 Complete
// ============================================================================

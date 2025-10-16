// ============================================================================
// File: backend/src/midi/player/MidiPlayer.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   MIDI file player with precise timing and tempo control
//
// Features:
//   - Load and play MIDI files
//   - Tempo control (50% - 200%)
//   - Seek by time or bar/beat
//   - Track mute/solo
//   - Loop playback
//   - Master volume
//   - Transposition
//   - Real-time metadata
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Integrated with MidiFileReader v4.1.0
//   - Enhanced metadata extraction
//   - Better thread synchronization
//   - Improved timing precision
//
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include "../MidiRouter.h"
#include "../file/MidiFileReader.h"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// ============================================================================
// ENUMS
// ============================================================================

/**
 * @enum PlayerState
 * @brief Player state
 */
enum class PlayerState {
    STOPPED,
    PLAYING,
    PAUSED
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct TrackInfo
 * @brief Information about a MIDI track
 */
struct TrackInfo {
    uint16_t index = 0;
    std::string name;
    uint8_t channel = 0;
    uint8_t programChange = 0;
    std::string instrumentName;
    uint16_t noteCount = 0;
    uint8_t minNote = 127;
    uint8_t maxNote = 0;
    uint8_t avgVelocity = 64;
    bool isMuted = false;
    bool isSolo = false;
};

/**
 * @struct MusicalPosition
 * @brief Position in musical notation
 */
struct MusicalPosition {
    uint32_t bar = 1;          ///< Bar number (1-based)
    uint8_t beat = 1;          ///< Beat in bar (1-based)
    uint16_t tick = 0;         ///< Tick within beat
    std::string formatted;     ///< "bar:beat:tick"
};

/**
 * @struct ScheduledEvent
 * @brief MIDI event with scheduling information
 */
struct ScheduledEvent {
    uint64_t tick = 0;
    uint64_t absoluteTime = 0;
    MidiMessage message;
    uint16_t trackNumber = 0;
    bool processed = false;
};

// ============================================================================
// CLASS: MidiPlayer
// ============================================================================

/**
 * @class MidiPlayer
 * @brief MIDI file player with precise timing
 * 
 * Plays MIDI files with accurate timing, supports tempo changes,
 * track mute/solo, transposition, and loop playback.
 * 
 * Thread Safety: YES (all public methods are thread-safe)
 * 
 * Example:
 * ```cpp
 * auto router = std::make_shared<MidiRouter>();
 * auto player = std::make_shared<MidiPlayer>(router);
 * 
 * // Load file
 * if (player->load("/path/to/file.mid")) {
 *     // Get metadata
 *     auto metadata = player->getMetadata();
 *     std::cout << "Duration: " << metadata["duration_ms"] << " ms" << std::endl;
 *     
 *     // Play
 *     player->play();
 *     
 *     // Seek to 30 seconds
 *     player->seek(30000);
 *     
 *     // Change tempo
 *     player->setTempo(140.0);
 *     
 *     // Stop
 *     player->stop();
 * }
 * ```
 */
class MidiPlayer {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback for state changes
     */
    using StateCallback = std::function<void(const std::string& newState)>;
    
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param router MIDI router for message output
     */
    explicit MidiPlayer(std::shared_ptr<MidiRouter> router);
    
    /**
     * @brief Destructor
     */
    ~MidiPlayer();
    
    // Disable copy
    MidiPlayer(const MidiPlayer&) = delete;
    MidiPlayer& operator=(const MidiPlayer&) = delete;
    
    // ========================================================================
    // FILE LOADING
    // ========================================================================
    
    /**
     * @brief Load MIDI file
     * @param filepath Path to .mid/.midi file
     * @return true if loaded successfully
     * @note Thread-safe
     */
    bool load(const std::string& filepath);
    
    /**
     * @brief Get current file path
     * @return File path or empty string
     */
    std::string getCurrentFile() const;
    
    /**
     * @brief Check if file is loaded
     * @return true if file loaded
     */
    bool hasFile() const;
    
    // ========================================================================
    // PLAYBACK CONTROL
    // ========================================================================
    
    /**
     * @brief Start playback
     * @return true if started
     * @note Thread-safe
     */
    bool play();
    
    /**
     * @brief Pause playback
     * @return true if paused
     * @note Thread-safe
     */
    bool pause();
    
    /**
     * @brief Stop playback
     * @note Thread-safe
     */
    void stop();
    
    /**
     * @brief Get current state
     * @return Player state
     */
    PlayerState getState() const;
    
    /**
     * @brief Check if playing
     * @return true if playing
     */
    bool isPlaying() const { return getState() == PlayerState::PLAYING; }
    
    // ========================================================================
    // POSITION CONTROL
    // ========================================================================
    
    /**
     * @brief Seek to time position
     * @param timeMs Time in milliseconds
     * @note Thread-safe
     */
    void seek(uint64_t timeMs);
    
    /**
     * @brief Seek to tick position
     * @param tick Tick number
     * @note Thread-safe
     */
    void seekToTick(uint64_t tick);
    
    /**
     * @brief Seek to bar/beat position
     * @param bar Bar number (1-based)
     * @param beat Beat in bar (1-based, default 1)
     * @param tick Tick within beat (default 0)
     * @return true if seeked successfully
     * @note Thread-safe
     */
    bool seekToBar(uint32_t bar, uint8_t beat = 1, uint16_t tick = 0);
    
    /**
     * @brief Get current position in milliseconds
     * @return Position in ms
     */
    uint64_t getCurrentPosition() const;
    
    /**
     * @brief Get current tick
     * @return Current tick
     */
    uint64_t getCurrentTick() const;
    
    /**
     * @brief Get musical position
     * @return Musical position (bar:beat:tick)
     */
    MusicalPosition getMusicalPosition() const;
    
    /**
     * @brief Get duration in milliseconds
     * @return Duration in ms
     */
    uint64_t getDuration() const;
    
    // ========================================================================
    // PLAYBACK PARAMETERS
    // ========================================================================
    
    /**
     * @brief Set tempo
     * @param bpm Beats per minute (50.0 - 300.0)
     * @note Thread-safe
     */
    void setTempo(double bpm);
    
    /**
     * @brief Get current tempo
     * @return BPM
     */
    double getTempo() const;
    
    /**
     * @brief Set loop mode
     * @param enabled Enable loop
     * @note Thread-safe
     */
    void setLoop(bool enabled);
    
    /**
     * @brief Get loop state
     * @return true if looping
     */
    bool isLooping() const;
    
    /**
     * @brief Set master volume
     * @param volume Volume (0.0 - 1.0)
     * @note Thread-safe
     */
    void setVolume(float volume);
    
    /**
     * @brief Get master volume
     * @return Volume (0.0 - 1.0)
     */
    float getVolume() const;
    
    /**
     * @brief Set transposition
     * @param semitones Semitones to transpose (-12 to +12)
     * @note Thread-safe
     */
    void setTranspose(int semitones);
    
    /**
     * @brief Get transposition
     * @return Semitones
     */
    int getTranspose() const;
    
    // ========================================================================
    // TRACK CONTROL
    // ========================================================================
    
    /**
     * @brief Set track mute
     * @param trackIndex Track index (0-based)
     * @param muted Mute state
     * @note Thread-safe
     */
    void setTrackMute(uint16_t trackIndex, bool muted);
    
    /**
     * @brief Set track solo
     * @param trackIndex Track index (0-based)
     * @param solo Solo state
     * @note Thread-safe
     */
    void setTrackSolo(uint16_t trackIndex, bool solo);
    
    /**
     * @brief Get track info
     * @param trackIndex Track index
     * @return Track information or nullptr
     */
    const TrackInfo* getTrackInfo(uint16_t trackIndex) const;
    
    /**
     * @brief Get all tracks info
     * @return Vector of track info
     */
    std::vector<TrackInfo> getTracksInfo() const;
    
    // ========================================================================
    // METADATA
    // ========================================================================
    
    /**
     * @brief Get file metadata
     * @return JSON metadata
     */
    json getMetadata() const;
    
    /**
     * @brief Set state callback
     * @param callback Callback function
     */
    void setStateCallback(StateCallback callback);

private:
    // ========================================================================
    // PRIVATE METHODS - LOADING
    // ========================================================================
    
    /**
     * @brief Parse all tracks from MidiFile
     */
    void parseAllTracks();
    
    /**
     * @brief Extract metadata from file
     */
    void extractMetadata();
    
    /**
     * @brief Analyze track for metadata
     */
    void analyzeTrack(uint16_t trackIndex);
    
    /**
     * @brief Calculate total duration
     */
    void calculateDuration();
    
    // ========================================================================
    // PRIVATE METHODS - PLAYBACK
    // ========================================================================
    
    /**
     * @brief Main playback loop
     */
    void playbackLoop();
    
    /**
     * @brief Check if event should be played
     */
    bool shouldPlayEvent(const ScheduledEvent& event) const;
    
    /**
     * @brief Apply modifications to message
     */
    MidiMessage applyModifications(const MidiMessage& message, 
                                   uint16_t trackNumber) const;
    
    /**
     * @brief Apply master volume to message
     */
    MidiMessage applyMasterVolume(const MidiMessage& message) const;
    
    /**
     * @brief Send All Notes Off on all channels
     */
    void sendAllNotesOff();
    
    /**
     * @brief Stop playback (internal, without lock)
     */
    void stopPlayback();
    
    /**
     * @brief Convert milliseconds to ticks
     */
    uint64_t msToTicks(uint64_t ms) const;
    
    /**
     * @brief Convert ticks to milliseconds
     */
    uint64_t ticksToMs(uint64_t ticks) const;
    
    /**
     * @brief Convert bar/beat to ticks
     */
    uint64_t musicalPositionToTicks(uint32_t bar, uint8_t beat, 
                                    uint16_t tick) const;
    
    /**
     * @brief Convert ticks to bar/beat
     */
    MusicalPosition ticksToMusicalPosition(uint64_t ticks) const;
    
    // ========================================================================
    // MEMBER VARIABLES - CORE
    // ========================================================================
    
    /// MIDI router
    std::shared_ptr<MidiRouter> router_;
    
    /// Thread safety
    mutable std::mutex mutex_;
    
    /// Playback thread
    std::thread playbackThread_;
    
    /// Player state
    std::atomic<PlayerState> state_;
    
    /// Thread running flag
    std::atomic<bool> running_;
    
    // ========================================================================
    // MEMBER VARIABLES - FILE DATA
    // ========================================================================
    
    /// Current file path
    std::string currentFile_;
    
    /// Parsed MIDI file
    MidiFile midiFile_;
    
    /// All events sorted by time
    std::vector<ScheduledEvent> allEvents_;
    
    /// Track information
    std::vector<TrackInfo> tracks_;
    
    // ========================================================================
    // MEMBER VARIABLES - TIMING
    // ========================================================================
    
    /// Current position in ticks
    std::atomic<uint64_t> currentTick_;
    
    /// Total duration in ticks
    uint64_t totalTicks_;
    
    /// Ticks per quarter note
    uint16_t ticksPerQuarterNote_;
    
    /// Current tempo (BPM)
    std::atomic<double> tempo_;
    
    /// Start time for playback
    std::chrono::high_resolution_clock::time_point startTime_;
    
    // ========================================================================
    // MEMBER VARIABLES - TIME SIGNATURE
    // ========================================================================
    
    /// Time signature numerator
    uint8_t timeSignatureNum_;
    
    /// Time signature denominator
    uint8_t timeSignatureDen_;
    
    /// Ticks per beat
    uint32_t ticksPerBeat_;
    
    // ========================================================================
    // MEMBER VARIABLES - PLAYBACK SETTINGS
    // ========================================================================
    
    /// Loop enabled
    std::atomic<bool> loopEnabled_;
    
    /// Transposition in semitones
    std::atomic<int> transpose_;
    
    /// Master volume (0.0 - 1.0)
    std::atomic<float> masterVolume_;
    
    /// State change callback
    StateCallback stateCallback_;
};

} // namespace midiMind
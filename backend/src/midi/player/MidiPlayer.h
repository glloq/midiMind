// ============================================================================
// File: backend/src/midi/player/MidiPlayer.h
// Version: 4.2.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.0:
//   - Added EventBus integration
//   - Added publishStateChange() method
//   - Modified constructor to accept EventBus
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
#include <memory>

using json = nlohmann::json;

namespace midiMind {

// Forward declarations
class EventBus;

// ============================================================================
// ENUMS
// ============================================================================

enum class PlayerState {
    STOPPED,
    PLAYING,
    PAUSED
};

// ============================================================================
// STRUCTURES
// ============================================================================

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

struct MusicalPosition {
    uint32_t bar = 1;
    uint8_t beat = 1;
    uint16_t tick = 0;
    std::string formatted;
};

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

class MidiPlayer {
public:
    using StateCallback = std::function<void(const std::string& newState)>;
    
    // Constructor with EventBus
    MidiPlayer(std::shared_ptr<MidiRouter> router,
               std::shared_ptr<EventBus> eventBus = nullptr);
    
    ~MidiPlayer();
    
    MidiPlayer(const MidiPlayer&) = delete;
    MidiPlayer& operator=(const MidiPlayer&) = delete;
    
    // File loading
    bool load(const std::string& filepath);
    std::string getCurrentFile() const;
    bool hasFile() const;
    
    // Playback control
    bool play();
    bool pause();
    void stop();
    PlayerState getState() const;
    bool isPlaying() const { return getState() == PlayerState::PLAYING; }
    
    // Position control
    void seek(uint64_t timeMs);
    void seekToTick(uint64_t tick);
    bool seekToBar(uint32_t bar, uint8_t beat = 1, uint16_t tick = 0);
    uint64_t getCurrentPosition() const;
    uint64_t getCurrentTick() const;
    MusicalPosition getMusicalPosition() const;
    uint64_t getDuration() const;
    
    // Playback parameters
    void setTempo(double bpm);
    double getTempo() const;
    void setLoop(bool enabled);
    bool isLooping() const;
    void setVolume(float volume);
    float getVolume() const;
    void setTranspose(int semitones);
    int getTranspose() const;
    
    // Track control
    void setTrackMute(uint16_t trackIndex, bool muted);
    void setTrackSolo(uint16_t trackIndex, bool solo);
    const TrackInfo* getTrackInfo(uint16_t trackIndex) const;
    std::vector<TrackInfo> getTracksInfo() const;
    
    // Metadata
    json getMetadata() const;
    void setStateCallback(StateCallback callback);
    
    // EventBus configuration
    void setEventBus(std::shared_ptr<EventBus> eventBus);

private:
    void parseAllTracks();
    void extractMetadata();
    void analyzeTrack(uint16_t trackIndex);
    void calculateDuration();
    
    void playbackLoop();
    bool shouldPlayEvent(const ScheduledEvent& event) const;
    MidiMessage applyModifications(const MidiMessage& message, uint16_t trackNumber) const;
    MidiMessage applyMasterVolume(const MidiMessage& message) const;
    void sendAllNotesOff();
    void stopPlayback();
    
    uint64_t msToTicks(uint64_t ms) const;
    uint64_t ticksToMs(uint64_t ticks) const;
    uint64_t musicalPositionToTicks(uint32_t bar, uint8_t beat, uint16_t tick) const;
    MusicalPosition ticksToMusicalPosition(uint64_t ticks) const;
    
    // EventBus helper
    void publishStateChange(PlayerState newState);
    
    // Core members
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<EventBus> eventBus_;
    mutable std::mutex mutex_;
    std::thread playbackThread_;
    std::atomic<PlayerState> state_;
    std::atomic<bool> running_;
    
    // File data
    std::string currentFile_;
    MidiFile midiFile_;
    std::vector<ScheduledEvent> allEvents_;
    std::vector<TrackInfo> tracks_;
    
    // Timing
    std::atomic<uint64_t> currentTick_;
    uint64_t totalTicks_;
    uint16_t ticksPerQuarterNote_;
    std::atomic<double> tempo_;
    std::chrono::high_resolution_clock::time_point startTime_;
    
    // Time signature
    uint8_t timeSignatureNum_;
    uint8_t timeSignatureDen_;
    uint32_t ticksPerBeat_;
    
    // Playback settings
    std::atomic<bool> loopEnabled_;
    std::atomic<int> transpose_;
    std::atomic<float> masterVolume_;
    StateCallback stateCallback_;
};

} // namespace midiMind
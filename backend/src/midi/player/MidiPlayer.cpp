// ============================================================================
// File: backend/src/midi/player/MidiPlayer.cpp
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Complete implementation of MIDI player (compilation fixes)
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// Changes v4.1.1:
//   - Fixed MidiMessage initialization (no setters available)
//   - Use proper MidiMessage constructor with status byte
//
// ============================================================================

#include "MidiPlayer.h"
#include "../../core/Logger.h"
#include <algorithm>
#include <cmath>

namespace midiMind {

// ============================================================================
// GM INSTRUMENT NAMES
// ============================================================================

static const char* GM_INSTRUMENTS[128] = {
    "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano",
    "Honky-tonk Piano", "Electric Piano 1", "Electric Piano 2", "Harpsichord",
    "Clavi", "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba",
    "Xylophone", "Tubular Bells", "Dulcimer", "Drawbar Organ", "Percussive Organ",
    "Rock Organ", "Church Organ", "Reed Organ", "Accordion", "Harmonica",
    "Tango Accordion", "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)",
    "Electric Guitar (jazz)", "Electric Guitar (clean)", "Electric Guitar (muted)",
    "Overdriven Guitar", "Distortion Guitar", "Guitar harmonics", "Acoustic Bass",
    "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
    "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2", "Violin",
    "Viola", "Cello", "Contrabass", "Tremolo Strings", "Pizzicato Strings",
    "Orchestral Harp", "Timpani", "String Ensemble 1", "String Ensemble 2",
    "SynthStrings 1", "SynthStrings 2", "Choir Aahs", "Voice Oohs",
    "Synth Voice", "Orchestra Hit", "Trumpet", "Trombone", "Tuba",
    "Muted Trumpet", "French Horn", "Brass Section", "SynthBrass 1",
    "SynthBrass 2", "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
    "Oboe", "English Horn", "Bassoon", "Clarinet", "Piccolo", "Flute",
    "Recorder", "Pan Flute", "Blown Bottle", "Shakuhachi", "Whistle",
    "Ocarina", "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)",
    "Lead 4 (chiff)", "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)",
    "Lead 8 (bass + lead)", "Pad 1 (new age)", "Pad 2 (warm)",
    "Pad 3 (polysynth)", "Pad 4 (choir)", "Pad 5 (bowed)", "Pad 6 (metallic)",
    "Pad 7 (halo)", "Pad 8 (sweep)", "FX 1 (rain)", "FX 2 (soundtrack)",
    "FX 3 (crystal)", "FX 4 (atmosphere)", "FX 5 (brightness)",
    "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)", "Sitar", "Banjo",
    "Shamisen", "Koto", "Kalimba", "Bag pipe", "Fiddle", "Shanai",
    "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock", "Taiko Drum",
    "Melodic Tom", "Synth Drum", "Reverse Cymbal", "Guitar Fret Noise",
    "Breath Noise", "Seashore", "Bird Tweet", "Telephone Ring", "Helicopter",
    "Applause", "Gunshot"
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

MidiPlayer::MidiPlayer(std::shared_ptr<MidiRouter> router)
    : router_(router)
    , state_(PlayerState::STOPPED)
    , running_(false)
    , currentTick_(0)
    , totalTicks_(0)
    , ticksPerQuarterNote_(480)
    , tempo_(120.0)
    , timeSignatureNum_(4)
    , timeSignatureDen_(4)
    , ticksPerBeat_(480)
    , loopEnabled_(false)
    , transpose_(0)
    , masterVolume_(1.0f)
{
    Logger::info("MidiPlayer", "MidiPlayer initialized");
}

MidiPlayer::~MidiPlayer() {
    stop();
    Logger::info("MidiPlayer", "MidiPlayer destroyed");
}

// ============================================================================
// FILE LOADING
// ============================================================================

bool MidiPlayer::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiPlayer", "Loading file: " + filepath);
    
    try {
        // Stop if playing
        if (state_ != PlayerState::STOPPED) {
            stopPlayback();
        }
        
        // Clear previous data
        currentFile_ = "";
        currentTick_ = 0;
        totalTicks_ = 0;
        tracks_.clear();
        allEvents_.clear();
        
        // Read file
        MidiFileReader reader;
        midiFile_ = reader.readFromFile(filepath);
        
        if (!midiFile_.isValid()) {
            Logger::error("MidiPlayer", "Invalid MIDI file");
            return false;
        }
        
        currentFile_ = filepath;
        ticksPerQuarterNote_ = midiFile_.header.division;
        
        // Parse tracks and build event list
        parseAllTracks();
        
        // Extract metadata
        extractMetadata();
        
        // Calculate duration
        calculateDuration();
        
        Logger::info("MidiPlayer", 
                    "✓ File loaded: " + std::to_string(tracks_.size()) + 
                    " tracks, " + std::to_string(totalTicks_) + " ticks");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MidiPlayer", "Failed to load file: " + std::string(e.what()));
        return false;
    }
}

std::string MidiPlayer::getCurrentFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentFile_;
}

bool MidiPlayer::hasFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !currentFile_.empty() && !allEvents_.empty();
}

// ============================================================================
// PLAYBACK CONTROL
// ============================================================================

bool MidiPlayer::play() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (allEvents_.empty()) {
        Logger::warning("MidiPlayer", "No file loaded");
        return false;
    }
    
    if (state_ == PlayerState::PLAYING) {
        Logger::debug("MidiPlayer", "Already playing");
        return true;
    }
    
    Logger::info("MidiPlayer", "Starting playback");
    
    state_ = PlayerState::PLAYING;
    running_ = true;
    
    // Join previous thread if exists
    if (playbackThread_.joinable()) {
        playbackThread_.join();
    }
    
    // Start playback thread
    playbackThread_ = std::thread(&MidiPlayer::playbackLoop, this);
    
    // Callback
    if (stateCallback_) {
        stateCallback_("playing");
    }
    
    return true;
}

bool MidiPlayer::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != PlayerState::PLAYING) {
        return false;
    }
    
    Logger::info("MidiPlayer", "Pausing playback");
    
    state_ = PlayerState::PAUSED;
    sendAllNotesOff();
    
    // Callback
    if (stateCallback_) {
        stateCallback_("paused");
    }
    
    return true;
}

void MidiPlayer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopPlayback();
}

void MidiPlayer::stopPlayback() {
    if (state_ == PlayerState::STOPPED) {
        return;
    }
    
    Logger::info("MidiPlayer", "Stopping playback");
    
    running_ = false;
    state_ = PlayerState::STOPPED;
    
    // Wait for thread to finish
    if (playbackThread_.joinable()) {
        mutex_.unlock();
        playbackThread_.join();
        mutex_.lock();
    }
    
    sendAllNotesOff();
    currentTick_ = 0;
    
    // Reset processed flags
    for (auto& event : allEvents_) {
        event.processed = false;
    }
    
    // Callback
    if (stateCallback_) {
        stateCallback_("stopped");
    }
}

PlayerState MidiPlayer::getState() const {
    return state_.load();
}

// ============================================================================
// POSITION CONTROL
// ============================================================================

void MidiPlayer::seek(uint64_t timeMs) {
    uint64_t ticks = msToTicks(timeMs);
    seekToTick(ticks);
}

void MidiPlayer::seekToTick(uint64_t tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clamp to valid range
    if (tick > totalTicks_) {
        tick = totalTicks_;
    }
    
    Logger::debug("MidiPlayer", "Seeking to tick: " + std::to_string(tick));
    
    sendAllNotesOff();
    currentTick_ = tick;
    
    // Reset processed flags for events at or after seek position
    for (auto& event : allEvents_) {
        event.processed = (event.tick < tick);
    }
}

bool MidiPlayer::seekToBar(uint32_t bar, uint8_t beat, uint16_t tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate
    if (bar < 1 || beat < 1 || beat > timeSignatureNum_) {
        Logger::warning("MidiPlayer", "Invalid bar/beat position");
        return false;
    }
    
    uint64_t targetTick = musicalPositionToTicks(bar, beat, tick);
    
    if (targetTick > totalTicks_) {
        targetTick = totalTicks_;
    }
    
    sendAllNotesOff();
    currentTick_ = targetTick;
    
    // Reset processed flags
    for (auto& event : allEvents_) {
        event.processed = (event.tick < targetTick);
    }
    
    Logger::info("MidiPlayer", 
                "Seeked to " + std::to_string(bar) + ":" + 
                std::to_string(beat) + ":" + std::to_string(tick));
    
    return true;
}

uint64_t MidiPlayer::getCurrentPosition() const {
    return ticksToMs(currentTick_.load());
}

uint64_t MidiPlayer::getCurrentTick() const {
    return currentTick_.load();
}

MusicalPosition MidiPlayer::getMusicalPosition() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ticksToMusicalPosition(currentTick_);
}

uint64_t MidiPlayer::getDuration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ticksToMs(totalTicks_);
}

// ============================================================================
// PLAYBACK PARAMETERS
// ============================================================================

void MidiPlayer::setTempo(double bpm) {
    if (bpm < 50.0) bpm = 50.0;
    if (bpm > 300.0) bpm = 300.0;
    
    tempo_ = bpm;
    Logger::debug("MidiPlayer", "Tempo set to: " + std::to_string(bpm) + " BPM");
}

double MidiPlayer::getTempo() const {
    return tempo_.load();
}

void MidiPlayer::setLoop(bool enabled) {
    loopEnabled_ = enabled;
    Logger::debug("MidiPlayer", 
                 "Loop " + std::string(enabled ? "enabled" : "disabled"));
}

bool MidiPlayer::isLooping() const {
    return loopEnabled_.load();
}

void MidiPlayer::setVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    
    masterVolume_ = volume;
    Logger::debug("MidiPlayer", "Volume set to: " + std::to_string(volume));
}

float MidiPlayer::getVolume() const {
    return masterVolume_.load();
}

void MidiPlayer::setTranspose(int semitones) {
    if (semitones < -12) semitones = -12;
    if (semitones > 12) semitones = 12;
    
    transpose_ = semitones;
    Logger::debug("MidiPlayer", 
                 "Transpose set to: " + std::to_string(semitones) + " semitones");
}

int MidiPlayer::getTranspose() const {
    return transpose_.load();
}

// ============================================================================
// TRACK CONTROL
// ============================================================================

void MidiPlayer::setTrackMute(uint16_t trackIndex, bool muted) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (trackIndex >= tracks_.size()) {
        return;
    }
    
    tracks_[trackIndex].isMuted = muted;
    
    Logger::debug("MidiPlayer", 
                 "Track " + std::to_string(trackIndex) + " " + 
                 (muted ? "muted" : "unmuted"));
}

void MidiPlayer::setTrackSolo(uint16_t trackIndex, bool solo) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (trackIndex >= tracks_.size()) {
        return;
    }
    
    tracks_[trackIndex].isSolo = solo;
    
    Logger::debug("MidiPlayer", 
                 "Track " + std::to_string(trackIndex) + " " + 
                 (solo ? "soloed" : "unsoloed"));
}

const TrackInfo* MidiPlayer::getTrackInfo(uint16_t trackIndex) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (trackIndex >= tracks_.size()) {
        return nullptr;
    }
    
    return &tracks_[trackIndex];
}

std::vector<TrackInfo> MidiPlayer::getTracksInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tracks_;
}

// ============================================================================
// METADATA
// ============================================================================

json MidiPlayer::getMetadata() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json metadata;
    
    metadata["file"] = currentFile_;
    metadata["format"] = midiFile_.header.format;
    metadata["track_count"] = tracks_.size();
    metadata["division"] = ticksPerQuarterNote_;
    metadata["duration_ticks"] = totalTicks_;
    metadata["duration_ms"] = ticksToMs(totalTicks_);
    metadata["tempo"] = tempo_.load();
    metadata["time_signature"] = {
        {"numerator", timeSignatureNum_},
        {"denominator", timeSignatureDen_}
    };
    
    // Tracks
    json tracksJson = json::array();
    for (const auto& track : tracks_) {
        tracksJson.push_back({
            {"index", track.index},
            {"name", track.name},
            {"channel", track.channel},
            {"program", track.programChange},
            {"instrument", track.instrumentName},
            {"note_count", track.noteCount},
            {"muted", track.isMuted},
            {"solo", track.isSolo}
        });
    }
    metadata["tracks"] = tracksJson;
    
    return metadata;
}

void MidiPlayer::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateCallback_ = callback;
}

// ============================================================================
// PRIVATE METHODS - LOADING
// ============================================================================

void MidiPlayer::parseAllTracks() {
    allEvents_.clear();
    tracks_.clear();
    
    for (size_t i = 0; i < midiFile_.tracks.size(); ++i) {
        const auto& track = midiFile_.tracks[i];
        
        // Create track info
        TrackInfo trackInfo;
        trackInfo.index = static_cast<uint16_t>(i);
        trackInfo.name = track.name;
        trackInfo.channel = track.channel;
        trackInfo.noteCount = track.noteCount;
        
        // Convert events
        for (const auto& event : track.events) {
            if (event.type == MidiEventType::MIDI_CHANNEL) {
                ScheduledEvent scheduled;
                scheduled.tick = event.absoluteTime;
                scheduled.trackNumber = static_cast<uint16_t>(i);
                scheduled.processed = false;
                
                // Create MidiMessage with proper constructor
                // Format: status byte includes channel
                uint8_t statusByte = event.status | event.channel;
                
                if (event.data.size() >= 2) {
                    // Three-byte message
                    scheduled.message = MidiMessage(statusByte, event.data[0], event.data[1]);
                } else if (event.data.size() == 1) {
                    // Two-byte message
                    scheduled.message = MidiMessage(statusByte, event.data[0]);
                } else {
                    // One-byte message (rare)
                    scheduled.message = MidiMessage(statusByte);
                }
                
                scheduled.message.setTimestamp(event.absoluteTime);
                allEvents_.push_back(scheduled);
            }
        }
        
        tracks_.push_back(trackInfo);
    }
    
    // Sort events by tick
    std::sort(allEvents_.begin(), allEvents_.end(),
        [](const ScheduledEvent& a, const ScheduledEvent& b) {
            return a.tick < b.tick;
        });
    
    Logger::debug("MidiPlayer", 
                 "Parsed " + std::to_string(allEvents_.size()) + " events");
}

void MidiPlayer::extractMetadata() {
    // Get tempo and time signature from file
    tempo_ = static_cast<double>(midiFile_.tempo);
    timeSignatureNum_ = midiFile_.timeSignature.numerator;
    timeSignatureDen_ = midiFile_.timeSignature.denominator;
    
    // Calculate ticks per beat
    ticksPerBeat_ = ticksPerQuarterNote_ * 4 / timeSignatureDen_;
    
    // Analyze each track
    for (size_t i = 0; i < tracks_.size(); ++i) {
        analyzeTrack(static_cast<uint16_t>(i));
    }
}

void MidiPlayer::analyzeTrack(uint16_t trackIndex) {
    if (trackIndex >= tracks_.size()) {
        return;
    }
    
    auto& trackInfo = tracks_[trackIndex];
    const auto& track = midiFile_.tracks[trackIndex];
    
    uint32_t totalVelocity = 0;
    uint16_t noteCount = 0;
    
    for (const auto& event : track.events) {
        if (event.type == MidiEventType::MIDI_CHANNEL) {
            uint8_t messageType = event.status & 0xF0;
            
            if (messageType == 0x90 && event.velocity > 0) {
                // Note On
                noteCount++;
                totalVelocity += event.velocity;
                
                trackInfo.minNote = std::min(trackInfo.minNote, event.note);
                trackInfo.maxNote = std::max(trackInfo.maxNote, event.note);
            }
            else if (messageType == 0xC0) {
                // Program Change
                trackInfo.programChange = event.program;
                if (event.program < 128) {
                    trackInfo.instrumentName = GM_INSTRUMENTS[event.program];
                }
            }
        }
    }
    
    if (noteCount > 0) {
        trackInfo.avgVelocity = static_cast<uint8_t>(totalVelocity / noteCount);
    }
}

void MidiPlayer::calculateDuration() {
    if (allEvents_.empty()) {
        totalTicks_ = 0;
        return;
    }
    
    totalTicks_ = allEvents_.back().tick;
    
    Logger::debug("MidiPlayer", 
                 "Duration: " + std::to_string(totalTicks_) + " ticks (" + 
                 std::to_string(ticksToMs(totalTicks_)) + " ms)");
}

// ============================================================================
// PRIVATE METHODS - PLAYBACK
// ============================================================================

void MidiPlayer::playbackLoop() {
    Logger::info("MidiPlayer", "Playback loop started");
    
    startTime_ = std::chrono::high_resolution_clock::now();
    
    while (running_) {
        if (state_ != PlayerState::PLAYING) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Calculate current position
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - startTime_).count();
        
        double tempo = tempo_.load();
        if (tempo <= 0.0) tempo = 120.0;
        
        double ticksPerSecond = (tempo / 60.0) * ticksPerQuarterNote_;
        double secondsElapsed = elapsed / 1000000.0;
        uint64_t newTick = static_cast<uint64_t>(secondsElapsed * ticksPerSecond);
        
        // Update position
        currentTick_ = newTick;
        
        // Process events
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            for (auto& event : allEvents_) {
                if (event.tick > currentTick_) {
                    break;
                }
                
                if (event.processed) {
                    continue;
                }
                
                if (shouldPlayEvent(event)) {
                    MidiMessage msg = applyModifications(event.message, 
                                                        event.trackNumber);
                    msg = applyMasterVolume(msg);
                    
                    if (masterVolume_ > 0.0f && router_) {
                        router_->route(msg);
                    }
                }
                
                event.processed = true;
            }
        }
        
        // Check for end
        bool reachedEnd = (currentTick_ >= totalTicks_);
        bool shouldLoop = loopEnabled_.load();
        
        if (reachedEnd) {
            if (shouldLoop) {
                Logger::debug("MidiPlayer", "Loop restart");
                
                std::lock_guard<std::mutex> lock(mutex_);
                currentTick_ = 0;
                startTime_ = std::chrono::high_resolution_clock::now();
                
                for (auto& event : allEvents_) {
                    event.processed = false;
                }
            } else {
                stop();
                break;
            }
        }
        
        // Sleep briefly
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    Logger::info("MidiPlayer", "Playback loop ended");
}

bool MidiPlayer::shouldPlayEvent(const ScheduledEvent& event) const {
    if (event.trackNumber >= tracks_.size()) {
        return false;
    }
    
    const auto& track = tracks_[event.trackNumber];
    
    if (track.isMuted) {
        return false;
    }
    
    // Check if any track is soloed
    bool anySolo = std::any_of(tracks_.begin(), tracks_.end(),
        [](const TrackInfo& t) { return t.isSolo; });
    
    if (anySolo && !track.isSolo) {
        return false;
    }
    
    return true;
}

MidiMessage MidiPlayer::applyModifications(const MidiMessage& message, 
                                          uint16_t trackNumber) const {
    // Apply transposition to note messages
    int transposeValue = transpose_.load();
    if (transposeValue != 0 && (message.isNoteOn() || message.isNoteOff())) {
        int newNote = static_cast<int>(message.getData1()) + transposeValue;
        newNote = std::clamp(newNote, 0, 127);
        
        // Create new message with transposed note
        uint8_t status = message.getStatus();
        uint8_t velocity = message.getData2();
        return MidiMessage(status, static_cast<uint8_t>(newNote), velocity);
    }
    
    return message;
}

MidiMessage MidiPlayer::applyMasterVolume(const MidiMessage& message) const {
    float volume = masterVolume_.load();
    
    if (message.isNoteOn() || message.isNoteOff()) {
        uint8_t velocity = message.getData2();
        float newVelocity = velocity * volume;
        newVelocity = std::clamp(newVelocity, 0.0f, 127.0f);
        
        // Create new message with adjusted velocity
        uint8_t status = message.getStatus();
        uint8_t note = message.getData1();
        return MidiMessage(status, note, static_cast<uint8_t>(newVelocity));
    }
    else if (message.isControlChange() && message.getData1() == 7) {
        // Volume CC
        uint8_t value = message.getData2();
        float newValue = value * volume;
        newValue = std::clamp(newValue, 0.0f, 127.0f);
        
        // Create new message with adjusted volume
        uint8_t status = message.getStatus();
        return MidiMessage(status, 7, static_cast<uint8_t>(newValue));
    }
    
    return message;
}

void MidiPlayer::sendAllNotesOff() {
    if (!router_) {
        return;
    }
    
    for (int channel = 0; channel < 16; ++channel) {
        // Control Change: All Notes Off (CC 123)
        uint8_t statusByte = 0xB0 | channel;  // Control Change + channel
        MidiMessage msg(statusByte, 123, 0);
        router_->route(msg);
    }
}

// ============================================================================
// PRIVATE METHODS - CONVERSION
// ============================================================================

uint64_t MidiPlayer::msToTicks(uint64_t ms) const {
    double tempo = tempo_.load();
    if (tempo <= 0.0) tempo = 120.0;
    
    double secondsPerBeat = 60.0 / tempo;
    double ticksPerMs = ticksPerQuarterNote_ / (secondsPerBeat * 1000.0);
    
    return static_cast<uint64_t>(ms * ticksPerMs);
}

uint64_t MidiPlayer::ticksToMs(uint64_t ticks) const {
    double tempo = tempo_.load();
    if (tempo <= 0.0) tempo = 120.0;
    
    double secondsPerBeat = 60.0 / tempo;
    double msPerTick = (secondsPerBeat * 1000.0) / ticksPerQuarterNote_;
    
    return static_cast<uint64_t>(ticks * msPerTick);
}

uint64_t MidiPlayer::musicalPositionToTicks(uint32_t bar, uint8_t beat, 
                                            uint16_t tick) const {
    uint64_t ticksPerBar = ticksPerBeat_ * timeSignatureNum_;
    uint64_t ticks = (bar - 1) * ticksPerBar;
    ticks += (beat - 1) * ticksPerBeat_;
    ticks += tick;
    
    return ticks;
}

MusicalPosition MidiPlayer::ticksToMusicalPosition(uint64_t ticks) const {
    MusicalPosition pos;
    
    uint64_t ticksPerBar = ticksPerBeat_ * timeSignatureNum_;
    
    pos.bar = static_cast<uint32_t>(ticks / ticksPerBar) + 1;
    uint64_t ticksInBar = ticks % ticksPerBar;
    
    pos.beat = static_cast<uint8_t>(ticksInBar / ticksPerBeat_) + 1;
    pos.tick = static_cast<uint16_t>(ticksInBar % ticksPerBeat_);
    
    pos.formatted = std::to_string(pos.bar) + ":" + 
                   std::to_string(pos.beat) + ":" + 
                   std::to_string(pos.tick);
    
    return pos;
}

} // namespace midiMind

// ============================================================================
// END OF FILE MidiPlayer.cpp v4.1.1
// ============================================================================
// ============================================================================
// File: backend/src/midi/player/MidiPlayer.cpp
// Version: 4.2.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.0:
//   - Added EventBus integration
//   - Published playback events
//   - Added publishStateChange() implementation
//
// ============================================================================

#include "MidiPlayer.h"
#include "../../core/Logger.h"
#include "../../core/EventBus.h"
#include "../../core/TimeUtils.h"
#include "../../events/Events.h"
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

MidiPlayer::MidiPlayer(std::shared_ptr<MidiRouter> router,
                       std::shared_ptr<EventBus> eventBus)
    : router_(router)
    , eventBus_(eventBus)
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
// EVENT BUS
// ============================================================================

void MidiPlayer::setEventBus(std::shared_ptr<EventBus> eventBus) {
    eventBus_ = eventBus;
    Logger::info("MidiPlayer", "EventBus configured");
}

void MidiPlayer::publishStateChange(PlayerState newState) {
    if (!eventBus_) return;
    
    try {
        events::PlaybackStateChangedEvent::State eventState;
        switch (newState) {
            case PlayerState::PLAYING:
                eventState = events::PlaybackStateChangedEvent::State::PLAYING;
                break;
            case PlayerState::PAUSED:
                eventState = events::PlaybackStateChangedEvent::State::PAUSED;
                break;
            case PlayerState::STOPPED:
            default:
                eventState = events::PlaybackStateChangedEvent::State::STOPPED;
                break;
        }
        
        eventBus_->publish(events::PlaybackStateChangedEvent(
            eventState,
            currentFile_,
            getCurrentPosition(),
            TimeUtils::systemNow()
        ));
        
        Logger::debug("MidiPlayer", "Published PlaybackStateChangedEvent");
    } catch (const std::exception& e) {
        Logger::error("MidiPlayer", 
            "Failed to publish PlaybackStateChangedEvent: " + std::string(e.what()));
    }
}

// ============================================================================
// FILE LOADING
// ============================================================================

bool MidiPlayer::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiPlayer", "Loading file: " + filepath);
    
    try {
        if (state_ != PlayerState::STOPPED) {
            stopPlayback();
        }
        
        currentFile_ = "";
        currentTick_ = 0;
        totalTicks_ = 0;
        tracks_.clear();
        allEvents_.clear();
        
        MidiFileReader reader;
        midiFile_ = reader.readFromFile(filepath);
        
        if (!midiFile_.isValid()) {
            Logger::error("MidiPlayer", "Invalid MIDI file");
            return false;
        }
        
        currentFile_ = filepath;
        ticksPerQuarterNote_ = midiFile_.header.division;
        
        parseAllTracks();
        extractMetadata();
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
    
    if (playbackThread_.joinable()) {
        playbackThread_.join();
    }
    
    playbackThread_ = std::thread(&MidiPlayer::playbackLoop, this);
    
    publishStateChange(state_);
    
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
    
    publishStateChange(state_);
    
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
    
    if (playbackThread_.joinable()) {
        mutex_.unlock();
        playbackThread_.join();
        mutex_.lock();
    }
    
    sendAllNotesOff();
    currentTick_ = 0;
    
    for (auto& event : allEvents_) {
        event.processed = false;
    }
    
    publishStateChange(state_);
    
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
    
    if (tick > totalTicks_) {
        tick = totalTicks_;
    }
    
    Logger::debug("MidiPlayer", "Seeking to tick: " + std::to_string(tick));
    
    sendAllNotesOff();
    currentTick_ = tick;
    
    for (auto& event : allEvents_) {
        event.processed = (event.tick < tick);
    }
}

bool MidiPlayer::seekToBar(uint32_t bar, uint8_t beat, uint16_t tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    
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
    
    json meta = json::object();
    
    meta["format"] = midiFile_.header.format;
    meta["track_count"] = midiFile_.header.numTracks;
    meta["division"] = ticksPerQuarterNote_;
    meta["duration_ms"] = ticksToMs(totalTicks_);
    meta["tempo_bpm"] = tempo_.load();
    meta["time_signature"] = std::to_string(timeSignatureNum_) + "/" + 
                             std::to_string(timeSignatureDen_);
    
    json tracksJson = json::array();
    for (const auto& track : tracks_) {
        json trackJson = {
            {"index", track.index},
            {"name", track.name},
            {"channel", track.channel},
            {"program", track.programChange},
            {"instrument", track.instrumentName},
            {"note_count", track.noteCount},
            {"min_note", track.minNote},
            {"max_note", track.maxNote},
            {"avg_velocity", track.avgVelocity},
            {"is_muted", track.isMuted},
            {"is_solo", track.isSolo}
        };
        tracksJson.push_back(trackJson);
    }
    
    meta["tracks"] = tracksJson;
    
    return meta;
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
    tracks_.resize(midiFile_.tracks.size());
    
    for (size_t trackIdx = 0; trackIdx < midiFile_.tracks.size(); ++trackIdx) {
        auto& track = midiFile_.tracks[trackIdx];
        auto& trackInfo = tracks_[trackIdx];
        trackInfo.index = trackIdx;
        
        uint64_t currentTick = 0;
        
        for (const auto& event : track.events) {
            currentTick += event.deltaTime;
            
            ScheduledEvent sched;
            sched.tick = currentTick;
			// Construire le message MIDI à partir des données de l'événement
			if (event.type == MidiEventType::MIDI_CHANNEL) {
				std::vector<uint8_t> midiData;
				midiData.push_back(event.status | event.channel);
				midiData.insert(midiData.end(), event.data.begin(), event.data.end());
				sched.message = MidiMessage(midiData);
			} else {
				// Pour les meta-events et sysex, créer un message vide ou approprié
				sched.message = MidiMessage();
			}
            sched.trackNumber = trackIdx;
            sched.processed = false;
            
            allEvents_.push_back(sched);
        }
    }
    
    std::sort(allEvents_.begin(), allEvents_.end(),
              [](const ScheduledEvent& a, const ScheduledEvent& b) {
                  return a.tick < b.tick;
              });
}

void MidiPlayer::extractMetadata() {
    for (auto& track : tracks_) {
        analyzeTrack(track.index);
    }
}

void MidiPlayer::analyzeTrack(uint16_t trackIndex) {
    if (trackIndex >= tracks_.size()) return;
    
    auto& track = tracks_[trackIndex];
    auto& midiTrack = midiFile_.tracks[trackIndex];
    
    track.name = "Track " + std::to_string(trackIndex + 1);
    track.channel = 0;
    track.programChange = 0;
    track.noteCount = 0;
    track.minNote = 127;
    track.maxNote = 0;
    track.avgVelocity = 64;
    
    uint32_t totalVelocity = 0;
    
    for (const auto& event : midiTrack.events) {
        MidiMessage msg(event.data);
        uint8_t status = msg.getStatus();
        uint8_t type = status & 0xF0;
        uint8_t channel = status & 0x0F;
        
        if (type == 0x90) {
            track.channel = channel;
            uint8_t note = msg.getData1();
            uint8_t velocity = msg.getData2();
            
            if (velocity > 0) {
                track.noteCount++;
                if (note < track.minNote) track.minNote = note;
                if (note > track.maxNote) track.maxNote = note;
                totalVelocity += velocity;
            }
        }
        else if (type == 0xC0) {
            track.programChange = msg.getData1();
            track.instrumentName = GM_INSTRUMENTS[track.programChange];
        }
    }
    
    if (track.noteCount > 0) {
        track.avgVelocity = totalVelocity / track.noteCount;
    }
}

void MidiPlayer::calculateDuration() {
    totalTicks_ = 0;
    
    for (const auto& event : allEvents_) {
        if (event.tick > totalTicks_) {
            totalTicks_ = event.tick;
        }
    }
}

// ============================================================================
// PRIVATE METHODS - PLAYBACK
// ============================================================================

void MidiPlayer::playbackLoop() {
    Logger::info("MidiPlayer", "Playback thread started");
    
    uint64_t tickCounter = 0;
    startTime_ = std::chrono::high_resolution_clock::now();
    
    while (running_) {
        if (state_ != PlayerState::PLAYING) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - startTime_).count();
        
        double currentTempo = tempo_.load();
        double microsecondsPerTick = (60.0 / currentTempo) * 1000000.0 / ticksPerQuarterNote_;
        uint64_t targetTick = static_cast<uint64_t>(elapsed / microsecondsPerTick);
        
        currentTick_ = targetTick;
        
        for (auto& event : allEvents_) {
            if (!running_ || state_ != PlayerState::PLAYING) break;
            
            if (event.tick <= targetTick && !event.processed) {
                if (shouldPlayEvent(event)) {
                    auto modifiedMsg = applyModifications(event.message, event.trackNumber);
                    modifiedMsg = applyMasterVolume(modifiedMsg);
                    
                    if (router_) {
                        router_->route(modifiedMsg);
                    }
                }
                event.processed = true;
            }
        }
        
        // Publish progress every 10 iterations
        if (eventBus_ && (tickCounter % 10 == 0)) {
            try {
                double position = getCurrentPosition();
                double duration = getDuration();
                double percentage = (duration > 0) ? (position / duration * 100.0) : 0.0;
                
                eventBus_->publish(events::PlaybackProgressEvent(
                    position,
                    duration,
                    percentage,
                    TimeUtils::systemNow()
                ));
            } catch (const std::exception&) {
                // Silent for progress events
            }
        }
        tickCounter++;
        
        if (currentTick_ >= totalTicks_) {
            if (loopEnabled_) {
                currentTick_ = 0;
                for (auto& event : allEvents_) {
                    event.processed = false;
                }
                startTime_ = std::chrono::high_resolution_clock::now();
            } else {
                break;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    Logger::info("MidiPlayer", "Playback thread stopped");
}

bool MidiPlayer::shouldPlayEvent(const ScheduledEvent& event) const {
    if (event.trackNumber >= tracks_.size()) {
        return true;
    }
    
    const auto& track = tracks_[event.trackNumber];
    
    if (track.isMuted) {
        return false;
    }
    
    bool anySolo = std::any_of(tracks_.begin(), tracks_.end(),
                                [](const TrackInfo& t) { return t.isSolo; });
    
    if (anySolo && !track.isSolo) {
        return false;
    }
    
    return true;
}

MidiMessage MidiPlayer::applyModifications(const MidiMessage& message, 
                                           uint16_t trackNumber) const {
    uint8_t status = message.getStatus();
    uint8_t type = status & 0xF0;
    
    if (type != 0x90 && type != 0x80) {
        return message;
    }
    
    uint8_t note = message.getData1();
    uint8_t velocity = message.getData2();
    
    int trans = transpose_.load();
    int newNote = static_cast<int>(note) + trans;
    
    if (newNote < 0) newNote = 0;
    if (newNote > 127) newNote = 127;
    
    return MidiMessage(status, static_cast<uint8_t>(newNote), velocity);
}

MidiMessage MidiPlayer::applyMasterVolume(const MidiMessage& message) const {
    uint8_t status = message.getStatus();
    uint8_t type = status & 0xF0;
    
    if (type != 0x90) {
        return message;
    }
    
    uint8_t note = message.getData1();
    uint8_t velocity = message.getData2();
    
    float volume = masterVolume_.load();
    uint8_t newVelocity = static_cast<uint8_t>(velocity * volume);
    
    return MidiMessage(status, note, newVelocity);
}

void MidiPlayer::sendAllNotesOff() {
    if (!router_) return;
    
    for (uint8_t channel = 0; channel < 16; ++channel) {
        uint8_t status = 0xB0 | channel;
        router_->route(MidiMessage(status, 123, 0));
    }
}

// ============================================================================
// PRIVATE METHODS - TIME CONVERSION
// ============================================================================

uint64_t MidiPlayer::msToTicks(uint64_t ms) const {
    double currentTempo = tempo_.load();
    double ticksPerMs = (currentTempo * ticksPerQuarterNote_) / 60000.0;
    return static_cast<uint64_t>(ms * ticksPerMs);
}

uint64_t MidiPlayer::ticksToMs(uint64_t ticks) const {
    double currentTempo = tempo_.load();
    double msPerTick = 60000.0 / (currentTempo * ticksPerQuarterNote_);
    return static_cast<uint64_t>(ticks * msPerTick);
}

uint64_t MidiPlayer::musicalPositionToTicks(uint32_t bar, uint8_t beat, 
                                             uint16_t tick) const {
    uint64_t ticks = 0;
    
    ticks += (bar - 1) * timeSignatureNum_ * ticksPerBeat_;
    ticks += (beat - 1) * ticksPerBeat_;
    ticks += tick;
    
    return ticks;
}

MusicalPosition MidiPlayer::ticksToMusicalPosition(uint64_t ticks) const {
    MusicalPosition pos;
    
    uint64_t ticksPerBar = timeSignatureNum_ * ticksPerBeat_;
    
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
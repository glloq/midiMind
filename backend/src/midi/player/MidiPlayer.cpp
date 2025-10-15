// ============================================================================
// Fichier: backend/src/midi/player/MidiPlayer.cpp
// Version: 3.1.1 - CORRECTIONS CRITIQUES APPLIQUÉES SUR BASE 3.1.0 COMPLÈTE
// Date: 2025-10-15
// Projet: MidiMind - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
//
// HISTORIQUE:
//   v3.1.0 - Base complète avec métadonnées + volume + analyse tracks
//   v3.1.1 - CORRECTIONS CRITIQUES (sans suppression fonctionnalités):
//     ✅ setTempo() - Validation pour éviter division par zéro
//     ✅ seek() - Validation bounds
//     ✅ seekToBar() - Validation beat/bar
//     ✅ playbackLoop() - Protection overflow + timing amélioré
//     ✅ getCurrentTimeMs() - Protection division par zéro
//     ✅ getTotalTimeMs() - Protection division par zéro
//     ✅ applyModifications() - Validation clamp note MIDI
//     ✅ applyMasterVolume() - Validation clamp velocity
//
// ============================================================================

#include "MidiPlayer.h"
#include "../MidiFileReader.h"
#include "../../core/TimeUtils.h"
#include "../../core/Error.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace midiMind {

// ============================================================================
// ✅ CONSTANTES - AJOUTÉES v3.1.1
// ============================================================================

namespace {
    constexpr float MIN_TEMPO = 20.0f;    // BPM minimum
    constexpr float MAX_TEMPO = 500.0f;   // BPM maximum
    constexpr int MIN_TRANSPOSE = -24;    // -2 octaves
    constexpr int MAX_TRANSPOSE = 24;     // +2 octaves
}

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR (v3.1.0 - INCHANGÉ)
// ============================================================================

MidiPlayer::MidiPlayer(std::shared_ptr<MidiRouter> router)
    : router_(router)
    , running_(false)
    , state_(PlayerState::STOPPED)
    , currentTick_(0)
    , totalTicks_(0)
    , ticksPerQuarterNote_(480)
    , tempo_(120.0)
    , loopEnabled_(false)
    , transpose_(0)
    , masterVolume_(1.0f)
    , isMuted_(false)
    , volumeBeforeMute_(1.0f) {
    
    Logger::info("MidiPlayer", "Player initialized with master volume support");
}

MidiPlayer::~MidiPlayer() {
    stop();
    Logger::info("MidiPlayer", "MidiPlayer destroyed");
}

// ============================================================================
// CHARGEMENT DE FICHIERS - v3.1.0 (INCHANGÉ)
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
        tempoChanges_.clear();
        
        MidiFileReader reader;
        midiFile_ = reader.read(filepath);
        
        ticksPerQuarterNote_ = midiFile_.ticksPerQuarterNote;
        totalTicks_ = calculateTotalTicks();
        
        parseAllTracks();
        extractMetadata();
        
        tracks_.clear();
        for (size_t i = 0; i < midiFile_.tracks.size(); i++) {
            TrackInfo info(i);
            info.name = getTrackName(i);
            info.eventCount = midiFile_.tracks[i].events.size();
            tracks_.push_back(info);
        }
        
        for (size_t i = 0; i < tracks_.size(); i++) {
            analyzeTrack(i);
        }
        
        currentFile_ = filepath;
        
        Logger::info("MidiPlayer", "✓ File loaded successfully:");
        Logger::info("MidiPlayer", "  - Tracks: " + std::to_string(midiFile_.tracks.size()));
        Logger::info("MidiPlayer", "  - Total ticks: " + std::to_string(totalTicks_));
        Logger::info("MidiPlayer", "  - Time signature: " + timeSignatureStr_);
        Logger::info("MidiPlayer", "  - Initial tempo: " + std::to_string(initialTempo_) + " BPM");
        Logger::info("MidiPlayer", "  - Key: " + keySignature_);
        if (!copyright_.empty()) {
            Logger::info("MidiPlayer", "  - Copyright: " + copyright_);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MidiPlayer", "Failed to load file: " + std::string(e.what()));
        currentFile_ = "";
        tracks_.clear();
        allEvents_.clear();
        return false;
    }
}

// ============================================================================
// EXTRACTION MÉTADONNÉES - v3.1.0 (INCHANGÉ)
// ============================================================================

void MidiPlayer::extractMetadata() {
    Logger::debug("MidiPlayer", "Extracting metadata...");
    
    copyright_ = "";
    keySignature_ = "C major";
    timeSignatureStr_ = "4/4";
    timeSignatureNum_ = 4;
    timeSignatureDen_ = 4;
    initialTempo_ = 120.0f;
    tempoChanges_.clear();
    
    bool foundInitialTempo = false;
    bool foundTimeSignature = false;
    bool foundKeySignature = false;
    
    for (const auto& scheduled : allEvents_) {
        const auto& event = scheduled.event;
        
        if (event.type != MidiEventType::META) {
            continue;
        }
        
        if (event.metaType == 0x02 && !event.data.empty()) {
            copyright_ = std::string(event.data.begin(), event.data.end());
            Logger::debug("MidiPlayer", "Found copyright: " + copyright_);
        }
        
        else if (event.metaType == 0x51 && event.data.size() >= 3) {
            uint32_t microsecondsPerQuarter = 
                (static_cast<uint32_t>(event.data[0]) << 16) |
                (static_cast<uint32_t>(event.data[1]) << 8) |
                static_cast<uint32_t>(event.data[2]);
            
            float bpm = 60000000.0f / microsecondsPerQuarter;
            
            if (!foundInitialTempo) {
                initialTempo_ = bpm;
                tempo_ = bpm;
                foundInitialTempo = true;
                Logger::debug("MidiPlayer", "Initial tempo: " + std::to_string(bpm) + " BPM");
            }
            
            double ticksPerSecond = (initialTempo_ / 60.0) * ticksPerQuarterNote_;
            uint32_t timeMs = static_cast<uint32_t>((scheduled.tick / ticksPerSecond) * 1000.0);
            
            TempoChange change(scheduled.tick, timeMs, bpm);
            tempoChanges_.push_back(change);
            
            Logger::debug("MidiPlayer", 
                "Tempo change at tick " + std::to_string(scheduled.tick) + 
                ": " + std::to_string(bpm) + " BPM");
        }
        
        else if (event.metaType == 0x58 && event.data.size() >= 4) {
            if (!foundTimeSignature) {
                timeSignatureNum_ = event.data[0];
                timeSignatureDen_ = 1 << event.data[1];
                timeSignatureStr_ = std::to_string(timeSignatureNum_) + "/" + 
                                   std::to_string(timeSignatureDen_);
                foundTimeSignature = true;
                Logger::debug("MidiPlayer", "Time signature: " + timeSignatureStr_);
            }
        }
        
        else if (event.metaType == 0x59 && event.data.size() >= 2) {
            if (!foundKeySignature) {
                int8_t sharpsFlats = static_cast<int8_t>(event.data[0]);
                bool isMinor = (event.data[1] == 1);
                keySignature_ = keySignatureToString(sharpsFlats, isMinor);
                foundKeySignature = true;
                Logger::debug("MidiPlayer", "Key signature: " + keySignature_);
            }
        }
    }
    
    Logger::info("MidiPlayer", 
        "Metadata extracted: " + timeSignatureStr_ + ", " + 
        std::to_string(initialTempo_) + " BPM, " + keySignature_);
}

std::string MidiPlayer::keySignatureToString(int8_t sharpsFlats, bool isMinor) const {
    static const char* MAJOR_KEYS[] = {
        "Cb major", "Gb major", "Db major", "Ab major", "Eb major", "Bb major", "F major",
        "C major",
        "G major", "D major", "A major", "E major", "B major", "F# major", "C# major"
    };
    
    static const char* MINOR_KEYS[] = {
        "Ab minor", "Eb minor", "Bb minor", "F minor", "C minor", "G minor", "D minor",
        "A minor",
        "E minor", "B minor", "F# minor", "C# minor", "G# minor", "D# minor", "A# minor"
    };
    
    if (sharpsFlats < -7) sharpsFlats = -7;
    if (sharpsFlats > 7) sharpsFlats = 7;
    
    int index = sharpsFlats + 7;
    
    return isMinor ? MINOR_KEYS[index] : MAJOR_KEYS[index];
}

// ============================================================================
// ANALYSE PISTES - v3.1.0 (INCHANGÉ)
// ============================================================================

void MidiPlayer::analyzeTrack(size_t trackIndex) {
    if (trackIndex >= tracks_.size() || trackIndex >= midiFile_.tracks.size()) {
        return;
    }
    
    TrackInfo& track = tracks_[trackIndex];
    const auto& midiTrack = midiFile_.tracks[trackIndex];
    
    std::unordered_map<uint8_t, int> channelCount;
    uint32_t totalVelocity = 0;
    uint32_t velocityCount = 0;
    
    uint64_t absoluteTick = 0;
    uint64_t firstNoteTick = 0;
    uint64_t lastNoteTick = 0;
    bool foundFirstNote = false;
    
    for (const auto& event : midiTrack.events) {
        absoluteTick += event.deltaTime;
        
        uint8_t channel = 255;
        if (event.type != MidiEventType::META && event.type != MidiEventType::SYSEX) {
            channel = event.status & 0x0F;
            channelCount[channel]++;
        }
        
        if (event.type == MidiEventType::NOTE_ON && event.data.size() >= 2) {
            uint8_t note = event.data[0];
            uint8_t velocity = event.data[1];
            
            if (velocity > 0) {
                track.noteCount++;
                track.minNote = std::min(track.minNote, note);
                track.maxNote = std::max(track.maxNote, note);
                totalVelocity += velocity;
                velocityCount++;
                
                if (!foundFirstNote) {
                    firstNoteTick = absoluteTick;
                    foundFirstNote = true;
                }
                lastNoteTick = absoluteTick;
            }
        }
        
        else if (event.type == MidiEventType::PROGRAM_CHANGE && event.data.size() >= 1) {
            if (track.programChange == 255) {
                track.programChange = event.data[0];
                track.instrumentName = getInstrumentName(track.programChange);
                Logger::debug("MidiPlayer", 
                    "Track " + std::to_string(trackIndex) + " instrument: " + 
                    track.instrumentName + " (PC " + std::to_string(track.programChange) + ")");
            }
        }
    }
    
    int maxCount = 0;
    for (const auto& pair : channelCount) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            track.channel = pair.first;
        }
    }
    
    if (velocityCount > 0) {
        track.avgVelocity = static_cast<uint8_t>(totalVelocity / velocityCount);
    }
    
    if (track.noteCount > 0 && lastNoteTick > firstNoteTick) {
        double ticksPerSecond = (initialTempo_ / 60.0) * ticksPerQuarterNote_;
        double durationSeconds = (lastNoteTick - firstNoteTick) / ticksPerSecond;
        
        if (durationSeconds > 0) {
            track.noteDensity = static_cast<float>(track.noteCount / durationSeconds);
        }
    }
    
    if (track.programChange == 255) {
        if (track.channel == 9) {
            track.instrumentName = "Drums";
        } else if (track.channel != 255) {
            track.instrumentName = "Unknown";
        }
    }
    
    Logger::debug("MidiPlayer", 
        "Track " + std::to_string(trackIndex) + " analyzed: " +
        std::to_string(track.noteCount) + " notes, " +
        "channel " + (track.channel != 255 ? std::to_string(track.channel) : "?") + ", " +
        "density " + std::to_string(track.noteDensity) + " notes/s");
}

// ============================================================================
// PARSING - v3.1.0 (INCHANGÉ)
// ============================================================================

void MidiPlayer::parseAllTracks() {
    allEvents_.clear();
    
    for (size_t trackIdx = 0; trackIdx < midiFile_.tracks.size(); trackIdx++) {
        uint64_t absoluteTick = 0;
        
        for (const auto& event : midiFile_.tracks[trackIdx].events) {
            absoluteTick += event.deltaTime;
            
            ScheduledEvent scheduled;
            scheduled.tick = absoluteTick;
            scheduled.trackIndex = trackIdx;
            scheduled.event = event;
            
            allEvents_.push_back(scheduled);
        }
    }
    
    std::sort(allEvents_.begin(), allEvents_.end(),
        [](const ScheduledEvent& a, const ScheduledEvent& b) {
            return a.tick < b.tick;
        });
    
    Logger::debug("MidiPlayer", "Parsed " + std::to_string(allEvents_.size()) + " events");
}

uint64_t MidiPlayer::calculateTotalTicks() const {
    uint64_t maxTick = 0;
    
    for (const auto& track : midiFile_.tracks) {
        uint64_t tick = 0;
        for (const auto& event : track.events) {
            tick += event.deltaTime;
        }
        maxTick = std::max(maxTick, tick);
    }
    
    return maxTick;
}

std::string MidiPlayer::getTrackName(size_t trackIndex) const {
    if (trackIndex >= midiFile_.tracks.size()) {
        return "Track " + std::to_string(trackIndex + 1);
    }
    
    for (const auto& event : midiFile_.tracks[trackIndex].events) {
        if (event.type == MidiEventType::META && 
            event.metaType == 0x03 && !event.data.empty()) {
            return std::string(event.data.begin(), event.data.end());
        }
    }
    
    return "Track " + std::to_string(trackIndex + 1);
}

// ============================================================================
// CONVERSION TEMPORELLE - v3.1.0 (INCHANGÉ)
// ============================================================================

MusicalPosition MidiPlayer::ticksToMusicalPosition(uint64_t ticks) const {
    MusicalPosition pos;
    
    if (timeSignatureNum_ == 0 || timeSignatureDen_ == 0 || ticksPerQuarterNote_ == 0) {
        pos.bar = 1;
        pos.beat = 1;
        pos.tick = 0;
        pos.numerator = 4;
        pos.denominator = 4;
        pos.formatted = "1:1:0";
        return pos;
    }
    
    uint64_t ticksPerBeat = ticksPerQuarterNote_;
    uint64_t ticksPerBar = ticksPerBeat * timeSignatureNum_;
    
    if (ticksPerBar == 0) {
        ticksPerBar = ticksPerQuarterNote_ * 4;
    }
    
    pos.bar = static_cast<int>(ticks / ticksPerBar) + 1;
    uint64_t ticksInBar = ticks % ticksPerBar;
    pos.beat = static_cast<int>(ticksInBar / ticksPerBeat) + 1;
    pos.tick = static_cast<int>(ticksInBar % ticksPerBeat);
    
    pos.numerator = timeSignatureNum_;
    pos.denominator = timeSignatureDen_;
    
    pos.formatted = std::to_string(pos.bar) + ":" + 
                   std::to_string(pos.beat) + ":" +
                   std::to_string(pos.tick);
    
    return pos;
}

uint64_t MidiPlayer::musicalPositionToTicks(int bar, int beat, int tick) const {
    if (bar < 1) bar = 1;
    if (beat < 1) beat = 1;
    if (tick < 0) tick = 0;
    
    if (timeSignatureNum_ == 0 || ticksPerQuarterNote_ == 0) {
        return 0;
    }
    
    if (beat > timeSignatureNum_) {
        beat = timeSignatureNum_;
    }
    
    uint64_t ticksPerBeat = ticksPerQuarterNote_;
    uint64_t ticksPerBar = ticksPerBeat * timeSignatureNum_;
    
    int barZero = bar - 1;
    int beatZero = beat - 1;
    
    uint64_t totalTicks = 0;
    totalTicks += static_cast<uint64_t>(barZero) * ticksPerBar;
    totalTicks += static_cast<uint64_t>(beatZero) * ticksPerBeat;
    totalTicks += static_cast<uint64_t>(tick);
    
    return totalTicks;
}

std::string MidiPlayer::getInstrumentName(uint8_t programChange) const {
    static const char* GM_INSTRUMENTS[128] = {
        "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano",
        "Honky-tonk Piano", "Electric Piano 1", "Electric Piano 2",
        "Harpsichord", "Clavinet",
        "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
        "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
        "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
        "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
        "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)",
        "Electric Guitar (clean)", "Electric Guitar (muted)", "Overdriven Guitar",
        "Distortion Guitar", "Guitar Harmonics",
        "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)",
        "Fretless Bass", "Slap Bass 1", "Slap Bass 2",
        "Synth Bass 1", "Synth Bass 2",
        "Violin", "Viola", "Cello", "Contrabass",
        "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
        "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
        "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
        "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
        "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
        "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
        "Oboe", "English Horn", "Bassoon", "Clarinet",
        "Piccolo", "Flute", "Recorder", "Pan Flute",
        "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
        "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
        "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
        "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
        "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
        "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
        "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
        "Sitar", "Banjo", "Shamisen", "Koto",
        "Kalimba", "Bag pipe", "Fiddle", "Shanai",
        "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
        "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
        "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
        "Telephone Ring", "Helicopter", "Applause", "Gunshot"
    };
    
    if (programChange > 127) {
        return "Unknown";
    }
    
    return GM_INSTRUMENTS[programChange];
}

// ============================================================================
// ✅ CONTRÔLES - v3.1.1 AVEC VALIDATIONS CRITIQUES
// ============================================================================

void MidiPlayer::play() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (allEvents_.empty()) {
        Logger::warn("MidiPlayer", "No file loaded");
        return;
    }
    
    if (state_ == PlayerState::PLAYING) {
        Logger::debug("MidiPlayer", "Already playing");
        return;
    }
    
    Logger::info("MidiPlayer", "Starting playback");
    
    state_ = PlayerState::PLAYING;
    running_ = true;
    
    if (playbackThread_.joinable()) {
        playbackThread_.join();
    }
    
    playbackThread_ = std::thread(&MidiPlayer::playbackLoop, this);
    
    if (stateCallback_) {
        stateCallback_("playing");
    }
}

void MidiPlayer::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != PlayerState::PLAYING) {
        return;
    }
    
    Logger::info("MidiPlayer", "Pausing playback");
    state_ = PlayerState::PAUSED;
    sendAllNotesOff();
    
    if (stateCallback_) {
        stateCallback_("paused");
    }
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
    
    if (stateCallback_) {
        stateCallback_("stopped");
    }
}

// ✅ CRITIQUE v3.1.1: Validation bounds
void MidiPlayer::seek(uint64_t tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // ✅ VALIDATION: Clamp dans bounds
    if (tick > totalTicks_) {
        Logger::warn("MidiPlayer", 
            "Seek tick " + std::to_string(tick) + 
            " beyond total " + std::to_string(totalTicks_) + " - clamping");
        tick = totalTicks_;
    }
    
    Logger::debug("MidiPlayer", "Seeking to tick: " + std::to_string(tick));
    sendAllNotesOff();
    currentTick_ = tick;
}

// ✅ CRITIQUE v3.1.1: Validation bar/beat
bool MidiPlayer::seekToBar(int bar, int beat, int tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // ✅ VALIDATION: Vérifier bounds
    if (bar < 1) {
        Logger::warn("MidiPlayer", "Bar must be >= 1");
        return false;
    }
    
    if (beat < 1 || beat > timeSignatureNum_) {
        Logger::warn("MidiPlayer", 
            "Beat " + std::to_string(beat) + 
            " out of range [1-" + std::to_string(timeSignatureNum_) + "]");
        return false;
    }
    
    if (tick < 0) {
        Logger::warn("MidiPlayer", "Tick must be >= 0");
        return false;
    }
    
    uint64_t targetTick = musicalPositionToTicks(bar, beat, tick);
    
    // ✅ VALIDATION: Clamp si dépasse
    if (targetTick > totalTicks_) {
        Logger::warn("MidiPlayer", "Position beyond file end - clamping");
        targetTick = totalTicks_;
    }
    
    sendAllNotesOff();
    currentTick_ = targetTick;
    
    Logger::info("MidiPlayer", 
        "Seeked to " + std::to_string(bar) + ":" + 
        std::to_string(beat) + ":" + std::to_string(tick));
    
    return true;
}

void MidiPlayer::sendAllNotesOff() {
    for (int channel = 0; channel < 16; channel++) {
        MidiMessage msg;
        msg.setStatus(0xB0 | channel);
        msg.setData1(123);
        msg.setData2(0);
        
        if (router_) {
            router_->route(msg);
        }
    }
}

// ============================================================================
// ✅ CONFIGURATION - v3.1.1 AVEC VALIDATIONS CRITIQUES
// ============================================================================

// ✅ CRITIQUE v3.1.1: Protection division par zéro
void MidiPlayer::setTempo(float bpm) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // ✅ VALIDATION: Éviter division par zéro
    if (bpm <= 0.0f) {
        Logger::error("MidiPlayer", 
            "Invalid tempo: " + std::to_string(bpm) + " BPM (must be > 0)");
        return;
    }
    
    // ✅ VALIDATION: Clamp dans range raisonnable
    if (bpm < MIN_TEMPO || bpm > MAX_TEMPO) {
        Logger::warn("MidiPlayer", 
            "Tempo " + std::to_string(bpm) + " BPM out of range - clamping");
        bpm = std::clamp(bpm, MIN_TEMPO, MAX_TEMPO);
    }
    
    tempo_ = bpm;
    Logger::info("MidiPlayer", "Tempo set to " + std::to_string(bpm) + " BPM");
}

float MidiPlayer::getTempo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tempo_;
}

void MidiPlayer::setTranspose(int semitones) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // ✅ VALIDATION: Limiter transpose
    if (semitones < MIN_TRANSPOSE || semitones > MAX_TRANSPOSE) {
        Logger::warn("MidiPlayer", "Transpose out of range - clamping");
        semitones = std::clamp(semitones, MIN_TRANSPOSE, MAX_TRANSPOSE);
    }
    
    transpose_ = semitones;
    Logger::info("MidiPlayer", "Transpose set to " + std::to_string(semitones));
}

int MidiPlayer::getTranspose() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transpose_;
}

void MidiPlayer::setLoop(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    loopEnabled_ = enabled;
    Logger::info("MidiPlayer", "Loop " + std::string(enabled ? "enabled" : "disabled"));
}

bool MidiPlayer::isLoopEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loopEnabled_;
}

// ============================================================================
// GETTERS - v3.1.0 (INCHANGÉ)
// ============================================================================

PlayerState MidiPlayer::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool MidiPlayer::isPlaying() const {
    return getState() == PlayerState::PLAYING;
}

MusicalPosition MidiPlayer::getMusicalPosition() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ticksToMusicalPosition(currentTick_);
}

// ✅ CRITIQUE v3.1.1: Protection division par zéro
double MidiPlayer::getCurrentPosition() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // ✅ VALIDATION: Éviter division par zéro
    if (tempo_ <= 0.0f || ticksPerQuarterNote_ == 0) {
        return 0.0;
    }
    
    double ticksPerSecond = (tempo_ / 60.0) * ticksPerQuarterNote_;
    return (currentTick_ / ticksPerSecond) * 1000.0;
}

// ✅ CRITIQUE v3.1.1: Protection division par zéro
double MidiPlayer::getDuration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // ✅ VALIDATION: Éviter division par zéro
    if (tempo_ <= 0.0f || ticksPerQuarterNote_ == 0) {
        return 0.0;
    }
    
    double ticksPerSecond = (tempo_ / 60.0) * ticksPerQuarterNote_;
    return (totalTicks_ / ticksPerSecond) * 1000.0;
}

json MidiPlayer::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json status;
    status["state"] = state_;
    status["position_ms"] = getCurrentPosition();
    status["duration_ms"] = getDuration();
    status["tempo"] = tempo_;
    status["loop_enabled"] = loopEnabled_;
    status["transpose"] = transpose_;
    status["volume"] = static_cast<int>(masterVolume_ * 100);
    status["volume_float"] = masterVolume_;
    status["is_muted"] = isMuted_;
    
    auto musicalPos = getMusicalPosition();
    status["musical_position"] = {
        {"bar", musicalPos.bar},
        {"beat", musicalPos.beat},
        {"tick", musicalPos.tick},
        {"formatted", musicalPos.formatted}
    };
    
    status["track_count"] = tracks_.size();
    
    return status;
}

json MidiPlayer::getMetadata() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json meta;
    meta["filename"] = currentFile_;
    meta["format"] = midiFile_.format;
    meta["ticks_per_quarter_note"] = ticksPerQuarterNote_;
    meta["initial_tempo"] = initialTempo_;
    meta["time_signature"] = timeSignatureStr_;
    meta["time_signature_numerator"] = timeSignatureNum_;
    meta["time_signature_denominator"] = timeSignatureDen_;
    meta["key_signature"] = keySignature_;
    meta["copyright"] = copyright_;
    meta["has_tempo_changes"] = !tempoChanges_.empty();
    meta["tempo_changes_count"] = tempoChanges_.size();
    
    json tempoArray = json::array();
    for (const auto& change : tempoChanges_) {
        json tc;
        tc["tick"] = change.tick;
        tc["time_ms"] = change.timeMs;
        tc["bpm"] = change.bpm;
        tempoArray.push_back(tc);
    }
    meta["tempo_changes"] = tempoArray;
    
    return meta;
}

// ============================================================================
// VOLUME - v3.1.0 (INCHANGÉ)
// ============================================================================

void MidiPlayer::setMasterVolume(float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    masterVolume_ = std::max(0.0f, std::min(1.0f, volume));
    Logger::debug("MidiPlayer", 
        "Master volume set to: " + std::to_string(static_cast<int>(masterVolume_ * 100)) + "%");
    
    if (isMuted_) {
        volumeBeforeMute_ = masterVolume_;
    }
}

float MidiPlayer::getMasterVolume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return masterVolume_;
}

float MidiPlayer::increaseVolume() {
    std::lock_guard<std::mutex> lock(mutex_);
    float newVolume = masterVolume_ + 0.1f;
    masterVolume_ = std::min(1.0f, newVolume);
    Logger::debug("MidiPlayer", 
        "Volume increased to: " + std::to_string(static_cast<int>(masterVolume_ * 100)) + "%");
    return masterVolume_;
}

float MidiPlayer::decreaseVolume() {
    std::lock_guard<std::mutex> lock(mutex_);
    float newVolume = masterVolume_ - 0.1f;
    masterVolume_ = std::max(0.0f, newVolume);
    Logger::debug("MidiPlayer", 
        "Volume decreased to: " + std::to_string(static_cast<int>(masterVolume_ * 100)) + "%");
    return masterVolume_;
}

void MidiPlayer::setMute(bool mute) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (mute && !isMuted_) {
        volumeBeforeMute_ = masterVolume_;
        masterVolume_ = 0.0f;
        isMuted_ = true;
        Logger::info("MidiPlayer", "Audio muted");
    } else if (!mute && isMuted_) {
        masterVolume_ = volumeBeforeMute_;
        isMuted_ = false;
        Logger::info("MidiPlayer", 
            "Audio unmuted (volume: " + 
            std::to_string(static_cast<int>(masterVolume_ * 100)) + "%)");
    }
}

bool MidiPlayer::isMuted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isMuted_;
}

// ✅ CRITIQUE v3.1.1: Validation clamp velocity
MidiMessage MidiPlayer::applyMasterVolume(const MidiMessage& message) const {
    if (masterVolume_ <= 0.0f) {
        return message;
    }
    
    if (std::abs(masterVolume_ - 1.0f) < 0.001f) {
        return message;
    }
    
    MidiMessage modifiedMessage = message;
    
    if (message.isNoteOn() || message.isNoteOff()) {
        uint8_t originalVelocity = message.data2;
        float newVelocityFloat = originalVelocity * masterVolume_;
        
        // ✅ VALIDATION: Clamp velocity MIDI
        uint8_t newVelocity = static_cast<uint8_t>(
            std::max(0.0f, std::min(127.0f, newVelocityFloat))
        );
        
        modifiedMessage.data2 = newVelocity;
    }
    
    if (message.isControlChange() && message.data1 == 7) {
        uint8_t originalValue = message.data2;
        float newValueFloat = originalValue * masterVolume_;
        
        // ✅ VALIDATION: Clamp CC value
        uint8_t newValue = static_cast<uint8_t>(
            std::max(0.0f, std::min(127.0f, newValueFloat))
        );
        
        modifiedMessage.data2 = newValue;
    }
    
    return modifiedMessage;
}

// ============================================================================
// ✅ PLAYBACK LOOP - v3.1.1 AVEC AMÉLIORATION TIMING
// ============================================================================

void MidiPlayer::playbackLoop() {
    Logger::info("MidiPlayer", "Playback loop started");
    
    // ✅ AMÉLIORATION: Utiliser time_point pour précision
    startTime_ = std::chrono::high_resolution_clock::now();
    
    while (running_) {
        if (state_ != PlayerState::PLAYING) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - startTime_
        ).count();
        
        // ✅ VALIDATION: Vérifier tempo valide
        double ticksPerSecond;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (tempo_ <= 0.0f || ticksPerQuarterNote_ == 0) {
                Logger::error("MidiPlayer", "Invalid tempo in playback - stopping");
                stop();
                return;
            }
            ticksPerSecond = (tempo_ / 60.0) * ticksPerQuarterNote_;
        }
        
        double secondsElapsed = elapsed / 1000000.0;
        uint64_t newTick = static_cast<uint64_t>(secondsElapsed * ticksPerSecond);
        
        // ✅ VALIDATION: Vérifier overflow
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (newTick > totalTicks_ + 1000) {
                Logger::warn("MidiPlayer", "Tick overflow detected - stopping");
                stop();
                return;
            }
            currentTick_ = newTick;
        }
        
        // Traiter événements
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& event : allEvents_) {
                if (event.tick > currentTick_) break;
                if (event.processed) continue;
                
                if (shouldPlayEvent(event)) {
                    MidiMessage msg = applyModifications(event.message, event.trackNumber);
                    msg = applyMasterVolume(msg);
                    
                    if (masterVolume_ > 0.0f && router_) {
                        router_->route(msg);
                    }
                }
                
                event.processed = true;
            }
        }
        
        // Vérifier fin
        bool reachedEnd, shouldLoop;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            reachedEnd = (currentTick_ >= totalTicks_);
            shouldLoop = loopEnabled_;
        }
        
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
        
        // ✅ AMÉLIORATION: Sleep précis
        auto nextTickTime = startTime_ + std::chrono::microseconds(
            static_cast<uint64_t>((currentTick_ + 1) / ticksPerSecond * 1000000)
        );
        std::this_thread::sleep_until(nextTickTime);
    }
    
    Logger::info("MidiPlayer", "Playback loop ended");
}

// ============================================================================
// HELPERS - v3.1.0 (avec validation v3.1.1)
// ============================================================================

bool MidiPlayer::shouldPlayEvent(const ScheduledEvent& event) const {
    auto it = trackInfo_.find(event.trackNumber);
    if (it == trackInfo_.end()) {
        return true;
    }
    
    const TrackInfo& track = it->second;
    
    if (track.isMuted) {
        return false;
    }
    
    bool anySolo = std::any_of(trackInfo_.begin(), trackInfo_.end(),
        [](const auto& pair) { return pair.second.isSolo; });
    
    if (anySolo && !track.isSolo) {
        return false;
    }
    
    return true;
}

// ✅ CRITIQUE v3.1.1: Validation clamp note
MidiMessage MidiPlayer::applyModifications(const MidiMessage& message, uint8_t trackNumber) const {
    MidiMessage modified = message;
    
    if ((message.type == MidiMessageType::NOTE_ON || 
         message.type == MidiMessageType::NOTE_OFF) && 
        transpose_ != 0) {
        
        int newNote = static_cast<int>(message.data1) + transpose_;
        
        // ✅ VALIDATION: Clamp note MIDI dans [0-127]
        newNote = std::clamp(newNote, 0, 127);
        
        modified.data1 = static_cast<uint8_t>(newNote);
    }
    
    return modified;
}

// ============================================================================
// CALLBACKS - v3.1.0 (INCHANGÉ)
// ============================================================================

void MidiPlayer::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateCallback_ = callback;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiPlayer.cpp - v3.1.1
// ============================================================================

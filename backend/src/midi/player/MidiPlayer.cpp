// ============================================================================
// Fichier: backend/src/midi/player/MidiPlayer.cpp
// Version: 3.1.0 - PHASES 2 & 3 COMPLETES - Métadonnées + Calculs
// Date: 2025-10-10
// ============================================================================
// Description:
//   Implémentation complète avec extraction métadonnées et calculs bar/beat.
//
// Modifications Phase 2:
//   ✅ extractMetadata() - Extraction complète des meta events
//   ✅ analyzeTrack() - Analyse enrichie des pistes (channel, notes, instrument)
//   ✅ load() modifié pour appeler extractMetadata() et analyzeTrack()
//
// Modifications Phase 3:
//   ✅ ticksToMusicalPosition() - Conversion ticks → bar:beat:tick
//   ✅ musicalPositionToTicks() - Conversion bar:beat:tick → ticks
//   ✅ getInstrumentName() - Mapping Program Change → Nom instrument
//
// Auteur: MidiMind Team
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
// CONSTRUCTEUR / DESTRUCTEUR
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
    , masterVolume_(1.0f)      // ✅ NOUVEAU: Volume par défaut 100%
    , isMuted_(false)          // ✅ NOUVEAU: Non muté par défaut
    , volumeBeforeMute_(1.0f)  // ✅ NOUVEAU
{
    Logger::info("MidiPlayer", "Player initialized with master volume support");
}

MidiPlayer::~MidiPlayer() {
    stop();
    Logger::info("MidiPlayer", "MidiPlayer destroyed");
}

// ============================================================================
// CHARGEMENT DE FICHIERS - PHASE 2 MODIFIÉ
// ============================================================================

bool MidiPlayer::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("MidiPlayer", "Loading file: " + filepath);
    
    try {
        // Arrêter la lecture en cours
        if (state_ != PlayerState::STOPPED) {
            stopPlayback();
        }
        
        // Réinitialiser tout
        currentFile_ = "";
        currentTick_ = 0;
        totalTicks_ = 0;
        tracks_.clear();
        allEvents_.clear();
        tempoChanges_.clear();
        
        // Lire le fichier MIDI
        MidiFileReader reader;
        midiFile_ = reader.read(filepath);
        
        // Extraire informations de base
        ticksPerQuarterNote_ = midiFile_.ticksPerQuarterNote;
        totalTicks_ = calculateTotalTicks();
        
        // Parser les événements de toutes les pistes
        parseAllTracks();
        
        // PHASE 2.1: Extraire métadonnées (tempo, time sig, key, copyright)
        extractMetadata();
        
        // Initialiser infos pistes avec valeurs de base
        tracks_.clear();
        for (size_t i = 0; i < midiFile_.tracks.size(); i++) {
            TrackInfo info(i);
            info.name = getTrackName(i);
            info.eventCount = midiFile_.tracks[i].events.size();
            tracks_.push_back(info);
        }
        
        // PHASE 2.2: Analyser chaque piste (channel, instrument, notes)
        for (size_t i = 0; i < tracks_.size(); i++) {
            analyzeTrack(i);
        }
        
        currentFile_ = filepath;
        
        Logger::info("MidiPlayer", 
            "✓ File loaded successfully:");
        Logger::info("MidiPlayer", 
            "  - Tracks: " + std::to_string(midiFile_.tracks.size()));
        Logger::info("MidiPlayer", 
            "  - Total ticks: " + std::to_string(totalTicks_));
        Logger::info("MidiPlayer", 
            "  - Time signature: " + timeSignatureStr_);
        Logger::info("MidiPlayer", 
            "  - Initial tempo: " + std::to_string(initialTempo_) + " BPM");
        Logger::info("MidiPlayer", 
            "  - Key: " + keySignature_);
        if (!copyright_.empty()) {
            Logger::info("MidiPlayer", "  - Copyright: " + copyright_);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MidiPlayer", 
            "Failed to load file: " + std::string(e.what()));
        
        // Cleanup en cas d'erreur
        currentFile_ = "";
        tracks_.clear();
        allEvents_.clear();
        
        return false;
    }
}

// ============================================================================
// EXTRACTION MÉTADONNÉES - PHASE 2.1
// ============================================================================

/**
 * @brief Extrait toutes les métadonnées du fichier MIDI
 * 
 * @details Parse les meta events pour extraire:
 * - Copyright (meta 0x02)
 * - Set Tempo (meta 0x51) → initialTempo_ + tempoChanges_
 * - Time Signature (meta 0x58) → timeSignatureNum_, Den_, Str_
 * - Key Signature (meta 0x59) → keySignature_
 * 
 * Parcourt allEvents_ car ils sont déjà triés par tick absolu.
 */
void MidiPlayer::extractMetadata() {
    Logger::debug("MidiPlayer", "Extracting metadata...");
    
    // Valeurs par défaut
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
    
    // Parcourir tous les événements pour trouver les meta events
    for (const auto& scheduled : allEvents_) {
        const auto& event = scheduled.event;
        
        if (event.type != MidiEventType::META) {
            continue;
        }
        
        // Copyright (0x02)
        if (event.metaType == 0x02 && !event.data.empty()) {
            copyright_ = std::string(event.data.begin(), event.data.end());
            Logger::debug("MidiPlayer", "Found copyright: " + copyright_);
        }
        
        // Set Tempo (0x51)
        else if (event.metaType == 0x51 && event.data.size() >= 3) {
            // Decoder microseconds per quarter note
            uint32_t microsecondsPerQuarter = 
                (static_cast<uint32_t>(event.data[0]) << 16) |
                (static_cast<uint32_t>(event.data[1]) << 8) |
                static_cast<uint32_t>(event.data[2]);
            
            float bpm = 60000000.0f / microsecondsPerQuarter;
            
            // Premier tempo = initial tempo
            if (!foundInitialTempo) {
                initialTempo_ = bpm;
                tempo_ = bpm;  // Définir aussi le tempo de lecture
                foundInitialTempo = true;
                Logger::debug("MidiPlayer", 
                    "Initial tempo: " + std::to_string(bpm) + " BPM");
            }
            
            // Calculer position en ms pour le tempo change
            double ticksPerSecond = (initialTempo_ / 60.0) * ticksPerQuarterNote_;
            uint32_t timeMs = static_cast<uint32_t>(
                (scheduled.tick / ticksPerSecond) * 1000.0
            );
            
            // Ajouter à la map des changements
            TempoChange change(scheduled.tick, timeMs, bpm);
            tempoChanges_.push_back(change);
            
            Logger::debug("MidiPlayer", 
                "Tempo change at tick " + std::to_string(scheduled.tick) + 
                ": " + std::to_string(bpm) + " BPM");
        }
        
        // Time Signature (0x58)
        else if (event.metaType == 0x58 && event.data.size() >= 4) {
            if (!foundTimeSignature) {
                timeSignatureNum_ = event.data[0];  // Numerator
                timeSignatureDen_ = 1 << event.data[1];  // 2^denominator
                
                // Construire string "4/4"
                timeSignatureStr_ = std::to_string(timeSignatureNum_) + "/" + 
                                   std::to_string(timeSignatureDen_);
                
                foundTimeSignature = true;
                Logger::debug("MidiPlayer", 
                    "Time signature: " + timeSignatureStr_);
            }
        }
        
        // Key Signature (0x59)
        else if (event.metaType == 0x59 && event.data.size() >= 2) {
            if (!foundKeySignature) {
                int8_t sharpsFlats = static_cast<int8_t>(event.data[0]);
                bool isMinor = (event.data[1] == 1);
                
                // Convertir en nom de tonalité
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

/**
 * @brief Convertit key signature MIDI en nom lisible
 * 
 * @param sharpsFlats Nombre de dièses (+) ou bémols (-), range -7 à +7
 * @param isMinor true si mineur, false si majeur
 * @return std::string Nom de la tonalité (ex: "C major", "A minor")
 */
std::string MidiPlayer::keySignatureToString(int8_t sharpsFlats, bool isMinor) const {
    // Tonalités majeures
    static const char* MAJOR_KEYS[] = {
        "Cb major", "Gb major", "Db major", "Ab major", "Eb major", "Bb major", "F major",
        "C major",  // 0
        "G major", "D major", "A major", "E major", "B major", "F# major", "C# major"
    };
    
    // Tonalités mineures
    static const char* MINOR_KEYS[] = {
        "Ab minor", "Eb minor", "Bb minor", "F minor", "C minor", "G minor", "D minor",
        "A minor",  // 0
        "E minor", "B minor", "F# minor", "C# minor", "G# minor", "D# minor", "A# minor"
    };
    
    // Limiter range
    if (sharpsFlats < -7) sharpsFlats = -7;
    if (sharpsFlats > 7) sharpsFlats = 7;
    
    // Index dans table (0 = C major ou A minor)
    int index = sharpsFlats + 7;
    
    if (isMinor) {
        return MINOR_KEYS[index];
    } else {
        return MAJOR_KEYS[index];
    }
}

// ============================================================================
// ANALYSE PISTES - PHASE 2.2
// ============================================================================

/**
 * @brief Analyse enrichie d'une piste
 * 
 * @details Extrait:
 * - Canal MIDI principal (le plus fréquent)
 * - Program Change (premier trouvé)
 * - Statistiques notes (count, min, max, avg velocity)
 * - Note density (notes par seconde)
 * 
 * @param trackIndex Index de la piste à analyser
 */
void MidiPlayer::analyzeTrack(size_t trackIndex) {
    if (trackIndex >= tracks_.size() || trackIndex >= midiFile_.tracks.size()) {
        return;
    }
    
    TrackInfo& track = tracks_[trackIndex];
    const auto& midiTrack = midiFile_.tracks[trackIndex];
    
    // Maps pour compter occurrences
    std::unordered_map<uint8_t, int> channelCount;
    uint32_t totalVelocity = 0;
    uint32_t velocityCount = 0;
    
    // Parcourir les événements de la piste
    uint64_t absoluteTick = 0;
    uint64_t firstNoteTick = 0;
    uint64_t lastNoteTick = 0;
    bool foundFirstNote = false;
    
    for (const auto& event : midiTrack.events) {
        absoluteTick += event.deltaTime;
        
        // Extraire canal si événement channel
        uint8_t channel = 255;  // invalide
        if (event.type != MidiEventType::META && 
            event.type != MidiEventType::SYSEX) {
            channel = event.status & 0x0F;
            channelCount[channel]++;
        }
        
        // Note ON
        if (event.type == MidiEventType::NOTE_ON && event.data.size() >= 2) {
            uint8_t note = event.data[0];
            uint8_t velocity = event.data[1];
            
            // Ignorer Note ON avec velocity 0 (= Note OFF)
            if (velocity > 0) {
                track.noteCount++;
                
                // Min/Max note
                track.minNote = std::min(track.minNote, note);
                track.maxNote = std::max(track.maxNote, note);
                
                // Vélocité moyenne
                totalVelocity += velocity;
                velocityCount++;
                
                // Premier et dernier tick de note
                if (!foundFirstNote) {
                    firstNoteTick = absoluteTick;
                    foundFirstNote = true;
                }
                lastNoteTick = absoluteTick;
            }
        }
        
        // Program Change
        else if (event.type == MidiEventType::PROGRAM_CHANGE && 
                 event.data.size() >= 1) {
            if (track.programChange == 255) {  // Premier program change
                track.programChange = event.data[0];
                track.instrumentName = getInstrumentName(track.programChange);
                
                Logger::debug("MidiPlayer", 
                    "Track " + std::to_string(trackIndex) + " instrument: " + 
                    track.instrumentName + " (PC " + std::to_string(track.programChange) + ")");
            }
        }
    }
    
    // Déterminer canal principal (le plus fréquent)
    int maxCount = 0;
    for (const auto& pair : channelCount) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            track.channel = pair.first;
        }
    }
    
    // Calculer vélocité moyenne
    if (velocityCount > 0) {
        track.avgVelocity = static_cast<uint8_t>(totalVelocity / velocityCount);
    }
    
    // Calculer note density (notes par seconde)
    if (track.noteCount > 0 && lastNoteTick > firstNoteTick) {
        double ticksPerSecond = (initialTempo_ / 60.0) * ticksPerQuarterNote_;
        double durationSeconds = (lastNoteTick - firstNoteTick) / ticksPerSecond;
        
        if (durationSeconds > 0) {
            track.noteDensity = static_cast<float>(track.noteCount / durationSeconds);
        }
    }
    
    // Si pas de program change trouvé, déterminer instrument par défaut selon canal
    if (track.programChange == 255) {
        if (track.channel == 9) {  // Canal 10 (9 en 0-based) = drums
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
// PARSING (code existant conservé)
// ============================================================================

void MidiPlayer::parseAllTracks() {
    allEvents_.clear();
    
    // Fusionner tous les événements avec leur tick absolu
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
    
    // Trier par tick
    std::sort(allEvents_.begin(), allEvents_.end(),
        [](const ScheduledEvent& a, const ScheduledEvent& b) {
            return a.tick < b.tick;
        });
    
    Logger::debug("MidiPlayer", 
        "Parsed " + std::to_string(allEvents_.size()) + " events");
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
    
    // Chercher événement "Track Name" (Meta Event 0x03)
    for (const auto& event : midiFile_.tracks[trackIndex].events) {
        if (event.type == MidiEventType::META && 
            event.metaType == 0x03 && !event.data.empty()) {
            // Extraire le nom depuis data
            return std::string(event.data.begin(), event.data.end());
        }
    }
    
    return "Track " + std::to_string(trackIndex + 1);
}

// ============================================================================
// CONVERSION TEMPORELLE - PHASE 3 (code conservé)
// ============================================================================

MusicalPosition MidiPlayer::ticksToMusicalPosition(uint64_t ticks) const {
    MusicalPosition pos;
    
    // Valeurs par défaut si pas de time signature
    if (timeSignatureNum_ == 0 || timeSignatureDen_ == 0 || ticksPerQuarterNote_ == 0) {
        pos.bar = 1;
        pos.beat = 1;
        pos.tick = 0;
        pos.numerator = 4;
        pos.denominator = 4;
        pos.formatted = "1:1:0";
        return pos;
    }
    
    // Calcul des durées
    uint64_t ticksPerBeat = ticksPerQuarterNote_;
    uint64_t ticksPerBar = ticksPerBeat * timeSignatureNum_;
    
    // Éviter division par zéro
    if (ticksPerBar == 0) {
        ticksPerBar = ticksPerQuarterNote_ * 4;
    }
    
    // Calcul bar, beat, tick
    pos.bar = static_cast<int>(ticks / ticksPerBar) + 1;
    uint64_t ticksInBar = ticks % ticksPerBar;
    pos.beat = static_cast<int>(ticksInBar / ticksPerBeat) + 1;
    pos.tick = static_cast<int>(ticksInBar % ticksPerBeat);
    
    // Métadonnées
    pos.numerator = timeSignatureNum_;
    pos.denominator = timeSignatureDen_;
    
    // Format "bar:beat:tick"
    pos.formatted = std::to_string(pos.bar) + ":" + 
                   std::to_string(pos.beat) + ":" +
                   std::to_string(pos.tick);
    
    return pos;
}

uint64_t MidiPlayer::musicalPositionToTicks(int bar, int beat, int tick) const {
    // Validation
    if (bar < 1) bar = 1;
    if (beat < 1) beat = 1;
    if (tick < 0) tick = 0;
    
    // Valeurs par défaut
    if (timeSignatureNum_ == 0 || ticksPerQuarterNote_ == 0) {
        return 0;
    }
    
    // Limiter beat à la signature
    if (beat > timeSignatureNum_) {
        beat = timeSignatureNum_;
    }
    
    // Calcul des durées
    uint64_t ticksPerBeat = ticksPerQuarterNote_;
    uint64_t ticksPerBar = ticksPerBeat * timeSignatureNum_;
    
    // Conversion en 0-based pour calcul
    int barZero = bar - 1;
    int beatZero = beat - 1;
    
    // Calcul total
    uint64_t totalTicks = 0;
    totalTicks += static_cast<uint64_t>(barZero) * ticksPerBar;
    totalTicks += static_cast<uint64_t>(beatZero) * ticksPerBeat;
    totalTicks += static_cast<uint64_t>(tick);
    
    return totalTicks;
}

std::string MidiPlayer::getInstrumentName(uint8_t programChange) const {
    // Table General MIDI Level 1
    static const char* GM_INSTRUMENTS[128] = {
        // Piano (0-7)
        "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano",
        "Honky-tonk Piano", "Electric Piano 1", "Electric Piano 2",
        "Harpsichord", "Clavinet",
        
        // Chromatic Percussion (8-15)
        "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
        "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
        
        // Organ (16-23)
        "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
        "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
        
        // Guitar (24-31)
        "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)",
        "Electric Guitar (clean)", "Electric Guitar (muted)", "Overdriven Guitar",
        "Distortion Guitar", "Guitar Harmonics",
        
        // Bass (32-39)
        "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)",
        "Fretless Bass", "Slap Bass 1", "Slap Bass 2",
        "Synth Bass 1", "Synth Bass 2",
        
        // Strings (40-47)
        "Violin", "Viola", "Cello", "Contrabass",
        "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
        
        // Ensemble (48-55)
        "String Ensemble 1", "String Ensemble 2", "Synth Strings 1", "Synth Strings 2",
        "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
        
        // Brass (56-63)
        "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
        "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
        
        // Reed (64-71)
        "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
        "Oboe", "English Horn", "Bassoon", "Clarinet",
        
        // Pipe (72-79)
        "Piccolo", "Flute", "Recorder", "Pan Flute",
        "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
        
        // Synth Lead (80-87)
        "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
        "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
        
        // Synth Pad (88-95)
        "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
        "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
        
        // Synth Effects (96-103)
        "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
        "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
        
        // Ethnic (104-111)
        "Sitar", "Banjo", "Shamisen", "Koto",
        "Kalimba", "Bag pipe", "Fiddle", "Shanai",
        
        // Percussive (112-119)
        "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
        "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
        
        // Sound Effects (120-127)
        "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
        "Telephone Ring", "Helicopter", "Applause", "Gunshot"
    };
    
    if (programChange > 127) {
        return "Unknown";
    }
    
    return GM_INSTRUMENTS[programChange];
}

// ============================================================================
// CONTRÔLES DE LECTURE (code existant conservé - tronqué pour taille)
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

void MidiPlayer::seek(uint64_t tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (tick > totalTicks_) {
        tick = totalTicks_;
    }
    
    Logger::debug("MidiPlayer", "Seeking to tick: " + std::to_string(tick));
    sendAllNotesOff();
    currentTick_ = tick;
}

bool MidiPlayer::seekToBar(int bar, int beat, int tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (bar < 1 || beat < 1 || beat > timeSignatureNum_ || tick < 0) {
        Logger::warn("MidiPlayer", "Invalid bar/beat/tick position");
        return false;
    }
    
    uint64_t targetTick = musicalPositionToTicks(bar, beat, tick);
    
    if (targetTick > totalTicks_) {
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
// GETTERS 
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

json MidiPlayer::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json status;
    
    // État de base
    status["state"] = state_;
    status["position_ms"] = getCurrentPosition();
    status["duration_ms"] = getDuration();
    status["tempo"] = tempo_;
    status["loop_enabled"] = loopEnabled_;
    status["transpose"] = transpose_;
    
    // ✅ NOUVEAU: Inclure volume
    status["volume"] = static_cast<int>(masterVolume_ * 100);
    status["volume_float"] = masterVolume_;
    status["is_muted"] = isMuted_;
    
    // Position musicale
    auto musicalPos = getMusicalPosition();
    status["musical_position"] = {
        {"bar", musicalPos.bar},
        {"beat", musicalPos.beat},
        {"tick", musicalPos.tick},
        {"formatted", musicalPos.formatted}
    };
    
    // Tracks
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
    
    // Ajouter la map des tempo changes
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
// ✅ NOUVELLES MÉTHODES - VOLUME CONTROL
// ============================================================================

void MidiPlayer::setMasterVolume(float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clamp entre 0.0 et 1.0
    masterVolume_ = std::max(0.0f, std::min(1.0f, volume));
    
    Logger::debug("MidiPlayer", 
        "Master volume set to: " + std::to_string(static_cast<int>(masterVolume_ * 100)) + "%");
    
    // Si muté, conserver le volume pour unmute ultérieur
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
        // Sauvegarder volume actuel et muter
        volumeBeforeMute_ = masterVolume_;
        masterVolume_ = 0.0f;
        isMuted_ = true;
        
        Logger::info("MidiPlayer", "Audio muted");
        
    } else if (!mute && isMuted_) {
        // Restaurer volume
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

// ============================================================================
// MÉTHODE AUXILIAIRE - APPLIQUER VOLUME À UN MESSAGE
// ============================================================================

MidiMessage MidiPlayer::applyMasterVolume(const MidiMessage& message) const {
    // Si pas de volume ou muté, retourner tel quel
    if (masterVolume_ <= 0.0f) {
        // Message muté - on pourrait retourner un message vide
        // ou simplement ne pas le router
        return message;
    }
    
    // Si volume = 100%, pas de modification
    if (std::abs(masterVolume_ - 1.0f) < 0.001f) {
        return message;
    }
    
    // Copier le message
    MidiMessage modifiedMessage = message;
    
    // Appliquer volume seulement aux notes (Note On/Off)
    if (message.isNoteOn() || message.isNoteOff()) {
        uint8_t originalVelocity = message.data2;
        
        // Calculer nouvelle vélocité
        float newVelocityFloat = originalVelocity * masterVolume_;
        uint8_t newVelocity = static_cast<uint8_t>(
            std::max(0.0f, std::min(127.0f, newVelocityFloat))
        );
        
        // Modifier la vélocité dans le message
        modifiedMessage.data2 = newVelocity;
    }
    
    // On peut aussi appliquer le volume aux Control Changes de volume (CC7)
    if (message.isControlChange() && message.data1 == 7) { // CC7 = Channel Volume
        uint8_t originalValue = message.data2;
        
        float newValueFloat = originalValue * masterVolume_;
        uint8_t newValue = static_cast<uint8_t>(
            std::max(0.0f, std::min(127.0f, newValueFloat))
        );
        
        modifiedMessage.data2 = newValue;
    }
    
    return modifiedMessage;
}




void MidiPlayer::playbackLoop() {
    Logger::info("MidiPlayer", "Playback loop started");
    
    startTime_ = std::chrono::high_resolution_clock::now();
    
    while (running_) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - startTime_
        ).count();
        
        // Calculer tick actuel basé sur le tempo
        double secondsElapsed = elapsed / 1000000.0;
        double ticksPerSecond = (tempo_ / 60.0) * ticksPerQuarterNote_;
        currentTick_ = static_cast<uint64_t>(secondsElapsed * ticksPerSecond);
        
        // Traiter tous les événements jusqu'au tick actuel
        for (auto& event : allEvents_) {
            if (event.tick > currentTick_) {
                break;
            }
            
            if (event.processed) {
                continue;
            }
            
            // Appliquer mute/solo/transpose
            if (shouldPlayEvent(event)) {
                MidiMessage msg = applyModifications(event.message, event.trackNumber);
                
                // ✅ NOUVEAU: Appliquer le volume master
                msg = applyMasterVolume(msg);
                
                // Router le message (seulement si volume > 0)
                if (masterVolume_ > 0.0f) {
                    router_->routeMessage(msg);
                }
            }
            
            event.processed = true;
        }
        
        // Vérifier fin de fichier
        if (currentTick_ >= totalTicks_) {
            if (loopEnabled_) {
                // Réinitialiser pour boucle
                currentTick_ = 0;
                startTime_ = std::chrono::high_resolution_clock::now();
                
                // Réinitialiser flags processed
                for (auto& event : allEvents_) {
                    event.processed = false;
                }
                
                Logger::debug("MidiPlayer", "Loop restart");
            } else {
                // Arrêter
                stop();
                break;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    Logger::info("MidiPlayer", "Playback loop ended");
}


// ============================================================================
// CALLBACKS
// ============================================================================

void MidiPlayer::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateCallback_ = callback;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiPlayer.cpp - Version 3.1.0 Phases 2+3+4 Complete
// ============================================================================

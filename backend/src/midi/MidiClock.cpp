// ============================================================================
// Fichier: src/midi/MidiClock.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "MidiClock.h"
#include "../core/Error.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

MidiClock::MidiClock()
    : state_(ClockState::STOPPED)
    , tempo_(DEFAULT_TEMPO)
    , pulse_(0)
    , stop_(false)
    , totalPulses_(0)
    , totalBeats_(0) {
    
    Logger::info("MidiClock", "MidiClock constructed");
    Logger::info("MidiClock", "  Default tempo: " + std::to_string(DEFAULT_TEMPO) + " BPM");
    Logger::info("MidiClock", "  PPQN: " + std::to_string(PPQN));
}

MidiClock::~MidiClock() {
    stop();
    Logger::info("MidiClock", "MidiClock destroyed");
}

// ============================================================================
// CONTRÔLE
// ============================================================================

void MidiClock::start() {
    if (state_ == ClockState::PLAYING) {
        Logger::warn("MidiClock", "Already playing");
        return;
    }
    
    Logger::info("MidiClock", "Starting MIDI Clock...");
    Logger::info("MidiClock", "  Tempo: " + std::to_string(tempo_.load()) + " BPM");
    
    // Réinitialiser la position
    pulse_ = 0;
    
    // Envoyer Start
    sendMessage(MidiMessage::start());
    
    // Démarrer le thread si pas déjà actif
    if (!clockThread_.joinable()) {
        stop_ = false;
        clockThread_ = std::thread([this]() {
            clockThread();
        });
    }
    
    state_ = ClockState::PLAYING;
    
    Logger::info("MidiClock", "✓ MIDI Clock started");
}

void MidiClock::stop() {
    if (state_ == ClockState::STOPPED) {
        return;
    }
    
    Logger::info("MidiClock", "Stopping MIDI Clock...");
    
    // Envoyer Stop
    sendMessage(MidiMessage::stop());
    
    // Arrêter le thread
    stop_ = true;
    if (clockThread_.joinable()) {
        clockThread_.join();
    }
    
    state_ = ClockState::STOPPED;
    pulse_ = 0;
    
    Logger::info("MidiClock", "✓ MIDI Clock stopped");
}

void MidiClock::pause() {
    if (state_ != ClockState::PLAYING) {
        return;
    }
    
    Logger::info("MidiClock", "Pausing MIDI Clock");
    
    state_ = ClockState::PAUSED;
    
    // Envoyer Stop (pas de message "Pause" en MIDI standard)
    sendMessage(MidiMessage::stop());
}

void MidiClock::resume() {
    if (state_ != ClockState::PAUSED) {
        return;
    }
    
    Logger::info("MidiClock", "Resuming MIDI Clock");
    
    // Envoyer Continue
    sendMessage(MidiMessage::continueMsg());
    
    state_ = ClockState::PLAYING;
}

bool MidiClock::isRunning() const {
    return state_ == ClockState::PLAYING;
}

ClockState MidiClock::getState() const {
    return state_;
}

// ============================================================================
// TEMPO
// ============================================================================

void MidiClock::setTempo(float bpm) {
    // Limiter le tempo
    bpm = std::clamp(bpm, MIN_TEMPO, MAX_TEMPO);
    
    tempo_ = bpm;
    
    Logger::info("MidiClock", "Tempo set to " + std::to_string(bpm) + " BPM");
}

float MidiClock::getTempo() const {
    return tempo_;
}

void MidiClock::adjustTempo(float delta) {
    float newTempo = tempo_.load() + delta;
    setTempo(newTempo);
}

// ============================================================================
// POSITION
// ============================================================================

uint32_t MidiClock::getPulse() const {
    return pulse_;
}

uint32_t MidiClock::getBeat() const {
    return pulse_ / PPQN;
}

void MidiClock::setPulse(uint32_t pulse) {
    pulse_ = pulse;
    Logger::debug("MidiClock", "Position set to pulse " + std::to_string(pulse));
}

void MidiClock::setBeat(uint32_t beat) {
    pulse_ = beat * PPQN;
    Logger::debug("MidiClock", "Position set to beat " + std::to_string(beat));
}

void MidiClock::reset() {
    pulse_ = 0;
    Logger::info("MidiClock", "Position reset");
}

// ============================================================================
// CALLBACKS
// ============================================================================

void MidiClock::setOnPulse(PulseCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onPulse_ = callback;
}

void MidiClock::setOnBeat(BeatCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onBeat_ = callback;
}

void MidiClock::setOnSendMessage(SendMessageCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onSendMessage_ = callback;
}

// ============================================================================
// INFORMATIONS
// ============================================================================

json MidiClock::getStatistics() const {
    json stats;
    
    stats["state"] = state_ == ClockState::PLAYING ? "PLAYING" :
                     state_ == ClockState::PAUSED ? "PAUSED" : "STOPPED";
    stats["tempo"] = tempo_.load();
    stats["pulse"] = pulse_.load();
    stats["beat"] = getBeat();
    stats["total_pulses"] = totalPulses_;
    stats["total_beats"] = totalBeats_;
    stats["pulse_interval_us"] = calculatePulseInterval();
    
    return stats;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void MidiClock::clockThread() {
    Logger::info("MidiClock", "Clock thread started");
    
    // Timestamp du dernier pulse
    uint64_t lastPulseTime = TimeUtils::getCurrentTimestampUs();
    
    while (!stop_) {
        // Attendre jusqu'au prochain pulse
        uint64_t now = TimeUtils::getCurrentTimestampUs();
        uint64_t interval = calculatePulseInterval();
        uint64_t nextPulseTime = lastPulseTime + interval;
        
        if (now < nextPulseTime) {
            // Sleep jusqu'au prochain pulse
            uint64_t sleepTime = nextPulseTime - now;
            TimeUtils::sleepUs(sleepTime);
        }
        
        // Vérifier l'état
        if (state_ != ClockState::PLAYING) {
            TimeUtils::sleepMs(10);
            continue;
        }
        
        // Générer le pulse
        lastPulseTime = TimeUtils::getCurrentTimestampUs();
        uint32_t currentPulse = pulse_++;
        totalPulses_++;
        
        // Envoyer Clock
        sendMessage(MidiMessage::clock());
        
        // Callback de pulse
        if (onPulse_) {
            try {
                onPulse_(currentPulse);
            } catch (const std::exception& e) {
                Logger::error("MidiClock", "Pulse callback exception: " + std::string(e.what()));
            }
        }
        
        // Beat ?
        if (currentPulse % PPQN == 0) {
            uint32_t beat = currentPulse / PPQN;
            totalBeats_++;
            
            // Callback de beat
            if (onBeat_) {
                try {
                    onBeat_(beat);
                } catch (const std::exception& e) {
                    Logger::error("MidiClock", "Beat callback exception: " + std::string(e.what()));
                }
            }
        }
    }
    
    Logger::info("MidiClock", "Clock thread stopped");
}

uint64_t MidiClock::calculatePulseInterval() const {
    // Intervalle entre pulses en microsecondes
    // 
    // Formule: 
    //   1 minute = 60'000'000 µs
    //   1 beat = 60'000'000 / BPM µs
    //   1 pulse = (60'000'000 / BPM) / 24 µs
    //
    // Exemple: 120 BPM
    //   1 beat = 500'000 µs
    //   1 pulse = 20'833 µs
    
    float bpm = tempo_.load();
    return static_cast<uint64_t>(60000000.0f / (bpm * PPQN));
}

void MidiClock::sendMessage(const MidiMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (onSendMessage_) {
        try {
            onSendMessage_(msg);
        } catch (const std::exception& e) {
            Logger::error("MidiClock", "Send message callback exception: " + std::string(e.what()));
        }
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiClock.cpp
// ============================================================================
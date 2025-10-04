// ============================================================================
// src/midi/player/MidiTransportControl.h
// Responsabilité: Contrôle du transport (play/pause/stop/seek)
// ============================================================================

enum class TransportState {
    STOPPED,
    PLAYING,
    PAUSED
};

class MidiTransportControl {
public:
    MidiTransportControl() 
        : state_(TransportState::STOPPED),
          positionMs_(0),
          durationMs_(0) {}
    
    void play() {
        if (state_ == TransportState::STOPPED || state_ == TransportState::PAUSED) {
            state_ = TransportState::PLAYING;
            playStartTime_ = std::chrono::steady_clock::now();
            Logger::debug("Transport", "▶ Playing");
        }
    }
    
    void pause() {
        if (state_ == TransportState::PLAYING) {
            // Sauvegarder la position actuelle
            updatePosition();
            state_ = TransportState::PAUSED;
            Logger::debug("Transport", "⏸ Paused at " + std::to_string(positionMs_) + "ms");
        }
    }
    
    void stop() {
        state_ = TransportState::STOPPED;
        positionMs_ = 0;
        Logger::debug("Transport", "⏹ Stopped");
    }
    
    void seek(uint32_t positionMs) {
        positionMs_ = std::min(positionMs, durationMs_);
        
        if (state_ == TransportState::PLAYING) {
            playStartTime_ = std::chrono::steady_clock::now();
        }
        
        Logger::debug("Transport", "⏩ Seeked to " + std::to_string(positionMs_) + "ms");
    }
    
    void setDuration(uint32_t durationMs) {
        durationMs_ = durationMs;
    }
    
    uint32_t getPosition() {
        if (state_ == TransportState::PLAYING) {
            updatePosition();
        }
        return positionMs_;
    }
    
    uint32_t getDuration() const { return durationMs_; }
    TransportState getState() const { return state_; }
    bool isPlaying() const { return state_ == TransportState::PLAYING; }
    bool isPaused() const { return state_ == TransportState::PAUSED; }
    bool isStopped() const { return state_ == TransportState::STOPPED; }
    
    float getProgressPercent() const {
        if (durationMs_ == 0) return 0.0f;
        return (float)positionMs_ / durationMs_ * 100.0f;
    }

private:
    void updatePosition() {
        if (state_ == TransportState::PLAYING) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - playStartTime_
            );
            
            positionMs_ = std::min(
                positionMs_ + (uint32_t)elapsed.count(),
                durationMs_
            );
            
            playStartTime_ = now;
            
            // Arrêter automatiquement à la fin
            if (positionMs_ >= durationMs_) {
                stop();
            }
        }
    }
    
    TransportState state_;
    uint32_t positionMs_;
    uint32_t durationMs_;
    std::chrono::steady_clock::time_point playStartTime_;
};

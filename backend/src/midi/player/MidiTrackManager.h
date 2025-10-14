// ============================================================================
// src/midi/player/MidiTrackManager.h
// Responsabilité: Gestion des pistes (mute/solo/volume/transpose)
// ============================================================================

struct TrackState {
    uint16_t index;
    bool muted = false;
    bool solo = false;
    float volume = 1.0f;
    int transpose = 0;
    std::string name;
    uint8_t channel = 0;
};

class MidiTrackManager {
public:
    MidiTrackManager() : anySolo_(false) {}
    
    void initialize(uint16_t trackCount) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        tracks_.clear();
        tracks_.reserve(trackCount);
        
        for (uint16_t i = 0; i < trackCount; i++) {
            TrackState track;
            track.index = i;
            track.name = "Track " + std::to_string(i + 1);
            tracks_.push_back(track);
        }
        
        anySolo_ = false;
        
        Logger::info("TrackManager", "Initialized " + std::to_string(trackCount) + " tracks");
    }
    
    void setMute(uint16_t trackIndex, bool mute) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (trackIndex >= tracks_.size()) return;
        
        tracks_[trackIndex].muted = mute;
        
        Logger::debug("TrackManager", 
            "Track " + std::to_string(trackIndex) + " " + 
            (mute ? "muted" : "unmuted"));
    }
    
    void setSolo(uint16_t trackIndex, bool solo) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (trackIndex >= tracks_.size()) return;
        
        tracks_[trackIndex].solo = solo;
        
        // Mettre à jour le flag global
        anySolo_ = false;
        for (const auto& track : tracks_) {
            if (track.solo) {
                anySolo_ = true;
                break;
            }
        }
        
        Logger::debug("TrackManager", 
            "Track " + std::to_string(trackIndex) + " solo " + 
            (solo ? "enabled" : "disabled"));
    }
    
    void setVolume(uint16_t trackIndex, float volume) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (trackIndex >= tracks_.size()) return;
        
        tracks_[trackIndex].volume = std::clamp(volume, 0.0f, 1.0f);
        
        Logger::debug("TrackManager", 
            "Track " + std::to_string(trackIndex) + " volume: " + 
            std::to_string(volume));
    }
    
    void setTranspose(uint16_t trackIndex, int semitones) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (trackIndex >= tracks_.size()) return;
        
        tracks_[trackIndex].transpose = std::clamp(semitones, -12, 12);
        
        Logger::debug("TrackManager", 
            "Track " + std::to_string(trackIndex) + " transpose: " + 
            std::to_string(semitones));
    }
    
    bool shouldPlayTrack(uint16_t trackIndex) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (trackIndex >= tracks_.size()) return false;
        
        const auto& track = tracks_[trackIndex];
        
        // Si muted → ne pas jouer
        if (track.muted) return false;
        
        // Si un solo est actif → jouer seulement les pistes en solo
        if (anySolo_ && !track.solo) return false;
        
        return true;
    }
    
    float getVolume(uint16_t trackIndex) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (trackIndex >= tracks_.size()) return 1.0f;
        return tracks_[trackIndex].volume;
    }
    
    int getTranspose(uint16_t trackIndex) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (trackIndex >= tracks_.size()) return 0;
        return tracks_[trackIndex].transpose;
    }
    
    std::vector<TrackState> getAllTracks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tracks_;
    }
    
    size_t getTrackCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tracks_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<TrackState> tracks_;
    bool anySolo_;
};
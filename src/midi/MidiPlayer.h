// ============================================================================
// src/midi/MidiPlayer.h - VERSION OPTIMISÉE ET THREAD-SAFE - CORRIGÉ
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <midifile/MidiFile.h>
#include "MidiRouter.h"
#include <set>

namespace midiMind {

enum class PlayerState {
    STOPPED,
    PLAYING,
    PAUSED
};

struct TrackState {
    bool muted = false;
    bool solo = false;
    float volume = 1.0f;
    int transposeSemitones = 0;
};

struct TrackPlaybackState {
    size_t currentEventIndex = 0;
    uint32_t lastProcessedMs = 0;
    std::set<uint8_t> activeNotes;
    
    void reset() {
        currentEventIndex = 0;
        lastProcessedMs = 0;
        activeNotes.clear();
    }
};

class MidiPlayer {
public:
    MidiPlayer(std::shared_ptr<MidiRouter> router)
        : router_(router), state_(PlayerState::STOPPED),
          positionMs_(0), tempoMultiplier_(1.0), globalTranspose_(0) {}

    ~MidiPlayer() {
        stop();
    }

    bool loadFile(const std::string& filepath) {
        stop();
        
        try {
            midiFile_.read(filepath);
            
            if (!midiFile_.status()) {
                Logger::error("MidiPlayer", "Failed to read MIDI file: " + filepath);
                return false;
            }
            
            midiFile_.doTimeAnalysis();
            midiFile_.linkNotePairs();
            
            durationMs_ = (uint32_t)(midiFile_.getFileDurationInSeconds() * 1000.0);
            
            // ✅ Thread-safe: Lock pendant modification des états
            std::lock_guard<std::mutex> lock(stateMutex_);
            
            trackStates_.clear();
            trackStates_.resize(midiFile_.getTrackCount());
            
            trackPlayback_.clear();
            trackPlayback_.resize(midiFile_.getTrackCount());
            
            currentFile_ = filepath;
            
            Logger::info("MidiPlayer", 
                "Loaded: " + filepath + 
                " (" + std::to_string(midiFile_.getTrackCount()) + " tracks, " +
                std::to_string(durationMs_) + "ms)");
            
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("MidiPlayer", "Exception loading file: " + std::string(e.what()));
            return false;
        }
    }

    void play() {
        if (state_ == PlayerState::PLAYING) return;
        if (currentFile_.empty()) {
            Logger::warn("MidiPlayer", "No file loaded");
            return;
        }
        
        state_ = PlayerState::PLAYING;
        
        if (!playThread_.joinable()) {
            playThread_ = std::thread(&MidiPlayer::playbackLoop, this);
        }
        
        Logger::info("MidiPlayer", "Started playback");
    }

    void pause() {
        if (state_ != PlayerState::PLAYING) return;
        
        state_ = PlayerState::PAUSED;
        sendAllNotesOffAllTracks();
        
        Logger::info("MidiPlayer", "Paused at " + std::to_string(positionMs_) + "ms");
    }

    void stop() {
        if (state_ == PlayerState::STOPPED) return;
        
        state_ = PlayerState::STOPPED;
        sendAllNotesOffAllTracks();
        
        if (playThread_.joinable()) {
            playThread_.join();
        }
        
        positionMs_ = 0;
        
        // ✅ Thread-safe: Reset des états de playback
        std::lock_guard<std::mutex> lock(stateMutex_);
        for (auto& playback : trackPlayback_) {
            playback.reset();
        }
        
        Logger::info("MidiPlayer", "Stopped");
    }

    void seek(uint32_t positionMs) {
        bool wasPlaying = (state_ == PlayerState::PLAYING);
        
        if (wasPlaying) {
            pause();
        }
        
        positionMs_ = std::min(positionMs, durationMs_);
        
        // ✅ BUG FIX: Vider les notes actives avant repositionnement
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            for (auto& playback : trackPlayback_) {
                playback.activeNotes.clear();
            }
        }
        
        repositionPlayback(positionMs_);
        
        if (wasPlaying) {
            play();
        }
        
        Logger::info("MidiPlayer", "Seeked to " + std::to_string(positionMs_) + "ms");
    }

    void setTempo(float multiplier) {
        tempoMultiplier_ = std::clamp(multiplier, 0.1f, 4.0f);
        Logger::info("MidiPlayer", "Tempo set to " + std::to_string(tempoMultiplier_) + "x");
    }

    void setGlobalTranspose(int semitones) {
        if (state_ == PlayerState::PLAYING) {
            sendAllNotesOffAllTracks();
        }
        
        globalTranspose_ = std::clamp(semitones, -12, 12);
        Logger::info("MidiPlayer", "Global transpose: " + std::to_string(globalTranspose_));
    }

void setTrackMute(int trackIndex, bool mute) {
    bool shouldSendAllNotesOff = false;
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (trackIndex < 0 || trackIndex >= (int)trackStates_.size()) {
            return;
        }
        
        trackStates_[trackIndex].muted = mute;
        
        // ✅ Décider pendant qu'on a le lock
        shouldSendAllNotesOff = (mute && state_ == PlayerState::PLAYING);
    } // ✅ Lock relâché ici
    
    // ✅ Appeler sans lock
    if (shouldSendAllNotesOff) {
        sendAllNotesOffForTrack(trackIndex);
    }
}

void setTrackSolo(int trackIndex, bool solo) {
    std::vector<int> tracksToMute;
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (trackIndex < 0 || trackIndex >= (int)trackStates_.size()) {
            return;
        }
        
        trackStates_[trackIndex].solo = solo;
        
        anySolo_ = false;
        for (const auto& state : trackStates_) {
            if (state.solo) {
                anySolo_ = true;
                break;
            }
        }
        
        if (anySolo_ && state_ == PlayerState::PLAYING) {
            for (int i = 0; i < (int)trackStates_.size(); i++) {
                if (!trackStates_[i].solo) {
                    tracksToMute.push_back(i);
                }
            }
        }
    } 
    for (int track : tracksToMute) {
        sendAllNotesOffForTrack(track);
    }
}

    void setTrackVolume(int trackIndex, float volume) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (trackIndex >= 0 && trackIndex < (int)trackStates_.size()) {
            trackStates_[trackIndex].volume = std::clamp(volume, 0.0f, 1.0f);
        }
    }

    void setTrackTranspose(int trackIndex, int semitones) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (trackIndex >= 0 && trackIndex < (int)trackStates_.size()) {
            trackStates_[trackIndex].transposeSemitones = std::clamp(semitones, -12, 12);
        }
    }

    PlayerState getState() const { return state_; }
    uint32_t getPosition() const { return positionMs_; }
    uint32_t getDuration() const { return durationMs_; }
    float getTempo() const { return tempoMultiplier_; }
    int getGlobalTranspose() const { return globalTranspose_; }
    std::string getCurrentFile() const { return currentFile_; }
    
    std::vector<TrackState> getTrackStates() const {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return trackStates_;
    }

private:
    void playbackLoop() {
        auto startTime = std::chrono::steady_clock::now();
        uint32_t startPositionMs = positionMs_;
        
        // ✅ PERFORMANCE FIX: 200 FPS (5ms) au lieu de 1000 FPS (choix 1A)
        const auto frameDuration = std::chrono::milliseconds(5);
        
        Logger::debug("MidiPlayer", "Playback loop started at 200 FPS (5ms frames)");
        
        while (state_ == PlayerState::PLAYING) {
            auto frameStart = std::chrono::steady_clock::now();
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                frameStart - startTime).count();
            
            positionMs_ = startPositionMs + (uint32_t)(elapsed * tempoMultiplier_);
            
            if (positionMs_ >= durationMs_) {
                stop();
                break;
            }
            
            processEventsIncremental(positionMs_);
            
            auto frameEnd = std::chrono::steady_clock::now();
            auto frameDurationActual = frameEnd - frameStart;
            
            if (frameDurationActual < frameDuration) {
                std::this_thread::sleep_for(frameDuration - frameDurationActual);
            }
        }
        
        Logger::debug("MidiPlayer", "Playback loop ended");
    }

    void processEventsIncremental(uint32_t currentPositionMs) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        for (int track = 0; track < midiFile_.getTrackCount(); track++) {
            if (trackStates_[track].muted) continue;
            if (anySolo_ && !trackStates_[track].solo) continue;
            
            auto& playback = trackPlayback_[track];
            const auto& midiTrack = midiFile_[track];
            
            while (playback.currentEventIndex < midiTrack.size()) {
                const auto& event = midiTrack[playback.currentEventIndex];
                
                uint32_t eventTimeMs = (uint32_t)(event.seconds * 1000.0);
                
                if (eventTimeMs > currentPositionMs) {
                    break;
                }
                
                if (eventTimeMs >= playback.lastProcessedMs) {
                    processEvent(track, playback.currentEventIndex);
                }
                
                playback.currentEventIndex++;
            }
            
            playback.lastProcessedMs = currentPositionMs;
        }
    }

    void processEvent(int trackIndex, int eventIndex) {
        auto& event = midiFile_[trackIndex][eventIndex];
        
        if (!event.isNoteOn() && !event.isNoteOff() && !event.isController() && 
            !event.isProgramChange() && !event.isPitchBend()) {
            return;
        }
        
        std::vector<uint8_t> msgData;
        for (int i = 0; i < event.size(); i++) {
            msgData.push_back(event[i]);
        }
        MidiMessage msg(msgData);
        
        if (msg.isNoteOn()) {
            trackPlayback_[trackIndex].activeNotes.insert(msg.getKeyNumber());
        } else if (msg.isNoteOff()) {
            trackPlayback_[trackIndex].activeNotes.erase(msg.getKeyNumber());
        }
        
        if (msg.isNote()) {
            int note = msg.getKeyNumber();
            note += trackStates_[trackIndex].transposeSemitones;
            note += globalTranspose_;
            note = std::clamp(note, 0, 127);
            msg.setKeyNumber(note);
        }
        
        if (msg.isNoteOn()) {
            int velocity = msg.getVelocity();
            velocity = (int)(velocity * trackStates_[trackIndex].volume);
            velocity = std::clamp(velocity, 1, 127);
            msg.setVelocity(velocity);
        }
        
        router_->routeMessage(msg.getChannel(), msg);
    }

    void repositionPlayback(uint32_t targetPositionMs) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        Logger::debug("MidiPlayer", "Repositioning playback to " + 
                     std::to_string(targetPositionMs) + "ms");
        
        for (int track = 0; track < midiFile_.getTrackCount(); track++) {
            auto& playback = trackPlayback_[track];
            playback.reset();
            
            const auto& midiTrack = midiFile_[track];
            
            playback.currentEventIndex = findEventIndexAtTime(midiTrack, targetPositionMs);
            playback.lastProcessedMs = targetPositionMs;
            
            if (playback.currentEventIndex > 0) {
                restoreTrackState(track, playback.currentEventIndex);
            }
        }
    }

    size_t findEventIndexAtTime(const smf::MidiEventList& track, uint32_t timeMs) const {
        if (track.size() == 0) return 0;
        
        size_t left = 0;
        size_t right = track.size() - 1;
        
        while (left < right) {
            size_t mid = left + (right - left) / 2;
            uint32_t midTimeMs = (uint32_t)(track[mid].seconds * 1000.0);
            
            if (midTimeMs < timeMs) {
                left = mid + 1;
            } else {
                right = mid;
            }
        }
        
        return left;
    }

    void restoreTrackState(int trackIndex, size_t upToEventIndex) {
        const auto& midiTrack = midiFile_[trackIndex];
        
        int lastProgram = -1;
        std::map<uint8_t, uint8_t> lastCC;
        
        for (size_t i = 0; i < upToEventIndex && i < midiTrack.size(); i++) {
            const auto& event = midiTrack[i];
            
            if (event.isProgramChange()) {
                lastProgram = event[1];
            } else if (event.isController()) {
                lastCC[event[1]] = event[2];
            }
        }
        
        if (lastProgram >= 0) {
            std::vector<uint8_t> pcData = {
                (uint8_t)(0xC0 | midiTrack[0].getChannel()),
                (uint8_t)lastProgram
            };
            router_->routeMessage(midiTrack[0].getChannel(), MidiMessage(pcData));
        }
        
        const std::vector<uint8_t> importantCC = {7, 10, 11, 64, 91, 93};
        for (uint8_t cc : importantCC) {
            if (lastCC.find(cc) != lastCC.end()) {
                std::vector<uint8_t> ccData = {
                    (uint8_t)(0xB0 | midiTrack[0].getChannel()),
                    cc,
                    lastCC[cc]
                };
                router_->routeMessage(midiTrack[0].getChannel(), MidiMessage(ccData));
            }
        }
    }

    void sendAllNotesOffForTrack(int trackIndex) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (trackIndex < 0 || trackIndex >= (int)trackPlayback_.size()) return;
        
        auto& playback = trackPlayback_[trackIndex];
        
        for (uint8_t note : playback.activeNotes) {
            if (trackIndex < midiFile_.getTrackCount() && midiFile_[trackIndex].size() > 0) {
                int channel = midiFile_[trackIndex][0].getChannel();
                router_->routeMessage(channel, MidiMessage::noteOff(channel, note));
            }
        }
        
        playback.activeNotes.clear();
        
        std::set<int> channels;
        for (int i = 0; i < midiFile_[trackIndex].size(); i++) {
            if (midiFile_[trackIndex][i].isChannelMessage()) {
                channels.insert(midiFile_[trackIndex][i].getChannel());
            }
        }
        
        for (int channel : channels) {
            router_->routeMessage(channel, MidiMessage::allNotesOff(channel));
        }
    }

void sendAllNotesOffForTrack(int trackIndex) {
    std::vector<std::pair<int, MidiMessage>> messagesToSend;
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (trackIndex < 0 || trackIndex >= (int)trackPlayback_.size()) return;
        
        auto& playback = trackPlayback_[trackIndex];
        
        // ✅ Préparer les messages sous lock
        for (uint8_t note : playback.activeNotes) {
            if (trackIndex < midiFile_.getTrackCount() && 
                midiFile_[trackIndex].size() > 0) {
                int channel = midiFile_[trackIndex][0].getChannel();
                messagesToSend.emplace_back(channel, 
                    MidiMessage::noteOff(channel, note));
            }
        }
        
        playback.activeNotes.clear();
    } // ✅ Lock relâché ici
    
    // ✅ Envoyer sans lock
    for (const auto& [channel, msg] : messagesToSend) {
        router_->routeMessage(channel, msg);
    }
    
    // ✅ Envoyer All Notes Off
    if (!messagesToSend.empty()) {
        int channel = messagesToSend[0].first;
        router_->routeMessage(channel, MidiMessage::allNotesOff(channel));
    }
}


    std::shared_ptr<MidiRouter> router_;
    smf::MidiFile midiFile_;
    std::string currentFile_;
    
    std::atomic<PlayerState> state_;
    std::atomic<uint32_t> positionMs_;
    std::atomic<uint32_t> durationMs_;
    std::atomic<float> tempoMultiplier_;
    std::atomic<int> globalTranspose_;
    
    mutable std::mutex stateMutex_;
    std::vector<TrackState> trackStates_;
    std::vector<TrackPlaybackState> trackPlayback_;
    bool anySolo_ = false;
    
    std::thread playThread_;
};

} // namespace midiMind

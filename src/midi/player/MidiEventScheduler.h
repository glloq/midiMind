// ============================================================================
// src/midi/player/MidiEventScheduler.h
// Responsabilité: Scheduling et envoi des événements MIDI
// ============================================================================

#include "../MidiRouter.h"

struct ScheduledEvent {
    uint32_t timeMs;
    uint8_t track;
    uint8_t channel;
    MidiMessage message;
    
    bool operator>(const ScheduledEvent& other) const {
        return timeMs > other.timeMs;
    }
};

class MidiEventScheduler {
public:
    MidiEventScheduler(std::shared_ptr<MidiRouter> router)
        : router_(router), running_(false), globalTranspose_(0) {}
    
    ~MidiEventScheduler() {
        stop();
    }
    
    void start() {
        if (running_.exchange(true)) return;
        
        schedulerThread_ = std::thread(&MidiEventScheduler::processingLoop, this);
        Logger::info("EventScheduler", "Started");
    }
    
    void stop() {
        if (!running_.exchange(false)) return;
        
        cv_.notify_all();
        
        if (schedulerThread_.joinable()) {
            schedulerThread_.join();
        }
        
        clearAllEvents();
        Logger::info("EventScheduler", "Stopped");
    }
    
    void scheduleEvent(uint32_t timeMs, uint8_t track, uint8_t channel, 
                      const MidiMessage& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        ScheduledEvent event;
        event.timeMs = timeMs;
        event.track = track;
        event.channel = channel;
        event.message = message;
        
        events_.push(event);
        cv_.notify_one();
    }
    
    void clearAllEvents() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Vider la queue
        while (!events_.empty()) {
            events_.pop();
        }
        
        Logger::debug("EventScheduler", "All events cleared");
    }
    
    void setGlobalTranspose(int semitones) {
        globalTranspose_ = std::clamp(semitones, -12, 12);
    }
    
    int getGlobalTranspose() const {
        return globalTranspose_;
    }
    
    void setTrackManager(std::shared_ptr<MidiTrackManager> trackManager) {
        trackManager_ = trackManager;
    }

private:
    void processingLoop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // Attendre qu'il y ait des événements
            cv_.wait(lock, [this] {
                return !events_.empty() || !running_;
            });
            
            if (!running_) break;
            
            auto now = getCurrentTimeMs();
            
            while (!events_.empty()) {
                const auto& event = events_.top();
                
                // Si l'événement n'est pas encore prêt, attendre
                if (event.timeMs > now) {
                    break;
                }
                
                // Vérifier si la piste doit être jouée
                bool shouldPlay = true;
                if (trackManager_) {
                    shouldPlay = trackManager_->shouldPlayTrack(event.track);
                }
                
                if (shouldPlay) {
                    // Appliquer transformations
                    MidiMessage transformedMsg = applyTransformations(event);
                    
                    // Envoyer au router
                    lock.unlock();
                    router_->routeMessage(event.channel, transformedMsg);
                    lock.lock();
                }
                
                events_.pop();
            }
        }
    }
    
    MidiMessage applyTransformations(const ScheduledEvent& event) {
        MidiMessage msg = event.message;
        
        // Appliquer transpose global
        if (msg.isNote() && globalTranspose_ != 0) {
            int note = msg.getKeyNumber();
            note = std::clamp(note + globalTranspose_, 0, 127);
            msg.setKeyNumber((uint8_t)note);
        }
        
        // Appliquer transpose de piste
        if (trackManager_) {
            int trackTranspose = trackManager_->getTranspose(event.track);
            if (msg.isNote() && trackTranspose != 0) {
                int note = msg.getKeyNumber();
                note = std::clamp(note + trackTranspose, 0, 127);
                msg.setKeyNumber((uint8_t)note);
            }
            
            // Appliquer volume de piste
            float volume = trackManager_->getVolume(event.track);
            if (msg.isNoteOn() && volume < 1.0f) {
                int velocity = msg.getVelocity();
                velocity = (int)(velocity * volume);
                velocity = std::clamp(velocity, 1, 127);
                msg.setVelocity((uint8_t)velocity);
            }
        }
        
        return msg;
    }
    
    uint32_t getCurrentTimeMs() {
        // Cette fonction devrait être synchronisée avec le transport
        // Pour simplifier, on retourne un timestamp local
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        );
        return (uint32_t)ms.count();
    }
    
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<MidiTrackManager> trackManager_;
    
    std::priority_queue<
        ScheduledEvent,
        std::vector<ScheduledEvent>,
        std::greater<ScheduledEvent>
    > events_;
    
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread schedulerThread_;
    std::atomic<bool> running_;
    
    std::atomic<int> globalTranspose_;
};
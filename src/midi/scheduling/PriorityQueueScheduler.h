// ============================================================================
// Fichier: src/midi/scheduling/PriorityQueueScheduler.h
// ============================================================================

#pragma once

#include <queue>
#include <vector>
#include <functional>
#include "../MidiMessage.h"

namespace midiMind {

/**
 * @struct ScheduledMessage
 * @brief Message MIDI avec timestamp pour scheduling
 */
struct ScheduledMessage {
    MidiMessage message;
    uint64_t timestamp;  // En microsecondes
    int priority;
    
    // Comparateur pour priority_queue (inverse pour min-heap)
    bool operator<(const ScheduledMessage& other) const {
        if (timestamp != other.timestamp) {
            return timestamp > other.timestamp;
        }
        return priority > other.priority;
    }
};

/**
 * @class PriorityQueueScheduler
 * @brief Scheduler de messages MIDI avec priority queue
 */
class PriorityQueueScheduler {
public:
    using MessageCallback = std::function<void(const MidiMessage&)>;
    
    PriorityQueueScheduler() = default;
    
    /**
     * @brief Ajoute un message à scheduler
     */
    void schedule(const MidiMessage& msg, uint64_t timestamp, int priority = 0) {
        ScheduledMessage scheduled{msg, timestamp, priority};
        queue_.push(scheduled);
    }
    
    /**
     * @brief Traite les messages dont le timestamp est atteint
     */
    void process(uint64_t currentTime, MessageCallback callback) {
        while (!queue_.empty() && queue_.top().timestamp <= currentTime) {
            auto msg = queue_.top();
            queue_.pop();
            callback(msg.message);
        }
    }
    
    /**
     * @brief Efface tous les messages
     */
    void clear() {
        while (!queue_.empty()) {
            queue_.pop();
        }
    }
    
    /**
     * @brief Nombre de messages en attente
     */
    size_t size() const {
        return queue_.size();
    }

private:
    std::priority_queue<ScheduledMessage> queue_;
};

} // namespace midiMind
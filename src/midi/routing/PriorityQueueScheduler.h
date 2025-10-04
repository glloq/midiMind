// ============================================================================
// src/midi/routing/PriorityQueueScheduler.h
// Implémentation avec priority_queue (stratégie par défaut)
// ============================================================================

#include <queue>
#include <mutex>

class PriorityQueueScheduler : public ISchedulingStrategy {
public:
    PriorityQueueScheduler() = default;
    
    void schedule(const ScheduledMidiMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(msg);
    }
    
    std::optional<ScheduledMidiMessage> getNext() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        ScheduledMidiMessage msg = queue_.top();
        queue_.pop();
        
        return msg;
    }
    
    bool hasReady(uint32_t currentTimeMs) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (queue_.empty()) {
            return false;
        }
        
        return queue_.top().timeMs <= currentTimeMs;
    }
    
    void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        while (!queue_.empty()) {
            queue_.pop();
        }
    }
    
    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    std::string getName() const override {
        return "PriorityQueue";
    }

private:
    mutable std::mutex mutex_;
    std::priority_queue<
        ScheduledMidiMessage,
        std::vector<ScheduledMidiMessage>,
        std::greater<ScheduledMidiMessage>
    > queue_;
};
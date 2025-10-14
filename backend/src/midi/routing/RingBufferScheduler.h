// ============================================================================
// src/midi/routing/RingBufferScheduler.h
// Implémentation avec ring buffer (low-latency, optionnel)
// ============================================================================

template<size_t CAPACITY = 4096>
class RingBufferScheduler : public ISchedulingStrategy {
public:
    RingBufferScheduler() : readIdx_(0), writeIdx_(0), size_(0) {}
    
    void schedule(const ScheduledMidiMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (size_ >= CAPACITY) {
            Logger::warn("RingBufferScheduler", "Buffer full, dropping message");
            return;
        }
        
        buffer_[writeIdx_] = msg;
        writeIdx_ = (writeIdx_ + 1) % CAPACITY;
        size_++;
    }
    
    std::optional<ScheduledMidiMessage> getNext() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (size_ == 0) {
            return std::nullopt;
        }
        
        // Trouver le message avec le timestamp le plus petit
        size_t minIdx = readIdx_;
        uint32_t minTime = buffer_[readIdx_].timeMs;
        
        for (size_t i = 1; i < size_; i++) {
            size_t idx = (readIdx_ + i) % CAPACITY;
            if (buffer_[idx].timeMs < minTime) {
                minTime = buffer_[idx].timeMs;
                minIdx = idx;
            }
        }
        
        ScheduledMidiMessage msg = buffer_[minIdx];
        
        // Compacter en déplaçant les éléments
        if (minIdx != readIdx_) {
            buffer_[minIdx] = buffer_[readIdx_];
        }
        
        readIdx_ = (readIdx_ + 1) % CAPACITY;
        size_--;
        
        return msg;
    }
    
    bool hasReady(uint32_t currentTimeMs) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (size_ == 0) return false;
        
        for (size_t i = 0; i < size_; i++) {
            size_t idx = (readIdx_ + i) % CAPACITY;
            if (buffer_[idx].timeMs <= currentTimeMs) {
                return true;
            }
        }
        
        return false;
    }
    
    void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        readIdx_ = 0;
        writeIdx_ = 0;
        size_ = 0;
    }
    
    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }
    
    std::string getName() const override {
        return "RingBuffer";
    }

private:
    mutable std::mutex mutex_;
    std::array<ScheduledMidiMessage, CAPACITY> buffer_;
    size_t readIdx_;
    size_t writeIdx_;
    size_t size_;
};
// ============================================================================
// File: backend/src/core/EventBus.h
// Version: 4.2.3 - FIX string concatenation with parentheses
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#pragma once

#include <functional>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <typeindex>
#include <atomic>
#include <algorithm>

namespace midiMind {

// Forward declaration
class EventBus;

// ============================================================================
// CLASS: Subscription (RAII)
// ============================================================================

class Subscription {
public:
    Subscription() = default;
    
    explicit Subscription(std::function<void()> unsubscribe)
        : unsubscribe_(std::move(unsubscribe)) {}
    
    ~Subscription() {
        if (unsubscribe_) {
            unsubscribe_();
        }
    }
    
    Subscription(Subscription&& other) noexcept
        : unsubscribe_(std::move(other.unsubscribe_)) {
        other.unsubscribe_ = nullptr;
    }
    
    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            if (unsubscribe_) {
                unsubscribe_();
            }
            unsubscribe_ = std::move(other.unsubscribe_);
            other.unsubscribe_ = nullptr;
        }
        return *this;
    }
    
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

private:
    std::function<void()> unsubscribe_;
};

// ============================================================================
// CLASS: EventBus
// ============================================================================

class EventBus {
private:
    struct HandlerInfo {
        uint64_t id;
        std::shared_ptr<std::function<void(const void*)>> handler;
        int priority;
        std::shared_ptr<std::function<bool(const void*)>> filter;
    };

public:
    EventBus() : valid_(true) {}
    
    ~EventBus() {
        valid_ = false;
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }
    
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    template<typename EventType>
    Subscription subscribe(std::function<void(const EventType&)> handler, 
                          int priority = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::type_index typeIdx(typeid(EventType));
        uint64_t id = nextId_++;
        
        HandlerInfo info;
        info.id = id;
        info.priority = priority;
        info.handler = std::make_shared<std::function<void(const void*)>>(
            [handler](const void* data) {
                const EventType* event = static_cast<const EventType*>(data);
                handler(*event);
            }
        );
        
        handlers_[typeIdx].push_back(std::move(info));
        
        std::sort(handlers_[typeIdx].begin(), handlers_[typeIdx].end(),
            [](const HandlerInfo& a, const HandlerInfo& b) {
                return a.priority > b.priority;
            });
        
        return Subscription([this, typeIdx, id]() {
            if (valid_.load(std::memory_order_acquire)) {
                unsubscribe(typeIdx, id);
            }
        });
    }
    
    template<typename EventType>
    Subscription subscribe(std::function<void(const EventType&)> handler,
                          std::function<bool(const EventType&)> filter,
                          int priority = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::type_index typeIdx(typeid(EventType));
        uint64_t id = nextId_++;
        
        HandlerInfo info;
        info.id = id;
        info.priority = priority;
        info.handler = std::make_shared<std::function<void(const void*)>>(
            [handler](const void* data) {
                const EventType* event = static_cast<const EventType*>(data);
                handler(*event);
            }
        );
        info.filter = std::make_shared<std::function<bool(const void*)>>(
            [filter](const void* data) {
                const EventType* event = static_cast<const EventType*>(data);
                return filter(*event);
            }
        );
        
        handlers_[typeIdx].push_back(std::move(info));
        
        std::sort(handlers_[typeIdx].begin(), handlers_[typeIdx].end(),
            [](const HandlerInfo& a, const HandlerInfo& b) {
                return a.priority > b.priority;
            });
        
        return Subscription([this, typeIdx, id]() {
            if (valid_.load(std::memory_order_acquire)) {
                unsubscribe(typeIdx, id);
            }
        });
    }
    
    template<typename EventType>
    size_t publish(const EventType& event) {
        std::type_index typeIdx(typeid(EventType));
        
        std::vector<HandlerInfo> handlersCopy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(typeIdx);
            if (it != handlers_.end()) {
                handlersCopy = it->second;
            }
        }
        
        size_t count = 0;
        for (const auto& info : handlersCopy) {
            try {
                if (info.filter && !(*info.filter)(&event)) {
                    continue;
                }
                
                (*info.handler)(&event);
                count++;
            } catch (const std::exception& e) {
                logError("EventBus", ("Handler exception: " + std::string(e.what())));
            } catch (...) {
                logError("EventBus", "Handler exception: unknown error");
            }
        }
        
        totalEventsPublished_++;
        return count;
    }
    
    template<typename EventType>
    size_t getSubscriberCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::type_index typeIdx(typeid(EventType));
        auto it = handlers_.find(typeIdx);
        return (it != handlers_.end()) ? it->second.size() : 0;
    }
    
    size_t getEventTypeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.size();
    }
    
    uint64_t getTotalEventsPublished() const {
        return totalEventsPublished_.load();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }

private:
    void unsubscribe(std::type_index typeIdx, uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = handlers_.find(typeIdx);
        if (it == handlers_.end()) {
            return;
        }
        
        auto& vec = it->second;
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                [id](const HandlerInfo& info) {
                    return info.id == id;
                }),
            vec.end()
        );
    }
    
    static void logError(const char* component, const std::string& message);
    
    mutable std::mutex mutex_;
    std::map<std::type_index, std::vector<HandlerInfo>> handlers_;
    std::atomic<uint64_t> nextId_{0};
    std::atomic<uint64_t> totalEventsPublished_{0};
    std::atomic<bool> valid_{true};
};

} // namespace midiMind
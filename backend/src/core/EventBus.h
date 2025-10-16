// ============================================================================
// File: backend/src/core/EventBus.h
// Version: 4.1.1 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Thread-safe publish/subscribe event bus for decoupled communication.
//   Fixed: Subscription class moved before EventBus to resolve incomplete type.
//
// Features:
//   - Type-safe event publishing
//   - Priority-based handler ordering
//   - RAII subscription management
//   - Thread-safe operations
//   - Optional event filtering
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// Changes v4.1.1:
//   - Moved Subscription class definition before EventBus
//   - Resolved "incomplete type" compilation error
//
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

// ============================================================================
// CLASS: Subscription (RAII) - DÉFINI EN PREMIER
// ============================================================================

/**
 * @class Subscription
 * @brief RAII subscription handle
 * 
 * Automatically unsubscribes when destroyed.
 * Move-only (cannot be copied).
 */
class Subscription {
public:
    /**
     * @brief Default constructor (empty subscription)
     */
    Subscription() = default;
    
    /**
     * @brief Constructor with unsubscribe callback
     */
    explicit Subscription(std::function<void()> unsubscribe)
        : unsubscribe_(std::move(unsubscribe)) {}
    
    /**
     * @brief Destructor - automatically unsubscribes
     */
    ~Subscription() {
        if (unsubscribe_) {
            unsubscribe_();
        }
    }
    
    // Move semantics
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
    
    // Disable copy
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

private:
    std::function<void()> unsubscribe_;
};

// ============================================================================
// CLASS: EventBus
// ============================================================================

/**
 * @class EventBus
 * @brief Thread-safe publish/subscribe event bus
 * 
 * Provides decoupled communication between components using type-safe events.
 * Handlers are called in priority order (higher priority first).
 * 
 * Example:
 * @code
 * EventBus bus;
 * 
 * // Subscribe
 * auto sub = bus.subscribe<MidiEvent>(
 *     [](const MidiEvent& e) { handleEvent(e); },
 *     100  // Priority
 * );
 * 
 * // Publish
 * MidiEvent event;
 * bus.publish(event);
 * @endcode
 */
class EventBus {
private:
    // ========================================================================
    // PRIVATE STRUCTURES
    // ========================================================================
    
    /**
     * @struct HandlerInfo
     * @brief Internal handler information
     */
    struct HandlerInfo {
        uint64_t id;
        std::function<void(const void*)> handler;
        int priority;
        std::function<bool(const void*)> filter;
    };

public:
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     */
    EventBus() = default;
    
    /**
     * @brief Destructor
     */
    ~EventBus() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }
    
    // Disable copy
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    // ========================================================================
    // SUBSCRIPTION
    // ========================================================================
    
    /**
     * @brief Subscribe to events
     * @tparam EventType Type of event
     * @param handler Event handler function
     * @param priority Priority (higher = called first, default 0)
     * @return Subscription RAII handle
     * 
     * Example:
     * @code
     * auto sub = bus.subscribe<MidiEvent>(
     *     [](const MidiEvent& e) { handleEvent(e); },
     *     100
     * );
     * @endcode
     */
    template<typename EventType>
    Subscription subscribe(std::function<void(const EventType&)> handler, 
                          int priority = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::type_index typeIdx(typeid(EventType));
        uint64_t id = nextId_++;
        
        HandlerInfo info;
        info.id = id;
        info.priority = priority;
        info.handler = [handler](const void* data) {
            const EventType* event = static_cast<const EventType*>(data);
            handler(*event);
        };
        
        handlers_[typeIdx].push_back(std::move(info));
        
        // Sort by priority (descending)
        std::sort(handlers_[typeIdx].begin(), handlers_[typeIdx].end(),
            [](const HandlerInfo& a, const HandlerInfo& b) {
                return a.priority > b.priority;
            });
        
        return Subscription([this, typeIdx, id]() {
            unsubscribe(typeIdx, id);
        });
    }
    
    /**
     * @brief Subscribe with filter
     * @tparam EventType Type of event
     * @param handler Event handler function
     * @param filter Filter function (return true to handle)
     * @param priority Priority (default 0)
     * @return Subscription RAII handle
     * 
     * Example:
     * @code
     * auto sub = bus.subscribe<MidiEvent>(
     *     [](const MidiEvent& e) { handleEvent(e); },
     *     [](const MidiEvent& e) { return e.velocity > 50; },
     *     100
     * );
     * @endcode
     */
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
        info.handler = [handler](const void* data) {
            const EventType* event = static_cast<const EventType*>(data);
            handler(*event);
        };
        info.filter = [filter](const void* data) {
            const EventType* event = static_cast<const EventType*>(data);
            return filter(*event);
        };
        
        handlers_[typeIdx].push_back(std::move(info));
        
        std::sort(handlers_[typeIdx].begin(), handlers_[typeIdx].end(),
            [](const HandlerInfo& a, const HandlerInfo& b) {
                return a.priority > b.priority;
            });
        
        return Subscription([this, typeIdx, id]() {
            unsubscribe(typeIdx, id);
        });
    }
    
    // ========================================================================
    // PUBLISHING
    // ========================================================================
    
    /**
     * @brief Publish event to all subscribers
     * @tparam EventType Type of event
     * @param event Event to publish
     * @return size_t Number of handlers called
     * 
     * Example:
     * @code
     * MidiEvent event;
     * size_t count = bus.publish(event);
     * @endcode
     */
    template<typename EventType>
    size_t publish(const EventType& event) {
        std::type_index typeIdx(typeid(EventType));
        
        // Copy handlers to avoid holding lock during callbacks
        std::vector<HandlerInfo> handlersCopy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(typeIdx);
            if (it != handlers_.end()) {
                handlersCopy = it->second;
            }
        }
        
        // Call handlers
        size_t count = 0;
        for (const auto& info : handlersCopy) {
            try {
                // Check filter if present
                if (info.filter && !info.filter(&event)) {
                    continue;
                }
                
                info.handler(&event);
                count++;
            } catch (const std::exception& e) {
                // Log error but continue with other handlers
            }
        }
        
        totalEventsPublished_++;
        return count;
    }
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get subscriber count for specific event type
     * @tparam EventType Type of event
     * @return size_t Number of subscribers
     */
    template<typename EventType>
    size_t getSubscriberCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::type_index typeIdx(typeid(EventType));
        auto it = handlers_.find(typeIdx);
        return (it != handlers_.end()) ? it->second.size() : 0;
    }
    
    /**
     * @brief Get total number of registered event types
     * @return size_t Number of event types
     */
    size_t getEventTypeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.size();
    }
    
    /**
     * @brief Get total events published since creation
     * @return uint64_t Total event count
     */
    uint64_t getTotalEventsPublished() const {
        return totalEventsPublished_.load();
    }
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    /**
     * @brief Clear all subscriptions
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Unsubscribe handler by ID
     */
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
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    mutable std::mutex mutex_;
    std::map<std::type_index, std::vector<HandlerInfo>> handlers_;
    std::atomic<uint64_t> nextId_{0};
    std::atomic<uint64_t> totalEventsPublished_{0};
};

} // namespace midiMind

// ============================================================================
// END OF FILE EventBus.h
// ============================================================================

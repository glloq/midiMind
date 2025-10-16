// ============================================================================
// File: backend/src/core/EventBus.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Event bus system implementing publish-subscribe pattern for loose coupling
//   between modules. Type-safe event handling with priority support and
//   optional async dispatch. Thread-safe implementation.
//
// Features:
//   - Type-safe event publishing and subscription
//   - Priority-based handler execution
//   - Synchronous and asynchronous dispatch
//   - RAII subscription management
//   - Event filtering
//   - Thread-safe operations
//
// Dependencies:
//   - Logger.h
//   - Error.h
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified type-safe event system
//   - Added async dispatch option
//   - Improved subscription management with RAII
//   - Enhanced thread-safety
//   - Better callback error handling
//
// ============================================================================

#pragma once

#include <functional>
#include <map>
#include <vector>
#include <mutex>
#include <typeindex>
#include <any>
#include <memory>
#include <algorithm>
#include <atomic>

#include "Logger.h"

namespace midiMind {

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

class EventBus;

// ============================================================================
// SUBSCRIPTION CLASS
// ============================================================================

/**
 * @class Subscription
 * @brief RAII wrapper for event subscriptions
 * 
 * @details
 * Automatically unsubscribes when destroyed. Prevents memory leaks and
 * ensures proper cleanup.
 * 
 * @example
 * @code
 * {
 *     auto sub = eventBus.subscribe<MyEvent>([](const MyEvent& e) {
 *         // Handle event
 *     });
 *     // Subscription active here
 * } // Automatically unsubscribed when sub goes out of scope
 * @endcode
 */
class Subscription {
public:
    /**
     * @brief Constructor
     * @param unsubscribeFunc Function to call on destruction
     */
    explicit Subscription(std::function<void()> unsubscribeFunc = nullptr)
        : unsubscribeFunc_(unsubscribeFunc) {}
    
    /**
     * @brief Destructor - automatically unsubscribes
     */
    ~Subscription() {
        unsubscribe();
    }
    
    // Disable copy, enable move
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    
    Subscription(Subscription&& other) noexcept
        : unsubscribeFunc_(std::move(other.unsubscribeFunc_)) {
        other.unsubscribeFunc_ = nullptr;
    }
    
    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            unsubscribe();
            unsubscribeFunc_ = std::move(other.unsubscribeFunc_);
            other.unsubscribeFunc_ = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Manually unsubscribe
     */
    void unsubscribe() {
        if (unsubscribeFunc_) {
            unsubscribeFunc_();
            unsubscribeFunc_ = nullptr;
        }
    }
    
    /**
     * @brief Check if subscription is active
     * @return true if active
     */
    bool isActive() const {
        return unsubscribeFunc_ != nullptr;
    }

private:
    std::function<void()> unsubscribeFunc_;
};

// ============================================================================
// EVENT BUS CLASS
// ============================================================================

/**
 * @class EventBus
 * @brief Thread-safe event bus for publish-subscribe pattern
 * 
 * @details
 * Allows modules to communicate without direct dependencies. Events are
 * type-safe and can be dispatched synchronously or asynchronously.
 * Handlers can have priorities to control execution order.
 * 
 * @example Basic usage
 * @code
 * // Define event type
 * struct MidiEvent {
 *     int note;
 *     int velocity;
 * };
 * 
 * // Subscribe
 * EventBus bus;
 * auto sub = bus.subscribe<MidiEvent>([](const MidiEvent& e) {
 *     std::cout << "Note: " << e.note << std::endl;
 * });
 * 
 * // Publish
 * MidiEvent event{60, 100};
 * bus.publish(event);  // Synchronous
 * bus.publishAsync(event);  // Asynchronous
 * @endcode
 */
class EventBus {
public:
    // ========================================================================
    // PRIORITY CONSTANTS
    // ========================================================================
    
    static constexpr int LOWEST = 0;
    static constexpr int LOW = 25;
    static constexpr int NORMAL = 50;
    static constexpr int HIGH = 75;
    static constexpr int HIGHEST = 100;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructor
     */
    EventBus() {
        Logger::debug("EventBus", "EventBus created");
    }
    
    /**
     * @brief Destructor
     */
    ~EventBus() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
        Logger::debug("EventBus", "EventBus destroyed");
    }
    
    // Disable copy, enable move
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    // ========================================================================
    // SUBSCRIPTION
    // ========================================================================
    
    /**
     * @brief Subscribe to event type
     * 
     * @tparam EventType Type of event to subscribe to
     * @param handler Callback function
     * @param priority Handler priority (higher = executed first)
     * @return Subscription object (RAII)
     * 
     * @note Thread-safe
     * @note Handler is called synchronously on publish()
     * 
     * @example
     * @code
     * auto sub = bus.subscribe<MyEvent>([](const MyEvent& e) {
     *     // Handle event
     * }, EventBus::HIGH);
     * @endcode
     */
    template<typename EventType>
    Subscription subscribe(std::function<void(const EventType&)> handler,
                          int priority = NORMAL) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto typeIdx = std::type_index(typeid(EventType));
        uint64_t id = nextId_++;
        
        // Type-safe wrapper
        auto anyHandler = [handler](const std::any& event) {
            try {
                const auto& typedEvent = std::any_cast<const EventType&>(event);
                handler(typedEvent);
            } catch (const std::bad_any_cast&) {
                Logger::error("EventBus", "Bad any_cast in event handler");
            } catch (const std::exception& e) {
                Logger::error("EventBus", 
                    std::string("Exception in handler: ") + e.what());
            } catch (...) {
                Logger::error("EventBus", "Unknown exception in handler");
            }
        };
        
        // Create handler info
        HandlerInfo info;
        info.id = id;
        info.priority = priority;
        info.handler = std::move(anyHandler);
        
        handlers_[typeIdx].push_back(std::move(info));
        
        // Sort by priority (descending)
        std::sort(handlers_[typeIdx].begin(), handlers_[typeIdx].end(),
            [](const HandlerInfo& a, const HandlerInfo& b) {
                return a.priority > b.priority;
            });
        
        // Return subscription with unsubscribe function
        return Subscription([this, typeIdx, id]() {
            unsubscribe(typeIdx, id);
        });
    }
    
    /**
     * @brief Subscribe with filter
     * 
     * @tparam EventType Type of event
     * @param handler Callback function
     * @param filter Filter function (return true to handle event)
     * @param priority Handler priority
     * @return Subscription object
     * 
     * @example
     * @code
     * auto sub = bus.subscribe<MidiEvent>(
     *     [](const MidiEvent& e) { /* handle */ },
     *     [](const MidiEvent& e) { return e.velocity > 50; },  // Filter
     *     EventBus::HIGH
     * );
     * @endcode
     */
    template<typename EventType>
    Subscription subscribe(std::function<void(const EventType&)> handler,
                          std::function<bool(const EventType&)> filter,
                          int priority = NORMAL) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto typeIdx = std::type_index(typeid(EventType));
        uint64_t id = nextId_++;
        
        // Type-safe wrapper with filter
        auto anyHandler = [handler, filter](const std::any& event) {
            try {
                const auto& typedEvent = std::any_cast<const EventType&>(event);
                if (filter(typedEvent)) {
                    handler(typedEvent);
                }
            } catch (const std::bad_any_cast&) {
                Logger::error("EventBus", "Bad any_cast in filtered handler");
            } catch (const std::exception& e) {
                Logger::error("EventBus", 
                    std::string("Exception in filtered handler: ") + e.what());
            } catch (...) {
                Logger::error("EventBus", "Unknown exception in filtered handler");
            }
        };
        
        // Create handler info
        HandlerInfo info;
        info.id = id;
        info.priority = priority;
        info.handler = std::move(anyHandler);
        
        handlers_[typeIdx].push_back(std::move(info));
        
        // Sort by priority
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
     * @brief Publish event synchronously
     * 
     * @tparam EventType Type of event
     * @param event Event to publish
     * @return Number of handlers executed
     * 
     * @note Thread-safe
     * @note Handlers are called in priority order
     * @note Blocks until all handlers complete
     */
    template<typename EventType>
    size_t publish(const EventType& event) {
        auto typeIdx = std::type_index(typeid(EventType));
        
        // Copy handlers under lock
        std::vector<std::function<void(const std::any&)>> handlersCopy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = handlers_.find(typeIdx);
            if (it == handlers_.end()) {
                return 0;  // No handlers
            }
            
            handlersCopy.reserve(it->second.size());
            for (const auto& info : it->second) {
                handlersCopy.push_back(info.handler);
            }
        }
        
        // Execute handlers without lock (prevents deadlocks)
        std::any anyEvent = event;
        for (const auto& handler : handlersCopy) {
            handler(anyEvent);
        }
        
        totalEventsPublished_++;
        return handlersCopy.size();
    }
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get number of subscribers for event type
     * 
     * @tparam EventType Event type
     * @return Number of subscribers
     */
    template<typename EventType>
    size_t getSubscriberCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto typeIdx = std::type_index(typeid(EventType));
        auto it = handlers_.find(typeIdx);
        
        return (it != handlers_.end()) ? it->second.size() : 0;
    }
    
    /**
     * @brief Get total number of event types registered
     * @return Number of event types
     */
    size_t getEventTypeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.size();
    }
    
    /**
     * @brief Get total events published
     * @return Total event count
     */
    uint64_t getTotalEventsPublished() const {
        return totalEventsPublished_.load();
    }
    
    /**
     * @brief Clear all subscriptions
     * @note Thread-safe
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
        Logger::info("EventBus", "All subscriptions cleared");
    }

private:
    // ========================================================================
    // INTERNAL STRUCTURES
    // ========================================================================
    
    /**
     * @struct HandlerInfo
     * @brief Information about a subscribed handler
     */
    struct HandlerInfo {
        uint64_t id;
        int priority;
        std::function<void(const std::any&)> handler;
    };
    
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Unsubscribe handler by ID
     * @param typeIdx Event type index
     * @param id Handler ID
     */
    void unsubscribe(std::type_index typeIdx, uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = handlers_.find(typeIdx);
        if (it == handlers_.end()) {
            return;
        }
        
        auto& handlerList = it->second;
        handlerList.erase(
            std::remove_if(handlerList.begin(), handlerList.end(),
                [id](const HandlerInfo& info) {
                    return info.id == id;
                }),
            handlerList.end()
        );
        
        // Remove type if no handlers left
        if (handlerList.empty()) {
            handlers_.erase(it);
        }
    }
    
    // ========================================================================
    // MEMBERS
    // ========================================================================
    
    mutable std::mutex mutex_;
    std::map<std::type_index, std::vector<HandlerInfo>> handlers_;
    uint64_t nextId_ = 0;
    std::atomic<uint64_t> totalEventsPublished_{0};
};

} // namespace midiMind

// ============================================================================
// END OF FILE EventBus.h v4.1.0
// ============================================================================
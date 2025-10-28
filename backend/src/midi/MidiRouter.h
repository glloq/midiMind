// ============================================================================
// File: backend/src/midi/MidiRouter.h
// Version: 4.2.0 - EventBus Integration
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================

#pragma once

#include "MidiMessage.h"
#include "../timing/LatencyCompensator.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <shared_mutex>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// Forward declarations
class MidiDevice;
class EventBus;

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * @struct MidiRoute
 * @brief Route definition for MIDI message routing
 */
struct MidiRoute {
    std::string id;                    // Unique route identifier
    std::string name;                  // Human-readable name
    std::string sourceDeviceId;        // Source device (empty = any)
    std::string destinationDeviceId;   // Destination device
    int priority;                      // Priority (higher = first)
    bool enabled;                      // Route enabled/disabled
    
    // Filters
    std::vector<uint8_t> channelFilter;     // Empty = all channels
    std::vector<uint8_t> messageTypeFilter; // Empty = all types
    
    // Transformations
    int8_t channelTransform;           // Channel offset (-16 to +16, 0 = no change)
    int8_t velocityTransform;          // Velocity offset (-127 to +127, 0 = no change)
    int8_t transposeTransform;         // Transpose semitones (-127 to +127, 0 = no change)
    
    MidiRoute()
        : priority(50)
        , enabled(true)
        , channelTransform(0)
        , velocityTransform(0)
        , transposeTransform(0)
    {}
};

/**
 * @struct RouteStatistics
 * @brief Statistics for a specific route
 */
struct RouteStatistics {
    std::string routeId;
    std::string routeName;
    std::atomic<uint64_t> messagesRouted{0};
    std::atomic<uint64_t> lastMessageTime{0};
    std::atomic<int64_t> avgCompensation{0};
    
    // Default constructor
    RouteStatistics() = default;
    
    // Copy constructor - needed for map operations
    RouteStatistics(const RouteStatistics& other) 
        : routeId(other.routeId)
        , routeName(other.routeName)
        , messagesRouted(other.messagesRouted.load())
        , lastMessageTime(other.lastMessageTime.load())
        , avgCompensation(other.avgCompensation.load())
    {}
    
    // Copy assignment operator
    RouteStatistics& operator=(const RouteStatistics& other) {
        if (this != &other) {
            routeId = other.routeId;
            routeName = other.routeName;
            messagesRouted.store(other.messagesRouted.load());
            lastMessageTime.store(other.lastMessageTime.load());
            avgCompensation.store(other.avgCompensation.load());
        }
        return *this;
    }
    
    // Move constructor
    RouteStatistics(RouteStatistics&& other) noexcept
        : routeId(std::move(other.routeId))
        , routeName(std::move(other.routeName))
        , messagesRouted(other.messagesRouted.load())
        , lastMessageTime(other.lastMessageTime.load())
        , avgCompensation(other.avgCompensation.load())
    {}
    
    // Move assignment operator
    RouteStatistics& operator=(RouteStatistics&& other) noexcept {
        if (this != &other) {
            routeId = std::move(other.routeId);
            routeName = std::move(other.routeName);
            messagesRouted.store(other.messagesRouted.load());
            lastMessageTime.store(other.lastMessageTime.load());
            avgCompensation.store(other.avgCompensation.load());
        }
        return *this;
    }
    
    json toJson() const {
        return json{
            {"route_id", routeId},
            {"route_name", routeName},
            {"messages_routed", messagesRouted.load()},
            {"last_message_time", lastMessageTime.load()},
            {"avg_compensation_us", avgCompensation.load()}
        };
    }
};

/**
 * @struct GlobalRoutingStatistics
 * @brief Global routing statistics
 */
struct GlobalRoutingStatistics {
    std::atomic<uint64_t> totalMessages{0};
    std::atomic<uint64_t> routedMessages{0};
    std::atomic<uint64_t> droppedMessages{0};
};

// ============================================================================
// CLASS: MidiRouter
// ============================================================================

/**
 * @class MidiRouter
 * @brief Routes MIDI messages with filtering, transformation, and latency compensation
 * 
 * Thread Safety: All methods are thread-safe using shared_mutex for read/write locks.
 * 
 * EventBus Integration: Publishes RouteAddedEvent and RouteRemovedEvent.
 */
class MidiRouter {
public:
    using MessageCallback = std::function<void(const MidiMessage&, const std::string& deviceId)>;
    
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param compensator Optional latency compensator
     * @param eventBus Optional EventBus for publishing events
     */
    explicit MidiRouter(LatencyCompensator* compensator = nullptr,
                       std::shared_ptr<EventBus> eventBus = nullptr);
    
    /**
     * @brief Destructor
     */
    ~MidiRouter();
    
    // Disable copy
    MidiRouter(const MidiRouter&) = delete;
    MidiRouter& operator=(const MidiRouter&) = delete;
    
    // ========================================================================
    // ROUTING
    // ========================================================================
    
    /**
     * @brief Route a MIDI message through configured routes
     * @param message MIDI message to route
     */
    void route(const MidiMessage& message);
    
    /**
     * @brief Route directly to a specific device (bypass routing table)
     * @param message MIDI message to send
     * @param deviceId Destination device ID
     */
    void routeTo(const MidiMessage& message, const std::string& deviceId);
    
    /**
     * @brief Set callback for routed messages
     * @param callback Function to call when message is routed
     */
    void setMessageCallback(MessageCallback callback);
    
    // ========================================================================
    // ROUTE MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Add a route
     * @param route Route to add
     */
    void addRoute(std::shared_ptr<MidiRoute> route);
    
    /**
     * @brief Add a simple route (device to device)
     * @param sourceDeviceId Source device ID (empty = any)
     * @param destinationDeviceId Destination device ID
     * @return bool true if added successfully
     */
    bool addRoute(const std::string& sourceDeviceId, 
                  const std::string& destinationDeviceId);
    
    /**
     * @brief Remove a route by ID
     * @param id Route ID
     * @return bool true if removed
     */
    bool removeRoute(const std::string& id);
    
    /**
     * @brief Remove a route by device IDs
     * @param sourceDeviceId Source device ID
     * @param destinationDeviceId Destination device ID
     * @return bool true if removed
     */
    bool removeRoute(const std::string& sourceDeviceId,
                     const std::string& destinationDeviceId);
    
    /**
     * @brief Get a route by ID
     * @param id Route ID
     * @return std::shared_ptr<MidiRoute> Route or nullptr
     */
    std::shared_ptr<MidiRoute> getRoute(const std::string& id) const;
    
    /**
     * @brief Get all routes
     * @return std::vector<std::shared_ptr<MidiRoute>> All routes
     */
    std::vector<std::shared_ptr<MidiRoute>> getRoutes() const;
    
    /**
     * @brief Enable/disable a route
     * @param id Route ID
     * @param enabled true to enable, false to disable
     */
    void setRouteEnabled(const std::string& id, bool enabled);
    
    /**
     * @brief Enable a route
     * @param sourceDeviceId Source device ID
     * @param destinationDeviceId Destination device ID
     * @return bool true if enabled
     */
    bool enableRoute(const std::string& sourceDeviceId,
                     const std::string& destinationDeviceId);
    
    /**
     * @brief Disable a route
     * @param sourceDeviceId Source device ID
     * @param destinationDeviceId Destination device ID
     * @return bool true if disabled
     */
    bool disableRoute(const std::string& sourceDeviceId,
                      const std::string& destinationDeviceId);
    
    /**
     * @brief Clear all routes
     */
    void clearRoutes();
    
    // ========================================================================
    // DEVICE MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Register a MIDI device
     * @param device Device to register
     */
    void registerDevice(std::shared_ptr<MidiDevice> device);
    
    /**
     * @brief Unregister a MIDI device
     * @param deviceId Device ID
     */
    void unregisterDevice(const std::string& deviceId);
    
    /**
     * @brief Get a registered device
     * @param deviceId Device ID
     * @return std::shared_ptr<MidiDevice> Device or nullptr
     */
    std::shared_ptr<MidiDevice> getDevice(const std::string& deviceId) const;
    
    // ========================================================================
    // LATENCY COMPENSATION
    // ========================================================================
    
    /**
     * @brief Set latency compensator
     * @param compensator Latency compensator
     */
    void setLatencyCompensator(LatencyCompensator* compensator);
    
    /**
     * @brief Enable/disable instrument-specific compensation
     * @param enabled true to enable
     */
    void setInstrumentCompensationEnabled(bool enabled);
    
    /**
     * @brief Check if instrument compensation is enabled
     * @return bool true if enabled
     */
    bool isInstrumentCompensationEnabled() const;
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get route statistics
     * @param routeId Route ID
     * @return RouteStatistics Statistics for the route
     */
    RouteStatistics getRouteStatistics(const std::string& routeId) const;
    
    /**
     * @brief Get global statistics
     * @return json Statistics as JSON
     */
    json getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics();
    
    // ========================================================================
    // EVENTBUS
    // ========================================================================
    
    /**
     * @brief Set EventBus for publishing events
     * @param eventBus EventBus instance
     */
    void setEventBus(std::shared_ptr<EventBus> eventBus);

private:
    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================
    
    /**
     * @brief Check if message matches route filters
     */
    bool matchesRoute(const MidiMessage& message, const MidiRoute& route) const;
    
    /**
     * @brief Apply transformations to message
     */
    MidiMessage applyTransformations(const MidiMessage& message, 
                                    const MidiRoute& route) const;
    
    /**
     * @brief Get compensation for route
     */
    int64_t getCompensationForRoute(const MidiRoute& route) const;
    
    /**
     * @brief Send message to device
     */
    void sendToDevice(const std::string& deviceId, const MidiMessage& message);
    
    /**
     * @brief Update route statistics
     */
    void updateRouteStatistics(const std::string& routeId, int64_t compensation);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /// Routes
    std::vector<std::shared_ptr<MidiRoute>> routes_;
    
    /// Registered devices
    std::unordered_map<std::string, std::shared_ptr<MidiDevice>> devices_;
    
    /// Route statistics
    std::unordered_map<std::string, RouteStatistics> routeStats_;
    
    /// Global statistics
    GlobalRoutingStatistics globalStats_;
    
    /// Latency compensator
    std::atomic<LatencyCompensator*> compensator_;
    
    /// Instrument compensation enabled
    std::atomic<bool> instrumentCompensationEnabled_;
    
    /// Message callback
    MessageCallback messageCallback_;
    std::mutex callbackMutex_;
    
    /// Thread safety
    mutable std::shared_mutex mutex_;
    
    /// EventBus for publishing events
    std::shared_ptr<EventBus> eventBus_;
};

} // namespace midiMind

// ============================================================================
// END OF FILE MidiRouter.h
// ============================================================================
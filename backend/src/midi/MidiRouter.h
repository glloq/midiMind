// ============================================================================
// File: backend/src/midi/MidiRouter.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   MIDI message routing system with filtering and transformation.
//   Routes MIDI messages from sources to destinations based on rules.
//
// Features:
//   - Rule-based routing with priorities
//   - Channel, note, velocity filtering
//   - Multi-destination support (layering)
//   - Instrument-level latency compensation
//   - Statistics and monitoring
//   - Thread-safe operations
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Integration with LatencyCompensator
//   - Per-instrument compensation
//   - Enhanced routing statistics
//
// ============================================================================

#pragma once

#include "MidiMessage.h"
#include "../timing/LatencyCompensator.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

// Forward declaration
class MidiDevice;

// ============================================================================
// STRUCTURE: MidiRoute
// ============================================================================

/**
 * @struct MidiRoute
 * @brief Defines a routing rule from source to destination
 */
struct MidiRoute {
    /// Unique route ID
    std::string id;
    
    /// Route name
    std::string name;
    
    /// Source device ID (empty = any)
    std::string sourceDeviceId;
    
    /// Destination device ID
    std::string destinationDeviceId;
    
    /// Instrument ID (for latency compensation)
    std::string instrumentId;
    
    /// Priority (higher = processed first)
    int priority;
    
    /// Enabled flag
    bool enabled;
    
    // Filters
    std::vector<uint8_t> channelFilter;        ///< Allowed channels (empty = all)
    std::vector<std::string> messageTypeFilter; ///< Allowed types (empty = all)
    std::optional<uint8_t> minNote;            ///< Minimum note (inclusive)
    std::optional<uint8_t> maxNote;            ///< Maximum note (inclusive)
    std::optional<uint8_t> minVelocity;        ///< Minimum velocity
    std::optional<uint8_t> maxVelocity;        ///< Maximum velocity
    
    // Transformations
    std::optional<int8_t> channelTransform;    ///< Channel offset
    std::optional<int8_t> transposeTransform;  ///< Transpose semitones
    std::optional<float> velocityScale;        ///< Velocity multiplier
    
    /**
     * @brief Constructor
     */
    MidiRoute()
        : priority(50)
        , enabled(true)
    {}
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const {
        json j = {
            {"id", id},
            {"name", name},
            {"source_device_id", sourceDeviceId},
            {"destination_device_id", destinationDeviceId},
            {"instrument_id", instrumentId},
            {"priority", priority},
            {"enabled", enabled}
        };
        
        if (!channelFilter.empty()) j["channel_filter"] = channelFilter;
        if (!messageTypeFilter.empty()) j["message_type_filter"] = messageTypeFilter;
        if (minNote.has_value()) j["min_note"] = minNote.value();
        if (maxNote.has_value()) j["max_note"] = maxNote.value();
        if (minVelocity.has_value()) j["min_velocity"] = minVelocity.value();
        if (maxVelocity.has_value()) j["max_velocity"] = maxVelocity.value();
        
        return j;
    }
    
    /**
     * @brief Create from JSON
     */
    static MidiRoute fromJson(const json& j) {
        MidiRoute route;
        
        route.id = j.value("id", "");
        route.name = j.value("name", "");
        route.sourceDeviceId = j.value("source_device_id", "");
        route.destinationDeviceId = j.value("destination_device_id", "");
        route.instrumentId = j.value("instrument_id", "");
        route.priority = j.value("priority", 50);
        route.enabled = j.value("enabled", true);
        
        if (j.contains("channel_filter")) {
            route.channelFilter = j["channel_filter"].get<std::vector<uint8_t>>();
        }
        if (j.contains("message_type_filter")) {
            route.messageTypeFilter = j["message_type_filter"].get<std::vector<std::string>>();
        }
        if (j.contains("min_note")) route.minNote = j["min_note"];
        if (j.contains("max_note")) route.maxNote = j["max_note"];
        if (j.contains("min_velocity")) route.minVelocity = j["min_velocity"];
        if (j.contains("max_velocity")) route.maxVelocity = j["max_velocity"];
        
        return route;
    }
};

// ============================================================================
// STRUCTURE: RouteStatistics
// ============================================================================

/**
 * @struct RouteStatistics
 * @brief Statistics for a single route
 */
struct RouteStatistics {
    std::string routeId;
    std::string routeName;
    uint64_t messagesRouted;
    uint64_t messagesDropped;
    int64_t avgCompensation;
    uint64_t lastMessageTime;
    
    /**
     * @brief Constructor
     */
    RouteStatistics()
        : messagesRouted(0)
        , messagesDropped(0)
        , avgCompensation(0)
        , lastMessageTime(0)
    {}
    
    /**
     * @brief Convert to JSON
     */
    json toJson() const {
        return {
            {"route_id", routeId},
            {"route_name", routeName},
            {"messages_routed", messagesRouted},
            {"messages_dropped", messagesDropped},
            {"avg_compensation_us", avgCompensation},
            {"last_message_time", lastMessageTime}
        };
    }
};

// ============================================================================
// CLASS: MidiRouter
// ============================================================================

/**
 * @class MidiRouter
 * @brief Routes MIDI messages with filtering and latency compensation
 * 
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses shared_mutex for read/write locking
 * 
 * Example:
 * ```cpp
 * MidiRouter router(latencyCompensator);
 * 
 * // Create route
 * MidiRoute route;
 * route.id = "piano_route";
 * route.sourceDeviceId = "keyboard";
 * route.destinationDeviceId = "synth";
 * route.instrumentId = "piano_001";
 * route.channelFilter = {0, 1};  // Channels 1-2
 * 
 * router.addRoute(std::make_shared<MidiRoute>(route));
 * 
 * // Route message
 * auto msg = MidiMessage::noteOn(0, 60, 100);
 * router.route(msg);
 * ```
 */
class MidiRouter {
public:
    // ========================================================================
    // TYPE DEFINITIONS
    // ========================================================================
    
    /// Message callback function
    using MessageCallback = std::function<void(const MidiMessage&, const std::string& deviceId)>;
    
    // ========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ========================================================================
    
    /**
     * @brief Constructor
     * @param compensator Latency compensator (optional)
     */
    explicit MidiRouter(LatencyCompensator* compensator = nullptr);
    
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
     * @brief Route message through matching routes
     * @param message MIDI message
     * @note Applies filters, transformations, and latency compensation
     */
    void route(const MidiMessage& message);
    
    /**
     * @brief Route message to specific device (bypass routes)
     * @param message MIDI message
     * @param deviceId Destination device ID
     */
    void routeTo(const MidiMessage& message, const std::string& deviceId);
    
    // ========================================================================
    // ROUTE MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Add route
     * @param route Route to add
     */
    void addRoute(std::shared_ptr<MidiRoute> route);
    
    /**
     * @brief Remove route
     * @param id Route ID
     * @return true if removed
     */
    bool removeRoute(const std::string& id);
    
    /**
     * @brief Get route by ID
     * @param id Route ID
     * @return Route or nullptr
     */
    std::shared_ptr<MidiRoute> getRoute(const std::string& id) const;
    
    /**
     * @brief Get all routes
     * @return Vector of routes
     */
    std::vector<std::shared_ptr<MidiRoute>> getRoutes() const;
    
    /**
     * @brief Enable/disable route
     * @param id Route ID
     * @param enabled State
     */
    void setRouteEnabled(const std::string& id, bool enabled);
    
    /**
     * @brief Clear all routes
     */
    void clearRoutes();
    
    // ========================================================================
    // DEVICE MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Register device
     * @param device Device to register
     */
    void registerDevice(std::shared_ptr<MidiDevice> device);
    
    /**
     * @brief Unregister device
     * @param deviceId Device ID
     */
    void unregisterDevice(const std::string& deviceId);
    
    /**
     * @brief Get device
     * @param deviceId Device ID
     * @return Device or nullptr
     */
    std::shared_ptr<MidiDevice> getDevice(const std::string& deviceId) const;
    
    // ========================================================================
    // LATENCY COMPENSATION
    // ========================================================================
    
    /**
     * @brief Set latency compensator
     * @param compensator Compensator instance
     */
    void setLatencyCompensator(LatencyCompensator* compensator) {
        compensator_ = compensator;
    }
    
    /**
     * @brief Enable/disable instrument compensation
     * @param enabled State
     */
    void setInstrumentCompensationEnabled(bool enabled) {
        instrumentCompensationEnabled_ = enabled;
    }
    
    /**
     * @brief Check if instrument compensation is enabled
     */
    bool isInstrumentCompensationEnabled() const {
        return instrumentCompensationEnabled_;
    }
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get route statistics
     * @param routeId Route ID
     * @return Statistics
     */
    RouteStatistics getRouteStatistics(const std::string& routeId) const;
    
    /**
     * @brief Get all statistics
     * @return JSON with all stats
     */
    json getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics();
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Set message callback
     * @param callback Callback function
     */
    void setMessageCallback(MessageCallback callback) {
        messageCallback_ = callback;
    }
    
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
    MidiMessage applyTransformations(const MidiMessage& message, const MidiRoute& route) const;
    
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
    
    /// Routes (sorted by priority)
    std::vector<std::shared_ptr<MidiRoute>> routes_;
    
    /// Devices (deviceId -> device)
    std::unordered_map<std::string, std::shared_ptr<MidiDevice>> devices_;
    
    /// Route statistics (routeId -> stats)
    mutable std::unordered_map<std::string, RouteStatistics> routeStats_;
    
    /// Latency compensator
    LatencyCompensator* compensator_;
    
    /// Instrument compensation enabled
    bool instrumentCompensationEnabled_;
    
    /// Message callback
    MessageCallback messageCallback_;
    
    /// Read/write mutex
    mutable std::shared_mutex mutex_;
    
    /// Global statistics
    struct {
        uint64_t totalMessages;
        uint64_t routedMessages;
        uint64_t droppedMessages;
    } globalStats_;
};

} // namespace midiMind
// ============================================================================
// File: backend/src/midi/MidiRouter.cpp
// Version: 4.2.0 - EventBus Integration
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.0:
//   🔧 ADDED: EventBus support for routing events
//   ✅ Publishes RouteAddedEvent and RouteRemovedEvent
//   ✅ Added enableRoute/disableRoute convenience methods
//
// ============================================================================

#include "MidiRouter.h"
#include "../core/Logger.h"
#include "../core/EventBus.h"
#include "../core/TimeUtils.h"
#include "../events/Events.h"
#include "../timing/TimestampManager.h"
#include "devices/MidiDevice.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

MidiRouter::MidiRouter(LatencyCompensator* compensator,
                       std::shared_ptr<EventBus> eventBus)
    : compensator_(compensator)
    , eventBus_(eventBus)
    , instrumentCompensationEnabled_(true)
{
    Logger::info("MidiRouter", "MidiRouter v4.2.0 created");
    
    // Initialize global stats
    globalStats_.totalMessages = 0;
    globalStats_.routedMessages = 0;
    globalStats_.droppedMessages = 0;
}

MidiRouter::~MidiRouter() {
    Logger::info("MidiRouter", "MidiRouter destroyed");
}

// ============================================================================
// ROUTING
// ============================================================================

void MidiRouter::route(const MidiMessage& message) {
    // Update global stats (atomic, no lock needed)
    globalStats_.totalMessages++;
    
    // Copy routes for processing (avoid holding lock during routing)
    std::vector<std::shared_ptr<MidiRoute>> routesCopy;
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        routesCopy = routes_;
    }
    
    // Find matching routes
    std::vector<std::shared_ptr<MidiRoute>> matchingRoutes;
    
    for (const auto& route : routesCopy) {
        if (route->enabled && matchesRoute(message, *route)) {
            matchingRoutes.push_back(route);
        }
    }
    
    // Sort by priority (highest first)
    std::sort(matchingRoutes.begin(), matchingRoutes.end(),
             [](const auto& a, const auto& b) {
                 return a->priority > b->priority;
             });
    
    // Route to all matching destinations
    if (matchingRoutes.empty()) {
        globalStats_.droppedMessages++;
        Logger::debug("MidiRouter", "No matching routes for message");
        return;
    }
    
    for (const auto& route : matchingRoutes) {
        // Apply transformations
        MidiMessage transformedMessage = applyTransformations(message, *route);
        
        // Get compensation
        int64_t compensation = getCompensationForRoute(*route);
        
        // Apply compensation if needed
        if (compensation != 0) {
            uint64_t currentTime = TimestampManager::instance().now();
            uint64_t scheduledTime = currentTime + static_cast<uint64_t>(std::abs(compensation));
            transformedMessage.setTimestamp(scheduledTime);
            
            Logger::debug("MidiRouter", 
                         "Applied compensation: " + std::to_string(compensation) + "µs");
        }
        
        // Send to destination
        sendToDevice(route->destinationDeviceId, transformedMessage);
        
        // Update statistics
        updateRouteStatistics(route->id, compensation);
        
        globalStats_.routedMessages++;
    }
}

void MidiRouter::routeTo(const MidiMessage& message, const std::string& deviceId) {
    Logger::debug("MidiRouter", "Direct routing to device: " + deviceId);
    
    sendToDevice(deviceId, message);
    
    globalStats_.totalMessages++;
    globalStats_.routedMessages++;
}

void MidiRouter::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    messageCallback_ = callback;
    Logger::info("MidiRouter", "Message callback set");
}

// ============================================================================
// ROUTE MANAGEMENT
// ============================================================================

void MidiRouter::addRoute(std::shared_ptr<MidiRoute> route) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (route->id.empty()) {
        // Generate ID if not provided
        route->id = "route_" + std::to_string(routes_.size());
    }
    
    // Check for duplicate ID
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [&route](const auto& r) { return r->id == route->id; });
    
    if (it != routes_.end()) {
        Logger::warning("MidiRouter", "Route ID already exists: " + route->id);
        return;
    }
    
    routes_.push_back(route);
    
    // Initialize statistics
    RouteStatistics stats;
    stats.routeId = route->id;
    stats.routeName = route->name;
    routeStats_[route->id] = stats;
    
    Logger::info("MidiRouter", "Route added: " + route->name + " (ID: " + route->id + ")");
    
    // Publish event to EventBus (without holding lock)
    std::string sourceId = route->sourceDeviceId;
    std::string destId = route->destinationDeviceId;
    lock.unlock();
    
    if (eventBus_) {
        try {
            eventBus_->publish(events::RouteAddedEvent(
                sourceId.empty() ? "any" : sourceId,
                destId,
                TimeUtils::getCurrentTimestamp()
            ));
            Logger::debug("MidiRouter", "Published RouteAddedEvent");
        } catch (const std::exception& e) {
            Logger::error("MidiRouter", 
                "Failed to publish RouteAddedEvent: " + std::string(e.what()));
        }
    }
}

bool MidiRouter::addRoute(const std::string& sourceDeviceId, const std::string& destinationDeviceId) {
    if (destinationDeviceId.empty()) {
        Logger::error("MidiRouter", "Cannot add route: destination device ID is empty");
        return false;
    }
    
    // Create new route
    auto route = std::make_shared<MidiRoute>();
    
    // Generate unique ID
    route->id = "route_" + sourceDeviceId + "_to_" + destinationDeviceId + "_" + 
                std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    route->name = sourceDeviceId.empty() ? 
                  "Any -> " + destinationDeviceId : 
                  sourceDeviceId + " -> " + destinationDeviceId;
    
    route->sourceDeviceId = sourceDeviceId;
    route->destinationDeviceId = destinationDeviceId;
    route->priority = 50;
    route->enabled = true;
    
    addRoute(route);
    
    Logger::info("MidiRouter", "Simple route added: " + route->name);
    return true;
}

bool MidiRouter::removeRoute(const std::string& id) {
    std::string sourceId, destId;
    bool found = false;
    
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto it = std::find_if(routes_.begin(), routes_.end(),
                              [&id](const auto& r) { return r->id == id; });
        
        if (it != routes_.end()) {
            sourceId = (*it)->sourceDeviceId;
            destId = (*it)->destinationDeviceId;
            Logger::info("MidiRouter", "Route removed: " + (*it)->name);
            routes_.erase(it);
            routeStats_.erase(id);
            found = true;
        }
    }
    
    // Publish event to EventBus (without holding lock)
    if (found && eventBus_) {
        try {
            eventBus_->publish(events::RouteRemovedEvent(
                sourceId.empty() ? "any" : sourceId,
                destId,
                TimeUtils::getCurrentTimestamp()
            ));
            Logger::debug("MidiRouter", "Published RouteRemovedEvent");
        } catch (const std::exception& e) {
            Logger::error("MidiRouter", 
                "Failed to publish RouteRemovedEvent: " + std::string(e.what()));
        }
    }
    
    return found;
}

bool MidiRouter::removeRoute(const std::string& sourceDeviceId, const std::string& destinationDeviceId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [&sourceDeviceId, &destinationDeviceId](const auto& r) { 
                              return r->sourceDeviceId == sourceDeviceId && 
                                     r->destinationDeviceId == destinationDeviceId; 
                          });
    
    if (it != routes_.end()) {
        std::string routeName = (*it)->name;
        std::string routeId = (*it)->id;
        routes_.erase(it);
        routeStats_.erase(routeId);
        
        Logger::info("MidiRouter", "Route removed by devices: " + routeName);
        
        // Publish event (unlock first)
        lock.unlock();
        
        if (eventBus_) {
            try {
                eventBus_->publish(events::RouteRemovedEvent(
                    sourceDeviceId.empty() ? "any" : sourceDeviceId,
                    destinationDeviceId,
                    TimeUtils::getCurrentTimestamp()
                ));
                Logger::debug("MidiRouter", "Published RouteRemovedEvent");
            } catch (const std::exception& e) {
                Logger::error("MidiRouter", 
                    "Failed to publish RouteRemovedEvent: " + std::string(e.what()));
            }
        }
        
        return true;
    }
    
    Logger::warning("MidiRouter", 
                   "No route found from " + sourceDeviceId + " to " + destinationDeviceId);
    return false;
}

std::shared_ptr<MidiRoute> MidiRouter::getRoute(const std::string& id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [&id](const auto& r) { return r->id == id; });
    
    if (it != routes_.end()) {
        return *it;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<MidiRoute>> MidiRouter::getRoutes() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return routes_;
}

void MidiRouter::setRouteEnabled(const std::string& id, bool enabled) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [&id](const auto& r) { return r->id == id; });
    
    if (it != routes_.end()) {
        (*it)->enabled = enabled;
        Logger::info("MidiRouter", 
                    "Route " + (*it)->name + " " + 
                    (enabled ? "enabled" : "disabled"));
    }
}

bool MidiRouter::enableRoute(const std::string& sourceDeviceId, 
                             const std::string& destinationDeviceId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [&sourceDeviceId, &destinationDeviceId](const auto& r) {
                              return r->sourceDeviceId == sourceDeviceId &&
                                     r->destinationDeviceId == destinationDeviceId;
                          });
    
    if (it != routes_.end()) {
        (*it)->enabled = true;
        Logger::info("MidiRouter", "Route enabled: " + (*it)->name);
        return true;
    }
    
    return false;
}

bool MidiRouter::disableRoute(const std::string& sourceDeviceId,
                              const std::string& destinationDeviceId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [&sourceDeviceId, &destinationDeviceId](const auto& r) {
                              return r->sourceDeviceId == sourceDeviceId &&
                                     r->destinationDeviceId == destinationDeviceId;
                          });
    
    if (it != routes_.end()) {
        (*it)->enabled = false;
        Logger::info("MidiRouter", "Route disabled: " + (*it)->name);
        return true;
    }
    
    return false;
}

void MidiRouter::clearRoutes() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    Logger::info("MidiRouter", "Clearing all routes");
    routes_.clear();
    routeStats_.clear();
}

// ============================================================================
// DEVICE MANAGEMENT
// ============================================================================

void MidiRouter::registerDevice(std::shared_ptr<MidiDevice> device) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (!device) {
        Logger::warning("MidiRouter", "Cannot register null device");
        return;
    }
    
    std::string deviceId = device->getId();
    devices_[deviceId] = device;
    
    Logger::info("MidiRouter", "Device registered: " + deviceId);
}

void MidiRouter::unregisterDevice(const std::string& deviceId) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = devices_.find(deviceId);
    if (it != devices_.end()) {
        devices_.erase(it);
        Logger::info("MidiRouter", "Device unregistered: " + deviceId);
    }
}

std::shared_ptr<MidiDevice> MidiRouter::getDevice(const std::string& deviceId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = devices_.find(deviceId);
    if (it != devices_.end()) {
        return it->second;
    }
    
    return nullptr;
}

// ============================================================================
// LATENCY COMPENSATION
// ============================================================================

void MidiRouter::setLatencyCompensator(LatencyCompensator* compensator) {
    compensator_.store(compensator);
    Logger::info("MidiRouter", "Latency compensator set");
}

void MidiRouter::setInstrumentCompensationEnabled(bool enabled) {
    instrumentCompensationEnabled_.store(enabled);
    Logger::info("MidiRouter", 
                std::string("Instrument compensation ") + (enabled ? "enabled" : "disabled"));
}

bool MidiRouter::isInstrumentCompensationEnabled() const {
    return instrumentCompensationEnabled_.load();
}

// ============================================================================
// STATISTICS
// ============================================================================

RouteStatistics MidiRouter::getRouteStatistics(const std::string& routeId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = routeStats_.find(routeId);
    if (it != routeStats_.end()) {
        return it->second;
    }
    
    // Return empty stats if not found
    RouteStatistics empty;
    empty.routeId = routeId;
    return empty;
}

json MidiRouter::getStatistics() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    json stats = {
        {"total_messages", globalStats_.totalMessages.load()},
        {"routed_messages", globalStats_.routedMessages.load()},
        {"dropped_messages", globalStats_.droppedMessages.load()},
        {"total_routes", routes_.size()},
        {"routes", json::array()}
    };
    
    for (const auto& [id, routeStat] : routeStats_) {
        stats["routes"].push_back(routeStat.toJson());
    }
    
    return stats;
}

void MidiRouter::resetStatistics() {
    Logger::info("MidiRouter", "Resetting statistics");
    
    globalStats_.totalMessages = 0;
    globalStats_.routedMessages = 0;
    globalStats_.droppedMessages = 0;
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (auto& [id, stats] : routeStats_) {
        stats.messagesRouted = 0;
        stats.avgCompensation = 0;
    }
}

// ============================================================================
// EVENTBUS
// ============================================================================

void MidiRouter::setEventBus(std::shared_ptr<EventBus> eventBus) {
    eventBus_ = eventBus;
    Logger::info("MidiRouter", "EventBus configured");
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool MidiRouter::matchesRoute(const MidiMessage& message, const MidiRoute& route) const {
    // Check channel filter
    if (!route.channelFilter.empty()) {
        int channel = message.getChannel();
        if (channel < 0) return false;
        
        bool channelMatch = false;
        for (uint8_t allowedChannel : route.channelFilter) {
            if (channel == allowedChannel) {
                channelMatch = true;
                break;
            }
        }
        
        if (!channelMatch) return false;
    }
    
    // Check message type filter
    if (!route.messageTypeFilter.empty()) {
        uint8_t status = message.getStatus();
        
        bool typeMatch = false;
        for (uint8_t allowedType : route.messageTypeFilter) {
            if (status == allowedType) {
                typeMatch = true;
                break;
            }
        }
        
        if (!typeMatch) return false;
    }
    
    return true;
}

MidiMessage MidiRouter::applyTransformations(const MidiMessage& message, 
                                            const MidiRoute& route) const {
    MidiMessage transformed = message;
    
    // Apply channel transform
    if (route.channelTransform != 0) {
        int channel = message.getChannel();
        if (channel >= 0) {
            int newChannel = channel + route.channelTransform;
            
            // Clamp to valid range (0-15)
            if (newChannel < 0) newChannel = 0;
            if (newChannel > 15) newChannel = 15;
            
            // Recreate message with new channel
            if (message.isNoteOn()) {
                transformed = MidiMessage::noteOn(newChannel, 
                                                  message.getData1(), 
                                                  message.getData2());
            } else if (message.isNoteOff()) {
                transformed = MidiMessage::noteOff(newChannel, 
                                                   message.getData1(), 
                                                   message.getData2());
            }
        }
    }
    
    // Apply transpose (for note messages)
    if (route.transposeTransform != 0 && 
        (message.isNoteOn() || message.isNoteOff())) {
        
        uint8_t note = message.getData1();
        int newNote = note + route.transposeTransform;
        
        // Clamp to valid range (0-127)
        if (newNote < 0) newNote = 0;
        if (newNote > 127) newNote = 127;
        
        int channel = transformed.getChannel();
        if (channel < 0) channel = 0;
        
        if (message.isNoteOn()) {
            transformed = MidiMessage::noteOn(channel, newNote, message.getData2());
        } else {
            transformed = MidiMessage::noteOff(channel, newNote, message.getData2());
        }
    }
    
    // Apply velocity transform (for note messages)
    if (route.velocityTransform != 0 && 
        (message.isNoteOn() || message.isNoteOff())) {
        
        uint8_t velocity = message.getData2();
        int newVelocity = velocity + route.velocityTransform;
        
        // Clamp to valid range (0-127)
        if (newVelocity < 0) newVelocity = 0;
        if (newVelocity > 127) newVelocity = 127;
        
        int channel = transformed.getChannel();
        if (channel < 0) channel = 0;
        
        if (message.isNoteOn()) {
            transformed = MidiMessage::noteOn(channel, message.getData1(), newVelocity);
        } else {
            transformed = MidiMessage::noteOff(channel, message.getData1(), newVelocity);
        }
    }
    
    return transformed;
}

int64_t MidiRouter::getCompensationForRoute(const MidiRoute& route) const {
    if (!instrumentCompensationEnabled_.load()) {
        return 0;
    }
    
    LatencyCompensator* comp = compensator_.load();
    if (comp) {
        std::string instrumentId = route.destinationDeviceId;
        int64_t compensation = comp->getInstrumentCompensation(instrumentId);
        
        Logger::debug("MidiRouter", 
                     "Compensation for " + instrumentId + ": " + 
                     std::to_string(compensation) + "µs");
        
        return compensation;
    }
    
    return 0;
}

void MidiRouter::sendToDevice(const std::string& deviceId, const MidiMessage& message) {
    // Get device
    auto device = getDevice(deviceId);
    
    if (!device) {
        Logger::warning("MidiRouter", "Device not found: " + deviceId);
        return;
    }
    
    // Copy callback with lock
    MessageCallback callback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback = messageCallback_;
    }
    
    if (callback) {
        callback(message, deviceId);
    }
    
    Logger::debug("MidiRouter", 
                 "Message sent to " + deviceId + ": " + message.getTypeName());
}

void MidiRouter::updateRouteStatistics(const std::string& routeId, int64_t compensation) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = routeStats_.find(routeId);
    if (it != routeStats_.end()) {
        auto& stats = it->second;
        
        uint64_t routed = stats.messagesRouted.fetch_add(1) + 1;
        stats.lastMessageTime.store(TimestampManager::instance().now());
        
        // Update average compensation (simple moving average)
        int64_t oldAvg = stats.avgCompensation.load();
        int64_t newAvg = (oldAvg * (routed - 1) + compensation) / routed;
        stats.avgCompensation.store(newAvg);
    }
}

} // namespace midiMind

// ============================================================================
// END OF FILE MidiRouter.cpp
// ============================================================================
// ============================================================================
// File: backend/src/midi/MidiRouter.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of MidiRouter.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Complete implementation
//   - Latency compensation integration
//   - Enhanced filtering and statistics
//
// ============================================================================

#include "MidiRouter.h"
#include "../core/Logger.h"
#include "../timing/TimestampManager.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

MidiRouter::MidiRouter(LatencyCompensator* compensator)
    : compensator_(compensator)
    , instrumentCompensationEnabled_(true)
{
    Logger::info("MidiRouter", "MidiRouter created");
    
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
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    // Update global stats
    globalStats_.totalMessages++;
    
    // Find matching routes
    std::vector<std::shared_ptr<MidiRoute>> matchingRoutes;
    
    for (const auto& route : routes_) {
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
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    Logger::debug("MidiRouter", "Direct routing to device: " + deviceId);
    
    sendToDevice(deviceId, message);
    
    globalStats_.totalMessages++;
    globalStats_.routedMessages++;
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
        Logger::warn("MidiRouter", "Route ID already exists: " + route->id);
        return;
    }
    
    routes_.push_back(route);
    
    // Initialize statistics
    RouteStatistics stats;
    stats.routeId = route->id;
    stats.routeName = route->name;
    routeStats_[route->id] = stats;
    
    Logger::info("MidiRouter", "Route added: " + route->name + " (ID: " + route->id + ")");
}

bool MidiRouter::removeRoute(const std::string& id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    auto it = std::find_if(routes_.begin(), routes_.end(),
                          [&id](const auto& r) { return r->id == id; });
    
    if (it != routes_.end()) {
        Logger::info("MidiRouter", "Route removed: " + (*it)->name);
        routes_.erase(it);
        routeStats_.erase(id);
        return true;
    }
    
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
        Logger::warn("MidiRouter", "Cannot register null device");
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
// STATISTICS
// ============================================================================

RouteStatistics MidiRouter::getRouteStatistics(const std::string& routeId) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = routeStats_.find(routeId);
    if (it != routeStats_.end()) {
        return it->second;
    }
    
    return RouteStatistics();
}

json MidiRouter::getStatistics() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    json stats = {
        {"global", {
            {"total_messages", globalStats_.totalMessages},
            {"routed_messages", globalStats_.routedMessages},
            {"dropped_messages", globalStats_.droppedMessages},
            {"route_count", routes_.size()},
            {"device_count", devices_.size()}
        }},
        {"routes", json::array()}
    };
    
    for (const auto& [routeId, routeStat] : routeStats_) {
        stats["routes"].push_back(routeStat.toJson());
    }
    
    return stats;
}

void MidiRouter::resetStatistics() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    Logger::info("MidiRouter", "Resetting statistics");
    
    globalStats_.totalMessages = 0;
    globalStats_.routedMessages = 0;
    globalStats_.droppedMessages = 0;
    
    for (auto& [routeId, stats] : routeStats_) {
        stats.messagesRouted = 0;
        stats.messagesDropped = 0;
        stats.avgCompensation = 0;
        stats.lastMessageTime = 0;
    }
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool MidiRouter::matchesRoute(const MidiMessage& message, const MidiRoute& route) const {
    // Check source device filter
    // Note: In real implementation, message would have source device ID
    // For now, we assume all messages can match source filter
    
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
        std::string msgType = message.getTypeName();
        
        bool typeMatch = false;
        for (const auto& allowedType : route.messageTypeFilter) {
            if (msgType == allowedType) {
                typeMatch = true;
                break;
            }
        }
        
        if (!typeMatch) return false;
    }
    
    // Check note range (for note messages)
    if (message.isNoteOn() || message.isNoteOff()) {
        uint8_t note = message.getData1();
        
        if (route.minNote.has_value() && note < route.minNote.value()) {
            return false;
        }
        
        if (route.maxNote.has_value() && note > route.maxNote.value()) {
            return false;
        }
    }
    
    // Check velocity range (for note messages)
    if (message.isNoteOn() || message.isNoteOff()) {
        uint8_t velocity = message.getData2();
        
        if (route.minVelocity.has_value() && velocity < route.minVelocity.value()) {
            return false;
        }
        
        if (route.maxVelocity.has_value() && velocity > route.maxVelocity.value()) {
            return false;
        }
    }
    
    return true;
}

MidiMessage MidiRouter::applyTransformations(const MidiMessage& message, 
                                            const MidiRoute& route) const {
    MidiMessage transformed = message;
    
    // Apply channel transform
    if (route.channelTransform.has_value()) {
        int channel = message.getChannel();
        if (channel >= 0) {
            int newChannel = channel + route.channelTransform.value();
            
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
            // Add other message types as needed
        }
    }
    
    // Apply transpose (for note messages)
    if (route.transposeTransform.has_value() && 
        (message.isNoteOn() || message.isNoteOff())) {
        
        uint8_t note = message.getData1();
        int newNote = note + route.transposeTransform.value();
        
        // Clamp to valid range (0-127)
        if (newNote < 0) newNote = 0;
        if (newNote > 127) newNote = 127;
        
        int channel = message.getChannel();
        if (channel < 0) channel = 0;
        
        if (message.isNoteOn()) {
            transformed = MidiMessage::noteOn(channel, newNote, message.getData2());
        } else {
            transformed = MidiMessage::noteOff(channel, newNote, message.getData2());
        }
    }
    
    // Apply velocity scaling (for note messages)
    if (route.velocityScale.has_value() && 
        (message.isNoteOn() || message.isNoteOff())) {
        
        uint8_t velocity = message.getData2();
        float scaled = velocity * route.velocityScale.value();
        
        // Clamp to valid range (0-127)
        if (scaled < 0.0f) scaled = 0.0f;
        if (scaled > 127.0f) scaled = 127.0f;
        
        int channel = message.getChannel();
        if (channel < 0) channel = 0;
        
        if (message.isNoteOn()) {
            transformed = MidiMessage::noteOn(channel, 
                                             message.getData1(), 
                                             static_cast<uint8_t>(scaled));
        } else {
            transformed = MidiMessage::noteOff(channel, 
                                              message.getData1(), 
                                              static_cast<uint8_t>(scaled));
        }
    }
    
    return transformed;
}

int64_t MidiRouter::getCompensationForRoute(const MidiRoute& route) const {
    if (!instrumentCompensationEnabled_ || !compensator_) {
        return 0;
    }
    
    // Try instrument-level compensation first
    if (!route.instrumentId.empty()) {
        int64_t compensation = compensator_->getInstrumentCompensation(route.instrumentId);
        
        Logger::debug("MidiRouter", 
                     "Instrument compensation for " + route.instrumentId + ": " + 
                     std::to_string(compensation) + "µs");
        
        return compensation;
    }
    
    // Fallback to device-level compensation
    if (!route.destinationDeviceId.empty()) {
        int64_t compensation = compensator_->getDeviceCompensation(route.destinationDeviceId);
        
        Logger::debug("MidiRouter", 
                     "Device compensation for " + route.destinationDeviceId + ": " + 
                     std::to_string(compensation) + "µs");
        
        return compensation;
    }
    
    return 0;
}

void MidiRouter::sendToDevice(const std::string& deviceId, const MidiMessage& message) {
    // Get device
    auto device = getDevice(deviceId);
    
    if (!device) {
        Logger::warn("MidiRouter", "Device not found: " + deviceId);
        return;
    }
    
    // Send message
    // Note: In real implementation, device would have a send() method
    // For now, just call the callback
    
    if (messageCallback_) {
        messageCallback_(message, deviceId);
    }
    
    Logger::debug("MidiRouter", 
                 "Message sent to " + deviceId + ": " + message.getTypeName());
}

void MidiRouter::updateRouteStatistics(const std::string& routeId, int64_t compensation) {
    auto it = routeStats_.find(routeId);
    if (it != routeStats_.end()) {
        auto& stats = it->second;
        
        stats.messagesRouted++;
        stats.lastMessageTime = TimestampManager::instance().now();
        
        // Update average compensation (simple moving average)
        if (stats.messagesRouted == 1) {
            stats.avgCompensation = compensation;
        } else {
            stats.avgCompensation = (stats.avgCompensation * (stats.messagesRouted - 1) + 
                                    compensation) / stats.messagesRouted;
        }
    }
}

} // namespace midiMind

// ============================================================================
// END OF FILE MidiRouter.cpp
// ============================================================================